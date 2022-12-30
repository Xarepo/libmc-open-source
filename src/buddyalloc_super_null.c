/*
 * Copyright (c) 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <buddyalloc.h>

struct buddyalloc_superblock_allocator buddyalloc_superblock_allocator_default = {0};
struct buddyalloc_superblock_allocator buddyalloc_superblock_allocator_malloc = {0};
