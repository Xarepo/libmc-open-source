/*
 * Copyright (c) 2013, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

// Header file for radix tree internals, not to be used by the API user
#ifndef MRX_BASE_INT_H
#define MRX_BASE_INT_H

#include <string.h> // memcpy etc

#include <bitops.h>
#include <mrx_base.h>

#define SCAN_NODE_MAX_BRANCH_COUNT 25u
#if ARCH_SIZEOF_PTR == 4
#define MASK_NODE_MAX_LOCAL_BRANCH_COUNT 13u
#define SCAN_NODE_MAX_BRANCH_COUNT_WITH_VALUE 24u
#elif ARCH_SIZEOF_PTR == 8
#define MASK_NODE_MAX_LOCAL_BRANCH_COUNT 12u
#define SCAN_NODE_MAX_BRANCH_COUNT_WITH_VALUE 23u
#endif

#define MASK_NODE_LOCAL_BRANCH_COUNT_FOR_MOVING_BACK (MASK_NODE_MAX_LOCAL_BRANCH_COUNT-3)

#if ARCH_SIZEOF_PTR == 4
#define PTR_PREFIX_MATCHES(ptr1, ptr2) (1u)
#elif ARCH_SIZEOF_PTR == 8
#define PTR_PREFIX_MATCHES(ptr1, ptr2) (((uintptr_t)(ptr1) >> 32u) == ((uintptr_t)(ptr2) >> 32u))
#endif


union mrx_next_block {
    mrx_sp_t sp[32];
    struct {
#define NEXT_BLOCK_LONG_PTR_HDR 0x5u
        uint32_t hdr_and_pad_; // access header through macros instead
        uint32_t lp_count;
#define NEXT_BLOCK_MAX_LP_COUNT 14u
        union mrx_node *lp[NEXT_BLOCK_MAX_LP_COUNT];
        union mrx_next_block *next;
    } lp;
};
#define SP_MASK 0xFFFFFFF8u
#if ARCH_SIZEOF_PTR == 4
#define NEXT_BLOCK_IS_SHORT_PTR(nx) 1u
#elif ARCH_SIZEOF_PTR == 8
#define NEXT_BLOCK_IS_SHORT_PTR(nx) (NEXT_BLOCK_HDR(nx) != NEXT_BLOCK_LONG_PTR_HDR)
#endif
#define NEXT_BLOCK_HDR_SET(nx, nsz) (nx)->sp[0] &= SP_MASK; (nx)->sp[0] |= (unsigned)(nsz)
#define NEXT_BLOCK_HDR(nx) ((nx)->sp[0] & ~SP_MASK)

/*
  This code has been checked with a clang-tidy rule that requires all binary bitwise operators
  work on unsigned operands.

  Sizeof int is in the code assumed to be at least 32 bit.

  Important to remember about te C language regarding this:
  - No bit operator works on smaller type than int, any smaller type will be promoted
    to at least int. An unsigned shorter type is promoted to signed int.
  - Bitwise operators on larger types than int can be dangerous, 64 bit pointer 32 bit int:
     - ptr & ~0x3u => ptr & 0x00000000FFFFFFF8
     - ptr & ~((uintptr_t)0x3u) => ptr & 0xFFFFFFFFFFFFFFF8
     - typically bitwise not ~ is the one to look out for unforeseen bugs.
 */


/*
  - Short pointer: 32 bit pointer aligned to at least 8 bytes, ie 29 bits are used, 3 bits are free for other use.
    For little endian it's the 3 LSB bits, for big endian the 3 MSB bits.
  - Short pointers may contain garbage bits in the three bits, ie the must be ignored when used as pointer
  - Macros:
      - SP(): Extract short pointer from long pointer
      - EP(): Expand short pointer to long pointer, by providing reference to get long pointer prefix
      - EP2(): Expand short pointer, prefix provided as separate 32 bit field
      - HP(): Extract long pointer 32 bit prefix from long pointer.
 */

#define SP(ptr) ((mrx_sp_t)((uintptr_t)(ptr) & SP_MASK))
#if ARCH_SIZEOF_PTR == 4
#define HP(ptr) 0
#define EP(ptr, ref) ((void *)SP(ptr))
#define EP2(lo_ptr, hi_ptr) ((void *)((lo_ptr) & SP_MASK))
#elif ARCH_SIZEOF_PTR == 8
#define HP(ptr) ((mrx_sp_t)((uintptr_t)(ptr) >> 32u))
#define EP(lo_ptr, ref)  ((void *)((uintptr_t)((lo_ptr) & SP_MASK) | ((uintptr_t)(ref) & 0xFFFFFFFF00000000u)))
#define EP2(lo_ptr, hi_ptr) ((void *)((uintptr_t)((lo_ptr) & SP_MASK) | ((uintptr_t)(hi_ptr) << 32u)))
#endif

#define SP_ALIGN(ptr) ((mrx_sp_t *)(((uintptr_t)(ptr) + 3u) & ~((uintptr_t)3u)))
#define NODE_END_REF_(node, nsz) (&(node)->nptr.s[(((2u << (unsigned)(nsz)) - 1u) & 0x1Fu) + 1u])
#define NODE_END_REF(node, nsz) ((void *)NODE_END_REF_(node, nsz))
#define NODE_END_CONST_REF(node, nsz) ((const void *)NODE_END_REF_(node, nsz))
#define VALUE_REF(node, nsz) MRX_NODE_VALUE_REF_(node, nsz)

#define HDR_BITS_MASK_NODE_NSZ = 0x7u
#define HDR_BIT_HAS_VALUE 0x0100u
#define HDR_BIT_LONGPTR MRX_HDR_BIT_LONGPTR_
#define HDR_INIT_PX_LEN(hdr, px_len) hdr |= ((unsigned)(px_len) << 9u)
#define HDR_INIT_BR_LEN(hdr, br_len) hdr |= ((((unsigned)(br_len) & 0x0Fu) << 4u) | (((unsigned)(br_len) & 0x10u) >> 4u))
#define HDR_SET(hdr, nsz, br_len, islong, px_len, hasvalue)                             \
    hdr = (unsigned)(nsz) |                                                             \
        ((((unsigned)(br_len) & 0x0Fu) << 4u) | (((unsigned)(br_len) & 0x10u) >> 4u)) | \
        ((unsigned)(px_len) << 9u) |                                                    \
        ((islong) ? HDR_BIT_LONGPTR : 0) | ((hasvalue) ? HDR_BIT_HAS_VALUE : 0)
#define HDR_SET_HAS_VALUE(hdr) hdr |= HDR_BIT_HAS_VALUE
#define HDR_SET_PX_LEN(hdr, px_len) hdr &= 0x01FFu; (hdr) |= ((unsigned)(px_len) << 9u)
#define HDR_SET_BR_LEN(hdr, br_len) hdr &= 0xFF0Fu & ~(((hdr) & 0x4u) >> 2u); \
    (hdr) |= ((((unsigned)(br_len) & 0x0Fu) << 4u) | (((unsigned)(br_len) & 0x10u) >> 4u))
#define HDR_SET_NSZ(hdr, nsz) (hdr) &= 0xFFF8u; (hdr) |= (nsz)
#define HDR_SET_NSZ_BR_LEN(hdr, nsz, br_len) hdr &= 0xFF08u; \
    (hdr) |= ((((unsigned)(br_len) & 0x0Fu) << 4u) | (((unsigned)(br_len) & 0x10u) >> 4u)) | (unsigned)(nsz)
#define HDR_CLR_HAS_VALUE(hdr) hdr &= ~HDR_BIT_HAS_VALUE
#if ARCH_SIZEOF_PTR == 4
#define HDR_SET_LONGPTR(hdr)
#define HDR_CLR_LONGPTR(hdr)
#else
#define HDR_SET_LONGPTR(hdr) hdr |= HDR_BIT_LONGPTR
#define HDR_CLR_LONGPTR(hdr) hdr &= ~HDR_BIT_LONGPTR
#endif // ARCH_SIZEOF_PTR == 4
#define FLATTEN_SN_NSZ(nsz) ((nsz) &= ~((unsigned)(nsz) >> 2u)) // 101 becomes 100
#define IS_SCAN_NODE(nsz) (!MRX_IS_MASK_NODE_(nsz))
#define IS_MASK_NODE(nsz) MRX_IS_MASK_NODE_(nsz)
#define HDR_NSZ(hdr) MRX_NODE_HDR_NSZ_(hdr)
#define HDR_SN_FLATTENED_NSZ(hdr) (MRX_NODE_HDR_NSZ_(hdr) & ~((unsigned)MRX_NODE_HDR_NSZ_(hdr) >> 2u)) // same as applying FLATTEN_SN_NSZ()
#define HDR_HAS_VALUE(hdr) (((unsigned)(hdr) & HDR_BIT_HAS_VALUE) != 0)
#define HDR_IS_SHORT_PTR(hdr) MRX_HDR_IS_SHORT_PTR_(hdr)
#define HDR_PX_LEN(hdr) ((hdr) >> 9u)
#define HDR_BR_LEN(hdr) ((((unsigned)(hdr) >> 4u) & 0x0Fu) | ((((unsigned)(hdr) & ((unsigned)(hdr) >> 2u)) << 4u) & 0x10u))
#define HDR_MN_LOCAL_USED(hdr) ((unsigned)(hdr) & 0x10u)
#define HDR_MN_SET_LOCAL_USED(hdr, used) (hdr) &= ~0x10u; (hdr) |= ((unsigned)!!(used)) << 4u

#define MAX_P2 4u
#if ARCH_SIZEOF_PTR == 8
#define MIN_P2 1u
#else
#define MIN_P2 0u
#endif

#define MIN_MOVE_PX_LEN_FOR_VALUE(brp_end, vref, br_end) (((brp_end) - (vref)) - ((4u - ((br_end) & 3u)) & 3u))
#define SN_PREFIX_PTR(node) (*(struct mrx_ptrpfx_node **)VALUE_REF(node, HDR_NSZ((node)->hdr)))
#define SN_PREFIX_CONST_PTR(node) (*(const struct mrx_ptrpfx_node * const *)VALUE_REF(node, HDR_NSZ((node)->hdr)))
#define NODE_SIZE_P2_TO_NSZ(size_p2) (bit32_bsr((uint32_t)((size_p2) | 8u)) - 3u)

/* header + prefix + branch octets + alignment + branch pointers +
   (aligment if 64 bit) + value. */
#if ARCH_SIZEOF_PTR == 4
#define SCAN_NODE_MIN_SIZE(px_len, br_len, has_value)     \
    ((((2u + (px_len) + (br_len)) + 3u) & ~3u) +          \
     (br_len) * sizeof(mrx_sp_t) +                        \
     (!!(has_value) * sizeof(void *)))
#elif ARCH_SIZEOF_PTR == 8
static inline uint8_t
scan_node_min_size_(const uint8_t px_len,
                    const uint8_t br_len,
                    const bool has_value)
{
    // header + prefix + branch octets + 4 byte alignment
    uint8_t len = (((2u + px_len + br_len) + 3u) & ~3u);
    if (has_value) {
        len += br_len * sizeof(mrx_sp_t); // + branch pointers
        len = (len + 7u) & ~7u; // + 8 byte alignment
        len += sizeof(void *); // + value
    } else {
        uint8_t lp_len = ((len + 7u) & ~7u) + sizeof(void *); // + 8 byte alignment + ptrpfx pointer
        uint8_t free_brp_count = (unsigned)(lp_len - len) >> 2u;
        if (br_len > free_brp_count) {
            len = lp_len + (br_len - free_brp_count) * sizeof(mrx_sp_t);
            len = (len + 7u) & ~7u;
        } else {
            len = lp_len;
        }
    }
    return len;
}
#define SCAN_NODE_MIN_SIZE(px_len, br_len, has_value)   \
    scan_node_min_size_(px_len, br_len, has_value)
#endif

static inline unsigned
next_power_of_two_for_8bit(unsigned value) // must not exceed 255
{
    value--;
    value |= value >> 1u;
    value |= value >> 2u;
    value |= value >> 4u;
    value++;
    return value;
}

static inline uint8_t
scan_node_min_size_nsz(const uint8_t px_len,
                       const uint8_t br_len,
                       const bool has_value)
{
    const uint8_t min_size = SCAN_NODE_MIN_SIZE(px_len, br_len, has_value);
    const unsigned min_size_p2 = next_power_of_two_for_8bit(min_size);
    return NODE_SIZE_P2_TO_NSZ(min_size_p2);
}

void
mrx_traverse_erase_all_nodes_(mrx_base_t *mrx);

void
mrx_print_node_(union mrx_node *node);

void *
mrx_alloc_node_(mrx_base_t *mrx,
                uint8_t nsz);

void
mrx_free_node_(mrx_base_t *mrx,
               void *ptr,
               uint8_t nsz);

// for the unit test code, make it possible to enable a test allocator
#if MRX_TEST_ALLOCATOR - 0 != 0

struct mrx_test_allocator {
    void *(*alloc_node)(mrx_base_t *, uint8_t);
    void (*free_node)(mrx_base_t *, void *, uint8_t);
};

extern struct mrx_test_allocator mrx_test_allocator_;

static inline void *
alloc_node(mrx_base_t *mrx,
           const uint8_t nsz)
{
    if (mrx_test_allocator_.alloc_node != NULL) {
        return mrx_test_allocator_.alloc_node(mrx, nsz);
    }
    return mrx_alloc_node_(mrx, nsz);
}
static inline void
free_node(mrx_base_t *mrx,
          void *ptr,
          const uint8_t nsz)
{
    if (mrx_test_allocator_.free_node != NULL) {
        mrx_test_allocator_.free_node(mrx, ptr, nsz);
    } else {
        mrx_free_node_(mrx, ptr, nsz);
    }
}
#else
#define alloc_node mrx_alloc_node_
#define free_node mrx_free_node_
#endif // MRX_TEST_ALLOCATOR

void
mrx_set_link_in_parent_slowpath_(mrx_base_t *mrx,
                                 struct mrx_iterator_path_element path[],
                                 int level,
                                 union mrx_node *child);

static inline int
mask_node_next_branch(const union mrx_node * const node,
                      const unsigned start)
{
    if (start < 256) {
        const uint32_t bm = node->mn.bitmask.u32[start >> 5u] >> (start & 0x1Fu);
        if (bm != 0) {
            return (int)(start + bit32_bsf(bm));
        }
        unsigned idx = node->mn.used & ~((1u << ((start >> 5u) + 1u)) - 1u);
        if (idx != 0) {
            idx = bit32_bsf(idx);
            return (int)(bit32_bsf(node->mn.bitmask.u32[idx]) + (idx << 5u));
        }
    }
    return -1;
}

static inline union mrx_node *
scan_node_get_child1(const union mrx_node *node,
                     const mrx_sp_t *brp,
                     const uint8_t br_pos)
{
    if (HDR_IS_SHORT_PTR(node->hdr)) {
        return EP(brp[br_pos], node);
    }
    const struct mrx_ptrpfx_node * const *pfxref = &SN_PREFIX_CONST_PTR(node);
    const mrx_sp_t *bp = &brp[br_pos];
    if ((uintptr_t)bp < (uintptr_t)pfxref) {
        return EP2(*bp, (*pfxref)->hp[br_pos]);
    }
    const unsigned idx = ((uintptr_t)bp - (uintptr_t)pfxref) >> 2u;
    return EP2((*pfxref)->orig.sp[idx], (*pfxref)->hp[br_pos]);
}

static inline union mrx_node *
scan_node_get_child(const union mrx_node *node,
                    const uint8_t *br,
                    const uint8_t br_len,
                    const uint8_t br_pos)
{
    return scan_node_get_child1(node, SP_ALIGN(&br[br_len]), br_pos);
}

static inline void
scan_node_set_child(union mrx_node *node,
                    mrx_sp_t *brp,
                    const uint8_t br_pos,
                    union mrx_node *child)
{
    if (HDR_IS_SHORT_PTR(node->hdr)) {
        brp[br_pos] = SP(child);
        return;
    }
    uintptr_t pfxref = (uintptr_t)&SN_PREFIX_PTR(node);
    uintptr_t bp = (uintptr_t)&brp[br_pos];
    if (bp < pfxref) {
        (*(mrx_sp_t *)bp) = SP(child);
    } else {
        bp -= pfxref;
        (*(struct mrx_ptrpfx_node **)pfxref)->orig.sp[bp >> 2u] = SP(child);
    }
}

static inline union mrx_next_block *
mask_node_get_next_block(const union mrx_node * const node,
                         const uint32_t b)
{
    union mrx_next_block *nx;
    if (HDR_IS_SHORT_PTR(node->hdr)) {
        nx = EP(node->mn.next[b >> 5u], node);
    } else {
        nx = EP2(node->mn.next[b >> 5u], node->mn.local[b >> 5u]);
    }
    return nx;
}

static inline union mrx_node *
next_block_get_child(const union mrx_next_block *nx,
                     uint32_t bidx)
{
    union mrx_node *node;
    if (NEXT_BLOCK_IS_SHORT_PTR(nx)) {
        node = EP(nx->sp[bidx], nx);
    } else {
        while (bidx >= NEXT_BLOCK_MAX_LP_COUNT) {
            nx = nx->lp.next;
            bidx -= NEXT_BLOCK_MAX_LP_COUNT;
        }
        node = nx->lp.lp[bidx];
    }
    return node;
}

void
mrx_ptrpfx_sn_copy_to_short_slowpath_(union mrx_node *target,
                                      union mrx_node *source);
static inline void
ptrpfx_sn_copy_to_short(union mrx_node *target,
                        union mrx_node *source,
                        const uint8_t nsz)
{
    if (HDR_IS_SHORT_PTR(source->hdr)) {
        memcpy(target, source, 8u << nsz);
        return;
    }
    mrx_ptrpfx_sn_copy_to_short_slowpath_(target, source);
}

static inline void
ptrpfx_sn_free(mrx_base_t *mrx,
               union mrx_node *node)
{
    if (HDR_IS_SHORT_PTR(node->hdr)) {
        return;
    }
    struct mrx_ptrpfx_node *pfx = SN_PREFIX_PTR(node);
    free_node(mrx, pfx, HDR_NSZ(pfx->hdr));
}

void
mrx_ptrpfx_sn_free_restore(mrx_base_t *mrx,
                           union mrx_node *node);

static inline void
ptrpfx_sn_free_restore_if_necessary(mrx_base_t *mrx,
                                    union mrx_node *node)
{
    if (HDR_IS_SHORT_PTR(node->hdr)) {
        return;
    }
    mrx_ptrpfx_sn_free_restore(mrx, node);
}

void
mrx_ptrpfx_convert_to_lp_due_to_branches_(mrx_base_t *mrx,
                                          union mrx_node *node,
                                          union mrx_node *branches[],
                                          uint8_t br_len);
static inline void
ptrpfx_convert_to_lp_if_necessary_due_to_branch(mrx_base_t *mrx,
                                                union mrx_node *parent, // must be short ptrnode
                                                union mrx_node *branch) // the added branch that may cause conversion
{
    if (PTR_PREFIX_MATCHES(parent, branch)) {
        return;
    }
    mrx_ptrpfx_convert_to_lp_due_to_branches_(mrx, parent, &branch, 1);
}

static inline void
ptrpfx_convert_to_lp_if_necessary_due_to_branches(mrx_base_t *mrx,
                                                  union mrx_node *node, // must be short ptr node
                                                  union mrx_node *branches[], // the added branches that may cause conversion
                                                  const uint8_t br_len)
{
    uint8_t i;
    for (i = 0; i < br_len; i++) {
        if (!PTR_PREFIX_MATCHES(node, branches[i])) {
            goto slowpath;
        }
    }
    return;
slowpath:
    mrx_ptrpfx_convert_to_lp_due_to_branches_(mrx, node, branches, br_len);
}

void
mrx_ptrpfx_sn_move_branch_ptrs_slowpath_(union mrx_node *node,
                                         uint8_t new_px_len,
                                         uint8_t old_px_len,
                                         uint8_t br_len);
static inline void
ptrpfx_sn_move_branch_ptrs(union mrx_node *node,
                           const uint8_t new_px_len,
                           const uint8_t old_px_len,
                           const uint8_t br_len)
{
    if (HDR_IS_SHORT_PTR(node->hdr)) {
    fastpath:
        memmove(SP_ALIGN(&node->sn.octets[new_px_len + br_len]),
                SP_ALIGN(&node->sn.octets[old_px_len + br_len]),
                br_len * sizeof(mrx_sp_t));
        return;
    }
    const uint8_t longest_px_len = (new_px_len > old_px_len) ? new_px_len : old_px_len;
    const uintptr_t end = (uintptr_t)SP_ALIGN(&node->sn.octets[longest_px_len + br_len]) + br_len * sizeof(mrx_sp_t);
    const uintptr_t pfx = (uintptr_t)&SN_PREFIX_PTR(node);
    if (end <= pfx) {
        goto fastpath;
    }
    mrx_ptrpfx_sn_move_branch_ptrs_slowpath_(node, new_px_len, old_px_len, br_len);
}

void
mrx_ptrpfx_sn_copy_branch_ptrs_and_value_slowpath_(mrx_base_t *mrx,
                                                   union mrx_node *target,
                                                   union mrx_node *source,
                                                   uint8_t t_px_len,
                                                   uint8_t s_px_len,
                                                   uint8_t br_len);
static inline void
ptrpfx_sn_copy_branch_ptrs_and_value(mrx_base_t *mrx,
                                     union mrx_node *target, // must be sptr
                                     union mrx_node *source,
                                     const uint8_t t_px_len,
                                     const uint8_t s_px_len,
                                     const uint8_t br_len)
{
    const uint8_t s_nsz = HDR_NSZ(source->hdr);
    const uint8_t t_nsz = HDR_NSZ(target->hdr);
    if (HDR_IS_SHORT_PTR(source->hdr) && PTR_PREFIX_MATCHES(target, source)) {
        mrx_sp_t *t_brp = SP_ALIGN(&target->sn.octets[t_px_len + br_len]);
        mrx_sp_t *s_brp = SP_ALIGN(&source->sn.octets[s_px_len + br_len]);
        memcpy(t_brp, s_brp, br_len * sizeof(mrx_sp_t));
        if (HDR_HAS_VALUE(source->hdr)) {
            HDR_SET_HAS_VALUE(target->hdr);
            *VALUE_REF(target, t_nsz) = *VALUE_REF(source, s_nsz);
        }
        return;
    }
    mrx_ptrpfx_sn_copy_branch_ptrs_and_value_slowpath_(mrx, target, source, t_px_len, s_px_len, br_len);
}

void
mrx_free_nx_node_slowpath_(mrx_base_t *mrx,
                           union mrx_next_block *nx,
                           bool keep_first);
static inline void
free_nx_node(mrx_base_t *mrx,
             union mrx_next_block *nx)
{
    if (NEXT_BLOCK_IS_SHORT_PTR(nx)) {
        const uint8_t nsz = NEXT_BLOCK_HDR(nx);
        free_node(mrx, nx, nsz);
        return;
    }
    mrx_free_nx_node_slowpath_(mrx, nx, 0);
}

void
mrx_mask_node_try_move_to_local_block_(mrx_base_t *mrx,
                                       union mrx_node *node);

void
mrx_ptrpfx_mn_insert_nx_ptr_slowpath_(mrx_base_t *mrx,
                                      union mrx_node *node,
                                      union mrx_next_block *nx,
                                      uint8_t br,
                                      int is_replacement);
static inline void
ptrpfx_mn_insert_nx_ptr(mrx_base_t *mrx,
                        union mrx_node *node,
                        union mrx_next_block *nx,
                        const unsigned br,
                        const bool is_replacement)
{
    if (HDR_IS_SHORT_PTR(node->hdr) && PTR_PREFIX_MATCHES(node, nx)) {
        if (is_replacement) {
            if (node->mn.next[br >> 5u] == SP(node->mn.local)) {
                HDR_MN_SET_LOCAL_USED(node->mn.hdr, false);
                node->mn.next[br >> 5u] = SP(nx);
                mrx_mask_node_try_move_to_local_block_(mrx, node);
                return;
            }
            free_nx_node(mrx, EP(node->mn.next[br >> 5u], node));
        }
        node->mn.next[br >> 5u] = SP(nx);
        return;
    }
    mrx_ptrpfx_mn_insert_nx_ptr_slowpath_(mrx, node, nx, br, is_replacement);
}

union mrx_next_block *
mrx_alloc_nx_node_lp_(mrx_base_t *mrx,
                      uint8_t capacity);

union mrx_next_block *
mrx_nx_lp_to_sp_(mrx_base_t *mrx,
                 union mrx_next_block *nx,
                 uint8_t bc);

#define MC_PREFIX nl
#define MC_VALUE_T union mrx_node *
#include <mv_tmpl.h>

void
mrx_traverse_children_(mrx_base_t *mrx,
                       nl_t *node_list,
                       union mrx_node *node,
                       bool do_free_traversed);

struct mrx_debug_allocator_stats {
    size_t freelist_size;
    size_t unused_superblock_size;
};

void
mrx_alloc_debug_stats(mrx_base_t *mrx,
                      struct mrx_debug_allocator_stats *stats);

void
mrx_debug_print(mrx_base_t *mrx);

void
mrx_debug_sanity_check_str2ref(mrx_base_t *mrx);

void
mrx_debug_sanity_check_int2ref(mrx_base_t *mrx);

void
mrx_debug_sanity_check_node(union mrx_node *node);

void
mrx_debug_sanity_check_node_with_children(union mrx_node *node);

void
mrx_debug_memory_stats(mrx_base_t *mrx,
                       size_t fixed_key_size);

#endif
