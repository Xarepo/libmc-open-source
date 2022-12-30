/*
 * Copyright (c) 2013, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <mrx_base_int.h>

static void
iterator_scan_to_leftmost_value(mrx_iterator_t *it)
{
    union mrx_node *node = it->path[it->level].node;
    for (;;) {
        const uint8_t px_len = HDR_PX_LEN(node->hdr);
        it->key_len += px_len + 1;
        if (HDR_HAS_VALUE(node->hdr)) {
            break;
        }
        if (IS_SCAN_NODE(HDR_NSZ(node->hdr))) {
            const uint8_t * const br = &node->sn.octets[px_len];
            it->path[it->level].br_pos = 0;
            it->path[it->level].br = br[0];
            node = scan_node_get_child(node, br, HDR_BR_LEN(node->hdr), 0);
        } else {
            uint32_t b = mask_node_next_branch(node, 0);
            it->path[it->level].br_pos = (int16_t)b;
            it->path[it->level].br = b;
            uint32_t bm = node->mn.bitmask.u32[b >> 5u];
            union mrx_next_block *nx = mask_node_get_next_block(node, b);
            b = 1u << (b & 0x1Fu);
            bm &= b - 1;
            b = bit32_count(bm);
            node = next_block_get_child(nx, b);
        }
        it->level++;
        it->path[it->level].node = node;
    }
    it->path[it->level].br_pos = -1;
}

void
mrx_traverse_children_(mrx_base_t *mrx,
                       nl_t *node_list,
                       union mrx_node *node,
                       const bool do_free_traversed)
{
    const uint8_t nsz = HDR_NSZ(node->hdr);
    if (IS_SCAN_NODE(nsz)) {
        const uint8_t br_len = HDR_BR_LEN(node->hdr);
        uint8_t * const br = &node->sn.octets[HDR_PX_LEN(node->hdr)];
        mrx_sp_t * const brp = SP_ALIGN(&br[br_len]);
        for (uint8_t i = 0; i < br_len; i++) {
            nl_push_back(node_list, scan_node_get_child1(node, brp, i));
        }
        if (do_free_traversed) {
            ptrpfx_sn_free(mrx, node);
            free_node(mrx, node, nsz);
        }
    } else {
        union mrx_next_block *last_nx = NULL;
        int br = -1;
        while ((br = barr32_bsf(node->mn.bitmask.u32, br + 1, 255)) != -1) {
            uint32_t b = br;
            uint32_t bm = node->mn.bitmask.u32[b >> 5u];
            union mrx_next_block *nx = mask_node_get_next_block(node, b);
            if (do_free_traversed) {
                if (nx != last_nx) {
                    if (last_nx != NULL && last_nx != (union mrx_next_block *)node->mn.local) {
                        free_nx_node(mrx, last_nx);
                    }
                    last_nx = nx;
                }
            }
            b = 1u << (b & 0x1Fu);
            bm &= b - 1;
            b = bit32_count(bm);
            nl_push_back(node_list, next_block_get_child(nx, b));
        }
        if (do_free_traversed) {
            free_node(mrx, node, nsz);
            if (last_nx != NULL && last_nx != (union mrx_next_block *)node->mn.local) {
                free_nx_node(mrx, last_nx);
            }
        }
    }
}

void
mrx_traverse_erase_all_nodes_(mrx_base_t *mrx)
{
    // FIXME: can this be made more efficient? Compact delete/clear would be more efficient then
    if (mrx->root == NULL) {
        return;
    }
    nl_t *parents = nl_new(~0u, 64);
    nl_t *children = nl_new(~0u, 64);
    mrx_traverse_children_(mrx, parents, mrx->root, true);
    do {
        for (nl_it_t *it = nl_begin(parents); it != nl_end(parents); it = nl_next(it)) {
            mrx_traverse_children_(mrx, children, nl_val(it), true);
        }
        nl_clear(parents);
        nl_t *swaptmp = children;
        children = parents;
        parents = swaptmp;
    } while (nl_size(parents) > 0);
    nl_delete(children);
    nl_delete(parents);
}

size_t
mrx_itdynsize_(mrx_base_t *mrx)
{
    const uint32_t max_keylen = (mrx->max_keylen_n_flags & ~MRX_FLAGS_MASK_);
    return (max_keylen + 1) * sizeof(struct mrx_iterator_path_element) + max_keylen + 1;
}

void
mrx_itinit_(mrx_base_t *mrx,
            mrx_iterator_t *it)
{
    const uint32_t max_keylen = (mrx->max_keylen_n_flags & ~MRX_FLAGS_MASK_);
    it->key = &((uint8_t *)it)[sizeof(*it) + (max_keylen + 1) * sizeof(struct mrx_iterator_path_element)];
    it->key_len = 0;
    it->key_level = -1;
    it->key_level_len = 0;
    it->path[0].node = mrx->root;
    it->level = 0;
    iterator_scan_to_leftmost_value(it);
}

bool
mrx_next_(mrx_iterator_t *it)
{
    for (;;) {
        union mrx_node * const node = it->path[it->level].node;
        int br_pos = it->path[it->level].br_pos;
        if (IS_SCAN_NODE(HDR_NSZ(node->hdr))) {
            const uint8_t br_len = HDR_BR_LEN(node->hdr);
            if (br_pos + 1 < br_len) {
                uint8_t * const br = &node->sn.octets[HDR_PX_LEN(node->hdr)];
                br_pos++;
                it->path[it->level].br_pos = (int16_t)br_pos;
                it->path[it->level].br = br[br_pos];
                it->level++;
                it->path[it->level].node = scan_node_get_child(node, br, br_len, br_pos);
                iterator_scan_to_leftmost_value(it);
                return true;
            }
        } else {
            br_pos = mask_node_next_branch(node, br_pos + 1);
            if (br_pos != -1) {
                uint32_t b = br_pos;
                uint32_t bm = node->mn.bitmask.u32[b >> 5u];
                union mrx_next_block *nx = mask_node_get_next_block(node, b);
                it->path[it->level].br_pos = (int16_t)br_pos;
                it->path[it->level].br = (uint8_t)br_pos;
                it->level++;
                b = 1u << (b & 0x1Fu);
                bm &= b - 1;
                b = bit32_count(bm);
                it->path[it->level].node = next_block_get_child(nx, b);
                iterator_scan_to_leftmost_value(it);
                return true;
            }
        }
        it->key_len -= HDR_PX_LEN(node->hdr) + 1;
        it->level--;
        if (it->level == -1) {
            break;
        }
        if (it->level < it->key_level) {
            it->key_level = it->level;
            it->key_level_len = it->key_len;
        }
    }
    return false;
}

const uint8_t *
mrx_key_(mrx_iterator_t *it)
{
    int pos = it->key_level_len;
    int i = it->key_level + 1;
    for (; i < it->level; i++) {
        const union mrx_node * const node = it->path[i].node;
        const uint8_t px_len = HDR_PX_LEN(node->hdr);
        memcpy(&it->key[pos], node->sn.octets, px_len);
        pos += px_len;
        it->key[pos++] = it->path[i].br;
    }
    if (it->key_level_len > 0) {
        it->key[it->key_level_len - 1] = it->path[it->key_level].br;
    }
    const union mrx_node * const node = it->path[i].node;
    const uint8_t px_len = HDR_PX_LEN(node->hdr);
    memcpy(&it->key[pos], node->sn.octets, px_len);
    pos += px_len;
    it->key[pos] = '\0';
    it->key_level = it->level;
    it->key_level_len = it->key_len;
    return it->key;
}
