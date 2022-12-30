/*
 * Copyright (c) 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#ifndef TRACKMEM_H
#define TRACKMEM_H

#include <stddef.h>
#include <stdbool.h>

struct trackmem_t_;
typedef struct trackmem_t_ trackmem_t;

trackmem_t *
trackmem_new(void);

void
trackmem_delete(trackmem_t *tm);

void
trackmem_register_alloc(trackmem_t *tm, void *ptr, size_t size);

void
trackmem_register_free(trackmem_t *tm, void *ptr, size_t size);

bool
trackmem_isallocated(trackmem_t *tm, void *ptr, size_t size);

void
trackmem_clear(trackmem_t *tm, void *base, size_t span);

#endif
