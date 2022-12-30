/*
 * Copyright (c) 2011 - 2012, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#ifndef MC_ARCH_H
#define MC_ARCH_H

#include <limits.h>
#include <stdint.h>

#ifndef ARCH_SIZEOF_INT
#if INT_MAX == 2147483647LL
#define ARCH_SIZEOF_INT 4
#elif INT_MAX == 9223372036854775807LL
#define ARCH_SIZEOF_INT 8
#else
 #error "Unsupported size of INT_MAX"
#endif
#endif

#ifndef ARCH_SIZEOF_LONG
#if LONG_MAX == 2147483647LL
#define ARCH_SIZEOF_LONG 4
#elif LONG_MAX == 9223372036854775807LL
#define ARCH_SIZEOF_LONG 8
#else
 #error "Unsupported size of LONG_MAX"
#endif
#endif

#ifndef ARCH_SIZEOF_LONGLONG
#if LLONG_MAX == 2147483647LL
#define ARCH_SIZEOF_LONGLONG 4
#elif LLONG_MAX == 9223372036854775807LL
#define ARCH_SIZEOF_LONGLONG 8
#else
 #error "Unsupported size of LLONG_MAX"
#endif
#endif

#ifndef ARCH_SIZEOF_PTR
#if UINTPTR_MAX == 4294967295ULL
#define ARCH_SIZEOF_PTR 4
#elif UINTPTR_MAX == 18446744073709551615ULL
#define ARCH_SIZEOF_PTR 8
#else
 #error "Unsupported size of UINTPTR_MAX"
#endif
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ARCH_LITTLE_ENDIAN 1
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ARCH_BIG_ENDIAN 1
#endif

#if defined(__i386__) || defined(_M_IX86)
#define ARCH_X86 1
#elif defined(__x86_64__) || defined(_M_X64)
#define ARCH_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_ARM64 1
#endif

// If this function fails at compile time, the architecture identification is wrong.
static __inline void
xarepo_arch_compile_time_macro_testing_(void)
{
    switch(0){case 0:break;case ARCH_SIZEOF_PTR==sizeof(void *):break;} // NOLINT
    switch(0){case 0:break;case ARCH_SIZEOF_INT==sizeof(int):break;} // NOLINT
    switch(0){case 0:break;case ARCH_SIZEOF_LONG==sizeof(long):break;} // NOLINT
    switch(0){case 0:break;case ARCH_SIZEOF_LONGLONG==sizeof(long long):break;} // NOLINT
}

#endif
