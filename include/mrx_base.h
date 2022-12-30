/*
 * Copyright (c) 2013, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#ifndef MRX_BASE_H
#define MRX_BASE_H

#include <mc_arch.h>
#include <nodepool_base.h>

#if ARCH_BIG_ENDIAN - 0 != 0
 #error "The radix tree currently does not support big endian"
#endif

typedef uint32_t mrx_sp_t;  // 32 bit "short" pointer

struct mrx_buddyalloc {
    uint32_t nonempty_freelists;
#if ARCH_SIZEOF_PTR == 4
    void *freelists[4];
#elif ARCH_SIZEOF_PTR == 8
    void *freelists[3];
#endif
    struct nodepool superblocks;
};

struct mrx_base_t_ {
    union mrx_node *root;
    uintptr_t count;
    uintptr_t capacity;
#define MRX_FLAGS_MASK_ 0x80000000u
#define MRX_FLAG_IS_COMPACT_ 0x80000000u
    uint32_t max_keylen_n_flags;
    struct mrx_buddyalloc nodealloc[];
};

typedef struct mrx_base_t_ mrx_base_t;

struct mrx_ptrpfx_node {
    uint16_t hdr;
    uint16_t lp_count;
    uint32_t pad_;
    union {
        mrx_sp_t sp[2];
        void *value;
    } orig;
    mrx_sp_t hp[];
};

union mrx_node {
    uint16_t hdr;
    struct { // scan node
        uint16_t hdr;
        uint8_t octets[126];
    } sn;
    struct { // mask node
        uint16_t hdr;
        uint8_t branch_count;
        uint8_t used;
        uint8_t lp_count;
        uint8_t pad_[3];
        union {
            uint32_t u32[8];
            uint64_t u64[4];
        } bitmask;
        mrx_sp_t next[8];
#if ARCH_SIZEOF_PTR == 4
        mrx_sp_t local[13];
#elif ARCH_SIZEOF_PTR == 8
        mrx_sp_t local[12];
#endif
        void *value;
    } mn;
    union { // aliases for pointer access
        mrx_sp_t s[32];
#if ARCH_SIZEOF_PTR == 4
        void *n[32];
#elif ARCH_SIZEOF_PTR == 8
        void *n[16];
#endif
    } nptr;
};

#if ARCH_SIZEOF_PTR == 4
#define MRX_NODE_VALUE_REF_(node, nsz) (&(node)->nptr.n[((2u << (unsigned)(nsz)) - 1u) & 0x1Fu])
#elif ARCH_SIZEOF_PTR == 8
#define MRX_NODE_VALUE_REF_(node, nsz) (&(node)->nptr.n[((((2u << (unsigned)(nsz)) - 1u) & 0x1Fu) - 1u) >> 1u])
#endif
#define MRX_NODE_MASK_NODE_NSZ_VALUE_ 0x7u
#define MRX_NODE_HDR_NSZ_(hdr) ((hdr) & 0x7u)
#define MRX_IS_MASK_NODE_(nsz) ((nsz) == MRX_NODE_MASK_NODE_NSZ_VALUE_)

#define MRX_HDR_BIT_LONGPTR_ 0x08u
#if ARCH_SIZEOF_PTR == 4
#define MRX_HDR_IS_SHORT_PTR_(hdr) (1u)
#elif ARCH_SIZEOF_PTR == 8
#define MRX_HDR_IS_SHORT_PTR_(hdr) (((hdr) & MRX_HDR_BIT_LONGPTR_) == 0)
#endif

#if ARCH_SIZEOF_PTR == 4
#define mrx_node_value_ref_ MRX_NODE_VALUE_REF_
#elif ARCH_SIZEOF_PTR == 8
static inline void **
mrx_node_value_ref_(union mrx_node *node,
                    const uint8_t nsz)
{
    if (MRX_HDR_IS_SHORT_PTR_(node->hdr) || MRX_IS_MASK_NODE_(nsz)) {
        return MRX_NODE_VALUE_REF_(node, nsz);
    }
    struct mrx_ptrpfx_node *pfx = *(struct mrx_ptrpfx_node **)MRX_NODE_VALUE_REF_(node, nsz);
    return &pfx->orig.value;
}
#endif

void
mrx_init_(mrx_base_t *mrx,
          size_t capacity,
          bool is_compact);

void
mrx_clear_(mrx_base_t *mrx);

void
mrx_delete_(mrx_base_t *mrx);

#define MRX_MAX_KEY_LENGTH_FOR_PATH_ON_STACK 512
void **
mrx_insert_(mrx_base_t *mrx,
            const uint8_t string[],
            unsigned string_length,
            bool *is_occupied);

void **
mrx_find_(union mrx_node *root,
          const uint8_t string[],
          unsigned string_length);

void **
mrx_findnt_(union mrx_node *root,
            const uint8_t string[]);

void **
mrx_findnear_(union mrx_node *root,
              const uint8_t string[],
              unsigned string_length,
              int *match_len);

void *
mrx_erase_(mrx_base_t *mrx,
           const uint8_t string[],
           unsigned string_length,
           bool *was_erased);

struct mrx_iterator_path_element {
    union mrx_node *node;
    int16_t br_pos;
    uint8_t br;
};

struct mrx_iterator_t_ {
    uint8_t *key;
    int level;
    int key_len;
    int key_level;
    int key_level_len;
    struct mrx_iterator_path_element path[];
};

typedef struct mrx_iterator_t_ mrx_iterator_t;

size_t
mrx_itdynsize_(mrx_base_t *mrx);

void
mrx_itinit_(mrx_base_t *mrx,
            mrx_iterator_t *it);

bool
mrx_next_(mrx_iterator_t *it);

const uint8_t *
mrx_key_(mrx_iterator_t *it);

void
mrx_constructor_(void);

void
mrx_disable_simd(void);

#endif
