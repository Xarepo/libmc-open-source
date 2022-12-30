/*
 * Copyright (c) 2011 - 2012, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*

  Single-linked list.

  Default configuration:

  List with 'void *' values, NULL as undefined value.

*/

#ifndef MC_PREFIX
#define MC_PREFIX mls
#define MC_VALUE_T void *
#endif

#define MC_MM_DEFAULT_BLOCK_SIZE_ 4096
#define MC_MM_DEFAULT_ MC_MM_COMPACT
#define MC_MM_SUPPORT_ (MC_MM_COMPACT | MC_MM_STATIC | MC_MM_PERFORMANCE)
#define MC_SEQUENCE_CONTAINER_ 1
#include <mc_tmpl.h>

#define MLS_NODE MC_CONCAT_(MC_PREFIX, _node)

// List node, value and pointer to next node.
struct MLS_NODE {
    struct MLS_NODE *next;
    MC_VALUE_T value;
};

// Set up macros for node allocation.
#if MC_MM_MODE == MC_MM_COMPACT

#define MLS_ALLOC_NODE_(mls) malloc(sizeof(struct MLS_NODE));
#define MLS_FREE_NODE_(mls, node) free(node);

#endif // MC_MM_MODE == MC_MM_COMPACT

#if MC_MM_MODE == MC_MM_STATIC

#define MLS_ALLOC_NODE_(mls) MC_FUN_(npstatic_alloc)(&mls->nodepool);
#define MLS_FREE_NODE_(mls, node) MC_FUN_(npstatic_free)(&mls->nodepool, node);
#define NPSTATIC_NODE_TYPE struct MLS_NODE
#include <npstatic_tmpl.h>

#endif // MC_MM_MODE == MC_MM_STATIC

#if MC_MM_MODE == MC_MM_PERFORMANCE

#define MLS_ALLOC_NODE_(mls) MC_FUN_(nodepool_alloc)(&mls->nodepool);
#define MLS_FREE_NODE_(mls, node) MC_FUN_(nodepool_free)(&mls->nodepool, node);
#define NODEPOOL_NODE_TYPE struct MLS_NODE
#define NODEPOOL_BLOCK_SIZE MC_MM_BLOCK_SIZE
#include <nodepool_tmpl.h>

#endif // MC_MM_MODE == MC_MM_PERFORMANCE


/* Base structure for the list */
typedef struct MC_T_ {
    struct MLS_NODE *head;
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

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(push_front)(MC_T * const mls MC_OPT_VALUE_INSERT_ARG_)
{
    MC_DEF_VALUE_UNDEF_;
    struct MLS_NODE *node;

    if (mls->count == mls->capacity) {
        return undef_value;
    }
    node = MLS_ALLOC_NODE_(mls);
    MC_OPT_ASSIGN_VALUE_(node->value, value);
    node->next = mls->head;
    mls->head = node;
    mls->count++;
    return MC_OPT_ADDROF_ node->value;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(pop_front)(MC_T * const mls)
{
    MC_DEF_VALUE_UNDEF_;
    struct MLS_NODE *node;
    MC_VALUE_T MC_OPT_PTR_ value;

    if (mls->head == NULL) {
        return undef_value;
    }
    node = mls->head;
    mls->head = node->next;
    mls->count--;
    value = MC_OPT_ADDROF_ node->value;
    MC_OPT_FREE_VALUE_(node->value);
    MLS_FREE_NODE_(mls, node);
    return value;
}

#if MC_MM_MODE == MC_MM_COMPACT || MC_MM_MODE == MC_MM_PERFORMANCE

static inline MC_T *
MC_FUN_(new)(const size_t capacity)
{
    MC_T *mls;

#if MC_MM_MODE == MC_MM_PERFORMANCE
    mls = buddyalloc_alloc(nodepool_mem, sizeof(MC_T));
    MC_FUN_(nodepool_init)(&mls->nodepool);
#endif
#if MC_MM_MODE == MC_MM_COMPACT
    mls = malloc(sizeof(MC_T));
#endif
    mls->head = NULL;
    mls->count = 0;
    mls->capacity = (uintptr_t)capacity;
    return mls;
}

static inline void
MC_FUN_(delete)(MC_T * const mls)
{
    if (mls == NULL) {
        return;
    }
#if defined(MC_FREE_VALUE) || MC_MM_MODE == MC_MM_COMPACT
    while (mls->count != 0) {
        MC_FUN_(pop_front)(mls);
    }
#endif
#if MC_MM_MODE == MC_MM_PERFORMANCE
    MC_FUN_(nodepool_delete)(&mls->nodepool);
    buddyalloc_free(nodepool_mem, mls, sizeof(MC_T));
#else
    free(mls);
#endif
}

static inline void
MC_FUN_(clear)(MC_T * const mls)
{
#if defined(MC_FREE_VALUE) || MC_MM_MODE == MC_MM_COMPACT
    while (mls->count != 0) {
        MC_FUN_(pop_front)(mls);
    }
#endif
#if MC_MM_MODE == MC_MM_PERFORMANCE
    MC_FUN_(nodepool_clear)(&mls->nodepool);
#endif
    mls->head = NULL;
    mls->count = 0;
}

#endif // MC_MM_MODE == MC_MM_COMPACT || MC_MM_MODE == MC_MM_PERFORMANCE

#if MC_MM_MODE == MC_MM_STATIC

static inline MC_T *
MC_FUN_(init)(MC_T * const mls,
              struct MLS_NODE * const nodes,
              const size_t sizeof_node_array)
{
    mls->head = NULL;
    mls->count = 0;
    mls->capacity = (uintptr_t)sizeof_node_array / sizeof(struct MLS_NODE);
    MC_FUN_(npstatic_init)(&mls->nodepool, nodes);
    return mls;
}

static inline MC_T *
MC_FUN_(new)(const size_t capacity)
{
    MC_T *mls;

    if (posix_memalign((void **)&mls, sizeof(MC_T),
                       sizeof(MC_T) + capacity * sizeof(struct MLS_NODE)) != 0)
    {
        return NULL;
    }
    return MC_FUN_(init)(mls, (struct MLS_NODE *)((uintptr_t)&mls[1]),
                         capacity * sizeof(struct MLS_NODE));
}

static inline void
MC_FUN_(delete)(MC_T * const mls)
{
    if (mls == NULL) {
        return;
    }
#ifdef MC_FREE_VALUE
    while (mls->count != 0) {
        MC_FUN_(pop_front)(mls);
    }
#endif
    free(mls);
}

static inline void
MC_FUN_(clear)(MC_T * const mls)
{
#ifdef MC_FREE_VALUE
    while (mls->count != 0) {
        MC_FUN_(pop_front)(mls);
    }
#endif
    MC_FUN_(npstatic_clear)(&mls->nodepool);
    mls->head = NULL;
    mls->count = 0;
}

#endif // MC_MM_MODE == MC_MM_STATIC

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(front)(MC_T * const mls)
{
    MC_DEF_VALUE_UNDEF_;
    if (mls->head == NULL) {
        return undef_value;
    }
    return MC_OPT_ADDROF_ mls->head->value;
}

static inline int
MC_FUN_(empty)(MC_T * const mls)
{
    return (mls->head == NULL);
}

static inline size_t
MC_FUN_(size)(MC_T * const mls)
{
    return mls->count;
}

static inline size_t
MC_FUN_(max_size)(MC_T * const mls)
{
    return mls->capacity;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(insert_after)(MC_T * const mls,
                      MC_ITERATOR_T * const it MC_OPT_VALUE_INSERT_ARG_)
{
    if (it == NULL) {
        return MC_FUN_(push_front)(mls MC_OPT_VALUE_PARAM_);
    }
    MC_DEF_VALUE_UNDEF_;
    struct MLS_NODE *node = (struct MLS_NODE *)it;
    struct MLS_NODE *newnode;

    if (mls->count == mls->capacity) {
        return undef_value;
    }
    newnode = MLS_ALLOC_NODE_(mls);
    MC_OPT_ASSIGN_VALUE_(newnode->value, value);
    newnode->next = node->next;
    node->next = newnode;
    mls->count++;
    return MC_OPT_ADDROF_ newnode->value;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(erase_after)(MC_T * const mls,
                     MC_ITERATOR_T * const it)
{
    if (it == NULL) {
        return MC_FUN_(pop_front)(mls);
    }
    MC_DEF_VALUE_UNDEF_;
    struct MLS_NODE *node = (struct MLS_NODE *)it;
    struct MLS_NODE *remnode;
    MC_VALUE_T MC_OPT_PTR_ value;

    remnode = node->next;
    if (remnode == NULL) {
        return undef_value;
    }
    node->next = remnode->next;
    mls->count--;
    value = MC_OPT_ADDROF_ remnode->value;
    MC_OPT_FREE_VALUE_(remnode->value);
    MLS_FREE_NODE_(mls, remnode);
    return value;
}

static inline MC_ITERATOR_T *
MC_FUN_(begin)(MC_T * const mls)
{
    return (MC_ITERATOR_T *)mls->head;
}

static inline MC_ITERATOR_T *
MC_FUN_(end)(void)
{
    return NULL;
}

static inline MC_ITERATOR_T *
MC_FUN_(next)(MC_ITERATOR_T * const it)
{
    return (MC_ITERATOR_T *)((struct MLS_NODE *)it)->next;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(val)(MC_ITERATOR_T * const it)
{
    return MC_OPT_ADDROF_ ((struct MLS_NODE *)it)->value;
}

#if MC_VALUE_NO_INSERT_ARG - 0 == 0
static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(setval)(MC_ITERATOR_T * const it MC_OPT_VALUE_INSERT_ARG_)
{
    MC_OPT_FREE_VALUE_(((struct MLS_NODE *)it)->value);
    MC_OPT_ASSIGN_VALUE_(((struct MLS_NODE *)it)->value, value);
    return MC_OPT_ADDROF_ ((struct MLS_NODE *)it)->value;
}
#endif

#include <mc_tmpl_undef.h>
#undef MLS_ALLOC_NODE_
#undef MLS_FREE_NODE_
