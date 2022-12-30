/*
 * Copyright (c) 2013, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <assert.h>

#include <mrx_base_int.h>

static struct mrx_ptrpfx_node *
alloc_and_attach_ptrpfx_node(mrx_base_t *mrx,
                             union mrx_node *node)
{
    const uint8_t nsz = HDR_NSZ(node->hdr);

    // for small nodes we need extra space in the prefix node to fit all info in worst case
    const uint8_t pfx_nsz = nsz <= 2 ? nsz + 1 : (nsz > 4 ? 4 : nsz);

    struct mrx_ptrpfx_node *pfx = alloc_node(mrx, pfx_nsz);
    HDR_SET(pfx->hdr, pfx_nsz, 0, false, 0, false);
    pfx->lp_count = 0;
    pfx->orig.value = *VALUE_REF(node, nsz);
    *VALUE_REF(node, nsz) = (void *)pfx;
    HDR_SET_LONGPTR(node->hdr);

    // this assert checks that the caller has made sure that the start of branch pointers does not start past pointer prefix pointer
    assert((uintptr_t)SP_ALIGN(&node->sn.octets[HDR_PX_LEN(node->hdr)+HDR_BR_LEN(node->hdr)]) <= (uintptr_t)VALUE_REF(node, nsz));

    return pfx;
}

static union mrx_next_block *
mask_node_move_out_local_block(mrx_base_t *mrx,
                               union mrx_node *node,
                               const uint8_t local_bc)
{
    union mrx_next_block *nx = alloc_node(mrx, 4);
    if (PTR_PREFIX_MATCHES(nx, node)) {
        memcpy(nx->sp, node->mn.local, local_bc * sizeof(mrx_sp_t));
        NEXT_BLOCK_HDR_SET(nx, 4);
    } else {
        NEXT_BLOCK_HDR_SET(nx, NEXT_BLOCK_LONG_PTR_HDR);
        nx->lp.lp_count = local_bc;
        uint8_t k;
        for (k = 0; k < local_bc; k++) {
            nx->lp.lp[k] = EP(node->mn.local[k], node);
        }
        nx->lp.next = NULL;
    }
    return nx;
}

// input: target is short ptr, source is long pointer or target/source mismatch
void
mrx_ptrpfx_sn_copy_branch_ptrs_and_value_slowpath_(mrx_base_t *mrx,
                                                   union mrx_node *target,
                                                   union mrx_node *source,
                                                   const uint8_t t_px_len,
                                                   const uint8_t s_px_len,
                                                   const uint8_t br_len)
{
    union mrx_node *branches[br_len];
    const uint8_t s_nsz = HDR_NSZ(source->hdr);
    const uint8_t t_nsz = HDR_NSZ(target->hdr);
    mrx_sp_t *s_brp = SP_ALIGN(&source->sn.octets[s_px_len + br_len]);
    mrx_sp_t *t_brp = SP_ALIGN(&target->sn.octets[t_px_len + br_len]);
    uint8_t i;
    for (i = 0; i < br_len; i++) {
        branches[i] = scan_node_get_child1(source, s_brp, i);
    }
    for (i = 0; i < br_len; i++) {
        t_brp[i] = SP(branches[i]);
    }
    if (HDR_HAS_VALUE(source->hdr)) {
        HDR_SET_HAS_VALUE(target->hdr);
        *VALUE_REF(target, t_nsz) = *mrx_node_value_ref_(source, s_nsz);
    }
    ptrpfx_convert_to_lp_if_necessary_due_to_branches(mrx, target, branches, br_len);
}

// input: node is long ptr, branch array extend into prefix pointer, br_len is unchanged
void
mrx_ptrpfx_sn_move_branch_ptrs_slowpath_(union mrx_node *node,
                                         const uint8_t new_px_len,
                                         const uint8_t old_px_len,
                                         const uint8_t br_len)
{
    struct mrx_ptrpfx_node *pfx = SN_PREFIX_PTR(node);
    const uint8_t nsz = HDR_NSZ(node->hdr);
    uint8_t *br = &node->sn.octets[old_px_len];
    mrx_sp_t *brp = SP_ALIGN(&br[br_len]);
    union mrx_node *branches[br_len];
    uint8_t i;
    for (i = 0; i < br_len; i++) {
        branches[i] = scan_node_get_child1(node, brp, i);
    }
    br = &node->sn.octets[new_px_len];
    brp = SP_ALIGN(&br[br_len]);
    for (i = 0; i < br_len; i++) {
        brp[i] = SP(branches[i]);
    }
    pfx->orig.value = *VALUE_REF(node, nsz);
    *VALUE_REF(node, nsz) = (void *)pfx;
}

// input: node is longptr
void
mrx_ptrpfx_sn_free_restore(mrx_base_t *mrx,
                           union mrx_node *node)
{
    struct mrx_ptrpfx_node *pfx = SN_PREFIX_PTR(node);
    *VALUE_REF(node, HDR_NSZ(node->hdr)) = pfx->orig.value;
    free_node(mrx, pfx, HDR_NSZ(pfx->hdr));
    HDR_CLR_LONGPTR(node->hdr);
}

// input: source is longptr
void
mrx_ptrpfx_sn_copy_to_short_slowpath_(union mrx_node *target,
                                      union mrx_node *source)
{
    uint8_t nsz = HDR_NSZ(source->hdr);
    FLATTEN_SN_NSZ(nsz);
    struct mrx_ptrpfx_node *pfx = SN_PREFIX_PTR(source);
    memcpy(target, source, 8u << nsz);
    *VALUE_REF(target, nsz) = pfx->orig.value;
    HDR_CLR_LONGPTR(target->hdr);
}

// input: node is fresh short ptr (pointers and brlen set etc),
// at least one mismatching branch
void
mrx_ptrpfx_convert_to_lp_due_to_branches_(mrx_base_t *mrx,
                                          union mrx_node *node,
                                          union mrx_node *branches[],
                                          const uint8_t br_len)
{
    assert(HDR_IS_SHORT_PTR(node->hdr));
    struct mrx_ptrpfx_node *pfx = alloc_and_attach_ptrpfx_node(mrx, node);
    for (uint8_t i = 0; i < br_len; i++) {
        pfx->hp[i] = HP(branches[i]);
        if (!PTR_PREFIX_MATCHES(node, branches[i])) {
            pfx->lp_count++;
        }
    }
    assert(pfx->lp_count > 0);
}


// input: either prefix mismatch or long pointer in node
void
mrx_ptrpfx_mn_insert_nx_ptr_slowpath_(mrx_base_t *mrx,
                                      union mrx_node *node,
                                      union mrx_next_block *nx,
                                      const uint8_t br,
                                      // replaced nx block is freed!
                                      const int is_replacement)
{
    const unsigned bli = (unsigned)br >> 5u;
    if (HDR_IS_SHORT_PTR(node->hdr)) {
        // make long ptr
        union mrx_next_block *lnx = NULL;
        int lnx_pos = -1;
        if (HDR_MN_LOCAL_USED(node->mn.hdr)) {
            // has local storage, must move out
            HDR_MN_SET_LOCAL_USED(node->mn.hdr, false);
            for (unsigned i = 0; i < 8; i++) {
                if (node->mn.next[i] == SP(node->mn.local)) {
                    if (!is_replacement || i != bli) {
                        const uint8_t bc = bit32_count(node->mn.bitmask.u32[i]);
                        lnx = mask_node_move_out_local_block(mrx, node, bc);
                        node->mn.next[i] = SP(lnx);
                        lnx_pos = (int)i;
                    }
                    break;
                }
            }
        }
        for (unsigned i = 0; i < 8; i++) {
            node->mn.local[i] = HP(node);
        }
        if (is_replacement && node->mn.next[bli] != SP(node->mn.local)) {
            assert(EP(node->mn.next[bli], node) != nx);
            free_nx_node(mrx, EP(node->mn.next[bli], node));
        }
        node->mn.next[bli] = SP(nx);
        node->mn.local[bli] = HP(nx);
        node->mn.lp_count = 1;
        if (lnx != NULL && !PTR_PREFIX_MATCHES(node, lnx)) {
            node->mn.lp_count = 2;
            node->mn.local[lnx_pos] = HP(lnx);
        }
        HDR_SET_LONGPTR(node->mn.hdr);
        return;
    }

    // node is long ptr
    if (PTR_PREFIX_MATCHES(node, nx)) {
        if (is_replacement) {
            union mrx_next_block *oldnx = mask_node_get_next_block(node, br);
            assert(oldnx != nx);
            free_nx_node(mrx, oldnx);
            if (!PTR_PREFIX_MATCHES(node, oldnx)) {
                node->mn.lp_count--;
                if (node->mn.lp_count == 0) {
                    // make short ptr
                    HDR_CLR_LONGPTR(node->mn.hdr);
                }
            }
            node->mn.next[bli] = SP(nx);
            if (node->mn.lp_count > 0) {
                node->mn.local[bli] = HP(nx);
            }
            mrx_mask_node_try_move_to_local_block_(mrx, node);
        } else {
            node->mn.next[bli] = SP(nx);
            node->mn.local[bli] = HP(nx);
        }
    } else {
        if (is_replacement) {
            union mrx_next_block *oldnx = mask_node_get_next_block(node, br);
            assert(oldnx != nx);
            free_nx_node(mrx, oldnx);
            if (PTR_PREFIX_MATCHES(node, oldnx)) {
                node->mn.lp_count++;
            }
        } else {
            node->mn.lp_count++;
        }
        node->mn.next[bli] = SP(nx);
        node->mn.local[bli] = HP(nx);
    }
}

// Input requirement: level >= 0, parent (or next block in case of mask node) is lp or child hp is not matching parent's hp
void
mrx_set_link_in_parent_slowpath_(mrx_base_t *mrx,
                                 struct mrx_iterator_path_element path[],
                                 int level,
                                 union mrx_node *child)
{
    union mrx_node *parent = path[level].node;
    const uint8_t nsz = HDR_NSZ(parent->hdr);
    if (IS_SCAN_NODE(nsz)) {
        const uint8_t px_len = HDR_PX_LEN(parent->hdr);
        const uint8_t br_len = HDR_BR_LEN(parent->hdr);
        mrx_sp_t *brp = SP_ALIGN(&parent->sn.octets[px_len+br_len]);
        if (HDR_IS_SHORT_PTR(parent->hdr)) {
            brp[path[level].br_pos] = SP(child);
            assert(!PTR_PREFIX_MATCHES(parent, child)); // the other case should be fastpath handled
            union mrx_node *branches[br_len];
            for (uint8_t i = 0; i < br_len; i++) {
                if (i == path[level].br_pos) {
                    branches[i] = child;
                } else {
                    branches[i] = EP(brp[i], parent);
                }
            }
            mrx_ptrpfx_convert_to_lp_due_to_branches_(mrx, parent, branches, br_len);
        } else {
            struct mrx_ptrpfx_node **pfxref = &SN_PREFIX_PTR(parent);
            mrx_sp_t *node_lo = &brp[path[level].br_pos];
            if ((uintptr_t)node_lo >= (uintptr_t)pfxref) {
                const unsigned idx = ((uintptr_t)node_lo - (uintptr_t)pfxref) >> 2u;
                node_lo = &(*pfxref)->orig.sp[idx];
            }
            if (!PTR_PREFIX_MATCHES(parent, EP2(*node_lo, (*pfxref)->hp[path[level].br_pos]))) {
                (*pfxref)->lp_count--;
            }
            *node_lo = SP(child);
            (*pfxref)->hp[path[level].br_pos] = HP(child);
            if (!PTR_PREFIX_MATCHES(parent, child)) {
                (*pfxref)->lp_count++;
            }
            if ((*pfxref)->lp_count == 0) {
                mrx_ptrpfx_sn_free_restore(mrx, parent);
            }
        }
    } else {
        uint32_t b = path[level].br;
        union mrx_next_block *nx = mask_node_get_next_block(parent, b);
        uint32_t bm = parent->mn.bitmask.u32[b >> 5u];
        b = 1u << (b & 0x1Fu);
        parent->mn.branch_count += ((bm & b) == 0); // if new branch, add 1
        bm &= b - 1;
        b = bit32_count(bm);
        if (NEXT_BLOCK_IS_SHORT_PTR(nx)) {
            assert(!PTR_PREFIX_MATCHES(nx, child)); // other case should be fastpath handled
            const uint8_t bc = bit32_count(parent->mn.bitmask.u32[path[level].br >> 5u]);
            union mrx_next_block *newnx = mrx_alloc_nx_node_lp_(mrx, bc);
            if (!PTR_PREFIX_MATCHES(nx, newnx)) {
                newnx->lp.lp_count += bc - 1;
            }
            if (!PTR_PREFIX_MATCHES(newnx, child)) {
                newnx->lp.lp_count++;
            }
            if (newnx->lp.lp_count == 0) {
                // can only happen for bc == 1
                assert(bc == 1);
                newnx->lp.lp[0] = child;
                newnx = mrx_nx_lp_to_sp_(mrx, newnx, bc);
            } else {
                union mrx_next_block *nxb = newnx;
                uint8_t ofs = 0;
                for (uint8_t i = 0; i < bc; i++) {
                    if (i == NEXT_BLOCK_MAX_LP_COUNT || i == 2 * NEXT_BLOCK_MAX_LP_COUNT) {
                        nxb = nxb->lp.next;
                        ofs += NEXT_BLOCK_MAX_LP_COUNT;
                    }
                    if (i != b) {
                        nxb->lp.lp[i-ofs] = EP(nx->sp[i], nx);
                    } else {
                        nxb->lp.lp[i-ofs] = child;
                    }
                }
            }
            ptrpfx_mn_insert_nx_ptr(mrx, parent, newnx, path[level].br, 1);
        } else {
            union mrx_next_block *nxb = nx;
            while (b >= NEXT_BLOCK_MAX_LP_COUNT) {
                nxb = nxb->lp.next;
                b -= NEXT_BLOCK_MAX_LP_COUNT;
            }
            if (!PTR_PREFIX_MATCHES(nx, nxb->lp.lp[b])) {
                nx->lp.lp_count--;
            }
            nxb->lp.lp[b] = child;
            if (!PTR_PREFIX_MATCHES(nx, child)) {
                nx->lp.lp_count++;
            }
            if (nx->lp.lp_count == 0) {
                const unsigned bli = path[level].br >> 5u;
                const uint8_t bc = bit32_count(parent->mn.bitmask.u32[bli]);
                // prefix is guaranteed to be kept
                nx = mrx_nx_lp_to_sp_(mrx, nx, bc);
                parent->mn.next[bli] = SP(nx);
                if (!HDR_IS_SHORT_PTR(parent->hdr)) {
                    parent->mn.local[bli] = HP(nx);
                } else if (bc <= MASK_NODE_MAX_LOCAL_BRANCH_COUNT) {
                    mrx_mask_node_try_move_to_local_block_(mrx, parent);
                }
            }
        }
    }
}

void
mrx_mask_node_try_move_to_local_block_(mrx_base_t *mrx,
                                       union mrx_node *node)
{
    if (!HDR_IS_SHORT_PTR(node->hdr) || HDR_MN_LOCAL_USED(node->hdr)) {
        return;
    }
    for (unsigned i = 0; i < 8; i++) {
        if (node->mn.bitmask.u32[i] == 0) {
            continue;
        }
        const unsigned bc = bit32_count(node->mn.bitmask.u32[i]);
        if (bc <= MASK_NODE_MAX_LOCAL_BRANCH_COUNT) {
            union mrx_next_block *nx = mask_node_get_next_block(node, i << 5u);
            // In theory we could also move a long pointer nx block if all its pointers
            // are matching the base node, but that case is so rare that we ignore it.
            // Moving to local is just space optimization anyway.
            if (PTR_PREFIX_MATCHES(nx, node) && NEXT_BLOCK_IS_SHORT_PTR(nx)) {
                HDR_MN_SET_LOCAL_USED(node->mn.hdr, true);
                node->mn.next[i] = SP(node->mn.local);
                memcpy(node->mn.local, nx, bc * sizeof(mrx_sp_t));
                NEXT_BLOCK_HDR_SET((union mrx_next_block *)node->mn.local, 0);
                free_nx_node(mrx, nx);
                return;
            }
        }
    }
}

void
mrx_free_nx_node_slowpath_(mrx_base_t *mrx,
                           union mrx_next_block *nx,
                           const bool keep_first)
{
    assert(!NEXT_BLOCK_IS_SHORT_PTR(nx));
    union mrx_next_block *mid = nx->lp.next;
    if (!keep_first) {
        free_node(mrx, nx, 4);
    }
    if (mid != NULL) {
        union mrx_next_block *top = mid->lp.next;
        free_node(mrx, mid, 4);
        if (top != NULL) {
            free_node(mrx, top, 3);
        }
    }
}

union mrx_next_block *
mrx_alloc_nx_node_lp_(mrx_base_t *mrx,
                      const uint8_t capacity)
{
#if ARCH_SIZEOF_PTR != 8
    abort();
#endif
    union mrx_next_block *nx = alloc_node(mrx, 4);
    NEXT_BLOCK_HDR_SET(nx, NEXT_BLOCK_LONG_PTR_HDR);
    nx->lp.lp_count = 0;
    if (capacity <= NEXT_BLOCK_MAX_LP_COUNT) {
        nx->lp.next = NULL;
        return nx;
    }
    nx->lp.next = alloc_node(mrx, 4);
    union mrx_next_block *mid = nx->lp.next;
    if (capacity <= 2 * NEXT_BLOCK_MAX_LP_COUNT) {
        mid->lp.next = NULL;
        return nx;
    }
    mid->lp.next = alloc_node(mrx, 3);
    assert(capacity <= 32);
    return nx;
}

/*
  Convert a 'next block' with long pointers to a short pointer next block.
  A next block needs to contain up to 32 pointers. Short pointers (4 byte)
  fit all into a 128 byte node, with size/alloc flags in 3 free bits of
  first pointer.

  Long pointer next blocks still use 128 byte nodes but to fit 32 pointers
  they can be chained up to three blocks (bottom, middle, top). The bottom
  and middle are 128 bytes and fit 14 pointers each, the top is 64 bytes
  and holds 4 pointers to get to the total of 32. The top could be 32
  bytes if the header was skipped, but due to the rare case code is kept
  simpler by having the same layout on all nodes

  The number of pointers needed, the branch count, is specified with the
  'bc' parameter
 */
union mrx_next_block *
mrx_nx_lp_to_sp_(mrx_base_t *mrx,
                 union mrx_next_block *nx,
                 const uint8_t bc)
{
#if ARCH_SIZEOF_PTR != 8
    abort();
#endif

    union mrx_next_block *bot = nx;

    //                           111111
    //                           5432109876543210
    const uint64_t bc_to_nsz = 0x4443333333222111;
    uint8_t new_nsz = bc > 15 ? 4 : (bc_to_nsz >> ((unsigned)bc << 2u) & 0xFu);
    if (new_nsz != 4) {
        union mrx_next_block *newnx = alloc_node(mrx, new_nsz);
        if (PTR_PREFIX_MATCHES(newnx, nx)) {
            nx = newnx;
        } else {
            // prefix must match; give and up use original instead
            free_node(mrx, newnx, new_nsz);
            new_nsz = 4;
        }
    }

    // transfer pointers
    unsigned i = 0;
    for (; i < bc && i < NEXT_BLOCK_MAX_LP_COUNT; i++) {
        nx->sp[i] = SP(bot->lp.lp[i]);
    }
    if (bot->lp.next != NULL) {
        union mrx_next_block *mid = bot->lp.next;
        for (; i < bc && i < 2 * NEXT_BLOCK_MAX_LP_COUNT; i++) {
            nx->sp[i] = SP(mid->lp.lp[i-NEXT_BLOCK_MAX_LP_COUNT]);
        }
        if (mid->lp.next != NULL) {
            union mrx_next_block *top = mid->lp.next;
            for (; i < bc; i++) {
                nx->sp[i] = SP(top->lp.lp[i - 2 * NEXT_BLOCK_MAX_LP_COUNT]);
            }
            free_node(mrx, top, 3);
        }
        free_node(mrx, mid, 4);
    }
    if (bot != nx) {
        free_node(mrx, bot, 4);
    }
    NEXT_BLOCK_HDR_SET(nx, new_nsz);
    return nx;
}
