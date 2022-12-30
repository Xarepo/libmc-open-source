/*
 * Copyright (c) 2011 - 2012, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*
  This is the template for a fast block-based node allocator, meant to be used
  by data structures that have a large amount of fixed sized nodes.
 */
#ifndef NODEPOOL_TMPL_ONCE_
#define NODEPOOL_TMPL_ONCE_

#include <stdint.h>

#include <nodepool_base.h>

#ifndef ARCH_SIZEOF_PTR
#if UINTPTR_MAX == 4294967295u
#define ARCH_SIZEOF_PTR 4
#elif UINTPTR_MAX == 18446744073709551615u
#define ARCH_SIZEOF_PTR 8
#else
 #error "Unsupported size of UINTPTR_MAX"
#endif
#endif // ARCH_SIZEOF_PTR


#define NODEPOOL_CONCAT_EVAL_(a, b) a ## b
#define NODEPOOL_CONCAT_(a, b) NODEPOOL_CONCAT_EVAL_(a, b)

// convert 0 or 1 => 0 or ~0
#define NODEPOOL_BOOL_TO_MASK_(expr) (-(uintptr_t)(expr))

// choose_a ? num_a : num_b for numbers without using branch
#define NODEPOOL_CHOOSE_NUM_(choose_a, num_a, num_b) \
    (((num_a) & NODEPOOL_BOOL_TO_MASK_(!!(choose_a))) | ((num_b) & NODEPOOL_BOOL_TO_MASK_(!(choose_a))))

#define NODEPOOL_CALC_SUPERBLOCK_GAP_NODE_COUNT_(node_size, block_size, block_end) \
    NODEPOOL_CHOOSE_NUM_((block_size) - (block_end) < NODEPOOL_SUPERBLOCK_GAP,     \
                         (NODEPOOL_SUPERBLOCK_GAP - ((block_size) - (block_end)) + (node_size) - 1) / (node_size), 0)


#define NODEPOOL_CALC_HEADER_SPACE_(base_alignment)                             \
    NODEPOOL_CHOOSE_NUM_((base_alignment) >= sizeof(struct nodepool_bh),        \
                         (base_alignment),                                      \
                         ((sizeof(struct nodepool_bh) + (base_alignment) - 1) / \
                          (base_alignment)) * (base_alignment))
/*
  Optimize node alignment of first node in the block in relation to cache line
  size. All nodes are always packed in an array after eachother, but if we
  choose a good starting alignment we can in the best case make sure that no
  node is split over more than one cache line and thus make traversals more
  cache efficient.

    - If node size is divisable with cache line size, then align at node size if
      it is smaller than cache line size, else at cache line size.
    - If node size is not divisable with cache line size, just use a small
      alignment 2 times the pointer size.
 */
#define NODEPOOL_CALC_BASE_ALIGNMENT_(node_size, cache_line_size)       \
    NODEPOOL_CHOOSE_NUM_(                                               \
        (node_size) % (cache_line_size) != 0,                           \
        2 * ARCH_SIZEOF_PTR,                                            \
        NODEPOOL_CHOOSE_NUM_((node_size) > (cache_line_size), (cache_line_size), (node_size)))

#endif // NODEPOOL_TMPL_ONCE_

#ifdef MC_CACHE_LINE_SIZE
#define NODEPOOL_CACHE_LINE_SIZE MC_CACHE_LINE_SIZE
#else
#define NODEPOOL_CACHE_LINE_SIZE 64
#endif

#ifdef MC_PREFIX
#define NODEPOOL_FUN_(name) \
    NODEPOOL_CONCAT_(NODEPOOL_CONCAT_(MC_PREFIX, _), name)
#else
#define NODEPOOL_FUN_(name) \
    NODEPOOL_CONCAT_(NODEPOOL_CONCAT_(NODEPOOL_PREFIX, _), name)
#endif

#ifndef NODEPOOL_BASE_ALIGNMENT
#define NODEPOOL_BASE_ALIGNMENT \
    NODEPOOL_CALC_BASE_ALIGNMENT_(sizeof(NODEPOOL_NODE_TYPE), NODEPOOL_CACHE_LINE_SIZE)
#endif

#ifndef NODEPOOL_BLOCK_SIZE
 #error "NODEPOOL_BLOCK_SIZE not defined"
#endif
#if NODEPOOL_BLOCK_SIZE < BUDDYALLOC_ALLOC_MIN
 #error "NODEPOOL_BLOCK_SIZE < BUDDYALLOC_ALLOC_MIN"
#endif
#if NODEPOOL_BLOCK_SIZE > BUDDYALLOC_ALLOC_MAX
 #error "NODEPOOL_BLOCK_SIZE > BUDDYALLOC_ALLOC_MAX"
#endif
#ifndef NODEPOOL_NODE_TYPE
 #error "NODEPOOL_NODE_TYPE not defined"
#endif

static inline void
NODEPOOL_FUN_(sizeof_compile_time_test_)(void)
{
    // if this fails at compile time, the node size is too small, it must be at least sizeof(void *)
    switch(0){case 0:break;case sizeof(NODEPOOL_NODE_TYPE)>=sizeof(void *):break;}
    // if this fails at compile time, NODEPOOL_BLOCK_SIZE is not a power of two
    switch(0){case 0:break;case ((NODEPOOL_BLOCK_SIZE) & ((NODEPOOL_BLOCK_SIZE) - 1)) == 0:break;}
}

#define NODEPOOL_BLOCK_HEADER_SPACE                                                      \
    NODEPOOL_CALC_HEADER_SPACE_(NODEPOOL_BASE_ALIGNMENT)

#define NODEPOOL_BLOCK_NODE_COUNT                          \
    ((NODEPOOL_BLOCK_SIZE - NODEPOOL_BLOCK_HEADER_SPACE) / sizeof(NODEPOOL_NODE_TYPE))

#define NODEPOOL_BLOCK_END \
    (NODEPOOL_BLOCK_HEADER_SPACE + NODEPOOL_BLOCK_NODE_COUNT * sizeof(NODEPOOL_NODE_TYPE))

#define NODEPOOL_SUPERBLOCK_GAP_NODE_COUNT                                              \
    NODEPOOL_CALC_SUPERBLOCK_GAP_NODE_COUNT_(sizeof(NODEPOOL_NODE_TYPE),                \
                                             NODEPOOL_BLOCK_SIZE, NODEPOOL_BLOCK_END)

static inline void
NODEPOOL_FUN_(nodepool_init)(struct nodepool *nodepool)
{
    nodepool_init_(nodepool, nodepool_mem,
                   sizeof(NODEPOOL_NODE_TYPE), NODEPOOL_BLOCK_SIZE,
                   NODEPOOL_BLOCK_HEADER_SPACE, NODEPOOL_BLOCK_END);
}

static inline void
NODEPOOL_FUN_(nodepool_delete)(struct nodepool *nodepool)
{
#if TRACKMEM_DEBUG - 0 != 0
    {
        struct nodepool_bh *bh = nodepool->blist_head;
        do {
            trackmem_clear(nodepool_tm, bh, NODEPOOL_BLOCK_SIZE);
            bh = bh->next;
        } while (bh != nodepool->blist_head);
    }
#endif
    nodepool_delete_(nodepool, nodepool_mem, NODEPOOL_BLOCK_SIZE);
}

static inline void
NODEPOOL_FUN_(nodepool_clear)(struct nodepool *nodepool)
{
#if TRACKMEM_DEBUG - 0 != 0
    {
        struct nodepool_bh *bh = nodepool->blist_head;
        do {
            trackmem_clear(nodepool_tm, bh, NODEPOOL_BLOCK_SIZE);
            bh = bh->next;
        } while (bh != nodepool->blist_head);
    }
#endif
    nodepool_clear_(nodepool, nodepool_mem,
                    sizeof(NODEPOOL_NODE_TYPE), NODEPOOL_BLOCK_SIZE,
                    NODEPOOL_BLOCK_HEADER_SPACE, NODEPOOL_BLOCK_END);
}

static inline NODEPOOL_NODE_TYPE *
NODEPOOL_FUN_(nodepool_alloc)(struct nodepool *nodepool)
{
    NODEPOOL_NODE_TYPE *node;
    struct nodepool_bh *bh = nodepool->blist_head;
    if (bh->free_count > 0) {
        node = (NODEPOOL_NODE_TYPE *)bh->freelist;
        bh->freelist = bh->freelist->next;
        bh->free_count--;
    } else {
        node = (NODEPOOL_NODE_TYPE *)nodepool->fresh_ptr;
        nodepool->fresh_ptr += sizeof(NODEPOOL_NODE_TYPE);
    }
    if (bh->free_count == 0 && nodepool->fresh_ptr == nodepool->fresh_end) {
        nodepool_more_nodes_(bh,
                             nodepool, nodepool_mem,
                             sizeof(NODEPOOL_NODE_TYPE),
                             NODEPOOL_BLOCK_SIZE,
                             NODEPOOL_BLOCK_HEADER_SPACE,
                             NODEPOOL_BLOCK_END);
    }
#if TRACKMEM_DEBUG - 0 != 0
    trackmem_register_alloc(nodepool_tm, node, sizeof(NODEPOOL_NODE_TYPE));
#endif
    return node;
}

static inline void
NODEPOOL_FUN_(nodepool_free)(struct nodepool *nodepool,
                             NODEPOOL_NODE_TYPE *node)
{
    struct nodepool_bh *bh;

#if TRACKMEM_DEBUG - 0 != 0
    trackmem_register_free(nodepool_tm, node, sizeof(NODEPOOL_NODE_TYPE));
#endif
    bh = (struct nodepool_bh *)((uintptr_t)node & ~((uintptr_t)NODEPOOL_BLOCK_SIZE - 1));
    bh->free_count++;
    if (bh->free_count < NODEPOOL_BLOCK_NODE_COUNT -
        (NODEPOOL_BOOL_TO_MASK_((((uintptr_t)bh + NODEPOOL_BLOCK_SIZE) & (BUDDYALLOC_ALLOC_MAX - 1)) == 0) &
         NODEPOOL_SUPERBLOCK_GAP_NODE_COUNT) ||
        nodepool->blist_head->next == nodepool->blist_head)
    {
        struct nodepool_freenode *oldhead = bh->freelist;
        bh->freelist = (struct nodepool_freenode *)((uintptr_t)node);
        bh->freelist->next = oldhead;
        if (oldhead == NULL && nodepool->blist_head != bh) {
            nodepool_block_to_front_(nodepool, bh);
        }
    } else {
        nodepool_block_free_(nodepool, nodepool_mem, bh, NODEPOOL_BLOCK_SIZE);
    }
}

static inline void
NODEPOOL_FUN_(nodepool_allocation_stats)(struct nodepool_allocation_stats *stats,
                                         struct nodepool *nodepool)
{
    nodepool_allocation_stats_(stats, nodepool, sizeof(NODEPOOL_NODE_TYPE),
                               NODEPOOL_BLOCK_SIZE,
                               NODEPOOL_BLOCK_HEADER_SPACE, NODEPOOL_BLOCK_END);
}

static inline void
NODEPOOL_FUN_(nodepool_debug_get_params)(struct nodepool_debug_params *p)
{
    p->node_size = sizeof(NODEPOOL_NODE_TYPE);
    p->block_size = NODEPOOL_BLOCK_SIZE;
    p->bh_space = NODEPOOL_BLOCK_HEADER_SPACE;
    p->block_end = NODEPOOL_BLOCK_END;
    p->block_node_count = NODEPOOL_BLOCK_NODE_COUNT;
    p->base_alignment = NODEPOOL_BASE_ALIGNMENT;
    p->cache_line_size = NODEPOOL_CACHE_LINE_SIZE;
}

#undef NODEPOOL_PREFIX
#undef NODEPOOL_NODE_TYPE
#undef NODEPOOL_BASE_ALIGNMENT
#undef NODEPOOL_BLOCK_SIZE
#undef NODEPOOL_BLOCK_HEADER_SPACE
#undef NODEPOOL_BLOCK_NODE_COUNT
#undef NODEPOOL_BLOCK_END
#undef NODEPOOL_CACHE_LINE_SIZE
#undef NODEPOOL_SUPERBLOCK_GAP_NODE_COUNT
#undef NODEPOOL_FUN_
