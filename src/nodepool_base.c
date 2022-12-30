/*
 * Copyright (c) 2011 - 2013, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*

  Design notes

  - Since standard malloc has a very small overhead (200 clock cycles or so),
    this code must be extremely streamlined to provide an advantage.
      - Performance is gained from simplicity that can be had thanks only having
        to support one size of nodes, and an underlying buddy allocator also
        suited for this specific task.
  - Fast path code implemented as inline functions in the template header to
    gain a few clock cycles while not causing too much bloat in the binary.
  - Nodes are (if possible) aligned to avoid that a single node is split over
    cache lines, which minimizes cache lines accesses in a data structure
    traversal - increase of performance and reduction of cache pollution
  - Node allocation is O(1), except for the RAM fetches from the OS, which is
    done sporadically since blocks are fetched at a time.
  - The underlying allocator used is a buddy allocator, which only allows for
    power-of-two block sizes, but also guarantees the same power-of-two
    alignment for these blocks. This is used as a feature to find the start of
    the block out of any node pointer, simply by masking away the least
    significant bits.
      - At the start of each block is a nodepool block header (struct
        nodepool_bh), which contains a single-linked free-list for free nodes
        in the local block, and next/prev pointers for a double-linked list of
        nodepool blocks.
  - A block is only freed when all nodes are free, simple but may lead to
    fragmentation in very special use cases. Reuse through the free-lists avoids
    fragmentation in normal use cases.
  - A freed node is put to the head of the single-linked free list, the LIFO
    order leads to efficient cache use (last freed is the first reused).
  - To avoid performance issues the free list is not filled when a block is
    allocated, instead a "fresh" pointer is used and new nodes are allocated
    from there when the free list of the current block is empty.
  - All blocks are kept in a double-linked list, ordered like this:
      - Only the head block uses the "fresh" pointer, all other has all nodes
        either allocated or on their free list.
      - Blocks that have non-empty free list are moved to head (or after the
        "fresh" block if there is such a block), which leads to that fully
        allocated blocks are last in the list.
      - This order is then used in the node allocation code.
  - The larger the blocks the better, less fiddling with the double-linked
    lists, but large blocks means fragmentation. Block size is chosen when the
    code is generated from the template.

 */
#include <nodepool_base.h>

static inline void
sizeof_compile_time_test(void)
{
    // this tests ensures that we have at least same gap at the start of the block
    switch(0){case 0:break;case (NODEPOOL_SUPERBLOCK_GAP <= sizeof(struct nodepool_bh)):break;} // NOLINT
}

static inline void
set_fresh_block(struct nodepool *nodepool,
                struct nodepool_bh *bh,
                const size_t bh_space,
                size_t block_end,
                const size_t block_size,
                const size_t node_size)
{
    nodepool->fresh_ptr = (uintptr_t)bh + bh_space;
    nodepool->fresh_end = (uintptr_t)bh + block_end;
    nodepool->fresh_block = bh;
    bh->freelist = NULL;
    bh->free_count = 0;
    if ((((uintptr_t)bh + block_size) & (BUDDYALLOC_ALLOC_MAX - 1)) == 0) {
        /*
          To allow for SSE and similar optimizations we exclude nodes that are too
          close to a superblock edge. This makes it possible to load unaligned
          16 byte data blocks from the last byte of the last node without risking
          getting a segmentation fault.
        */
        while (block_size - block_end < NODEPOOL_SUPERBLOCK_GAP) {
            block_end -= node_size;
        }
        nodepool->fresh_end = (uintptr_t)bh + block_end;
    }
}

void
nodepool_init_(struct nodepool *nodepool,
               buddyalloc_t *mem,
               const size_t node_size,
               const size_t block_size,
               const size_t bh_space,
               const size_t block_end)
{
    struct nodepool_bh *bh = (struct nodepool_bh *)buddyalloc_alloc(mem, block_size);
    set_fresh_block(nodepool, bh, bh_space, block_end, block_size, node_size);
    bh->next = bh;
    bh->prev = bh;
    nodepool->blist_head = bh;
}

void
nodepool_delete_(struct nodepool *nodepool,
                 buddyalloc_t *mem,
                 const size_t block_size)
{
    struct nodepool_bh *block = nodepool->blist_head;
    do {
        struct nodepool_bh *curblock = block;
        block = block->next;
        buddyalloc_free(mem, curblock, block_size);
    } while (block != nodepool->blist_head);
}

void
nodepool_clear_(struct nodepool *nodepool,
                buddyalloc_t *mem,
                const size_t node_size,
                const size_t block_size,
                const size_t bh_space,
                const size_t block_end)
{
    struct nodepool_bh *block = nodepool->blist_head->next;
    while (block != nodepool->blist_head) {
        struct nodepool_bh *curblock = block;
        block = block->next;
        buddyalloc_free(mem, curblock, block_size);
    }
    struct nodepool_bh *bh = nodepool->blist_head;
    bh->next = bh;
    bh->prev = bh;
    set_fresh_block(nodepool, bh, bh_space, block_end, block_size, node_size);
}

void
nodepool_more_nodes_(struct nodepool_bh *bh,
                     struct nodepool *nodepool,
                     buddyalloc_t *mem,
                     const size_t node_size,
                     const size_t block_size,
                     const size_t bh_space,
                     const size_t block_end)
{
    bh = bh->next;
    nodepool->fresh_block = NULL;
    if (bh->freelist != NULL) {
        // previous element becomes last in (circular) list when we move the head forward
        nodepool->blist_head = bh;
    } else {

        // all blocks full, alloc new
        bh = (struct nodepool_bh *)buddyalloc_alloc(mem, block_size);
        set_fresh_block(nodepool, bh, bh_space, block_end, block_size, node_size);

        // insert new block at the front of list
        bh->next = nodepool->blist_head;
        bh->prev = nodepool->blist_head->prev;
        nodepool->blist_head->prev->next = bh;
        nodepool->blist_head->prev = bh;
        nodepool->blist_head = bh;
    }
}

void
nodepool_block_free_(struct nodepool *nodepool,
                     buddyalloc_t *mem,
                     struct nodepool_bh *bh,
                     const size_t block_size)
{
    if (nodepool->blist_head == bh) {
        nodepool->blist_head = nodepool->blist_head->next;
    }
    bh->next->prev = bh->prev;
    bh->prev->next = bh->next;
    buddyalloc_free(mem, bh, block_size);
    if (nodepool->fresh_block == bh) {
        nodepool->fresh_block = NULL;
    }
}

void
nodepool_block_to_front_(struct nodepool *nodepool,
                         struct nodepool_bh *bh)
{
    bh->next->prev = bh->prev;
    bh->prev->next = bh->next;
    if (nodepool->fresh_block != NULL) {
        bh->next = nodepool->fresh_block->next;
        bh->prev = nodepool->fresh_block;
        nodepool->fresh_block->next->prev = bh;
        nodepool->fresh_block->next = bh;
    } else {
        bh->next = nodepool->blist_head;
        bh->prev = nodepool->blist_head->prev;
        nodepool->blist_head->prev->next = bh;
        nodepool->blist_head->prev = bh;
        nodepool->blist_head = bh;
    }
}

void
nodepool_allocation_stats_(struct nodepool_allocation_stats *stats,
                           struct nodepool *nodepool,
                           const size_t node_size,
                           const size_t block_size,
                           const size_t bh_space,
                           const size_t block_end)
{
    struct nodepool_bh *bh = nodepool->blist_head;

    *stats = (struct nodepool_allocation_stats){0};
    do {
        stats->superblock_size += block_size;
        if ((((uintptr_t)bh + block_size) & (BUDDYALLOC_ALLOC_MAX - 1)) == 0) {
            size_t be = block_end;
            while (block_size - be < NODEPOOL_SUPERBLOCK_GAP) {
                be -= node_size;
            }
            stats->overhead_size += block_size - (be - bh_space);
        } else {
            stats->overhead_size += block_size - (block_end - bh_space);
        }
        stats->free_size += bh->free_count * node_size;
        stats->block_count++;
        bh = bh->next;
    } while (bh != nodepool->blist_head);
    if (nodepool->fresh_block != NULL) {
        stats->free_size += nodepool->fresh_end - nodepool->fresh_ptr;
    }
}
