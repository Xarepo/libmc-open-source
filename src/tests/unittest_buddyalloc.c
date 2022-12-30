/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <string.h>
#include <pthread.h>

#include <unittest_helpers.h>

#include <bitops.h>
#include <buddyalloc.h>
#include <trackmem.h>

#define MC_PREFIX refset
#define MC_KEY_T void *
#define MC_NO_VALUE 1
#include <mrb_tmpl.h>

static uint32_t taus_state[3];
static uint64_t dummy = 0;

buddyalloc_t ba_static = BUDDYALLOC_INITIALIZER(0);
extern trackmem_t *buddyalloc_tm;
trackmem_t *buddyalloc_tm;

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

static void
scan_block(const void *block, size_t size)
{
    // read block to get valgrind warnings if there are bad allocations
    ASSERT(size % 8 == 0);
    const uint64_t *ptr = (const uint64_t *)block;
    for (size_t i = 0; i < size/8; i++) {
        dummy ^= ptr[i];
    }
}

static void
buddyalloc_integrity_check(buddyalloc_t *ba)
{
    refset_t *refset = refset_new(~0);

    ASSERT(!atomic_load(&ba->lock));
    for (int i = 0; i < BUDDYALLOC_FREELIST_SIZE; i++) {
        int p2 = BUDDYALLOC_MIN_SIZE_LOG2_ + i;
        if (bit32_isset(&ba->nonempty_normal_freelists, p2)) {
            ASSERT(ba->normal_freelists[i] != NULL);
            struct free_block *prev = NULL;
            for (struct free_block *node = ba->normal_freelists[i]; node != NULL; node = node->next) {
                if (node != ba->normal_freelists[i]) { // prev link is not set on head node
                    ASSERT(node->prev == prev);
                }
                ASSERT(refset_find(refset, node) == NULL);
                refset_insert(refset, node);
                ASSERT(node->p2 == p2);
                ASSERT(node->free_lsb == 1);
                scan_block(node, 1 << node->p2);
                prev = node;
            }
        } else {
            ASSERT(ba->normal_freelists[i] == NULL);
        }
        for (struct lockfree_free_block *node = (struct lockfree_free_block *)atomic_load(&ba->lockfree_freelists[i]);
             node != NULL;
             node = (struct lockfree_free_block *)atomic_load(&node->next))
        {
            ASSERT(refset_find(refset, node) == NULL);
            refset_insert(refset, node);
            ASSERT(node->p2 == p2);
            ASSERT(node->free_lsb == 0);
            scan_block(node, 1 << node->p2);
        }
    }

    {
        uintptr_t ptr = atomic_load(&ba->free_superblock);
        if (ptr != 0) {
            ASSERT(refset_find(refset, (void *)ptr) == NULL);
            scan_block((void *)ptr, BUDDYALLOC_ALLOC_MAX);
        }
    }
    refset_delete(refset);
}

static void *
alloc_thread(void *arg)
{
    buddyalloc_t *ba = (buddyalloc_t *)arg;
    const int test_size = 1000;
    struct {
        size_t size;
        void *ptr;
    } blocks[test_size];
    uint32_t ts[3];

    tausrand_init(ts, 0);
    memset(blocks, 0, sizeof(blocks[0]) * test_size);

    for (int i = 0; i < test_size; i++) {
        int p2 = tausrand(ts) % BUDDYALLOC_FREELIST_SIZE + BUDDYALLOC_MIN_SIZE_LOG2_ + 1;
        blocks[i].size = 1u << p2;
        blocks[i].ptr = buddyalloc_alloc(ba, blocks[i].size);

        if (tausrand(ts) % 5 == 0) {
            int pos = tausrand(ts) % (i + 1);
            buddyalloc_free(ba, blocks[pos].ptr, blocks[pos].size);
            blocks[pos].ptr = NULL;
            blocks[pos].size = 0;
        }
    }

    return NULL;
}

static struct {
    refset_t *ptrs;
} custom_alloc_test;

static void *
memalign_aligned_alloc(void *arg, unsigned int power_of_two_size)
{
    ASSERT(arg == (void *)1);
    ASSERT(power_of_two_size == BUDDYALLOC_ALLOC_MAX);
    void *ptr;
#if defined(_WIN32)
    ptr = _aligned_malloc(power_of_two_size, power_of_two_size);
#else
    (void)posix_memalign(&ptr, power_of_two_size, power_of_two_size);
#endif
    // make sure code doesn't depend on zeroed header
    *(uintptr_t *)ptr = ~((uintptr_t)0);
    refset_insert(custom_alloc_test.ptrs, ptr);
    return ptr;
}

static void
memalign_aligned_free(void *arg, void *ptr, unsigned int size)
{
    ASSERT(ptr != NULL);
    ASSERT(refset_find(custom_alloc_test.ptrs, ptr) != NULL);
    ASSERT(arg == (void *)1);
    ASSERT(size == BUDDYALLOC_ALLOC_MAX);
    refset_erase(custom_alloc_test.ptrs, ptr);
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

static void
buddyalloc_tests(void)
{
    fprintf(stderr, "Test: new/delete with provided struct space...");
    {
        buddyalloc_t ba_base, *ba;
        ba = buddyalloc_new(&ba_base, NULL, true);
        ASSERT(ba == &ba_base);
        ASSERT(ba_base.abort_if_out_of_memory);
        ASSERT(!ba_base.allocated_base_memory);
        buddyalloc_integrity_check(ba);
        buddyalloc_delete(&ba_base);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: custom superblock allocator...");
    {
        struct buddyalloc_superblock_allocator baalloc = {
            .alloc = memalign_aligned_alloc,
            .free = memalign_aligned_free,
            .arg = (void *)1
        };
        buddyalloc_t *ba = buddyalloc_new(NULL, &baalloc, true);
        custom_alloc_test.ptrs = refset_new(~0);
        const int test_size = 10;
        void *ptrs[test_size];
        for (int i = 0; i < test_size; i++) {
            void *ptr = buddyalloc_alloc(ba, BUDDYALLOC_ALLOC_MAX);
            ASSERT(refset_find(custom_alloc_test.ptrs, ptr) != NULL);
            ptrs[i] = ptr;
        }
        ASSERT(refset_size(custom_alloc_test.ptrs) == test_size);
        for (int i = 0; i < test_size; i++) {
            buddyalloc_free(ba, ptrs[i], BUDDYALLOC_ALLOC_MAX);
        }
        ASSERT(refset_size(custom_alloc_test.ptrs) == 1); // one spare block
        refset_delete(custom_alloc_test.ptrs);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: alloc/free patterns...");
    {
        struct buddyalloc_superblock_allocator baalloc = {
            .alloc = memalign_aligned_alloc,
            .free = memalign_aligned_free,
            .arg = (void *)1
        };
        buddyalloc_t *ba = buddyalloc_new(NULL, &baalloc, true);

        custom_alloc_test.ptrs = refset_new(~0);
        ASSERT(buddyalloc_alloc(ba, 0) == NULL);
        for (ssize_t base_sz = 1; base_sz <= BUDDYALLOC_ALLOC_MAX; base_sz *= 2) {
            for (ssize_t sz = base_sz - 20 < 1 ? 1 : base_sz - 20;
                 sz < base_sz + 20 && sz <= BUDDYALLOC_ALLOC_MAX;
                 sz++)
            {
                void *ptr = buddyalloc_alloc(ba, sz);
                ASSERT((*(uintptr_t *)ptr & 1) == 0);
                buddyalloc_integrity_check(ba);
                ASSERT(ptr != NULL);
                buddyalloc_free(ba, ptr, sz);
                buddyalloc_integrity_check(ba);
            }
        }
        buddyalloc_delete(ba);
        refset_delete(custom_alloc_test.ptrs);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: random alloc/free...");
    {
        // use the secondary malloc allocator so that gets tested too
        buddyalloc_t *ba = buddyalloc_new(NULL, &buddyalloc_superblock_allocator_malloc, true);
        const int test_size = 1000;
        struct {
            size_t size;
            void *ptr;
        } blocks[test_size];

        memset(blocks, 0, sizeof(blocks[0]) * test_size);

        for (int i = 0; i < test_size; i++) {
            int p2 = tausrand(taus_state) % BUDDYALLOC_FREELIST_SIZE + BUDDYALLOC_MIN_SIZE_LOG2_ + 1;
            blocks[i].size = 1u << p2;
            blocks[i].ptr = buddyalloc_alloc(ba, blocks[i].size);

            if (tausrand(taus_state) % 10 == 0) {
                int pos = tausrand(taus_state) % (i + 1);
                buddyalloc_free(ba, blocks[pos].ptr, blocks[pos].size);
                blocks[pos].ptr = NULL;
                blocks[pos].size = 0;
            }
            buddyalloc_integrity_check(ba);
        }
        for (int i = 0; i < test_size; i++) {
            if (blocks[i].ptr != NULL) {
                buddyalloc_free(ba, blocks[i].ptr, blocks[i].size);
                buddyalloc_integrity_check(ba);
            }
        }
        buddyalloc_delete(ba);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: multithread random alloc/free...");
    {
        buddyalloc_t *ba = buddyalloc_new(NULL, NULL, true);
        const int thread_count = 5;
        pthread_t thread_id[thread_count];
        for (int i = 0; i < thread_count; i++) {
            pthread_create(&thread_id[i], NULL, alloc_thread, ba);
        }
        for (int i = 0; i < thread_count; i++) {
            pthread_join(thread_id[i], NULL);
        }
        buddyalloc_integrity_check(ba);
        buddyalloc_delete(ba);
    }
    fprintf(stderr, "pass\n");
}

int
main(void)
{
    buddyalloc_tm = trackmem_new();
    tausrand_init(taus_state, 0);
    buddyalloc_tests();
    fprintf(stderr, "Next line will generate valgrind errors as allocated but uninitialized memory has been scanned\n");
    fprintf(stderr, "(dummy: %llu)\n", (long long)dummy); // print to avoid optimizer removing code
    trackmem_delete(buddyalloc_tm);
    return 0;
}
