/*
 * Copyright (c) 2013, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <mrx_scan.h>

#define BITOPS_PREFIX bituptr
#define BITOPS_TYPE uintptr_t
#define BITOPS_TYPE_MAX UINTPTR_MAX
#include <bitops.h>

typedef uintptr_t __attribute__(( aligned(1) )) unaligned_uintptr_t;

#define ALL01 (~(uintptr_t)0u / 0xFFu)         // 0x0101010101010101u
#define ALL80 (~(uintptr_t)0u / 0xFFu * 0x80u) // 0x8080808080808080u
#define ALL7F (~(uintptr_t)0u / 0xFFu * 0x7Fu)
#if ARCH_SIZEOF_PTR == 4
#define LOG2_PTR_SIZE 2u
#elif ARCH_SIZEOF_PTR == 8
#define LOG2_PTR_SIZE 3u
#endif

// This reference implementation is only used in unittests
uint8_t
mrx_find_branch_ref_impl_(const uint8_t c,
                          const uint8_t *br,
                          const uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        if (c == br[i]) {
            return i;
        }
    }
    return len;
}

uint8_t
mrx_find_branch_generic_(const uint8_t c,
                         const uint8_t *br,
                         uint8_t len)
{
#define MARK_ZERO_OCTETS(v) (~(((((v) & ALL7F) + ALL7F) | (v)) | ALL7F))

    // set all bytes to c
    const uintptr_t all_c = c * ALL01;

    // align ptr to branch array, this will cause reading before the pointer but as the
    // branch array is stored in an aligned node it's safe
    const uint8_t *abr = (const uint8_t *)((uintptr_t)br & ~(sizeof(uintptr_t) - 1));

    // offset starts at -alignment difference, and compensate len with same amount
    int8_t offset = (int8_t)-((uintptr_t)br - (uintptr_t)abr);
    len -= offset;

    // xor with all_c => only bytes equal to c will be zero
    uintptr_t vec = *(const uintptr_t *)abr ^ all_c;

    // bit trick: set all octets to zero except those that are already zero, those become 0x80
    vec = MARK_ZERO_OCTETS(vec);

    // remove all invalid octets prior to start of array due to alignment
    vec &= ALL80 << (-offset << 3u);

    // repeat the same procedure, but now with aligned pointer
    for (;;) {
        // if remaining length is less than uintptr_t, add stop bit
        vec |= (uintptr_t)(!(len >> LOG2_PTR_SIZE)) << (len << 3u);
        if (vec != 0) {
            return offset + (bituptr_bsf(vec) >> 3u);
        }
        len -= sizeof(uintptr_t);
        abr += sizeof(uintptr_t);
        offset += sizeof(uintptr_t);
        vec = *(const uintptr_t *)abr ^ all_c;
        vec = MARK_ZERO_OCTETS(vec);
    }
}

uint8_t
mrx_find_new_branch_pos_ref_impl_(const uint8_t c,
                                  const uint8_t *br,
                                  const uint8_t len)
{
    uint8_t i = 0;
    for (; i < len; i++) {
        if (c < br[i]) {
            return i;
        }
    }
    return i;
}

uint8_t
mrx_find_new_branch_pos_generic_(uint8_t c,
                                 const uint8_t *br,
                                 uint8_t len)
{
    const uint8_t *abr = (const uint8_t *)((uintptr_t)br & ~(sizeof(uintptr_t) - 1));
    int8_t offset = (int8_t)-((uintptr_t)br - (uintptr_t)abr);
    len -= offset;

// "Determine if a word has a byte greater than n" bit trick, only works if n is less than 128
#define HASMORE(x, all_127_minus_n) ((((x) + (all_127_minus_n)) | (x)) & ALL80)

    uintptr_t vec = *(const uintptr_t *)abr;
    if (c < 128) {
        const uintptr_t all_c = (127 - c) * ALL01;
        vec = HASMORE(vec, all_c);
        vec &= ALL80 << (-offset << 3u);
        for (;;) {
            vec |= (uintptr_t)(!(len >> LOG2_PTR_SIZE)) << (len << 3u);
            if (vec != 0) {
                return offset + (bituptr_bsf(vec) >> 3u);
            }
            len -= sizeof(uintptr_t);
            abr += sizeof(uintptr_t);
            offset += sizeof(uintptr_t);
            vec = *(const uintptr_t *)abr;
            vec = HASMORE(vec, all_c);
        }
    } else {
        // do same as for c < 128, but subtract 128 from c and branch bytes to keep the bit trick comparison valid
        const uintptr_t all_c = (255 - c) * ALL01;
        // remove all bytes less than 0x80, and subtract 0x80 from the others
        uintptr_t mask80plus = ((vec & ALL80) >> 7u) * 0xFFu;
        uintptr_t num7f = ((vec & mask80plus) | (ALL80 & ~mask80plus)) - ALL80;
        vec = HASMORE(num7f, all_c);
        vec &= ALL80 << (-offset << 3u);
        for (;;) {
            vec |= (uintptr_t)(!(len >> LOG2_PTR_SIZE)) << (len << 3u);
            if (vec != 0) {
                return offset + (bituptr_bsf(vec) >> 3u);
            }
            len -= sizeof(uintptr_t);
            abr += sizeof(uintptr_t);
            offset += sizeof(uintptr_t);
            vec = *(const uintptr_t *)abr;
            mask80plus = ((vec & ALL80) >> 7u) * 0xFFu;
            num7f = ((vec & mask80plus) | (ALL80 & ~mask80plus)) - ALL80;
            vec = HASMORE(num7f, all_c);
        }
    }
}

uint8_t
mrx_prefix_find_first_diff_ref_impl_(const uint8_t *a,
                                     const uint8_t *b,
                                     const uint8_t len)
{
    uint8_t i = len;
    while (i > 0) {
        if (*a != *b) {
            return len - i;
        }
        a++;
        b++;
        i--;
    }
    return len;
}

uint8_t
mrx_prefix_find_first_diff_generic_(const uint8_t *str,
                                    const uint8_t *prefix,
                                    const uint8_t prefix_len)
{
    unsigned px_len = (unsigned)prefix_len;
    uintptr_t prefix_;
    uintptr_t vec;
    uint8_t idx;
    uint8_t offset;

    prefix_ = *(const uintptr_t *)&prefix[-2]; // aligned
    prefix_ >>= 16u;
    vec = (prefix_ ^ *(const unaligned_uintptr_t *)str) | ((uintptr_t)1u << (sizeof(uintptr_t)*8u - 16u));
    vec |= (uintptr_t)!(px_len >> LOG2_PTR_SIZE) << (px_len << 3u);
    idx = (bituptr_bsf(vec) + sizeof(uintptr_t) - 1u) >> 3u;
    if (idx < sizeof(uintptr_t) - 2) {
        return idx;
    }
    if (prefix_len <= sizeof(uintptr_t) - 2) {
        return prefix_len;
    }
    offset = sizeof(uintptr_t) - 2;
    px_len -= sizeof(uintptr_t) - 2;
    prefix += sizeof(uintptr_t) - 2;
    str += sizeof(uintptr_t) - 2;
    for (;;) {
        vec = (*(const uintptr_t *)prefix ^ *(const unaligned_uintptr_t *)str);
        vec |= (uintptr_t)!(px_len >> LOG2_PTR_SIZE) << (px_len << 3u);
        if (vec != 0) {
            idx = (bituptr_bsf(vec) + sizeof(uintptr_t) - 1u) >> 3u;
            return idx + offset;
        }
        px_len -= sizeof(uintptr_t);
        prefix += sizeof(uintptr_t);
        str += sizeof(uintptr_t);
        offset += sizeof(uintptr_t);
    }
}
