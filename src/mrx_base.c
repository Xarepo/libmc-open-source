/*
 * Copyright (c) 2013, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*

  Design notes:

   - Radix tree with 8 bit stride.
   - Speed through loading few cache lines, rather than few instructions.
   - Small aligned nodes to minimize cache line loading.
   - Smaller nodes for shorter prefixes and/or branches.
   - A special "mask" node type for large branch counts.
   - Supports generic keys, although null-terminated strings is the most
     probable use case.
   - About prefix placement:
      - For any known set of keys, one could calculate an optimal node layout
        with optimal prefix splitting etc, but as we don't know in advance which
        keys are going to inserted it's not possible to make optimal.
      - The overall strategy is to put as much of the prefixes as possible
        early in the tree, to minimize traversing in find.
      - Mask nodes have no prefix for simplicity, as the maximum length
        would be only 1 anyway.
   - There is no parent link in the nodes:
      - Trees often have a link to its parent to be able to backtrack, this
        is however omitted here.
      - Drawback: the iterator can then not just be a pointer to a node, it
        must be a structure containing the traversed path.
      - Advantages: no costly parent link updates of children (when node size
        changes), more space efficient.
      - For mrx_key(it) function there is an advantage to have a stateful
        iterator anyway, so the sacrifice is not too large. The branch index
        at each level also needs to be stored, so we could not only have a
        pointer anyway.
   - Value limited to uintptr_t size:
      - Code greatly simplified by only allowing uintptr_t value size.
      - As nodes can be resized and moved (unlike in a red-black tree for
        example) there is limited value of having larger values directly in the
        node anyway (as pointers to them cannot be returned).
   - Mixed insert/erase loads:
      - Some parts in the tree has a hysteresis on insert/erase to reduce memory
        management overhead. This is a tradeoff of course, since it contributes
        to some internal fragmentation.
   - There is no maintained max depth value
      - Not feasible as we would need to keep track of each sub-tree
      - Key max length is stored instead, with the drawback that it cannot be
        shortened when the tree is reduced (as it would require to scan through
        the tree).
   - About node format:
      - 128 byte aligned nodes, sub-allocated into 8 - 64 with buddy allocator.
      - In compact mode normal malloc() is used, and it is assumed that it
        guarantees 2 * sizeof(void *) alignment.
      - 128 max size chosen as a cacheline / prefix space tradeoff.
      - Scan nodes are 8, 16, 32, 64 or 128 bytes
      - 8 byte nodes not used on 64 bit platforms (little loss, and lot of
        simplification).
      - Mask nodes are 128 bytes with links to link blocks, used when
        branches exceed maximum possible for scan nodes (~25). They can
        store the full 256 branches.
      - Each node has a 16 bit flag field. See description of it further down.
      - Scan node layout: flags + prefix octets + branch octes + pad +
        branch pointers + possibly a value (always last).
      - Mask node layout:
         - 16 bit flags
         - 1 byte branch count (where 0 means 0 or 256)
         - 1 byte bitmask which specifies which 32 bit blocks of the 256 bit
           mask that have set bits.
         - 1 byte pad (or long pointer count)
         - 3 byte pad, which makes the following bitmask 8 byte aligned.
         - 32x8 = 256 bits branching, bit set if branch.
         - 8 pointers to "next blocks" with the actual node pointers.
            - Next blocks are smaller if fewer pointers are stored.
            - Pointers are packed in order, so if bits 3, 5, 7 is set in a
              32 bit block, and branch for bit 7 should be followed it is the
              third pointer in the next block.
            - In 64 long pointer case the blocks may be chained, chained blocks
              while a bit messy to maintain it allows using the same allocator
              all over.
         - Leftover space in the node is used as a "local next block", fits
           about 12 pointers. The code strives to use this block when possible
           as it reduces number of cache line accesses.
         - 64 bit long pointer mode store high 32 bits of next block pointers in
           the local storage block instead (if necessary).
   - Pointer prefix bit for scan nodes:
      - On 64 bit systems the likelihood is very high that pointers to child
        nodes have the same 32 bit prefix as the current node, so we store
        pointers as 32 bits and just expand with the same prefix. To handle
        the exception when the prefix differs the pointer prefix bit is set
        in the flag field and indicates that alternate prefixes is required.
      - If the bit is set the value pointer is present but is instead a
        pointer to a pointer prefix node (which will contain the real value
        pointer if value is present too)
      - If there is no value, the prefix node can contain two short pointer
        branch pointers in the value position. The only time there is waste is
        if there is only one branch pointer, no value and the node is currently
        in short pointer mode, then 4 bytes is wasted to make sure the node
        can be converted to long pointer mode if required later.
      - The pointer prefix node contains the prefixes for all pointers in the
        current node (plus the value if any, or 1 or 2 branch pointers if that
        is used in place of the value).
      - Pointer prefix bit is never set on 32 bit systems, and thus much of
        the code goes inactive.
   - Other notes:
      - Big endian support not implemented.
         - The code encodes first byte header bits to the low unused bits of a
           pointer, and that will only work for little endian, as the first byte
           will contain the LSBs. For big endian the pointer needs to be stored
           shifted down so the unused bits is located in the MSBs. This is
           possible to do and quite easy to implement, but at the time of
           writing the low popularity of big endian systems and scarcity of big
           endian toolchains has lead to omitting big endian support.

________________________________________________________________________________
Node layout examples:

   HH: the 16 bit field of flags
    _: pad octet (often left uninitialized for speed)
    p: prefix octet
    b: branch octet
 [vp]: value pointer (which is 8 bytes on 64 bit systems!)
 [cp]: child node pointer
 [np]: next block pointer

 8 byte nodes (not used on 64 bit):
    4    8
 HH__ [vp]
 HHp_ [vp]
 HHpp [vp]
 HHpb [cp]
 HHb_ [cp]

 16 byte nodes:
         8        16
 HHbb [cp] [cp] ____
 HHbb [cp] [cp] [vp]
 HHpp pppp pppp [vp]
 HHpp pppb [cp] [vp]
 HHpp b___ [cp] [vp]
 HHpb [cp] ____ [vp]
 HHb_ [cp] ____ [vp]
 HHpp b___ [cp] ____
 HHpp pppp pppb [cp]

 32 byte nodes:
         8        16        24        32
 HHpp pppp pppp bbbb [cp] [cp] [cp] [cp]
 HHbb bbbb [cp] [cp] [cp] [cp] [cp] [cp]
 HHpp pppp pppp pppp pppp pppp pppp [vp]
 HHpp pppp pppp pppp pppp pppb [cp] [vp]
 HHpp pppp pppp b___ [cp] ____ ____ [vp]

 64 byte nodes:
         8        16        24        32        40        48        56        64
 HHbb bbbb bbbb bb__ [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp]
 HHpp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp [vp]
 HHpp pppp bbbb bbbb [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp] ____ ____ ____ ____
 HHpp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppb [cp] [val..ue]
 HHpp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppb [cp]

 128 byte nodes:
 HHpp pppp bbbb bbbb bbbb bbbb bbbb bbbb [cp] x 24
 HHbb bbbb bbbb bbbb bbbb bbbb bbb_ [cp] x 25
 HHbb bbbb bbbb bbbb bbbb bbbb bbb_ [cp] x 24 [vp] // 32 bit value
 HHbb bbbb bbbb bbbb bbbb bbbb bb__ [cp] x 23 [val..ue] // 64 bit value
 HHpp pppp x 29 pppb [cp] (long prefix, continues in the single "child" node)
 HHpp pppp x 30 [vp]

 128 byte mask nodes:
         8        16        24        32        40        48        56        64
 HHCU ____ BBBB BBBB BBBB BBBB BBBB BBBB BBBB BBBB [np] [np] [np] [np] [np] [np]
        72        80        88        96       104       112       120       128
 [np] [np] [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp] [cp] [vp]


 128 byte mask node with long pointer next nodes
         8        16        24        32        40        48        56        64
 HHCU L___ BBBB BBBB BBBB BBBB BBBB BBBB BBBB BBBB [np] [np] [np] [np] [np] [np]
        72        80        88        96       104       112       120       128
 [np] [np] [nh] [nh] [nh] [nh] [nh] [nh] [nh] [nh] ____ ____ ____ ____ [val..ue]
 C: total branch count (ie set bits it bitmask), where 0 means 256 (or zero)
 U: 8 bit bitmask of which 32 bit blocks that has non-zero branch count
 L: how many pointers that require long pointers

________________________________________________________________________________
 next block node:

 next node 3 bit LSB first byte (buddyalloc freebit is MSB for node sizes < 128)
 000: 8
 001: 16
 010: 32
 011: 64
 100: 128
 101: 128 lp

 32 byte node no prefix
 [cp] [cp] [cp] [cp]

 64 byte long ptr chained (always 128 byte block)
 HH__ [ct] [no...de]   x 14   [ne...xt]
   ct: how many pointers that require long pointers
   next block: same shape, but no count

________________________________________________________________________________
 16 bit flag field:

 Bit tricks with overloading to fit 5 bit branch length and buddyalloc freebit.

 <7 bits: prefix length><1 bit: has value>
 <4 bits: branch length><1 bit: long pointers><3 bits node size>

 node size: 000: 8
            001: 16
            010: 32
            011: 64
            100: 128 - LSB is the MSB for 5 bit branch length
            101: 128 - LSB is the MSB for 5 bit branch length
            111: 128 - mask node
 node size MSB is freebit for buddyalloc (always 0) for node sizes < 128

 For mask nodes: branch length is instead a flag (0 or 1) and specifies if
 local storage is used.

________________________________________________________________________________
*/
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <alloca.h>
#endif
#include <assert.h>
#include <ctype.h>

#include <mrx_base_int.h>
#include <mrx_scan.h>

#define debug_print(...)
#define debug_check_node(...)
//#define debug_print(...) fprintf(stderr, __VA_ARGS__)
//#define debug_check_node(...) mrx_debug_sanity_check_node(__VA_ARGS__)

#if MRX_TEST_ALLOCATOR - 0 != 0
struct mrx_test_allocator mrx_test_allocator_ = { NULL, NULL };
#endif

#define find_branch glob.find_branch_fun
#define prefix_find_first_diff glob.prefix_find_first_diff_fun
#define find_new_branch_pos glob.find_new_branch_pos_fun
#define prefix_differs(a,b,c) (prefix_find_first_diff(a,b,c) != (c))

static struct {
    uint8_t (*find_branch_fun)(uint8_t, const uint8_t *, uint8_t);
    uint8_t (*find_new_branch_pos_fun)(uint8_t, const uint8_t *, uint8_t);
    uint8_t (*prefix_find_first_diff_fun)(const uint8_t *, const uint8_t *, uint8_t);
} glob;

// set link to (new) child in parent node
static inline void
set_link_in_parent(mrx_base_t *mrx,
                   struct mrx_iterator_path_element path[],
                   int level,
                   union mrx_node *child)
{
    if (level < 0) {
        mrx->root = child;
        return;
    }
    union mrx_node *parent = path[level].node;
    if (IS_SCAN_NODE(HDR_NSZ(parent->hdr))) {
        if (HDR_IS_SHORT_PTR(parent->hdr) && PTR_PREFIX_MATCHES(parent, child)) {
            const uint8_t px_len = HDR_PX_LEN(parent->hdr);
            const uint8_t br_len = HDR_BR_LEN(parent->hdr);
            mrx_sp_t *brp = SP_ALIGN(&parent->sn.octets[px_len+br_len]);
            brp[path[level].br_pos] = SP(child);
            return;
        }
    } else {
        uint32_t b = path[level].br;
        union mrx_next_block *nx = mask_node_get_next_block(parent, b);
        if (NEXT_BLOCK_IS_SHORT_PTR(nx) && PTR_PREFIX_MATCHES(nx, child)) {
            uint32_t bm = parent->mn.bitmask.u32[b >> 5u];
            b = 1u << (b & 0x1Fu);
            parent->mn.branch_count += ((bm & b) == 0); // if new branch, add 1
            bm &= b - 1;
            b = bit32_count(bm);
            // if b is zero we need to maintain next block header
            nx->sp[b] &= ~SP_MASK;
            nx->sp[b] |= SP(child);
            return;
        }
    }
    debug_print("pre set link %p %d\n", child, path[level].br);
    mrx_set_link_in_parent_slowpath_(mrx, path, level, child);
    debug_print("post set link\n");
    debug_check_node(parent);
}

// Input requirement: no value and 1 branch in parent, level > 0
static void
prefix_lengthen_in_parent(mrx_base_t *mrx,
                          struct mrx_iterator_path_element path[],
                          const int level,
                          const uint8_t nsz,
                          const uint8_t new_nsz,
                          const uint8_t extra_px[],
                          const uint8_t extra_px_len)
{
    assert(level > 0);

    union mrx_node *node = path[level].node;
    union mrx_node *parent = path[level - 1].node;

    debug_print("prefix lengthen in parent with %d (nsz %d new_nsz %d)\n", extra_px_len, nsz, new_nsz);
    debug_check_node(parent);

    const uint8_t px_len = HDR_PX_LEN(parent->hdr);
    if (new_nsz <= nsz) {
        memcpy(&parent->sn.octets[px_len + 1], extra_px, extra_px_len);
        HDR_SET_PX_LEN(parent->hdr, px_len + extra_px_len);
        mrx_sp_t * const brp = SP_ALIGN(&parent->sn.octets[px_len + extra_px_len + 1]);
        scan_node_set_child(parent, brp, 0, node);

        debug_check_node(parent);
        return;
    }

    // new parent required
    union mrx_node *newparent = alloc_node(mrx, new_nsz);
    HDR_SET(newparent->hdr, new_nsz, 1, false, px_len + extra_px_len, false);
    memcpy(newparent->sn.octets, parent->sn.octets, px_len + 1);
    memcpy(&newparent->sn.octets[px_len + 1], extra_px, extra_px_len);
    mrx_sp_t * const brp = SP_ALIGN(&newparent->sn.octets[px_len + extra_px_len + 1]);
    brp[0] = SP(node);
    ptrpfx_convert_to_lp_if_necessary_due_to_branch(mrx, newparent, node);
    path[level - 1].node = newparent;
    path[level - 1].br_pos = 0;
    path[level - 1].br = newparent->sn.octets[px_len + extra_px_len];
    set_link_in_parent(mrx, path, level - 2, newparent);
    ptrpfx_sn_free(mrx, parent);
    free_node(mrx, parent, nsz);

    debug_check_node(newparent);
}

static void
prefix_shorten(mrx_base_t *mrx,
               struct mrx_iterator_path_element path[],
               const int level,
               const uint8_t strip_px_len,
               const bool keep_size)
{
    debug_print("prefix shorten with %d\n", strip_px_len);

    union mrx_node *node = path[level].node;
    const uint8_t px_len = HDR_PX_LEN(node->hdr);
    const uint8_t br_len = HDR_BR_LEN(node->hdr);
    assert(strip_px_len <= px_len);
    const uint8_t new_px_len = px_len - strip_px_len;

    debug_check_node(node);

    if (keep_size) {
        // keep size even if the node could be made smaller
        goto prefix_shorten_inplace;
    }

    const uint8_t new_nsz = scan_node_min_size_nsz(new_px_len, br_len, HDR_HAS_VALUE(node->hdr));

    if (new_nsz == HDR_SN_FLATTENED_NSZ(node->hdr)) {
    prefix_shorten_inplace:
        HDR_SET_PX_LEN(node->hdr, new_px_len);
        memmove(node->sn.octets, &node->sn.octets[strip_px_len], new_px_len);
        memmove(&node->sn.octets[new_px_len], &node->sn.octets[px_len], br_len);
        ptrpfx_sn_move_branch_ptrs(node, new_px_len, px_len, br_len);

        debug_check_node(node);
        return;
    }

    // different size, new node required
    union mrx_node *newnode = alloc_node(mrx, new_nsz);
    HDR_SET(newnode->hdr, new_nsz, br_len, false, new_px_len, false);
    memcpy(newnode->sn.octets, &node->sn.octets[strip_px_len], new_px_len);
    memcpy(&newnode->sn.octets[new_px_len], &node->sn.octets[px_len], br_len);
    ptrpfx_sn_copy_branch_ptrs_and_value(mrx, newnode, node, new_px_len, px_len, br_len);
    path[level].node = newnode;
    set_link_in_parent(mrx, path, level - 1, newnode);
    ptrpfx_sn_free(mrx, node);
    free_node(mrx, node, HDR_NSZ(node->hdr));

    debug_check_node(newnode);
}

// Find the least prefix bytes to move up to fit an additional branch
// Input requirement: node must be 128 size, and br_len must not be maxed out.
static inline uint8_t
minimum_move_px_len_for_new_branch(const union mrx_node *node,
                                   const uint8_t br[],
                                   const uint8_t br_len)
{
    const uintptr_t end = (uintptr_t)NODE_END_CONST_REF(node, 4) - HDR_HAS_VALUE(node->hdr) * sizeof(void *);
    const uint8_t *br_end = &br[br_len];
    const uintptr_t brp_end = (uintptr_t)&SP_ALIGN(br_end)[br_len];
    const uint8_t px_len = HDR_PX_LEN(node->hdr);
    const uint8_t min_px_len =
        (brp_end == end) * sizeof(mrx_sp_t) +
        (((uintptr_t)br_end & 3u) == 0); // move one extra if needed to avoid alignment step-up of branch array

    // optimization: avoid only one prefix byte left as it then cannot
    // convert to mask node without having to do an extra move
    return (min_px_len > px_len || px_len - min_px_len == 1) ? px_len : min_px_len;
}

static void
prefix_move_up(mrx_base_t *mrx,
               struct mrx_iterator_path_element path[],
               int *level,
               const uint8_t min_px_len)
{
    union mrx_node *node = path[*level].node;
    debug_print("move prefix up (min_px_len %d, px_len %d)\n", min_px_len, HDR_PX_LEN(node->hdr));
    debug_check_node(node);

    const uint8_t px_len = HDR_PX_LEN(node->hdr);
    if (px_len == 0) {
        return; // no work required
    }
    assert(min_px_len <= px_len);

    // Can we move up to current parent? For that to work the parent must just contain a prefix and
    // single branch to this node, and no value. In other words, the parent must only be a prefix
    // container and not do any real branching.
#if ARCH_SIZEOF_PTR == 4
    // occupied space: header 2, one branch octet 1, short branch pointer 4.
    const unsigned occupied_space = 2 + 1 + 4;
#elif ARCH_SIZEOF_PTR == 8
    // occupied space: header 2, one branch octet 1, ptrpfx pointer 8. The ptrpfx pointer is only
    // used if the node is in long pointer mode, but space must be kept anyway to allow for conversion.
    const unsigned occupied_space = 2 + 1 + 8;
#endif
    uint8_t move_px_len;
    union mrx_node *parent = (*level > 0) ? path[*level - 1].node : NULL;
    if (parent != NULL &&
        !HDR_HAS_VALUE(parent->hdr) &&
        IS_SCAN_NODE(HDR_NSZ(parent->hdr)) &&
        HDR_BR_LEN(parent->hdr) == 1 &&
        HDR_PX_LEN(parent->hdr) + min_px_len <= 128u - occupied_space)
    {
        debug_print("use parent\n");
        debug_check_node(parent);
        const uint8_t pr_nsz = HDR_SN_FLATTENED_NSZ(parent->hdr);

        // prefer to keep parent size if possible
        uint8_t new_nsz = pr_nsz;
        const uint8_t pr_px_len = HDR_PX_LEN(parent->hdr);
        const uint8_t pr_occupied_space = occupied_space + pr_px_len;
        move_px_len = (8u << pr_nsz) - pr_occupied_space;
        if (move_px_len < min_px_len) {
            // need to upsize, then try to move out everything
            const unsigned tot_size_p2 = px_len + pr_occupied_space < 128 ? next_power_of_two_for_8bit(px_len + pr_occupied_space) : 128;
            move_px_len = tot_size_p2 - pr_occupied_space;
            new_nsz = NODE_SIZE_P2_TO_NSZ(tot_size_p2);
        }
        if (move_px_len > px_len) {
            move_px_len = px_len;
        }
        debug_print("px_len %d; pr_px_len %d; move_px_len %d; min_px_len %d\n", px_len, pr_px_len, move_px_len, min_px_len);
        assert(move_px_len >= min_px_len);

        prefix_lengthen_in_parent(mrx, path, *level, pr_nsz, new_nsz, node->sn.octets, move_px_len);
    } else {
        // here, when a new parent is created, we move out as much as possible as we will create a new node anyway.
        // reduce one from occupied space as we create a new branch octet.
        const unsigned tot_size_p2 = next_power_of_two_for_8bit(px_len + occupied_space - 1);
        move_px_len = tot_size_p2 - occupied_space + 1;
        if (move_px_len > px_len) {
            move_px_len = px_len;
        }
        const uint8_t new_nsz = NODE_SIZE_P2_TO_NSZ(tot_size_p2);

        // New parent required
        debug_print(" creating parent %d (%d)\n", move_px_len, new_nsz);
        union mrx_node *newnode = alloc_node(mrx, new_nsz);
        HDR_SET(newnode->hdr, new_nsz, 1, false, move_px_len - 1, false);
        memcpy(newnode->sn.octets, node->sn.octets, move_px_len);
        mrx_sp_t *brp = SP_ALIGN(&newnode->sn.octets[move_px_len]);
        brp[0] = SP(node);
        ptrpfx_convert_to_lp_if_necessary_due_to_branch(mrx, newnode, node);
        debug_check_node(newnode);
        set_link_in_parent(mrx, path, *level - 1, newnode);
        path[*level].node = newnode;
        path[*level].br_pos = 0;
        path[*level].br = node->sn.octets[move_px_len-1];
        (*level)++;
        path[*level].node = node;
    }

    // keep_size flag set to true as the reason we shorten the prefix is to free up space
    // to fill the node with other stuff (branch or value)
    prefix_shorten(mrx, path, *level, move_px_len, true);

    debug_check_node(node = path[*level].node);
    debug_print("move up prefix end\n");
}

static void
enlarge_node(mrx_base_t *mrx,
             struct mrx_iterator_path_element path[],
             int level)
{
    union mrx_node *node = path[level].node;
    debug_check_node(node);
    union mrx_node *newnode;
    const uint8_t nsz = HDR_NSZ(node->hdr);

    newnode = alloc_node(mrx, nsz + 1u);
    HDR_SET(newnode->hdr, nsz + 1u, 0, false, 0, false);
    path[level].node = newnode;
    ptrpfx_sn_copy_to_short(newnode, node, nsz);
    HDR_SET_NSZ(newnode->hdr, nsz + 1u);
    if (HDR_HAS_VALUE(newnode->hdr)) {
        *VALUE_REF(newnode, nsz + 1u) = *VALUE_REF(newnode, nsz);
    }
    if (!HDR_IS_SHORT_PTR(node->hdr) || !PTR_PREFIX_MATCHES(node, newnode)) {
        const uint8_t br_len = HDR_BR_LEN(node->hdr);
        const uint8_t px_len = HDR_PX_LEN(node->hdr);
        const uint8_t *br = &node->sn.octets[px_len];
        const mrx_sp_t *brp = SP_ALIGN(&br[br_len]);
        union mrx_node *branches[br_len];
        for (uint8_t i = 0; i < br_len; i++) {
            branches[i] = scan_node_get_child1(node, brp, i);
        }
        ptrpfx_convert_to_lp_if_necessary_due_to_branches(mrx, newnode, branches, br_len);
        ptrpfx_sn_free(mrx, node);
    }
    set_link_in_parent(mrx, path, level - 1, newnode);
    free_node(mrx, node, nsz);
    debug_check_node(newnode);
    debug_print("enlarge node out %p\n", newnode);
}

// Input requirement: scan node, only one branch, no value.
static void
scan_node_child_merge(mrx_base_t *mrx,
                      struct mrx_iterator_path_element path[],
                      int level)
{
    union mrx_node *node = path[level].node;
    const uint8_t px_len = HDR_PX_LEN(node->hdr);
    union mrx_node *child = scan_node_get_child(node, &node->sn.octets[px_len], 1, 0);
    debug_print("scan node child merge\n");
    debug_check_node(node);
    debug_check_node(child);

    uint8_t ch_nsz = HDR_NSZ(child->hdr);
    if (IS_MASK_NODE(ch_nsz)) {
        // Merge impossible as mask nodes cannot hold a prefix
        debug_print("merge impossible (mask node child)\n");
        return;
    }

    const uint8_t ch_px_len = HDR_PX_LEN(child->hdr);
    const uint8_t ch_br_len = HDR_BR_LEN(child->hdr);
    const uint8_t new_px_len = px_len + 1 + ch_px_len;
    const uint8_t new_nsz = scan_node_min_size_nsz(new_px_len, ch_br_len, HDR_HAS_VALUE(child->hdr));
    if (new_nsz > 4) {
        // Merge impossible
        debug_print("merge impossible (too large size)\n");
        return;
    }
    const uint8_t nsz = HDR_SN_FLATTENED_NSZ(node->hdr);
    ptrpfx_sn_free_restore_if_necessary(mrx, node);
    if (nsz == new_nsz) {
        // Merge to current
        HDR_SET_PX_LEN(node->hdr, new_px_len);
        HDR_SET_NSZ_BR_LEN(node->hdr, nsz, ch_br_len);
        memcpy(&node->sn.octets[px_len + 1], child->sn.octets, ch_px_len + ch_br_len);
        ptrpfx_sn_copy_branch_ptrs_and_value(mrx, node, child, new_px_len, ch_px_len, ch_br_len);
        ptrpfx_sn_free(mrx, child);
        free_node(mrx, child, ch_nsz);

        debug_print("merge to current\n");
        debug_check_node(node);
        return;
    }

    FLATTEN_SN_NSZ(ch_nsz);
    if (ch_nsz == new_nsz) {
        // Merge to child
        HDR_SET_PX_LEN(child->hdr, new_px_len);
        ptrpfx_sn_move_branch_ptrs(child, new_px_len, ch_px_len, ch_br_len);
        memmove(&child->sn.octets[px_len + 1], child->sn.octets, ch_px_len + ch_br_len);
        memcpy(child->sn.octets, node->sn.octets, px_len + 1);
        ptrpfx_sn_free(mrx, node);
        free_node(mrx, node, nsz);
        set_link_in_parent(mrx, path, level - 1, child);

        debug_print("merge to child\n");
        debug_check_node(child);
        return;
    }

    // Merge to new
    union mrx_node *newnode = alloc_node(mrx, new_nsz);
    HDR_SET(newnode->hdr, new_nsz, ch_br_len, false, new_px_len, false);
    memcpy(newnode->sn.octets, node->sn.octets, px_len + 1);
    memcpy(&newnode->sn.octets[px_len + 1], child->sn.octets, ch_px_len + ch_br_len);
    ptrpfx_sn_copy_branch_ptrs_and_value(mrx, newnode, child, new_px_len, ch_px_len, ch_br_len);
    set_link_in_parent(mrx, path, level - 1, newnode);
    ptrpfx_sn_free(mrx, node);
    ptrpfx_sn_free(mrx, child);
    free_node(mrx, node, nsz);
    free_node(mrx, child, ch_nsz);

    debug_print("merge to new\n");
    debug_check_node(newnode);
}

static void
sn_erase_branch(mrx_base_t *mrx,
                union mrx_node *node,
                uint8_t br_pos,
                uint8_t br[],
                uint8_t new_br_len)
{
    const uint8_t old_br_len = new_br_len + 1;
    mrx_sp_t *brp = SP_ALIGN(&br[new_br_len]);
    mrx_sp_t *old_brp = SP_ALIGN(&br[old_br_len]);
    struct mrx_ptrpfx_node *pfx = NULL;
    bool rewrite_pfx_ptr = false;

    HDR_SET_BR_LEN(node->hdr, new_br_len);
    if (br_pos < new_br_len) {
        memmove(&br[br_pos], &br[br_pos+1], (new_br_len - br_pos));
    }
    if (!HDR_IS_SHORT_PTR(node->hdr)) {
        pfx = SN_PREFIX_PTR(node);
        if (!PTR_PREFIX_MATCHES(scan_node_get_child(node, br, old_br_len, br_pos), node)) {
            pfx->lp_count--;
            if (pfx->lp_count == 0) {
                mrx_ptrpfx_sn_free_restore(mrx, node);
                goto short_ptr;
            }
        }
        if (br_pos < new_br_len) {
            memmove(&pfx->hp[br_pos], &pfx->hp[br_pos+1], (new_br_len - br_pos) * sizeof(mrx_sp_t));
        }
        if ((uintptr_t)&old_brp[old_br_len] > (uintptr_t)&SN_PREFIX_PTR(node)) {
            // write back pointers so memmove() further down will work, and instruct to restore after
            rewrite_pfx_ptr = true;
            if ((uintptr_t)&old_brp[old_br_len-1] > (uintptr_t)&SN_PREFIX_PTR(node)) {
                old_brp[old_br_len - 2] = pfx->orig.sp[0];
                old_brp[old_br_len - 1] = pfx->orig.sp[1];
            } else {
                old_brp[old_br_len - 1] = pfx->orig.sp[0];
            }
        }
    }

short_ptr:
    if (brp == old_brp) {
        memmove(&brp[br_pos], &brp[br_pos + 1], (new_br_len - br_pos) * sizeof(mrx_sp_t));
    } else {
        memmove(brp, &brp[1], br_pos * sizeof(mrx_sp_t));
        memmove(&brp[br_pos], &brp[br_pos + 2], (new_br_len - br_pos) * sizeof(mrx_sp_t));
    }
    if (rewrite_pfx_ptr) {
        const uint8_t nsz = HDR_NSZ(node->hdr);
        void *orig = *VALUE_REF(node, nsz);
        *VALUE_REF(node, nsz) = (void *)pfx;
        pfx->orig.value = orig;
    }
}

static void
sn_reduce_to_value_only(mrx_base_t *mrx,
                        struct mrx_iterator_path_element path[],
                        int level)
{
    union mrx_node *node = path[level].node;
    const uint8_t px_len = HDR_PX_LEN(node->hdr);
    if (level == 0) {
        // if value only and level 0, this is the only node left, so we can reset max key length
        mrx->max_keylen_n_flags = px_len | (mrx->max_keylen_n_flags & MRX_FLAGS_MASK_);
    }
#if ARCH_SIZEOF_PTR == 4
    if (px_len <= 2) {
        // Nodes are reduced when br_len > 1 so there is only one case left when
        // further reduction is possible, which we handle here
        union mrx_node *newnode = alloc_node(mrx, 0);
        HDR_SET(newnode->hdr, 0, 0, false, px_len, true);
        newnode->sn.octets[0] = node->sn.octets[0];
        newnode->sn.octets[1] = node->sn.octets[1];
        const uint8_t nsz = HDR_NSZ(node->hdr);
        *VALUE_REF(newnode, 0) = *VALUE_REF(node, nsz);
        set_link_in_parent(mrx, path, level - 1, newnode);
        ptrpfx_sn_free(mrx, node); // actually not required in 32 bit mode, kept for consistency
        free_node(mrx, node, nsz);

        debug_print("node reduced to 8\n");
        debug_check_node(newnode);
        return;
    }
#endif
    HDR_SET_BR_LEN(node->hdr, 0);
    ptrpfx_sn_free_restore_if_necessary(mrx, node);
    debug_check_node(node);
}

// Input requirement: if no value, more than one branch, else at least one
// branch - in other words after removal of branch the node is still left.
static void
scan_node_erase_branch(mrx_base_t *mrx,
                       struct mrx_iterator_path_element path[], // may not be valid after return
                       int level)
{
    debug_print("scan node erase branch\n");
    union mrx_node *node = path[level].node;
    debug_check_node(node);

    const uint8_t br_len = HDR_BR_LEN(node->hdr) - 1; // br_len after erase
    const uint8_t px_len = HDR_PX_LEN(node->hdr);
    const uint8_t nsz = HDR_SN_FLATTENED_NSZ(node->hdr);

    if (br_len == 0) {
        // last branch

        assert(HDR_HAS_VALUE(node->hdr));
        if (level < 1) {
            sn_reduce_to_value_only(mrx, path, level);
            return;
        }

        // Test if parent merge is possible
        union mrx_node *parent = path[level - 1].node;
        uint8_t pr_nsz = HDR_NSZ(parent->hdr);
        if (!IS_SCAN_NODE(pr_nsz) || HDR_BR_LEN(parent->hdr) != 1 || HDR_HAS_VALUE(parent->hdr)) {
            sn_reduce_to_value_only(mrx, path, level);
            return;
        }

        const uint8_t pr_px_len = HDR_PX_LEN(parent->hdr);
        const uint8_t new_nsz = scan_node_min_size_nsz(pr_px_len+px_len+1, 0, 1);

        if (new_nsz > 4) {
            sn_reduce_to_value_only(mrx, path, level);
            return;
        }

        // Parent merge is possible here
        FLATTEN_SN_NSZ(pr_nsz);
        if (new_nsz == pr_nsz) {
            // Merge to parent
            ptrpfx_sn_free_restore_if_necessary(mrx, parent);
            HDR_SET_NSZ_BR_LEN(parent->hdr, pr_nsz, 0);
            HDR_SET_PX_LEN(parent->hdr, pr_px_len+px_len+1);
            HDR_SET_HAS_VALUE(parent->hdr);
            memcpy(&parent->sn.octets[pr_px_len + 1], node->sn.octets, px_len);
            *VALUE_REF(parent, pr_nsz) = *mrx_node_value_ref_(node, nsz);
            ptrpfx_sn_free(mrx, node);
            free_node(mrx, node, nsz);

            debug_print("merge to parent\n");
            debug_check_node(parent);
        } else if (new_nsz == nsz) {
            // Merge to current
            ptrpfx_sn_free_restore_if_necessary(mrx, node);
            HDR_SET_NSZ_BR_LEN(node->hdr, nsz, 0);
            HDR_SET_PX_LEN(node->hdr, pr_px_len+px_len+1);
            memmove(&node->sn.octets[pr_px_len + 1], node->sn.octets, px_len);
            memcpy(node->sn.octets, parent->sn.octets, pr_px_len + 1);
            ptrpfx_sn_free(mrx, parent);
            free_node(mrx, parent, pr_nsz);
            set_link_in_parent(mrx, path, level - 2, node);

            debug_print("merge to current\n");
            debug_check_node(node);
        } else {
            // Merge to new
            union mrx_node *newnode = alloc_node(mrx, new_nsz);
            HDR_SET(newnode->hdr, new_nsz, 0, false, pr_px_len + px_len + 1, true);
            memcpy(&newnode->sn.octets[pr_px_len + 1], node->sn.octets, px_len);
            *VALUE_REF(newnode, new_nsz) = *mrx_node_value_ref_(node, nsz);
            memcpy(newnode->sn.octets, parent->sn.octets, pr_px_len + 1);
            ptrpfx_sn_free(mrx, parent);
            ptrpfx_sn_free(mrx, node);
            free_node(mrx, parent, pr_nsz);
            free_node(mrx, node, nsz);
            set_link_in_parent(mrx, path, level - 2, newnode);

            debug_print("merge to new\n");
            debug_check_node(newnode);
        }
        return;
    }

    // More than one branch left
    uint8_t * const br = &node->sn.octets[px_len];
    const uint8_t * const br_end = &br[br_len];
    const uintptr_t brp_end = (uintptr_t)&SP_ALIGN(br_end)[br_len];
    const uintptr_t smaller_brp_end_max = (uintptr_t)NODE_END_REF(node, nsz - 1) - HDR_HAS_VALUE(node->hdr) * sizeof(void *);
    // With larger nodes we have a larger margin before reducing size, a
    // strategy to avoid enlarge/reduce flapping in insert/erase loads.
    if (brp_end + (nsz - 1) * sizeof(mrx_sp_t) > smaller_brp_end_max
#if ARCH_SIZEOF_PTR == 8
        || nsz == 1 // don't make 8 byte nodes!
#endif
        )
    {
        // Keep current node unchanged
        sn_erase_branch(mrx, node, path[level].br_pos, br, br_len);
        debug_print("keep current node\n");
        debug_check_node(node);
        return;
    }

    // Reduce size

    // convert back original node to short pointer if possible
    const int br_pos = path[level].br_pos;
    if (!HDR_IS_SHORT_PTR(node->hdr)) {
        struct mrx_ptrpfx_node *pfx = SN_PREFIX_PTR(node);
        if (!PTR_PREFIX_MATCHES(scan_node_get_child(node, br, br_len + 1, br_pos), node)) {
            if (pfx->lp_count == 1) {
                mrx_ptrpfx_sn_free_restore(mrx, node);
                debug_print("convert back to short pointer\n");
            }
        }
    }

    union mrx_node *newnode = alloc_node(mrx, nsz - 1);
    uint8_t *new_br = &newnode->sn.octets[px_len];
    mrx_sp_t *brp = SP_ALIGN(&node->sn.octets[px_len + br_len + 1]);
    mrx_sp_t *new_brp = SP_ALIGN(&newnode->sn.octets[px_len + br_len]);
    HDR_SET(newnode->hdr, nsz - 1, br_len, false, px_len, HDR_HAS_VALUE(node->hdr));
    if (HDR_IS_SHORT_PTR(node->hdr)) {
        if (!PTR_PREFIX_MATCHES(node, newnode)) {
            // rare case, old is short pointer but new smaller node doesn't match, then it's better to keep the old
            free_node(mrx, newnode, nsz - 1);
            sn_erase_branch(mrx, node, path[level].br_pos, br, br_len);
            debug_print("keep current node 2\n");
            debug_check_node(node);
            return;
        }
        // short to short pointer case, this should be the most common by wide margin
        debug_print("short pointer\n");
        memcpy(newnode->sn.octets, node->sn.octets, px_len + br_pos);
        memcpy(&new_br[br_pos], &br[br_pos + 1], (br_len - br_pos));
        memcpy(new_brp, brp, br_pos * sizeof(mrx_sp_t));
        memcpy(&new_brp[br_pos], &brp[br_pos + 1], (br_len - br_pos) * sizeof(mrx_sp_t));
        if (HDR_HAS_VALUE(node->hdr)) {
            *VALUE_REF(newnode, nsz - 1) = *VALUE_REF(node, nsz);
        }
    } else {
        // long to either short or long pointer
        debug_print("long pointer\n");
        sn_erase_branch(mrx, node, br_pos, br, br_len);
        memcpy(newnode->sn.octets, node->sn.octets, px_len + br_len);
        ptrpfx_sn_copy_branch_ptrs_and_value(mrx, newnode, node, px_len, px_len, br_len);
        ptrpfx_sn_free(mrx, node);
    }
    set_link_in_parent(mrx, path, level - 1, newnode);
    free_node(mrx, node, nsz);
    debug_print("reduce size\n");
    debug_check_node(newnode);
}

// This function won't touch the prefix of target node
static void
mask_node_to_scan_node_branch(mrx_base_t *mrx,
                              uint8_t br_dest[],
                              const uint8_t br_len,
                              union mrx_node *node,
                              union mrx_node *target_node, // no prefix pointer
                              const uint8_t skip_octet)
{
    debug_print("mask node to scan node branch\n");
    const bool do_out_of_place = (target_node == node);
    union mrx_node *bra_long[SCAN_NODE_MAX_BRANCH_COUNT];
    mrx_sp_t bra_[SCAN_NODE_MAX_BRANCH_COUNT];
    uint8_t br_[SCAN_NODE_MAX_BRANCH_COUNT];
    uint8_t *br = do_out_of_place ? br_ : br_dest;
    mrx_sp_t *bra = do_out_of_place ? bra_ : SP_ALIGN(&br_dest[br_len]);

    uint8_t i = 0;
    uint8_t j = 0;
    mrx_sp_t *nxp = NULL;
    union mrx_next_block *cnx = NULL;
    bool long_ptr_required = false;
    int b = -1;
    int8_t oi = -1;
    while ((b = mask_node_next_branch(node, b + 1)) != -1) {
        const int8_t ci = (int8_t)((unsigned)b >> 5u);
        if (ci != oi) {
            if (nxp != NULL && cnx != (union mrx_next_block *)node->mn.local) {
                free_nx_node(mrx, cnx);
            }
            j = 0;
            nxp = &node->mn.next[ci];
            cnx = EP2(*nxp, HDR_IS_SHORT_PTR(node->hdr) ? HP(node) : node->mn.local[ci]);
            oi = ci;
        }
        if (b != skip_octet) {
            union mrx_node *child = next_block_get_child(cnx, j);
            if (!PTR_PREFIX_MATCHES(target_node, child)) {
                long_ptr_required = true;
            }
            bra_long[i] = child;
            bra[i] = SP(child);
            br[i] = (uint8_t)b;
            i++;
        }
        j++;
    }
    if (cnx != (union mrx_next_block *)node->mn.local) {
        free_nx_node(mrx, cnx);
    }
    if (do_out_of_place) {
        memcpy(br_dest, br, br_len);
        memcpy(SP_ALIGN(&br_dest[br_len]), bra, br_len * sizeof(mrx_sp_t));
        HDR_CLR_LONGPTR(target_node->hdr);
    }
    if (long_ptr_required) {
        ptrpfx_convert_to_lp_if_necessary_due_to_branches(mrx, target_node, bra_long, br_len);
    } else {
        HDR_CLR_LONGPTR(target_node->hdr);
    }
}

// Mask node must have at least two branches (which it will have as mask nodes are only
// created when there are many branches)
static void
mask_node_erase_branch(mrx_base_t *mrx,
                       struct mrx_iterator_path_element path[],
                       int level)
{
    const uint8_t br = path[level].br;
    union mrx_node *node = path[level].node;

    debug_print("mask node erase branch in 0x%02X %c\n", br, isprint(br) ? br : ' ');
    assert(node->mn.branch_count != 1);
    debug_check_node(node);

    // we use some margin before converting back to scan node
    // note special case of 0, as branch_count 256 will overflow to 0, ie 0 == 256.
    if (node->mn.branch_count < 20 && node->mn.branch_count != 0) {
        // convert back to scan node
        const uint8_t br_len = node->mn.branch_count - 1;
        const uint8_t skip_octet = br;

        // check if there is a possibility to merge with parent node
        if (level < 1) {
            HDR_SET_NSZ_BR_LEN(node->hdr, 4, br_len);
            mask_node_to_scan_node_branch(mrx, node->sn.octets, br_len, node, node, skip_octet);
            debug_check_node(node);
            return;
        }

        union mrx_node *parent = path[level - 1].node;
        uint8_t pr_nsz = HDR_NSZ(parent->hdr);
        const uint8_t pr_br_len = HDR_BR_LEN(parent->hdr);
        if (!IS_SCAN_NODE(pr_nsz) || pr_br_len != 1 || HDR_HAS_VALUE(parent->hdr)) {
            HDR_SET_NSZ_BR_LEN(node->hdr, 4, br_len);
            mask_node_to_scan_node_branch(mrx, node->sn.octets, br_len, node, node, skip_octet);
            debug_check_node(node);
            return;
        }

        const uint8_t pr_px_len = HDR_PX_LEN(parent->hdr);
        const uint8_t new_nsz = scan_node_min_size_nsz(pr_px_len + pr_br_len, br_len, HDR_HAS_VALUE(node->hdr));

        if (new_nsz > 4) {
            HDR_SET_NSZ_BR_LEN(node->hdr, 4, br_len);
            mask_node_to_scan_node_branch(mrx, node->sn.octets, br_len, node, node, skip_octet);
            debug_check_node(node);
            return;
        }

        // merge with parent
        FLATTEN_SN_NSZ(pr_nsz);
        if (new_nsz == pr_nsz) {
            // Merge to parent
            debug_print("merge to parent\n");
            ptrpfx_sn_free_restore_if_necessary(mrx, parent);
            HDR_SET(parent->hdr, pr_nsz, br_len, false, pr_px_len + 1, false);
            mask_node_to_scan_node_branch(mrx, &parent->sn.octets[pr_px_len+1], br_len, node, parent, skip_octet);
            if (HDR_HAS_VALUE(node->hdr)) {
                HDR_SET_HAS_VALUE(parent->hdr);
                *mrx_node_value_ref_(parent, pr_nsz) = *VALUE_REF(node, 4);
            }
            free_node(mrx, node, 4);
            debug_check_node(parent);
        } else if (new_nsz == 4) {
            // Merge to current (mask) node
            debug_print("merge to current\n");
            mask_node_to_scan_node_branch(mrx, &node->sn.octets[pr_px_len + 1], br_len, node, node, skip_octet);
            memcpy(node->sn.octets, parent->sn.octets, pr_px_len + 1);
            HDR_SET(node->hdr, 4, br_len, !HDR_IS_SHORT_PTR(node->hdr), pr_px_len + 1, HDR_HAS_VALUE(node->hdr));
            ptrpfx_sn_free(mrx, parent);
            free_node(mrx, parent, pr_nsz);
            set_link_in_parent(mrx, path, level - 2, node);
            debug_check_node(node);
        } else {
            // Merge to new
            debug_print("merge to new\n");
            union mrx_node *newnode = alloc_node(mrx, new_nsz);
            HDR_SET(newnode->hdr, new_nsz, br_len, false, pr_px_len + 1, HDR_HAS_VALUE(node->hdr));
            memcpy(newnode->sn.octets, parent->sn.octets, pr_px_len + 1);
            ptrpfx_sn_free(mrx, parent);
            free_node(mrx, parent, pr_nsz);
            mask_node_to_scan_node_branch(mrx, &newnode->sn.octets[pr_px_len + 1], br_len, node, newnode, skip_octet);
            if (HDR_HAS_VALUE(node->hdr)) {
                *mrx_node_value_ref_(newnode, new_nsz) = *VALUE_REF(node, 4);
            }
            free_node(mrx, node, 4);
            set_link_in_parent(mrx, path, level - 2, newnode);
            debug_check_node(newnode);
        }
        return;
    }

    // keep current mask node
    uint32_t b = br;
    union mrx_next_block * const oldnx = mask_node_get_next_block(node, b);
    uint32_t * const bm = &node->mn.bitmask.u32[b >> 5u];

    node->mn.branch_count--;
    b = 1u << (b & 0x1Fu);
    *bm &= ~b;
    if (*bm == 0) {
        // all bits removed in 32 bit range, free nx block
        node->mn.used &= ~(1u << ((unsigned)br >> 5u));
        if (oldnx == (union mrx_next_block *)node->mn.local) {
            HDR_MN_SET_LOCAL_USED(node->mn.hdr, false);
            mrx_mask_node_try_move_to_local_block_(mrx, node);
        } else {
            free_nx_node(mrx, oldnx);
            if (!PTR_PREFIX_MATCHES(oldnx, node)) {
                node->mn.lp_count--;
                if (node->mn.lp_count == 0) {
                    HDR_CLR_LONGPTR(node->mn.hdr);
                    mrx_mask_node_try_move_to_local_block_(mrx, node);
                }
            }
        }
        debug_check_node(node);
        return;
    }

    // at least one branch left in the block after erase
    uint8_t bc = bit32_count(*bm); // total bit count after erased branch
    const uint8_t nb = bit32_count(*bm & (b - 1)); //index of erased branch
    union mrx_next_block *newnx = oldnx; // default is in-place branch erase

    // test if there is a reason not to do in-place erase, and if so re-assign newnx
    if (oldnx != (union mrx_next_block *)node->mn.local) {
        // Use some margin before reducing size of nx block for better
        // performance for mixed insert/erase loads
        if (!HDR_MN_LOCAL_USED(node->mn.hdr) &&
            HDR_IS_SHORT_PTR(node->mn.hdr) &&
            bc <= MASK_NODE_LOCAL_BRANCH_COUNT_FOR_MOVING_BACK &&
            PTR_PREFIX_MATCHES(oldnx, node) &&
            NEXT_BLOCK_IS_SHORT_PTR(oldnx))
        {
            // move to local!
            newnx = (union mrx_next_block *)node->mn.local;
        } else if (NEXT_BLOCK_IS_SHORT_PTR(oldnx)) {
            const uint8_t old_sz = NEXT_BLOCK_HDR(oldnx);
            const uint32_t nsz_to_bc_sizedown_limit =
#if ARCH_SIZEOF_PTR != 8
                // 8 byte nodes not used on 64 bit platforms
                (2u << 4u) | // sizedown branch count for old_nsz 1 (16 to 8 bytes)
#endif
                (3u << 8u) | // 32 to 16 bytes old_nsz 2 => new_nsz 1
                (6u << 12u) | // 64 to 32
                (13u << 16u); // 128 to 64
            const uint8_t bc_sizedown_limit = (nsz_to_bc_sizedown_limit >> ((unsigned)old_sz << 2u)) & 0xFu;
            const uint8_t new_sz = old_sz - (bc < bc_sizedown_limit);
            if (new_sz < old_sz) {
                newnx = alloc_node(mrx, new_sz);
                NEXT_BLOCK_HDR_SET(newnx, new_sz);
                if (!PTR_PREFIX_MATCHES(newnx, oldnx)) {
                    // we got unlucky and the newnx will have to be long pointer, then it's
                    // better to just keep the old
                    free_node(mrx, newnx, new_sz);
                    newnx = oldnx;
                }
            }
        }
    }

    if (oldnx == newnx) {
        // in place branch erase
        debug_print("in-place branch erase %p\n", newnx);
        if (NEXT_BLOCK_IS_SHORT_PTR(newnx)) {
            const uint8_t hdr = NEXT_BLOCK_HDR(newnx);
            memmove(&newnx->sp[nb], &newnx->sp[nb+1], (bc - nb) * sizeof(mrx_sp_t));
            NEXT_BLOCK_HDR_SET(newnx, hdr);
        } else if (!PTR_PREFIX_MATCHES(newnx, next_block_get_child(newnx, nb)) &&
                   // note the --newnx->lp.lp_count, bit ugly to have inside, but get nicer if/else flow
                   --newnx->lp.lp_count == 0)
        {
            debug_print("convert to short pointer\n");
            // prefix guaranteed to be kept
            newnx = mrx_nx_lp_to_sp_(mrx, newnx, bc+1);
            const uint8_t hdr = NEXT_BLOCK_HDR(newnx);
            memmove(&newnx->sp[nb], &newnx->sp[nb+1], (bc - nb) * sizeof(mrx_sp_t));
            NEXT_BLOCK_HDR_SET(newnx, hdr);
            node->mn.next[br >> 5u] = SP(newnx);
            if (!HDR_IS_SHORT_PTR(node->hdr)) {
                node->mn.local[br >> 5u] = HP(newnx);
            } else if (bc <= MASK_NODE_MAX_LOCAL_BRANCH_COUNT) {
                mrx_mask_node_try_move_to_local_block_(mrx, node);
            }
        } else {
            if (nb < NEXT_BLOCK_MAX_LP_COUNT - 1) {
                const unsigned start0 = nb;
                const unsigned end0 = bc < NEXT_BLOCK_MAX_LP_COUNT - 1 ? bc : NEXT_BLOCK_MAX_LP_COUNT - 1;
                memmove(&newnx->lp.lp[start0], &newnx->lp.lp[start0+1], (end0 - start0) * sizeof(void *));
            }
            if (bc >= NEXT_BLOCK_MAX_LP_COUNT) {
                union mrx_next_block *mid = newnx->lp.next;
                if (nb < NEXT_BLOCK_MAX_LP_COUNT) {
                    newnx->lp.lp[NEXT_BLOCK_MAX_LP_COUNT - 1] = mid->lp.lp[0];
                }
                const unsigned start1 = nb < NEXT_BLOCK_MAX_LP_COUNT ? 0 : nb - NEXT_BLOCK_MAX_LP_COUNT;
                const unsigned end1 = bc < 2 * NEXT_BLOCK_MAX_LP_COUNT - 1 ? bc - NEXT_BLOCK_MAX_LP_COUNT : NEXT_BLOCK_MAX_LP_COUNT - 1;
                if (end1 > start1) {
                    memmove(&mid->lp.lp[start1], &mid->lp.lp[start1+1], (end1 - start1) * sizeof(void *));
                }
                if (bc >= 2 * NEXT_BLOCK_MAX_LP_COUNT) {
                    union mrx_next_block *top = mid->lp.next;
                    if (nb < 2 * NEXT_BLOCK_MAX_LP_COUNT) {
                        mid->lp.lp[NEXT_BLOCK_MAX_LP_COUNT - 1] = top->lp.lp[0];
                    }
                    const unsigned start2 = nb < 2 * NEXT_BLOCK_MAX_LP_COUNT ? 0 : nb - 2 * NEXT_BLOCK_MAX_LP_COUNT;
                    const unsigned end2 = bc - 2 * NEXT_BLOCK_MAX_LP_COUNT;
                    memmove(&top->lp.lp[start2], &top->lp.lp[start2+1], (end2 - start2) * sizeof(void *));
                }
            }
            // see if any chained block should be freed, we use some margin to minimize flapping
            if (bc <= 10) {
                if (newnx->lp.next != NULL) {
                    union mrx_next_block *mid = newnx->lp.next;
                    if (mid->lp.next != NULL) {
                        union mrx_next_block *top = mid->lp.next;
                        free_node(mrx, top, 3);
                    }
                    free_node(mrx, mid, 4);
                    newnx->lp.next = NULL;
                }
            } else if (bc <= 24) {
                if (newnx->lp.next != NULL) {
                    union mrx_next_block *mid = newnx->lp.next;
                    if (mid->lp.next != NULL) {
                        union mrx_next_block *top = mid->lp.next;
                        free_node(mrx, top, 3);
                        mid->lp.next = NULL;
                    }
                }
            }
        }
        debug_check_node(node);
    } else {
        // out of place branch erase, can only happen if both old and new is short ptr
        debug_print("out-of-place branch erase %p %p\n", oldnx, newnx);
        assert(NEXT_BLOCK_IS_SHORT_PTR(oldnx) && NEXT_BLOCK_IS_SHORT_PTR(newnx));
        const uint8_t hdr = NEXT_BLOCK_HDR(newnx);
        memcpy(newnx->sp, oldnx->sp, nb * sizeof(mrx_sp_t));
        memcpy(&newnx->sp[nb], &oldnx->sp[nb+1], (bc - nb) * sizeof(mrx_sp_t));
        NEXT_BLOCK_HDR_SET(newnx, hdr);
        if (newnx == (union mrx_next_block *)node->mn.local) {
            HDR_MN_SET_LOCAL_USED(node->mn.hdr, true);
            node->mn.next[br >> 5u] = SP(node->mn.local);
            free_nx_node(mrx, oldnx);
        } else {
            ptrpfx_mn_insert_nx_ptr(mrx, node, newnx, br, 1);
        }
        debug_check_node(node);
    }
}

static void
erase_value_from_node(mrx_base_t *mrx,
                      struct mrx_iterator_path_element path[],
                      int level)
{
    union mrx_node *node = path[level].node;
    uint8_t nsz = HDR_NSZ(node->hdr);

    if (IS_MASK_NODE(nsz)) {
        // mask node means lots of branches, just erase value
        HDR_CLR_HAS_VALUE(node->hdr);
        return;
    }
    uint8_t br_len = HDR_BR_LEN(node->hdr);
    if (br_len > 0) {
        HDR_CLR_HAS_VALUE(node->hdr);
        if (br_len == 1) {
            // only one branch, it may be possible to merge with child after value is erased
            scan_node_child_merge(mrx, path, level);
        }
        return;
    }

    // no branches, erase the whole node, this may lead to erases of parents too
    free_node(mrx, node, nsz);
    while (level > 0) {
        level--;
        node = path[level].node;
        nsz = HDR_NSZ(node->hdr);
        if (IS_MASK_NODE(nsz)) {
            mask_node_erase_branch(mrx, path, level);
            return;
        }
        br_len = HDR_BR_LEN(node->hdr);
        if (br_len > 1 || HDR_HAS_VALUE(node->hdr)) {
            scan_node_erase_branch(mrx, path, level);
            return;
        }
        // last child erased and no value, erase this node too
        ptrpfx_sn_free(mrx, node);
        free_node(mrx, node, nsz);
    }
    // root was erased!
    mrx->root = NULL;
    mrx->max_keylen_n_flags = mrx->max_keylen_n_flags & MRX_FLAGS_MASK_;
}

static void
mn_ib_inplace_short(union mrx_next_block *nx,
                    const uint8_t nb, // position of new branch
                    const uint8_t bc, // branch count in block after insert
                    union mrx_node *branch)
{
    debug_print("inplace short\n");
    const uint8_t hdr = NEXT_BLOCK_HDR(nx);
    if (bc - nb > 1) {
        memmove(&nx->sp[nb+1], &nx->sp[nb], (bc-nb-1) * sizeof(mrx_sp_t));
    }
    nx->sp[nb] = SP(branch);
    NEXT_BLOCK_HDR_SET(nx, hdr);
}

static void
mn_ib_old_short_new_short(mrx_base_t *mrx,
                          union mrx_node *node,
                          union mrx_next_block *oldnx,
                          union mrx_next_block *newnx,
                          const uint8_t nb, // position of new branch
                          const uint8_t bc, // branch count in block after insert
                          const uint8_t branch_octet,
                          union mrx_node *branch)
{
    debug_print("old short new short\n");
    const uint8_t oldhdr = NEXT_BLOCK_HDR(oldnx);
    const uint8_t newhdr = NEXT_BLOCK_HDR(newnx);
    if (nb > 0) {
        memcpy(&newnx->sp[0], &oldnx->sp[0], nb * sizeof(mrx_sp_t));
    }
    newnx->sp[nb] = SP(branch);
    if (bc - nb > 1) {
        memcpy(&newnx->sp[nb+1], &oldnx->sp[nb], (bc-nb-1) * sizeof(mrx_sp_t));
    }
    NEXT_BLOCK_HDR_SET(oldnx, oldhdr);
    NEXT_BLOCK_HDR_SET(newnx, newhdr);

    ptrpfx_mn_insert_nx_ptr(mrx, node, newnx, branch_octet, 1);
    debug_check_node(node);
}

static void
mn_ib_old_short_new_long(mrx_base_t *mrx,
                         union mrx_node *node,
                         union mrx_next_block *oldnx,
                         union mrx_next_block *newnx,
                         const uint8_t nb, // position of new branch
                         const uint8_t bc, // branch count in block after insert
                         const uint8_t branch_octet,
                         union mrx_node *branch)
{
    debug_print("old short new long\n");
    if (!PTR_PREFIX_MATCHES(branch, newnx)) {
        newnx->lp.lp_count = 1;
    }
    if (!PTR_PREFIX_MATCHES(oldnx, newnx)) {
        newnx->lp.lp_count += bc - 1;
    }
    uint8_t j = 0;
    uint8_t ofs = 0;
    if (nb == 0) {
        newnx->lp.lp[0] = branch;
    } else {
        newnx->lp.lp[0] = EP(oldnx->sp[0], oldnx);
        j = 1;
    }
    union mrx_next_block *nxb = newnx;
    for (uint8_t i = 1; i < bc; i++) {
        if (i == NEXT_BLOCK_MAX_LP_COUNT || i == 2 * NEXT_BLOCK_MAX_LP_COUNT) {
            nxb = nxb->lp.next;
            ofs += NEXT_BLOCK_MAX_LP_COUNT;
        }
        if (i == nb) {
            nxb->lp.lp[i-ofs] = branch;
        } else {
            nxb->lp.lp[i-ofs] = EP(oldnx->sp[j], oldnx);
            j++;
        }
    }

    ptrpfx_mn_insert_nx_ptr(mrx, node, newnx, branch_octet, 1);
    debug_check_node(node);
}

static void
mn_ib_inplace_long_may_extend(mrx_base_t *mrx,
                              union mrx_next_block *nx,
                              const uint8_t nb, // position of new branch
                              const uint8_t bc, // branch count in block after insert
                              union mrx_node *branch)
{
    debug_print("inplace long may extend\n");
    if (!PTR_PREFIX_MATCHES(branch, nx)) {
        nx->lp.lp_count++;
    }
    // extend if required
    union mrx_next_block *mid = nx->lp.next;
    union mrx_next_block *top = mid == NULL ? NULL : mid->lp.next;
    if (bc == NEXT_BLOCK_MAX_LP_COUNT + 1 && mid == NULL) {
        mid = alloc_node(mrx, 4);
        nx->lp.next = mid;
        mid->lp.next = NULL;
    }
    if (bc == 2 * NEXT_BLOCK_MAX_LP_COUNT + 1) {
        top = mid->lp.next;
        if (top == NULL) {
            top = alloc_node(mrx, 3);
            mid->lp.next = top;
        }
    }
    // fill in leaf pointer and shift up pointers above it
    union mrx_next_block *nxb;
    int ofs;
    if (nb < NEXT_BLOCK_MAX_LP_COUNT) {
        nxb = nx;
        ofs = 0;
    } else if (nb < 2 * NEXT_BLOCK_MAX_LP_COUNT) {
        nxb = mid;
        ofs = NEXT_BLOCK_MAX_LP_COUNT;
    } else {
        nxb = top;
        ofs = 2 * NEXT_BLOCK_MAX_LP_COUNT;
    }
    union mrx_node *prev = nxb->lp.lp[nb-ofs];
    nxb->lp.lp[nb-ofs] = branch;
    for (uint8_t i = nb + 1; i < bc; i++) {
        if (i == NEXT_BLOCK_MAX_LP_COUNT || i == 2 * NEXT_BLOCK_MAX_LP_COUNT) {
            nxb = nxb->lp.next;
            ofs += NEXT_BLOCK_MAX_LP_COUNT;
        }
        union mrx_node *swp = prev;
        prev = nxb->lp.lp[i-ofs];
        nxb->lp.lp[i-ofs] = swp;
    }
}

static void
mn_ib_first_branch_in_block(mrx_base_t *mrx,
                            union mrx_node *node,
                            const uint8_t branch_octet,
                            union mrx_node *branch)
{
    debug_print("first branch in block\n");
    // first branch in 32 bit block, create new next block
    if (!HDR_MN_LOCAL_USED(node->mn.hdr) && HDR_IS_SHORT_PTR(node->mn.hdr) && PTR_PREFIX_MATCHES(branch, node)) {
        // we can use local
        node->mn.local[0] = SP(branch);
        node->mn.next[branch_octet >> 5u] = SP(node->mn.local);
        HDR_MN_SET_LOCAL_USED(node->mn.hdr, true);
        debug_check_node(node);
        return;
    }

    // first test if we can have a short pointer next block
#if ARCH_SIZEOF_PTR == 8
    const uint8_t min_nsz = 1;
#else
    const uint8_t min_nsz = 0;
#endif
    union mrx_next_block *newnx = alloc_node(mrx, min_nsz);
    if (PTR_PREFIX_MATCHES(branch, newnx)) {
        newnx->sp[0] = SP(branch);
        NEXT_BLOCK_HDR_SET(newnx, min_nsz);
    } else {
        free_node(mrx, newnx, min_nsz);
        newnx = mrx_alloc_nx_node_lp_(mrx, 1);
        newnx->lp.lp[0] = branch;
        newnx->lp.lp_count = 1;
        if (PTR_PREFIX_MATCHES(branch, newnx)) {
            //unlikely case, new allocation caused a matching pointer
            newnx = mrx_nx_lp_to_sp_(mrx, newnx, 1);
        }
    }
    ptrpfx_mn_insert_nx_ptr(mrx, node, newnx, branch_octet, 0);
    debug_check_node(node);
}

// Input requirement: must be new branch, ie not replacing existing
static void
mask_node_insert_branch(mrx_base_t *mrx,
                        union mrx_node *node,
                        const unsigned branch_octet,
                        union mrx_node *branch)
{
    debug_print("mask node insert branch in %02X\n", branch_octet);
    debug_check_node(node);

    bit32_set(node->mn.bitmask.u32, branch_octet);
    node->mn.used |= 1u << (branch_octet >> 5u);
    node->mn.branch_count++;

    const uint8_t bc = bit32_count(node->mn.bitmask.u32[branch_octet >> 5u]);
    if (bc == 1) {
        mn_ib_first_branch_in_block(mrx, node, branch_octet, branch);
        return;
    }

    const uint32_t b = 1u << (branch_octet & 0x1Fu);
    const uint8_t nb = bit32_count(node->mn.bitmask.u32[branch_octet >> 5u] & (b - 1));
    union mrx_next_block *nx = mask_node_get_next_block(node, branch_octet);
    union mrx_next_block *newnx;

    // if necessary allocate a new node of proper size
    if (nx != (union mrx_next_block *)node->mn.local) {
        if (!NEXT_BLOCK_IS_SHORT_PTR(nx)) {
            mn_ib_inplace_long_may_extend(mrx, nx, nb, bc, branch);
        } else if (!PTR_PREFIX_MATCHES(branch, nx)) {
            // must make long pointer block
            debug_print("make long pointer %d\n", bc);
            newnx = mrx_alloc_nx_node_lp_(mrx, bc);
            mn_ib_old_short_new_long(mrx, node, nx, newnx, nb, bc, branch_octet, branch);
        } else if (!HDR_MN_LOCAL_USED(node->mn.hdr) &&
            bc <= MASK_NODE_MAX_LOCAL_BRANCH_COUNT &&
            HDR_IS_SHORT_PTR(node->mn.hdr) &&
            PTR_PREFIX_MATCHES(branch, node))
        {
            debug_print("new local\n");
            newnx = (union mrx_next_block *)node->mn.local;
            HDR_MN_SET_LOCAL_USED(node->mn.hdr, true);
            mn_ib_old_short_new_short(mrx, node, nx, newnx, nb, bc, branch_octet, branch);
        } else if ((((unsigned)bc - 1u) & ((unsigned)bc - 2u)) != 0) { // test if bc is power of 2 + 1, if so we need to extend, else inplace
            mn_ib_inplace_short(nx, nb, bc, branch);
        } else {
            debug_print("new extend\n");
            uint8_t new_sz = bit32_bsf((((unsigned)bc - 1u) * sizeof(mrx_sp_t)) >> 2u);
#if ARCH_SIZEOF_PTR == 8
            if (new_sz == 0) {
                new_sz = 1; // don't use 8 byte node size on 64 bit
            }
#endif
            newnx = alloc_node(mrx, new_sz);
            NEXT_BLOCK_HDR_SET(newnx, new_sz);
            if (PTR_PREFIX_MATCHES(nx, newnx)) {
                mn_ib_old_short_new_short(mrx, node, nx, newnx, nb, bc, branch_octet, branch);
            } else {
                // prefix mismatch (should be unlikely)
                free_node(mrx, newnx, new_sz);
                newnx = mrx_alloc_nx_node_lp_(mrx, bc);
                if (PTR_PREFIX_MATCHES(nx, newnx)) {
                    // prefix matches again...
                    mrx_free_nx_node_slowpath_(mrx, newnx, 1); // keep first
                    NEXT_BLOCK_HDR_SET(newnx, 4);
                    mn_ib_old_short_new_short(mrx, node, nx, newnx, nb, bc, branch_octet, branch);
                } else {
                    mn_ib_old_short_new_long(mrx, node, nx, newnx, nb, bc, branch_octet, branch);
                }
            }
        }
    } else {
        // old is local in node
        if (!PTR_PREFIX_MATCHES(branch, node)) {
            newnx = mrx_alloc_nx_node_lp_(mrx, bc);
            mn_ib_old_short_new_long(mrx, node, nx, newnx, nb, bc, branch_octet, branch);
        } else if (bc <= MASK_NODE_MAX_LOCAL_BRANCH_COUNT) {
            mn_ib_inplace_short(nx, nb, bc, branch);
        } else {
            debug_print("local exceeded\n");
            newnx = alloc_node(mrx, 3);
            NEXT_BLOCK_HDR_SET(newnx, 3);
            if (PTR_PREFIX_MATCHES(newnx, node)) {
                mn_ib_old_short_new_short(mrx, node, nx, newnx, nb, bc, branch_octet, branch);
            } else {
                free_node(mrx, newnx, 3);
                newnx = mrx_alloc_nx_node_lp_(mrx, bc);
                if (PTR_PREFIX_MATCHES(newnx, node)) {
                    mrx_free_nx_node_slowpath_(mrx, newnx, 1); // keep first
                    NEXT_BLOCK_HDR_SET(newnx, 4);
                    mn_ib_old_short_new_short(mrx, node, nx, newnx, nb, bc, branch_octet, branch);
                } else {
                    mn_ib_old_short_new_long(mrx, node, nx, newnx, nb, bc, branch_octet, branch);
                }
            }
        }
    }
}

// Inserts a branch in a scan node, also handles ptrpfx updates, and updates branch length.
// Condition: there must be space for inserting a branch, and branch_octet must not exist
// in current node.
static void
sn_insert_branch(mrx_base_t *mrx,
                 union mrx_node *node,
                 const uint8_t branch_octet,
                 uint8_t br[],
                 uint8_t cur_br_len,
                 union mrx_node *branch)
{
    mrx_sp_t *sp = SP_ALIGN(&br[cur_br_len]);

    // shift: 1 if we we have to move short pointer array start up one step, otherwise 0
    const uint8_t sp_shift = !(SP_ALIGN(&br[cur_br_len + 1]) == sp);

    // bit trick: pfx becomes NULL if node is short pointer, no branch required
    struct mrx_ptrpfx_node *pfx = (struct mrx_ptrpfx_node *)((uintptr_t)SN_PREFIX_PTR(node) & (-(uintptr_t)!HDR_IS_SHORT_PTR(node->hdr)));
    const uint8_t br_pos = find_new_branch_pos(branch_octet, br, cur_br_len);

    // order of memmove() is important, due to possible overlaps
    if (br_pos != cur_br_len) {
        memmove(&sp[br_pos+sp_shift+1], &sp[br_pos], (cur_br_len - br_pos) * sizeof(mrx_sp_t));
        if (sp_shift && br_pos > 0) {
            memmove(&sp[sp_shift], sp, br_pos * sizeof(mrx_sp_t));
        }
        sp[br_pos+sp_shift] = SP(branch);
        memmove(&br[br_pos+1], &br[br_pos], (cur_br_len - br_pos) * sizeof(uint8_t));
    } else {
        if (sp_shift && br_pos > 0) {
            memmove(&sp[sp_shift], sp, br_pos * sizeof(mrx_sp_t));
        }
        sp[br_pos+sp_shift] = SP(branch);

    }
    br[br_pos] = branch_octet;

    const uint8_t br_len = cur_br_len + 1;
    HDR_SET_BR_LEN(node->hdr, br_len);

    if (pfx != NULL) {
        memmove(&pfx->hp[br_pos+1], &pfx->hp[br_pos], (cur_br_len - br_pos) * sizeof(mrx_sp_t));
        pfx->hp[br_pos] = HP(branch);
        if (!PTR_PREFIX_MATCHES(node, branch)) {
            pfx->lp_count++;
        }
        const uintptr_t pfxref = (uintptr_t)&SN_PREFIX_PTR(node);
        const uintptr_t sp_end = (uintptr_t)&sp[sp_shift+br_len];
        //fprintf(stderr, "%lu shift %d %d (%p)\n", sp_end - pfxref, sp_shift, br_pos == cur_br_len, branch);
        if (sp_end > pfxref) {
            // Pointer overwritten, restore.
            if (sp_end - pfxref == 8) {
                // two pointers shifted up. This will be a bit messy as the memmoves above is optimized for
                // the common short pointer case and when the short pointer array overlaps with two, one
                // pointer can be bad.
                if (sp_shift) {
                    // shift 1 and lengthen 1, both memmoved pointers will be ok
                    pfx->orig.sp[1] = *(mrx_sp_t *)(pfxref + sizeof(mrx_sp_t));
                    pfx->orig.sp[0] = *(mrx_sp_t *)(pfxref);
                } else {
                    if (br_pos == cur_br_len) {
                        // added to end, first pointer same as before, second is the new branch pointer
                        pfx->orig.sp[1] = *(mrx_sp_t *)(pfxref + sizeof(mrx_sp_t)); // same as SP(branch)
                    } else {
                        // first pointer will be okay, but not the second as memmove() will have moved the pfx pointer
                        pfx->orig.sp[1] = pfx->orig.sp[0];
                        pfx->orig.sp[0] = *(mrx_sp_t *)(pfxref);
                    }
                }
            } else {
                // only one pointer shifted up, contains correct value
                pfx->orig.sp[0] = *(mrx_sp_t *)(pfxref);
            }
            *(struct mrx_ptrpfx_node **)pfxref = pfx;
        }
   } else if (!PTR_PREFIX_MATCHES(node, branch)) {
        union mrx_node *branches[br_len];
        for (uint8_t i = 0; i < br_len; i++) {
            if (i == br_pos) {
                branches[i] = branch;
            } else {
                branches[i] = EP(sp[i], node);
            }
        }
        mrx_ptrpfx_convert_to_lp_due_to_branches_(mrx, node, branches, br_len);
    }
}

// Converts a 128 byte scan node without prefix to a 128 byte mask node
static void
scan_node_to_mask_node(mrx_base_t *mrx,
                       union mrx_node *node,
                       const int branch_octet, // -1 (no octet) or new, not pre-existing
                       union mrx_node *branch)
{
    debug_print("scan node to mask node 0x%02X\n", branch_octet);
    debug_check_node(node);
    assert(HDR_PX_LEN(node->hdr) == 0);

    const uint8_t br_len = HDR_BR_LEN(node->hdr);
    const uint8_t *br = node->sn.octets;
    const mrx_sp_t *brp = SP_ALIGN(&br[br_len]);
    uint32_t bm[8];
    uint8_t used = 0;
    for (uint8_t i = 0; i < br_len; i++) {
        const uint8_t bro = br[i];
        const uint8_t j = bro >> 5u;
        const uint8_t used_bit = 1u << j;
        const uint32_t br_bit = 1u << (bro & 0x1Fu);
        bm[j] = (bm[j] & -(uint32_t)((used & used_bit) != 0)) | br_bit;
        used |= used_bit;
    }
    if (branch_octet != -1) { // branch octet is optional
        const uint8_t j = (unsigned)branch_octet >> 5u;
        const uint8_t used_bit = 1u << j;
        const uint32_t br_bit = 1u << ((unsigned)branch_octet & 0x1Fu);
        bm[j] = (bm[j] & -(uint32_t)((used & used_bit) != 0)) | br_bit;
        used |= used_bit;
    }

    bool nxp_long_ptr_required = false;
    union mrx_next_block *nxp[8]; // pointers to next blocks
    mrx_sp_t local[MASK_NODE_MAX_LOCAL_BRANCH_COUNT]; // local next block
    int local_pos = -1;
    uint8_t local_bc = 0;
    uint8_t br_pos = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if ((used & (1u << i)) == 0) {
            bm[i] = 0;
            nxp[i] = NULL;
            continue;
        }
        mrx_sp_t n_sp[32];
#if ARCH_SIZEOF_PTR == 8
        union mrx_node *n_fp[32];
        struct {
            mrx_sp_t prefix;
            uint8_t count;
        } hp[32];
        n_fp[0] = NULL;
        hp[0].prefix = 0;
#endif
        uint8_t hp_count = 0;
        // fill in pointers
        uint32_t bm_block = bm[i];
        const uint8_t bc = bit32_count(bm_block);
        for (uint8_t j = 0; j < bc; j++) {
            const uint8_t b = ((unsigned)i << 5u) | bit32_bsf(bm_block);
            union mrx_node *n;
            if (b == branch_octet) {
                n = branch;
            } else {
                n = scan_node_get_child1(node, brp, br_pos);
                br_pos++;
            }
            n_sp[j] = SP(n);
#if ARCH_SIZEOF_PTR == 8
            n_fp[j] = n;
            if (hp_count == 0) {
                hp[0].prefix = HP(n);
                hp[0].count = 1;
                hp_count = 1;
            } else {
                uint8_t k = 0;
                for (; k < hp_count; k++) {
                    if (HP(n) == hp[k].prefix) {
                        break;
                    }
                }
                if (k == hp_count) {
                    hp[hp_count].prefix = HP(n);
                    hp[hp_count].count = 1;
                    hp_count++;
                } else {
                    hp[k].count++;
                }
            }
#endif
            bm_block &= ~(1u << ((unsigned)b & 0x1Fu));
        }
        const unsigned tot_size_p2 = hp_count > 1 ? 128 : next_power_of_two_for_8bit(bc * sizeof(mrx_sp_t));
        uint8_t nsz = NODE_SIZE_P2_TO_NSZ(tot_size_p2);
#if ARCH_SIZEOF_PTR == 8
        nsz = nsz == 0 ? 1 : nsz; // don't use size 8 for 64 bit
#endif
        if (local_pos == -1 &&
            !nxp_long_ptr_required &&
            bc <= MASK_NODE_MAX_LOCAL_BRANCH_COUNT &&
            hp_count <= 1 &&
            PTR_PREFIX_MATCHES(n_fp[0], node))
        {
            debug_print("use local\n");
            local_pos = i;
            local_bc = bc;
            memcpy(local, n_sp, bc * sizeof(mrx_sp_t));
        } else {
            nxp[i] = alloc_node(mrx, nsz);
            bool long_ptr_nx = false;
#if ARCH_SIZEOF_PTR == 8
            if (nsz < 4 && !PTR_PREFIX_MATCHES(nxp[i], n_fp[0])) {
                // short pointer block failed
                free_node(mrx, nxp[i], nsz);
                nsz = 4;
                nxp[i] = alloc_node(mrx, nsz);
                // now nxp[i] might match again, if so let it be.
            }
            if (!PTR_PREFIX_MATCHES(nxp[i], node)) {
                nxp_long_ptr_required = true;
                if (local_pos != -1) {
                    // move out local block to an allocated block
                    debug_print("cancel local\n");
                    // always allocate full length as it's likely that this also requires long pointer and thus full length
                    nxp[local_pos] = alloc_node(mrx, 4);
                    union mrx_next_block *nx = nxp[local_pos];
                    if (PTR_PREFIX_MATCHES(nx, node)) {
                        // unlikely case that it actually matches, we get a too long block here but that's not too bad
                        memcpy(nx->sp, local, local_bc * sizeof(mrx_sp_t));
                        NEXT_BLOCK_HDR_SET(nx, 4);
                    } else {
                        NEXT_BLOCK_HDR_SET(nx, NEXT_BLOCK_LONG_PTR_HDR);
                        nx->lp.lp_count = local_bc;
                        for (uint8_t k = 0; k < local_bc; k++) {
                            nx->lp.lp[k] = EP(local[k], node);
                        }
                        nx->lp.next = NULL;
                    }
                    local_pos = -1;
                }
            }
            if (hp_count > 1 || HP(nxp[i]) != hp[0].prefix) {
                long_ptr_nx = true;
                union mrx_next_block *nx = nxp[i];
                if (bc > NEXT_BLOCK_MAX_LP_COUNT) {
                    union mrx_next_block *mid = alloc_node(mrx, 4);
#if SCAN_NODE_MAX_BRANCH_COUNT > 2 * NEXT_BLOCK_MAX_LP_COUNT
#error "code expects SCAN_NODE_MAX_BRANCH_COUNT to be <= 2 * NEXT_BLOCK_MAX_LP_COUNT"
#endif
                    memcpy(mid->lp.lp, &n_fp[NEXT_BLOCK_MAX_LP_COUNT], (bc - NEXT_BLOCK_MAX_LP_COUNT) * sizeof(uintptr_t));
                    mid->lp.next = NULL;
                    memcpy(nx->lp.lp, n_fp, NEXT_BLOCK_MAX_LP_COUNT * sizeof(uintptr_t));
                    nx->lp.next = mid;
                } else {
                    memcpy(nx->lp.lp, n_fp, bc * sizeof(uintptr_t));
                    nx->lp.next = NULL;
                }
                NEXT_BLOCK_HDR_SET(nx, NEXT_BLOCK_LONG_PTR_HDR);
                // fill in long pointer count
                nx->lp.lp_count = 0;
                for (int k = 0; k < hp_count; k++) {
                    if (hp[k].prefix != HP(nx)) {
                        nx->lp.lp_count += hp[k].count;
                    }
                }
            }
#endif
            if (!long_ptr_nx) {
                union mrx_next_block *nx = nxp[i];
                memcpy(nx, n_sp, bc * sizeof(mrx_sp_t));
                NEXT_BLOCK_HDR_SET(nx, nsz);
            }
        }
    }
#if ARCH_SIZEOF_PTR == 8
    if (!HDR_IS_SHORT_PTR(node->hdr)) {
        // for less branching we get the value out regardless if there is a value or not
        struct mrx_ptrpfx_node *pfx = SN_PREFIX_PTR(node);
        void *value = pfx->orig.value;
        ptrpfx_sn_free(mrx, node);
        node->mn.value = value;
    }
#endif
    HDR_SET(node->mn.hdr, MRX_NODE_MASK_NODE_NSZ_VALUE_, 0, false, 0, HDR_HAS_VALUE(node->hdr));
    node->mn.used = used;
    node->mn.branch_count = br_len + (branch_octet != -1);
    memcpy(node->mn.bitmask.u32, bm, sizeof(bm));
    if (local_pos != -1) {
        HDR_MN_SET_LOCAL_USED(node->mn.hdr, true);
        nxp[local_pos] = (union mrx_next_block *)node->mn.local;
        memcpy(node->mn.local, local, local_bc * sizeof(mrx_sp_t));
    } else {
        HDR_MN_SET_LOCAL_USED(node->mn.hdr, false);
    }
    for (uint8_t i = 0; i < 8; i++) {
        node->mn.next[i] = SP(nxp[i]);
    }
#if ARCH_SIZEOF_PTR == 8
    if (nxp_long_ptr_required) {
        HDR_SET_LONGPTR(node->hdr);
        uint8_t lp_count = 0;
        for (uint8_t i = 0; i < 8; i++) {
            node->mn.local[i] = HP(nxp[i]);
            if (bm[i] != 0 && HP(nxp[i]) != HP(node)) {
                lp_count++;
            }
        }
        node->mn.lp_count = lp_count;
    }
#endif
    debug_check_node(node);
}

static void
scan_node_insert_branch(mrx_base_t *mrx,
                        struct mrx_iterator_path_element path[],
                        int *level,
                        uint8_t nsz,
                        uint8_t br[],
                        const uint8_t br_len,
                        const uint8_t branch_octet,
                        union mrx_node *branch)
{
    union mrx_node *node = path[*level].node;
    debug_print("scan node insert branch in %p\n", node);
    debug_check_node(node);
    mrx_sp_t * const node_end = NODE_END_REF(node, nsz);
    uint8_t * const br_end = &br[br_len];
    mrx_sp_t * const brp_end = &SP_ALIGN(br_end)[br_len];

    // If branch array ends at alignment, branch pointer array base will have to
    // move up one step when another branch is inserted.
    uint8_t shift = !((uintptr_t)br_end & 3u);

    // remove the space associated to a value (if exists)
    shift += HDR_HAS_VALUE(node->hdr) * (sizeof(void *)/sizeof(mrx_sp_t));

    if (brp_end + shift < node_end) {
        // fits in current
        sn_insert_branch(mrx, node, branch_octet, br, br_len, branch);
    } else if (nsz <= 3) {
        // enlarge node
        const uint8_t px_len = HDR_PX_LEN(node->hdr);
        // px_cmp: if prefix is this size (range) we will reach max branch count
        // without further shortening of prefix
#if ARCH_SIZEOF_PTR == 4
        const int px_cmp = (px_len == 1);
#elif ARCH_SIZEOF_PTR == 8
        const int px_cmp = (px_len > 0 && px_len < 5);
#endif
        if (nsz == 3 && px_cmp) {
            debug_print("move out prefix (len %d)\n", px_len);
            // special case, must move out prefix to make it possible to convert
            // the max size node to a mask node later.
            prefix_move_up(mrx, path, level, px_len);
        }
        enlarge_node(mrx, path, *level);
        nsz++;
        node = path[*level].node;
        br = &node->sn.octets[HDR_PX_LEN(node->hdr)];
        sn_insert_branch(mrx, node, branch_octet, br, br_len, branch);
    } else if (br_len <
               SCAN_NODE_MAX_BRANCH_COUNT * !HDR_HAS_VALUE(node->hdr) +
               SCAN_NODE_MAX_BRANCH_COUNT_WITH_VALUE * HDR_HAS_VALUE(node->hdr))
    {
        // With space for more branches, but we may need to move prefix
        const uint8_t mv_px_len = minimum_move_px_len_for_new_branch(node, br, br_len);
        prefix_move_up(mrx, path, level, mv_px_len);
        node = path[*level].node;
        br = &node->sn.octets[HDR_PX_LEN(node->hdr)];
        sn_insert_branch(mrx, node, branch_octet, br, br_len, branch);
    } else {
        const uint8_t px_len = HDR_PX_LEN(node->hdr);
        if (px_len > 0) {
            // Only exists for safety, normally when we get here the node should already have zero prefix
            prefix_move_up(mrx, path, level, px_len);
        }
        scan_node_to_mask_node(mrx, node, branch_octet, branch);
    }
    debug_check_node(path[*level].node);
    debug_print("scan node insert branch out %p\n", path[*level].node);
}

static void **
scan_node_insert_value(mrx_base_t *mrx,
                       struct mrx_iterator_path_element path[],
                       int *level,
                       uint8_t nsz)
{
    union mrx_node *node = path[*level].node;
    debug_print("scan node insert value\n");
    debug_check_node(node);
    const uint8_t px_len = HDR_PX_LEN(node->hdr);
    const uint8_t br_len = HDR_BR_LEN(node->hdr);
    const uint8_t *br = &node->sn.octets[px_len];
    const uintptr_t br_end = (uintptr_t)&br[br_len];
    const uintptr_t brp_end = (uintptr_t)&SP_ALIGN(br_end)[br_len];
    const uintptr_t vref = (uintptr_t)VALUE_REF(node, nsz);

    if (brp_end > vref) {
        // larger node or mask node needed
        if (nsz <= 3) {
            enlarge_node(mrx, path, *level);
            node = path[*level].node;
            nsz++;
            debug_print("scan node insert value - enlarge node\n");
        } else {
            if (br_len <= SCAN_NODE_MAX_BRANCH_COUNT_WITH_VALUE) {
                // Not maximum branch count, pack the value in here by moving prefix up to parent.
                const uint8_t mv_px_len = MIN_MOVE_PX_LEN_FOR_VALUE(brp_end, vref, br_end);
                prefix_move_up(mrx, path, level, mv_px_len);
                node = path[*level].node;
                debug_print("scan node insert value - move prefix up\n");
            } else {
                prefix_move_up(mrx, path, level, px_len);
                scan_node_to_mask_node(mrx, node, -1, NULL);
                debug_print("scan node insert value - make bm node\n");
            }
            nsz = HDR_NSZ(node->hdr); // maybe smaller size or mask node
        }
    }
    debug_check_node(node);
    HDR_SET_HAS_VALUE(node->hdr);
    return mrx_node_value_ref_(node, nsz);
}

static union mrx_node *
new_leaf(mrx_base_t *mrx,
         const uint8_t rest_string[],
         const unsigned rest_length,
         void ***vrefp)
{
    // if rest string is long, we need to store the leaf in a chain of nodes

    const uint8_t *s = rest_string;
    unsigned slen = rest_length;

    // + 2 is for 2 byte header, void * is the value.
    const unsigned occupied_space = 2 + sizeof(void *);
    unsigned tot_size_p2 = slen + occupied_space < 128 ? next_power_of_two_for_8bit(slen + occupied_space) : 128;
    uint8_t px_len = tot_size_p2 - occupied_space;
    uint8_t nsz = NODE_SIZE_P2_TO_NSZ(tot_size_p2);
    union mrx_node *first_node = alloc_node(mrx, nsz);
    union mrx_node *node = first_node;
    union mrx_node *prev_node;
    mrx_sp_t *prev_node_handle;

    for (;;) {
        if (slen <= px_len) {
            *vrefp = VALUE_REF(node, nsz);
            HDR_SET(node->hdr, nsz, 0, false, slen, true);
            memcpy(node->sn.octets, s, slen);
            debug_print("new leaf %d %p\n", rest_length, first_node);
            return first_node;
        }
        // note for the 64 bit platform we "waste" the last 4 bytes, to be able to later add a
        // ptrpfx pointer if necessary
        prev_node_handle = (mrx_sp_t *)VALUE_REF(node, nsz);
        HDR_SET(node->hdr, nsz, 1, false, px_len - 1, false);
        memcpy(node->sn.octets, s, px_len);
        s += px_len;
        slen -= px_len;
        prev_node = node;
        debug_check_node(node);

        // make node for next iteration
        tot_size_p2 = slen + occupied_space < 128 ? next_power_of_two_for_8bit(slen + occupied_space) : 128;
        px_len = tot_size_p2 - occupied_space;
        nsz = NODE_SIZE_P2_TO_NSZ(tot_size_p2);
        node = alloc_node(mrx, nsz);

        // Try init short pointer, then attach pointer prefix node if it turns out to be
        // necessary.

        // This case may look odd and sub-optimal as we here always have only one pointer and
        // that would fit in the VALUE_REF position which fits a full pointer. However, this
        // leaf node is still a generic scan node and must be formatted as such for traversal
        // code to work, ie use short pointers unless the node is converted to a long pointer
        // node with a pointer prefix node.
        //
        // (In a multi-branch node the VALUE_REF space may hold two short pointers)
        *prev_node_handle = SP(node);
        ptrpfx_convert_to_lp_if_necessary_due_to_branch(mrx, prev_node, node);
    }
}

static void **
scan_node_split(mrx_base_t *mrx,
                struct mrx_iterator_path_element path[],
                int *level,
                uint8_t equal_len,
                const uint8_t rest_string[],
                const unsigned rest_length)
{
    union mrx_node *node = path[*level].node;
    debug_print("split node in (at %d)\n", equal_len);
    debug_check_node(node);
    uint8_t offset = 0;
    const bool has_value = rest_length == 0; // value in first node
    const uint8_t br_len = has_value ? 1 : 2;

    // size for first node
    uint8_t nsz = scan_node_min_size_nsz(equal_len, has_value ? 1 : 2, has_value);
    if (nsz > 4) {
        // first node would become too large, we need to split into two
#if ARCH_SIZEOF_PTR == 4
        // occupied space: header 2, one branch octet 1, short branch pointer 4.
        const int occupied_space = 2 + 1 + 4;
#elif ARCH_SIZEOF_PTR == 8
        // occupied space: header 2, one branch octet 1, maybe ptrpfx pointer 8.
        const int occupied_space = 2 + 1 + 8;
#endif
        offset = 64 - occupied_space + 1; // offset == px_len + the branch octet
        union mrx_node *newnode = alloc_node(mrx, 3);
        HDR_SET(newnode->hdr, 3, 1, false, offset - 1, false);
        memcpy(newnode->sn.octets, node->sn.octets, offset);
        mrx_sp_t *brp = SP_ALIGN(&newnode->sn.octets[offset]);
        *brp = SP(node);
        ptrpfx_convert_to_lp_if_necessary_due_to_branch(mrx, newnode, node);
        set_link_in_parent(mrx, path, *level - 1, newnode);

        equal_len -= offset;
        debug_check_node(newnode);
        debug_print("split node retry %d\n", equal_len);
        path[*level].br_pos = 0;
        path[*level].br = node->sn.octets[offset-1];
        path[*level].node = newnode;
        (*level)++;
        path[*level].node = node;

        nsz = scan_node_min_size_nsz(equal_len, br_len, has_value);
    }

    // current as second
    union mrx_node *newnode = alloc_node(mrx, nsz);
    HDR_SET(newnode->hdr, nsz, br_len, false, equal_len, has_value);
    void **vref;
    if (has_value) {
        debug_print("insert value in first\n");

        // equal_len + 1 to get the branch octet too
        mrx_sp_t *brp = SP_ALIGN(&newnode->sn.octets[equal_len + 1]);
        memcpy(newnode->sn.octets, &node->sn.octets[offset], equal_len + 1);
        brp[0] = SP(node);
        ptrpfx_convert_to_lp_if_necessary_due_to_branch(mrx, newnode, node);
        const int vref_level = *level;
        path[*level].br_pos = 0;
        path[*level].br = node->sn.octets[equal_len];

        set_link_in_parent(mrx, path, *level - 1, newnode);
        path[*level].node = newnode;
        debug_check_node(path[*level].node);
        (*level)++;
        path[*level].node = node;
        prefix_shorten(mrx, path, *level, equal_len + offset + 1, false);
        debug_check_node(path[*level].node);
        // due to possible node reallocations and relinking there is a small risk that newnode
        // gets transformed since we first created it, so we need to get vref here at the end
        vref = mrx_node_value_ref_(path[vref_level].node, HDR_NSZ(path[vref_level].node->hdr));
    } else {
        debug_print("insert branch in first\n");
        union mrx_node *leaf = new_leaf(mrx, &rest_string[1], rest_length - 1, &vref);
        union mrx_node *branches[2];
        mrx_sp_t *brp = SP_ALIGN(&newnode->sn.octets[equal_len + 2]);
        memcpy(newnode->sn.octets, &node->sn.octets[offset], equal_len+1);
        if (*rest_string < node->sn.octets[equal_len + offset]) {
            newnode->sn.octets[equal_len] = *rest_string;
            newnode->sn.octets[equal_len + 1] = node->sn.octets[equal_len + offset];
            brp[0] = SP(leaf);
            brp[1] = SP(node);
            branches[0] = leaf;
            branches[1] = node;
            path[*level].br_pos = 1;
            path[*level].br = node->sn.octets[equal_len + offset];
        } else {
            newnode->sn.octets[equal_len + 1] = *rest_string;
            brp[0] = SP(node);
            brp[1] = SP(leaf);
            branches[0] = node;
            branches[1] = leaf;
            path[*level].br_pos = 0;
            path[*level].br = *rest_string;
        }
        ptrpfx_convert_to_lp_if_necessary_due_to_branches(mrx, newnode, branches, 2);

        set_link_in_parent(mrx, path, *level - 1, newnode);
        path[*level].node = newnode;
        debug_check_node(path[*level].node);
        (*level)++;
        path[*level].node = node;
        prefix_shorten(mrx, path, *level, equal_len + offset + 1, false);
        debug_check_node(path[*level].node);
    }
    return vref;
}

void **
mrx_insert_(mrx_base_t *mrx,
            const uint8_t string[],
            const unsigned string_length,
            bool *is_occupied)
{
    if (mrx->count == mrx->capacity) {
        return NULL;
    }
    *is_occupied = false;
    if (string_length > (mrx->max_keylen_n_flags & ~MRX_FLAGS_MASK_)) {
        mrx->max_keylen_n_flags = string_length | (mrx->max_keylen_n_flags & MRX_FLAGS_MASK_);
    }
    if (mrx->root == NULL) {
        void **vref;
        mrx->root = new_leaf(mrx, string, string_length, &vref);
        mrx->count++;
        return vref;
    }

    const uint8_t *s = string;
    unsigned slen = string_length;
    union mrx_node *node = mrx->root;
    int level = 0;
    void **vref = NULL;

    struct mrx_iterator_path_element *path;
    const uint32_t max_keylen = (mrx->max_keylen_n_flags & ~MRX_FLAGS_MASK_);
    const bool path_is_on_stack = (max_keylen <= MRX_MAX_KEY_LENGTH_FOR_PATH_ON_STACK);
    if (path_is_on_stack) {
        path = alloca((max_keylen + 1) * sizeof(*path));
    } else {
        path = malloc((max_keylen + 1) * sizeof(*path));
    }
    path[0].node = node;

    for (;;) {
        const uint8_t nsz = HDR_NSZ(node->hdr);
        const uint8_t px_len = HDR_PX_LEN(node->hdr);
        const uint8_t test_len = (px_len > slen) ? slen : px_len;
        const uint8_t equal_len = prefix_find_first_diff(s, node->sn.octets, test_len);
        if (equal_len != test_len || px_len > slen) {
            s += equal_len;
            slen -= equal_len;
            assert(!IS_MASK_NODE(nsz)); // mask nodes cannot have prefix
            mrx->count++;
            vref = scan_node_split(mrx, path, &level, equal_len, s, slen);
            break;
        }
        s += px_len;
        slen -= px_len;
        if (slen == 0) {
            if (HDR_HAS_VALUE(node->hdr)) {
                *is_occupied = true;
            } else {
                mrx->count++;
                if (IS_SCAN_NODE(nsz)) {
                    vref = scan_node_insert_value(mrx, path, &level, nsz);
                    break;
                }
            }
            HDR_SET_HAS_VALUE(node->hdr);
            vref = mrx_node_value_ref_(node, nsz);
            break;
        }
        if (IS_SCAN_NODE(nsz)) {
            const uint8_t br_len = HDR_BR_LEN(node->hdr);
            uint8_t *br = &node->sn.octets[px_len];
            const int8_t br_pos = find_branch(*s, br, br_len);
            if ((uint8_t)br_pos == br_len) {
                mrx->count++;
                union mrx_node *leaf = new_leaf(mrx, &s[1], slen - 1, &vref);
                scan_node_insert_branch(mrx, path, &level, nsz, br, br_len, *s, leaf);
                break;
            }
            path[level].br_pos = (int16_t)br_pos;
            path[level].br = *s;
            node = scan_node_get_child(node, br, br_len, br_pos);
        } else {
            path[level].br_pos = *s;
            path[level].br = *s;
            uint32_t b = *s;
            uint32_t bm = node->mn.bitmask.u32[b >> 5u];
            union mrx_next_block *nx = mask_node_get_next_block(node, b);
            b = 1u << (b & 0x1Fu);
            if ((bm & b) == 0) {
                mrx->count++;
                union mrx_node *leaf = new_leaf(mrx, &s[1], slen - 1, &vref);
                mask_node_insert_branch(mrx, node, *s, leaf);
                break;
            }
            bm &= b - 1;
            b = bit32_count(bm);
            node = next_block_get_child(nx, b);
        }
        s++;
        slen--;
        level++;
        path[level].node = node;
    }
    if (!path_is_on_stack) {
        free(path);
    }
    return vref;
}

void *
mrx_erase_(mrx_base_t *mrx, // root must be non-null
           const uint8_t string[],
           const unsigned string_length,
           bool *was_erased) // must be non-null
{
    *was_erased = false;
    const uint8_t *s = string;
    unsigned slen = string_length;
    int level = 0;
    union mrx_node *node = mrx->root;
    struct mrx_iterator_path_element *path;

    const uint32_t max_keylen = (mrx->max_keylen_n_flags & ~MRX_FLAGS_MASK_);
    const bool path_is_on_stack = (max_keylen <= MRX_MAX_KEY_LENGTH_FOR_PATH_ON_STACK);
    if (path_is_on_stack) {
        path = alloca((max_keylen + 1) * sizeof(*path));
    } else {
        path = malloc((max_keylen + 1) * sizeof(*path));
    }
    path[0].node = node;
    void *value = NULL;
    for (;;) {
        const uint8_t nsz = HDR_NSZ(node->hdr);
        const uint8_t px_len = HDR_PX_LEN(node->hdr);
        if (px_len > slen || prefix_differs(s, node->sn.octets, px_len)) {
            break;
        }
        s += px_len;
        slen -= px_len;
        if (slen == 0) {
            if (HDR_HAS_VALUE(node->hdr)) {
                value = *mrx_node_value_ref_(node, nsz);
                *was_erased = true;
                erase_value_from_node(mrx, path, level);
                mrx->count--;
            }
            break;
        }
        if (IS_SCAN_NODE(nsz)) {
            const uint8_t br_len = HDR_BR_LEN(node->hdr);
            const uint8_t *br = &node->sn.octets[px_len];
            const int8_t br_pos = find_branch(*s, br, br_len);
            if ((uint8_t)br_pos == br_len) {
                break;
            }
            path[level].br_pos = (int16_t)br_pos;
            path[level].br = *s;
            node = scan_node_get_child(node, br, br_len, br_pos);
        } else {
            path[level].br_pos = *s;
            path[level].br = *s;
            uint32_t b = *s;
            uint32_t bm = node->mn.bitmask.u32[b >> 5u];
            union mrx_next_block *nx = mask_node_get_next_block(node, b);
            b = 1u << (b & 0x1Fu);
            if ((bm & b) == 0) {
                break;
            }
            bm &= b - 1;
            b = bit32_count(bm);
            node = next_block_get_child(nx, b);
        }
        s++;
        slen--;
        level++;
        path[level].node = node;
    };
    if (!path_is_on_stack) {
        free(path);
    }
    return value;
}

void **
mrx_findnear_(union mrx_node *root, // must not be NULL
              const uint8_t string[],
              const unsigned string_length,
              int *match_len) // must be non-null and init to zero
{
    const uint8_t *s = string;
    unsigned slen = string_length;
    union mrx_node *node = root;
    void **match_vref = NULL;

    *match_len = 0;
    for (;;) {
        const uint8_t nsz = HDR_NSZ(node->hdr);
        const uint8_t px_len = HDR_PX_LEN(node->hdr);

        if (px_len > slen) {
            return match_vref;
        }
        if (px_len > 0) {
            if (prefix_differs(s, node->sn.octets, px_len)) {
                return match_vref;
            }
        }
        if (slen == px_len) {
            if (HDR_HAS_VALUE(node->hdr)) {
                *match_len = (int)(string_length - slen + px_len);
                return mrx_node_value_ref_(node, nsz);
            }
            return match_vref;
        }
        if (HDR_HAS_VALUE(node->hdr)) {
            *match_len = (int)(string_length - slen + px_len);
            match_vref = mrx_node_value_ref_(node, nsz);
        }
        if (IS_SCAN_NODE(nsz)) {
            const uint8_t br_len = HDR_BR_LEN(node->hdr);
            const uint8_t *br = &node->sn.octets[px_len];
            const int8_t br_pos = find_branch(s[px_len], br, br_len);
            if ((uint8_t)br_pos == br_len) {
                return match_vref;
            }
            node = scan_node_get_child(node, br, br_len, br_pos);
        } else {
            uint32_t b = s[px_len];
            uint32_t bm = node->mn.bitmask.u32[b >> 5u];
            union mrx_next_block *nx = mask_node_get_next_block(node, b);
            b = 1u << (b & 0x1Fu);
            if ((bm & b) == 0) {
                return match_vref;
            }
            bm &= b - 1;
            b = bit32_count(bm);
            node = next_block_get_child(nx, b);
        }
        s += px_len + 1;
        slen -= px_len + 1;
    }
}

void **
mrx_find_(union mrx_node *root, // must not be NULL
          const uint8_t string[],
          const unsigned string_length)
{
    const uint8_t *s = string;
    unsigned slen = string_length;
    union mrx_node *node = root;

    // parent only used for debugging with -O0, otherwise optimized away
    //union mrx_node *parent = NULL;
    for (;;) {
        const uint8_t nsz = HDR_NSZ(node->hdr);
        const uint8_t px_len = HDR_PX_LEN(node->hdr);

        if (px_len > slen) {
            return NULL;
        }
        if (px_len > 0) {
            if (prefix_differs(s, node->sn.octets, px_len)) {
                return NULL;
            }
        }
        if (slen == px_len) {
            if (HDR_HAS_VALUE(node->hdr)) {
                return mrx_node_value_ref_(node, nsz);
            }
            return NULL;
        }
        if (IS_SCAN_NODE(nsz)) {
            const uint8_t br_len = HDR_BR_LEN(node->hdr);
            const uint8_t *br = &node->sn.octets[px_len];
            const int8_t br_pos = find_branch(s[px_len], br, br_len);
            if ((uint8_t)br_pos == br_len) {
                return NULL;
            }
            //parent = node;
            node = scan_node_get_child(node, br, br_len, br_pos);
        } else {
            uint32_t b = s[px_len];
            uint32_t bm = node->mn.bitmask.u32[b >> 5u];
            union mrx_next_block *nx = mask_node_get_next_block(node, b);
            b = 1u << (b & 0x1Fu);
            if ((bm & b) == 0) {
                return NULL;
            }
            bm &= b - 1;
            b = bit32_count(bm);
            //parent = node;
            node = next_block_get_child(nx, b);
        }
        s += px_len + 1;
        slen -= px_len + 1;
    }
}

// This only makes a real difference if we make find misses with very long strings,
// then we get the advantage of not looking at the whole string to get the length.
void **
mrx_findnt_(union mrx_node *root, // must not be NULL
            const uint8_t string[])
{
    const uint8_t *s = string;
    union mrx_node *node = root;

    for (;;) {
        const uint8_t nsz = HDR_NSZ(node->hdr);
        const uint8_t px_len = HDR_PX_LEN(node->hdr);

        /* as prefix can be assumed to never contain '\0' (or else it would not make
           sense to use findnt()), we can use the normal prefix_differs() function
           which does not specifically look for null termination, when '\0' is
           found in 's', a difference will be found. */
        if (prefix_differs(s, node->sn.octets, px_len)) {
            return NULL;
        }
        const uint8_t branch_octet = s[px_len];
        if (branch_octet == 0) {
            if (HDR_HAS_VALUE(node->hdr)) {
                return mrx_node_value_ref_(node, nsz);
            }
            return NULL;
        }
        if (IS_SCAN_NODE(nsz)) {
            uint8_t br_len = HDR_BR_LEN(node->hdr);
            uint8_t *br = &node->sn.octets[px_len];
            int8_t br_pos = find_branch(branch_octet, br, br_len);
            if ((uint8_t)br_pos == br_len) {
                return NULL;
            }
            node = scan_node_get_child(node, br, br_len, br_pos);
        } else {
            uint32_t b = (uint32_t)branch_octet;
            uint32_t bm = node->mn.bitmask.u32[b >> 5u];
            union mrx_next_block *nx = mask_node_get_next_block(node, b);
            b = 1u << (b & 0x1Fu);
            if ((bm & b) == 0) {
                return NULL;
            }
            bm &= b - 1;
            b = bit32_count(bm);
            node = next_block_get_child(nx, b);
        }
        s += px_len + 1;
    }
}

void
mrx_constructor_(void)
{
    glob.find_branch_fun = mrx_find_branch_generic_;
    glob.find_new_branch_pos_fun = mrx_find_new_branch_pos_generic_;
    glob.prefix_find_first_diff_fun = mrx_prefix_find_first_diff_generic_;
#if __has_builtin(__builtin_cpu_supports)
    if (__builtin_cpu_supports("sse2") != 0) {
        glob.find_branch_fun = mrx_find_branch_sse2_;
        glob.find_new_branch_pos_fun = mrx_find_new_branch_pos_sse2_;
        glob.prefix_find_first_diff_fun = mrx_prefix_find_first_diff_sse2_;
    }
#else
#pragma message("CPU support builtin lacking! May miss important optimizations!")
#endif
}

void
mrx_disable_simd(void)
{
    glob.find_branch_fun = mrx_find_branch_generic_;
    glob.find_new_branch_pos_fun = mrx_find_new_branch_pos_generic_;
    glob.prefix_find_first_diff_fun = mrx_prefix_find_first_diff_generic_;
}

static void __attribute__ ((constructor))
mrx_call_constructor(void)
{
    mrx_constructor_();
}
