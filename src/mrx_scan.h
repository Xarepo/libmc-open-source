/*
 * Copyright (c) 2013, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#ifndef MRX_SCAN_H
#define MRX_SCAN_H

#include <stdint.h>

uint8_t
mrx_find_branch_ref_impl_(uint8_t c,
                          const uint8_t *br,
                          uint8_t len);
uint8_t
mrx_find_branch_generic_(uint8_t c,
                         const uint8_t *br,
                         uint8_t len);

uint8_t
mrx_find_new_branch_pos_ref_impl_(uint8_t c,
                                  const uint8_t *br,
                                  uint8_t len);

uint8_t
mrx_find_new_branch_pos_generic_(uint8_t c,
                                 const uint8_t *br,
                                 uint8_t len);

uint8_t
mrx_prefix_find_first_diff_ref_impl_(const uint8_t *a,
                                     const uint8_t *b,
                                     uint8_t len);

uint8_t
mrx_prefix_find_first_diff_generic_(const uint8_t *str,
                                    const uint8_t *prefix,
                                    uint8_t prefix_len);

uint8_t
mrx_prefix_find_first_diff_sse2_(const uint8_t str[],
                                 const uint8_t prefix[],
                                 uint8_t prefix_len);

uint8_t
mrx_find_branch_sse2_(uint8_t c,
                      const uint8_t *br,
                      uint8_t len);

uint8_t
mrx_find_new_branch_pos_sse2_(uint8_t c,
                              const uint8_t *br,
                              uint8_t len);

uint8_t
mrx_prefix_find_first_diff_sse42_(const uint8_t str[],
                                  const uint8_t prefix[],
                                  uint8_t prefix_len);

uint8_t
mrx_find_branch_sse42_(uint8_t c,
                       const uint8_t br[],
                       uint8_t len);

uint8_t
mrx_prefix_find_first_diff_avx2_(const uint8_t str[],
                                 const uint8_t prefix[],
                                 uint8_t prefix_len);

uint8_t
mrx_find_branch_avx2_(uint8_t c,
                      const uint8_t br[],
                      uint8_t len);

#endif
