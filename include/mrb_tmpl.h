/*
 * Copyright (c) 2011 - 2012 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*
  Red-black tree.

  Compile-time options / tuning:

  MRB_KEYCMP(result, a, b) - the key comparison macro. Default is set to
  "result = a - b", or "(a > b) - (a < b)" when there can be overflow.
  'a' is the key in the tree, 'b' is the key given as the function argument.
  The result has the type 'intptr_t', and the macro must fulfill transitivity,
  or else the tree may break. Transitivity means that if a <= b and b <= c then
  a <= c. For example, the comparison 'result = a - b' does not work if the
  subtraction can overflow (wrap around).

  Default configuration:

  Map with 'intptr_t' keys to 'void *', with NULL as undefined value.

  Other configurations:

  MRB_PRESET_const_str_TO_REF_COPY_KEY
    - constant strings to void *, key is copied/freed
    - str2ref default name
    - sorted in alphanumeric order

  MRB_PRESET_const_str_COPY
    - constant strings, copied. No value.
    - strset default name
    - sorted in alphanumeric order
*/

#ifdef MRB_PRESET_const_str_TO_REF_COPY_KEY
#include <string.h>
#ifndef MC_PREFIX
#define MC_PREFIX str2ref
#endif
#define MC_KEY_T const char *
#define MC_VALUE_T void *
#define MC_COPY_KEY(dest, src) dest = strdup(src)
#define MC_FREE_KEY(key) free(MC_DECONST(void *, key))
#define MRB_KEYCMP(result, key1, key2) result = strcmp(key1, key2)
#undef MRB_PRESET_const_str_TO_REF_COPY_KEY
#endif

#ifdef MRB_PRESET_const_str_COPY
#include <string.h>
#ifndef MC_PREFIX
#define MC_PREFIX strset
#endif
#define MC_KEY_T const char *
#define MC_NO_VALUE 1
#define MC_COPY_KEY(dest, src) dest = strdup(src)
#define MC_FREE_KEY(key) free(MC_DECONST(void *, key))
#define MRB_KEYCMP(result, key1, key2) result = strcmp(key1, key2)
#undef MRB_PRESET_const_str_COPY
#endif

#ifndef MC_PREFIX
#define MC_PREFIX mrb
#define MC_KEY_T intptr_t
#define MC_VALUE_T void *
#endif

#define MC_ASSOCIATIVE_CONTAINER_ 1
#define MC_MM_DEFAULT_BLOCK_SIZE_ 16384
#define MC_MM_DEFAULT_ MC_MM_COMPACT
#define MC_MM_SUPPORT_ (MC_MM_PERFORMANCE | MC_MM_STATIC | MC_MM_COMPACT)
#include <mc_tmpl.h>

#ifndef MRB_TMPL_ONCE_
#define MRB_TMPL_ONCE_
#include <mrb_base.h>
#endif // MRB_TMPL_ONCE_

#define MRB_LEFT_ child[0]
#define MRB_RIGHT_ child[1]

#define MRB_NODE_KV MC_CONCAT_(MC_PREFIX, _node_kv)
struct MRB_NODE_KV {
    struct mrb_node node;
    MC_KEY_T key;
#if MC_NO_VALUE - 0 == 0
    MC_VALUE_T value;
#endif
};

#ifndef MRB_KEYCMP
#if __has_builtin(__builtin_choose_expr)
#define MRB_KEYCMP(result, key1, key2)                  \
    __builtin_choose_expr(                              \
        sizeof(key1) < sizeof(intptr_t),                \
        (result) = (intptr_t)(key1) - (intptr_t)(key2), \
        (result) = ((key1) > (key2)) - ((key1) < (key2)))
#else
/* Any sane compiler should optimize away the static sizeof comparison, when
   optimization is enabled */
#define MRB_KEYCMP(result, key1, key2)                                  \
    result = (sizeof(key1) < sizeof(intptr_t)) ?                        \
        (intptr_t)(key1) - (intptr_t)(key2) : ((key1) > (key2)) - ((key1) < (key2))
#endif // __has_builtin(__builtin_choose_expr)
#endif // MRB_KEYCMP

#if MC_MM_MODE == MC_MM_STATIC

#define MRB_ALLOC_NODE_(mrb) \
    MC_FUN_(npstatic_alloc)(&mrb->nodepool);
#define MRB_FREE_NODE_(mrb, node) \
    MC_FUN_(npstatic_free)(&mrb->nodepool, node);

#define NPSTATIC_NODE_TYPE struct MRB_NODE_KV
#include <npstatic_tmpl.h>

#endif // MC_MM_MODE == MC_MM_STATIC

#if MC_MM_MODE == MC_MM_COMPACT

#define MRB_ALLOC_NODE_(mrb) malloc(sizeof(struct MRB_NODE_KV));
#define MRB_FREE_NODE_(mrb, node) free(node);

#endif // MC_MM_MODE == MC_MM_COMPACT

#if MC_MM_MODE == MC_MM_PERFORMANCE

#define MRB_ALLOC_NODE_(mrb) MC_FUN_(nodepool_alloc)(&mrb->nodepool);
#define MRB_FREE_NODE_(mrb, node) MC_FUN_(nodepool_free)(&mrb->nodepool, node);

#define NODEPOOL_NODE_TYPE struct MRB_NODE_KV
#define NODEPOOL_BLOCK_SIZE MC_MM_BLOCK_SIZE
#include <nodepool_tmpl.h>

#endif // MC_MM_MODE == MC_MM_PERFORMANCE

typedef struct MC_T_ {
    struct mrb_node *root;
    uintptr_t count;
    uintptr_t capacity;
#if MC_MM_MODE == MC_MM_PERFORMANCE
    struct nodepool nodepool;
    uintptr_t pad_[1];
#endif
#if MC_MM_MODE == MC_MM_STATIC
    struct npstatic nodepool;
    uintptr_t pad_[2];
#endif
} MC_T;

#if MC_MM_MODE == MC_MM_STATIC || MC_MM_MODE == MC_MM_PERFORMANCE
static inline void
MC_FUN_(sizeof_verify_)(void)
{
    switch (0) {
    case 0: break;
    case (64 % sizeof(MC_T) == 0): break;
    }
}
#endif // MC_MM_MODE == MC_MM_STATIC || MC_MM_MODE == MC_MM_PERFORMANCE

static inline MC_ITERATOR_T *
MC_FUN_(begin)(MC_T * const mrb)
{
    struct mrb_node *node;

    node = mrb->root;
    if (node == NULL) {
        return NULL;
    }
    while (node->MRB_LEFT_ != NULL) {
        node = node->MRB_LEFT_;
    }
    return (MC_ITERATOR_T *)node;
}

static inline MC_ITERATOR_T *
MC_FUN_(rbegin)(MC_T * const mrb)
{
    struct mrb_node *node;

    node = mrb->root;
    if (node == NULL) {
        return NULL;
    }
    while (node->MRB_RIGHT_ != NULL) {
        node = node->MRB_RIGHT_;
    }
    return (MC_ITERATOR_T *)node;
}

static inline MC_ITERATOR_T *
MC_FUN_(end)(void)
{
    return NULL;
}

static inline MC_ITERATOR_T *
MC_FUN_(rend)(void)
{
    return NULL;
}

static inline MC_ITERATOR_T *
MC_FUN_(next_compact_delete_)(MC_ITERATOR_T * const it)
{
    struct mrb_node *node = (struct mrb_node *)it;
    struct mrb_node *parent;

    if (mrb_parent_get_(node) == node) {
        free(node);
        return NULL;
    }
    if (node->MRB_RIGHT_ != NULL) {
        node = node->MRB_RIGHT_;
        while (node->MRB_LEFT_ != NULL) {
            node = node->MRB_LEFT_;
        }
        return (MC_ITERATOR_T *)node;
    }
    while ((parent = mrb_parent_get_(node)) != NULL &&
           node == parent->MRB_RIGHT_)
    {
        free(node);
        node = parent;
    }
    free(node);
    return (MC_ITERATOR_T *)parent;
}

static inline MC_ITERATOR_T *
MC_FUN_(next)(MC_ITERATOR_T * const it)
{
    struct mrb_node *node = (struct mrb_node *)it;
    struct mrb_node *parent;

    if (mrb_parent_get_(node) == node) {
        return NULL;
    }
    if (node->MRB_RIGHT_ != NULL) {
        node = node->MRB_RIGHT_;
        while (node->MRB_LEFT_ != NULL) {
            node = node->MRB_LEFT_;
        }
        return (MC_ITERATOR_T *)node;
    }
    while ((parent = mrb_parent_get_(node)) != NULL &&
           node == parent->MRB_RIGHT_)
    {
        node = parent;
    }
    return (MC_ITERATOR_T *)parent;
}

static inline MC_ITERATOR_T *
MC_FUN_(prev)(MC_ITERATOR_T * const it)
{
    struct mrb_node *node = (struct mrb_node *)it;
    struct mrb_node *parent;

    if (mrb_parent_get_(node) == node) {
        return NULL;
    }
    if (node->MRB_LEFT_ != NULL) {
        node = node->MRB_LEFT_;
        while (node->MRB_RIGHT_ != NULL) {
            node = node->MRB_RIGHT_;
        }
        return (MC_ITERATOR_T *)node;
    }
    while ((parent = mrb_parent_get_(node)) != NULL &&
           node == parent->MRB_LEFT_)
    {
        node = parent;
    }
    return (MC_ITERATOR_T *)parent;
}

static inline void
MC_FUN_(clear_nodes_)(MC_T * const mrb)
{
#if defined(MC_FREE_KEY) || defined(MC_FREE_VALUE) ||MC_MM_MODE == MC_MM_COMPACT
    MC_ITERATOR_T *next_it;
    MC_ITERATOR_T *it = MC_FUN_(begin)(mrb);
    while (it != MC_FUN_(end)()) {
        MC_OPT_FREE_KEY_(((struct MRB_NODE_KV *)it)->key);
#if MC_NO_VALUE - 0 == 0
        MC_OPT_FREE_VALUE_(((struct MRB_NODE_KV *)it)->value);
#endif
#if MC_MM_MODE == MC_MM_COMPACT
        next_it = MC_FUN_(next_compact_delete_)(it);
#else
        next_it = MC_FUN_(next)(it);
#endif
        it = next_it;
    }
#endif
}

#if MC_MM_MODE == MC_MM_COMPACT || MC_MM_MODE == MC_MM_PERFORMANCE

static inline MC_T *
MC_FUN_(new)(const size_t capacity)
{
    MC_T *mrb;

#if MC_MM_MODE == MC_MM_PERFORMANCE
    mrb = buddyalloc_alloc(nodepool_mem, sizeof(MC_T));
    MC_FUN_(nodepool_init)(&mrb->nodepool);
#endif
#if MC_MM_MODE == MC_MM_COMPACT
    mrb = malloc(sizeof(MC_T));
#endif
    mrb->root = NULL;
    mrb->count = 0;
    mrb->capacity = capacity;
    return mrb;
}

static inline void
MC_FUN_(delete)(MC_T * const mrb)
{
    if (mrb == NULL) {
        return;
    }
    MC_FUN_(clear_nodes_)(mrb);
#if MC_MM_MODE == MC_MM_PERFORMANCE
    MC_FUN_(nodepool_delete)(&mrb->nodepool);
    buddyalloc_free(nodepool_mem, mrb, sizeof(MC_T));
#else
    free(mrb);
#endif
}

static inline void
MC_FUN_(clear)(MC_T * const mrb)
{
    MC_FUN_(clear_nodes_)(mrb);
#if MC_MM_MODE == MC_MM_PERFORMANCE
    MC_FUN_(nodepool_clear)(&mrb->nodepool);
#endif
    mrb->root = NULL;
    mrb->count = 0;
}

#endif // MC_MM_MODE == MC_MM_COMPACT || MC_MM_MODE == MC_MM_PERFORMANCE

#if MC_MM_MODE == MC_MM_STATIC

static inline MC_T *
MC_FUN_(init)(MC_T * const mrb,
              struct MRB_NODE_KV * const nodes,
              const size_t sizeof_node_array)
{
    mrb->root = NULL;
    mrb->count = 0;
    mrb->capacity = (uintptr_t)sizeof_node_array / sizeof(struct MRB_NODE_KV);
    MC_FUN_(npstatic_init)(&mrb->nodepool, nodes);
    return mrb;
}

static inline MC_T *
MC_FUN_(new)(const size_t capacity)
{
    MC_T *mrb;

    if (posix_memalign((void **)&mrb, sizeof(MC_T),
                       sizeof(MC_T) + capacity * sizeof(struct MRB_NODE_KV))
        != 0)
    {
        return NULL;
    }
    return MC_FUN_(init)(mrb, (struct MRB_NODE_KV *)((uintptr_t)&mrb[1]),
                         capacity * sizeof(struct MRB_NODE_KV));
}

static inline void
MC_FUN_(delete)(MC_T * const mrb)
{
    if (mrb == NULL) {
        return;
    }
    free(mrb);
}

static inline void
MC_FUN_(clear)(MC_T * const mrb)
{
    MC_FUN_(clear_nodes_)(mrb);
    MC_FUN_(npstatic_clear)(&mrb->nodepool);
    mrb->count = 0;
    mrb->root = NULL;
}

#endif // MC_MM_MODE == MC_MM_STATIC

static inline int
MC_FUN_(empty)(MC_T * const mrb)
{
    return (mrb->count == 0);
}

static inline size_t
MC_FUN_(size)(MC_T * const mrb)
{
    return mrb->count;
}

static inline size_t
MC_FUN_(max_size)(MC_T * const mrb)
{
    return mrb->capacity;
}

static inline MC_ITERATOR_T *
MC_FUN_(itinsert)(MC_T * const mrb,
                  MC_KEY_T const key MC_OPT_VALUE_INSERT_ARG_)
{
    struct mrb_node **new;
    struct mrb_node *parent;
    struct MRB_NODE_KV *newnode;
    intptr_t result;

    if (mrb->count == mrb->capacity) {
        return NULL;
    }
    new = &mrb->root;
    parent = NULL;
    while (*new != NULL) {
        MRB_KEYCMP(result, ((struct MRB_NODE_KV *)*new)->key, key);
        if (result == 0) {
#if MC_NO_VALUE - 0 == 0
            MC_OPT_FREE_VALUE_(((struct MRB_NODE_KV *)*new)->value);
            MC_OPT_ASSIGN_VALUE_(((struct MRB_NODE_KV *)*new)->value, value);
            return (MC_ITERATOR_T *)(*new);
#else
            return (MC_ITERATOR_T *)(*new);
#endif
        }
        parent = *new;
        if (result > 0) {
            new = &(*new)->MRB_LEFT_;
        } else {
            new = &(*new)->MRB_RIGHT_;
        }
    }
    newnode = MRB_ALLOC_NODE_(mrb);
    MC_ASSIGN_KEY_(newnode->key, key);
#if MC_NO_VALUE - 0 == 0
    MC_OPT_ASSIGN_VALUE_(newnode->value, value);
#endif
    mrb->count++;
    mrb_insert_node_(&mrb->root, &newnode->node, parent, new);
    return (MC_ITERATOR_T *)newnode;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(insert)(MC_T * const mrb,
                MC_KEY_T const key MC_OPT_VALUE_INSERT_ARG_)
{
    MC_DEF_VALUE_UNDEF_;
    struct mrb_node **new;
    struct mrb_node *parent;
    struct MRB_NODE_KV *newnode;
    intptr_t result;

    if (mrb->count == mrb->capacity) {
        return undef_value;
    }
    new = &mrb->root;
    parent = NULL;
    while (*new != NULL) {
        MRB_KEYCMP(result, ((struct MRB_NODE_KV *)*new)->key, key);
        if (result == 0) {
#if MC_NO_VALUE - 0 == 0
            MC_OPT_FREE_VALUE_(((struct MRB_NODE_KV *)*new)->value);
            MC_OPT_ASSIGN_VALUE_(((struct MRB_NODE_KV *)*new)->value, value);
            return MC_OPT_ADDROF_ ((struct MRB_NODE_KV *)*new)->value;
#else
            return MC_OPT_ADDROF_ ((struct MRB_NODE_KV *)*new)->key;
#endif
        }
        parent = *new;
        if (result > 0) {
            new = &(*new)->MRB_LEFT_;
        } else {
            new = &(*new)->MRB_RIGHT_;
        }
    }
    newnode = MRB_ALLOC_NODE_(mrb);
    MC_ASSIGN_KEY_(newnode->key, key);
#if MC_NO_VALUE - 0 == 0
    MC_OPT_ASSIGN_VALUE_(newnode->value, value);
#endif
    mrb->count++;

    mrb_insert_node_(&mrb->root, &newnode->node, parent, new);

#if MC_NO_VALUE - 0 == 0
    return MC_OPT_ADDROF_ newnode->value;
#else
    return MC_OPT_ADDROF_ newnode->key;
#endif
}

static inline MC_ITERATOR_T *
MC_FUN_(iterase)(MC_T * const mrb,
                 MC_ITERATOR_T * const it)
{
    MC_ITERATOR_T *next = MC_FUN_(next)(it);

    MC_OPT_FREE_KEY_(((struct MRB_NODE_KV *)it)->key);
#if MC_NO_VALUE - 0 == 0
    MC_OPT_FREE_VALUE_(((struct MRB_NODE_KV *)it)->value);
#endif
    mrb_erase_node_(&mrb->root, (struct mrb_node *)it);
    mrb->count--;

    MRB_FREE_NODE_(mrb, (struct MRB_NODE_KV *)it);
    return next;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(erase)(MC_T * const mrb,
               MC_KEY_T const key)
{
    MC_DEF_VALUE_UNDEF_;
    struct mrb_node *node = mrb->root;
    MC_VALUE_T MC_OPT_PTR_ value;
    intptr_t result;

    while (node != NULL) {
        MRB_KEYCMP(result, ((struct MRB_NODE_KV *)node)->key, key);
        if (result == 0) {
            goto erase;
        }
        else if (result > 0) {
            node = node->MRB_LEFT_;
        } else {
            node = node->MRB_RIGHT_;
        }
    }
    return undef_value;
erase:
#if MC_NO_VALUE - 0 == 0
    MC_OPT_FREE_KEY_(((struct MRB_NODE_KV *)node)->key);
    value = MC_OPT_ADDROF_ ((struct MRB_NODE_KV *)node)->value;
    MC_OPT_FREE_VALUE_(((struct MRB_NODE_KV *)node)->value);
#else
    value = MC_OPT_ADDROF_ ((struct MRB_NODE_KV *)node)->key;
    MC_OPT_FREE_KEY_(((struct MRB_NODE_KV *)node)->key);
#endif
    mrb_erase_node_(&mrb->root, node);
    mrb->count--;

    MRB_FREE_NODE_(mrb, (struct MRB_NODE_KV *)node);
    return value;
}

static inline MC_ITERATOR_T *
MC_FUN_(itfind)(MC_T * const mrb,
                MC_KEY_T const key)
{
    struct mrb_node *node;
    intptr_t result;

    node = mrb->root;
    while (node != NULL) {
        MRB_KEYCMP(result, ((struct MRB_NODE_KV *)node)->key, key);
        if (result == 0) {
            return (MC_ITERATOR_T *)node;
        }
        if (result > 0) {
            node = node->MRB_LEFT_;
        } else {
            node = node->MRB_RIGHT_;
        }
    }
    return NULL;
}

static inline MC_ITERATOR_T *
MC_FUN_(itfindnear)(MC_T * const mrb,
                    MC_KEY_T const key)
{
    struct mrb_node *node;
    struct mrb_node *next_node;
    intptr_t result;

    node = mrb->root;
    while (node != NULL) {
        MRB_KEYCMP(result, ((struct MRB_NODE_KV *)node)->key, key);
        if (result == 0) {
            return (MC_ITERATOR_T *)node;
        }
        if (result > 0) {
            next_node = node->MRB_LEFT_;
        } else {
            next_node = node->MRB_RIGHT_;
        }
        if (next_node == NULL) {
            return (MC_ITERATOR_T *)node;
        }
        node = next_node;
    }
    return NULL;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(find)(MC_T * const mrb,
              MC_KEY_T const key)
{
    MC_DEF_VALUE_UNDEF_;
    struct mrb_node *node;
    intptr_t result;

    node = mrb->root;
    while (node != NULL) {
        MRB_KEYCMP(result, ((struct MRB_NODE_KV *)node)->key, key);
        if (result == 0) {
#if MC_NO_VALUE - 0 == 0
            return MC_OPT_ADDROF_ ((struct MRB_NODE_KV *)node)->value;
#else
            return MC_OPT_ADDROF_ ((struct MRB_NODE_KV *)node)->key;
#endif
        }
        else if (result > 0) {
            node = node->MRB_LEFT_;
        } else {
            node = node->MRB_RIGHT_;
        }
    }
    return undef_value;
}

static inline MC_KEY_T
MC_FUN_(key)(MC_ITERATOR_T * const it)
{
    return ((struct MRB_NODE_KV *)it)->key;
}

static inline MC_KEY_T *
MC_FUN_(keyp)(MC_ITERATOR_T * const it)
{
    return &((struct MRB_NODE_KV *)it)->key;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(val)(MC_ITERATOR_T * const it)
{
#if MC_NO_VALUE - 0 == 0
    return MC_OPT_ADDROF_ ((struct MRB_NODE_KV *)it)->value;
#else
    return MC_OPT_ADDROF_ ((struct MRB_NODE_KV *)it)->key;
#endif
}

#if MC_VALUE_NO_INSERT_ARG - 0 == 0
static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(setval)(MC_ITERATOR_T * const it MC_OPT_VALUE_INSERT_ARG_)
{
    MC_OPT_FREE_VALUE_(((struct MRB_NODE_KV *)it)->value);
    MC_OPT_ASSIGN_VALUE_(((struct MRB_NODE_KV *)it)->value, value);
    return MC_OPT_ADDROF_ ((struct MRB_NODE_KV *)it)->value;
}
#endif

#include <mc_tmpl_undef.h>
#undef MRB_KEYCMP
#undef MRB_ALLOC_NODE_
#undef MRB_FREE_NODE_
#undef MRB_LEFT_
#undef MRB_RIGHT_
