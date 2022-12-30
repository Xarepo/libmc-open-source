/*
 * Copyright (c) 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#ifndef MRX_TEST_ALLOCATOR_H
#define MRX_TEST_ALLOCATOR_H

#include <mrx_base.h>

void
mrx_test_init_allocator(size_t size);

void
mrx_test_destroy_allocator(void);

void
mrx_test_configure_allocator(bool random_lp,
                             bool use_default_lp);

void
mrx_test_clear_allocator(mrx_base_t *mrx);

void *
mrx_test_alloc_node(mrx_base_t *mrx,
                    const uint8_t nsz);

void *
mrx_test_alloc_node_alt(mrx_base_t *mrx,
                        const uint8_t nsz,
                        bool use_default_lp);

void
mrx_test_free_node(mrx_base_t *mrx,
                   void *ptr,
                   const uint8_t nsz);

size_t
mrx_test_currently_allocated_nodes(void);

#endif
