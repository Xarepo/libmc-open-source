/*
 * Copyright (c) 2011 - 2012 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*

  Lifo/fifo queue.

  Default configuration:

  Queue with 'void *' values, NULL as undefined value.

*/
#ifndef MC_PREFIX
#define MC_PREFIX mq
#define MC_VALUE_T void *
#endif

#define MC_SEQUENCE_CONTAINER_ 1
#define MC_MM_DEFAULT_ MC_MM_STATIC
#define MC_MM_SUPPORT_ MC_MM_STATIC
#include <mc_tmpl.h>

typedef struct MC_T_ {
    uintptr_t capacity;
    uintptr_t size_mask;
    uintptr_t head;
    uintptr_t tail;
    MC_VALUE_T values[];
} MC_T;

static inline MC_T *
MC_FUN_(init)(MC_T * const mq,
              MC_VALUE_T * const values,
              const size_t capacity,
              const size_t sizeof_value_array)
{
    uintptr_t size = sizeof_value_array / sizeof(MC_VALUE_T);
    uintptr_t lg;

    for (lg = 0; (uintptr_t)1u << lg < size; lg++) {};
    if ((uintptr_t)1u << lg != size ||
        capacity > size ||
        mq->values != values)
    {
        abort();
    }
    mq->capacity = (uintptr_t)capacity;
    mq->size_mask = size - 1;
    mq->head = 0;
    mq->tail = 0;
    return mq;
}

static inline MC_T *
MC_FUN_(new)(const size_t capacity)
{
    uintptr_t lg;
    MC_T *mq;

    for (lg = 0; (uintptr_t)1u << lg < capacity; lg++) {};
    if (posix_memalign((void **)&mq, sizeof(MC_T),
                       sizeof(MC_T) + ((uintptr_t)1u << lg) * sizeof(MC_VALUE_T)) != 0)
    {
        return NULL;
    }
    return MC_FUN_(init)(mq, mq->values, capacity, ((uintptr_t)1u << lg) * sizeof(MC_VALUE_T));
}

static inline void
MC_FUN_(delete)(MC_T * const mq)
{
#if defined(MC_FREE_VALUE)
    for (uintptr_t i = mq->head; i != mq->tail; i++) {
        MC_OPT_FREE_VALUE_(mq->values[i & mq->size_mask]);
    }
#endif
    free(mq);
}

static inline void
MC_FUN_(clear)(MC_T * const mq)
{
#if defined(MC_FREE_VALUE)
    for (uintptr_t i = mq->head; i != mq->tail; i++) {
        MC_OPT_FREE_VALUE_(mq->values[i & mq->size_mask]);
    }
#endif
    MC_FUN_(init)(mq, mq->values, mq->capacity, (mq->size_mask + 1) * sizeof(MC_VALUE_T));
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(front)(MC_T * const mq)
{
    MC_DEF_VALUE_UNDEF_;
    if (mq->tail == mq->head) {
        return undef_value;
    }
    return MC_OPT_ADDROF_ mq->values[mq->head & mq->size_mask];
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(back)(MC_T * const mq)
{
    MC_DEF_VALUE_UNDEF_;
    if (mq->tail == mq->head) {
        return undef_value;
    }
    return MC_OPT_ADDROF_ mq->values[(mq->tail - 1) & mq->size_mask];
}

static inline int
MC_FUN_(empty)(MC_T * const mq)
{
    return (mq->tail == mq->head);
}

static inline size_t
MC_FUN_(size)(MC_T * const mq)
{
    return mq->tail - mq->head;
}

static inline size_t
MC_FUN_(max_size)(MC_T * const mq)
{
    return mq->capacity;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(push_front)(MC_T * const mq MC_OPT_VALUE_INSERT_ARG_)
{
    MC_DEF_VALUE_UNDEF_;

    if (mq->tail - mq->head == mq->capacity) {
        return undef_value;
    }
    mq->head--;
    MC_OPT_ASSIGN_VALUE_(mq->values[mq->head & mq->size_mask], value);
    return MC_OPT_ADDROF_ mq->values[mq->head & mq->size_mask];
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(push_back)(MC_T * const mq MC_OPT_VALUE_INSERT_ARG_)
{
    uintptr_t tail_pos;
    MC_DEF_VALUE_UNDEF_;

    if (mq->tail - mq->head == mq->capacity) {
        return undef_value;
    }
    tail_pos = mq->tail & mq->size_mask;
    mq->tail++;
    MC_OPT_ASSIGN_VALUE_(mq->values[tail_pos], value);
    return MC_OPT_ADDROF_ mq->values[tail_pos & mq->size_mask];
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(pop_front)(MC_T * const mq)
{
    MC_DEF_VALUE_UNDEF_;
    MC_VALUE_T MC_OPT_PTR_ value;

    if (mq->head == mq->tail) {
        return undef_value;
    }
    MC_OPT_FREE_VALUE_(mq->values[mq->head & mq->size_mask]);
    value = MC_OPT_ADDROF_ mq->values[mq->head & mq->size_mask];
    mq->head++;
    return value;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(pop_back)(MC_T * const mq)
{
    MC_DEF_VALUE_UNDEF_;
    MC_VALUE_T MC_OPT_PTR_ value;

    if (mq->head == mq->tail) {
        return undef_value;
    }
    mq->tail--;
    MC_OPT_FREE_VALUE_(mq->values[mq->tail & mq->size_mask]);
    value = MC_OPT_ADDROF_ mq->values[mq->tail & mq->size_mask];
    return value;
}

static inline MC_ITERATOR_T *
MC_FUN_(begin)(MC_T * const mq)
{
    return (MC_ITERATOR_T *)mq->head;
}

static inline MC_ITERATOR_T *
MC_FUN_(rbegin)(MC_T * const mq)
{
    return (MC_ITERATOR_T *)(mq->tail - 1);
}

static inline MC_ITERATOR_T *
MC_FUN_(end)(MC_T * const mq)
{
    return (MC_ITERATOR_T *)mq->tail;
}

static inline MC_ITERATOR_T *
MC_FUN_(rend)(MC_T * const mq)
{
    return (MC_ITERATOR_T *)(mq->head - 1);
}

static inline MC_ITERATOR_T *
MC_FUN_(next)(MC_ITERATOR_T * const it)
{
    return (MC_ITERATOR_T *)((uintptr_t)it + 1);
}

static inline MC_ITERATOR_T *
MC_FUN_(prev)(MC_ITERATOR_T * const it)
{
    return (MC_ITERATOR_T *)((uintptr_t)it - 1);
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(val)(MC_T * const mq,
             MC_ITERATOR_T * const it)
{
    return MC_OPT_ADDROF_ mq->values[(uintptr_t)it & mq->size_mask];
}

#if MC_VALUE_NO_INSERT_ARG - 0 == 0
static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(setval)(MC_T * const mq,
                MC_ITERATOR_T * const it MC_OPT_VALUE_INSERT_ARG_)
{
    MC_VALUE_T *node = &mq->values[(uintptr_t)it & mq->size_mask];
    MC_OPT_FREE_VALUE_(*node);
    MC_OPT_ASSIGN_VALUE_(*node, value);
    return MC_OPT_ADDROF_ (*node);
}
#endif

#include <mc_tmpl_undef.h>
