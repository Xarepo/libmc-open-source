/*
 * Copyright (c) 2011 - 2013, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#ifndef NODEPOOL_BASE_H
#define NODEPOOL_BASE_H

#include <buddyalloc.h>

#ifdef TRACKMEM_DEBUG
#include <trackmem.h>
extern trackmem_t *nodepool_tm;
#endif

#define NODEPOOL_SUPERBLOCK_GAP 15

struct nodepool_debug_params {
    size_t node_size;
    size_t block_size;
    size_t bh_space;
    size_t block_end;
    size_t block_node_count;
    size_t base_alignment;
    size_t cache_line_size;
};

struct nodepool {
    struct nodepool_bh *fresh_block;
    uintptr_t fresh_ptr;
    uintptr_t fresh_end;
    struct nodepool_bh *blist_head;
};

struct nodepool_freenode {
    struct nodepool_freenode *next;
};

struct nodepool_bh {
    /* The buddy allocator has reserved the LSB of first pointer which must be
       zero. By having a pointer as the first member we make sure that this will
       be the case (always aligned to at least 4 bytes) */
    struct nodepool_bh *next;
    uintptr_t free_count;
    struct nodepool_freenode *freelist;
    struct nodepool_bh *prev;
};

extern buddyalloc_t *nodepool_mem;

void
nodepool_init_(struct nodepool *nodepool,
               buddyalloc_t *mem,
               size_t node_size,
               size_t block_size,
               size_t bh_space,
               size_t block_end);

void
nodepool_delete_(struct nodepool *nodepool,
                 buddyalloc_t *mem,
                 size_t block_size);

void
nodepool_clear_(struct nodepool *nodepool,
                buddyalloc_t *mem,
                size_t node_size,
                size_t block_size,
                size_t bh_space,
                size_t block_end);

void
nodepool_more_nodes_(struct nodepool_bh *bh,
                     struct nodepool *nodepool,
                     buddyalloc_t *mem,
                     size_t node_size,
                     size_t block_size,
                     size_t bh_space,
                     size_t block_end);

void
nodepool_block_free_(struct nodepool *nodepool,
                     buddyalloc_t *mem,
                     struct nodepool_bh *bh,
                     size_t block_size);

void
nodepool_block_to_front_(struct nodepool *nodepool,
                         struct nodepool_bh *bh);

struct nodepool_allocation_stats {
    size_t superblock_size; // total allocated from buddy allocator
    size_t overhead_size; // headers and padding
    size_t free_size;
    size_t block_count;
};

void
nodepool_allocation_stats_(struct nodepool_allocation_stats *stats,
                           struct nodepool *nodepool,
                           size_t node_size,
                           size_t block_size,
                           size_t bh_space,
                           size_t block_end);

#endif
