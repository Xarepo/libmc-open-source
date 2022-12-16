/*
 * Copyright (c) 2012, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*

  Self-contained header with common (and not so common) bit operations.
  Uses builtins when possible.

  Contains the following:

  bit*_swap()  - byte-wise reverse (bit16_swap() also available)
  bit*_isset() - test if bit is set (single integer or array)
  bit*_set()   - set bit (single integer or array)
  bit*_unset() - unset bit (single integer or array)
  bit*_bsf()   - bit scan forward
  bit*_bsr()   - bit scan reverse
  bit*_rev()   - reverse bits
  bit*_count() - count number of set bits

  For bit arrays (note bit_isset() bit_set() and bit_unset() are also for arrays)

  barr*_set()   - set bits in a range
  barr*_unset() - unset bits in a range
  barr*_not     - not operation on a bit arrays over a range
  barr*_and     - and operation between to bit arrays over a range
  barr*_nand    - nand operation between to bit arrays over a range
  barr*_or      - or operation between to bit arrays over a range
  barr*_nor     - nor operation between to bit arrays over a range
  barr*_xor     - xor operation between to bit arrays over a range
  barr*_xnor    - xnor operation between to bit arrays over a range
  barr*_count   - count bits in a range
  barr*_bsf     - bit scan forward in a range
  barr*_bsr     - bit scan reverse in a range
  barr*_notbsf  - inverted bit scan forward in a range
  barr*_notbsr  - inverted bit scan reverse in a range

  bit sizes are 32 or 64, and if 'f32' is used, eg bitf32_set(), the same size
  as uint_fast32_t is used, which may be 64 rather than 32.

*/

#ifndef BITOPS_H
#define BITOPS_H

#include <limits.h>
#include <stdint.h>

/* Setup integer size macros from C-standard header defines */
#ifndef ARCH_SIZEOF_INT
#if INT_MAX == 2147483647
#define ARCH_SIZEOF_INT 4
#elif INT_MAX == 9223372036854775807
#define ARCH_SIZEOF_INT 8
#else
 #error "Unsupported size of INT_MAX"
#endif
#endif

#ifndef ARCH_SIZEOF_LONG
#if LONG_MAX == 2147483647
#define ARCH_SIZEOF_LONG 4
#elif LONG_MAX == 9223372036854775807
#define ARCH_SIZEOF_LONG 8
#else
 #error "Unsupported size of LONG_MAX"
#endif
#endif

#ifndef ARCH_SIZEOF_LONGLONG
#if LLONG_MAX == 2147483647
#define ARCH_SIZEOF_LONGLONG 4
#elif LLONG_MAX == 9223372036854775807
#define ARCH_SIZEOF_LONGLONG 8
#else
 #error "Unsupported size of LLONG_MAX"
#endif
#endif

#ifndef ARCH_SIZEOF_PTR
#if UINTPTR_MAX == 4294967295u
#define ARCH_SIZEOF_PTR 4
#elif UINTPTR_MAX == 18446744073709551615u
#define ARCH_SIZEOF_PTR 8
#else
 #error "Unsupported size of UINTPTR_MAX"
#endif
#endif

#if INT_FAST32_MAX == 9223372036854775807
typedef uint64_t bitf32_t;
#define bitf32_isset bit64_isset
#define bitf32_set bit64_set
#define bitf32_unset bit64_unset
#define bitf32_bsf bit64_bsf
#define bitf32_bsr bit64_bsr
#define bitf32_rev bit64_rev
#define bitf32_count bit64_count
#define bitf32_swap bit64_swap
#define barrf32_set barr64_set
#define barrf32_unset barr64_unset
#define barrf32_not barr64_not
#define barrf32_and barr64_and
#define barrf32_nand barr64_nand
#define barrf32_or barr64_or
#define barrf32_nor barr64_nor
#define barrf32_xor barr64_xor
#define barrf32_xnor barr64_xnor
#define barrf32_count barr64_count
#define barrf32_bsf barr64_bsf
#define barrf32_bsr barr64_bsr
#define barrf32_notbsf barr64_notbsf
#define barrf32_notbsr barr64_notbsr
#else
typedef uint32_t bitf32_t;
#define bitf32_isset bit32_isset
#define bitf32_set bit32_set
#define bitf32_unset bit32_unset
#define bitf32_bsf bit32_bsf
#define bitf32_bsr bit32_bsr
#define bitf32_rev bit32_rev
#define bitf32_count bit32_count
#define bitf32_swap bit32_swap
#define barrf32_set barr32_set
#define barrf32_unset barr32_unset
#define barrf32_not barr32_not
#define barrf32_and barr32_and
#define barrf32_nand barr32_nand
#define barrf32_or barr32_or
#define barrf32_nor barr32_nor
#define barrf32_xor barr32_xor
#define barrf32_xnor barr32_xnor
#define barrf32_count barr32_count
#define barrf32_bsf barr32_bsf
#define barrf32_bsr barr32_bsr
#define barrf32_notbsf barr32_notbsf
#define barrf32_notbsr barr32_notbsr
#endif // INT_FAST32_MAX == 9223372036854775807

/* bit_all32 / bit_all64 unions are used to avoid aliasing issues in casts */
union bitunion32 {
    uint32_t u32;
#if ARCH_SIZEOF_INT == 4
    unsigned a;
#endif
#if ARCH_SIZEOF_LONG == 4
    unsigned long b;
#endif
#if ARCH_SIZEOF_PTR == 4
    void *c;
#endif
};

union bitunion64 {
    uint64_t u64;
#if ARCH_SIZEOF_INT == 8
    unsigned a;
#endif
#if ARCH_SIZEOF_LONG == 8
    unsigned long b;
#endif
#if ARCH_SIZEOF_LONG_LONG == 8
    unsigned long long c;
#endif
#if ARCH_SIZEOF_PTR == 8
    void *d;
#endif
};

static inline int
bit32_isset(const uint32_t bits[],
            const unsigned position)
{
    const union bitunion32 *bs = (const union bitunion32 *)bits;
    const unsigned i = position >> 5u;
    return !!(bs[i].u32 & (uint32_t)1u << (position - (i << 5u)));
}
static inline int
bit64_isset(const uint64_t bits[],
            const unsigned position)
{
    const union bitunion64 *bs = (const union bitunion64 *)bits;
    const unsigned i = position >> 6u;
    return !!(bs[i].u64 & (uint64_t)1u << (position - (i << 6u)));
}

static inline void
bit32_set(uint32_t bits[],
          const unsigned position)
{
    union bitunion32 *bs = (union bitunion32 *)bits;
    const unsigned i = position >> 5u;
    bs[i].u32 = bs[i].u32 | (uint32_t)1u << (position - (i << 5u));
}
static inline void
bit64_set(uint64_t bits[],
          const unsigned position)
{
    union bitunion64 *bs = (union bitunion64 *)bits;
    const unsigned i = position >> 6u;
    bs[i].u64 = bs[i].u64 | (uint64_t)1u << (position - (i << 6u));
}

static inline void
bit32_unset(uint32_t bits[],
            const unsigned position)
{
    union bitunion32 *bs = (union bitunion32 *)bits;
    const unsigned i = position >> 5u;
    bs[i].u32 = bs[i].u32 & ~((uint32_t)1u << (position - (i << 5u)));
}
static inline void
bit64_unset(uint64_t bits[],
            const unsigned position)
{
    union bitunion64 *bs = (union bitunion64 *)bits;
    const unsigned i = position >> 6u;
    bs[i].u64 = bs[i].u64 & ~((uint64_t)1u << (position - (i << 6u)));
}

static inline unsigned
bit32_bsf_generic(const uint32_t value)
{
    static const unsigned table[256] = {
        0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    };
    if ((value & 0x0000FFFFu) != 0) {
       if ((value & 0x000000FFu) != 0) {
           return table[value & 0x000000FFu];
       } else {
           return 8u + table[(value & 0x0000FF00u) >> 8u];
       }
    } else {
       if ((value & 0x00FF0000u) != 0) {
           return 16u + table[(value & 0x00FF0000u) >> 16u];
       } else {
           return 24u + table[(value & 0xFF000000u) >> 24u];
       }
    }
}

/* Note on bit scans below: if integer is zero the return value is undefined! */
static inline unsigned
bit32_bsf(const uint32_t value)
{
#if ARCH_SIZEOF_INT >= 4 && __has_builtin(__builtin_ctz)
    return (unsigned)__builtin_ctz((unsigned)value);
#else
    return bit32_bsf_generic(value);
#endif
}

static inline unsigned
bit32_bsr_generic(const uint32_t value)
{
    static const unsigned table[256] = {
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
    };
    if ((value & 0xFFFF0000u) != 0) {
        if ((value & 0xFF000000u) != 0) {
            return 24u + table[(value & 0xFF000000u) >> 24u];
        } else {
            return 16u + table[(value & 0x00FF0000u) >> 16u];
        }
    } else {
        if ((value & 0x0000FF00u) != 0) {
            return 8u + table[(value & 0x0000FF00u) >> 8u];
        } else {
            return table[value & 0x000000FFu];
        }
    }
}

static inline unsigned
bit32_bsr(const uint32_t value)
{
#if ARCH_SIZEOF_INT == 4 && __has_builtin(__builtin_clz)
    return 31u - (unsigned)__builtin_clz((unsigned)value);
#elif ARCH_SIZEOF_INT == 8 && __has_builtin(__builtin_clz)
    return 63u - (unsigned)__builtin_clz((unsigned)value);
#else
    return bit32_bsr_generic(value);
#endif
}

static inline unsigned
bit64_bsf_generic(const uint64_t value)
{
    const uint32_t lobits = value & 0xFFFFFFFFu;
    if (lobits != 0) {
        return bit32_bsf_generic(lobits);
    }
    return 32 + bit32_bsf_generic((const uint32_t)(value >> 32u));
}

static inline unsigned
bit64_bsf(const uint64_t value)
{
#if ARCH_SIZEOF_INT >= 8 && __has_builtin(__builtin_ctz)
    return (unsigned)__builtin_ctz((unsigned)value);
#elif ARCH_SIZEOF_LONG >= 8 && __has_builtin(__builtin_ctzl)
    return (unsigned)__builtin_ctzl((unsigned long)value);
#elif ARCH_SIZEOF_LONG_LONG >= 8 && __has_builtin(__builtin_ctzll)
    return (unsigned)__builtin_ctzll((unsigned long long)value);
#else
    const uint32_t lobits = value & 0xFFFFFFFFu;
    if (lobits != 0) {
        return bit32_bsf(lobits);
    }
    return 32 + bit32_bsf((const uint32_t)(value >> 32));
#endif
}

static inline unsigned
bit64_bsr_generic(const uint64_t value)
{
    const uint32_t hibits = (const uint32_t)(value >> 32u);
    if (hibits != 0) {
        return 32u + bit32_bsr_generic(hibits);
    }
    return bit32_bsr_generic((const uint32_t)(value & 0xFFFFFFFFu));
}

static inline unsigned
bit64_bsr(const uint64_t value)
{
#if ARCH_SIZEOF_INT == 8 && __has_builtin(__builtin_ctz)
    return 63u - (unsigned)__builtin_clz((unsigned)value);
#elif ARCH_SIZEOF_LONG == 8 && __has_builtin(__builtin_ctzl)
    return 63u - (unsigned)__builtin_clzl((unsigned long)value);
#elif ARCH_SIZEOF_LONG_LONG == 8 && __has_builtin(__builtin_ctzll)
    return 63u - (unsigned)__builtin_clzll((unsigned long long)value);
#else
    const uint32_t hibits = (const uint32_t)(value >> 32);
    if (hibits != 0) {
        return 32 + bit32_bsr(hibits);
    }
    return bit32_bsr((const uint32_t)(value & 0xFFFFFFFFu));
#endif
}

static inline uint32_t
bit32_rev(uint32_t v)
{
    v = ((v >> 1u) & 0x55555555u) | ((v & 0x55555555u) << 1u);
    v = ((v >> 2u) & 0x33333333u) | ((v & 0x33333333u) << 2u);
    v = ((v >> 4u) & 0x0F0F0F0Fu) | ((v & 0x0F0F0F0Fu) << 4u);
    v = ((v >> 8u) & 0x00FF00FFu) | ((v & 0x00FF00FFu) << 8u);
    v = ( v >> 16u              ) | ( v                << 16u);
    return v;
}

static inline uint64_t
bit64_rev(uint64_t v)
{
    v = ((v >> 1u)  & 0x5555555555555555u)|((v & 0x5555555555555555u) << 1u);
    v = ((v >> 2u)  & 0x3333333333333333u)|((v & 0x3333333333333333u) << 2u);
    v = ((v >> 4u)  & 0x0F0F0F0F0F0F0F0Fu)|((v & 0x0F0F0F0F0F0F0F0Fu) << 4u);
    v = ((v >> 8u)  & 0x00FF00FF00FF00FFu)|((v & 0x00FF00FF00FF00FFu) << 8u);
    v = ((v >> 16u) & 0x0000FFFF0000FFFFu)|((v & 0x0000FFFF0000FFFFu) << 16u);
    v = ( v >> 32u                       )|( v                        << 32u);
    return v;
}

static inline unsigned
bit32_count_generic(uint32_t v)
{
    v = (v & 0x55555555u) + ((v & 0xAAAAAAAAu) >> 1u);
    v = (v & 0x33333333u) + ((v & 0xCCCCCCCCu) >> 2u);
    v = (v & 0x0F0F0F0Fu) + ((v & 0xF0F0F0F0u) >> 4u);
    v = (v & 0x00FF00FFu) + ((v & 0xFF00FF00u) >> 8u);
    v = (v & 0x0000FFFFu) + ((v & 0xFFFF0000u) >> 16u);
    return (unsigned)v;
}

static inline unsigned
bit32_count(uint32_t v)
{
#if ARCH_SIZEOF_INT >= 4 && __has_builtin(__builtin_popcount)
    return (unsigned)__builtin_popcount((unsigned)v);
#elif ARCH_SIZEOF_LONG >= 4 && __has_builtin(__builtin_popcountl)
    return (unsigned)__builtin_popcountl((unsigned long)v);
#else
    return bit32_count_generic(v);
#endif
}

static inline unsigned
bit64_count_generic(uint64_t v)
{
    v = (v & 0x5555555555555555u) + ((v & 0xAAAAAAAAAAAAAAAAu) >> 1u);
    v = (v & 0x3333333333333333u) + ((v & 0xCCCCCCCCCCCCCCCCu) >> 2u);
    v = (v & 0x0F0F0F0F0F0F0F0Fu) + ((v & 0xF0F0F0F0F0F0F0F0u) >> 4u);
    v = (v & 0x00FF00FF00FF00FFu) + ((v & 0xFF00FF00FF00FF00u) >> 8u);
    v = (v & 0x0000FFFF0000FFFFu) + ((v & 0xFFFF0000FFFF0000u) >> 16u);
    v = (v & 0x00000000FFFFFFFFu) + ((v & 0xFFFFFFFF00000000u) >> 32u);
    return (unsigned)v;
}

static inline unsigned
bit64_count(uint64_t v)
{
#if ARCH_SIZEOF_INT == 8 && __has_builtin(__builtin_popcount)
    return (unsigned)__builtin_popcount((unsigned)v);
#elif ARCH_SIZEOF_LONG == 8 && __has_builtin(__builtin_popcountl)
    return (unsigned)__builtin_popcountl((unsigned long)v);
#elif ARCH_SIZEOF_LONGLONG == 8 && __has_builtin(__builtin_popcountll)
    return (unsigned)__builtin_popcountll((unsigned long long)v);
#else
    return bit64_count_generic(v);
#endif
}

#define bit_swap16_generic(value) (((((unsigned)(value)) & 0xFF00u) >> 8u) | ((((unsigned)(value)) & 0x00FFu) << 8u))
#if __has_builtin(__builtin_bswap16)
#define bit_swap16 __builtin_bswap16
#else
#define bit_swap16 bit_swap16_generic
#endif

static inline uint32_t
bit_swap32_generic(uint32_t v)
{
    return bit_swap16((uint16_t)((v & 0xFFFF0000u ) >> 16u)) |
        ((uint32_t)bit_swap16((uint16_t)(v & 0x0000FFFFu)) << 16u);
}
#if __has_builtin(__builtin_bswap32)
#define bit_swap32 __builtin_bswap32
#else
#define bit_swap32 bit_swap32_generic
#endif

static inline uint64_t
bit_swap64_generic(uint64_t v)
{
    return bit_swap32((uint32_t)((v & 0xFFFFFFFF00000000u ) >> 32u)) |
        ((uint64_t)bit_swap32((uint32_t)(v & 0xFFFFFFFFu)) << 32u);
}
#if __has_builtin(__builtin_bswap64)
#define bit_swap64 __builtin_bswap64
#else
#define bit_swap64 bit_swap64_generic
#endif

static inline void
barr32_set(uint32_t bits[],
           const unsigned from,
           const unsigned to)
{
    union bitunion32 *bs = (union bitunion32 *)bits;
    if ((to >> 5u) > (from >> 5u)) {
        bs[from >> 5u].u32 |= 0xFFFFFFFFu << (from & 0x1Fu);
        for (unsigned i = (from >> 5u) + 1u; i < (to >> 5u); i++) {
            bs[i].u32 = 0xFFFFFFFFu;
        }
        bs[to >> 5u].u32 |= 0xFFFFFFFFu >> (0x1Fu - (to & 0x1Fu));
    } else {
        bs[from >> 5u].u32 |=
            ((0xFFFFFFFFu << (from & 0x1Fu)) &
             (0xFFFFFFFFu >> (0x1Fu - (to & 0x1Fu))));
    }
}
static inline void
barr64_set(uint64_t bits[],
           const unsigned from,
           const unsigned to)
{
    union bitunion64 *bs = (union bitunion64 *)bits;
    if ((to >> 6u) > (from >> 6u)) {
        bs[from >> 6u].u64 |= 0xFFFFFFFFFFFFFFFFu << (from & 0x3Fu);
        for (unsigned i = (from >> 6u) + 1; i < (to >> 6u); i++) {
            bs[i].u64 = 0xFFFFFFFFFFFFFFFFu;
        }
        bs[to >> 6u].u64 |= 0xFFFFFFFFFFFFFFFFu >> (0x3Fu - (to & 0x3Fu));
    } else {
        bs[from >> 6u].u64 |=
            ((0xFFFFFFFFFFFFFFFFu << (from & 0x3Fu)) &
             (0xFFFFFFFFFFFFFFFFu >> (0x3Fu - (to & 0x3Fu))));
    }
}

static inline void
barr32_unset(uint32_t bits[],
             const unsigned from,
             const unsigned to)
{
    union bitunion32 *bs = (union bitunion32 *)bits;
    if ((to >> 5u) > (from >> 5u)) {
        bs[from >> 5u].u32 &= ~(0xFFFFFFFFu << (from & 0x1Fu));
        for (unsigned i = (from >> 5u) + 1; i < (to >> 5u); i++) {
            bs[i].u32 = 0;
        }
        bs[to >> 5u].u32 &= ~(0xFFFFFFFFu >> (0x1Fu - (to & 0x1Fu)));
    } else {
        bs[from >> 5u].u32 &=
            ~((0xFFFFFFFFu << (from & 0x1Fu)) &
              (0xFFFFFFFFu >> (0x1Fu - (to & 0x1Fu))));
    }
}
static inline void
barr64_unset(uint64_t bits[],
             const unsigned from,
             const unsigned to)
{
    union bitunion64 *bs = (union bitunion64 *)bits;
    if ((to >> 6u) > (from >> 6u)) {
        bs[from >> 6u].u64 &= ~(0xFFFFFFFFFFFFFFFFu << (from & 0x3Fu));
        for (unsigned i = (from >> 6u) + 1; i < (to >> 6u); i++) {
            bs[i].u64 = 0;
        }
        bs[to >> 6u].u64 &= ~(0xFFFFFFFFFFFFFFFFu >> (0x3Fu - (to & 0x3Fu)));
    } else {
        bs[from >> 6u].u64 &=
            ~((0xFFFFFFFFFFFFFFFFu << (from & 0x3Fu)) &
              (0xFFFFFFFFFFFFFFFFu >> (0x3Fu - (to & 0x3Fu))));
    }
}

static inline void
barr32_not(uint32_t bits[],
           const unsigned from,
           const unsigned to)
{
    union bitunion32 *bs = (union bitunion32 *)bits;
    if ((to >> 5u) > (from >> 5u)) {
        uint32_t mask = 0xFFFFFFFFu << (from & 0x1Fu);
        uint32_t cpy = bs[from >> 5u].u32;
        bs[from >> 5u].u32 &= ~mask;
        bs[from >> 5u].u32 |= ~cpy & mask;
        for (unsigned i = (from >> 5u) + 1u; i < (to >> 5u); i++) {
            bs[i].u32 = ~bs[i].u32;
        }
        mask = 0xFFFFFFFFu >> (0x1Fu - (to & 0x1Fu));
        cpy = bs[to >> 5u].u32;
        bs[to >> 5u].u32 &= ~mask;
        bs[to >> 5u].u32 |= ~cpy & mask;
    } else {
        const uint32_t mask = ((0xFFFFFFFFu << (from & 0x1Fu)) & (0xFFFFFFFFu >> (0x1Fu - (to & 0x1Fu))));
        const uint32_t cpy = bs[from >> 5u].u32;
        bs[from >> 5u].u32 &= ~mask;
        bs[from >> 5u].u32 |= ~cpy & mask;
    }
}
static inline void
barr64_not(uint64_t bits[],
           const unsigned from,
           const unsigned to)
{
    union bitunion64 *bs = (union bitunion64 *)bits;
    if ((to >> 6u) > (from >> 6u)) {
        uint64_t mask = 0xFFFFFFFFFFFFFFFFu << (from & 0x3Fu);
        uint64_t cpy = bs[from >> 6u].u64;
        bs[from >> 6u].u64 &= ~mask;
        bs[from >> 6u].u64 |= ~cpy & mask;
        for (unsigned i = (from >> 6u) + 1u; i < (to >> 6u); i++) {
            bs[i].u64 = ~bs[i].u64;
        }
        mask = 0xFFFFFFFFFFFFFFFFu >> (0x3Fu - (to & 0x3Fu));
        cpy = bs[to >> 6u].u64;
        bs[to >> 6u].u64 &= ~mask;
        bs[to >> 6u].u64 |= ~cpy & mask;
    } else {
        const uint64_t mask = ((0xFFFFFFFFFFFFFFFFu << (from & 0x3Fu)) &
                               (0xFFFFFFFFFFFFFFFFu >> (0x3Fu - (to & 0x3Fu))));
        const uint64_t cpy = bs[from >> 6u].u64;
        bs[from >> 6u].u64 &= ~mask;
        bs[from >> 6u].u64 |= ~cpy & mask;
    }
}

#define BITOPS_ARRAYOP32_(BITOP, MASKOP)                                 \
    if ((to >> 5u) > (from >> 5u)) {                                     \
        uint32_t mask = 0xFFFFFFFFu << (from & 0x1Fu);                   \
        dst[from >> 5u].u32 BITOP ((src[from >> 5u].u32 & mask) MASKOP); \
        for (unsigned i = (from >> 5u) + 1; i < (to >> 5u); i++) {       \
            dst[i].u32 BITOP src[i].u32;                                 \
        }                                                                \
        mask = 0xFFFFFFFFu >> (0x1Fu - (to & 0x1Fu));                    \
        dst[to >> 5u].u32 BITOP ((src[to >> 5u].u32 & mask) MASKOP);     \
    } else {                                                             \
        const uint32_t mask = ((0xFFFFFFFFu << (from & 0x1Fu)) &         \
                               (0xFFFFFFFFu >> (0x1Fu - (to & 0x1Fu)))); \
        dst[from >> 5u].u32 BITOP ((src[from >> 5u].u32 & mask) MASKOP); \
    }
#define BITOPS_ARRAYNOTOP32_(BITOP)                                      \
    if ((to >> 5u) > (from >> 5u)) {                                     \
        uint32_t mask = 0xFFFFFFFFu << (from & 0x1Fu);                   \
        uint32_t cpy = dst[from >> 5u].u32;                              \
        dst[from >> 5u].u32 &= ~mask;                                    \
        dst[from >> 5u].u32 |= ~(cpy BITOP src[from >> 5u].u32) & mask;  \
        for (unsigned i = (from >> 5u) + 1; i < (to >> 5u); i++) {       \
            dst[i].u32 = ~(dst[i].u32 BITOP src[i].u32);                 \
        }                                                                \
        mask = 0xFFFFFFFFu >> (0x1Fu - (to & 0x1Fu));                    \
        cpy = dst[to >> 5u].u32;                                         \
        dst[to >> 5u].u32 &= ~mask;                                      \
        dst[to >> 5u].u32 |= ~(cpy BITOP src[to >> 5u].u32) & mask;      \
    } else {                                                             \
        const uint32_t mask = ((0xFFFFFFFFu << (from & 0x1Fu)) &         \
                               (0xFFFFFFFFu >> (0x1Fu - (to & 0x1Fu)))); \
        const uint32_t cpy = dst[from >> 5u].u32;                        \
        dst[from >> 5u].u32 &= ~mask;                                    \
        dst[from >> 5u].u32 |= ~(cpy BITOP src[from >> 5u].u32) & mask;  \
    }
#define BITOPS_ARRAYOP64_(BITOP, MASKOP)                                 \
    if ((to >> 6u) > (from >> 6u)) {                                     \
        uint64_t mask = 0xFFFFFFFFFFFFFFFFu << (from & 0x3Fu);           \
        dst[from >> 6u].u64 BITOP ((src[from >> 6u].u64 & mask) MASKOP); \
        for (unsigned i = (from >> 6u) + 1; i < (to >> 6u); i++) {       \
            dst[i].u64 BITOP src[i].u64;                                 \
        }                                                                \
        mask = 0xFFFFFFFFFFFFFFFFu >> (0x3Fu - (to & 0x3Fu));            \
        dst[to >> 6u].u64 BITOP ((src[to >> 6u].u64 & mask) MASKOP);     \
    } else {                                                             \
        const uint64_t mask =                                            \
            ((0xFFFFFFFFFFFFFFFFu << (from & 0x3Fu)) &                   \
             (0xFFFFFFFFFFFFFFFFu >> (0x3Fu - (to & 0x3Fu))));           \
        dst[from >> 6u].u64 BITOP ((src[from >> 6u].u64 & mask) MASKOP); \
    }
#define BITOPS_ARRAYNOTOP64_(BITOP)                                      \
    if ((to >> 6u) > (from >> 6u)) {                                     \
        uint64_t mask = 0xFFFFFFFFFFFFFFFFu << (from & 0x3Fu);           \
        uint64_t cpy = dst[from >> 6u].u64;                              \
        dst[from >> 6u].u64 &= ~mask;                                    \
        dst[from >> 6u].u64 |= ~(cpy BITOP src[from >> 6u].u64) & mask;  \
        for (unsigned i = (from >> 6u) + 1; i < (to >> 6u); i++) {       \
            dst[i].u64 = ~(dst[i].u64 BITOP src[i].u64);                 \
        }                                                                \
        mask = 0xFFFFFFFFFFFFFFFFu >> (0x3Fu - (to & 0x3Fu));            \
        cpy = dst[to >> 6u].u64;                                         \
        dst[to >> 6u].u64 &= ~mask;                                      \
        dst[to >> 6u].u64 |= ~(cpy BITOP src[to >> 6u].u64) & mask;      \
    } else {                                                             \
        const uint64_t mask =                                            \
            ((0xFFFFFFFFFFFFFFFFu << (from & 0x3Fu)) &                   \
             (0xFFFFFFFFFFFFFFFFu >> (0x3Fu - (to & 0x3Fu))));           \
        const uint64_t cpy = dst[from >> 6u].u64;                        \
        dst[from >> 6u].u64 &= ~mask;                                    \
        dst[from >> 6u].u64 |= ~(cpy BITOP src[from >> 6u].u64) & mask;  \
    }

static inline void
barr32_and(uint32_t destination[],
           const uint32_t source[],
           const unsigned from,
           const unsigned to)
{
    union bitunion32 *dst = (union bitunion32 *)destination;
    const union bitunion32 *src = (const union bitunion32 *)source;
    BITOPS_ARRAYOP32_(&=, | ~mask);
}
static inline void
barr64_and(uint64_t destination[],
           const uint64_t source[],
           const unsigned from,
           const unsigned to)
{
    union bitunion64 *dst = (union bitunion64 *)destination;
    const union bitunion64 *src = (const union bitunion64 *)source;
    BITOPS_ARRAYOP64_(&=, | ~mask);
}

static inline void
barr32_nand(uint32_t destination[],
            const uint32_t source[],
            const unsigned from,
            const unsigned to)
{
    union bitunion32 *dst = (union bitunion32 *)destination;
    const union bitunion32 *src = (const union bitunion32 *)source;
    BITOPS_ARRAYNOTOP32_(&);
}
static inline void
barr64_nand(uint64_t destination[],
            const uint64_t source[],
            const unsigned from,
            const unsigned to)
{
    union bitunion64 *dst = (union bitunion64 *)destination;
    const union bitunion64 *src = (const union bitunion64 *)source;
    BITOPS_ARRAYNOTOP64_(&);
}

static inline void
barr32_or(uint32_t destination[],
          const uint32_t source[],
          const unsigned from,
          const unsigned to)
{
    union bitunion32 *dst = (union bitunion32 *)destination;
    const union bitunion32 *src = (const union bitunion32 *)source;
    BITOPS_ARRAYOP32_(|=, );
}
static inline void
barr64_or(uint64_t destination[],
          const uint64_t source[],
          const unsigned from,
          const unsigned to)
{
    union bitunion64 *dst = (union bitunion64 *)destination;
    const union bitunion64 *src = (const union bitunion64 *)source;
    BITOPS_ARRAYOP64_(|=, );
}

static inline void
barr32_nor(uint32_t destination[],
           const uint32_t source[],
           const unsigned from,
           const unsigned to)
{
    union bitunion32 *dst = (union bitunion32 *)destination;
    const union bitunion32 *src = (const union bitunion32 *)source;
    BITOPS_ARRAYNOTOP32_(|);
}
static inline void
barr64_nor(uint64_t destination[],
           const uint64_t source[],
           const unsigned from,
           const unsigned to)
{
    union bitunion64 *dst = (union bitunion64 *)destination;
    const union bitunion64 *src = (const union bitunion64 *)source;
    BITOPS_ARRAYNOTOP64_(|);
}

static inline void
barr32_xor(uint32_t destination[],
           const uint32_t source[],
           const unsigned from,
           const unsigned to)
{
    union bitunion32 *dst = (union bitunion32 *)destination;
    const union bitunion32 *src = (const union bitunion32 *)source;
    BITOPS_ARRAYOP32_(^=, );
}
static inline void
barr64_xor(uint64_t destination[],
           const uint64_t source[],
           const unsigned from,
           const unsigned to)
{
    union bitunion64 *dst = (union bitunion64 *)destination;
    const union bitunion64 *src = (const union bitunion64 *)source;
    BITOPS_ARRAYOP64_(^=, );
}

static inline void
barr32_xnor(uint32_t destination[],
            const uint32_t source[],
            const unsigned from,
            const unsigned to)
{
    union bitunion32 *dst = (union bitunion32 *)destination;
    const union bitunion32 *src = (const union bitunion32 *)source;
    BITOPS_ARRAYNOTOP32_(^);
}
static inline void
barr64_xnor(uint64_t destination[],
            const uint64_t source[],
            const unsigned from,
            const unsigned to)
{
    union bitunion64 *dst = (union bitunion64 *)destination;
    const union bitunion64 *src = (const union bitunion64 *)source;
    BITOPS_ARRAYNOTOP64_(^);
}

#undef BITOPS_ARRAYOP32_
#undef BITOPS_ARRAYOP64_
#undef BITOPS_ARRAYNOTOP32_
#undef BITOPS_ARRAYNOTOP64_

static inline unsigned
barr32_count(const uint32_t bits[],
             const unsigned from,
             const unsigned to)
{
    const union bitunion32 *bs = (const union bitunion32 *)bits;

    if (to < from) {
        return 0;
    }
    uint32_t bb = bs[from >> 5u].u32;
    bb &= ~(((uint32_t)1u << (from & 0x1Fu)) - 1);
    unsigned count = 0;
    if ((to >> 5u) > (from >> 5u)) {
        count += bit32_count(bb);
        for (unsigned i = (from >> 5u) + 1; i < (to >> 5u); i++) {
            count += bit32_count(bs[i].u32);
        }
        bb = bs[to >> 5u].u32;
    }
    count += bit32_count(bb << (0x1Fu - (to & 0x1Fu)));
    return count;
}
static inline unsigned
barr64_count(const uint64_t bits[],
             const unsigned from,
             const unsigned to)
{
    const union bitunion64 *bs = (const union bitunion64 *)bits;

    if (to < from) {
        return 0;
    }
    uint64_t bb = bs[from >> 6u].u64;
    bb &= ~(((uint64_t)1u << (from & 0x3Fu)) - 1);
    unsigned count = 0;
    if ((to >> 6u) > (from >> 6u)) {
        count += bit64_count(bb);
        for (unsigned i = (from >> 6u) + 1; i < (to >> 6u); i++) {
            count += bit64_count(bs[i].u64);
        }
        bb = bs[to >> 6u].u64;
    }
    count += bit64_count(bb << (0x3Fu - (to & 0x3Fu)));
    return count;
}

static inline int
barr32_bsf(const uint32_t bits[],
           const unsigned from,
           const unsigned to)
{
    const union bitunion32 *bs = (const union bitunion32 *)bits;
    unsigned i;
    uint32_t bb;

    if ((const int)to < (const int)from) {
        return -1;
    }
    if ((bb = bs[from >> 5u].u32 >> (from & 0x1Fu)) != 0) {
	if ((i = bit32_bsf(bb) + from) > to) {
	    return -1;
	}
	return (int)i;
    }
    for (i = (from >> 5u) + 1; i <= (to >> 5u); i++) {
	if (bs[i].u32 != 0) {
	    if ((i = bit32_bsf(bs[i].u32) + (i << 5u)) > to) {
		return -1;
	    }
	    return (int)i;
	}
    }
    return -1;
}
static inline int
barr64_bsf(const uint64_t bits[],
           const unsigned from,
           const unsigned to)
{
    const union bitunion64 *bs = (const union bitunion64 *)bits;
    unsigned i;
    uint64_t bb;

    if ((const int)to < (const int)from) {
        return -1;
    }
    if ((bb = bs[from >> 6u].u64 >> (from & 0x3Fu)) != 0) {
	if ((i = bit64_bsf(bb) + from) > to) {
	    return -1;
	}
	return (int)i;
    }
    for (i = (from >> 6u) + 1; i <= (to >> 6u); i++) {
	if (bs[i].u64 != 0) {
	    if ((i = bit64_bsf(bs[i].u64) + (i << 6u)) > to) {
		return -1;
	    }
	    return (int)i;
	}
    }
    return -1;
}

static inline int
barr32_bsr(const uint32_t bits[],
           const unsigned from,
           const unsigned to)
{
    const union bitunion32 *bs = (const union bitunion32 *)bits;
    unsigned i;
    uint32_t bb;

    if ((const int)to < (const int)from) {
        return -1;
    }
    if ((bb = bs[to >> 5u].u32 << (31 - (to & 0x1Fu))) != 0) {
	if ((i = bit32_bsr(bb) - 31 + to) < from) {
	    return -1;
	}
	return (int)i;
    }
    for (i = (to >> 5u) - 1; (int32_t)i >= (int32_t)(from >> 5u); i--) {
	if (bs[i].u32 != 0) {
	    if ((i = bit32_bsr(bs[i].u32) + (i << 5u)) < from) {
		return -1;
	    }
	    return (int)i;
	}
    }
    return -1;
}
static inline int
barr64_bsr(const uint64_t bits[],
           const unsigned from,
           const unsigned to)
{
    const union bitunion64 *bs = (const union bitunion64 *)bits;
    unsigned i;
    uint64_t bb;

    if ((const int)to < (const int)from) {
        return -1;
    }
    if ((bb = bs[to >> 6u].u64 << (0x3Fu - (to & 0x3Fu))) != 0) {
	if ((i = bit64_bsr(bb) - 0x3Fu + to) < from) {
	    return -1;
	}
	return (int)i;
    }
    for (i = (to >> 6u) - 1; (int)i >= (const int)(from >> 6u); i--) {
	if (bs[i].u64 != 0) {
	    if ((i = bit64_bsr(bs[i].u64) + (i << 6u)) < from) {
		return -1;
	    }
	    return (int)i;
	}
    }
    return -1;
}

static inline int
barr32_notbsf(const uint32_t bits[],
              const unsigned from,
              const unsigned to)
{
    const union bitunion32 *bs = (const union bitunion32 *)bits;
    unsigned i;
    uint32_t bb;

    if ((const int)to < (const int)from) {
        return -1;
    }
    i = (from & 0x1Fu);
    if ((bb = bs[from >> 5u].u32 >> i) != 0xFFFFFFFFu >> i) {
	if ((i = bit32_bsf(~bb) + from) > to) {
	    return -1;
	}
	return (int)i;
    }
    for (i = (from >> 5u) + 1; i <= (to >> 5u); i++) {
	if (bs[i].u32 != 0xFFFFFFFFu) {
	    if ((i = bit32_bsf(~bs[i].u32) + (i << 5u)) > to) {
		return -1;
	    }
	    return (int)i;
	}
    }
    return -1;
}
static inline int
barr64_notbsf(const uint64_t bits[],
              const unsigned from,
              const unsigned to)
{
    const union bitunion64 *bs = (const union bitunion64 *)bits;
    unsigned i;
    uint64_t bb;

    if ((const int)to < (const int)from) {
        return -1;
    }
    i = (from & 0x3Fu);
    if ((bb = bs[from >> 6u].u64 >> i) != 0xFFFFFFFFFFFFFFFFu >> i) {
	if ((i = bit64_bsf(~bb) + from) > to) {
	    return -1;
	}
	return (int)i;
    }
    for (i = (from >> 6u) + 1; i <= (to >> 6u); i++) {
	if (bs[i].u64 != 0xFFFFFFFFFFFFFFFFu) {
	    if ((i = bit64_bsf(~bs[i].u64) + (i << 6u)) > to) {
		return -1;
	    }
	    return (int)i;
	}
    }
    return -1;
}

static inline int
barr32_notbsr(const uint32_t bits[],
              const unsigned from,
              const unsigned to)
{
    const union bitunion32 *bs = (const union bitunion32 *)bits;
    unsigned i;
    uint32_t bb;

    if ((const int)to < (const int)from) {
        return -1;
    }
    i = 0x1Fu - (to & 0x1Fu);
    if ((bb = bs[to >> 5u].u32 << i) != 0xFFFFFFFFu << i) {
	if ((i = bit32_bsr(~bb) - 31 + to) < from) {
	    return -1;
	}
	return (int)i;
    }
    for (i = (to >> 5u) - 1; (int32_t)i >= (int32_t)(from >> 5u); i--) {
	if (bs[i].u32 != 0xFFFFFFFFu) {
	    if ((i = bit32_bsr(~bs[i].u32) + (i << 5u)) < from) {
		return -1;
	    }
	    return (int)i;
	}
    }
    return -1;
}
static inline int
barr64_notbsr(const uint64_t bits[],
              const unsigned from,
              const unsigned to)
{
    const union bitunion64 *bs = (const union bitunion64 *)bits;
    unsigned i;
    uint64_t bb;

    if ((const int)to < (const int)from) {
        return -1;
    }
    i = 0x3Fu - (to & 0x3Fu);
    if ((bb = bs[to >> 6u].u64 << i) != 0xFFFFFFFFFFFFFFFFu << i) {
	if ((i = bit64_bsr(~bb) - 0x3Fu + to) < from) {
	    return -1;
	}
	return (int)i;
    }
    for (i = (to >> 6u) - 1; (int)i >= (const int)(from >> 6u); i--) {
	if (bs[i].u64 != 0xFFFFFFFFFFFFFFFFu) {
	    if ((i = bit64_bsr(~bs[i].u64) + (i << 6u)) < from) {
		return -1;
	    }
	    return (int)i;
	}
    }
    return -1;
}

#endif // BITOPS_H
