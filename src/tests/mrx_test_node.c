/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <assert.h>

#include <bitops.h>
#include <mrx_test_allocator.h>
#include <mrx_test_node.h>
#include <unittest_helpers.h>

#include <mrx_base_int.h>

union mrx_node *
mrx_test_generate_scan_node(mrx_base_t *mrx,
                            bool use_default_lp,
                            bool is_long_pointer,
                            bool has_value,
                            uint8_t px_len,
                            uint8_t br_len,
                            uint32_t taus_state[3])
{
#if ARCH_SIZEOF_PTR == 4
    is_long_pointer = false;
#endif
    if (br_len > 0) {
        br_len = br_len % (SCAN_NODE_MAX_BRANCH_COUNT + 1);
        if (br_len == 0) {
            br_len = 1;
        }
    }
    if (px_len > 126 - br_len) {
        px_len = 126 - br_len; // will be further reduced in loop below
    }

    uint8_t nsz = 5;
    while (nsz == 5) {
        uint8_t minsize = SCAN_NODE_MIN_SIZE(px_len, br_len, has_value);
        if (minsize <= 8) {
            nsz = 0;
            if (ARCH_SIZEOF_PTR > 4) {
                nsz = 1;
            }
        } else if (minsize <= 16) {
            nsz = 1;
        } else if (minsize <= 32) {
            nsz = 2;
        } else if (minsize <= 64) {
            nsz = 3;
        } else if (minsize <= 128) {
            nsz = 4;
        } else {
            if (px_len > 1) {
                px_len--;
            } else if (br_len > 1) {
                br_len--;
            }
        }
    }
    union mrx_node *node = mrx_test_alloc_node_alt(mrx, nsz, use_default_lp);
    HDR_SET(node->hdr, nsz, br_len, false, px_len, has_value);
    *VALUE_REF(node, nsz) = (void *)0xBEEFBEEFBEEFBEEFu;
    int i = 0;
    for (; i < px_len; i++) {
        node->sn.octets[i] = (uint8_t)'a' + i % 26;
    }
    mrx_sp_t *brp = SP_ALIGN(&node->sn.octets[px_len + br_len]);
    union mrx_node *branches[br_len];
    bool has_used_other = false;
    for (int k = 0; k < br_len; k++, i++) {
        node->sn.octets[i] = (uint8_t)'A' + k;
        bool use_default = use_default_lp;
        if (is_long_pointer) {
            use_default = tausrand(taus_state) % 2 == 0;
            if (use_default != use_default_lp) {
                has_used_other = true;
            } else if (k == br_len - 1 && !has_used_other) {
                use_default = !use_default_lp;
            }
        }
        union mrx_node *child = mrx_test_alloc_node_alt(mrx, 1, use_default);
        HDR_SET(child->hdr, 1, 0, false, 0, true);
        *VALUE_REF(child, 1) = (void *)0xBEEFBABEBEEFBABEu;
        branches[k] = child;
        brp[k] = SP(child);
    }
    ptrpfx_convert_to_lp_if_necessary_due_to_branches(mrx, node, branches, br_len);
    mrx_debug_sanity_check_node(node);
    return node;
}

union mrx_node *
mrx_test_generate_mask_node(mrx_base_t *mrx,
                            bool use_default_lp,
                            bool is_long_pointer,
                            bool has_value,
                            bool use_local,
                            uint32_t taus_state[3])
{
#if ARCH_SIZEOF_PTR == 4
    is_long_pointer = false;
#endif
    union mrx_node *node = mrx_test_alloc_node_alt(mrx, 4, use_default_lp);
    HDR_SET(node->hdr, MRX_NODE_MASK_NODE_NSZ_VALUE_, 0, false, 0, has_value);
    node->mn.used = 0;

    int block_set_count = 0;
    if (tausrand(taus_state) % 100 == 0 && !use_local) {
        // special case: all bits set
        for (unsigned i = 0; i < 8; i++) {
            node->mn.bitmask.u32[i] = 0xFFFFFFFFu;
        }
        node->mn.used = 0xFFu;
    } else {
        for (unsigned i = 0; i < 8; i++) {
            node->mn.bitmask.u32[i] = 0;
            if (tausrand(taus_state) % 2 == 0 || (block_set_count == 0 && i == 7)) {
                if (tausrand(taus_state) % 10 == 0) {
                    // special case: only one bit set in the block
                    if (tausrand(taus_state) % 2 == 0) {
                        // first bit is "dangerous" as it can lead to conflicts of node header in buggy code
                        bit32_set(&node->mn.bitmask.u32[i], 0);
                    } else {
                        bit32_set(&node->mn.bitmask.u32[i], tausrand(taus_state) % 32);
                    }
                } else if (tausrand(taus_state) % 10 == 0) {
                    // special case: 27 - 32 bits set in the block
                    node->mn.bitmask.u32[i] = 0xFFFFFFFFu;
                    unsigned k = tausrand(taus_state) % 5;
                    for (unsigned j = 0; j < k; j++) {
                        bit32_unset(&node->mn.bitmask.u32[i], tausrand(taus_state) % 32);
                    }
                } else {
                    for (unsigned j = 0; j < 32; j++) {
                        if (tausrand(taus_state) % 4 == 0 || (node->mn.bitmask.u32[i] == 0 && j == 31)) {
                            bit32_set(&node->mn.bitmask.u32[i], j);
                        }
                    }
                }
                block_set_count++;
                node->mn.used |= 1u << i;
            }
        }
    }
    if (block_set_count == 1) {
        // make sure that at least two bits are set
        unsigned i = bit32_bsf(node->mn.used);
        while (bit32_count(node->mn.bitmask.u32[i]) == 1) {
            bit32_set(&node->mn.bitmask.u32[i], tausrand(taus_state) % 32);
        }
    }

    union mrx_next_block *np[8]; // pointers to next blocks
    int block_set_count2 = 0;
    bool local_assigned = false;
    bool has_used_longptr = false;
    node->mn.branch_count = 0;
    for (unsigned i = 0; i < 8; i++) {
        if (node->mn.bitmask.u32[i] == 0) {
            np[i] = NULL;
            continue;
        }
        block_set_count2++;
        if (use_local && block_set_count2 == block_set_count && !local_assigned) {
            // if we haven't fitted into local block yet make sure we can do now
            while (bit32_count(node->mn.bitmask.u32[i]) > MASK_NODE_MAX_LOCAL_BRANCH_COUNT ||
                   bit32_count(node->mn.bitmask.u32[i]) < 1)
            {
                node->mn.bitmask.u32[i] = tausrand(taus_state);
            }
        }
        if (!use_local && !is_long_pointer) {
            // make sure that next block doesn't have too few pointers,
            while (bit32_count(node->mn.bitmask.u32[i]) <= MASK_NODE_LOCAL_BRANCH_COUNT_FOR_MOVING_BACK) {
                node->mn.bitmask.u32[i] = tausrand(taus_state);
            }
        }

        bool maybe_longptr = use_default_lp;
        if (is_long_pointer) {
            maybe_longptr = tausrand(taus_state) % 2 == 0;
            if (maybe_longptr != use_default_lp) {
                has_used_longptr = true;
            }
            if (!has_used_longptr && block_set_count2 == block_set_count) {
                maybe_longptr = !use_default_lp;
            }
        }


        // make next block
        uint8_t bc = bit32_count(node->mn.bitmask.u32[i]);
        node->mn.branch_count += bc;
#if ARCH_SIZEOF_PTR == 4
        bool lp_nx = false;
#else
        bool lp_nx = tausrand(taus_state) % 2 == 0;
#endif
        uint8_t nsz;
        if (bc > 16 || lp_nx) {
            nsz = 4;
        } else if (bc > 8) {
            nsz = 3;
        } else if (bc > 4) {
            nsz = 2;
        } else if (bc > 2) {
            nsz = 1;
        } else {
            nsz = ARCH_SIZEOF_PTR > 4 ? 1 : 0;
        }
        union mrx_next_block *nx;
        if (!is_long_pointer &&
            use_local &&
            !local_assigned &&
            bc <= MASK_NODE_MAX_LOCAL_BRANCH_COUNT)
        {
            local_assigned = true;
            lp_nx = false;
            nx = (union mrx_next_block *)node->mn.local;
        } else {
            nx = mrx_test_alloc_node_alt(mrx, nsz, maybe_longptr);
        }

        // make child pointers
        union mrx_node *nodeptrs[32];
        bool nx_has_used_lp = false;
        for (uint8_t k = 0; k < bc; k++) {
            bool maybe_lp = lp_nx ? tausrand(taus_state) % 2 == 0 : maybe_longptr;
            if (lp_nx && k == bc - 1 && !nx_has_used_lp) {
                maybe_lp = !maybe_longptr;
            }
            if (maybe_lp != maybe_longptr) {
                nx_has_used_lp = true;
            }
            union mrx_node *child = mrx_test_alloc_node_alt(mrx, 1, maybe_lp);
            HDR_SET(child->hdr, 1, 0, false, 0, true);
            *VALUE_REF(child, 1) = (void *)0xBEEFBABEBEEFBABEu;
            nodeptrs[k] = child;
        }

        np[i] = nx;
        if (lp_nx) {
            if (bc > 14) {
                union mrx_next_block *mid = alloc_node(mrx, 4);
                if (bc > 28) {
                    union mrx_next_block *top = alloc_node(mrx, 3);
                    memcpy(top->lp.lp, &nodeptrs[28], (bc - 28) * sizeof(uintptr_t));
                    memcpy(mid->lp.lp, &nodeptrs[14], 14 * sizeof(uintptr_t));
                    mid->lp.next = top;
                } else {
                    memcpy(mid->lp.lp, &nodeptrs[14], (bc - 14) * sizeof(uintptr_t));
                    mid->lp.next = NULL;
                }
                memcpy(nx->lp.lp, nodeptrs, 14 * sizeof(uintptr_t));
                nx->lp.next = mid;
            } else {
                memcpy(nx->lp.lp, nodeptrs, bc * sizeof(uintptr_t));
                nx->lp.next = NULL;
            }
            NEXT_BLOCK_HDR_SET(nx, NEXT_BLOCK_LONG_PTR_HDR);
            nx->lp.lp_count = 0;
            for (uint8_t k = 0; k < bc; k++) {
                if (HP(nodeptrs[k]) != HP(nx)) {
                    nx->lp.lp_count++;
                }
            }
        } else {
            for (uint8_t k = 0; k < bc; k++) {
                nx->sp[k] = SP(nodeptrs[k]);
            }
            NEXT_BLOCK_HDR_SET(nx, nsz);
        }
    }
    assert(node->mn.branch_count != 1);

    if (has_value) {
        node->mn.value = (void *)0xBEEFBEEFBEEFBEEFu;
        HDR_SET_HAS_VALUE(node->mn.hdr);
    } else {
        node->mn.value = NULL;
    }
    HDR_MN_SET_LOCAL_USED(node->mn.hdr, local_assigned);
    for (uint8_t i = 0; i < 8; i++) {
        node->mn.next[i] = SP(np[i]);
    }
    if (is_long_pointer) {
        HDR_SET_LONGPTR(node->hdr);
        int lp_count = 0;
        for (uint8_t i = 0; i < 8; i++) {
            node->mn.local[i] = HP(np[i]);
            if (np[i] != NULL && HP(np[i]) != HP(node)) {
                lp_count++;
            }
        }
        node->mn.lp_count = lp_count;
    }
    mrx_debug_sanity_check_node(node);
    //mrx_print_node_(node);
    return node;
}
