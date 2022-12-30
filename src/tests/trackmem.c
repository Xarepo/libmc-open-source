/*
 * Copyright (c) 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <stdio.h>
#include <pthread.h>

#include <trackmem.h>

#define MC_PREFIX ref2sz
#define MC_KEY_T void *
#define MC_VALUE_T size_t
#include <mrb_tmpl.h>

struct trackmem_t_ {
    ref2sz_t *allocated;
    ref2sz_t *freed;
    pthread_mutex_t mutex;
};

trackmem_t *
trackmem_new()
{
    trackmem_t *tm = calloc(1, sizeof(*tm));
    tm->allocated = ref2sz_new(~0);
    tm->freed = ref2sz_new(~0);
    pthread_mutex_init(&tm->mutex, NULL);
    return tm;
}

void
trackmem_delete(trackmem_t *tm)
{
    if (tm == NULL) {
        return;
    }
    ref2sz_delete(tm->allocated);
    ref2sz_delete(tm->freed);
    free(tm);
}

void
trackmem_register_alloc(trackmem_t *tm, void *ptr, size_t size)
{
    pthread_mutex_lock(&tm->mutex);
    ref2sz_it_t *it = ref2sz_itfind(tm->allocated, ptr);
    if (it != NULL) {
        fprintf(stderr, "%p is already allocated (allocated size %zd, this size %zd)\n", ptr, ref2sz_val(it), size);
        abort();
    }
    ref2sz_erase(tm->freed, ptr);
    ref2sz_insert(tm->allocated, ptr, size);
    pthread_mutex_unlock(&tm->mutex);
}

void
trackmem_register_free(trackmem_t *tm, void *ptr, size_t size)
{
    if (ptr == NULL) {
        return;
    }
    pthread_mutex_lock(&tm->mutex);
    ref2sz_it_t *it = ref2sz_itfind(tm->allocated, ptr);
    if (it == NULL) {
        it = ref2sz_itfind(tm->freed, ptr);
        if (it == NULL) {
            fprintf(stderr, "tried to free %p which is not allocated or newly freed\n", ptr);
        } else {
            fprintf(stderr, "tried to free %p which is newly freed (freed size %zd, this size %zd)\n", ptr, ref2sz_val(it), size);
        }
        abort();
    } else if (ref2sz_val(it) != size) {
        fprintf(stderr, "tried to free %p with size %zd but was allocated with size %zd\n", ptr, size, ref2sz_val(it));
        abort();
    }
    ref2sz_iterase(tm->allocated, it);
    ref2sz_insert(tm->freed, ptr, size);
    pthread_mutex_unlock(&tm->mutex);
}

bool
trackmem_isallocated(trackmem_t *tm, void *ptr, size_t size)
{
    pthread_mutex_lock(&tm->mutex);
    ref2sz_it_t *it = ref2sz_itfind(tm->allocated, ptr);
    if (it == NULL) {
        pthread_mutex_unlock(&tm->mutex);
        return false;
    }
    if (size != 0 && ref2sz_val(it) != size) {
        fprintf(stderr, "%p expected to have size %zd but is allocated with size %zd\n", ptr, size, ref2sz_val(it));
        abort();
    }
    pthread_mutex_unlock(&tm->mutex);
    return true;
}

void
trackmem_clear(trackmem_t *tm, void *base_, size_t span)
{
    pthread_mutex_lock(&tm->mutex);
    if (base_ == NULL) {
        ref2sz_clear(tm->allocated);
        ref2sz_clear(tm->freed);
    } else {
        uintptr_t base = (uintptr_t)base_;
        for (int i = 0; i < 2; i++) {
            ref2sz_t *tt = i == 0 ? tm->allocated : tm->freed;
            ref2sz_it_t *it = ref2sz_begin(tt);
            while (it != NULL) {
                uintptr_t ptr = (uintptr_t)ref2sz_key(it);
                if (ptr >= base && ptr + ref2sz_val(it) <= base + span) {
                    it = ref2sz_iterase(tt, it);
                } else {
                    it = ref2sz_next(it);
                }
            }
        }
    }
    pthread_mutex_unlock(&tm->mutex);
}

