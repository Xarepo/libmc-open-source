/*
 * Copyright (c) 2013, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>

#include <mrx_base_int.h>

static int
unused_node_space(union mrx_node *node,
                  int *totsize,
                  int *has_lp,
                  int *is_mask,
                  int *nx_count)
{
    /* we define unused space as such space in a node that is not accessed, ie
       we do not count 'inefficient' layouts such as extra pointers, unnecessary
       wide fields etc */
    const uint8_t nsz = HDR_NSZ(node->hdr);
    const uint8_t px_len = HDR_PX_LEN(node->hdr);
    const uint16_t br_len = HDR_BR_LEN(node->hdr);
    const uint8_t size = (((8u << (unsigned)nsz) - 1u) & 0x7Fu) + 1u;
    *nx_count = 0;
    if (IS_MASK_NODE(nsz)) {
        *totsize = 128;
        *is_mask = 1;
        int unused = 128 - 2 - 1 - 8*4;
        if (HDR_HAS_VALUE(node->hdr)) {
            unused -= sizeof(void *);
        }
        unused -= 8*4;
        uint8_t i;
	for (i = 0; i < 8; i++) {
            if (node->mn.bitmask.u32[i] == 0) {
                unused += 4;
            }
        }
        unused -= MASK_NODE_MAX_LOCAL_BRANCH_COUNT * 4;
        if (!HDR_MN_LOCAL_USED(node->hdr)) {
            // local storage is not used
            unused += MASK_NODE_MAX_LOCAL_BRANCH_COUNT * 4;
        } else {
            unused += MASK_NODE_MAX_LOCAL_BRANCH_COUNT * 4;
            for (i = 0; i < 8; i++) {
                int bc = bit32_count(node->mn.bitmask.u32[i]);
                union mrx_next_block *nx = mask_node_get_next_block(node, (unsigned)i << 5u);
                if (bc > 0 && nx == (union mrx_next_block *)node->mn.local) {
                    unused -= bc * 4;
                    break;
                }
            }
        }
        *has_lp = 0;
        if (!HDR_IS_SHORT_PTR(node->hdr)) {
            unused -= 8 * 4;
            *has_lp = 1;
        }
        for (i = 0; i < 8; i++) {
            int bc = bit32_count(node->mn.bitmask.u32[i]);
            if (bc == 0) {
                continue;
            }
            union mrx_next_block *nx = mask_node_get_next_block(node, (unsigned)i << 5u);
            if (nx == (union mrx_next_block *)node->mn.local) {
                continue;
            }
            if (NEXT_BLOCK_IS_SHORT_PTR(nx)) {
                *totsize += 8u << NEXT_BLOCK_HDR(nx);
                unused += (8u << NEXT_BLOCK_HDR(nx)) - bc*4;
                (*nx_count)++;
            } else {
                (*nx_count)++;
                *has_lp = 1;
                int overhead_size = 2 * sizeof(void *);
                int nx_size = 128;
                if (nx->lp.next != NULL) {
                    (*nx_count)++;
                    nx_size += 128;
                    overhead_size += sizeof(void *);
                    union mrx_next_block *nx1 = nx->lp.next;
                    if (nx1->lp.next != NULL) {
                        (*nx_count)++;
                        nx_size += 64;
                    }
                }
                *totsize += nx_size;
                unused += nx_size - overhead_size - bc*8;
            }
        }
        return unused;
    } else {
        *is_mask = 0;
        *totsize = size;
        int unused = size - 2 - px_len - br_len - br_len * sizeof(mrx_sp_t);
        if (HDR_HAS_VALUE(node->hdr)) {
            unused -= sizeof(void *);
        }
        *has_lp = 0;
        if (!HDR_IS_SHORT_PTR(node->hdr)) {
            *has_lp = 1;
            struct mrx_ptrpfx_node *pfx = SN_PREFIX_PTR(node);
            const uint8_t pfx_nsz = HDR_NSZ(pfx->hdr);
            uint8_t pfx_size = 8u << pfx_nsz;
            unused += pfx_size - 2 - 2 - br_len * sizeof(mrx_sp_t);
            *totsize += pfx_size;
        }
        return unused;
    }
}

static void
print_node(union mrx_node *node,
           int do_sanity_check)
{
    const uint8_t nsz = HDR_NSZ(node->hdr);
    const uint8_t px_len = HDR_PX_LEN(node->hdr);
    const uint16_t br_len = HDR_BR_LEN(node->hdr);
    const uint8_t *px = node->sn.octets;
    const uint8_t *br = &px[px_len];

    int totsize;
    int has_lp;
    int is_mask;
    int nx_count;
    int unused = unused_node_space(node, &totsize, &has_lp, &is_mask, &nx_count);
    if (IS_MASK_NODE(nsz)) {
        (void)fprintf(stderr, "mn %p (sz:%d unused:%d lp:%d) {\n", node, totsize, unused,
                      !HDR_IS_SHORT_PTR(node->hdr));
    } else {
        (void)fprintf(stderr, "sn %p (sz:%d unused:%d lp:%d) {\n", node, totsize, unused,
                      !HDR_IS_SHORT_PTR(node->hdr));
    }
    if (px_len > 0) {
        int i;
        char s[px_len + 1];
        memcpy(s, px, px_len);
        s[px_len] = '\0';
        for (i = 0; i < px_len; i++) {
            if (!isprint(s[i])) {
                break;
            }
        }
        if (i != px_len) {
            fprintf(stderr, "    prefix: %d { ", px_len);
            for (i = 0; i < px_len; i++) {
                fprintf(stderr, "0x%02X ", (uint8_t)s[i]);
            }
            fprintf(stderr, "}\n");
        } else {
            fprintf(stderr, "    prefix: %d {%s}\n", px_len, s);
        }
    } else if (IS_SCAN_NODE(nsz)) {
        fprintf(stderr, "    prefix: 0\n");
    }
    if (IS_MASK_NODE(nsz)) {
        int pos = -1;
        int i;
        uint16_t brl = 0;
        char s[257];
        int is_printable = 1;
        while ((pos = barr32_bsf(node->mn.bitmask.u32, pos + 1, 255)) != -1) {
            s[brl] = (char)pos;
            if (!isprint(s[brl])) {
                is_printable = 0;
            }
            brl++;
        }
        s[brl] = '\0';
        if (is_printable) {
            fprintf(stderr, "    branch: %d {%s} =>\n      {\n", brl, s);
        } else {
            fprintf(stderr, "    branch: %d { ", brl);
            for (i = 0; i < brl; i++) {
                fprintf(stderr, "0x%02X ", (uint8_t)s[i]);
            }
            fprintf(stderr, " } =>\n      {\n");
        }
	for (i = 0; i < 8; i++) {
            uint8_t j;
            uint8_t bc = bit32_count(node->mn.bitmask.u32[i]);
            union mrx_next_block *nx = mask_node_get_next_block(node, (unsigned)i << 5u);
            if (bc > 0) {
                if (nx == (union mrx_next_block *)node->mn.local) {
                    fprintf(stderr, "        [%p sz:%d bc:%d] { ",
                            nx,
                            MASK_NODE_MAX_LOCAL_BRANCH_COUNT * 4, bc);
                } else {
                    if (NEXT_BLOCK_IS_SHORT_PTR(nx)) {
                        fprintf(stderr, "        [%p sz:%d, bc:%d] { ",
                                nx, 8u << NEXT_BLOCK_HDR(nx), bc);
                    } else {
                        if (nx->lp.next != NULL) {
                            union mrx_next_block *nx1 = nx->lp.next;
                            if (nx1->lp.next != NULL) {
                                union mrx_next_block *nx2 = nx1->lp.next;
                                fprintf(stderr, "        [%p->%p->%p sz:320 bc:%d lpc:%d] { ",
                                        nx, nx1, nx2, bc, nx->lp.lp_count);
                            } else {
                                fprintf(stderr, "        [%p->%p sz:256 bc:%d lpc:%d] { ",
                                        nx, nx1, bc, nx->lp.lp_count);
                            }
                        } else {
                            fprintf(stderr, "        [%p sz:128 bc:%d lpc:%d] { ",
                                    nx, bc, nx->lp.lp_count
                                );
                        }
                    }
                }
            }
            for (j = 0; j < bc; j++) {
                if (bc > 8 && j % 8 == 0) {
                    fprintf(stderr, "\n          ");
                }
                fprintf(stderr, "%p ", next_block_get_child(nx, j));
            }
            if (bc > 0) {
                if (bc > 8) {
                    fprintf(stderr, "\n       ");
                }
                fprintf(stderr, " }\n");
            }
	}
        fprintf(stderr, "      }\n");
    } else {
        if (br_len > 0) {
            int i;
            char s[br_len + 1];
            memcpy(s, br, br_len);
            s[br_len] = '\0';
            for (i = 0; i < br_len; i++) {
                if (!isprint(s[i])) {
                    break;
                }
            }
            if (i != br_len) {
                fprintf(stderr, "    branch: %d { ", br_len);
                for (i = 0; i < br_len; i++) {
                    fprintf(stderr, "0x%02X ", (uint8_t)s[i]);
                }
                fprintf(stderr, "} => { ");
            } else {
                fprintf(stderr, "    branch: %d {%s} => { ", br_len, s);
            }
            for (i = 0; i < br_len; i++) {
                if (br_len > 8 && i % 8 == 0) {
                    fprintf(stderr, "\n          ");
                }
                fprintf(stderr, "%p ",
                        scan_node_get_child1(node, SP_ALIGN(&br[br_len]), i));
            }
            if (br_len > 8) {
                fprintf(stderr, "\n        ");
            }
            fprintf(stderr, "}\n");
        } else {
            fprintf(stderr, "    branch: 0\n");
        }
    }
    if (HDR_HAS_VALUE(node->hdr)) {
//        fprintf(stderr, "    value: %p\n", mrx_node_value_ref_(node, nsz));
        fprintf(stderr, "    value: %p [%p]\n", mrx_node_value_ref_(node, nsz),
               * (void **)mrx_node_value_ref_(node, nsz));
    }
    fprintf(stderr, "}\n");

    if (do_sanity_check) {
        mrx_debug_sanity_check_node(node);
    }
}

void
mrx_print_node_(union mrx_node *node)
{
    print_node(node, 1);
}

void
mrx_debug_print(mrx_base_t *mrx)
{
    union mrx_node *node;
    int level = 0;
    nl_t *parents = nl_new(~0u, 64);
    nl_t *children = nl_new(~0u, 64);

    if (mrx->root == NULL) {
        fprintf(stderr, "_________________ tree is empty _________________\n");
        return;
    }
    nl_push_back(parents, mrx->root);
    fprintf(stderr, "\n\n____ level 0 (node count: 1) "
            "____________________________________"
            "____________________________________________"
            "\n\n");
    mrx_print_node_(mrx->root);
    level++;
    for (;;) {
        while ((node = nl_pop_back(parents)) != NULL) {
            mrx_traverse_children_(NULL, children, node, false);
        }
        if (nl_size(children) == 0) {
            break;
        }
        fprintf(stderr, "\n\n____ level %d (node count: %u) "
                "____________________________________"
                "____________________________________________"
                "\n\n",
                level, (int)nl_size(children));
        while ((node = nl_pop_back(children)) != NULL) {
            mrx_print_node_(node);
            nl_push_back(parents, node);
        }
        level++;
    }
    nl_delete(parents);
    nl_delete(children);
    fprintf(stderr, "\n\n____ end of tree "
            "____________________________________"
            "____________________________________________"
            "\n\n");
}

#define MRB_PRESET_const_str_TO_REF_COPY_KEY
#include <mrb_tmpl.h>

static inline void *
get_val(mrx_iterator_t *it)
{
    union mrx_node *node = it->path[it->level].node;
    const uint8_t nsz = MRX_NODE_HDR_NSZ_(node->hdr);
    return *(void **)mrx_node_value_ref_(node, nsz);
}

void
mrx_debug_sanity_check_str2ref(mrx_base_t *mrx)
{
    if (mrx->root == NULL) {
        return;
    }
    str2ref_t *ref = str2ref_new(~0u);
    mrx_iterator_t *it = malloc(sizeof(*it) + mrx_itdynsize_(mrx));
    mrx_itinit_(mrx, it);
    unsigned int key_count = 0;
    do {
        const char *key = (const char *)mrx_key_(it);
        void *val = get_val(it);
        //(void)fprintf(stderr, "[%s] %p\n", key, val);
        str2ref_insert(ref, key, val);
        key_count++;
    } while (mrx_next_(it));
    if (str2ref_size(ref) != key_count) {
        (void)fprintf(stderr, "size mismatch, ref %u mrx %u\n", (int)str2ref_size(ref), key_count);
        mrx_debug_print(mrx);
        abort();
    }

    str2ref_it_t *itr = str2ref_begin(ref);
    mrx_itinit_(mrx, it);
    do {
        const char *key = (const char *)mrx_key_(it);
        if (strcmp(key, str2ref_key(itr)) != 0) {
            (void)fprintf(stderr, "key mismatch (got \"%s\", expected \"%s\")\n", key, str2ref_key(itr));
            mrx_debug_print(mrx);
            abort();
        }
        if (get_val(it) != str2ref_val(itr)) {
            (void)fprintf(stderr, "value mismatch\n");
            abort();
        }
        void **valp = (void **)mrx_find_(mrx->root,
                                         (const uint8_t *)str2ref_key(itr),
                                         strlen(str2ref_key(itr)));
        if (valp == NULL) {
            (void)fprintf(stderr, "could not find key \"%s\"\n", key);
            mrx_debug_print(mrx);
            abort();
        }
        if (*valp != str2ref_val(itr)) {
            (void)fprintf(stderr, "find mismatch [%s] -> %p not %p\n", str2ref_key(itr), *valp, str2ref_val(itr));
            abort();
        }
        itr = str2ref_next(itr);
    } while (mrx_next_(it));
    str2ref_delete(ref);
    free(it);
}

#ifndef MC_PREFIX
#define MC_PREFIX mrb
#define MC_KEY_T uintptr_t
#define MC_VALUE_T void *
#endif
#include <mrb_tmpl.h>

void
mrx_debug_sanity_check_int2ref(mrx_base_t *mrx)
{
    if (mrx->root == NULL) {
        return;
    }
    const uint32_t max_keylen = (mrx->max_keylen_n_flags & ~MRX_FLAGS_MASK_);
    if (max_keylen != sizeof(uintptr_t)) {
	(void)fprintf(stderr, "cannot run mrx_debug_sanity_check_int2ref(), key size mismatch\n");
	abort();
    }
    mrb_t *ref = mrb_new(~0u);
    mrx_iterator_t *it = malloc(sizeof(*it) + mrx_itdynsize_(mrx));
    mrx_itinit_(mrx, it);
    unsigned int key_count = 0;
    do {
        uintptr_t key;
        memcpy(&key, mrx_key_(it), sizeof(uintptr_t));
        if (sizeof(uintptr_t) == 8) {
            key = bit64_swap(key);
        } else {
            key = bit32_swap(key);
        }
        void *val = get_val(it);
        if (val == NULL) {
            (void)fprintf(stderr, "null value in tree, sanity check does not support that");
            mrx_debug_print(mrx);
            abort();
        }
        mrb_insert(ref, key, val);
        key_count++;
    } while (mrx_next_(it));
    if (mrb_size(ref) != key_count) {
        (void)fprintf(stderr, "size mismatch, ref %u mrx %u\n", (int)mrb_size(ref), key_count);
        mrx_debug_print(mrx);
        abort();
    }
    /*{
        for (mrb_it_t *itr = mrb_begin(ref); itr != mrb_end(); itr = mrb_next(itr)) {
            fprintf(stderr, ">> %zd 0x%016zX %p\n", (size_t)mrb_key(itr), (size_t)mrb_key(itr), mrb_val(itr));
        }
        }*/

    int i = 0;
    mrb_it_t *itr = mrb_begin(ref);
    mrx_itinit_(mrx, it);
    do {
        uintptr_t key;
        memcpy(&key, mrx_key_(it), sizeof(uintptr_t));
        if (sizeof(uintptr_t) == 8) {
            key = bit64_swap(key);
        } else {
            key = bit32_swap(key);
        }
        if (key != mrb_key(itr)) {
            (void)fprintf(stderr, "key mismatch at %d (got 0x%016zX, expected 0x%016zX)\n",
                          i, (size_t)key, (size_t)mrb_key(itr));
            mrx_debug_print(mrx);
            abort();
        }
        if (get_val(it) != mrb_val(itr)) {
            (void)fprintf(stderr, "value mismatch\n");
            abort();
        }
        uintptr_t skey;
        if (sizeof(uintptr_t) == 8) {
            skey = bit64_swap(key);
        } else {
            skey = bit32_swap(key);
        }
        void **valp = (void **)mrx_find_(mrx->root, (const uint8_t *)&skey,
                                         sizeof(uintptr_t));
        if (valp == NULL) {
            (void)fprintf(stderr, "could not find key 0x%016zX\n", (size_t)key);
            mrx_debug_print(mrx);
            abort();
        }
        if (*valp != mrb_val(itr)) {
            (void)fprintf(stderr, "find mismatch 0x%016zX -> %p not %p\n", (size_t)mrb_key(itr), *valp, mrb_val(itr));
            abort();
        }
        itr = mrb_next(itr);
        i++;
    } while (mrx_next_(it));
    mrb_delete(ref);
    free(it);
}

void
mrx_debug_sanity_check_node_with_children(union mrx_node *node)
{
    nl_t *parents = nl_new(~0u, 64);
    nl_t *children = nl_new(~0u, 64);

    nl_push_back(parents, node);
    for (;;) {
        while ((node = nl_pop_back(parents)) != NULL) {
            mrx_debug_sanity_check_node(node);
            mrx_traverse_children_(NULL, children, node, false);
        }
        if (nl_size(children) == 0) {
            break;
        }
        nl_t *tmp = children;
        children = parents;
        parents = tmp;
    }
    nl_delete(parents);
    nl_delete(children);
}

void
mrx_debug_sanity_check_node(union mrx_node *node)
{
    if (((uintptr_t)node & ((2*sizeof(void *))-1)) != 0) {
        (void)fprintf(stderr, "misaligned node pointer %p\n", node);
        abort();
    }
    uint8_t zero[128];
    memset(zero, 0, sizeof(zero));
    const uint8_t nsz = HDR_NSZ(node->hdr);
    const uint8_t px_len = HDR_PX_LEN(node->hdr);
    const uint16_t br_len = HDR_BR_LEN(node->hdr);
    const uint8_t size = (((8u << (unsigned)nsz) - 1u) & 0x7Fu) + 1u;
    if (IS_SCAN_NODE(nsz)) {
        const uint8_t *px = node->sn.octets;
        const uint8_t *br = &px[px_len];
        for (unsigned i = 1; i < br_len; i++) {
            if (br[i-1] >= br[i]) {
                (void)fprintf(stderr, "Node %p branch array not sorted\n", node);
                print_node(node, 0);
                abort();
            }
        }
        if ((size != 128 && size != 64 && size != 32 && size != 16 && size != 8) ||
            (sizeof(uintptr_t) == 8 && size == 8))
        {
            (void)fprintf(stderr, "Node %p has bad size (got %d)\n", node, size);
            print_node(node, 0);
            abort();
        }
        uint8_t min_size = SCAN_NODE_MIN_SIZE(px_len, br_len, HDR_HAS_VALUE(node->hdr));
        if (min_size > size) {
            (void)fprintf(stderr, "Node %p is too small! Size must be at least %u (px_len %u br_len %u), but is only %u (nsz %u)\n",
                          node, min_size, px_len, br_len, size, nsz);
            print_node(node, 0);
            abort();
        }
        if (!HDR_IS_SHORT_PTR(node->hdr)) {
            struct mrx_ptrpfx_node *pfx = SN_PREFIX_PTR(node);
            const uint8_t pfx_nsz = HDR_NSZ(pfx->hdr);
            if ((nsz == 1 && pfx_nsz != 2) ||
                (nsz == 2 && pfx_nsz != 3) ||
                (nsz == 3 && pfx_nsz != 3) ||
                (nsz > 3 && pfx_nsz != 4))
            {
                fprintf(stderr, "node %p: bad pointer prefix node size "
                        "(pfx_nsz %u, nsz %u, pfx %p)\n", node, pfx_nsz, nsz, pfx);
                print_node(node, 0);
                abort();
            }
            int actual_lp_count = 0;
            uint8_t i;
            for (i = 0; i < br_len; i++) {
                if (!PTR_PREFIX_MATCHES(scan_node_get_child1(node, SP_ALIGN(&br[br_len]), i), node)) {
                    actual_lp_count++;
                }
            }
            if (pfx->lp_count != actual_lp_count) {
                fprintf(stderr, "node %p pfx %p bad lp_count, stored %d "
                        "actual %d\n", node, pfx, pfx->lp_count, actual_lp_count);
                print_node(node, 0);
                abort();
            }
        }
        if (br_len == 0 && !HDR_HAS_VALUE(node->hdr)) {
            fprintf(stderr, "node %p missing both branches and value\n", node);
            print_node(node, 0);
            abort();
        }
    } else {
        if (size != 128) {
            fprintf(stderr, "Node %p has bad size (got %d)\n", node, size);
            print_node(node, 0);
            abort();
        }
        int nx_lp_count = 0;
        unsigned branch_count = 0;
        for (uint8_t i = 0; i < 4; i++) {
            branch_count += bit64_count(node->mn.bitmask.u64[i]);
        }
        if (node->mn.branch_count != branch_count && (node->mn.branch_count != 0 || branch_count != 256)) {
            fprintf(stderr, "Node %p branch count field does not match actual count in bitmask (got %d, bit mask has %d)\n", node, node->mn.branch_count, branch_count);
            print_node(node, 0);
            abort();
        }
        for (uint8_t i = 0; i < 8; i++) {
            uint8_t bc = (uint8_t)bit32_count(node->mn.bitmask.u32[i]);
            if (bc == 0) {
                continue;
            }
            union mrx_next_block *nx = mask_node_get_next_block(node, (unsigned)i << 5u);
            if (!PTR_PREFIX_MATCHES(nx, node)) {
                nx_lp_count++;
            }
            if (!NEXT_BLOCK_IS_SHORT_PTR(nx)) {
                int actual_lp_count = 0;
                union mrx_next_block *nxb = nx;
                uint8_t ofs = 0;
                for (uint8_t j = 0; j < bc; j++) {
                    if (j == 14 || j == 28) {
                        nxb = nxb->lp.next;
                        ofs += 14;
                    }
                    if (nxb->lp.lp[j-ofs] == NULL) {
                        (void)fprintf(stderr, "node %p next %p null pointer\n", node, nx);
                        print_node(node, 0);
                        abort();
                    }
                    if (!PTR_PREFIX_MATCHES(nxb->lp.lp[j-ofs], nx)) {
                        actual_lp_count++;
                    }
                }
                if (nx->lp.lp_count != actual_lp_count) {
                    (void)fprintf(stderr, "node %p next %p bad lp_count, stored %d actual %d\n", node, nx, nx->lp.lp_count, actual_lp_count);
                    print_node(node, 0);
                    abort();
                }
                if (nx->lp.lp_count == 0) {
                    (void)fprintf(stderr, "node %p next %p has zero lp_count\n", node, nx);
                    print_node(node, 0);
                    abort();
                }
            } else {
                if (nx != (union mrx_next_block *)node->mn.local) {
                    int nx_size = 8u << NEXT_BLOCK_HDR(nx);
                    if (nx_size != 16 && nx_size != 32 && nx_size != 64 &&
                        nx_size != 128 && nx_size != 2 * sizeof(uintptr_t))
                    {
                        (void)fprintf(stderr, "node %p next %p has bad size (%u)\n", node, nx, nx_size);
                        print_node(node, 0);
                        abort();
                    }
                }
            }
        }
        if (HDR_IS_SHORT_PTR(node->hdr)) {
            assert(nx_lp_count == 0);
            if (!HDR_MN_LOCAL_USED(node->hdr)) {
                for (uint8_t i = 0; i < 8; i++) {
                    uint8_t bc = (uint8_t)bit32_count(node->mn.bitmask.u32[i]);
                    if (bc == 0) {
                        continue;
                    }
                    if (node->mn.next[i] == SP(node->mn.local)) {
                        (void)fprintf(stderr, "node %p points to local but has not local flag set\n", node);
                        print_node(node, 0);
                        abort();
                    }
                    union mrx_next_block *nx = mask_node_get_next_block(node, (unsigned)i << 5u);
                    if (bc <= MASK_NODE_LOCAL_BRANCH_COUNT_FOR_MOVING_BACK &&
                        NEXT_BLOCK_IS_SHORT_PTR(nx) &&
                        // special case in long pointer conversions, 128 bit node may be kept
                        (8u << NEXT_BLOCK_HDR(nx)) != 128)
                    {
                        (void)fprintf(stderr, "node %p fits local branches but does not have it\n", node);
                        print_node(node, 0);
                        abort();
                    }
                }
            } else {
                int found_local = 0;
                for (uint8_t i = 0; i < 8; i++) {
                    uint8_t bc = bit32_count(node->mn.bitmask.u32[i]);
                    if (bc == 0) {
                        continue;
                    }
                    if (node->mn.next[i] == SP(node->mn.local)) {
                        if (found_local) {
                            (void)fprintf(stderr, "node %p duplicate local\n", node);
                            print_node(node, 0);
                            abort();
                        }
                        if (bc > MASK_NODE_MAX_LOCAL_BRANCH_COUNT) {
                            (void)fprintf(stderr, "node %p too many local branches\n", node);
                            print_node(node, 0);
                            abort();
                        }
                        found_local = 1;
                    }
                }
                if (!found_local) {
                    (void)fprintf(stderr, "node %p no local pointer despite local flag set\n", node);
                    print_node(node, 0);
                    abort();
                }
            }
        } else {
            if (node->mn.lp_count != nx_lp_count) {
                (void)fprintf(stderr, "node %p bad lp_count, stored %d actual %d\n", node, node->mn.lp_count, nx_lp_count);
                print_node(node, 0);
                abort();
            }
        }

#if 0
	// if nodes are checked when half-finished this check should not be
	// active as it checks child node pointers
        for (uint8_t i = 0; i < 8; i++) {
            int bc = bit32_count(node->mn.bitmask.u32[i]);
            if (bc == 0) {
                continue;
            }
            union mrx_next_block *nx = mask_node_get_next_block(node, i << 5);
            if (NEXT_BLOCK_IS_SHORT_PTR(nx)) {
                for (uint8_t j = 0; j < bc; j++) {
                    union mrx_node *child = EP(nx->sp[j], nx);
                    const uint8_t cnsz = HDR_NSZ(child->hdr);
                    if (!mrx_alloc_debug_verify_node_pointer(child, cnsz)) {
                        fprintf(stderr, "bad child pointer %p\n", child);
                        print_node(node, 0);
                        abort();
                    }
                    mrx_debug_sanity_check_node(child);
                }
            } else {
                union mrx_next_block *nxb = nx;
                uint8_t ofs = 0;
                for (uint8_t j = 0; j < bc; j++) {
                    if (j == 14 || j == 28) {
                        nxb = nxb->lp.next;
                        ofs += 14;
                    }
                    union mrx_node *child = nxb->lp.lp[j-ofs];
                    const uint8_t cnsz = HDR_NSZ(child->hdr);
                    if (!mrx_alloc_debug_verify_node_pointer(child, cnsz)) {
                        fprintf(stderr, "bad child pointer %p\n", child);
                        print_node(node, 0);
                        abort();
                    }
                    mrx_debug_sanity_check_node(child);
                }
            }
        }
#endif

    }
}

static uintptr_t
make_prefix(void *ptr)
{
#if ARCH_SIZEOF_PTR == 4
    return 0;
#else
    return (uintptr_t)ptr >> 32u;
#endif
}

void
mrx_debug_memory_stats(mrx_base_t *mrx,
                       const size_t fixed_key_size)
{
    if (mrx->root == NULL) {
        return;
    }

    mrb_t *prefix_counts = mrb_new(~0u);
    union mrx_node *node;
    int level = 0;
    nl_t *parents = nl_new(~0u, 64);
    nl_t *children = nl_new(~0u, 64);

    int totsz;
    int has_lp;
    int nx_count;
    int is_mask;
    size_t node_count = 0;
    size_t next_count = 0;
    size_t tot_size = 0;
    size_t unused_size = 0;
    size_t lp_count = 0;
    size_t mask_count = 0;
    mrb_insert(prefix_counts, make_prefix(mrx->root), (void *)1);
    nl_push_back(parents, mrx->root);
    unused_size += unused_node_space(mrx->root, &totsz, &has_lp, &is_mask, &nx_count);
    mrx_debug_sanity_check_node(mrx->root);
    tot_size += totsz;
    lp_count += has_lp;
    next_count += nx_count;
    mask_count += is_mask;
    node_count++;
    level++;
    for (;;) {
        while ((node = nl_pop_back(parents)) != NULL) {
            mrx_traverse_children_(NULL, children, node, false);
        }
        if (nl_size(children) == 0) {
            break;
        }
        node_count += nl_size(children);
        while ((node = nl_pop_back(children)) != NULL) {
            unused_size += unused_node_space(node, &totsz, &has_lp, &is_mask, &nx_count);
            mrx_debug_sanity_check_node(node);
            tot_size += totsz;
            lp_count += has_lp;
            next_count += nx_count;
            mask_count += is_mask;
            nl_push_back(parents, node);
	    uintptr_t prefix = make_prefix(node);
	    uintptr_t count = (uintptr_t)mrb_find(prefix_counts, prefix) + 1;
	    mrb_insert(prefix_counts, prefix, (void *)count);
        }
        level++;
    }
    nl_delete(parents);
    nl_delete(children);

    mrx_iterator_t *it = malloc(sizeof(*it) + mrx_itdynsize_(mrx));
    mrx_itinit_(mrx, it);
    size_t key_count = 0;
    size_t key_tot_len = 0;
    size_t key_max_len = fixed_key_size;
    do {
        if (fixed_key_size == 0) {
            const char *key = (const char *)mrx_key_(it);
            unsigned int slen = strlen(key);
            key_tot_len += slen;
            if (slen > key_max_len) {
                key_max_len = slen;
            }
        } else {
            key_tot_len += fixed_key_size;
        }
        key_count++;
    } while (mrx_next_(it));
    free(it);
    struct mrx_debug_allocator_stats stats;
    mrx_alloc_debug_stats(mrx, &stats);
    char pc_str[mrb_size(prefix_counts) * 15 + 10];
    char *pc_str_p = pc_str;
    mrb_it_t *it1;
    for (it1 = mrb_begin(prefix_counts); it1 != mrb_end(); it1 = mrb_next(it1)) {
	uintptr_t count = (uintptr_t)mrb_val(it1);
        pc_str_p += sprintf(pc_str_p, "%u ", (int)count);
    }
    pc_str_p[-1] = '\0';
    size_t allocated = (tot_size + stats.freelist_size + stats.unused_superblock_size);
    (void)fprintf(stderr,
                  "tree statistics:\n"
                  "  levels: .......... %d\n"
                  "  key max length ... %zu\n"
                  "  key/value count ........... %zu\n"
                  "  scan node count ........... %zu\n"
                  "  mask node count ........... %zu\n"
                  "  next block count .......... %zu\n"
                  "  total count ............... %zu\n"
                  "  long pointer node count ... %zu (%.2f%%)\n"
                  "  pointer prefix count ...... %zu (%s)\n"
                  "  node total size ......................... %zu\n"
                  "  node total internally unused size ....... %zu (%.2f%%)\n"
                  "  nodepool total externally unused size ... %zu\n"
                  "  nodepool total overhead size ............ %zu\n"
                  "  node used size / allocated .............. %.2f%%\n"
                  "  key+value size / allocated .............. %.2f%%\n"
                  "  \n",
                  level, key_max_len, key_count, node_count - mask_count, mask_count,
                  next_count,
                  node_count, lp_count,
                  100.0 * (double)lp_count / node_count,
                  mrb_size(prefix_counts), pc_str,
                  tot_size, unused_size,
                  100.0 * (double)unused_size / tot_size,
                  stats.freelist_size, stats.unused_superblock_size,
                  100.0 * (double)(tot_size - unused_size) / allocated,
                  100.0 * (double)(key_tot_len + key_count * sizeof(void *)) / allocated);
    mrb_delete(prefix_counts);
}
