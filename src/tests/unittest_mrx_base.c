/*
 * Copyright (c) 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <mrx_test_allocator.h>
#include <mrx_test_node.h>
#include <unittest_helpers.h>

#define MRB_PRESET_const_str_TO_REF_COPY_KEY
#include <mrb_tmpl.h>

#include <mrx_tmpl.h>
#include <mrx_scan.h>

// include c file directly to give access to static functions
#include <mrx_base.c>

static uint32_t taus_state[3];

// check that all allocated nodes in test allocator is present in tree
static void
test_memleak_and_clear_memory(mrx_base_t *mrx)
{
    //fprintf(stderr, "allocated nodes before tree is freed %zu\n", mrx_test_currently_allocated_nodes());
    mrx_traverse_erase_all_nodes_(mrx);
    //fprintf(stderr, "allocated nodes after tree is freed  %zu\n", mrx_test_currently_allocated_nodes());
    ASSERT(mrx_test_currently_allocated_nodes() == 0);
    mrx_test_clear_allocator(mrx);
    mrx->root = NULL;
}

static union mrx_node *
get_child(union mrx_node *node,
          uint8_t branch_octet)
{
    if (IS_MASK_NODE(HDR_NSZ(node->hdr))) {
        uint32_t b = branch_octet;
        uint32_t bm = node->mn.bitmask.u32[b >> 5u];
        union mrx_next_block *nx = mask_node_get_next_block(node, b);
        b = 1u << (b & 0x1Fu);
        ASSERT((bm & b) != 0);
        bm &= b - 1;
        b = bit32_count(bm);
        return next_block_get_child(nx, b);
    }
    int8_t br_pos = mrx_find_branch_generic_(branch_octet,
                                             &node->sn.octets[HDR_PX_LEN(node->hdr)],
                                             HDR_BR_LEN(node->hdr));
    ASSERT(br_pos != -1);
    return scan_node_get_child(node, &node->sn.octets[HDR_PX_LEN(node->hdr)], HDR_BR_LEN(node->hdr), br_pos);
}

static void
free_node_and_its_children(union mrx_node *node)
{
    mrx_base_t dummy_mrx;
    dummy_mrx.root = node;
    mrx_traverse_erase_all_nodes_(&dummy_mrx);
}

static void
replace_link_in_parent(mrx_base_t *mrx,
                       struct mrx_iterator_path_element path[],
                       int level,
                       union mrx_node *child)
{
    union mrx_node *parent = path[level].node;
    uint32_t b = path[level].br;
    union mrx_node *old_node = get_child(parent, b);
    free_node_and_its_children(old_node);
    set_link_in_parent(mrx, path, level, child);
}

static int
get_nodeptrs(union mrx_node *nodeptrs[], int bro[], union mrx_node *node)
{
    int br_len;
    uint8_t nsz = HDR_NSZ(node->hdr);
    if (IS_SCAN_NODE(nsz)) {
        br_len = HDR_BR_LEN(node->hdr);
        int px_len = HDR_PX_LEN(node->hdr);
        for (int j = 0; j < br_len; j++) {
            bro[j] = (&node->sn.octets[px_len])[j];
            nodeptrs[j] = scan_node_get_child(node, &node->sn.octets[px_len], br_len, j);
        }
    } else {
        br_len = 0;
        for (unsigned i = 0; i < 256; i++) {
            if (bit32_isset(node->mn.bitmask.u32, i)) {
                bro[br_len] = i;
                nodeptrs[br_len] = get_child(node, i);
                br_len++;
            }
        }
    }

    // check that there are no duplicates or NULL ptrs
    for (int j = 0; j < br_len; j++) {
        ASSERT(nodeptrs[j] != NULL);
        for (int k = 0; k < br_len; k++) {
            if (j == k) {
                continue;
            }
            ASSERT(bro[k] != bro[j]);
            ASSERT(nodeptrs[k] != nodeptrs[j]);
        }
    }
    return br_len;
}

static uint8_t
minimum_move_px_len_for_new_branch_ref_impl(union mrx_node *node,
                                            uint8_t br[],
                                            uint8_t br_len)
{
    const uint8_t nsz = 4;
    assert(HDR_NSZ(node->hdr) == 4);
    void **node_end = NODE_END_REF(node, nsz);
    uint8_t *br_end = &br[br_len];
    mrx_sp_t *brp_end = &SP_ALIGN(br_end)[br_len];

    node_end -= !!HDR_HAS_VALUE(node->hdr);

    uint8_t px_len = HDR_PX_LEN(node->hdr);
    uint8_t min_px_len;

    if (brp_end == (mrx_sp_t *)node_end) {
        // add space to fit extra pointer
        min_px_len = sizeof(mrx_sp_t);
    } else {
        min_px_len = 0;
    }
    if (((uintptr_t)br_end & 3u) == 0) {
        // move one extra to avoid alignment step-up of branch array
        min_px_len++;
    }
    if (min_px_len > px_len) {
        assert(min_px_len <= px_len + (4 - ((uintptr_t)br_end & 3u)));
        min_px_len = px_len;
    } else {
        // optimization: avoid only one prefix byte left as it then cannot
        // convert to mask node without having to do an extra move
        if (px_len - min_px_len == 1) {
            min_px_len = px_len;
        }
    }
    return min_px_len;
}

/*
  Code coverage of static functions in mrx_base.c:

  + set_link_in_parent
  - prefix_lengthen_in_parent (through prefix_move_up())
  - prefix_shorten (through prefix_move_up())
  + minimum_move_px_len_for_new_branch
  + prefix_move_up
  + enlarge_node
  + scan_node_child_merge
  - sn_erase_branch (htrough scan_node_erase_branch())
  + scan_node_erase_branch
  - mask_node_to_scan_node_branch (through mask_node_erase_branch())
  + mask_node_erase_branch
  + erase_value_from_node
  + mask_node_insert_branch
  - sn_insert_branch (through scan_node_insert_branch)
  + scan_node_to_mask_node
  + scan_node_insert_branch
  + scan_node_insert_value
  + new_leaf
  + scan_node_split

  Code coverage in mrx_ptrpfx.c 100% as side effect of all tests
 */

static void
test_base_macros_and_small_functions(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: mrb_base_int.h macros...");
    {
        // test HDR_HAS_VALUE()
        {
            uint16_t hdr = tausrand(taus_state) & ~HDR_BIT_HAS_VALUE;
            uint16_t hdr_with_value = hdr | HDR_BIT_HAS_VALUE;
            uint16_t hdr_without_value = hdr & ~HDR_BIT_HAS_VALUE;
            // must return 0 or 1 (rather than zero or non-zero) as it is used as multiplier sometimes
            ASSERT(HDR_HAS_VALUE(hdr_with_value) == 1);
            ASSERT(HDR_HAS_VALUE(hdr_without_value) == 0);
        }

        // test SCAN_NODE_MIN_SIZE() and scan_node_min_size_nsz();
        for (unsigned has_value = 0; has_value <= 1; has_value++) {
            for (unsigned px_len = 0; px_len < 128; px_len++) {
                for (unsigned br_len = 0; br_len < 128; br_len++) {
                    unsigned totlen = SCAN_NODE_MIN_SIZE(px_len, br_len, has_value);

                    // header + prefix + br_len + 4 byte alignment
                    unsigned expected_len = (2u + px_len + br_len + 3u) & ~3u;
                    if (sizeof(void *) == 8) {
                        if (has_value) {
                            expected_len += br_len * sizeof(mrx_sp_t); // branch short pointers
                            expected_len = (expected_len + 7u) & ~7u; // align to 8 bytes
                            expected_len += sizeof(void *); // value
                        } else {
                            // cover the pointer prefix case

                            // minimum size with pointer prefix pointer
                            unsigned lp_len = ((expected_len + 7u) & ~7u) + sizeof(void *);

                            // "free" branch short pointers we get with the minimum size
                            unsigned free_brp_count = (lp_len - expected_len) / sizeof(mrx_sp_t);
                            if (br_len <= free_brp_count) {
                                // minimimum size is enough
                                expected_len = lp_len;
                            } else {
                                expected_len += br_len * sizeof(mrx_sp_t);
                                expected_len = (expected_len + 7u) & ~7u;
                            }
                        }
                    } else {
                        if (has_value) {
                            expected_len += sizeof(void *);
                        }
                        expected_len += br_len * sizeof(mrx_sp_t);
                    }
                    if (expected_len > sizeof(union mrx_node) + 32) {
                        // ignore checking sizes way belong max node size
                        continue;
                    }
                    //fprintf(stderr, "px_len %d, br_len %d, has_value %d => min size %d\n", px_len, br_len, has_value, totlen);
                    ASSERT(totlen == expected_len);
                    unsigned expected_nsz;
                    if (expected_len <= 8) {
                        expected_nsz = 0;
                    } else if (expected_len <= 16) {
                        expected_nsz = 1;
                    } else if (expected_len <= 32) {
                        expected_nsz = 2;
                    } else if (expected_len <= 64) {
                        expected_nsz = 3;
                    } else if (expected_len <= 128) {
                        expected_nsz = 4;
                    } else {
                        expected_nsz = 5;
                    }
                    uint8_t nsz = scan_node_min_size_nsz(px_len, br_len, has_value);
                    ASSERT(nsz == expected_nsz);
                }
            }
        }
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: MIN_MOVE_PX_LEN_FOR_VALUE() and minimum_move_px_len_for_new_branch()...");
    {
        const int test_size = 10;
        for (int i = 0; i < test_size; i++) {
            for (int br_len = 0; br_len <= SCAN_NODE_MAX_BRANCH_COUNT; br_len++) {
                for (int px_len = 0; px_len < 128 - br_len; px_len++) {
                    mrx_test_configure_allocator(true, true);
                    union mrx_node *node = mrx_test_generate_scan_node(mrx,
                                                                        tausrand(taus_state) % 2 == 0,
                                                                        tausrand(taus_state) % 2 == 0,
                                                                        tausrand(taus_state) % 2 == 0 || br_len == 0,
                                                                        px_len,
                                                                        br_len,
                                                                        taus_state);
                    uint8_t nsz = HDR_NSZ(node->hdr);
                    uint8_t px_len1 = HDR_PX_LEN(node->hdr);
                    uint8_t br_len1 = HDR_BR_LEN(node->hdr);
                    uint8_t *br = &node->sn.octets[px_len1];
                    uintptr_t vref = (uintptr_t)VALUE_REF(node, nsz);
                    uintptr_t br_end = (uintptr_t)&br[br_len1];
                    uintptr_t brp_end = (uintptr_t)&SP_ALIGN(br_end)[br_len1];
                    uint8_t mv_len = 0;

                    if (nsz == 4 && br_len1 < (HDR_HAS_VALUE(node->hdr) ? SCAN_NODE_MAX_BRANCH_COUNT_WITH_VALUE : SCAN_NODE_MAX_BRANCH_COUNT)) {
                        const uint8_t mv_br_px_len = minimum_move_px_len_for_new_branch(node, br, br_len1);
                        uint8_t len = minimum_move_px_len_for_new_branch_ref_impl(node, br, br_len1);
                        ASSERT(mv_br_px_len == len);
                    }

                    const uint8_t mv_px_len = MIN_MOVE_PX_LEN_FOR_VALUE(brp_end, vref, br_end);
                    if (brp_end > vref) {
                        while (brp_end > vref) {
                            mv_len++;
                            // note mv_len can become larger than px_len1, ie more than maximum may need to be moved
                            br = &node->sn.octets[px_len1-mv_len];
                            br_end = (uintptr_t)&br[br_len1];
                            brp_end = (uintptr_t)&SP_ALIGN(br_end)[br_len1];
                        }
                        ASSERT(mv_px_len == mv_len);
                    }
                    mrx_test_clear_allocator(mrx);
                }
            }
        }
    }
    fprintf(stderr, "pass\n");

    mrx_test_clear_allocator(mrx);
    mrx_delete(tt);
}

static void
test_set_link_in_parent(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: set_link_in_parent()...");
    {
        const int test_size = 100000; // needs to be high number to get full code coverage
        for (int i = 0; i < test_size; i++) {
            mrx_test_configure_allocator(true, true);
            const int level = tausrand(taus_state) % 100 == 0 ? -1 : 0;
            struct mrx_iterator_path_element path[1];
            union mrx_node *parent = NULL;
            int br = -1;
            int br_pos = -1;
            if (level != -1) {
                if (tausrand(taus_state) % 4 == 0) {
                    parent = mrx_test_generate_mask_node(mrx,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         taus_state);
                    br_pos = -1;
                    for (unsigned j = 0; j < 8; j++) {
                        // to trigger rare special case if there is just one bit set in a block, make it likely we choose that
                        if (bit32_count(parent->mn.bitmask.u32[j]) == 1 && tausrand(taus_state) % 2 == 0) {
                            br_pos = bit32_bsf(parent->mn.bitmask.u32[j]) + (j << 5u);
                        }
                    }
                    if (br_pos == -1) {
                        do {
                            br_pos = tausrand(taus_state) % 256;
                        } while (!bit32_isset(parent->mn.bitmask.u32, br_pos));
                    }
                    br = br_pos;
                } else {
                    parent = mrx_test_generate_scan_node(mrx,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 128,
                                                         1 + tausrand(taus_state) % 128,
                                                         taus_state);
                    const uint8_t br_len = HDR_BR_LEN(parent->hdr);
                    const uint8_t px_len = HDR_PX_LEN(parent->hdr);
                    br_pos = tausrand(taus_state) % br_len;
                    br = parent->sn.octets[px_len+br_pos];
                }
                mrx->root = parent;
                path[0].node = parent;
                path[0].br = br;
                path[0].br_pos = br_pos;
                free_node_and_its_children(get_child(parent, br));
            }

            void **vref;
            uint8_t rest_string[1] = { (uint8_t)'a' };
            union mrx_node *child = new_leaf(mrx, rest_string, 1, &vref);
            *vref = (void *)0xDEADBABEDEADBEEFu;
            //fprintf(stderr, "before\n");
            //mrx_print_node_(parent);
            set_link_in_parent(mrx, path, level, child);
            //fprintf(stderr, "after\n");
            //mrx_print_node_(parent);
            if (level == -1) {
                ASSERT(mrx->root == child);
            } else {
                mrx_debug_sanity_check_node(parent);
                ASSERT(mrx->root != child);
                if (IS_SCAN_NODE(HDR_NSZ(parent->hdr))) {
                    const uint8_t px_len = HDR_PX_LEN(parent->hdr);
                    const uint8_t br_len = HDR_BR_LEN(parent->hdr);
                    union mrx_node *child1 = scan_node_get_child(parent, &parent->sn.octets[px_len], br_len, br_pos);
                    ASSERT(child1 == child);
                } else {
                    uint32_t bm = parent->mn.bitmask.u32[br >> 5u];
                    union mrx_next_block *nx = mask_node_get_next_block(parent, br);
                    uint32_t b = 1u << (br & 0x1Fu);
                    ASSERT((bm & b) != 0);
                    bm &= b - 1;
                    b = bit32_count(bm);
                    union mrx_node *child1 = next_block_get_child(nx, b);
                    ASSERT(child1 == child);
                }
            }
            test_memleak_and_clear_memory(mrx);
        }
    }
    fprintf(stderr, "pass\n");
    mrx_delete(tt);
}

static void
test_prefix_move_up(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: prefix_move_up()...");
    {
        // lots of boundary conditions in this function, so we want to really test every prefix length and
        // long pointer/short pointer combo

        for (int orig_level = 1; orig_level >= 0; orig_level--) {
            for (int lp_combo = 0; lp_combo < 4; lp_combo++) {
                fprintf(stderr, ".");
                for (int pr_px_len = 0; pr_px_len <= orig_level * 128; pr_px_len++) {
                    for (int px_len = 0; px_len < 128; px_len++) {
                        for (int min_px_len = 0; min_px_len <= px_len; min_px_len++) {
                            bool parent_use_default;
                            bool extra_use_default;
                            switch (lp_combo) {
                            case 0:
                                parent_use_default = false;
                                extra_use_default = false;
                                break;
                            case 1:
                                parent_use_default = true;
                                extra_use_default = false;
                                break;
                            case 2:
                                parent_use_default = false;
                                extra_use_default = true;
                                break;
                            case 3:
                                parent_use_default = true;
                                extra_use_default = true;
                                break;
                            }
                            mrx_test_configure_allocator(false, extra_use_default);
                            union mrx_node *node = mrx_test_generate_scan_node(mrx,
                                                                                true,
                                                                                false,
                                                                                true,
                                                                                px_len,
                                                                                0,
                                                                                taus_state);
                            if (HDR_PX_LEN(node->hdr) != px_len) {
                                // px_len not fulfillable, skip iteration
                                mrx_test_clear_allocator(mrx);
                                continue;
                            }
                            uint8_t orig_prefix[256];
                            unsigned orig_prefix_len;
                            struct mrx_iterator_path_element path[10];
                            union mrx_node *parent = NULL;
                            unsigned orig_parent_size = 0;
                            if (orig_level > 0) {
                                // for long pointer we allocate in different area than the child above to force long pointer
                                // when they are linked together with set_link_in_parent()
                                parent = mrx_test_generate_scan_node(mrx,
                                                                     parent_use_default,
                                                                     false,
                                                                     false, // always no value
                                                                     pr_px_len,
                                                                     1,
                                                                     taus_state);
                                if (HDR_PX_LEN(parent->hdr) != pr_px_len) {
                                    // px_len not fulfillable, skip iteration
                                    mrx_test_clear_allocator(mrx);
                                    continue;
                                }

                                memcpy(orig_prefix, parent->sn.octets, pr_px_len + 1);
                                memcpy(&orig_prefix[pr_px_len + 1], node->sn.octets, px_len);
                                orig_prefix_len = pr_px_len + 1 + px_len;
                                orig_parent_size = 8u << HDR_NSZ(parent->hdr);

                                // link together with parent
                                mrx->root = parent;
                                path[0].node = parent;
                                path[0].br = parent->sn.octets[pr_px_len];
                                path[0].br_pos = 0;
                                replace_link_in_parent(mrx, path, 0, node);
                                path[1].node = node;
                                path[1].br = 0xFF;
                                path[1].br_pos = -1;
                            } else {
                                // no parent, node is root
                                mrx->root = node;
                                memcpy(orig_prefix, node->sn.octets, px_len);
                                orig_prefix_len = px_len;
                                path[0].node = node;
                                path[0].br = 0xFF;
                                path[0].br_pos = -1;
                            }
                            orig_prefix[orig_prefix_len] = 0;

                            //fprintf(stderr, "before\n");
                            if (parent != NULL) {
                                //mrx_print_node_(parent);
                                mrx_debug_sanity_check_node(parent);
                            }
                            //mrx_print_node_(node);
                            mrx_debug_sanity_check_node(node);

                            int level = orig_level;
                            //fprintf(stderr, "level %d lp_combo %d pr_px_len %d px_len %d min_px_len %d\n", orig_level, lp_combo, pr_px_len, px_len, min_px_len);
                            // call function under test
                            prefix_move_up(mrx, path, &level, min_px_len);

                            //fprintf(stderr, "after\n");
                            for (int j = 0; j <= level; j++) {
                                //mrx_print_node_(path[j].node);
                            }

                            for (int j = 0; j <= level; j++) {
                                mrx_debug_sanity_check_node(path[j].node);
                            }

                            // check that the prefix is still the same
                            {
                                //fprintf(stderr, "orig prefix: (%s) level %d\n", orig_prefix, level);
                                int lvl = level;
                                node = path[lvl].node;
                                //mrx_print_node_(node);
                                int k = (int)HDR_PX_LEN(node->hdr) - 1;
                                for (int j = orig_prefix_len - 1; j >= 0; j--, k--) {
                                    if (k == -1) {
                                        lvl--;
                                        node = (lvl < 0) ? mrx->root : path[lvl].node;
                                        //fprintf(stderr, "next NODE %d\n", lvl);
                                        //mrx_print_node_(node);
                                        k = HDR_PX_LEN(node->hdr); // include branch octet in parents
                                    }
                                    //fprintf(stderr, "%d %d %d %d\n", j, k, orig_prefix[j], node->sn.octets[k]);

                                    ASSERT(orig_prefix[j] == node->sn.octets[k]);
                                }
                            }

                            // expect that as much as possible of the prefix has been moved to the parent
                            unsigned new_px_len = HDR_PX_LEN(path[level].node->hdr);
                            unsigned moved_px_len = px_len - new_px_len;
                            ASSERT(moved_px_len >= min_px_len);
                            if (moved_px_len < new_px_len) {
                                // not all moved, there must be a good reason, ie pre-existing parent with size enough
                                // to move min_px_len so the original size was kept
                                //fprintf(stderr, ">> %u %u %u\n", moved_px_len, new_px_len, px_len);
                                ASSERT(orig_parent_size > 0);
                                unsigned min_size = SCAN_NODE_MIN_SIZE(pr_px_len, 1, false);
                                unsigned p2_size = 1u << bit32_bsr(min_size);
                                if (p2_size < min_size) {
                                    p2_size *= 2;
                                }
                                unsigned unused = orig_parent_size - p2_size;
                                //fprintf(stderr, "%u %u %u\n", orig_parent_size, min_size, unused);
                                ASSERT(unused == 0);
                            }
                            test_memleak_and_clear_memory(mrx);
                        }
                    }
                }
            }
        }
    }
    fprintf(stderr, "pass\n");
    mrx_test_clear_allocator(mrx);
    mrx_delete(tt);
}

static void
test_scan_node_child_merge(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: scan_node_child_merge()...");
    {
        const int test_size = 100000;
        for (int i = 0; i < test_size; i++) {
            mrx_test_configure_allocator(true, true);
            union mrx_node *parent = mrx_test_generate_scan_node(mrx,
                                                                 tausrand(taus_state) % 2 == 0,
                                                                 tausrand(taus_state) % 2 == 0,
                                                                 false,
                                                                 tausrand(taus_state) % 128,
                                                                 1,
                                                                 taus_state);
            uint8_t prefix[256];
            memcpy(prefix, parent->sn.octets, HDR_PX_LEN(parent->hdr)+1);
            prefix[HDR_PX_LEN(parent->hdr)+1] = 0;
            unsigned tot_px_len = HDR_PX_LEN(parent->hdr) + 1; // include branch octet
            unsigned parent_px_len = HDR_PX_LEN(parent->hdr);
            union mrx_node *child;
            bool is_mask_child = tausrand(taus_state) % 20 == 0;
            unsigned child_br_length = 0;
            if (is_mask_child) {
                child = mrx_test_generate_mask_node(mrx,
                                                    tausrand(taus_state) % 2 == 0,
                                                    tausrand(taus_state) % 2 == 0,
                                                    tausrand(taus_state) % 2 == 0,
                                                    tausrand(taus_state) % 2 == 0,
                                                    taus_state);
            } else {
                child = mrx_test_generate_scan_node(mrx,
                                                    tausrand(taus_state) % 2 == 0,
                                                    tausrand(taus_state) % 2 == 0,
                                                    tausrand(taus_state) % 2 == 0,
                                                    tausrand(taus_state) % 128,
                                                    1 + tausrand(taus_state) % 128,
                                                    taus_state);
                child_br_length = HDR_BR_LEN(child->hdr);
                memcpy(&prefix[tot_px_len], child->sn.octets, HDR_PX_LEN(child->hdr) + child_br_length);
                tot_px_len += HDR_PX_LEN(child->hdr) + child_br_length;
                prefix[tot_px_len] = 0;
            }
            bool child_has_value = HDR_HAS_VALUE(child->hdr);
            struct mrx_iterator_path_element path[1];
            mrx->root = parent;
            path[0].node = parent;
            path[0].br = parent->sn.octets[HDR_PX_LEN(parent->hdr)];
            path[0].br_pos = 0;

            replace_link_in_parent(mrx, path, 0, child);
            assert(scan_node_get_child(parent, &parent->sn.octets[HDR_PX_LEN(parent->hdr)], 1, 0) == child);

            //fprintf(stderr, "before\n");
            //mrx_print_node_(mrx->root);
            scan_node_child_merge(mrx, path, 0);

            //fprintf(stderr, "after\n");
            //mrx_print_node_(mrx->root);
            mrx_debug_sanity_check_node(mrx->root);

            ASSERT(!IS_MASK_NODE(HDR_NSZ(mrx->root->hdr)));
            if (is_mask_child) {
                ASSERT(memcmp(prefix, mrx->root->sn.octets, tot_px_len) == 0);
                ASSERT(HDR_PX_LEN(mrx->root->hdr) == parent_px_len);
                ASSERT(HDR_BR_LEN(mrx->root->hdr) == 1);
            } else {
                uint8_t minsize = SCAN_NODE_MIN_SIZE(tot_px_len - child_br_length, child_br_length, child_has_value);
                if (tot_px_len < 128 && minsize <= 128) {
                    ASSERT(memcmp(prefix, mrx->root->sn.octets, tot_px_len) == 0);
                    ASSERT(HDR_PX_LEN(mrx->root->hdr) == tot_px_len - child_br_length);
                    ASSERT(HDR_BR_LEN(mrx->root->hdr) == child_br_length);
                } else {
                    ASSERT(memcmp(prefix, mrx->root->sn.octets, parent_px_len + 1) == 0);
                    ASSERT(HDR_PX_LEN(mrx->root->hdr) == parent_px_len);
                    ASSERT(HDR_BR_LEN(mrx->root->hdr) == 1);
                }
            }

            test_memleak_and_clear_memory(mrx);
        }
    }
    fprintf(stderr, "pass\n");
    mrx_test_clear_allocator(mrx);
    mrx_delete(tt);
}

static void
test_erase_value_from_node(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: erase_value_from_node()...");
    {
        const int test_size = 100000;
        for (int i = 0; i < test_size; i++) {
            mrx_test_configure_allocator(true, true);

            int level = tausrand(taus_state) % 50 == 0 ? 0 : 1;

            union mrx_node *node;
            if (tausrand(taus_state) % 100 == 0) {
                node = mrx_test_generate_mask_node(mrx,
                                                   tausrand(taus_state) % 2 == 0,
                                                   tausrand(taus_state) % 2 == 0,
                                                   true,
                                                   tausrand(taus_state) % 2 == 0,
                                                   taus_state);
            } else {
                node = mrx_test_generate_scan_node(mrx,
                                                   tausrand(taus_state) % 2 == 0,
                                                   tausrand(taus_state) % 2 == 0,
                                                   true,
                                                   tausrand(taus_state) % 128,
                                                   tausrand(taus_state) % 10,
                                                   taus_state);
            }

            struct mrx_iterator_path_element path[2];
            int br, br_pos;
            union mrx_node *parent;
            if (level > 0) {
                if (tausrand(taus_state) % 2 == 0) {
                    parent = mrx_test_generate_mask_node(mrx,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         taus_state);
                    do {
                        br_pos = tausrand(taus_state) % 256;
                    } while (!bit32_isset(parent->mn.bitmask.u32, br_pos));
                    br = br_pos;
                } else {
                    parent = mrx_test_generate_scan_node(mrx,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 128,
                                                         2 + tausrand(taus_state) % 128,
                                                         taus_state);
                    const uint8_t br_len = HDR_BR_LEN(parent->hdr);
                    const uint8_t px_len = HDR_PX_LEN(parent->hdr);
                    br_pos = tausrand(taus_state) % br_len;
                    br = parent->sn.octets[px_len+br_pos];
                }
                mrx->root = parent;
                path[0].node = parent;
                path[0].br = br;
                path[0].br_pos = br_pos;
                replace_link_in_parent(mrx, path, 0, node);
            } else {
                mrx->root = node;
            }

            path[level].node = node;
            path[level].br = 0xFFu;
            path[level].br_pos = -1;
            mrx_debug_sanity_check_node(node);

            erase_value_from_node(mrx, path, level);

            test_memleak_and_clear_memory(mrx);
        }
    }
    fprintf(stderr, "pass\n");
    mrx_test_clear_allocator(mrx);
}

static void
test_mask_node_erase_branch(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: mask_node_erase_branch()...");
    {
        const int test_size = 100000;
        for (int i = 0; i < test_size; i++) {
            mrx_test_configure_allocator(true, true);

            int level = tausrand(taus_state) % 50 == 0 ? 0 : 1;
            int br, br_pos;
            union mrx_node *parent;
            bool is_mask_parent = tausrand(taus_state) % 2 == 0;
            if (is_mask_parent) {
                parent = mrx_test_generate_mask_node(mrx,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 2 == 0,
                                                     taus_state);
                do {
                    br_pos = tausrand(taus_state) % 256;
                } while (!bit32_isset(parent->mn.bitmask.u32, br_pos));
                br = br_pos;
            } else {
                parent = mrx_test_generate_scan_node(mrx,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 128,
                                                     2 + tausrand(taus_state) % 128,
                                                     taus_state);
                const uint8_t br_len = HDR_BR_LEN(parent->hdr);
                const uint8_t px_len = HDR_PX_LEN(parent->hdr);
                br_pos = tausrand(taus_state) % br_len;
                br = parent->sn.octets[px_len+br_pos];
            }
            bool has_value = tausrand(taus_state) % 2 == 0;
            union mrx_node *node = mrx_test_generate_mask_node(mrx,
                                                               tausrand(taus_state) % 2 == 0,
                                                               tausrand(taus_state) % 2 == 0,
                                                               has_value,
                                                               tausrand(taus_state) % 2 == 0,
                                                               taus_state);

            { // make some next blocks oversized to test size reduction
                for (uint32_t j = 0; j < 8; j++) {
                    if (node->mn.bitmask.u32[j] == 0) {
                        continue;
                    }
                    union mrx_next_block *nx = mask_node_get_next_block(node, j << 5u);
                    if (nx == (union mrx_next_block *)node->mn.local || tausrand(taus_state) % 2 == 0) {
                        continue;
                    }
                    if (NEXT_BLOCK_IS_SHORT_PTR(nx)) {
                        uint8_t old_sz = NEXT_BLOCK_HDR(nx);
                        if (old_sz < 4) {
                            uint8_t new_sz = old_sz + 1;
                            union mrx_next_block *newnx;
                            for (;;) {
                                newnx = mrx_test_alloc_node(mrx, new_sz);
                                if (PTR_PREFIX_MATCHES(newnx, nx)) {
                                    break;
                                }
                                mrx_test_free_node(mrx, newnx, new_sz);
                            }
                            memcpy(newnx, nx, 8u << old_sz);
                            NEXT_BLOCK_HDR_SET(newnx, new_sz);
                            mrx_test_free_node(mrx, nx, old_sz);
                            assert(node->mn.next[j] == SP(nx));
                            node->mn.next[j] = SP(newnx);
                        }
                    } else {
                        if (nx->lp.next == NULL) {
                            nx->lp.next = mrx_test_alloc_node(mrx, 4);
                            if (tausrand(taus_state) % 2 == 0) {
                                nx->lp.next->lp.next = mrx_test_alloc_node(mrx, 3);
                            } else {
                                nx->lp.next->lp.next = NULL;
                            }
                        } else if (nx->lp.next->lp.next == NULL) {
                            nx->lp.next->lp.next = mrx_test_alloc_node(mrx, 3);
                        }
                    }
                }
            }

            int bro[256];
            union mrx_node *nodeptrs[256];
            int parent_br_len = get_nodeptrs(nodeptrs, bro, parent);
            int old_br_len = get_nodeptrs(nodeptrs, bro, node);
            if (old_br_len < 5) {
                // too few branches for this use case, skip iteration
                mrx_test_clear_allocator(mrx);
                mrx->root = NULL;
                continue;
            }
            if (has_value) {
                *mrx_node_value_ref_(node, HDR_NSZ(node->hdr)) = (void *)0x12345678u;
            }

            struct mrx_iterator_path_element path[2];
            if (level > 0) {
                mrx->root = parent;
                path[0].node = parent;
                path[0].br = br;
                path[0].br_pos = br_pos;
                replace_link_in_parent(mrx, path, 0, node);
            } else {
                free_node_and_its_children(parent);
                mrx->root = node;
            }

            do {
                br_pos = tausrand(taus_state) % 256;
            } while (!bit32_isset(node->mn.bitmask.u32, br_pos));
            br = br_pos;
            union mrx_node *branch_to_remove = get_child(node, br);

            path[level].node = node;
            path[level].br = br;
            path[level].br_pos = br_pos;
            mrx_debug_sanity_check_node(node);

            //fprintf(stderr, "parent before\n");
            //mrx_print_node_(parent);

            //fprintf(stderr, "before\n");
            //mrx_print_node_(node);

            // call function under test
            mask_node_erase_branch(mrx, path, level);

            // path node list and level may not be valid after call to scan_node_erase_branch()
            mrx_debug_sanity_check_node(mrx->root);
            if (level == 0) {
                node = mrx->root;
            } else {
                // parent/node may have been merged, use branch length to figure out
                int bro1[256];
                union mrx_node *nodeptrs1[256];
                int root_br_len = get_nodeptrs(nodeptrs1, bro1, mrx->root);
                if (root_br_len == old_br_len + parent_br_len - 2 && root_br_len > 1) {
                    node = mrx->root;
                } else {
                    node = get_child(mrx->root, path[0].br);
                }
            }

            free_node_and_its_children(branch_to_remove);
            //fprintf(stderr, "root after\n");
            //mrx_print_node_(mrx->root);

            //fprintf(stderr, "after\n");
            //mrx_print_node_(node);

            mrx_debug_sanity_check_node(node);

            { // check that appropriate branch has been removed
                int bro1[256];
                union mrx_node *nodeptrs1[256];
                int new_br_len = get_nodeptrs(nodeptrs1, bro1, node);
                ASSERT(new_br_len == old_br_len - 1);
                for (int k = 0; k < old_br_len; k++) {
                    int j;
                    for (j = 0; j < new_br_len; j++) {
                        if (bro1[j] == bro[k]) {
                            ASSERT(nodeptrs1[j] == nodeptrs[k]);
                            break;
                        }
                    }
                    if (j == new_br_len) {
                        ASSERT(bro[k] == br);
                        ASSERT(nodeptrs[k] == branch_to_remove);
                    }
                }
            }
            // check value and prefix
            ASSERT(!HDR_HAS_VALUE(node->hdr) == !has_value);
            if (has_value) {
                ASSERT(*mrx_node_value_ref_(node, HDR_NSZ(node->hdr)) == (void *)0x12345678u);
            }

            test_memleak_and_clear_memory(mrx);
        }
    }
    fprintf(stderr, "pass\n");
    mrx_test_clear_allocator(mrx);
}

static void
test_scan_node_erase_branch(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: scan_node_erase_branch()...");
    {
        const int test_size = 100000;
        for (int i = 0; i < test_size; i++) {
            mrx_test_configure_allocator(true, true);

            int level = tausrand(taus_state) % 50 == 0 ? 0 : 1;
            int br, br_pos;
            union mrx_node *parent;
            bool is_mask_parent = tausrand(taus_state) % 20 == 0;
            uint8_t parent_prefix[128];
            uint8_t parent_prefix_len = 0;
            if (is_mask_parent) {
                parent = mrx_test_generate_mask_node(mrx,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 2 == 0,
                                                     taus_state);
                do {
                    br_pos = tausrand(taus_state) % 256;
                } while (!bit32_isset(parent->mn.bitmask.u32, br_pos));
                br = br_pos;
            } else {
                parent = mrx_test_generate_scan_node(mrx,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 2 == 0,
                                                     tausrand(taus_state) % 128,
                                                     2 + tausrand(taus_state) % 128,
                                                     taus_state);
                const uint8_t br_len = HDR_BR_LEN(parent->hdr);
                const uint8_t px_len = HDR_PX_LEN(parent->hdr);
                br_pos = tausrand(taus_state) % br_len;
                br = parent->sn.octets[px_len+br_pos];
                memcpy(parent_prefix, parent->sn.octets, px_len);
                parent_prefix_len = px_len;
            }
            bool has_value = tausrand(taus_state) % 2 == 0;
            uint8_t tbr_len = tausrand(taus_state) % (SCAN_NODE_MAX_BRANCH_COUNT + 1);
            if (tbr_len < 2 && !has_value) {
                tbr_len = 2;
            }
            if (tbr_len < 1 && has_value) {
                tbr_len = 1;
            }
            union mrx_node *node = mrx_test_generate_scan_node(mrx,
                                                               tausrand(taus_state) % 2 == 0,
                                                               tausrand(taus_state) % 2 == 0,
                                                               has_value,
                                                               tausrand(taus_state) % 128,
                                                               tbr_len,
                                                               taus_state);

            const uint8_t px_len = HDR_PX_LEN(node->hdr);
            const uint8_t br_len = HDR_BR_LEN(node->hdr);
            int bro[br_len];
            union mrx_node *nodeptrs[br_len];
            get_nodeptrs(nodeptrs, bro, node);
            uint8_t prefix[px_len];
            memcpy(prefix, node->sn.octets, px_len);
            if (has_value) {
                *mrx_node_value_ref_(node, HDR_NSZ(node->hdr)) = (void *)0x12345678u;
            }

            struct mrx_iterator_path_element path[2];
            if (level > 0) {
                mrx->root = parent;
                path[0].node = parent;
                path[0].br = br;
                path[0].br_pos = br_pos;
                replace_link_in_parent(mrx, path, 0, node);
            } else {
                free_node_and_its_children(parent);
                mrx->root = node;
            }

            br_pos = tausrand(taus_state) % br_len;
            br = node->sn.octets[px_len+br_pos];
            union mrx_node *branch_to_remove = get_child(node, br);

            path[level].node = node;
            path[level].br = br;
            path[level].br_pos = br_pos;
            if (tausrand(taus_state) % 10 == 0 && HDR_NSZ(node->hdr) < 4) {
                // enlarge node to simulate that some branches have been removed already
                enlarge_node(mrx, path, level);
                node = path[level].node;
            }
            mrx_debug_sanity_check_node(node);

            // call function under test
            scan_node_erase_branch(mrx, path, level);

            // path node list and level may not be valid after call to scan_node_erase_branch()
            mrx_debug_sanity_check_node(mrx->root);
            if (level == 0) {
                node = mrx->root;
            } else {
                if (br_len > 1) {
                    node = get_child(mrx->root, path[0].br);
                } else {
                    // parent/node may have been merged
                    if (HDR_HAS_VALUE(mrx->root->hdr) &&
                        *mrx_node_value_ref_(mrx->root, HDR_NSZ(mrx->root->hdr)) == (void *)0x12345678u)
                    {
                        node = mrx->root;
                    } else {
                        node = get_child(mrx->root, path[0].br);
                    }
                }
            }
            mrx_debug_sanity_check_node(node);
            free_node_and_its_children(branch_to_remove);

            { // check that appropriate branch has been removed
                ASSERT(HDR_BR_LEN(node->hdr) == br_len - 1);
                int bro1[br_len];
                union mrx_node *nodeptrs1[br_len];
                get_nodeptrs(nodeptrs1, bro1, node);
                for (int k = 0; k < br_len; k++) {
                    int j;
                    for (j = 0; j < HDR_BR_LEN(node->hdr); j++) {
                        if (bro1[j] == bro[k]) {
                            ASSERT(nodeptrs1[j] == nodeptrs[k]);
                            break;
                        }
                    }
                    if (j == HDR_BR_LEN(node->hdr)) {
                        ASSERT(bro[k] == br);
                        ASSERT(nodeptrs[k] == nodeptrs[br_pos]);
                    }
                }
            }
            // check value and prefix
            ASSERT(!HDR_HAS_VALUE(node->hdr) == !has_value);
            if (has_value) {
                ASSERT(*mrx_node_value_ref_(node, HDR_NSZ(node->hdr)) == (void *)0x12345678u);
            }
            ASSERT(HDR_PX_LEN(node->hdr) >= px_len);
            if (HDR_PX_LEN(node->hdr) == px_len) {
                ASSERT(memcmp(node->sn.octets, prefix, px_len) == 0);
            } else {
                ASSERT(HDR_PX_LEN(node->hdr) == parent_prefix_len + 1 + px_len);
                ASSERT(memcmp(node->sn.octets, parent_prefix, parent_prefix_len) == 0);
                ASSERT(memcmp(&node->sn.octets[parent_prefix_len+1], prefix, px_len) == 0);
                ASSERT(node->sn.octets[parent_prefix_len] == path[0].br);
            }

            test_memleak_and_clear_memory(mrx);
        }
    }
    fprintf(stderr, "pass\n");
    mrx_test_clear_allocator(mrx);
}


static void
test_mask_node_insert_branch(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: mask_node_insert_branch()...");
    {
        const int test_size = 100;
        for (int i = 0; i < test_size; i++) {
            for (int is_local_used = 0; is_local_used < 2; is_local_used++) {
                for (int is_long_ptr = 0; is_long_ptr < 2; is_long_ptr++) {
                    for (int branch_octet = 0; branch_octet < 256; branch_octet++) {
                        mrx_test_configure_allocator(true, true);
                        union mrx_node *node = mrx_test_generate_mask_node(mrx,
                                                                           tausrand(taus_state) % 2 == 0,
                                                                           is_long_ptr,
                                                                           tausrand(taus_state) % 2 == 0,
                                                                           is_local_used,
                                                                           taus_state);
                        if (bit32_isset(node->mn.bitmask.u32, branch_octet)) {
                            mrx_test_clear_allocator(mrx);
                            mrx->root = NULL;
                            continue;
                        }
                        void **vref;
                        uint8_t rest_string[1] = { (uint8_t)'a' };
                        union mrx_node *leaf = new_leaf(mrx, rest_string, 1, &vref);
                        *vref = (void *)0xDEADBABEDEADBEEFu;

                        union mrx_node *branches[256];
                        uint32_t bitmask[8];
                        memset(bitmask, 0, sizeof(bitmask));
                        for (int j = 0; j < 256; j++) {
                            if (bit32_isset(node->mn.bitmask.u32, j)) {
                                bit32_set(bitmask, j);

                                uint32_t bm = node->mn.bitmask.u32[j >> 5u];
                                union mrx_next_block *nx = mask_node_get_next_block(node, j);
                                uint32_t b = 1u << (j & 0x1Fu);
                                bm &= b - 1;
                                b = bit32_count(bm);
                                branches[j] = next_block_get_child(nx, b);
                                assert(branches[j] != NULL);
                            } else {
                                branches[j] = NULL;
                            }
                        }
                        branches[branch_octet] = leaf;
                        bit32_set(bitmask, branch_octet);
                        mrx->root = node;

                        //fprintf(stderr, "before 0x%02X %d\n", branch_octet, branch_octet);
                        //mrx_print_node_(node);
                        mrx_debug_sanity_check_node(node);

                        mask_node_insert_branch(&tt->mrx, node, branch_octet, leaf);

                        //fprintf(stderr, "after\n");
                        //mrx_print_node_(node);

                        mrx_debug_sanity_check_node(node);

                        for (int j = 0; j < 256; j++) {
                            if (bit32_isset(bitmask, j)) {
                                ASSERT(bit32_isset(node->mn.bitmask.u32, j));

                                uint32_t bm = node->mn.bitmask.u32[j >> 5u];
                                union mrx_next_block *nx = mask_node_get_next_block(node, j);
                                uint32_t b = 1u << (j & 0x1Fu);
                                bm &= b - 1;
                                b = bit32_count(bm);
                                ASSERT(next_block_get_child(nx, b) == branches[j]);
                            } else {
                                ASSERT(!bit32_isset(node->mn.bitmask.u32, j));
                            }
                        }

                        test_memleak_and_clear_memory(mrx);
                    }
                }
            }
        }
    }
    fprintf(stderr, "pass\n");
    mrx_test_clear_allocator(mrx);
    mrx_delete(tt);
}

static void
test_scan_node_to_mask_node_and_enlarge_node(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: scan_node_to_mask_node() and enlarge_node()...");
    {
        const int test_size = 1000;
        for (int i = 0; i < test_size; i++) {
            for (int try_br_len = 0; try_br_len <= SCAN_NODE_MAX_BRANCH_COUNT; try_br_len++) {
                mrx_test_configure_allocator(true, true);

                bool has_value = tausrand(taus_state) % 2 == 0 || try_br_len == 0;
                union mrx_node *node = mrx_test_generate_scan_node(mrx,
                                                                    tausrand(taus_state) % 2 == 0,
                                                                    tausrand(taus_state) % 2 == 0,
                                                                    has_value,
                                                                    0, // px_len must be zero for conversion to mask node
                                                                    try_br_len,
                                                                    taus_state);
                int br_len = HDR_BR_LEN(node->hdr);
                while (HDR_NSZ(node->hdr) < 4) {
                    struct mrx_iterator_path_element path[1];
                    mrx->root = node;
                    path[0].node = node;

                    // bonus function under test
                    enlarge_node(mrx, path, 0);

                    node = path[0].node;
                }
                if (has_value) {
                    *mrx_node_value_ref_(node, HDR_NSZ(node->hdr)) = (void *)0xDEADBABEDEADBEEFu;
                }
                int bro[br_len+1];
                union mrx_node *nodeptrs[br_len+1];
                get_nodeptrs(nodeptrs, bro, node);

                void **vref;
                uint8_t rest_string[1] = { (uint8_t)'a' };
                union mrx_node *leaf = new_leaf(mrx, rest_string, 1, &vref);
                *vref = (void *)0xDEADBABEDEADBEEFu;
                nodeptrs[br_len] = leaf;

                int branch_octet = tausrand(taus_state) % 10 == 0 ? -1 : (int)'a' + tausrand(taus_state) % 26;
                bro[br_len] = branch_octet;
                if (branch_octet == -1) {
                    free_node_and_its_children(leaf);
                    leaf = NULL;
                }

                //fprintf(stderr, "before\n");
                //mrx_print_node_(node);

                scan_node_to_mask_node(mrx, node, branch_octet, leaf);

                //fprintf(stderr, "after\n");
                //mrx_print_node_(node);

                mrx_debug_sanity_check_node(node);
                if (has_value) {
                    ASSERT(*mrx_node_value_ref_(node, HDR_NSZ(node->hdr)) == (void *)0xDEADBABEDEADBEEFu);
                }
                int br_len_ex = br_len + (branch_octet != -1 ? 1 : 0);
                for (int j = 0; j < br_len_ex; j++) {
                    if (bro[j] == -1) {
                        continue;
                    }
                    uint32_t b = bro[j];
                    uint32_t bm = node->mn.bitmask.u32[b >> 5u];
                    union mrx_next_block *nx = mask_node_get_next_block(node, b);
                    b = 1u << (b & 0x1Fu);
                    ASSERT((bm & b) != 0);
                    bm &= b - 1;
                    b = bit32_count(bm);
                    union mrx_node *leaf1 = next_block_get_child(nx, b);
                    ASSERT(leaf1 == nodeptrs[j]);
                }
                ASSERT(barr32_count(node->mn.bitmask.u32, 0, 255) ==  br_len_ex);

                mrx->root = node;
                test_memleak_and_clear_memory(mrx);
            }
        }
    }
    fprintf(stderr, "pass\n");
    mrx_test_clear_allocator(mrx);
    mrx_delete(tt);
}

static void
test_scan_node_insert_branch(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: scan_node_insert_branch()...");
    {
        const int test_size = 10000; // needs to be high number to get full code coverage
        for (int i = 0; i < test_size; i++) {
            for (int use_new_branch = 0; use_new_branch <= 1; use_new_branch++) {
                for (int tbr_len = 0; tbr_len <= SCAN_NODE_MAX_BRANCH_COUNT; tbr_len++) {
                    mrx_test_configure_allocator(true, true);

                    union mrx_node *node = mrx_test_generate_scan_node(mrx,
                                                                       tausrand(taus_state) % 2 == 0,
                                                                       tausrand(taus_state) % 2 == 0,
                                                                       tausrand(taus_state) % 2 == 0 || tbr_len == 0,
                                                                       tausrand(taus_state) % 128,
                                                                       tbr_len,
                                                                       taus_state);
                    const uint8_t br_len = HDR_BR_LEN(node->hdr);
                    void **vref;
                    uint8_t rest_string[1] = { (uint8_t)'a' };
                    union mrx_node *leaf = new_leaf(mrx, rest_string, 1, &vref);
                    *vref = (void *)0xDEADBABEDEADBEEFu;
                    struct mrx_iterator_path_element path[3];

                    mrx->root = node;
                    path[0].node = node;
                    path[0].br = 0xFF;
                    path[0].br_pos = -1;

                    //fprintf(stderr, "before\n");
                    //mrx_print_node_(node);

                    int level = 0;
                    uint8_t nsz = HDR_NSZ(node->hdr);
                    uint8_t *br = &node->sn.octets[HDR_PX_LEN(node->hdr)];
                    uint8_t branch_octet = 0;
                    if (use_new_branch || br_len == 0) {
                        branch_octet = (uint8_t)'a' + tausrand(taus_state) % 26;
                    } else {
                        int br_pos = tausrand(taus_state) % br_len;
                        for (uint8_t j = 0; j < br_len; j++) {
                            if (j == br_pos) {
                                branch_octet = (uint8_t)'A' + j;
                            }
                            br[j] = (uint8_t)'A' + j + (j < br_pos ? 0 : 1);
                        }
                    }

                    union mrx_node *branches[SCAN_NODE_MAX_BRANCH_COUNT];
                    uint8_t branch_octets[SCAN_NODE_MAX_BRANCH_COUNT];
                    for (uint8_t j = 0; j < br_len; j++) {
                        branch_octets[j] = br[j];
                        branches[j] = scan_node_get_child(node, &node->sn.octets[HDR_PX_LEN(node->hdr)],
                                                          HDR_BR_LEN(node->hdr), j);
                    }
                    scan_node_insert_branch(mrx, path, &level, nsz, br, br_len, branch_octet, leaf);

                    node = path[level].node;
                    //fprintf(stderr, "after\n");
                    //mrx_print_node_(node);
                    for (int j = 0; j <= level; j++) {
                        mrx_debug_sanity_check_node(path[j].node);
                    }
                    if (IS_MASK_NODE(HDR_NSZ(node->hdr))) {
                        uint32_t b = branch_octet;
                        uint32_t bm = node->mn.bitmask.u32[b >> 5u];
                        union mrx_next_block *nx = mask_node_get_next_block(node, b);
                        b = 1u << (b & 0x1Fu);
                        ASSERT((bm & b) != 0);
                        bm &= b - 1;
                        b = bit32_count(bm);
                        union mrx_node *leaf1 = next_block_get_child(nx, b);
                        ASSERT(leaf == leaf1);
                    } else {
                        // check scan node result thoroughly as this function also tests sn_node_insert() help function
                        int8_t br_pos = mrx_find_branch_generic_(branch_octet,
                                                                 &node->sn.octets[HDR_PX_LEN(node->hdr)],
                                                                 HDR_BR_LEN(node->hdr));
                        ASSERT(br_pos != -1);
                        union mrx_node *leaf1 = scan_node_get_child(node, &node->sn.octets[HDR_PX_LEN(node->hdr)],
                                                                    HDR_BR_LEN(node->hdr), br_pos);
                        ASSERT(leaf1 == leaf);
                        uint8_t *nbr = &node->sn.octets[HDR_PX_LEN(node->hdr)];
                        int offset = 0;
                        for (int j = 0; j < br_len + 1; j++) {
                            union mrx_node *b = scan_node_get_child(node, nbr, HDR_BR_LEN(node->hdr), j);
                            if (j == br_pos) {
                                offset = 1;
                                ASSERT(nbr[j] == branch_octet);
                                ASSERT(b == leaf);
                            } else {
                                ASSERT(nbr[j] == branch_octets[j-offset]);
                                ASSERT(b == branches[j-offset]);
                            }
                        }
                    }

                    test_memleak_and_clear_memory(mrx);
                }
            }
        }
    }
    fprintf(stderr, "pass\n");
    mrx_test_clear_allocator(mrx);
    mrx_delete(tt);
}

static void
test_scan_node_insert_value(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: scan_node_insert_value()...");
    {
        const int test_size = 100000; // needs to be high number to get full code coverage
        for (int i = 0; i < test_size; i++) {
            mrx_test_configure_allocator(true, true);

            union mrx_node *node = mrx_test_generate_scan_node(mrx,
                                                               tausrand(taus_state) % 2 == 0,
                                                               tausrand(taus_state) % 2 == 0,
                                                               false, // always no value
                                                               tausrand(taus_state) % 128,
                                                               1 + tausrand(taus_state) % 128,
                                                               taus_state);

            struct mrx_iterator_path_element path[3];
            int br_pos;
            int br;
            int level = tausrand(taus_state) % 2;
            if (level > 0) {
                union mrx_node *parent;
                if (tausrand(taus_state) % 4 == 0) {
                    parent = mrx_test_generate_mask_node(mrx,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         taus_state);
                    do {
                        br_pos = tausrand(taus_state) % 256;
                    } while (!bit32_isset(parent->mn.bitmask.u32, br_pos));
                    br = br_pos;
                } else {
                    parent = mrx_test_generate_scan_node(mrx,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 2 == 0,
                                                         tausrand(taus_state) % 128,
                                                         1 + tausrand(taus_state) % 128,
                                                         taus_state);
                    const uint8_t br_len = HDR_BR_LEN(parent->hdr);
                    const uint8_t px_len = HDR_PX_LEN(parent->hdr);
                    br_pos = tausrand(taus_state) % br_len;
                    br = parent->sn.octets[px_len+br_pos];
                }
                // link together with parent
                mrx->root = parent;
                path[0].node = parent;
                path[0].br = br;
                path[0].br_pos = br_pos;
                replace_link_in_parent(mrx, path, 0, node);
            } else {
                mrx->root = node;
            }

            // prepare and call function under test
            const uint8_t nsz = HDR_NSZ(node->hdr);
            path[level].node = node;
            path[level].br = br;
            path[level].br_pos = br_pos;

            ASSERT(!HDR_HAS_VALUE(node->hdr));
            //fprintf(stderr, "before\n");
            //mrx_print_node_(node);
            void **vref = scan_node_insert_value(mrx, path, &level, nsz);
            *vref = (void *)0xDEADBABEDEADBEEFu;
            node = path[level].node;
            //fprintf(stderr, "after\n");
            //mrx_print_node_(node);
            ASSERT(HDR_HAS_VALUE(node->hdr));
            for (int j = 0; j <= level; j++) {
                mrx_debug_sanity_check_node(path[j].node);
            }

            test_memleak_and_clear_memory(mrx);
        }
    }
    fprintf(stderr, "pass\n");
    mrx_delete(tt);
}

static void
test_new_leaf(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: new_leaf()...");
    {
        const int test_size = 500;
        uint8_t rest_string[test_size];
        void **vref;
        for (unsigned i = 0; i < test_size; i++) {
            rest_string[i] = (uint8_t)'a' + i % 26u;
        }
        for (int i = 0; i < 100; i++) {
            for (unsigned rest_length = 0; rest_length < test_size; rest_length++) {
                mrx_test_configure_allocator(i > 0, true); // use one pass with one allocator, trigger long pointer cases for the rest
                union mrx_node *node = new_leaf(mrx, rest_string, rest_length, &vref);
                *vref = NULL;
                mrx_debug_sanity_check_node_with_children(node);
                mrx->root = node;
                test_memleak_and_clear_memory(mrx);
            }
        }
    }
    fprintf(stderr, "pass\n");
    mrx_test_clear_allocator(mrx);
    mrx_delete(tt);
}

static void
test_scan_node_split(void)
{
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: scan_node_split()...");
    {
        for (int tpx_len = 1; tpx_len < 128; tpx_len++) {
            for (int tequal_len = 0; tequal_len < tpx_len; tequal_len++) {
                for (int br_len = 0; br_len <= SCAN_NODE_MAX_BRANCH_COUNT; br_len++) {
                    mrx_test_configure_allocator(true, true);
                    union mrx_node *node = mrx_test_generate_scan_node(mrx,
                                                                        tausrand(taus_state) % 2 == 0,
                                                                        tausrand(taus_state) % 2 == 0,
                                                                        tausrand(taus_state) % 2 == 0 || br_len == 0,
                                                                        tpx_len,
                                                                        br_len,
                                                                        taus_state);
                    const int px_len = HDR_PX_LEN(node->hdr);
                    const int equal_len = tequal_len < px_len ? tequal_len : px_len - 1;
                    struct mrx_iterator_path_element path[3];
                    uint8_t orig_prefix[256];
                    uint8_t orig_branch[32];
                    memcpy(orig_prefix, node->sn.octets, px_len);
                    memcpy(orig_branch, &node->sn.octets[px_len], br_len);

                    mrx->root = node;
                    path[0].node = node;
                    path[0].br = 0xFFu;
                    path[0].br_pos = -1;

                    uint8_t rest_string[256];
                    unsigned rest_length = tausrand(taus_state) % 256;
                    if (tausrand(taus_state) % 4 == 0) {
                        rest_length = 0; // test special case 0 a bit more often
                    }
                    for (int j = 0; j < rest_length; j++) {
                        rest_string[j] = '0' + j % 10;
                    }

                    //fprintf(stderr, "before\n");
                    //mrx_print_node_(node);
                    int level = 0;
                    //fprintf(stderr, "px_len %d equal_len %d rest_length %d\n", HDR_PX_LEN(node->hdr), equal_len, rest_length);
                    void **vref = scan_node_split(mrx, path, &level, equal_len, rest_string, rest_length);
                    *vref = (void *)0xDEADBABEDEADBFFFu;

                    //fprintf(stderr, "after\n");
                    for (int j = 0; j <= level; j++) {
                        //mrx_print_node_(path[j].node);
                        mrx_debug_sanity_check_node(path[j].node);
                    }

                    node = mrx->root;
                    int offset = 0;
                    int eq_len = equal_len;
                    int lvl = 1;
                    node = mrx->root;
                    for (;;) {
                        int test_len = (HDR_PX_LEN(node->hdr) < eq_len) ? HDR_PX_LEN(node->hdr) : eq_len;
                        ASSERT(memcmp(&orig_prefix[offset], node->sn.octets, test_len) == 0);
                        offset += test_len;
                        eq_len -= test_len;
                        uint8_t *br = &node->sn.octets[HDR_PX_LEN(node->hdr)];
                        if (eq_len == 0) {
                            if (offset == equal_len) {
                                if (rest_length == 0) {
                                    ASSERT(HDR_BR_LEN(node->hdr) == 1);
                                    ASSERT(br[0] == orig_prefix[offset]);
                                    ASSERT(*mrx_node_value_ref_(node, HDR_NSZ(node->hdr)) == (void *)0xDEADBABEDEADBFFFu);
                                    break;
                                }
                                ASSERT(HDR_BR_LEN(node->hdr) == 2);
                                ASSERT(br[0] == '0');
                                ASSERT(br[1] == orig_prefix[offset]);
                                node = get_child(node, br[0]);

                                eq_len = rest_length - 1;
                                offset = 1;
                                while (eq_len > 0) {
                                    test_len = (HDR_PX_LEN(node->hdr) < eq_len) ? HDR_PX_LEN(node->hdr) : eq_len;
                                    ASSERT(memcmp(&rest_string[offset], node->sn.octets, test_len) == 0);
                                    offset += test_len;
                                    eq_len -= test_len;
                                    if (eq_len > 0) {
                                        ASSERT(HDR_BR_LEN(node->hdr) == 1);
                                        br = &node->sn.octets[HDR_PX_LEN(node->hdr)];
                                        ASSERT(br[0] == rest_string[offset]);
                                        offset += 1;
                                        eq_len -= 1;
                                        node = get_child(node, br[0]);
                                    }
                                }
                                ASSERT(HDR_BR_LEN(node->hdr) == 0);
                                ASSERT(*mrx_node_value_ref_(node, HDR_NSZ(node->hdr)) == (void *)0xDEADBABEDEADBFFFu);
                                break;
                            } else {
                                ASSERT(HDR_BR_LEN(node->hdr) == br_len);
                                ASSERT(memcmp(br, orig_branch, br_len) == 0);
                                ASSERT(*mrx_node_value_ref_(node, HDR_NSZ(node->hdr)) == (void *)0xDEADBABEDEADBFFFu);
                                break;
                            }
                        }
                        ASSERT(br[0] == orig_prefix[offset]);
                        offset += 1;
                        eq_len -= 1;
                        ASSERT(HDR_BR_LEN(node->hdr) == 1);
                        node = get_child(node, br[0]);
                        lvl++;
                    }
                    ASSERT(level == lvl);

                    test_memleak_and_clear_memory(mrx);
                }
            }
        }
    }
    fprintf(stderr, "pass\n");
    mrx_test_clear_allocator(mrx);
    mrx_delete(tt);
}

static void
test_iterator(void)
{
    fprintf(stderr, "Test: mrx iterators...");
    {
        str2ref_t *shadow_tt = str2ref_new(~0);
        mrx_t *tt = mrx_new(~0u);
        mrx_base_t *mrx = &tt->mrx;

        mrx_test_configure_allocator(true, true);
        const unsigned test_size = 70000;
        unsigned j = 0;
        for (uintptr_t i = 0; i < test_size; i++) {
            char key[3200];
            sprintf(key, "  %08X", tausrand(taus_state));
            // make sure we use a mask node at a couple of levels
            key[0] = 1 + i % 255; // don't use '\0' as keys are null-terminated strings
            if (key[0] == 1) {
                key[1] = 1 + j % 255;
                j++;
            } else if (key[0] == 2) {
                unsigned add_len = 1000 + tausrand(taus_state) % 2000;
                for (int k = 0; k < add_len; k++) {
                    key[10+k] = 'a' + k % 26;
                }
                key[10+add_len] = '\0';
            }
            void *value = (void *)(i+1);
            str2ref_insert(shadow_tt, key, value);
            mrx_insertnt(tt, key, value);
        }
        mrx_debug_sanity_check_str2ref(mrx);
        //mrx_debug_print(mrx);

        str2ref_it_t *sit = str2ref_begin(shadow_tt);
        for (mrx_it_t *it = mrx_beginst(tt, alloca(mrx_itsize(tt))); it != mrx_end(); it = mrx_next(it)) {
            ASSERT(strcmp(mrx_key(it), str2ref_key(sit)) == 0);
            ASSERT(mrx_val(it) == str2ref_val(sit));
            sit = str2ref_next(sit);
        }
        ASSERT(sit == NULL);

        // with heap-allocated iterator
        sit = str2ref_begin(shadow_tt);
        for (mrx_it_t *it = mrx_begin(tt); it != mrx_end(); it = mrx_next(it)) {
            ASSERT(strcmp(mrx_key(it), str2ref_key(sit)) == 0);
            ASSERT(mrx_val(it) == str2ref_val(sit));
            sit = str2ref_next(sit);
        }
        ASSERT(sit == NULL);

        test_memleak_and_clear_memory(mrx);
        mrx_delete(tt);
        str2ref_delete(shadow_tt);
    }
    fprintf(stderr, "pass\n");
}

static void
test_mrx_nx_lp_to_sp(void)
{
#if ARCH_SIZEOF_PTR == 4
    // mrx_nx_lp_to_sp() is not used in 32 bit mode
    return;
#endif
    mrx_t *tt = mrx_new(~0u);
    mrx_base_t *mrx = &tt->mrx;

    fprintf(stderr, "Test: mrx_nx_lp_to_sp()...");
    {
        for (int alloc_lp = 0; alloc_lp < 2; alloc_lp++) {
            for (int bc = 0; bc <= 32; bc++) {
                for (int capacity = bc; capacity <= 32; capacity++) {
                    mrx_test_configure_allocator(false, true);
                    union mrx_next_block *nx = mrx_alloc_nx_node_lp_(mrx, capacity);
                    union mrx_node *ptrs[32];
                    for (int j = 0; j < bc; j++) {
                        ptrs[j] = EP2(j << 4, HP(nx));
                    }
                    union mrx_next_block *nxb = nx;
                    int ofs = 0;
                    for (int j = 0; j < bc; j++) {
                        if (j == 14 || j == 28) {
                            nxb = nxb->lp.next;
                            ofs += 14;
                        }
                        nxb->lp.lp[j-ofs] = ptrs[j];
                    }
                    mrx_test_configure_allocator(false, !alloc_lp);

                    union mrx_next_block *spnx = mrx_nx_lp_to_sp_(mrx, nx, bc);
                    ASSERT(NEXT_BLOCK_IS_SHORT_PTR(spnx));
                    uint8_t nsz = NEXT_BLOCK_HDR(spnx);
                    if (alloc_lp) {
                        ASSERT(nsz == 4);
                    } else {
                        if (bc < 3) {
                            ASSERT(nsz == 1);
                        } else if (bc < 6) {
                            ASSERT(nsz == 2);
                        } else if (bc < 13) {
                            ASSERT(nsz == 3);
                        } else {
                            ASSERT(nsz == 4);
                        }
                    }
                    if (nsz == 4) {
                        ASSERT(spnx == nx);
                    } else {
                        ASSERT(spnx != nx);
                    }
                    for (int j = 0; j < bc; j++) {
                        ASSERT(EP2(spnx->sp[j], HP(nx)) == ptrs[j]);
                    }
                    mrx_test_free_node(mrx, spnx, nsz);
                    test_memleak_and_clear_memory(mrx);
                }
            }
        }
    }
    fprintf(stderr, "pass\n");
    mrx_test_clear_allocator(mrx);
    mrx_delete(tt);
}

#if TRACKMEM_DEBUG - 0 != 0
extern trackmem_t *buddyalloc_tm;
trackmem_t *buddyalloc_tm;
trackmem_t *nodepool_tm;
#endif

int
main(void)
{
#if TRACKMEM_DEBUG - 0 != 0
    buddyalloc_tm = trackmem_new();
    nodepool_tm = trackmem_new();
#endif
    tausrand_init(taus_state, 0);
    mrx_test_init_allocator(16 * 1024 * 1024);
    mrx_test_allocator_.alloc_node = mrx_test_alloc_node;
    mrx_test_allocator_.free_node = mrx_test_free_node;

    test_base_macros_and_small_functions();
    test_prefix_move_up();
    test_scan_node_child_merge();
    test_erase_value_from_node();
    test_mask_node_erase_branch();
    test_scan_node_erase_branch();
    test_mask_node_insert_branch();
    test_scan_node_to_mask_node_and_enlarge_node();
    test_scan_node_insert_branch();
    test_scan_node_insert_value();
    test_set_link_in_parent();
    test_new_leaf();
    test_scan_node_split();
    test_iterator();
    test_mrx_nx_lp_to_sp();

#if TRACKMEM_DEBUG - 0 != 0
    trackmem_delete(nodepool_tm);
    trackmem_delete(buddyalloc_tm);
#endif
    mrx_test_destroy_allocator();
    return 0;
}
