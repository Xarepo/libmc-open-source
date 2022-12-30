/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*

  Dynamic sized array (similar to C++ std::vector).

  Default configuration:

  Array with 'void *' values, NULL as undefined value.

  Supports compact and static memory management modes simultaneously,
  if no configuration flags necessary. In the static case it can be
  used as a span/array_view.

*/
#ifndef MC_PREFIX
#define MC_PREFIX mv
#define MC_VALUE_T void *
#endif

#define MC_SEQUENCE_CONTAINER_ 1
#define MC_MM_DEFAULT_BLOCK_SIZE_ 32
#define MC_MM_DEFAULT_ MC_MM_COMPACT
#define MC_MM_SUPPORT_ (MC_MM_STATIC | MC_MM_COMPACT)
#include <mc_tmpl.h>

#ifndef MV_TMPL_ONCE_
#define MV_TMPL_ONCE_
#include <bitops.h>
#include <string.h>

#define MV_MAXCAP_FLAGS_BITS_ 2u
#define MV_MAXCAP_STATIC_MEM_FLAG_ 1u
#define MV_MAXCAP_FIXED_SIZE_FLAG_ 2u
#define MV_SPAN_INITIALIZER(array) {                              \
    .count = sizeof(array)/sizeof((array)[0]),                    \
    .current_capacity = sizeof(array)/sizeof((array)[0]),         \
    .max_capacity_n_flags = (sizeof(array)/sizeof((array)[0]) << MV_MAXCAP_FLAGS_BITS_) | \
        MV_MAXCAP_STATIC_MEM_FLAG_ | MV_MAXCAP_FIXED_SIZE_FLAG_,    \
    .values = (array)                                               \
}
#endif // MV_TMPL_ONCE_

typedef struct MC_T_ {
    uintptr_t count;
    uintptr_t current_capacity;
#define MV_MAX_CAPACITY_(mv) ((mv)->max_capacity_n_flags >> MV_MAXCAP_FLAGS_BITS_)
#define MV_IS_STATIC_MEM_(mv) (((mv)->max_capacity_n_flags & MV_MAXCAP_STATIC_MEM_FLAG_) != 0)
#define MV_IS_FIXED_SIZE_(mv) (((mv)->max_capacity_n_flags & MV_MAXCAP_FIXED_SIZE_FLAG_) != 0)
    uintptr_t max_capacity_n_flags;
    MC_VALUE_T *values;
} MC_T;

static inline MC_T *
MC_FUN_(init)(MC_T * const mv,
              MC_VALUE_T * const values,
              const size_t sizeof_value_array)
{
    mv->count = 0;
    mv->current_capacity = sizeof_value_array / sizeof(MC_VALUE_T);
    mv->max_capacity_n_flags = MV_MAXCAP_STATIC_MEM_FLAG_ |
        (mv->current_capacity << MV_MAXCAP_FLAGS_BITS_);
    mv->values = values;
    return mv;
}

static inline MC_T *
MC_FUN_(init_span)(MC_T * const mv,
                   MC_VALUE_T * const values,
                   const size_t sizeof_value_array)
{
    mv->count = sizeof_value_array / sizeof(MC_VALUE_T);
    mv->current_capacity = mv->count;
    mv->max_capacity_n_flags = MV_MAXCAP_STATIC_MEM_FLAG_ | MV_MAXCAP_FIXED_SIZE_FLAG_ |
        (mv->current_capacity << MV_MAXCAP_FLAGS_BITS_);
    mv->values = values;
    return mv;
}

static inline MC_T *
MC_FUN_(new)(const size_t max_capacity,
             size_t initial_capacity)
{
    MC_T *mv = (MC_T *)malloc(sizeof(MC_T));
    if (mv == NULL) {
        return NULL;
    }
    if (initial_capacity > ((max_capacity << MV_MAXCAP_FLAGS_BITS_) >> MV_MAXCAP_FLAGS_BITS_)) {
        abort();
    }
    if (initial_capacity == 0) {
        mv->values = NULL;
    } else {
        mv->values = (MC_VALUE_T *)malloc(initial_capacity * sizeof(MC_VALUE_T));
        if (mv->values == NULL) {
            free(mv);
            return NULL;
        }
    }
    mv->count = 0;
    mv->max_capacity_n_flags = (uintptr_t)max_capacity << MV_MAXCAP_FLAGS_BITS_;
    mv->current_capacity = initial_capacity;
    return mv;
}

static inline void
MC_FUN_(delete)(MC_T * const mv)
{
    if (mv == NULL) {
        return;
    }
#if defined(MC_FREE_VALUE)
    for (uintptr_t i = 0; i < mv->count; i++) {
        MC_OPT_FREE_VALUE_(mv->values[i]);
    }
#endif
    free(mv->values);
    free(mv);
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(at)(MC_T * const mv, size_t idx)
{
    if (idx >= mv->count) {
        abort();
    }
    return MC_OPT_ADDROF_ mv->values[idx];
}

static inline MC_VALUE_T MC_OPT_PTR_ *
MC_FUN_(data)(MC_T * const mv)
{
    return mv->values;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(front)(MC_T * const mv)
{
    MC_DEF_VALUE_UNDEF_;
    if (mv->count == 0) {
        return undef_value;
    }
    return MC_OPT_ADDROF_ mv->values[0];
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(back)(MC_T * const mv)
{
    MC_DEF_VALUE_UNDEF_;
    if (mv->count == 0) {
        return undef_value;
    }
    return MC_OPT_ADDROF_ mv->values[mv->count-1];
}

static inline int
MC_FUN_(empty)(MC_T * const mv)
{
    return mv->count == 0;
}

static inline size_t
MC_FUN_(size)(MC_T * const mv)
{
    return mv->count;
}

static inline void
MC_FUN_(maybe_decrease_capacity_)(MC_T * const mv)
{
    const uintptr_t cap = (uintptr_t)1u << bit32_bsr((uint32_t)mv->current_capacity);
    if (mv->count <= cap >> 1u) {
        const uintptr_t new_capacity = cap - (cap >> 2u);
        MC_VALUE_T *new_values = realloc(mv->values, new_capacity * sizeof(MC_VALUE_T));
        if (new_values == NULL) {
            abort();
        }
        mv->values = new_values;
        mv->current_capacity = new_capacity;
    }
}

static inline void
MC_FUN_(maybe_increase_capacity_)(MC_T * const mv, size_t count)
{
    if (count > mv->current_capacity) {
        const unsigned bp = bit32_bsr((uint32_t)count);
        uintptr_t new_capacity = bp < 12 ? (uintptr_t)2u << bp : (mv->current_capacity + 4096) & ~((uintptr_t)0xfff);
        if (new_capacity > MV_MAX_CAPACITY_(mv)) {
            new_capacity = MV_MAX_CAPACITY_(mv);
        }
        MC_VALUE_T *new_values = realloc(mv->values, new_capacity * sizeof(MC_VALUE_T));
        if (new_values == NULL) {
            abort();
        }
        mv->values = new_values;
        mv->current_capacity = new_capacity;
    }
}

static inline void
MC_FUN_(resize)(MC_T * const mv, size_t count)
{
    if (MV_IS_STATIC_MEM_(mv)) {
        return;
    }
    if (count > mv->count) {
        MC_FUN_(maybe_increase_capacity_)(mv, count);
        memset(&mv->values[mv->count], 0, (count - mv->count) * sizeof(MC_VALUE_T));
        mv->count = count;
    } else {
        mv->count = count;
        MC_FUN_(maybe_decrease_capacity_)(mv);
    }
}

static inline size_t
MC_FUN_(max_size)(MC_T * const mv)
{
    return MV_MAX_CAPACITY_(mv);
}

static inline void
MC_FUN_(shrink_to_fit)(MC_T * const mv)
{
    if (!MV_IS_STATIC_MEM_(mv) && mv->count != mv->current_capacity) {
        MC_VALUE_T *new_values = realloc(mv->values, mv->count * sizeof(MC_VALUE_T));
        if (new_values != NULL) { // note: realloc to smaller size should never fail
            mv->values = new_values;
            mv->current_capacity = mv->count;
        }
    }
}

static inline void
MC_FUN_(reserve)(MC_T * const mv, size_t count)
{
    if (!MV_IS_STATIC_MEM_(mv) && count > mv->current_capacity) {
        if (count > MV_MAX_CAPACITY_(mv)) {
            count = MV_MAX_CAPACITY_(mv);
        }
        MC_VALUE_T *new_values = realloc(mv->values, count * sizeof(MC_VALUE_T));
        if (new_values != NULL) {
            mv->values = new_values;
            mv->current_capacity = count;
        }
    }
}

static inline void
MC_FUN_(clear)(MC_T * const mv)
{
#if defined(MC_FREE_VALUE)
    for (uintptr_t i = 0; i < mv->count; i++) {
        MC_OPT_FREE_VALUE_(mv->values[i]);
    }
#endif
    if (!MV_IS_STATIC_MEM_(mv)) {
        mv->current_capacity = 0;
        free(mv->values);
        mv->values = NULL;
    }
    if (MV_IS_FIXED_SIZE_(mv)) {
        memset(mv->values, 0, mv->current_capacity * sizeof(MC_VALUE_T));
    } else {
        mv->count = 0;
    }
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(push_back)(MC_T * const mv MC_OPT_VALUE_INSERT_ARG_)
{
    MC_DEF_VALUE_UNDEF_;

    if (MV_IS_FIXED_SIZE_(mv) || mv->count >= MV_MAX_CAPACITY_(mv)) {
        return undef_value;
    }
    if (!MV_IS_STATIC_MEM_(mv)) {
        MC_FUN_(maybe_increase_capacity_)(mv, mv->count + 1);
    }
    mv->count++;
    MC_OPT_ASSIGN_VALUE_(mv->values[mv->count-1], value);
    return MC_OPT_ADDROF_ mv->values[mv->count-1];
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(pop_back)(MC_T * const mv)
{
    MC_DEF_VALUE_UNDEF_;
    if (MV_IS_FIXED_SIZE_(mv) || mv->count == 0) {
        return undef_value;
    }
    // note: keep space for return value
    if (!MV_IS_STATIC_MEM_(mv)) {
        MC_FUN_(maybe_decrease_capacity_)(mv);
    }
    mv->count--;
    MC_OPT_FREE_VALUE_(mv->values[mv->count]);
    return MC_OPT_ADDROF_ mv->values[mv->count];
}

static inline MC_ITERATOR_T *
MC_FUN_(begin)(MC_T * const mv)
{
    return (MC_ITERATOR_T *)mv->values;
}

static inline MC_ITERATOR_T *
MC_FUN_(rbegin)(MC_T * const mv)
{
    return (MC_ITERATOR_T *)&mv->values[mv->count - 1];
}

static inline MC_ITERATOR_T *
MC_FUN_(end)(MC_T * const mv)
{
    return (MC_ITERATOR_T *)&mv->values[mv->count];
}

static inline MC_ITERATOR_T *
MC_FUN_(rend)(MC_T * const mv)
{
    return (MC_ITERATOR_T *)&mv->values[-1];
}

static inline MC_ITERATOR_T *
MC_FUN_(next)(MC_ITERATOR_T * const it)
{
    return (MC_ITERATOR_T *)&((MC_VALUE_T *)it)[1];
}

static inline MC_ITERATOR_T *
MC_FUN_(prev)(MC_ITERATOR_T * const it)
{
    return (MC_ITERATOR_T *)&((MC_VALUE_T *)it)[-1];
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(val)(MC_ITERATOR_T * const it)
{
    return MC_OPT_ADDROF_ *(MC_VALUE_T *)it;
}

#if MC_VALUE_NO_INSERT_ARG - 0 == 0
static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(setval)(MC_ITERATOR_T * const it MC_OPT_VALUE_INSERT_ARG_)
{
    MC_VALUE_T *node = (MC_VALUE_T *)it;
    MC_OPT_FREE_VALUE_(*node);
    MC_OPT_ASSIGN_VALUE_(*node, value);
    return MC_OPT_ADDROF_ (*node);
}
#endif

#include <mc_tmpl_undef.h>
#undef MV_MAX_CAPACITY_
#undef MV_IS_STATIC_MEM_
#undef MV_IS_FIXED_SIZE_
