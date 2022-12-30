/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <string.h>

#include <unittest_helpers.h>
#include <bitops.h>

#define BITOPS_PREFIX bitu
#define BITOPS_BARR_PREFIX barru
#if ARCH_SIZEOF_INT == 4
#define BITOPS_TYPE unsigned
#elif ARCH_SIZEOF_LONG == 4
#define BITOPS_TYPE unsigned long
#endif
#define BITOPS_TYPE_WIDTH 32
#include <bitops.h>

// test that another type can be generated
#define BITOPS_PREFIX bitl
#define BITOPS_BARR_PREFIX barrl
#if ARCH_SIZEOF_INT == 4
#define BITOPS_TYPE unsigned
#elif ARCH_SIZEOF_LONG == 4
#define BITOPS_TYPE unsigned long
#endif
#define BITOPS_TYPE_MAX 2147483647
#include <bitops.h>

static inline void
compile_time_macro_testing_(void)
{
    switch(0){case 0:break;case ARCH_SIZEOF_PTR==sizeof(void *):break;}
    switch(0){case 0:break;case ARCH_SIZEOF_INT==sizeof(int):break;}
    switch(0){case 0:break;case ARCH_SIZEOF_LONG==sizeof(long):break;}
    switch(0){case 0:break;case ARCH_SIZEOF_LONGLONG==sizeof(long long):break;}
}

static uint32_t taus_state[3];

int
main(void)
{
    tausrand_init(taus_state, 0);

    fprintf(stderr, "Test: bit*_bsf/bsr/rev/swap/count/isset/set/unset()...");
    for (int j = 0; j < 10000; j++) {
        for (int i = 0; i < 32; i++) {
            uint32_t mask = tausrand(taus_state);
            mask <<= i;
            mask |= (uint32_t)1 << i;
            ASSERT(bit32_bsf(mask) == i);
            ASSERT(bit32_bsf_generic(mask) == i);
            uint32_t revmask = bit32_rev(mask);
            ASSERT(bit32_rev(revmask) == mask);
            ASSERT(bit32_bsr(revmask) == 31-i);
            ASSERT(bit32_bsr_generic(revmask) == 31-i);

            uint16_t mask16 = (uint16_t)((mask ^ revmask) & 0xFFFF);
            ASSERT(bit16_swap(mask16) == ((mask16 >> 8) | ((mask16 & 0xFF) << 8)));
            ASSERT(bit16_swap_generic(mask16) == ((mask16 >> 8) | ((mask16 & 0xFF) << 8)));
            uint32_t swapped_mask = 0;
            for (int k = 0; k < 32; k += 8) {
                swapped_mask |= ((mask >> k) & 0xFF) << (24 - k);
            }
            ASSERT(bit32_swap(mask) == swapped_mask);
            ASSERT(bit32_swap_generic(mask) == swapped_mask);

            unsigned int count = 0;
            uint32_t masks[2] = { 0xdeadbeef, mask };
            for (int k = 0; k < 32; k++) {
                int isset = !!(mask & (1 << k));
                count += isset;
                ASSERT(bit32_isset(&mask, k) == isset);
                ASSERT(bit32_isset(masks, 32 + k) == isset);

                uint32_t modmask = mask;
                bit32_unset(&modmask, k);
                bit32_unset(masks, 32 + k);
                ASSERT(modmask == (mask & ~(1 << k)));
                ASSERT(masks[1] == (mask & ~(1 << k)) && masks[0] == 0xdeadbeef);
                bit32_set(&modmask, k);
                bit32_set(masks, 32 + k);
                ASSERT(modmask == (mask | (1 << k)));
                ASSERT(masks[1] == (mask | (1 << k)) && masks[0] == 0xdeadbeef);
                masks[1] = mask;
            }
            ASSERT(bit32_count(mask) == count);
            ASSERT(bit32_count_generic(mask) == count);
        }

        // test generated type/functions
        for (int i = 0; i < 32; i++) {
            uint32_t mask = tausrand(taus_state);
            mask <<= i;
            mask |= (uint32_t)1 << i;
            ASSERT(bitu_bsf(mask) == i);
            uint32_t revmask = bitu_rev(mask);
            ASSERT(bitu_rev(revmask) == mask);
            ASSERT(bitu_bsr(revmask) == 31-i);

            uint32_t swapped_mask = 0;
            for (int k = 0; k < 32; k += 8) {
                swapped_mask |= ((mask >> k) & 0xFF) << (24 - k);
            }
            ASSERT(bitu_swap(mask) == swapped_mask);

            unsigned int count = 0;
            uint32_t masks[2] = { 0xdeadbeef, mask };
            for (int k = 0; k < 32; k++) {
                int isset = !!(mask & (1 << k));
                count += isset;
                ASSERT(bitu_isset(&mask, k) == isset);
                ASSERT(bitu_isset(masks, 32 + k) == isset);

                uint32_t modmask = mask;
                bitu_unset(&modmask, k);
                bitu_unset(masks, 32 + k);
                ASSERT(modmask == (mask & ~(1 << k)));
                ASSERT(masks[1] == (mask & ~(1 << k)) && masks[0] == 0xdeadbeef);
                bitu_set(&modmask, k);
                bitu_set(masks, 32 + k);
                ASSERT(modmask == (mask | (1 << k)));
                ASSERT(masks[1] == (mask | (1 << k)) && masks[0] == 0xdeadbeef);
                masks[1] = mask;
            }
            ASSERT(bitu_count(mask) == count);
        }

        for (int i = 0; i < 64; i++) {
            uint64_t mask = ((uint64_t)tausrand(taus_state) << 32) | tausrand(taus_state);
            mask <<= i;
            mask |= (uint64_t)1 << i;
            ASSERT(bit64_bsf(mask) == i);
            ASSERT(bit64_bsf_generic(mask) == i);
            uint64_t revmask = bit64_rev(mask);
            ASSERT(bit64_rev(revmask) == mask);
            ASSERT(bit64_bsr(revmask) == 63-i);
            ASSERT(bit64_bsr_generic(revmask) == 63-i);
            uint64_t swapped_mask = 0;
            for (int k = 0; k < 64; k += 8) {
                swapped_mask |= ((mask >> k) & 0xFF) << (56 - k);
            }
            ASSERT(bit64_swap(mask) == swapped_mask);
            ASSERT(bit64_swap_generic(mask) == swapped_mask);

            unsigned int count = 0;
            uint64_t masks[2] = { 0xdeadbeefdeadbeefull, mask };
            for (int k = 0; k < 64; k++) {
                int isset = !!(mask & (1ull << k));
                count += isset;
                ASSERT(bit64_isset(&mask, k) == isset);
                ASSERT(bit64_isset(masks, 64 + k) == isset);

                uint64_t modmask = mask;
                bit64_unset(&modmask, k);
                bit64_unset(masks, 64 + k);
                ASSERT(modmask == (mask & ~(1ull << k)));
                ASSERT(masks[1] == (mask & ~(1ull << k)) && masks[0] == 0xdeadbeefdeadbeefull);
                bit64_set(&modmask, k);
                bit64_set(masks, 64 + k);
                ASSERT(modmask == (mask | (1ull << k)));
                ASSERT(masks[1] == (mask | (1ull << k)) && masks[0] == 0xdeadbeefdeadbeefull);
                masks[1] = mask;
            }
            ASSERT(bit64_count(mask) == count);
            ASSERT(bit64_count_generic(mask) == count);
        }
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: barr32_*...");
    {
        const int bit_blocks = 4;
        uint32_t bitset[bit_blocks];
        uint32_t sbitset[bit_blocks];
        const int bit_count = bit_blocks * sizeof(bitset[0]) * 8;
        memset(bitset, 0, sizeof(bitset[0]) * bit_blocks);
        memset(sbitset, 0xFF, sizeof(bitset[0]) * bit_blocks);
        for (int i = 0; i < bit_blocks * 32; i++) {
            for (int j = i; j < bit_blocks * 32; j++) {
                barr32_set(bitset, i, j);
                ASSERT(barr32_count(bitset, i, j) == j - i + 1);
                ASSERT(barr32_count(bitset, 0, bit_count - 1) == j - i + 1);
                barr32_unset(sbitset, i, j);
                ASSERT(barr32_count(sbitset, i, j) == 0);
                ASSERT(barr32_count(sbitset, 0, bit_count - 1) == bit_count - (j - i + 1));

                ASSERT(barr32_bsf(sbitset, i, j) == -1);
                ASSERT(barr32_bsf(bitset, 0, bit_count-1) == i);
                if (i > 0) {
                    ASSERT(barr32_bsf(bitset, i-1, j) == i);
                }

                ASSERT(barr32_bsr(sbitset, i, j) == -1);
                ASSERT(barr32_bsr(bitset, 0, bit_count-1) == j);
                if (j < bit_count - 1) {
                    ASSERT(barr32_bsr(bitset, i, j+1) == j);
                }

                ASSERT(barr32_notbsf(bitset, i, j) == -1);
                ASSERT(barr32_notbsf(sbitset, 0, bit_count-1) == i);
                if (i > 0) {
                    ASSERT(barr32_notbsf(sbitset, i-1, j) == i);
                }

                ASSERT(barr32_notbsr(bitset, i, j) == -1);
                ASSERT(barr32_notbsr(sbitset, 0, bit_count-1) == j);
                if (j < bit_count - 1) {
                    ASSERT(barr32_notbsr(sbitset, i, j+1) == j);
                }

                for (int op = 0; op <= 6; op++) {
                    uint32_t dstbits[bit_blocks];
                    uint32_t srcbits[bit_blocks];
                    uint32_t dstcopy[bit_blocks];
                    uint32_t srccopy[bit_blocks];
                    for (int k = 0; k < bit_blocks; k++) {
                        srcbits[k] = tausrand(taus_state);
                        dstbits[k] = tausrand(taus_state);
                        dstcopy[k] = dstbits[k];
                        srccopy[k] = srcbits[k];
                    }
                    switch (op) {
                    case 0:
                        barr32_and(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 1:
                        barr32_nand(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 2:
                        barr32_or(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 3:
                        barr32_nor(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 4:
                        barr32_xor(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 5:
                        barr32_xnor(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 6:
                        barr32_not(dstbits, i, j);
                    }
                    for (int k = 0; k < bit_count; k++) {
                        ASSERT(bit32_isset(srcbits, k) == bit32_isset(srccopy, k));
                        if (k < i || k > j) {
                            ASSERT(bit32_isset(dstbits, k) == bit32_isset(dstcopy, k));
                        } else {
                            int a = bit32_isset(dstcopy, k);
                            int b = bit32_isset(srcbits, k);
                            int c = bit32_isset(dstbits, k);
                            switch (op) {
                            case 0:
                                ASSERT(c == (a & b));
                                break;
                            case 1:
                                ASSERT(c == !(a & b));
                                break;
                            case 2:
                                ASSERT(c == (a | b));
                                break;
                            case 3:
                                ASSERT(c == !(a | b));
                                break;
                            case 4:
                                ASSERT(c == (a ^ b));
                                break;
                            case 5:
                                ASSERT(c == !(a ^ b));
                                break;
                            case 6:
                                ASSERT(c == !a);
                                break;
                            }
                        }
                    }
                }

                barr32_set(sbitset, i, j);
                barr32_unset(bitset, i, j);
            }
        }
        ASSERT(barr32_count(bitset, 1, 0) == 0);
        ASSERT(barr32_bsf(bitset, 1, 0) == -1);
        ASSERT(barr32_bsr(bitset, 1, 0) == -1);
        ASSERT(barr32_notbsf(bitset, 1, 0) == -1);
        ASSERT(barr32_notbsr(bitset, 1, 0) == -1);
    }

    // copy past to test generated functions
    {
        const int bit_blocks = 4;
        uint32_t bitset[bit_blocks];
        uint32_t sbitset[bit_blocks];
        const int bit_count = bit_blocks * sizeof(bitset[0]) * 8;
        memset(bitset, 0, sizeof(bitset[0]) * bit_blocks);
        memset(sbitset, 0xFF, sizeof(bitset[0]) * bit_blocks);
        for (int i = 0; i < bit_blocks * 32; i++) {
            for (int j = i; j < bit_blocks * 32; j++) {
                barru_set(bitset, i, j);
                ASSERT(barru_count(bitset, i, j) == j - i + 1);
                ASSERT(barru_count(bitset, 0, bit_count - 1) == j - i + 1);
                barru_unset(sbitset, i, j);
                ASSERT(barru_count(sbitset, i, j) == 0);
                ASSERT(barru_count(sbitset, 0, bit_count - 1) == bit_count - (j - i + 1));

                ASSERT(barru_bsf(sbitset, i, j) == -1);
                ASSERT(barru_bsf(bitset, 0, bit_count-1) == i);
                if (i > 0) {
                    ASSERT(barru_bsf(bitset, i-1, j) == i);
                }

                ASSERT(barru_bsr(sbitset, i, j) == -1);
                ASSERT(barru_bsr(bitset, 0, bit_count-1) == j);
                if (j < bit_count - 1) {
                    ASSERT(barru_bsr(bitset, i, j+1) == j);
                }

                ASSERT(barru_notbsf(bitset, i, j) == -1);
                ASSERT(barru_notbsf(sbitset, 0, bit_count-1) == i);
                if (i > 0) {
                    ASSERT(barru_notbsf(sbitset, i-1, j) == i);
                }

                ASSERT(barru_notbsr(bitset, i, j) == -1);
                ASSERT(barru_notbsr(sbitset, 0, bit_count-1) == j);
                if (j < bit_count - 1) {
                    ASSERT(barru_notbsr(sbitset, i, j+1) == j);
                }

                for (int op = 0; op <= 6; op++) {
                    uint32_t dstbits[bit_blocks];
                    uint32_t srcbits[bit_blocks];
                    uint32_t dstcopy[bit_blocks];
                    uint32_t srccopy[bit_blocks];
                    for (int k = 0; k < bit_blocks; k++) {
                        srcbits[k] = tausrand(taus_state);
                        dstbits[k] = tausrand(taus_state);
                        dstcopy[k] = dstbits[k];
                        srccopy[k] = srcbits[k];
                    }
                    switch (op) {
                    case 0:
                        barru_and(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 1:
                        barru_nand(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 2:
                        barru_or(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 3:
                        barru_nor(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 4:
                        barru_xor(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 5:
                        barru_xnor(dstbits, (const uint32_t *)srcbits, i, j);
                        break;
                    case 6:
                        barru_not(dstbits, i, j);
                    }
                    for (int k = 0; k < bit_count; k++) {
                        ASSERT(bit32_isset(srcbits, k) == bit32_isset(srccopy, k));
                        if (k < i || k > j) {
                            ASSERT(bit32_isset(dstbits, k) == bit32_isset(dstcopy, k));
                        } else {
                            int a = bit32_isset(dstcopy, k);
                            int b = bit32_isset(srcbits, k);
                            int c = bit32_isset(dstbits, k);
                            switch (op) {
                            case 0:
                                ASSERT(c == (a & b));
                                break;
                            case 1:
                                ASSERT(c == !(a & b));
                                break;
                            case 2:
                                ASSERT(c == (a | b));
                                break;
                            case 3:
                                ASSERT(c == !(a | b));
                                break;
                            case 4:
                                ASSERT(c == (a ^ b));
                                break;
                            case 5:
                                ASSERT(c == !(a ^ b));
                                break;
                            case 6:
                                ASSERT(c == !a);
                                break;
                            }
                        }
                    }
                }

                barru_set(sbitset, i, j);
                barru_unset(bitset, i, j);
            }
        }
        ASSERT(barru_count(bitset, 1, 0) == 0);
        ASSERT(barru_bsf(bitset, 1, 0) == -1);
        ASSERT(barru_bsr(bitset, 1, 0) == -1);
        ASSERT(barru_notbsf(bitset, 1, 0) == -1);
        ASSERT(barru_notbsr(bitset, 1, 0) == -1);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: barr64_*...");
    // exact copy of 32 bit tests with 32=>64
    {
        const int bit_blocks = 4;
        uint64_t bitset[bit_blocks];
        uint64_t sbitset[bit_blocks];
        const int bit_count = bit_blocks * sizeof(bitset[0]) * 8;
        memset(bitset, 0, sizeof(bitset[0]) * bit_blocks);
        memset(sbitset, 0xFF, sizeof(bitset[0]) * bit_blocks);
        for (int i = 0; i < bit_blocks * 64; i++) {
            for (int j = i; j < bit_blocks * 64; j++) {
                barr64_set(bitset, i, j);
                ASSERT(barr64_count(bitset, i, j) == j - i + 1);
                ASSERT(barr64_count(bitset, 0, bit_count - 1) == j - i + 1);
                barr64_unset(sbitset, i, j);
                ASSERT(barr64_count(sbitset, i, j) == 0);
                ASSERT(barr64_count(sbitset, 0, bit_count - 1) == bit_count - (j - i + 1));

                ASSERT(barr64_bsf(sbitset, i, j) == -1);
                ASSERT(barr64_bsf(bitset, 0, bit_count-1) == i);
                if (i > 0) {
                    ASSERT(barr64_bsf(bitset, i-1, j) == i);
                }

                ASSERT(barr64_bsr(sbitset, i, j) == -1);
                ASSERT(barr64_bsr(bitset, 0, bit_count-1) == j);
                if (j < bit_count - 1) {
                    ASSERT(barr64_bsr(bitset, i, j+1) == j);
                }

                ASSERT(barr64_notbsf(bitset, i, j) == -1);
                ASSERT(barr64_notbsf(sbitset, 0, bit_count-1) == i);
                if (i > 0) {
                    ASSERT(barr64_notbsf(sbitset, i-1, j) == i);
                }

                ASSERT(barr64_notbsr(bitset, i, j) == -1);
                ASSERT(barr64_notbsr(sbitset, 0, bit_count-1) == j);
                if (j < bit_count - 1) {
                    ASSERT(barr64_notbsr(sbitset, i, j+1) == j);
                }

                for (int op = 0; op <= 6; op++) {
                    uint64_t dstbits[bit_blocks];
                    uint64_t srcbits[bit_blocks];
                    uint64_t dstcopy[bit_blocks];
                    uint64_t srccopy[bit_blocks];
                    for (int k = 0; k < bit_blocks; k++) {
                        srcbits[k] = tausrand(taus_state);
                        dstbits[k] = tausrand(taus_state);
                        dstcopy[k] = dstbits[k];
                        srccopy[k] = srcbits[k];
                    }
                    switch (op) {
                    case 0:
                        barr64_and(dstbits, (const uint64_t *)srcbits, i, j);
                        break;
                    case 1:
                        barr64_nand(dstbits, (const uint64_t *)srcbits, i, j);
                        break;
                    case 2:
                        barr64_or(dstbits, (const uint64_t *)srcbits, i, j);
                        break;
                    case 3:
                        barr64_nor(dstbits, (const uint64_t *)srcbits, i, j);
                        break;
                    case 4:
                        barr64_xor(dstbits, (const uint64_t *)srcbits, i, j);
                        break;
                    case 5:
                        barr64_xnor(dstbits, (const uint64_t *)srcbits, i, j);
                        break;
                    case 6:
                        barr64_not(dstbits, i, j);
                    }
                    for (int k = 0; k < bit_count; k++) {
                        ASSERT(bit64_isset(srcbits, k) == bit64_isset(srccopy, k));
                        if (k < i || k > j) {
                            ASSERT(bit64_isset(dstbits, k) == bit64_isset(dstcopy, k));
                        } else {
                            int a = bit64_isset(dstcopy, k);
                            int b = bit64_isset(srcbits, k);
                            int c = bit64_isset(dstbits, k);
                            switch (op) {
                            case 0:
                                ASSERT(c == (a & b));
                                break;
                            case 1:
                                ASSERT(c == !(a & b));
                                break;
                            case 2:
                                ASSERT(c == (a | b));
                                break;
                            case 3:
                                ASSERT(c == !(a | b));
                                break;
                            case 4:
                                ASSERT(c == (a ^ b));
                                break;
                            case 5:
                                ASSERT(c == !(a ^ b));
                                break;
                            case 6:
                                ASSERT(c == !a);
                                break;
                            }
                        }
                    }
                }

                barr64_set(sbitset, i, j);
                barr64_unset(bitset, i, j);
            }
        }
        ASSERT(barr64_count(bitset, 1, 0) == 0);
        ASSERT(barr64_bsf(bitset, 1, 0) == -1);
        ASSERT(barr64_bsr(bitset, 1, 0) == -1);
        ASSERT(barr64_notbsf(bitset, 1, 0) == -1);
        ASSERT(barr64_notbsr(bitset, 1, 0) == -1);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Unittests for bitops.h passed successfully\n");
    return 0;
}
