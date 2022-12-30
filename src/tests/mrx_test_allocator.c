/*
 * Copyright (c) 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>

#include <mrx_tmpl.h>
#include <mrx_base_int.h>
#include <mrx_test_allocator.h>
#include <unittest_helpers.h>

struct node_info {
    size_t size;
};

#define MC_PREFIX ptr2node
#define MC_KEY_T void *
#define MC_VALUE_T struct node_info
#define MC_VALUE_RETURN_REF 1
#include <mrb_tmpl.h>

struct dumb_allocator {
    uint8_t *buf;
    size_t buf_size;
    size_t offset;
};

struct {
    struct dumb_allocator allocator[2];
    uint32_t taus_state[3];
    int allocator_idx;
    bool use_random_allocator;
    ptr2node_t *ptr2node;
    ptr2node_t *ptr2node_freed;
} glob;

static void *
allocate_mem(size_t size)
{
    int ai;
    if (glob.use_random_allocator) {
        ai = tausrand(glob.taus_state) % 2;
    } else {
        ai = glob.allocator_idx;
    }
    struct dumb_allocator *a = &glob.allocator[ai];
    if (a->offset == 0) {
        memset(&a->buf[a->offset], 0xFEu, 2 * sizeof(void *));
        a->offset += 2 * sizeof(void *);
    }
    void *data = &a->buf[a->offset];
    a->offset += size;
    memset(&a->buf[a->offset], 0xFEu, 2 * sizeof(void *));
    a->offset += 2 * sizeof(void *);
    if (a->offset > a->buf_size) {
        fprintf(stderr, "Test allocator ran out of memory. Increase buffer size to support what the unit tests needs.\n");
        fprintf(stderr, "Allocated blocks %lu, buf size %lu, offset %lu\n", (long)ptr2node_size(glob.ptr2node), (long)a->buf_size, (long)a->offset);
        abort();
    }
    //fprintf(stderr, "alloc %p %zu\n", data, size);
    return data;
}

void
mrx_test_init_allocator(size_t size)
{
    void *base_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#if ARCH_SIZEOF_PTR == 4
    void *other_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#else
    void *ref_addr = base_addr;
    void *other_addr = NULL;
    for (unsigned i = 0; i < 100; i++) {
        ref_addr = (void *)((uintptr_t)ref_addr + 0x000100000000);
        other_addr = mmap(ref_addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (other_addr == (void *)MAP_FAILED) {
            continue;
        }
        if (((uintptr_t)base_addr ^ (uintptr_t)other_addr) >> 32 != 0) {
            break;
        }
        munmap(other_addr, size);
    }
    if (other_addr == (void *)MAP_FAILED || ((uintptr_t)base_addr ^ (uintptr_t)other_addr) >> 32 == 0) {
        fprintf(stderr, "Failed to allocate two different spaces for the test allocator\n");
        abort();
    }
#endif
    tausrand_init(glob.taus_state, 0);
    glob.allocator[0].buf = base_addr;
    glob.allocator[1].buf = other_addr;
    fprintf(stderr, "Allocated two test blocks %p and %p\n", base_addr, other_addr);
    glob.allocator[0].buf_size = size;
    glob.allocator[1].buf_size = size;
    glob.ptr2node = ptr2node_new(~0u);
    glob.ptr2node_freed = ptr2node_new(~0u);
}

void
mrx_test_destroy_allocator(void)
{
    munmap(glob.allocator[0].buf, glob.allocator[0].buf_size);
    munmap(glob.allocator[1].buf, glob.allocator[1].buf_size);
    ptr2node_delete(glob.ptr2node);
    ptr2node_delete(glob.ptr2node_freed);
}

void
mrx_test_configure_allocator(bool random_lp,
                             bool use_default_lp)
{
    glob.use_random_allocator = random_lp;
    glob.allocator_idx = use_default_lp ? 0 : 1;
}

void
mrx_test_clear_allocator(mrx_base_t *mrx)
{
    glob.use_random_allocator = false;
    glob.allocator_idx = 0;
    glob.allocator[0].offset = 0;
    glob.allocator[1].offset = 0;
    ptr2node_clear(glob.ptr2node);
    ptr2node_clear(glob.ptr2node_freed);
}

size_t
mrx_test_currently_allocated_nodes(void)
{
    /*
    for (ptr2node_it_t *it = ptr2node_begin(glob.ptr2node); it != NULL ; it = ptr2node_next(it)) {
        fprintf(stderr, "%p %zu\n", ptr2node_key(it), ptr2node_val(it)->size);
    }
    */
    return ptr2node_size(glob.ptr2node);
}

void *
mrx_test_alloc_node(mrx_base_t *mrx,
                    const uint8_t nsz)
{
    size_t size = nsz > 3 ? 128u : 8u << (unsigned)nsz;
    uint8_t *data = allocate_mem(size);
    memset(data, 0xFF, size);
    data[0] = 0xF8u | nsz;
    ptr2node_insert(glob.ptr2node, data, (struct node_info){ size });
    //fprintf(stderr, "Allocated blocks %lu\n", (long)ptr2node_size(glob.ptr2node));
    return data;
}

void *
mrx_test_alloc_node_alt(mrx_base_t *mrx,
                        const uint8_t nsz,
                        bool use_default_lp)
{
    int ai = glob.allocator_idx;
    bool random_lp = glob.use_random_allocator;
    glob.use_random_allocator = false;
    glob.allocator_idx = use_default_lp ? 0 : 1;
    void *ptr = mrx_test_alloc_node(mrx, nsz);
    glob.allocator_idx = ai;
    glob.use_random_allocator = random_lp;
    return ptr;
}

void
mrx_test_free_node(mrx_base_t *mrx,
                   void *ptr,
                   const uint8_t nsz)
{
    size_t size = nsz > 3 ? 128u : 8u << (unsigned)nsz;
    ptr2node_it_t *it = ptr2node_itfind(glob.ptr2node_freed, ptr);
    if (it != NULL) {
        fprintf(stderr, "bad free: double free node pointer %p (current size %zu, old size %zu) \n",
                ptr, size, ptr2node_val(it)->size);
        abort();
    }
    it = ptr2node_itfind(glob.ptr2node, ptr);
    if (it == NULL) {
        fprintf(stderr, "bad free: bad node pointer %p (%zu), not found\n", ptr, size);
        abort();
    }
    struct node_info *ni = ptr2node_val(it);
    if (size != ni->size) {
        fprintf(stderr, "bad free: bad node pointer %p, bad size expected %zu got %zu\n",  ptr, size, ni->size);
        abort();
    }
    {
        for (int i = 0; i < 2 * sizeof(void *); i++) {
            if (((const uint8_t *)ptr)[-2 * sizeof(void *) + i] != 0xFEu) {
                fprintf(stderr, "trashed data: trashed data before pointer %p\n",  ptr);
                abort();
            }
            if (((const uint8_t *)ptr)[size + i] != 0xFEu) {
                fprintf(stderr, "trashed data: trashed data after pointer %p\n",  ptr);
                abort();
            }
        }
    }
    //fprintf(stderr, "free %p %zu\n", ptr, size);
    ptr2node_iterase(glob.ptr2node, it);
    ptr2node_insert(glob.ptr2node_freed, ptr, (struct node_info){ size });
}
