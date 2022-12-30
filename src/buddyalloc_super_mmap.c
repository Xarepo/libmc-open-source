/*
 * Copyright (c) 2011 - 2013, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

// source defines for MAP_ANONYMOUS / MAP_ANON
#define _GNU_SOURCE // NOLINT
#define _DARWIN_C_SOURCE // NOLINT
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#define MAP_FAILED 0
static int
munmap(void *addr, size_t length)
{
    if (VirtualFree(addr, length, MEM_DECOMMIT) == 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}
#else
#include <sys/mman.h>
#endif

#ifndef  MAP_ANONYMOUS
#ifdef MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

#include <buddyalloc.h>

static void *
mmap_aligned_alloc_int(unsigned int power_of_two_size)
{
    const size_t sz = power_of_two_size;
    const uintptr_t sz_mask = sz - 1u;

    // For windows we skip first attempt on aligned mmap(), tests on Windows 10
    // show that we always(?) get unaligned memory.
#ifndef _WIN32
    // We first make a quick alloc and hope it to be aligned (quite likely),
    // and if not we do a slower but guaranteed method.
    uintptr_t ptr = (uintptr_t)mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == (uintptr_t)MAP_FAILED) {
        if (errno != ENOMEM) {
            fprintf(stderr, "mmap() failed: %s\n", strerror(errno)); // NOLINT
            abort();
        }
        return NULL;
    }
    if ((ptr & sz_mask) == 0) {
        return (void *)ptr;
    }
    munmap((void *)ptr, sz);
#endif // ifndef _WIN32

#ifdef _WIN32
    uintptr_t ptr = (uintptr_t)VirtualAlloc(0, sz << 1u, MEM_COMMIT, PAGE_READWRITE);
#else
    ptr = (uintptr_t)mmap(NULL, sz << 1u, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    if (ptr == (uintptr_t)MAP_FAILED) {
        if (errno != ENOMEM) {
            fprintf(stderr, "mmap() failed: %s\n", strerror(errno)); // NOLINT
            abort();
        }
        return NULL;
    }
    if ((ptr & sz_mask) == 0) {
        munmap((void *)(ptr + sz), sz);
        return (void *)ptr;
    }
    uintptr_t oldptr = ptr;
    uintptr_t newptr = (oldptr + sz) & ~sz_mask;
    munmap((void *)oldptr, newptr - oldptr);
    munmap((void *)(newptr + sz), sz - (newptr - oldptr));
    return (void *)newptr;
}

static void *
mmap_aligned_alloc(void *arg, unsigned int size)
{
    (void)arg;
    void *ptr = mmap_aligned_alloc_int(size);
    if (ptr == NULL) {
        return NULL;
    }
#ifdef MADV_HUGEPAGE // available on recent Linux
    // enable TLB hugepage if available, we don't care to check the result
    (void)madvise(ptr, BUDDYALLOC_ALLOC_MAX, MADV_HUGEPAGE);
#endif
    return ptr;
}

static void
mmap_aligned_free(void *arg, void *ptr, unsigned int size)
{
    (void)arg;
    if (munmap(ptr, size) == -1) {
        fprintf(stderr, "munmap() failed: %s\n", strerror(errno)); // NOLINT
        abort();
    }
}

struct buddyalloc_superblock_allocator buddyalloc_superblock_allocator_default = {
    .alloc = mmap_aligned_alloc,
    .free = mmap_aligned_free,
    .arg = NULL
};
