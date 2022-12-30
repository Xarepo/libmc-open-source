/*
 * Copyright (c) 2013, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#ifndef UNITTEST_HELPERS_H
#define UNITTEST_HELPERS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <mc_arch.h>

#if ARCH_X86_64 - 0 != 0 || ARCH_X86 - 0 != 0
#include <x86intrin.h>
#endif

#define ASSERT(expr)                                   \
    do {                                               \
        if (!(expr)) {                                 \
            fprintf(stderr, "\"%s\" failed\n", #expr); \
            exit(EXIT_FAILURE);                        \
        }                                              \
    } while(0)

static inline uint32_t
tausrand(uint32_t state[3])
{
#define TAUSRAND(s,a,b,c,d) ((s & c) << d) ^ (((s << a) ^ s) >> b)
    state[0] = TAUSRAND(state[0], 13, 19, ((uint32_t)0xFFFFFFFEL), 12);
    state[1] = TAUSRAND(state[1], 2, 25, ((uint32_t)0xFFFFFFF8L), 4);
    state[2] = TAUSRAND(state[2], 3, 11, ((uint32_t)0xFFFFFFF0L), 17);
#undef TAUSRAND
    return (state[0] ^ state[1] ^ state[2]);
}

static inline void
tausrand_init(uint32_t state[3],
              uint32_t seed)
{
    /* default seed is 1 */
    if (seed == 0) {
	seed = 1;
    }

#define LCG(n) ((69069 * n) & 0xFFFFFFFFL)
    state[0] = LCG(seed);
    state[1] = LCG(state[0]);
    state[2] = LCG(state[1]);
#undef LCG
    /* "warm it up" */
    tausrand(state);
    tausrand(state);
    tausrand(state);
    tausrand(state);
    tausrand(state);
    tausrand(state);
}

#if ARCH_X86_64 - 0 != 0 || ARCH_X86 - 0 != 0
static inline void
timestamp(uint64_t *ts)
{
    // from brief tests it doesn't seem to be a problem that it's not asm volatile,
    // ie the rdtsc is kept in the place it's called even after optimisation.
    *ts = __rdtsc();
    /*
    uint32_t hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    *ts = (uint64_t)hi << 32 | lo;
    */
}
#endif // ARCH_X86_64

#if ARCH_ARM64 - 0 != 0
static inline void
timestamp(uint64_t *ts)
{
    __asm__ __volatile__ ("mrs %0, cntvct_el0" : "=r" (*ts));
}
#endif // ARCH_ARM64

#endif
