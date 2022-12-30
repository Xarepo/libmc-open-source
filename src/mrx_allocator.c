/*
 * Copyright (c) 2013, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*
  The allocator uses the nodepool allocator to get its 128 byte superblocks. These 128 byte superblocks
  are then subdivided using internal buddy allocator to smaller power of two sizes.

  To not waste any data on headers, the alignment of pointers to at least 8 bytes is used meaning that
  three bits of the pointer is unused and instead reserved to store free bit and block size.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <mrx_base_int.h>

#if MAX_P2 != 4u
#error "this code expects MAX_P2 to be 4 (max node size 128)"
#endif

struct node128 { uint8_t raw[128]; };
#define NODEPOOL_PREFIX node128
#define NODEPOOL_BASE_ALIGNMENT 128u
#define NODEPOOL_NODE_TYPE struct node128
#define NODEPOOL_BLOCK_SIZE 32768u
#include <nodepool_tmpl.h>

static inline void mrx_node_sizeof_verify_(void)
{switch (0) {case 0: break;case (sizeof(union mrx_node) == 128): break;}} // NOLINT

// size: 00: 8, 01: 16, 10: 32, 11: 64
struct free_block {
    // 1 freebit , 2 size bits, rest pointer
#define NEXT_PTR(n) (struct free_block *)((n) & ~((uintptr_t)0x7u))
#define NSZ(n) ((n) & 0x3u)
#define FREEBIT ((uintptr_t)0x4u)
#define INIT_NEXT_SZ_FREEBIT(n, sz, fb) ((uintptr_t)(n) | (sz) | (((uintptr_t)!!(fb)) << 2u))
    uintptr_t next_sz_freebit;
    struct free_block *prev;
};

static inline void
superblock_free(mrx_base_t *mrx,
                void *ptr)
{
    node128_nodepool_free(&mrx->nodealloc->superblocks, ptr);
}

static inline void *
superblock_alloc(mrx_base_t *mrx)
{
    return node128_nodepool_alloc(&mrx->nodealloc->superblocks);
}

static inline bool
try_erase_from_freelist(struct mrx_buddyalloc *ba,
                        void *ptr,
                        const uint_fast8_t p2)
{
    struct free_block *prev;
    struct free_block *next;

    struct free_block *cur = (struct free_block *)ptr;
    // fast path, test if block is the head in the freelist
    if (ba->freelists[p2-MIN_P2] == cur) {
        next = NEXT_PTR(cur->next_sz_freebit);
        ba->freelists[p2-MIN_P2] = next;
        if (next == NULL) {
            // list became empty
            ba->nonempty_freelists &= ~(1u << p2);
        }
        return true;
    }

    // test if free, and if expected block size
    if ((cur->next_sz_freebit & FREEBIT) == 0 || NSZ(cur->next_sz_freebit) != (unsigned)p2) {
        return false;
    }

    prev = cur->prev;
    next = NEXT_PTR(cur->next_sz_freebit);
    prev->next_sz_freebit = INIT_NEXT_SZ_FREEBIT(next, p2, 1u);
    if (next != NULL) {
        next->prev = prev;
    }
    return true;
}

static inline void
push_to_freelist(struct mrx_buddyalloc *ba,
                 void *ptr,
                 const uint_fast8_t p2)
{
    struct free_block *oldhead = (struct free_block *)ba->freelists[p2-MIN_P2];
    struct free_block *newhead = (struct free_block *)ptr;
    newhead->next_sz_freebit = INIT_NEXT_SZ_FREEBIT(oldhead, p2, 1u);
    ba->freelists[p2-MIN_P2] = newhead;
    if (oldhead == NULL) {
        ba->nonempty_freelists |= 1u << p2;
    } else {
        oldhead->prev = newhead;
    }
}

static inline void *
pop_from_freelist(struct mrx_buddyalloc *ba,
                  const uint_fast8_t p2)
{
    struct free_block *head = (struct free_block *)ba->freelists[p2-MIN_P2];
    struct free_block *next = NEXT_PTR(head->next_sz_freebit);
    ba->freelists[p2-MIN_P2] = next;
    head->next_sz_freebit &= ~FREEBIT;
    if (next == NULL) {
        ba->nonempty_freelists &= ~(1u << p2);
    }
    return head;
}

void *
mrx_alloc_node_(mrx_base_t *mrx,
                const uint8_t nsz)
{
#if ARCH_SIZEOF_PTR == 8
    assert(nsz != 0);
#endif
    if ((mrx->max_keylen_n_flags & MRX_FLAG_IS_COMPACT_) != 0) {
        return malloc(8u << ((nsz > MAX_P2) ? MAX_P2 : nsz));
    }
    void *ptr;
    if (nsz > 3) {
        return superblock_alloc(mrx);
    }
    uint_fast8_t p2 = nsz;
    uint_fast8_t free_p2;

    uint32_t nonempty = mrx->nodealloc->nonempty_freelists;
    nonempty &= ~((1u << p2)  - 1);
    if (nonempty != 0) {
        free_p2 = bit32_bsf(nonempty);
        ptr = pop_from_freelist(mrx->nodealloc, free_p2);
        if (free_p2 == p2) {
            return ptr;
        }
    } else {
        ptr = superblock_alloc(mrx);
        free_p2 = MAX_P2;
    }

    // push unused to freelists
    for (; p2 < free_p2; p2++) {
        void *buddy = (void *)((uintptr_t)ptr + (8u << p2));
        push_to_freelist(mrx->nodealloc, buddy, p2);
    }
    return ptr;
}

void
mrx_free_node_(mrx_base_t *mrx,
               void *ptr,
               const uint8_t nsz)
{
#if ARCH_SIZEOF_PTR == 8
    assert(nsz != 0);
#endif
    if ((mrx->max_keylen_n_flags & MRX_FLAG_IS_COMPACT_) != 0) {
        free(ptr);
        return;
    }
    if (nsz >= MAX_P2) {
        superblock_free(mrx, ptr);
        return;
    }
    uint_fast8_t p2 = nsz;
    do {
        void *buddy;
        void *base;
        // Buddy is to the left or right? Use pointer alignment to find out.
        if (((uintptr_t)ptr & ((8u << (p2 + 1u)) - 1)) == 0) {
            buddy = (void *)((uintptr_t)ptr + (8u << p2));
            base = ptr;
        } else {
            buddy = (void *)((uintptr_t)ptr - (8u << p2));
            base = buddy;
        }
        if (!try_erase_from_freelist(mrx->nodealloc, buddy, p2)) {
            // buddy was allocated or split into an allocated/unallocated part
            break;
        }
        // buddy erased from free list, join to larger block
        ptr = base;
        p2++;
    } while (p2 < MAX_P2);
    if (p2 < MAX_P2) {
        push_to_freelist(mrx->nodealloc, ptr, p2);
    } else {
        superblock_free(mrx, ptr);
    }
}

void
mrx_init_(mrx_base_t *mrx,
          const size_t capacity,
          const bool compact)
{
    mrx->root = NULL;
    mrx->count = 0;
    mrx->capacity = (uintptr_t)capacity;
    mrx->max_keylen_n_flags = compact ? MRX_FLAG_IS_COMPACT_ : 0;
    if ((mrx->max_keylen_n_flags & MRX_FLAG_IS_COMPACT_) == 0) {
        mrx->nodealloc->nonempty_freelists = 0;
        for (size_t i = 0; i < sizeof(mrx->nodealloc->freelists)/sizeof(mrx->nodealloc->freelists[0]); i++) {
            mrx->nodealloc->freelists[i] = 0;
        }
        node128_nodepool_init(&mrx->nodealloc->superblocks);
    }
}

void
mrx_clear_(mrx_base_t *mrx)
{
    if ((mrx->max_keylen_n_flags & MRX_FLAG_IS_COMPACT_) == 0) {
        mrx->nodealloc->nonempty_freelists = 0;
        for (size_t i = 0; i < sizeof(mrx->nodealloc->freelists)/sizeof(mrx->nodealloc->freelists[0]); i++) {
            mrx->nodealloc->freelists[i] = 0;
        }
        node128_nodepool_clear(&mrx->nodealloc->superblocks);
    } else {
        mrx_traverse_erase_all_nodes_(mrx);
    }
    mrx->root = NULL;
    mrx->count = 0;
    mrx->max_keylen_n_flags = (mrx->max_keylen_n_flags & MRX_FLAGS_MASK_);
}

void
mrx_delete_(mrx_base_t *mrx)
{
    if ((mrx->max_keylen_n_flags & MRX_FLAG_IS_COMPACT_) == 0) {
        node128_nodepool_delete(&mrx->nodealloc->superblocks);
    } else {
        mrx_traverse_erase_all_nodes_(mrx);
    }
}

void
mrx_alloc_debug_stats(mrx_base_t *mrx,
                      struct mrx_debug_allocator_stats *stats)
{
    *stats = (struct mrx_debug_allocator_stats){0};
    if ((mrx->max_keylen_n_flags & MRX_FLAG_IS_COMPACT_) != 0) {
        return;
    }
    size_t freelist_sz = 0;
    const uint32_t nonempty = mrx->nodealloc->nonempty_freelists;
    for (uint_fast8_t p2 = MIN_P2; p2 <= MAX_P2 - 1; p2++) {
        if ((nonempty & (1u << p2)) != 0) {
            struct free_block *p = (struct free_block *)
                mrx->nodealloc->freelists[p2-MIN_P2];
            do {
                freelist_sz += 8u << p2;
                p = NEXT_PTR(p->next_sz_freebit);
            } while (p != NULL);
        }
    }
    struct nodepool_allocation_stats npstats;
    node128_nodepool_allocation_stats(&npstats, &mrx->nodealloc->superblocks);
    stats->freelist_size = freelist_sz + npstats.free_size;
    stats->unused_superblock_size = npstats.overhead_size;
}
