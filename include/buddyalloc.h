/*
 * Copyright (c) 2011 - 2013, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*
  This is a buddy memory allocator, meant to be used in very special use cases,
  typically for block-allocating nodes in data structures. For normal all-around
  use, standard malloc is better.

  CANNOT BE USED AS DROP-IN REPLACEMENT FOR MALLOC!

  Properties:

  - Only sizes of the power of two can be allocated.
  - No header on blocks, so an "free bit" is reserved and must be taken into
    account by the user.
  - All blocks will be fully aligned, 4096 block at 4096 borders,
    512K at 512K etc.
  - Multi-thread features: lock-contention free and better performance than
    (GNU) libc malloc in many cases.
  - Superblock large enough to fit huge TLB entries for typical platforms.
 */
#ifndef BUDDYALLOC_H
#define BUDDYALLOC_H

#include <stdatomic.h> // available in C11
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void *(*buddyalloc_aligned_alloc_t)(void *, unsigned int);
typedef void (*buddyalloc_aligned_free_t)(void *, void *, unsigned int);
struct buddyalloc_superblock_allocator {
    buddyalloc_aligned_alloc_t alloc;
    buddyalloc_aligned_free_t free;
    void *arg;
};

/* The default allocator can be set/changed by setting pointers in runtime before first allocation.
   It's also possible to set superblock allocator individually per buddy allocator. */
extern struct buddyalloc_superblock_allocator buddyalloc_superblock_allocator_default;
extern struct buddyalloc_superblock_allocator buddyalloc_superblock_allocator_malloc;

// struct buddyalloc_t_ should not be accessed directly by user!
typedef struct buddyalloc_t_ {
    atomic_bool lock;
#define BUDDYALLOC_MIN_SIZE_LOG2_ 5u
#define BUDDYALLOC_MAX_SIZE_LOG2_ 22u
#define BUDDYALLOC_FREELIST_SIZE (BUDDYALLOC_MAX_SIZE_LOG2_-BUDDYALLOC_MIN_SIZE_LOG2_)
    // freelist contains every block size except the largest
    atomic_uintptr_t lockfree_freelists[BUDDYALLOC_FREELIST_SIZE];
    uint32_t nonempty_normal_freelists;
    void *normal_freelists[BUDDYALLOC_FREELIST_SIZE];
    bool abort_if_out_of_memory;
    bool allocated_base_memory;
    struct buddyalloc_superblock_allocator superblock_allocator;
    atomic_uint_fast32_t superblock_count;
    atomic_uintptr_t free_superblock;
} buddyalloc_t;

// will initialize with default allocator
#define BUDDYALLOC_INITIALIZER(abort_if_out_of_memory_)              \
    {                                                                \
        .lock = 0,                                                   \
        .lockfree_freelists = {0},                                   \
        .nonempty_normal_freelists = 0,                              \
        .normal_freelists = {0},                                     \
        .abort_if_out_of_memory = (abort_if_out_of_memory_),         \
        .allocated_base_memory = false,                              \
        .superblock_allocator = {0},                                 \
        .superblock_count = 0,                                       \
        .free_superblock = 0                                         \
    }

#define BUDDYALLOC_ALLOC_MIN (1u << BUDDYALLOC_MIN_SIZE_LOG2_) // 32 bytes
#define BUDDYALLOC_ALLOC_MAX (1u << BUDDYALLOC_MAX_SIZE_LOG2_) // 4 MB
/* Important note: blocks are headerless and therefore a free bit must be stored
   inside the block, which is in the LSB of the first pointer. An allocated
   block must ALWAYS HAVE THIS BIT SET TO ZERO. A typical way to do this is to
   use this to allocate space for structs which have a pointer to aligned
   memory (ie most memory) as the first member. */
void *
buddyalloc_alloc(buddyalloc_t *ba,
                 size_t size);

void
buddyalloc_free(buddyalloc_t *ba,
                void *ptr,
                size_t size);

void
buddyalloc_free_buffers(buddyalloc_t *ba);

/* If optional_struct_space is passed it will used for storing the base structure.
   When buddyalloc is later deleted, it will not be freed so it may point to static
   memory. */
buddyalloc_t *
buddyalloc_new(struct buddyalloc_t_ *optional_struct_space,
               const struct buddyalloc_superblock_allocator *optional_allocator,
               bool abort_if_out_of_memory);

void
buddyalloc_delete(buddyalloc_t *ba);

#endif
