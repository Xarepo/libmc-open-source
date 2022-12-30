/*
 * Copyright (c) 2011 - 2012, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*
  Single-hashing open-addressed hash table. Extremely fast in best case, but
  very poor performance if worst cases are triggered - for expert users only.

  Compile time options / tuning:

  MHT_KEYEQUAL(a, b) - the key comparison macro which is default 'a == b',
  where 'a' is the key in the table and 'b' is the key given as function
  argument.

  MHT_HASHFUNC(hash, key) - the hash value macro, which generates a uint32_t
  hash value. Default hash functions (Knuth multiplicative method) are provided
  for 32 and 64 bit integer keys. Note that a suitable hash function is of
  key importance for performance, a poor hash function can generate many
  collisions and thus very poor performance.

  Note that if a complex hash function is required that may by itself reduce
  performance so much that it is more suitable to use a standard tree container.

  Other hash functions exist here, use them by pre-defining MHT_HASHFUNC to the
  targeted hash function, for example:

  #define MHT_HASHFUNC MHT_HASHFUNC_ALIGNED_PTR

  Look in the header below for MHT_HASHFUNC_* macros to find other pre-defined
  hash functions.

  Default configuration:

  Map with 'intptr_t' keys to 'void *', with -1 as undefined key and NULL
  as undefined value. Knuth multiplicative method as hash function.

 */

/*

  Design notes

    - A simple hash function (Knuth multiplicative method) is used per default
      which is good enough for many cases. The problem with more generic and
      "safer" hash functions is that the often cost more than the common
      case runtime of the complete insert/erase/find() functions.
    - Power of two table size allows for fast & instead of slow % when modding
      the hash value.
    - Rehash is made directly at removal so there are no dead keys.
    - By using single hashing only rehashing the local cluster is required,
      which in the common case is fast. The drawback of single hashing is that
      clustering risk is larger, so the hash function must be chosen with care.
    - Key/values are stored together a tradeoff made to be good for lookups but
      worse for traversals.
    - The data array is stored directly in base structure to avoid an extra
      pointer lookup.
    - An extra element at the end avoids end-of-table testing in the mht_next()
      function.

    A couple of rehash examples to give an overview of how rehashing works:

       h = the hash value of the current position, masked for the table size.
       epos = empty table position

    Here's a simple example, where the cluster in which removal is made does
    not wrap around in the table. In this case we just scan through the
    cluster and if h <= epos then that element is moved to epos and rehashing
    is complete.

          r
        0 1 2 3 4 5 6 7         0 1 2 3 4 5 6 7
       [_ b c b _ _ _ _]  -->  [_ b c _ _ _ _ _]

    Here are special cases when the cluster is wrapped:

                      r
        0 1 2 3 4 5 6 7         0 1 2 3 4 5 6 7
       [a g _ _ _ _ g h]  -->  [a _ _ _ _ _ g g]

        r
        0 1 2 3 4 5 6 7         0 1 2 3 4 5 6 7
       [a h c b _ _ _ h]  -->  [h b c _ _ _ _ h]

    To make this work we need to expand the simple h <= epos test to the
    following: (epos - h) & size_mask <= half_size. The "(epos - h) & size_mask"
    part calculates how many steps to the right from epos h is, including wrap.
    To make it work though, cluster length cannot be longer than half_size,
    meaning that we cannot fill the table more than half, which for a high
    performance hash table is not really a bad limitation and also contributes
    to keep the clusters shorter, and thus rehashes faster.

*/

#ifndef MC_PREFIX
#define MC_PREFIX mht
#define MC_KEY_T intptr_t
#define MC_KEY_UNDEFINED (-1)
#define MC_KEY_DIFFERENT_FROM_UNDEFINED 0
#define MC_VALUE_T void *
#define MHT_HASHFUNC MHT_HASHFUNC_PTR
#endif

#define MC_ASSOCIATIVE_CONTAINER_ 1
#define MC_NEED_KEY_UNDEFINED_ 1
#define MC_NEED_KEY_DIFFERENT_FROM_UNDEFINED_ 1
#define MC_MM_DEFAULT_ MC_MM_STATIC
#define MC_MM_SUPPORT_ MC_MM_STATIC
#include <mc_tmpl.h>

#ifndef MHT_TMPL_ONCE_
#define MHT_TMPL_ONCE_
#include <bitops.h>
#include <mc_arch.h>

/* Knuth multiplicative method for 32 bit and 64 bit integers */
#define MHT_HASHFUNC_U32(hash, key) \
    *(hash) = (uint32_t)(key) * 2654435761u
#define MHT_HASHFUNC_U64(hash, key)                   \
    *(hash) = ((uint32_t)((uint64_t)(key) >> 32u) ^   \
               (uint32_t)(key)) * 2654435761u

#endif // MHT_TMPL_ONCE_

/* Hash function for aligned pointers, an alignment of at least sizeof(ptr)
   is required, that is what normal allocators such as malloc() return. A
   version for unaligned pointers is also here. */
#if ARCH_SIZEOF_PTR == 4
#define MHT_HASHFUNC_ALIGNED_PTR(hash, key)             \
    MHT_HASHFUNC_U32(hash, ((uintptr_t)(key)) >> 2u)
#define MHT_HASHFUNC_PTR(hash, key)                     \
    MHT_HASHFUNC_U32(hash, (uintptr_t)(key))
#elif ARCH_SIZEOF_PTR == 8
#define MHT_HASHFUNC_ALIGNED_PTR(hash, key)             \
    MHT_HASHFUNC_U64(hash, ((uintptr_t)(key) >> 3u))
#define MHT_HASHFUNC_PTR(hash, key)                     \
    MHT_HASHFUNC_U64(hash, (uintptr_t)(key))
#endif

#ifndef MHT_HASHFUNC
#if __has_builtin(__builtin_choose_expr)
/* Compile error here? Then you need to specify MHT_HASHFUNC. */
#define MHT_HASHFUNC(hash, key)                                          \
    __builtin_choose_expr(sizeof(key) == 4, MHT_HASHFUNC_U32(hash, key), \
    __builtin_choose_expr(sizeof(key) == 8, MHT_HASHFUNC_U64(hash, key), \
                         (void)0))
#else
/* If you get this compile error it means that the compiler could not figure
   out which hash function to use, and you must specify one manually through a
   pre define, such as #define MHT_HASHFUNC MHT_HASHFUNC_U32 */
 #error "Auto-choice of MHT_HASHFUNC not supported, pick your own hash function"
#endif
#endif

#ifndef MHT_KEYEQUAL
#define MHT_KEYEQUAL(key1, key2) key1 == key2
#endif

static inline void
MC_FUN_(test_hashfunc_and_keyequal_)(void)
{
    MC_KEY_T const key = MC_KEY_UNDEFINED;
    uint32_t hash;

    /* Compile error here? You then need to define your own MHT_HASHFUNC and
       MHT_KEYEQUAL. */
    if (MHT_KEYEQUAL(key, key)) {
        MHT_HASHFUNC(&hash, key);
    }
}

#define MHT_T_KV MC_CONCAT_(MC_T, _kv)
struct MHT_T_KV {
    MC_KEY_T key;
#if MC_NO_VALUE - 0 == 0
    MC_VALUE_T value;
#endif
};

typedef struct MC_T_ {
    uint32_t size_mask;
    uint32_t half_size;
    uint32_t key_count;
    uint32_t capacity;
    struct MHT_T_KV kv[];
} MC_T;

static inline void
MC_FUN_(init)(MC_T * const mht,
              struct MHT_T_KV * const kv,
              const size_t capacity,
              const size_t sizeof_kv_table)
{
    MC_DEF_KEY_DIFF_UNDEF_;
    MC_DEF_KEY_UNDEF_;
    unsigned int i;
    uint32_t size;

    size = sizeof_kv_table / sizeof(struct MHT_T_KV) - 1;
    /* test if it is a valid size that works with the hashtable algorithm,
       that is not less than 2, not larger than ~2 billion (0x7FFFFFF),
       and that it is a power of two. */
    if (size < 2 ||
        size > 0x7FFFFFF ||
        size != (1u << bit32_bsr(size)) ||
        capacity > (size >> 1u) ||
        mht->kv != kv)
    {
        abort();
    }
    mht->size_mask = size - 1u;
    mht->half_size = size >> 1u;
    mht->capacity = (const uint32_t)capacity;
    mht->key_count = 0;
    for (i = 0; i < size; i++) {
        mht->kv[i].key = undef_key;
    }
    mht->kv[size].key = diff_undef_key;
}

static inline uint32_t
MC_FUN_(capacity_to_table_size_)(const size_t capacity)
{
    return 1u << (bit32_bsr((const uint32_t)capacity) + 2u);
}

static inline MC_T *
MC_FUN_(new)(const size_t capacity)
{
    uint32_t size;
    MC_T *mht;

    size = MC_FUN_(capacity_to_table_size_)(capacity);
    mht = malloc(sizeof(*mht) + (size + 1) * sizeof(mht->kv[0]));
    MC_FUN_(init)(mht, mht->kv, capacity, (size + 1) * sizeof(mht->kv[0]));
    return mht;
}

static inline void
MC_FUN_(delete)(MC_T * const mht)
{
    if (mht == NULL) {
        return;
    }
#if defined(MC_FREE_KEY) || defined(MC_FREE_VALUE)
    struct MHT_T_KV *kv = mht->kv;
    MC_DEF_KEY_UNDEF_;

    for (;;) {
        while (MHT_KEYEQUAL(kv->key, undef_key)) {
            kv++;
        }
        if (kv == &mht->kv[mht->size_mask + 1]) {
            break;
        }
        MC_OPT_FREE_KEY_(kv->key);
#if MC_NO_VALUE - 0 == 0
        MC_OPT_FREE_VALUE_(kv->value);
#endif
        kv++;
    }
#endif
    free(mht);
}

static inline void
MC_FUN_(clear)(MC_T * const mht)
{
#if defined(MC_FREE_KEY) || defined(MC_FREE_VALUE)
    struct MHT_T_KV *kv = mht->kv;
    MC_DEF_KEY_UNDEF_;

    for (;;) {
        while (MHT_KEYEQUAL(kv->key, undef_key)) {
            kv++;
        }
        if (kv == &mht->kv[mht->size_mask + 1]) {
            break;
        }
        MC_OPT_FREE_KEY_(kv->key);
        kv->key = undef_key;
#if MC_NO_VALUE - 0 == 0
        MC_OPT_FREE_VALUE_(kv->value);
#endif
        kv++;
    }
    mht->key_count = 0;
#else
    MC_FUN_(init)(mht, mht->kv, mht->capacity, (mht->size_mask + 2) * sizeof(mht->kv[0]));
#endif
}

static inline int
MC_FUN_(empty)(MC_T * const mht)
{
    return (mht->key_count == 0);
}

static inline size_t
MC_FUN_(size)(MC_T * const mht)
{
    return mht->key_count;
}

static inline size_t
MC_FUN_(max_size)(MC_T * const mht)
{
    return mht->capacity;
}

static inline MC_ITERATOR_T *
MC_FUN_(next)(MC_ITERATOR_T *it)
{
    MC_KEY_T const undef_key = MC_KEY_UNDEFINED;
    struct MHT_T_KV *kv = (struct MHT_T_KV *)it;

    do {
        kv++;
    } while (MHT_KEYEQUAL(kv->key, undef_key));
    return (MC_ITERATOR_T *)kv;
}

static inline MC_ITERATOR_T *
MC_FUN_(begin)(MC_T * const mht)
{
    return MC_FUN_(next)((MC_ITERATOR_T *)&mht->kv[-1]);
}

static inline MC_ITERATOR_T *
MC_FUN_(end)(MC_T * const mht)
{
    const unsigned int end = mht->size_mask + 1;
    return (MC_ITERATOR_T *)&mht->kv[end];
}

static inline MC_ITERATOR_T *
MC_FUN_(itfind)(MC_T * const mht,
                MC_KEY_T const key)
{
    MC_DEF_KEY_UNDEF_;
    uint32_t pos;

    MHT_HASHFUNC(&pos, key);
    for (;;) {
        pos &= mht->size_mask;
        if (MHT_KEYEQUAL(mht->kv[pos].key, key)) {
            return (MC_ITERATOR_T *)&mht->kv[pos];
        }
        if (MHT_KEYEQUAL(mht->kv[pos].key, undef_key)) {
            return MC_FUN_(end)(mht);
        }
        pos++;
    }
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(find)(MC_T * const mht,
              MC_KEY_T const key)
{
    MC_DEF_VALUE_UNDEF_;
    MC_DEF_KEY_UNDEF_;
    uint32_t pos;

    MHT_HASHFUNC(&pos, key);
    for (;;) {
        pos &= mht->size_mask;
        if (MHT_KEYEQUAL(mht->kv[pos].key, key)) {
#if MC_NO_VALUE - 0 != 0
            return MC_OPT_ADDROF_ mht->kv[pos].key;
#else
            return MC_OPT_ADDROF_ mht->kv[pos].value;
#endif
        }
        if (MHT_KEYEQUAL(mht->kv[pos].key, undef_key)) {
            return undef_value;
        }
        pos++;
    }
}

static inline MC_ITERATOR_T *
MC_FUN_(itinsert)(MC_T * const mht,
                  MC_KEY_T const key MC_OPT_VALUE_INSERT_ARG_)
{
    MC_DEF_KEY_UNDEF_;
    uint32_t pos;

    if (mht->key_count == mht->capacity) {
        return MC_FUN_(end)(mht);
    }
    MHT_HASHFUNC(&pos, key);
    for (;;) {
        pos &= mht->size_mask;
        if (MHT_KEYEQUAL(mht->kv[pos].key, undef_key)) {
            mht->key_count++;
            MC_ASSIGN_KEY_(mht->kv[pos].key, key);
#if MC_NO_VALUE - 0 == 0
            MC_OPT_ASSIGN_VALUE_(mht->kv[pos].value, value);
#endif
            return (MC_ITERATOR_T *)&mht->kv[pos];
        }
        if (MHT_KEYEQUAL(mht->kv[pos].key, key)) {
#if MC_NO_VALUE - 0 == 0
            MC_OPT_FREE_VALUE_(mht->kv[pos].value);
            MC_OPT_ASSIGN_VALUE_(mht->kv[pos].value, value);
#endif
            return (MC_ITERATOR_T *)&mht->kv[pos];
        }
        pos++;
    }
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(insert)(MC_T * const mht,
                MC_KEY_T const key MC_OPT_VALUE_INSERT_ARG_)
{
    MC_DEF_VALUE_UNDEF_;
    MC_DEF_KEY_UNDEF_;
    uint32_t pos;

    if (mht->key_count == mht->capacity) {
        return undef_value;
    }
    MHT_HASHFUNC(&pos, key);
    for (;;) {
        pos &= mht->size_mask;
        if (MHT_KEYEQUAL(mht->kv[pos].key, undef_key)) {
            mht->key_count++;
            MC_ASSIGN_KEY_(mht->kv[pos].key, key);
#if MC_NO_VALUE - 0 != 0
            return MC_OPT_ADDROF_ mht->kv[pos].key;
#else
            MC_OPT_ASSIGN_VALUE_(mht->kv[pos].value, value);
            return MC_OPT_ADDROF_ mht->kv[pos].value;
#endif
        }
        if (MHT_KEYEQUAL(mht->kv[pos].key, key)) {
#if MC_NO_VALUE - 0 != 0
            return MC_OPT_ADDROF_ mht->kv[pos].key;
#else
            MC_OPT_FREE_VALUE_(mht->kv[pos].value);
            MC_OPT_ASSIGN_VALUE_(mht->kv[pos].value, value);
            return MC_OPT_ADDROF_ mht->kv[pos].value;
#endif
        }
        pos++;
    }
}

static inline void
MC_FUN_(iterase)(MC_T * const mht,
                 MC_ITERATOR_T * const it)
{
    MC_DEF_KEY_UNDEF_;

    const uint32_t opos = ((uintptr_t)it - (uintptr_t)mht->kv) / sizeof(struct MHT_T_KV);
    mht->key_count--;
    uint32_t epos = opos;
    uint32_t pos = opos;
    MC_OPT_FREE_KEY_(mht->kv[epos].key);
    mht->kv[epos].key = undef_key;
#if MC_NO_VALUE - 0 == 0
    MC_OPT_FREE_VALUE_(mht->kv[epos].value);
    mht->kv[epos].value = 0;
#endif
    for (;;) {
        pos++;
        pos &= mht->size_mask;
        if (MHT_KEYEQUAL(mht->kv[pos].key, undef_key)) {
            return;
        }
        uint32_t h;
        MHT_HASHFUNC(&h, mht->kv[pos].key);
        if (((epos - h) & mht->size_mask) <= mht->half_size) {
            mht->kv[epos] = mht->kv[pos];
            mht->kv[pos].key = undef_key;
            epos = pos;
        }
    }
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(erase)(MC_T * const mht,
               MC_KEY_T const key)
{
    MC_DEF_VALUE_UNDEF_;
    MC_DEF_KEY_UNDEF_;
    uint32_t pos;

    MHT_HASHFUNC(&pos, key);
    for (;;) {
        pos &= mht->size_mask;
        if (MHT_KEYEQUAL(mht->kv[pos].key, key)) {
            /* remove and rehash up to next undef key */
            mht->key_count--;
            uint32_t epos = pos;
#if MC_NO_VALUE - 0 != 0
            MC_VALUE_T MC_OPT_PTR_ value = MC_OPT_ADDROF_ mht->kv[epos].key;
#else
            MC_VALUE_T MC_OPT_PTR_ value = MC_OPT_ADDROF_ mht->kv[epos].value;
            MC_OPT_FREE_VALUE_(mht->kv[epos].value);
#endif
            MC_OPT_FREE_KEY_(mht->kv[epos].key);
            mht->kv[epos].key = undef_key;
            for (;;) {
                pos++;
                pos &= mht->size_mask;
                if (MHT_KEYEQUAL(mht->kv[pos].key, undef_key)) {
                    return value;
                }
                uint32_t h;
                MHT_HASHFUNC(&h, mht->kv[pos].key);
                if (((epos - h) & mht->size_mask) <= mht->half_size) {
                    mht->kv[epos] = mht->kv[pos];
                    mht->kv[pos].key = undef_key;
                    epos = pos;
                }
            }
        }
        if (MHT_KEYEQUAL(mht->kv[pos].key, undef_key)) {
            return undef_value;
        }
        pos++;
    }
}

static inline MC_KEY_T
MC_FUN_(key)(MC_ITERATOR_T * const it)
{
    return ((struct MHT_T_KV *)it)->key;
}

static inline MC_KEY_T *
MC_FUN_(keyp)(MC_ITERATOR_T * const it)
{
    return &((struct MHT_T_KV *)it)->key;
}

static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(val)(MC_ITERATOR_T * const it)
{
#if MC_NO_VALUE - 0 != 0
    return MC_OPT_ADDROF_ ((struct MHT_T_KV *)it)->key;
#else
    return MC_OPT_ADDROF_ ((struct MHT_T_KV *)it)->value;
#endif
}

#if MC_VALUE_NO_INSERT_ARG - 0 == 0
static inline MC_VALUE_T MC_OPT_PTR_
MC_FUN_(setval)(MC_ITERATOR_T * const it MC_OPT_VALUE_INSERT_ARG_)
{
    MC_OPT_FREE_VALUE_(((struct MHT_T_KV *)it)->value);
    MC_OPT_ASSIGN_VALUE_(((struct MHT_T_KV *)it)->value, value);
    return MC_OPT_ADDROF_ ((struct MHT_T_KV *)it)->value;
}
#endif

#include <mc_tmpl_undef.h>
#undef MHT_HASHFUNC
#undef MHT_HASHFUNC_PTR
#undef MHT_HASHFUNC_ALIGNED_PTR
#undef MHT_KEYEQUAL
#undef MHT_T_KV
