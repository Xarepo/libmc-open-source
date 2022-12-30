/*
 * Copyright (c) 2013, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <mc_arch.h>

#include <mrx_scan.h>

// we don't check for all builtins, but for key ones where the availability of the rest can be assumed
#if __has_builtin(__builtin_ia32_loaddqu) // SSE2

#include <xmmintrin.h>
typedef char v16c __attribute__ ((vector_size (16)));
typedef long long int v2di __attribute__ ((vector_size (16)));

/*
  Warning: the SSE optimizations makes loadqu meaning that to not risking getting
  a segmentation fault the used allocators must allow "dummy" reads 15 bytes past
  the last byte of the actual data used.
*/

//fprintf(stderr, "B %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", (uint8_t)prefix_[0], (uint8_t)prefix_[1], (uint8_t)prefix_[2], (uint8_t)prefix_[3], (uint8_t)prefix_[4], (uint8_t)prefix_[5], (uint8_t)prefix_[6], (uint8_t)prefix_[7], (uint8_t)prefix_[8], (uint8_t)prefix_[9], (uint8_t)prefix_[10], (uint8_t)prefix_[11], (uint8_t)prefix_[12], (uint8_t)prefix_[13], (uint8_t)prefix_[14], (uint8_t)prefix_[15]);

#if ARCH_SIZEOF_PTR == 8

uint8_t
mrx_prefix_find_first_diff_sse2_(const uint8_t str[],
                                 const uint8_t prefix[], // always 2 bytes from aligned
                                 uint8_t prefix_len)
{
    // - prefix guaranteed to start at 2 bytes (directly after node header)
    // - nodes are guaranteed to be at least 16 byte aligned
    // - make aligned load, and skip the 2 header bytes, so we check 14 bytes first

    v16c prefix_;
    v16c vec;
    uint32_t bm;
    uint8_t idx;
    uint8_t offset;

    prefix_ = (v16c)__builtin_ia32_psrldqi128(*(const v2di *)&prefix[-2], 16); // shift 2 bytes
    vec = __builtin_ia32_loaddqu((const char *)str);
    vec = __builtin_ia32_pcmpeqb128(prefix_, vec);
    bm = __builtin_ia32_pmovmskb128(vec);
    bm = ~bm;
    bm |= (1u << 14);
    bm |= !(prefix_len >> 4u) << prefix_len;
    idx = __builtin_ctz(bm);
    if (idx < 14) {
        return idx;
    }
    offset = 14;
    prefix_len -= 14;
    prefix += 14;
    str += 14;
    for (;;) {
        vec = __builtin_ia32_loaddqu((const char *)str);
        vec = __builtin_ia32_pcmpeqb128(*(const v16c *)prefix, vec);
        bm = __builtin_ia32_pmovmskb128(vec);
        bm = ~bm;
        bm |= !(prefix_len >> 4u) << prefix_len;
        idx = __builtin_ctz(bm);
        if (idx < 16) {
            return idx + offset;
        }
        prefix_len -= 16;
        prefix += 16;
        str += 16;
        offset += 16;
    }
}

// ARCH_SIZEOF_PTR == 8 above
#elif ARCH_SIZEOF_PTR == 4

// in 32 bit mode, we can't trust on 16 byte alignment, so we use loaddqu all the way
uint8_t
mrx_prefix_find_first_diff_sse2_(const uint8_t str[],
                                 const uint8_t prefix[],
                                 uint8_t prefix_len)
{
    v16c prefix_;
    v16c vec;
    uint32_t bm;
    uint8_t idx;
    uint8_t offset = 0;
    for (;;) {
        vec = __builtin_ia32_loaddqu((const char *)str);
        prefix_ = __builtin_ia32_loaddqu((const char *)prefix);
        vec = __builtin_ia32_pcmpeqb128(prefix_, vec);
        bm = __builtin_ia32_pmovmskb128(vec);
        bm = ~bm;
        bm |= !(prefix_len >> 4u) << prefix_len;
        idx = __builtin_ctz(bm);
        if (idx < 16) {
            return idx + offset;
        }
        prefix_len -= 16;
        prefix += 16;
        str += 16;
        offset += 16;
    }
}

#endif // ARCH_SIZEOF_PTR == 4

uint8_t
mrx_find_branch_sse2_(const uint8_t c,
                      const uint8_t *br,
                      const uint8_t len)
{
    v16c br_;
    v16c vec;
    uint32_t bm;

    const v16c c_ = { c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c };
    br_ = __builtin_ia32_loaddqu((const char *)br);
    vec = __builtin_ia32_pcmpeqb128(br_, c_);
    bm = __builtin_ia32_pmovmskb128(vec);
    bm &= (1u << len) - 1;
    if (bm != 0) {
        return __builtin_ctz(bm);
    }
    if (len <= 16) {
        return len;
    }
    // packed node max branch count is 25, no need for loop
    br_ = __builtin_ia32_loaddqu((const char *)&br[16]);
    vec = __builtin_ia32_pcmpeqb128(br_, c_);
    bm = __builtin_ia32_pmovmskb128(vec);
    bm &= (1u << (len - 16)) - 1;
    bm |= 1u << (len - 16);
    return 16 + __builtin_ctz(bm);
}

uint8_t
mrx_find_new_branch_pos_sse2_(const uint8_t c,
                              const uint8_t *br,
                              const uint8_t len)
{
    v16c br_;
    v16c vec;
    uint32_t bm;

    const v16c c_ = { c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c };
    br_ = __builtin_ia32_loaddqu((const char *)br);
    vec = __builtin_ia32_pminub128(br_, c_);
    vec = __builtin_ia32_pcmpeqb128(vec, c_);
    bm = __builtin_ia32_pmovmskb128(vec);
    bm &= (1u << len) - 1;
    if (bm != 0) {
        return __builtin_ctz(bm);
    }
    if (len <= 16) {
        return len;
    }
    br_ = __builtin_ia32_loaddqu((const char *)&br[16]);
    vec = __builtin_ia32_pminub128(br_, c_);
    vec = __builtin_ia32_pcmpeqb128(vec, c_);
    bm = __builtin_ia32_pmovmskb128(vec);
    bm &= (1u << (len - 16)) - 1;
    bm |= 1u << (len - 16);
    return 16 + __builtin_ctz(bm);
}

#if __has_builtin(__builtin_ia32_pcmpestri128)

// in testing these have turned out to be slower than sse2 versions.
uint8_t
mrx_prefix_find_first_diff_sse42_(const uint8_t str[],
                                  const uint8_t prefix[],
                                  uint8_t prefix_len)
{
    v16c arr1 = (v16c)__builtin_ia32_psrldqi128(*(const v2di *)&prefix[-2], 16); // shift 2 bytes
    v16c arr2 = __builtin_ia32_loaddqu((const char *)str);
    uint8_t idx = __builtin_ia32_pcmpestri128(arr1, prefix_len, arr2, prefix_len, 0x18);
    if (idx < 14) {
        return idx;
    }
    if (prefix_len <= 14) {
        return prefix_len;
    }
    uint8_t offset = 14;
    prefix_len -= 14;
    prefix += 14;
    str += 14;
    for (;;) {
        arr1 = *(const v16c *)prefix;
        arr2 = __builtin_ia32_loaddqu((const char *)str);
        idx = __builtin_ia32_pcmpestri128(arr1, prefix_len, arr2, prefix_len, 0x18);
        if (idx < 16) {
            return idx + offset;
        }
        if (prefix_len <= 16) {
            return prefix_len + offset;
        }
        prefix_len -= 16;
        prefix += 16;
        str += 16;
        offset += 16;
    }
}

uint8_t
mrx_find_branch_sse42_(uint8_t c,
                       const uint8_t br[],
                       const uint8_t len)
{
    const v16c c_ = { (char)c };
    v16c br_ = __builtin_ia32_loaddqu((const char *)br);
    uint8_t idx = __builtin_ia32_pcmpestri128(c_, 1, br_, len, 0);
    if (idx != 16) {
        return idx;
    }
    if (len <= 16) {
        return len;
    }
    // packed node max branch count is 25, no need for loop
    br_ = __builtin_ia32_loaddqu((const char *)&br[16]);
    idx = __builtin_ia32_pcmpestri128(c_, 1, br_, len - 16, 0);
    if (idx != 16) {
        return 16 + idx;
    }
    return len;
}

#endif // __has_builtin(__builtin_ia32_pcmpestri128) SSE4.2

#endif // __has_builtin(__builtin_ia32_loaddqu) // SSE2

#if __has_builtin(__builtin_ia32_pminub256) // avx2

typedef char v32c __attribute__ ((vector_size (32)));
typedef long long int v4di __attribute__ ((vector_size (32)));

// This AVX2 version is slightly faster than SSE2 version for long prefixes
uint8_t
mrx_prefix_find_first_diff_avx2_(const uint8_t str[],
                                 const uint8_t prefix[], // always 2 bytes from aligned
                                 uint8_t prefix_len)
{
    v32c prefix_;
    v32c vec;
    uint64_t bm;
    uint8_t idx;
    uint8_t offset;

    // there is no psrlqqi256, ie shift that can do 32 byte (max is 16) so we can't do the
    // aligned read with shift here. So we do an unaligned read. And since we do an unaligned
    // read we don't need alignment on 32 bytes either.
    prefix_ = __builtin_ia32_loaddqu256((const char *)prefix);
    vec = __builtin_ia32_loaddqu256((const char *)str);
    vec = __builtin_ia32_pcmpeqb256(prefix_, vec);
    bm = __builtin_ia32_pmovmskb256(vec);
    bm = ~bm;
    bm |= (uint64_t)!(prefix_len >> 5u) << prefix_len;
    idx = __builtin_ctzl(bm);
    if (idx < 32) {
        return idx;
    }
    // step up only 14 or 30 to make next read aligned
    const uint8_t step = 30 - ((uintptr_t)&prefix[-2] & 31u);
    offset = step;
    prefix_len -= step;
    prefix += step;
    str += step;
    for (;;) {
        vec = __builtin_ia32_loaddqu256((const char *)str);
        vec = __builtin_ia32_pcmpeqb256(*(const v32c *)prefix, vec);
        bm = __builtin_ia32_pmovmskb256(vec);
        bm = ~bm;
        bm |= (uint64_t)!(prefix_len >> 5u) << prefix_len;
        idx = __builtin_ctzl(bm);
        if (idx < 32) {
            return idx + offset;
        }
        prefix_len -= 32;
        prefix += 32;
        str += 32;
        offset += 32;
    }
}

// From testing the avx2 version is (insignificantly) slower than sse2 version, even at max length
uint8_t
mrx_find_branch_avx2_(uint8_t c,
                      const uint8_t br[],
                      const uint8_t len) // never longer than SCAN_NODE_MAX_BRANCH_COUNT
{
    v32c br_;
    v32c vec;
    uint32_t bm;

    const v32c c_ = { c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c };
    br_ = __builtin_ia32_loaddqu256((const char *)br);
    vec = __builtin_ia32_pcmpeqb256(br_, c_);
    bm = __builtin_ia32_pmovmskb256(vec);
    bm &= (1u << len) - 1;
    bm |= 1u << len;
    return __builtin_ctz(bm);
}

#endif // __has_builtin(__builtin_ia32_pminub256) avx2
