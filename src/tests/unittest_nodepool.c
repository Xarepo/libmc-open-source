/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <string.h>

#include <unittest_helpers.h>
#include <bitops.h>

#define MC_PREFIX refset
#define MC_KEY_T void *
#define MC_NO_VALUE 1
#include <mrb_tmpl.h>

#define NODEPOOL_PREFIX t1
#define NODEPOOL_NODE_TYPE uintptr_t
#define NODEPOOL_BLOCK_SIZE 512u
#include <nodepool_tmpl.h>

struct t2node {
    uint8_t data[66];
};
#define NODEPOOL_PREFIX t2
#define NODEPOOL_NODE_TYPE struct t2node
#define NODEPOOL_BLOCK_SIZE 4096
#include <nodepool_tmpl.h>

struct t3node {
    uint8_t data[512];
};
#define NODEPOOL_PREFIX t3
#define NODEPOOL_NODE_TYPE struct t3node
#define NODEPOOL_BLOCK_SIZE 16384
#include <nodepool_tmpl.h>

#define NODEPOOL_PREFIX t4
#define NODEPOOL_NODE_TYPE uintptr_t
#define NODEPOOL_BLOCK_SIZE BUDDYALLOC_ALLOC_MAX
#include <nodepool_tmpl.h>

#if TRACKMEM_DEBUG - 0 != 0
extern trackmem_t *buddyalloc_tm;
trackmem_t *buddyalloc_tm;
trackmem_t *nodepool_tm;
#endif

static uint32_t taus_state[3];

static void
nodepool_check_parameters(const struct nodepool_debug_params *p)
{
    ASSERT(bit32_count(p->block_size) == 1); // should be power of two
    ASSERT(p->node_size >= sizeof(uintptr_t));
    ASSERT(p->cache_line_size % 64 == 0);
    if (p->node_size % p->cache_line_size) {
        ASSERT(p->base_alignment == sizeof(uintptr_t) * 2);
    } else if (p->node_size > p->cache_line_size) {
        ASSERT(p->base_alignment == p->cache_line_size);
    } else {
        ASSERT(p->base_alignment == p->node_size);
    }
    if (p->base_alignment >= sizeof(struct nodepool_bh)) {
        ASSERT(p->bh_space == p->base_alignment);
    } else {
        size_t alignment = p->base_alignment;
        while (alignment < sizeof(struct nodepool_bh)) {
            alignment += p->base_alignment;
        }
        ASSERT(p->bh_space == alignment);
    }
    ASSERT(p->block_node_count == (p->block_size - p->bh_space) / p->node_size);
    ASSERT(p->block_end == p->bh_space + p->block_node_count * p->node_size);
}

static bool t1_node_is_allocated(void *ptr)
{
    return *(uintptr_t *)ptr == ~((uintptr_t)0);
}

static size_t
get_block_end(const struct nodepool_bh *bh,
              const size_t node_size,
              const size_t block_size,
              const size_t block_end)
{
    size_t bh_block_end;
    if ((((uintptr_t)bh + block_size) & (BUDDYALLOC_ALLOC_MAX - 1)) == 0) {
        bh_block_end = block_end;
        // fit at least 15 bytes at the end of a superblock (see nodepool_base.c why)
        while (block_size - bh_block_end < NODEPOOL_SUPERBLOCK_GAP) {
            bh_block_end -= node_size;
        }
    } else {
        bh_block_end = block_end;
    }
    return bh_block_end;
}

static void
nodepool_integrity_check(struct nodepool *np,
                         const struct nodepool_debug_params *p,
                         bool (*node_is_allocated)(void *))
{
    refset_t *bhdrs = refset_new(~0);

    struct nodepool_bh *bh = np->blist_head;
    struct nodepool_bh *bh_prev = bh->prev;

    const uintptr_t fresh_nodes = (np->fresh_end - np->fresh_ptr) / p->node_size;
    if (np->fresh_block != NULL) {
        size_t bh_block_end = get_block_end(np->fresh_block, p->node_size, p->block_size, p->block_end);
        ASSERT((uintptr_t)np->fresh_ptr >  (uintptr_t)np->fresh_block);
        uintptr_t fpo = (uintptr_t)np->fresh_ptr - (uintptr_t)np->fresh_block;
        ASSERT(fpo >= p->bh_space);
        ASSERT((fpo - p->bh_space) % p->node_size == 0);
        ASSERT(fpo <= bh_block_end);
        ASSERT(np->fresh_end == (uintptr_t)np->fresh_block + bh_block_end);
        ASSERT(np->blist_head == np->fresh_block);
    } else {
        ASSERT(np->fresh_ptr == np->fresh_end);
    }
    bool first_node = true;
    bool free_count_zero = false;
    do {
        ASSERT(refset_find(bhdrs, bh) == NULL);
        refset_insert(bhdrs, bh);

        size_t bh_block_end = get_block_end(bh, p->node_size, p->block_size, p->block_end);
        ASSERT(bh->free_count <= p->block_node_count);
        ASSERT(bh->prev == bh_prev);
        struct nodepool_freenode *fn = bh->freelist;
        uintptr_t free_count = 0;
        while (fn != NULL) {
            ASSERT((uintptr_t)fn > (uintptr_t)bh);
            uintptr_t fno = (uintptr_t)fn - (uintptr_t)bh;
            ASSERT(fno >= p->bh_space);
            ASSERT((fno - p->bh_space) % p->node_size == 0);
            ASSERT(fno + p->node_size <= bh_block_end);
            if (np->fresh_block != NULL) {
                ASSERT((uintptr_t)fn + p->node_size <= np->fresh_ptr || (uintptr_t)fn > np->fresh_end);
            }
            free_count++;
            fn = fn->next;
        }
        ASSERT(bh->free_count == free_count);
        if (first_node) {
            ASSERT(bh->free_count > 0 || (np->fresh_block == bh && fresh_nodes > 0));
        }
        if (free_count == 0 && !first_node) {
            free_count_zero = true;
        } else {
            ASSERT(!free_count_zero);
        }

        if (node_is_allocated != NULL) {
            uintptr_t allocated_count = 0;
            for (uintptr_t ptr = (uintptr_t)bh + p->bh_space; ptr < (uintptr_t)bh + bh_block_end; ptr += p->node_size) {
                if (ptr < np->fresh_ptr || ptr > np->fresh_end) {
                    if (node_is_allocated((void *)ptr)) {
                        allocated_count++;
                    }
                }
            }
            if (np->fresh_block == bh) {
                uintptr_t tot_count = fresh_nodes + free_count + allocated_count;
                ASSERT(tot_count == p->block_node_count);
            } else {
                ASSERT(free_count + allocated_count == p->block_node_count);
            }
            /*
            if (allocated_count < p->block_node_count) {
                fprintf(stderr, "%zd %zd %zd\n", free_count, allocated_count, p->block_node_count);
            }
            */
        }

        if (bh == np->blist_head) {
            if (np->fresh_block != NULL) {
                ASSERT(bh == np->fresh_block);
            }
        } else {
            ASSERT(bh != np->fresh_block);
        }
        first_node = false;
        bh_prev = bh;
        bh = bh->next;
    } while (bh != np->blist_head);
    //fprintf(stderr, "\n");

    refset_delete(bhdrs);
}

static void
nodepool_tests(void)
{
    fprintf(stderr, "Test: nodepool macros...");
    {
        for (size_t node_size = sizeof(uintptr_t); node_size < 600; node_size++) {
            for (size_t cache_line_size = 64; cache_line_size <= 1024; cache_line_size *= 2) {
                for (size_t block_size = BUDDYALLOC_ALLOC_MIN; block_size < BUDDYALLOC_ALLOC_MAX; block_size *= 2) {

                    size_t base_alignment = NODEPOOL_CALC_BASE_ALIGNMENT_(node_size, cache_line_size);
                    if (node_size % cache_line_size != 0) {
                        ASSERT(base_alignment == 2 * sizeof(uintptr_t));
                    } else if (node_size > cache_line_size) {
                        ASSERT(base_alignment == cache_line_size);
                    } else {
                        ASSERT(base_alignment == node_size);
                    }

                    size_t header_space = NODEPOOL_CALC_HEADER_SPACE_(base_alignment);
                    if (base_alignment > sizeof(struct nodepool_bh)) {
                        ASSERT(header_space == base_alignment);
                    } else {
                        size_t sz = sizeof(struct nodepool_bh);
                        while (sz < base_alignment) sz += sizeof(struct nodepool_bh);
                        ASSERT(header_space == sz);
                    }
                    if (header_space >= block_size) {
                        // invalid config
                        continue;
                    }

                    size_t block_node_count = (block_size - header_space) / node_size;
                    if (block_node_count < 1) {
                        // invalid config
                        continue;
                    }
                    size_t block_end = header_space + block_node_count * node_size;
                    ASSERT(block_end <= block_size);

                    size_t gap_node_count = NODEPOOL_CALC_SUPERBLOCK_GAP_NODE_COUNT_(node_size, block_size, block_end);
                    size_t end_gap = block_size - block_end;
                    if (end_gap >= NODEPOOL_SUPERBLOCK_GAP) {
                        ASSERT(gap_node_count == 0);
                    } else {
                        ASSERT(end_gap + gap_node_count * node_size >= NODEPOOL_SUPERBLOCK_GAP);
                        ASSERT(end_gap + (gap_node_count-1) * node_size < NODEPOOL_SUPERBLOCK_GAP);
                    }
                }
            }
        }
    }
    fprintf(stderr, "pass\n");

    buddyalloc_t *ba = buddyalloc_new(NULL, &buddyalloc_superblock_allocator_malloc, true);
    nodepool_mem = ba;
    fprintf(stderr, "Test: nodepool alloc/free functions...");
    {
        const int test_size = 10000;
        struct nodepool np;

        t1_nodepool_init(&np);

        struct nodepool_debug_params params;
        t1_nodepool_debug_get_params(&params);
        nodepool_check_parameters(&params);
        for (int iteration = 0; iteration < 2; iteration++) {
            void *nodes[test_size];
            memset(nodes, 0, sizeof(nodes[0]) * test_size);

            for (int i = 0; i < test_size; i++) {
                nodes[i] = t1_nodepool_alloc(&np);
                *(uintptr_t *)nodes[i] = ~((uintptr_t)0);
                nodepool_integrity_check(&np, &params, t1_node_is_allocated);

                if (tausrand(taus_state) % 10 == 0) {
                    int free_iterations = tausrand(taus_state) % 10 + 1;
                    for (int k = 0; k < free_iterations; k++) {
                        int pos = tausrand(taus_state) % (i + 1);
                        if (nodes[pos] != NULL) {
                            *(uintptr_t *)nodes[pos] = 0;
                            t1_nodepool_free(&np, nodes[pos]);
                            nodepool_integrity_check(&np, &params, t1_node_is_allocated);
                            nodes[pos] = NULL;
                        }
                    }
                }
            }

            // we do nothing with the stats here, just to get the code coverage
            struct nodepool_allocation_stats stats;
            t1_nodepool_allocation_stats(&stats, &np);

            t1_nodepool_clear(&np);
            ASSERT(np.blist_head->next == np.blist_head);
            ASSERT(np.blist_head->prev == np.blist_head);
            nodepool_integrity_check(&np, &params, t1_node_is_allocated);
        }

        t1_nodepool_delete(&np);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: nodepool with various sizes...");
    {
        struct nodepool_debug_params params;
        struct nodepool np;


        t2_nodepool_init(&np);
        t2_nodepool_debug_get_params(&params);
        nodepool_check_parameters(&params);
        t2_nodepool_delete(&np);

        t3_nodepool_init(&np);
        t3_nodepool_debug_get_params(&params);
        nodepool_check_parameters(&params);
        t3_nodepool_delete(&np);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: nodepool superblock gap test...");
    {
        struct nodepool_debug_params params;
        struct nodepool np;
        t4_nodepool_init(&np);
        void **ptrs = malloc(2*BUDDYALLOC_ALLOC_MAX / sizeof(uintptr_t) * sizeof(void *));
#if ARCH_SIZEOF_PTR == 8
        const size_t max_block_count = 524282;
#elif ARCH_SIZEOF_PTR == 4
        const size_t max_block_count = 1048568;
#endif
        for (int i = 0; i < 2 * max_block_count; i++) {
            uintptr_t *ptr = t4_nodepool_alloc(&np);
            ptrs[i] = ptr;
            memset(ptr, 0xff, sizeof(ptr) + NODEPOOL_SUPERBLOCK_GAP);
        }

        // we do nothing with the stats here, just to get the code coverage
        struct nodepool_allocation_stats stats;
        t4_nodepool_allocation_stats(&stats, &np);

        t4_nodepool_debug_get_params(&params);
        nodepool_integrity_check(&np, &params, NULL);
        size_t gap_node_count = NODEPOOL_CALC_SUPERBLOCK_GAP_NODE_COUNT_(params.node_size, params.block_size, params.block_end);
        ASSERT(gap_node_count > 0);

        // Expect three blocks
        ASSERT(np.blist_head->next->next->next == np.blist_head);
        // Expect the first block to be newly allocated with all fresh nodes free
        ASSERT(np.blist_head->free_count == 0);
        ASSERT(np.fresh_block == np.blist_head);
        ASSERT(np.fresh_end - np.fresh_ptr == max_block_count * params.node_size);
        // The two others are fully allocated
        ASSERT(np.blist_head->next->free_count == 0);
        ASSERT(np.blist_head->next->next->free_count == 0);

        for (int i = 0; i < 2 * max_block_count; i++) {
            t4_nodepool_free(&np, ptrs[i]);
        }
        nodepool_integrity_check(&np, &params, NULL);
        ASSERT(np.blist_head->next == np.blist_head);
        ASSERT(np.blist_head->free_count == 0);

        t4_nodepool_delete(&np);
        free(ptrs);
    }
    fprintf(stderr, "pass\n");

    buddyalloc_delete(ba);
}

int
main(void)
{
#if TRACKMEM_DEBUG - 0 != 0
    buddyalloc_tm = trackmem_new();
    nodepool_tm = trackmem_new();
#endif
    tausrand_init(taus_state, 0);
    nodepool_tests();
#if TRACKMEM_DEBUG - 0 != 0
    trackmem_delete(nodepool_tm);
    trackmem_delete(buddyalloc_tm);
#endif
    return 0;
}
