/*
 * Copyright (c) 2013, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*
  Radix tree.

  Compile-time options / tuning:

  MRX_KEY_VARSIZE 1 - enables variable key size which leads to that
  mrx_insert(), mrx_find() etc will require a size argument, and extra
  functions for NULL-terminated strings are added, mrx_insertnt(),
  mrx_findnt() etc.

  MRX_KEY_SORTINT 1 - indicates that the key is an integer and adapts
  insertion such that integers will be correctly sorted. This requires
  swapping on little endian machines. If sort order is not important, do
  not enable this.

  Default configuration:

  Map with 'const char *' keys to 'void *', with NULL as undefined value.
*/

/*

  Design notes: see mrx_base.c

*/

#ifndef MC_PREFIX
#define MC_PREFIX mrx
#define MC_KEY_T const char *
#define MC_VALUE_T void *
#define MRX_KEY_VARSIZE 1
#endif

#define MC_ASSOCIATIVE_CONTAINER_ 1
#define MC_CUSTOM_ITERATOR_ 1
#define MC_MM_DEFAULT_ MC_MM_COMPACT
#define MC_MM_SUPPORT_ (MC_MM_PERFORMANCE | MC_MM_COMPACT)
#include <mc_tmpl.h>

#if defined(MC_COPY_KEY) || defined(MC_FREE_KEY)
// As keys are stored as the path in a radix tree it makes no sense to copy them.
#error "MC_COPY_KEY and MC_FREE_KEY macros are not supported by the radix tree."
#endif

#if MC_VALUE_RETURN_REF - 0 != 0
// The radix tree may need to reallocate nodes and thus move around values,
// meaning that it makes no sense to return references to values.
#error "MC_VALUE_RETURN_REF=1 is not supported by the radix tree."
#endif

#ifndef MRX_TMPL_ONCE_
#define MRX_TMPL_ONCE_
#include <alloca.h> // for the non-standard alloca() function, may need other header on some systems
#include <string.h>

#include <bitops.h>
#include <mrx_base.h>
#endif

#if MRX_KEY_VARSIZE - 0 == 0
  #define MRX_KEY_SIZE_ sizeof(MC_KEY_T)
  #define MRX_KEY_SIZE_ARG_
  #define MRX_KEY_ADDROF_ &
#else
  #define MRX_KEY_SIZE_ key_size_
  #define MRX_KEY_SIZE_ARG_ , const size_t key_size_
  #define MRX_KEY_ADDROF_
#endif

#if MRX_KEY_SORTINT - 0 != 0
  #if MRX_KEY_VARSIZE - 0 != 0
    #error "MRX_KEY_SORTINT and MRX_KEY_VARSIZE cannot both be enabled"
  #endif
static inline void
MC_FUN_(compile_time_sortint_keysize_testing_)(void)
{
    // If this does not compile, the key size does not work with MRX_KEY_SORTINT
    MC_KEY_T key;
    switch(0){case 0:break;
    case sizeof(key)==2||sizeof(key)==4||sizeof(key)==8:break;
    }
}
  #if ARCH_LITTLE_ENDIAN - 0 != 0
    #if __has_builtin(__builtin_choose_expr)
    #define MRX_KEY_SWAP_(key)                                             \
        MC_KEY_T MRX_SWAPPED_KEY_ =                                        \
        __builtin_choose_expr(                                             \
            sizeof(key) == 2, MRX_SWAPPED_KEY_ = bit16_swap(key),          \
            __builtin_choose_expr(                                         \
                sizeof(key) == 4, MRX_SWAPPED_KEY_ = bit32_swap(key),      \
                __builtin_choose_expr(                                     \
                    sizeof(key) == 8, MRX_SWAPPED_KEY_ = bit64_swap(key),  \
                    (void)0)));
    #else
    #define MRX_KEY_SWAP_(key)                                             \
        MC_KEY_T MRX_SWAPPED_KEY_ =                                        \
           ((sizeof(key) == 2) ? bit16_swap(key) :                         \
            ((sizeof(key) == 4) ? bit32_swap(key) :                        \
             ((sizeof(key) == 8) ? bit64_swap(key) : 0)));
    #endif
  #elif ARCH_BIG_ENDIAN - 0 != 0
    #define MRX_KEY_SWAP_(key)
    #define MRX_SWAPPED_KEY_ key
  #else
    #error "unknown endian"
  #endif
#else
  #define MRX_KEY_SWAP_(key)
  #define MRX_SWAPPED_KEY_ key
#endif

typedef struct MC_T_ {
    mrx_base_t mrx;
} MC_T;

typedef struct {
    bool is_on_stack;
    mrx_iterator_t it;
} MC_ITERATOR_T;


static inline size_t
MC_FUN_(itsize)(MC_T * const mrx)
{
    return sizeof(MC_ITERATOR_T) + mrx_itdynsize_(&mrx->mrx);
}

static inline MC_ITERATOR_T *
MC_FUN_(begin)(MC_T * const mrx)
{
    MC_ITERATOR_T *it;
    if (mrx->mrx.root == NULL) {
        return NULL;
    }
    it = malloc(MC_FUN_(itsize)(mrx));
    it->is_on_stack = false;
    mrx_itinit_(&mrx->mrx, &it->it);
    return it;
}

static inline MC_ITERATOR_T *
MC_FUN_(beginst)(MC_T * const mrx,
                 void *itsize_space)
{
    MC_ITERATOR_T *it = (MC_ITERATOR_T *)itsize_space;
    if (mrx->mrx.root == NULL) {
        return NULL;
    }
    it->is_on_stack = true;
    mrx_itinit_(&mrx->mrx, &it->it);
    return it;
}

static inline MC_ITERATOR_T *
MC_FUN_(next)(MC_ITERATOR_T * const it)
{
    if (!mrx_next_(&it->it)) {
        if (!it->is_on_stack) {
            free(it);
        }
        return NULL;
    }
    return it;
}

static inline MC_ITERATOR_T *
MC_FUN_(end)(void)
{
    return NULL;
}

static inline void
MC_FUN_(itdelete)(MC_ITERATOR_T * const it)
{
    if (it == NULL) {
        return;
    }
    if (!it->is_on_stack) {
        return;
    }
    free(it);
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(val)(MC_ITERATOR_T * const it)
{
    union mrx_node *node = it->it.path[it->it.level].node;
    const uint8_t nsz = MRX_NODE_HDR_NSZ_(node->hdr);
    return MC_OPT_DREF_ (MC_VALUE_T *)mrx_node_value_ref_(node, nsz);
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(setval)(MC_ITERATOR_T * const it MC_OPT_VALUE_INSERT_ARG_)
{
    union mrx_node *node = it->it.path[it->it.level].node;
    const uint8_t nsz = MRX_NODE_HDR_NSZ_(node->hdr);
    void *val = (void *)mrx_node_value_ref_(node, nsz);
    MC_OPT_FREE_VALUE_((MC_OPT_DREF_(MC_VALUE_T *)val));
    MC_OPT_ASSIGN_VALUE_((MC_OPT_DREF_(MC_VALUE_T *)val), value);
    return MC_OPT_DREF_ (MC_VALUE_T *)val;
}

#if MRX_KEY_VARSIZE - 0 == 0
static inline MC_KEY_T
MC_FUN_(key)(MC_ITERATOR_T * const it)
{
    MC_KEY_T key;
    if (it->it.key_level == it->it.level) {
        key = *(const MC_KEY_T *)it->it.key;
        MRX_KEY_SWAP_(key);
        return MRX_SWAPPED_KEY_;
    }
    key = *(const MC_KEY_T *)mrx_key_(&it->it);
    MRX_KEY_SWAP_(key);
    return MRX_SWAPPED_KEY_;
}
#else
static inline const MC_KEY_T
MC_FUN_(key)(MC_ITERATOR_T * const it)
{
    if (it->it.key_level == it->it.level) {
        return (const MC_KEY_T)it->it.key;
    }
    return (const MC_KEY_T)mrx_key_(&it->it);
}
#endif

static inline void
MC_FUN_(clear_nodes_)(MC_T * const mrx)
{
#if defined(MC_FREE_VALUE)
    void *itspace = alloca(MC_FUN_(itsize)(mrx));
    MC_ITERATOR_T *it;
    for (it = MC_FUN_(beginst)(mrx, itspace);
         it != MC_FUN_(end)();
         it = MC_FUN_(next)(it))
    {
        MC_OPT_FREE_VALUE_(MC_FUN_(val)(it));
    }
#endif
}

static inline MC_T *
MC_FUN_(new)(const size_t capacity)
{
    MC_T *mrx;

#if MC_MM_MODE == MC_MM_COMPACT
    if ((mrx = malloc(sizeof(MC_T))) == NULL) {
        return NULL;
    }
    mrx_init_(&mrx->mrx, capacity, 1);
#else
    if ((mrx = malloc(sizeof(MC_T) + sizeof(struct mrx_buddyalloc))) == NULL) {
        return NULL;
    }
    mrx_init_(&mrx->mrx, capacity, 0);
#endif
    return mrx;
}

static inline void
MC_FUN_(delete)(MC_T * const mrx)
{
    if (mrx == NULL) {
        return;
    }
    MC_FUN_(clear_nodes_)(mrx);
    mrx_delete_(&mrx->mrx);
    free(mrx);
}

static inline void
MC_FUN_(clear)(MC_T * const mrx)
{
    MC_FUN_(clear_nodes_)(mrx);
    mrx_clear_(&mrx->mrx);
}

static inline int
MC_FUN_(empty)(MC_T * const mrx)
{
    return (mrx->mrx.count == 0);
}

static inline size_t
MC_FUN_(size)(MC_T * const mrx)
{
    return mrx->mrx.count;
}

static inline size_t
MC_FUN_(max_size)(MC_T * const mrx)
{
    return mrx->mrx.capacity;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(insert)(MC_T * const mrx,
                MC_KEY_T const key MRX_KEY_SIZE_ARG_ MC_OPT_VALUE_INSERT_ARG_)
{
    MC_DEF_VALUE_UNDEF_;
    bool is_occupied;
    void *val;

    MRX_KEY_SWAP_(key);
    val = mrx_insert_(&mrx->mrx,
                      (const uint8_t *) MRX_KEY_ADDROF_ MRX_SWAPPED_KEY_,
                      MRX_KEY_SIZE_, &is_occupied);
    if (val == NULL) {
        return undef_value;
    }
    if (is_occupied) {
        MC_OPT_FREE_VALUE_((MC_OPT_DREF_(MC_VALUE_T *)val));
    }
    MC_OPT_ASSIGN_VALUE_((MC_OPT_DREF_(MC_VALUE_T *)val), value);
    return MC_OPT_DREF_ (MC_VALUE_T *)val;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(erase)(MC_T * const mrx,
               MC_KEY_T const key MRX_KEY_SIZE_ARG_)
{
    MC_VALUE_T MC_OPT_PTR_ value;
    MC_DEF_VALUE_UNDEF_;
    bool was_erased;

    if (mrx->mrx.root == NULL) {
        return undef_value;
    }
    MRX_KEY_SWAP_(key);
    value = (MC_VALUE_T MC_OPT_PTR_)(uintptr_t)
        mrx_erase_(&mrx->mrx,
                   (const uint8_t *) MRX_KEY_ADDROF_ MRX_SWAPPED_KEY_,
                   MRX_KEY_SIZE_, &was_erased);
    if (!was_erased) {
        return undef_value;
    }
    // pointer cast hack to avoid warning of returning free'd value for certain configurations
    uintptr_t valptr = (uintptr_t)value;
    (void)valptr;
    MC_OPT_FREE_VALUE_((MC_VALUE_T MC_OPT_PTR_)valptr);
    return value;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(find)(MC_T * const mrx,
              MC_KEY_T const key MRX_KEY_SIZE_ARG_)
{
    MC_DEF_VALUE_UNDEF_;
    if (mrx->mrx.root == NULL) {
        return undef_value;
    }
    MRX_KEY_SWAP_(key);
    void *val = mrx_find_(mrx->mrx.root,
                          (const uint8_t *) MRX_KEY_ADDROF_ MRX_SWAPPED_KEY_,
                          MRX_KEY_SIZE_);
    if (val == NULL) {
        return undef_value;
    }
    return MC_OPT_DREF_ (MC_VALUE_T *)val;
}

#if MRX_KEY_VARSIZE - 0 != 0
static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(insertnt)(MC_T * const mrx,
                  MC_KEY_T const key MC_OPT_VALUE_INSERT_ARG_)
{
    return MC_FUN_(insert)(mrx, key, strlen((const char *)key)
                           MC_OPT_VALUE_PARAM_);
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(erasent)(MC_T * const mrx,
                 MC_KEY_T const key)
{
    const size_t slen = strlen((const char *)key);
    return MC_FUN_(erase)(mrx, key, slen);
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(findnt)(MC_T * const mrx,
                MC_KEY_T const key)
{
    MC_DEF_VALUE_UNDEF_;
    if (mrx->mrx.root == NULL) {
        return undef_value;
    }
    void *val = mrx_findnt_(mrx->mrx.root, (const uint8_t *)key);
    if (val == NULL) {
        return undef_value;
    }
    return MC_OPT_DREF_ (MC_VALUE_T *)val;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(findnear)(MC_T * const mrx,
                  MC_KEY_T const key MRX_KEY_SIZE_ARG_,
                  int * const match_len_)
{
    MC_DEF_VALUE_UNDEF_;
    int match_len__;
    int *match_len = (match_len_ == NULL) ? &match_len__ : match_len_;

    if (mrx->mrx.root == NULL) {
        *match_len = 0;
        return undef_value;
    }
    MRX_KEY_SWAP_(key);
    void *val = mrx_findnear_(mrx->mrx.root,
                              (const uint8_t *) MRX_KEY_ADDROF_ MRX_SWAPPED_KEY_,
                              MRX_KEY_SIZE_, match_len);
    if (val == NULL) {
        return undef_value;
    }
    return MC_OPT_DREF_ (MC_VALUE_T *)val;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(findnearnt)(MC_T * const mrx,
                     MC_KEY_T const key,
                     int * const match_len_)
{
    int match_len__;
    int *match_len = (match_len_ == NULL) ? &match_len__ : match_len_;
    const size_t slen = strlen((const char *)key);
    return MC_FUN_(findnear)(mrx, key, slen, match_len);
}
#endif // MRX_KEY_VARSIZE - 0 != 0

#include <mc_tmpl_undef.h>
#undef MRX_KEY_VARSIZE
#undef MRX_KEY_SORTINT
#undef MRX_KEY_SWAP_
#undef MRX_SWAPPED_KEY_
#undef MRX_KEY_SIZE_ARG_
#undef MRX_KEY_SIZE_
#undef MRX_KEY_ADDROF_
