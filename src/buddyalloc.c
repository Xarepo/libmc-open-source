/*
 * Copyright (c) 2011 - 2012, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*

  Design notes

  - A basic buddy allocation algorithm without block headers, meaning that the
    full power-of-two blocks are returned to the user.
  - Since there are no headers, a single bit has been reserved to mark if the
    block is free or not, put at the LSB of the first pointer. The user must
    keep that bit to zero.
      - This may seem as a huge (and dangerous) limitation, but the main use of
        this allocator is for data structure node allocation where it is common
        to have a pointer to a node as the first member.
  - It is possible to make a allocator without the free-bit requirement, but
    we have chosen not to:
      - A pointer->isfree hash would work, but to large impact on performance.
      - Having a header, and skipping the aligned power-of-two block feature,
        but that is too large a hit on the main use as a memory allocator for
        data structure nodes.
  - Performance is key, if worse than standard malloc it would be near
    meaningless (although alignment is a valuable feature)
      - The normal case must be extremely quick, about 100 clock cycles to be
        competetive with standard malloc. This means that atomic operations (CAS
        and similar) must be very sparsely used, one CAS typically costs about
        20 cycles.
          - This has a large effect on the multithreaded design
      - The resulting code performs better than malloc in many cases, with some
        exceptions. The main weakness of buddy allocation is the joining of
        split blocks which means jumping around in RAM - slow cache loads,
        prefetch helps this somewhat.
  - The normal free lists are double-linked to allow erasing from the middle of
    the list, which happens when blocks are merged.
  - Designed for multi-threaded and soft realtime use.
      - While a 100% lock-free design is possible, the CAS cost for that is huge
        (mainly due to double-linked lists) meaning that performance would
        suffer too much.
      - Instead the same approach as standard malloc is employed -- having an
        extremely fast normal case minimizes the risk of collision in the first
        place, and that is combined with a special case when collision does
        occur which is lock contention free but less optimal performance-wise.
          - In total this is much more efficient than a 100% lock-free design.
          - The collision case is slower, but not too slow to be a problem in
            soft realtime applications.
      - One global lock is used, causing only one expensive atomic operation
        (an atomic swap) per call in the fast common case.
          - Due to the very fast common case a single global lock makes sense.
  - At thread collision an alternative allocation algorithm is used, buddy
    allocation without merging with blocks which allows for single-linked lists,
    and can then be made lock-free with good performance.
      - Free lists are separate and due to no merging there is fragmentation,
        but if lock is gained when a block is freed (highly likely) it will be
        put in the normal double-linked free lists and merging takes place as
        needed.
      - Since collisions are rare, there is not much data in the lock-free
        allocator.
   - BUDDYALLOC_ALLOC_MODE_MALLOC exists only for debugging, the MMAP mode
     should be used.

 */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define BITOPS_PREFIX bitf32
#define BITOPS_TYPE uint_fast32_t
#define BITOPS_TYPE_MAX INT_FAST32_MAX
#include <bitops.h>
#include <buddyalloc.h>

#define MIN_P2 BUDDYALLOC_MIN_SIZE_LOG2_
#define MAX_P2 BUDDYALLOC_MAX_SIZE_LOG2_

// TRACKMEM_DEBUG is not primarily to detect bugs in buddyalloc code itself, but to detect
// when its data structure is corrupted due to misuse (double free etc), so TRACKMEM_DEBUG
// is suitably activated during development and test of code that uses buddyalloc.

#if TRACKMEM_DEBUG - 0 != 0
#define MC_PREFIX refset
#define MC_KEY_T void *
#define MC_NO_VALUE 1
#include <mrb_tmpl.h>
#include <trackmem.h>
extern trackmem_t *buddyalloc_tm;

#endif

// Free block headers, both normal and lockfree versions, which must have compatible layouts.
struct free_block {
    uintptr_t free_lsb;
    struct free_block *next;
    struct free_block *prev;
    uint_fast8_t p2;
};
struct lockfree_free_block {
    uintptr_t free_lsb;
    atomic_uintptr_t next;
    void *placeholder;
    uint_fast8_t p2;
};

#define UNLOCK(ba)                        \
    do {                                  \
        atomic_store(&(ba)->lock, false); \
    } while(0)

static inline bool
try_lock(buddyalloc_t *ba)
{
    // load is much cheaper than swap so we test with load first
    if (atomic_load(&ba->lock)) {
        return false;
    }
    if (atomic_exchange(&ba->lock, true)) {
        return false;
    }
    return true;
}

#if TRACKMEM_DEBUG - 0 != 0
static void
buddyalloc_integrity_check(buddyalloc_t *ba)
{
    refset_t *refset = refset_new(~0);

    while (!try_lock(ba)) {}; // spinlock, no problem as its only debug code

    for (int i = 0; i < BUDDYALLOC_FREELIST_SIZE; i++) {
        int p2 = BUDDYALLOC_MIN_SIZE_LOG2_ + i;
        if (bit32_isset(&ba->nonempty_normal_freelists, p2)) {
            assert(ba->normal_freelists[i] != NULL);
            struct free_block *prev = NULL;
            for (struct free_block *node = ba->normal_freelists[i]; node != NULL; node = node->next) {
                if (node != ba->normal_freelists[i]) { // prev link is not set on head node
                    assert(node->prev == prev);
                }
                assert(refset_find(refset, node) == NULL);
                refset_insert(refset, node);
                assert(node->p2 == p2);
                assert(node->free_lsb == 1);
                prev = node;
            }
        } else {
            assert(ba->normal_freelists[i] == NULL);
        }
        for (struct lockfree_free_block *node = (struct lockfree_free_block *)atomic_load(&ba->lockfree_freelists[i]);
             node != NULL;
             node = (struct lockfree_free_block *)atomic_load(&node->next))
        {
            assert(refset_find(refset, node) == NULL);
            refset_insert(refset, node);
            assert(node->p2 == p2);
            assert(node->free_lsb == 0);
        }
    }

    UNLOCK(ba);

    refset_delete(refset);
}
#endif // TRACKMEM_DEBUG

static void *
superblock_alloc(buddyalloc_t *ba)
{
    if (ba->superblock_allocator.alloc == NULL) {
        /*
          Uninitialized buddyalloc functions. Set the allocation functions during the
          first allocation, which allows for changing default allocator in runtime.

          If the library is linked with the null allocators there will be null
          pointer functions in there, in that case the user must either provide allocator
          at buddyalloc_new(), or set the function pointers in the default allocator
          to an own superblock allocator.
        */
        ba->superblock_allocator = buddyalloc_superblock_allocator_default;
        if (ba->superblock_allocator.alloc == NULL) {
            (void)fprintf(stderr, "The library has no default superblock allocator on its own, custom must be provided\n");
            abort();
        }
    }

    void *ptr = ba->superblock_allocator.alloc(ba->superblock_allocator.arg, BUDDYALLOC_ALLOC_MAX);
    if (ptr == NULL) {
        if (ba->abort_if_out_of_memory) {
            (void)fprintf(stderr, "out of memory!\n");
            abort();
        }
        errno = ENOMEM;
        return NULL;
    }
    if (((uintptr_t)ptr & (BUDDYALLOC_ALLOC_MAX - 1)) != 0) {
        (void)fprintf(stderr, "badly aligned superblock, bug in allocator!\n");
        abort();
    }
    atomic_fetch_add(&ba->superblock_count, 1);
    return ptr;
}

static void
superblock_free(buddyalloc_t *ba,
                void *ptr)
{
    if (atomic_load(&ba->superblock_count) == 0) {
        // bug!
        (void)fprintf(stderr, "bad superblock count\n");
        abort();
    }
    atomic_fetch_sub(&ba->superblock_count, 1);
    ba->superblock_allocator.free(ba->superblock_allocator.arg, ptr, BUDDYALLOC_ALLOC_MAX);
}

static void
push_to_normal_freelist(buddyalloc_t *ba,
                        uintptr_t ptr,
                        uint_fast8_t p2)
{
    struct free_block *oldhead = ba->normal_freelists[p2-MIN_P2];
    struct free_block *newhead = (struct free_block *)ptr;
    newhead->next = oldhead;
    newhead->free_lsb = 1;
    newhead->p2 = p2;
    ba->normal_freelists[p2-MIN_P2] = newhead;
    if (oldhead == NULL) {
        ba->nonempty_normal_freelists |= 1u << p2;
    } else {
        oldhead->prev = newhead;
    }
}

static uintptr_t
pop_from_normal_freelist(buddyalloc_t *ba,
                         uint_fast8_t p2)
{
    struct free_block *head = ba->normal_freelists[p2-MIN_P2];
    struct free_block *next = head->next;
    ba->normal_freelists[p2-MIN_P2] = next;
    head->free_lsb = 0;
    if (next == NULL) {
        ba->nonempty_normal_freelists &= ~(1u << p2);
    }
    return (uintptr_t)head;
}

static void
push_to_lockfree_freelist(buddyalloc_t *ba,
                          uintptr_t ptr,
                          uint_fast8_t p2)
{
    struct lockfree_free_block *newhead;
    uintptr_t oldhead;

    newhead = (struct lockfree_free_block *)ptr;
    // Since we use the same blocks as the normal freelists, we must mark it as
    // allocated to avoid conflicts in freelist handling.
    newhead->free_lsb = 0;
    newhead->p2 = p2;

    oldhead = atomic_load(&ba->lockfree_freelists[p2-MIN_P2]);
    do {
        atomic_store(&newhead->next, oldhead);
    } while (!atomic_compare_exchange_weak(&ba->lockfree_freelists[p2-MIN_P2], &oldhead, (uintptr_t)newhead));
}

static uintptr_t
pop_from_lockfree_freelist(buddyalloc_t *ba,
                           uint_fast8_t p2)
{
    uintptr_t next;
    uintptr_t head = atomic_load(&ba->lockfree_freelists[p2-MIN_P2]);
    do {
        if (head == 0) {
            return 0;
        }
        next = atomic_load(&((struct lockfree_free_block *)head)->next);
    } while (!atomic_compare_exchange_weak(&ba->lockfree_freelists[p2-MIN_P2], &head, next));
    return head;
}

static bool
try_erase_from_normal_freelist(buddyalloc_t *ba,
                               void *ptr,
                               uint_fast8_t p2)
{
    struct free_block *prev;
    struct free_block *next;

    struct free_block *cur = (struct free_block *)ptr;
    // fast path, test if block is the head in the freelist
    if (ba->normal_freelists[p2-MIN_P2] == cur) {
        next = cur->next;
        ba->normal_freelists[p2-MIN_P2] = next;
        if (next == NULL) {
            // list became empty
            ba->nonempty_normal_freelists &= ~(1u << p2);
        }
        return true;
    }

    // test if free, and if expected block size
    if ((cur->free_lsb & 1u) == 0 || cur->p2 != p2) {
        return false;
    }

    prev = cur->prev;
    next = cur->next;
    prev->next = next;
    if (next != NULL) {
        next->prev = prev;
    }
    return true;
}

static int_fast8_t
size_to_p2(size_t size)
{
    if (size == 0) {
        // requesting zero size, should result in NULL pointer return
        return -1;
    }
    if (size > BUDDYALLOC_ALLOC_MAX) {
        (void)fprintf(stderr, "Tried to allocate %lu bytes which is larger than the "
                      "super block size, which is %lu. This is not allowed. "
                      "Aborting.\n",
                      (unsigned long)size, (unsigned long)BUDDYALLOC_ALLOC_MAX);
        abort();
    }
    if (size < BUDDYALLOC_ALLOC_MIN) {
        size = BUDDYALLOC_ALLOC_MIN;
    }
    int_fast8_t p2 = (int_fast8_t)bitf32_bsr((uint_fast32_t)size);
    // if size is not purely a power of two, we need to add to p2
    p2 += (bitf32_bsf((uint_fast32_t)size) != p2); // NOLINT
    return p2;
}

static void *
allocate_and_unlock(buddyalloc_t *ba,
                    uint_fast8_t p2);

static void *
allocate_when_lock_contention(buddyalloc_t *ba,
                              uint_fast8_t p2)
{
    uintptr_t ptr = 0;
    uint_fast8_t free_p2;

    // find smallest block that has block available in free list, if any
    for (free_p2 = p2; free_p2 < MAX_P2; free_p2++) {
        ptr = pop_from_lockfree_freelist(ba, free_p2);
        if (ptr != 0) {
            break;
        }
    }

    if (ptr == 0) {
        // free lists empty, we first try to gain lock again, quite likely that
        // we gain the lock on this second try since some time has passed
        if (try_lock(ba)) {
            return allocate_and_unlock(ba, p2);
        }
        // get a new superblock and allocate
        ptr = atomic_exchange(&ba->free_superblock, 0);
        if (ptr == 0) {
            if ((ptr = (uintptr_t)superblock_alloc(ba)) == 0) {
                return NULL;
            }
            // more time has passed, try to lock yet again
            if (try_lock(ba)) {
                ptr = atomic_exchange(&ba->free_superblock, ptr);
                if (ptr == 0) {
                    return allocate_and_unlock(ba, p2);
                }
                // someone got in between and assigned the prealloc pointer,
                // so we need to use it despite that we gained the lock
                UNLOCK(ba);
            }
        }
        // initialize first pointer with cleared free bit to mark it as allocated
        *(uintptr_t *)ptr = 0;
        free_p2 = MAX_P2;
    } else if (free_p2 == p2) {
        // popped from free list in desired size, so we're done
        return (void *)ptr;
    }
    // push buddy blocks to appropriate free lists
    for (; p2 < free_p2; p2++) {
        uintptr_t buddy = ptr + (1u << p2);
        push_to_lockfree_freelist(ba, buddy, p2);
    }
    return (void *)ptr;
}

static void *
allocate_and_unlock(buddyalloc_t *ba,
                    uint_fast8_t p2)
{
    uint_fast8_t free_p2;
    uintptr_t ptr;

    uint32_t nonempty = ba->nonempty_normal_freelists;
    // check if there is any free blocks of same or larger size
    nonempty &= ~((1u << p2)  - 1u);
    if (nonempty != 0) {
        free_p2 = bit32_bsf(nonempty); // smallest free size
        ptr = pop_from_normal_freelist(ba, free_p2);
        if (free_p2 == p2) {
            UNLOCK(ba);
            return (void *)ptr;
        }
    } else {
        // When we need to allocate new block we unlock, since it takes some time.
        ptr = atomic_exchange(&ba->free_superblock, 0);
        if (ptr == 0) {
            UNLOCK(ba);
            if ((ptr = (uintptr_t)superblock_alloc(ba)) == 0) {
                return NULL;
            }
            if (!try_lock(ba)) {
                ptr = atomic_exchange(&ba->free_superblock, ptr);
                if (ptr != 0) {
                    superblock_free(ba, (void *)ptr);
                }
                return allocate_when_lock_contention(ba, p2);
            }
        }
        // initialize first pointer with cleared free bit to mark it as allocated
        *(uintptr_t *)ptr = 0;
        free_p2 = MAX_P2;
    }

    // Push un-utilized parts of the block to the appropriate free lists.
    // Due to prefetch this is loop ends early and makes the last iteration
    // outside the loop.
    //
    // Same loop without prefetch:
    // for (; p2 < free_p2; p2++) {
    //     uintptr_t buddy = ptr + (1 << p2);
    //     push_to_normal_freelist(ba, buddy, p2);
    // }
    //
    {
        uintptr_t next_buddy = ptr + (1u << p2);
        for (free_p2--; p2 < free_p2; p2++) {
            uintptr_t buddy = ptr + (1u << p2);
            next_buddy = ptr + (1u << (p2 + 1u));
#if __has_builtin(__builtin_prefetch)
            __builtin_prefetch((void *)next_buddy, 0, 1);
#endif
            push_to_normal_freelist(ba, buddy, p2);
        }
        push_to_normal_freelist(ba, next_buddy, p2);
    }

    UNLOCK(ba);
    return (void *)ptr;
}

void *
buddyalloc_alloc(buddyalloc_t *ba,
                 size_t size)
{
    void *ptr;
    int_fast8_t p2 = size_to_p2(size);
    if (p2 < 0) {
        return NULL;
    }
    if (p2 == MAX_P2) {
        // max size, we just get a superblock
        ptr = (void *)atomic_exchange(&ba->free_superblock, 0);
        if (ptr == 0) {
            ptr = superblock_alloc(ba);
        }
    } else if (try_lock(ba)) {
        // got the lock, do normal allocation
        ptr = allocate_and_unlock(ba, p2);
    } else {
        // There is lock contention so we did not gain the lock (unlikely case),
        // so we run the special case allocation for this case.
        ptr = allocate_when_lock_contention(ba, p2);
    }
#if TRACKMEM_DEBUG - 0 != 0
    trackmem_register_alloc(buddyalloc_tm, ptr, size);
    buddyalloc_integrity_check(ba);
#endif
    return ptr;
}

void
buddyalloc_free(buddyalloc_t *ba,
                void *ptr_,
                size_t size)
{
#if TRACKMEM_DEBUG - 0 != 0
    trackmem_register_free(buddyalloc_tm, ptr_, size);
#endif
    if (ptr_ == NULL) {
        return;
    }
    uintptr_t ptr = (uintptr_t)ptr_;
    int_fast8_t try_p2 = size_to_p2(size);
    if (try_p2 < 0) {
        return;
    }

    uint_fast8_t p2 = try_p2;
    if (p2 == MAX_P2) {
        ptr = atomic_exchange(&ba->free_superblock, ptr);
        if (ptr != 0) {
            superblock_free(ba, (void *)ptr);
        }
        return;
    }

    if (!try_lock(ba)) {
        push_to_lockfree_freelist(ba, ptr, p2);
#if TRACKMEM_DEBUG - 0 != 0
        buddyalloc_integrity_check(ba);
#endif
        return;
    }

    do {
        uintptr_t buddy;
        uintptr_t base;
        // buddy is to the left or right?
        if ((ptr & ((1u << (p2 + 1u)) - 1u)) == 0) {
            buddy = ptr + (1u << p2);
            base = ptr;
        } else {
            buddy = ptr - (1u << p2);
            base = buddy;
        }
        if (!try_erase_from_normal_freelist(ba, (void *)buddy, p2)) {
            // buddy was allocated or split into an allocated/unallocated part
            break;
        }
        // buddy erased from free list, join to larger block
        ptr = base;
        p2++;
    } while (p2 < MAX_P2);

    if (p2 < MAX_P2) {
        push_to_normal_freelist(ba, ptr, p2);
    }

    UNLOCK(ba);

    if (p2 == MAX_P2) {
        ptr = atomic_exchange(&ba->free_superblock, ptr);
        if (ptr != 0) {
            superblock_free(ba, (void *)ptr);
        }
    }
#if TRACKMEM_DEBUG - 0 != 0
    buddyalloc_integrity_check(ba);
#endif
}

void
buddyalloc_free_buffers(buddyalloc_t *ba)
{
    // Note: it's the responsibility of the user to free all memory that has been allocated,
    // here we only free the extra superblock kept for performance reasons.
    uintptr_t ptr;

    if ((ptr = atomic_exchange(&ba->free_superblock, 0)) != 0) {
        superblock_free(ba, (void *)ptr);
    }
}

buddyalloc_t *
buddyalloc_new(struct buddyalloc_t_ *optional_struct_space,
               const struct buddyalloc_superblock_allocator *allocator,
               bool abort_if_out_of_memory)
{
    buddyalloc_t *ba;

    if (optional_struct_space != NULL) {
        ba = optional_struct_space;
        *optional_struct_space = (struct buddyalloc_t_){0};
    } else {
        ba = calloc(1, sizeof(*ba));
        ba->allocated_base_memory = true;
    }

    ba->abort_if_out_of_memory = abort_if_out_of_memory;
    if (allocator != NULL) {
        ba->superblock_allocator = *allocator;
    }
    return ba;
}

void
buddyalloc_delete(buddyalloc_t *ba)
{
    if (ba == NULL) {
        return;
    }
    buddyalloc_free_buffers(ba);
    if (ba->allocated_base_memory) {
        free(ba);
    }
}
