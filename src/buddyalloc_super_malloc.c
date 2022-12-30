/*
 * Copyright (c) 2011 - 2013, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <buddyalloc.h>

static void *
memalign_aligned_alloc(void *arg, unsigned int power_of_two_size)
{
    (void)arg;
    int err;
    void *ptr;
#if defined(_WIN32)
    err = 0;
    ptr = _aligned_malloc(power_of_two_size, power_of_two_size);
#else
    ptr = NULL;
    err = posix_memalign(&ptr, power_of_two_size, power_of_two_size);
#endif
    if (ptr == NULL || err != 0) {
        if (err != ENOMEM) {
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            (void)fprintf(stderr, "posix_memalign() failed: %s\n", strerror(err));
            abort();
        }
        return NULL;
    }
    return ptr;
}

static void
memalign_aligned_free(void *arg, void *ptr, unsigned int size)
{
    (void)arg;
    (void)size;
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

struct buddyalloc_superblock_allocator buddyalloc_superblock_allocator_malloc = {
    .alloc = memalign_aligned_alloc,
    .free = memalign_aligned_free,
    .arg = NULL
};
