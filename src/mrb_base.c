/*
 * Copyright (c) 2011 - 2012, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*
  The procedures implemented here are well-described in the book
  "Introduction to Algorithms" by Cormen, Leiserson and Rivest.
*/
#include <stdbool.h>
#include <stddef.h>

#include <mrb_base.h>

#define left child[0]
#define right child[1]
enum direction {
    LEFT = 0,
    RIGHT = 1
};

// Optimization: if we know the node is not NULL (per definition black) we can use the nonnil macro
// NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
#define is_nonnil_black(node)   ((node)->parent_n_color & MRB_IS_BLACK_BIT)
// NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
#define is_nonnil_red(node)     (~(node)->parent_n_color & MRB_IS_BLACK_BIT)

#define is_black(node) ((node) == NULL || ((node)->parent_n_color & MRB_IS_BLACK_BIT))
#define is_red(node)   ((node) != NULL && (~(node)->parent_n_color & MRB_IS_BLACK_BIT))

#define make_black(node) (node)->parent_n_color |= MRB_IS_BLACK_BIT
#define make_red(node)   (node)->parent_n_color &= ~MRB_IS_BLACK_BIT
#define copy_color(dest, src)                          \
    (dest)->parent_n_color =                           \
        ((dest)->parent_n_color & ~MRB_IS_BLACK_BIT) | \
        ((src)->parent_n_color & MRB_IS_BLACK_BIT);

#define parent_get(node) mrb_parent_get_(node)
#define parent_set(node, p) \
    ((node)->parent_n_color = (uintptr_t)(p) | ((node)->parent_n_color & 0x3u))

/*

  Right rotation as example, left is exactly the same but mirrored:

  before:       after:

     c            c
     |            |
     a            b
    / z          x \
   b                a
  x y              y z

*/
static inline void
rotate_nodes(struct mrb_node **root,
             struct mrb_node *a,
             const enum direction dir)
{
    struct mrb_node *b;
    struct mrb_node *c;

    c = parent_get(a);
    b = a->child[!dir];
    a->child[!dir] = b->child[dir];
    if (b->child[dir] != NULL) {
        parent_set(b->child[dir], a);
    }
    b->child[dir] = a;
    parent_set(b, c);
    if (c != NULL) {
        if (a == c->child[dir]) {
            c->child[dir] = b;
        } else {
            c->child[!dir] = b;
        }
    } else {
        *root = b;
    }
    parent_set(a, b);
}

// Maintain red-black tree coloring properties after erase (RB-Delete-Fixup page 274 in ItA book.)
static void
erase_rebalance(struct mrb_node **root,
                struct mrb_node *child, // child to erased
                struct mrb_node *parent) // parent to erased
{
    enum direction dir = LEFT;
    while (is_black(child) && child != *root) {

        /* Pick left or right. The cases are exactly mirrored so we don't have
           separate code for left and right */
        if (parent->child[dir] != child) {
            dir = !dir;
        }

        struct mrb_node *sibling = parent->child[!dir];
        if (is_nonnil_red(sibling)) {
            // Case 1
            make_black(sibling);
            make_red(parent);
            rotate_nodes(root, parent, dir);
            sibling = parent->child[!dir];
        }
        if (is_black(sibling->child[0]) && is_black(sibling->child[1])) {
            // Case 2
            make_red(sibling);
            child = parent;
            parent = parent_get(child);
        } else {
            if (is_black(sibling->child[!dir])) {
                // Case 3
                if (sibling->child[dir] != NULL) {
                    make_black(sibling->child[dir]);
                }
                make_red(sibling);
                rotate_nodes(root, sibling, !dir);
                sibling = parent->child[!dir];
            }
            // Case 4
            copy_color(sibling, parent);
            make_black(parent);
            if (sibling->child[!dir] != NULL) {
                make_black(sibling->child[!dir]);
            }
            rotate_nodes(root, parent, dir);
            child = *root;
            // at root, will break in while(), so we break already here
            break;
        }
    }
    if (child != NULL) {
        make_black(child);
    }
}

// A pseudo code description exists in book ItA "RB-Insert" page 268.
void
mrb_insert_node_(struct mrb_node **root,
                 struct mrb_node *node,
                 struct mrb_node *parent,
                 struct mrb_node **link_in_parent)
{
    enum direction dir = LEFT;

    node->parent_n_color = (uintptr_t)parent; // no bit set == red color
    node->child[LEFT] = NULL;
    node->child[RIGHT] = NULL;
    *link_in_parent = node;

    while (is_red(parent)) {
        struct mrb_node *grandp = parent_get(parent);

        // test which direction to go
        if (parent != grandp->child[dir]) {
            dir = !dir;
        }

        struct mrb_node *uncle = grandp->child[!dir];
        if (is_red(uncle)) {
            // Case 1
            make_black(uncle);
            make_black(parent);
            make_red(grandp);
            node = grandp;
        } else {
            if (parent->child[!dir] == node) {
                // Case 2
                rotate_nodes(root, parent, dir);
                struct mrb_node *tmp = parent;
                parent = node;
                node = tmp;
            }
            /* Case 3 */
            make_black(parent);
            make_red(grandp);
            rotate_nodes(root, grandp, !dir);
        }
        parent = parent_get(node);
    }
    make_black(*root);
}

// A pseudo code description exists in book ItA "RB-Delete" page 273. The order
// of this implementation is a bit messy due to optimization of comparisons.
void
mrb_erase_node_(struct mrb_node **root,
                struct mrb_node *node)
{
    struct mrb_node *echild;
    struct mrb_node *eparent;
    bool erased_is_black;

    // Remove erased node from tree by relinking, and check color of node,
    // if it's black we need to rebalance.
    if (node->left != NULL) {
        if (node->right != NULL) {
            // Two children, more complex case.

            struct mrb_node *successor;
            for (successor = node->right;
                 successor->left != NULL;
                 successor = successor->left) { }

            erased_is_black = is_nonnil_black(successor);
            echild = successor->right;
            eparent = parent_get(successor);

            // Remove successor node from the tree and put it back into the
            // tree in the place of erase-node.
            successor->parent_n_color = node->parent_n_color;
            successor->left = node->left;
            parent_set(node->left, successor);
            if (eparent == node) {
                successor->right = echild;
                eparent = successor;
            } else {
                successor->right = node->right;
                parent_set(node->right, successor);
                eparent->left = echild;
            }
            if (echild != NULL) {
                parent_set(echild, eparent);
            }

            // If erase-node had a parent, replace link in that
            struct mrb_node *tmpparent = parent_get(node);
            if (tmpparent != NULL) {
                if (tmpparent->left == node) {
                    tmpparent->left = successor;
                } else {
                    tmpparent->right = successor;
                }
            } else {
                *root = successor;
            }
            goto erase_rebalance;
        } else {
            // one child
            echild = node->left;
            eparent = parent_get(node);
            parent_set(echild, eparent);
        }
    } else if (node->right != NULL) {
        // one child
        echild = node->right;
        eparent = parent_get(node);
        parent_set(echild, eparent);
    } else {
        // zero children
        echild = NULL;
        eparent = parent_get(node);
    }
    // this is only done for the one/zero child cases
    erased_is_black = is_nonnil_black(node);
    if (eparent != NULL) {
        if (eparent->left == node) {
            eparent->left = echild;
        } else {
            eparent->right = echild;
        }
    } else {
        *root = echild;
    }

erase_rebalance:
    if (erased_is_black) {
        erase_rebalance(root, echild, eparent);
    }

    // mess up node so if it is reused illegaly we get a crash
    node->left = (struct mrb_node *)0x0000DEAD;
    node->right = (struct mrb_node *)0x0000DEAD;
    node->parent_n_color = 0x000DEAD0;
}
