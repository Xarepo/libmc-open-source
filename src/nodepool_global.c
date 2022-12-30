/*
 * Copyright (c) 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <nodepool_base.h>

static buddyalloc_t nodepool_mem_ = BUDDYALLOC_INITIALIZER(0);
buddyalloc_t *nodepool_mem = &nodepool_mem_;

static void __attribute__ ((destructor))
nodepool_destructor(void)
{
    buddyalloc_free_buffers(nodepool_mem);
}
