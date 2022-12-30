/*
 * Copyright (c) 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#ifndef MRX_TEST_NODE_H
#define MRX_TEST_NODE_H

#include <mrx_base.h>

union mrx_node *
mrx_test_generate_scan_node(mrx_base_t *mrx,
                            bool use_default_lp,
                            bool is_long_pointer,
                            bool has_value,
                            uint8_t px_len,
                            uint8_t br_len,
                            uint32_t taus_state[3]);

union mrx_node *
mrx_test_generate_mask_node(mrx_base_t *mrx,
                            bool use_default_lp,
                            bool is_long_pointer,
                            bool has_value,
                            bool use_local,
                            uint32_t taus_state[3]);

#endif
