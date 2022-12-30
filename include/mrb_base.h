/*
 * Copyright (c) 2011 - 2012, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#ifndef MRB_BASE_H
#define MRB_BASE_H

#include <stdint.h>

struct mrb_node {
    struct mrb_node *child[2]; // 0 == left, 1 == right
#define MRB_IS_BLACK_BIT ((uintptr_t)1u) // make use of aligned pointers to save space
    uintptr_t parent_n_color;
};

#define mrb_parent_get_(node) ((struct mrb_node *)((node)->parent_n_color & ~MRB_IS_BLACK_BIT))

void
mrb_insert_node_(struct mrb_node **root,
                 struct mrb_node *node,
                 struct mrb_node *parent,
                 struct mrb_node **link_in_parent);

void
mrb_erase_node_(struct mrb_node **root,
                struct mrb_node *node);

#endif
