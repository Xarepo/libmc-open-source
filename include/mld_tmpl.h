/*
 * Copyright (c) 2011 - 2013, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*

  Double-linked list.

  Default configuration:

  List with 'void *' values, NULL as undefined value.

*/
#ifndef MC_PREFIX
#define MC_PREFIX mld
#define MC_VALUE_T void *
#endif

#define MC_SEQUENCE_CONTAINER_ 1
#define MC_MM_DEFAULT_BLOCK_SIZE_ 4096
#define MC_MM_DEFAULT_ MC_MM_COMPACT
#define MC_MM_SUPPORT_ (MC_MM_PERFORMANCE | MC_MM_STATIC | MC_MM_COMPACT)
#include <mc_tmpl.h>

#define MLD_NODE MC_CONCAT_(MC_PREFIX, _node)
struct MLD_NODE {
    struct MLD_NODE *next;
    struct MLD_NODE *prev;
    MC_VALUE_T value;
};

#if MC_MM_MODE == MC_MM_STATIC

#define MLD_ALLOC_NODE_(mld) MC_FUN_(npstatic_alloc)(&mld->nodepool);
#define MLD_FREE_NODE_(mld, node) MC_FUN_(npstatic_free)(&mld->nodepool, node);
#define NPSTATIC_NODE_TYPE struct MLD_NODE
#include <npstatic_tmpl.h>

#endif // MC_MM_MODE == MC_MM_STATIC

#if MC_MM_MODE == MC_MM_COMPACT

#define MLD_ALLOC_NODE_(mld) (struct MLD_NODE *)malloc(sizeof(struct MLD_NODE));
#define MLD_FREE_NODE_(mld, node) free(node);

#endif // MC_MM_MODE == MC_MM_COMPACT

#if MC_MM_MODE == MC_MM_PERFORMANCE

#define MLD_ALLOC_NODE_(mld) MC_FUN_(nodepool_alloc)(&mld->nodepool);
#define MLD_FREE_NODE_(mld, node) MC_FUN_(nodepool_free)(&mld->nodepool, node);
#define NODEPOOL_NODE_TYPE struct MLD_NODE
#define NODEPOOL_BLOCK_SIZE MC_MM_BLOCK_SIZE
#include <nodepool_tmpl.h>

#endif // MC_MM_MODE == MC_MM_PERFORMANCE

typedef struct MC_T_ {
    struct MLD_NODE *head;
    struct MLD_NODE *tail;
    uintptr_t count;
    uintptr_t capacity;
#if MC_MM_MODE == MC_MM_PERFORMANCE
    struct nodepool nodepool;
#endif
#if MC_MM_MODE == MC_MM_STATIC
    struct npstatic nodepool;
    uintptr_t pad_[1];
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
MC_FUN_(push_front)(MC_T * const mld MC_OPT_VALUE_INSERT_ARG_)
{
    MC_DEF_VALUE_UNDEF_;
    struct MLD_NODE *node;

    if (mld->count == mld->capacity) {
        return undef_value;
    }
    node = MLD_ALLOC_NODE_(mld);
    MC_OPT_ASSIGN_VALUE_(node->value, value);
    node->next = mld->head;
    node->prev = NULL;
    if (mld->head != NULL) {
        mld->head->prev = node;
    } else {
        mld->tail = node;
    }
    mld->head = node;
    mld->count++;
    return MC_OPT_ADDROF_ node->value;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(pop_front)(MC_T * const mld)
{
    MC_DEF_VALUE_UNDEF_;
    struct MLD_NODE *node;
    MC_VALUE_T MC_OPT_PTR_ value;

    if (mld->head == NULL) {
        return undef_value;
    }
    node = mld->head;
    mld->head = node->next;
    if (node == mld->tail) {
        mld->tail = NULL;
    } else {
        mld->head->prev = NULL;
    }
    mld->count--;
    value = MC_OPT_ADDROF_ node->value;
    MC_OPT_FREE_VALUE_(node->value);
    MLD_FREE_NODE_(mld, node);
    return value;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(push_back)(MC_T * const mld MC_OPT_VALUE_INSERT_ARG_)
{
    MC_DEF_VALUE_UNDEF_;
    struct MLD_NODE *node;

    if (mld->count == mld->capacity) {
        return undef_value;
    }
    node = MLD_ALLOC_NODE_(mld);
    MC_OPT_ASSIGN_VALUE_(node->value, value);
    node->next = NULL;
    node->prev = mld->tail;
    if (mld->tail != NULL) {
        mld->tail->next = node;
    } else {
        mld->head = node;
    }
    mld->tail = node;
    mld->count++;
    return MC_OPT_ADDROF_ node->value;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(pop_back)(MC_T * const mld)
{
    MC_DEF_VALUE_UNDEF_;
    struct MLD_NODE *node;
    MC_VALUE_T MC_OPT_PTR_ value;

    if (mld->tail == NULL) {
        return undef_value;
    }
    node = mld->tail;
    mld->tail = node->prev;
    if (node == mld->head) {
        mld->head = NULL;
    } else {
        mld->tail->next = NULL;
    }
    mld->count--;
    value = MC_OPT_ADDROF_ node->value;
    MC_OPT_FREE_VALUE_(node->value);
    MLD_FREE_NODE_(mld, node);
    return value;
}

#if MC_MM_MODE == MC_MM_COMPACT || MC_MM_MODE == MC_MM_PERFORMANCE

static inline MC_T *
MC_FUN_(new)(const size_t capacity)
{
    MC_T *mld;

#if MC_MM_MODE == MC_MM_PERFORMANCE
    mld = buddyalloc_alloc(nodepool_mem, sizeof(MC_T));
    MC_FUN_(nodepool_init)(&mld->nodepool);
#endif
#if MC_MM_MODE == MC_MM_COMPACT
    mld = (MC_T *)malloc(sizeof(MC_T));
#endif
    mld->head = NULL;
    mld->tail = NULL;
    mld->count = 0;
    mld->capacity = (uintptr_t)capacity;
    return mld;
}

static inline void
MC_FUN_(delete)(MC_T * const mld)
{
    if (mld == NULL) {
        return;
    }
#if defined(MC_FREE_VALUE) || MC_MM_MODE == MC_MM_COMPACT
    while (mld->count != 0) {
        MC_FUN_(pop_front)(mld);
    }
#endif
#if MC_MM_MODE == MC_MM_PERFORMANCE
    MC_FUN_(nodepool_delete)(&mld->nodepool);
    buddyalloc_free(nodepool_mem, mld, sizeof(MC_T));
#else
    free(mld);
#endif
}

static inline void
MC_FUN_(clear)(MC_T * const mld)
{
#if defined(MC_FREE_VALUE) || MC_MM_MODE == MC_MM_COMPACT
    while (mld->count != 0) {
        MC_FUN_(pop_front)(mld);
    }
#endif
#if MC_MM_MODE == MC_MM_PERFORMANCE
    MC_FUN_(nodepool_clear)(&mld->nodepool);
#endif
    mld->head = NULL;
    mld->tail = NULL;
    mld->count = 0;
}

#endif // MC_MM_MODE == MC_MM_COMPACT || MC_MM_MODE == MC_MM_PERFORMANCE

#if MC_MM_MODE == MC_MM_STATIC

static inline MC_T *
MC_FUN_(init)(MC_T * const mld,
              struct MLD_NODE * const nodes,
              const size_t sizeof_node_array)
{
    mld->head = NULL;
    mld->tail = NULL;
    mld->count = 0;
    mld->capacity = (uintptr_t)sizeof_node_array / sizeof(struct MLD_NODE);
    MC_FUN_(npstatic_init)(&mld->nodepool, nodes);
    return mld;
}

static inline MC_T *
MC_FUN_(new)(const size_t capacity)
{
    MC_T *mld;

    if (posix_memalign((void **)&mld, sizeof(MC_T),
                       sizeof(MC_T) + capacity * sizeof(struct MLD_NODE)) != 0)
    {
        return NULL;
    }
    return MC_FUN_(init)(mld, (struct MLD_NODE *)((uintptr_t)&mld[1]),
                         capacity * sizeof(struct MLD_NODE));
}

static inline void
MC_FUN_(delete)(MC_T * const mld)
{
    if (mld == NULL) {
        return;
    }
#ifdef MC_FREE_VALUE
    while (mld->count != 0) {
        MC_FUN_(pop_front)(mld);
    }
#endif
    free(mld);
}

static inline void
MC_FUN_(clear)(MC_T * const mld)
{
#ifdef MC_FREE_VALUE
    while (mld->count != 0) {
        MC_FUN_(pop_front)(mld);
    }
#endif
    MC_FUN_(npstatic_clear)(&mld->nodepool);
    mld->head = NULL;
    mld->tail = NULL;
    mld->count = 0;
}

#endif // MC_MM_MODE == MC_MM_STATIC

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(front)(MC_T * const mld)
{
    MC_DEF_VALUE_UNDEF_;
    if (mld->head == NULL) {
        return undef_value;
    }
    return MC_OPT_ADDROF_ mld->head->value;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(back)(MC_T * const mld)
{
    MC_DEF_VALUE_UNDEF_;
    if (mld->tail == NULL) {
        return undef_value;
    }
    return MC_OPT_ADDROF_ mld->tail->value;
}

static inline int
MC_FUN_(empty)(MC_T * const mld)
{
    return (mld->head == NULL);
}

static inline size_t
MC_FUN_(size)(MC_T * const mld)
{
    return mld->count;
}

static inline size_t
MC_FUN_(max_size)(MC_T * const mld)
{
    return mld->capacity;
}

static inline void
MC_FUN_(to_front)(MC_T * const mld,
                  MC_ITERATOR_T * const it)
{
    struct MLD_NODE *node = (struct MLD_NODE *)it;

    if (node == mld->head) {
        return;
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    if (node == mld->tail) {
        mld->tail = node->prev;
    }
    node->next = mld->head;
    node->prev = NULL;
    mld->head->prev = node;
    mld->head = node;
}

static inline void
MC_FUN_(to_back)(MC_T * const mld,
                 MC_ITERATOR_T * const it)
{
    struct MLD_NODE *node = (struct MLD_NODE *)it;

    if (node == mld->tail) {
        return;
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    if (node == mld->head) {
        mld->head = node->next;
    }
    node->next = NULL;
    node->prev = mld->tail;
    mld->tail->next = node;
    mld->tail = node;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(insert)(MC_T * const mld,
                MC_ITERATOR_T * const it MC_OPT_VALUE_INSERT_ARG_)
{
    MC_DEF_VALUE_UNDEF_;
    struct MLD_NODE *node = (struct MLD_NODE *)it;
    struct MLD_NODE *newnode;

    if (mld->count == mld->capacity) {
        return undef_value;
    }
    newnode = MLD_ALLOC_NODE_(mld);
    MC_OPT_ASSIGN_VALUE_(newnode->value, value);
    newnode->next = node;
    newnode->prev = node->prev;
    if (mld->head == node) {
        mld->head = newnode;
    } else {
        node->prev->next = newnode;
    }
    node->prev = newnode;
    mld->count++;
    return MC_OPT_ADDROF_ newnode->value;
}

static inline MC_ITERATOR_T *
MC_FUN_(erase)(MC_T * const mld,
               MC_ITERATOR_T * const it)
{
    MC_ITERATOR_T *next_it;
    struct MLD_NODE *node = (struct MLD_NODE *)it;
    struct MLD_NODE *x;

    x = node->next;
    next_it = (MC_ITERATOR_T *)x;
    if (x != NULL) {
        x->prev = node->prev;
    }
    if (node == mld->head) {
        mld->head = x;
    }
    x = node->prev;
    if (x != NULL) {
        x->next = node->next;
    }
    if (node == mld->tail) {
        mld->tail = x;
    }
    mld->count--;
    MC_OPT_FREE_VALUE_(node->value);
    MLD_FREE_NODE_(mld, node);
    return next_it;
}

static inline MC_ITERATOR_T *
MC_FUN_(begin)(MC_T * const mld)
{
    return (MC_ITERATOR_T *)mld->head;
}

static inline MC_ITERATOR_T *
MC_FUN_(rbegin)(MC_T * const mld)
{
    return (MC_ITERATOR_T *)mld->tail;
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
MC_FUN_(next)(MC_ITERATOR_T * const it)
{
    return (MC_ITERATOR_T *)((struct MLD_NODE *)it)->next;
}

static inline MC_ITERATOR_T *
MC_FUN_(prev)(MC_ITERATOR_T * const it)
{
    return (MC_ITERATOR_T *)((struct MLD_NODE *)it)->prev;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(val)(MC_ITERATOR_T * const it)
{
    return MC_OPT_ADDROF_ ((struct MLD_NODE *)it)->value;
}

#if MC_VALUE_NO_INSERT_ARG - 0 == 0
static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(setval)(MC_ITERATOR_T * const it MC_OPT_VALUE_INSERT_ARG_)
{
    MC_OPT_FREE_VALUE_(((struct MLD_NODE *)it)->value);
    MC_OPT_ASSIGN_VALUE_(((struct MLD_NODE *)it)->value, value);
    return MC_OPT_ADDROF_ ((struct MLD_NODE *)it)->value;
}
#endif

#include <mc_tmpl_undef.h>
#undef MLD_ALLOC_NODE_
#undef MLD_FREE_NODE_
