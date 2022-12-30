/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <ctype.h>

#include <unittest_helpers.h>

#include <bitops.h>
#include <mrx_base_int.h>
#include <mrx_scan.h>
#include <mrx_tmpl.h>

#define MC_MM_MODE MC_MM_PERFORMANCE
#define MC_PREFIX mrxi
#define MC_KEY_T uintptr_t
#define MC_VALUE_T void *
#define MRX_KEY_SORTINT 1
#include <mrx_tmpl.h>

#define MC_MM_MODE MC_MM_PERFORMANCE
#define MC_PREFIX mrxa
#define MC_KEY_T uintptr_t
#define MC_VALUE_T char *
#define MC_COPY_VALUE(dest, src) dest = strdup(src)
#define MC_FREE_VALUE(value) free(value)
#define MRX_KEY_SORTINT 1
#include <mrx_tmpl.h>

static uint32_t taus_state[3];

#define BUILD_AVX2 0

static struct {
    bool sse42_supported;
    bool sse2_supported;
    bool avx2_supported;
} glob;

static void
mrx_scan_tests(void)
{
    fprintf(stderr, "Test: prefix_find_first_diff_*()...\n");
    {
        const int max_px_len = 118;
        uint8_t prefix1_[64 + 118];
        uint8_t prefix2_[64 + 118];
        uint64_t tssum[5] = {0,0,0,0,0};
        uint64_t tssum_ref = 0;
        uint64_t ts1, ts2;
        unsigned test_count = 0;
        const int test_size = 100;
        for (int i = 0; i < test_size; i++) {
            for (int is_aligned_32 = 0; is_aligned_32 < 2; is_aligned_32++) {
                uint8_t *prefix1 = &prefix1_[1];
                uint8_t *prefix2 = &prefix2_[2];
                const uintptr_t align_mask = 2u * sizeof(uintptr_t) - 1u;
                while (((uintptr_t)prefix1 & align_mask) != 0) {
                    prefix1++;
                }
                if (((uintptr_t)prefix1 & 31u) != 0 && is_aligned_32) {
                    prefix1 += 2 * sizeof(uintptr_t);
                }
                prefix1 += 2; // should be 2 bytes from aligned
                prefix1[-1] = 0xFE; // make sure byte before is not zero
                prefix1[-2] = 0xFE; // make sure byte before is not zero
                while (((uintptr_t)prefix2 & align_mask) != 0) {
                    prefix2++;
                }
                prefix2 += 1; // test with unaligned
                prefix2[-1] = 0xFE; // make sure byte before is not zero
                prefix2[-2] = 0xFE; // make sure byte before is not zero
                for (unsigned j = 0; j < max_px_len+2; j++) {
                    prefix1[j] = 'a' + tausrand(taus_state) % 26;
                }
                for (int px_len = 0; px_len <= max_px_len; px_len++) {
                    for (int dp = 0; dp <= px_len + 1; dp++) {
                        memcpy(prefix2, prefix1, px_len);
                        prefix2[dp] = prefix1[dp] + 1;
                        prefix2[px_len] = prefix1[px_len]; // match post end to see if functions scan too far

                        timestamp(&ts1);
                        timestamp(&ts2);
                        tssum_ref += ts2 - ts1;
                        timestamp(&ts1);
                        uint8_t ref = mrx_prefix_find_first_diff_ref_impl_(prefix2, prefix1, px_len);
                        timestamp(&ts2);
                        tssum[0] += ts2 - ts1;
                        timestamp(&ts1);
                        uint8_t res1 = mrx_prefix_find_first_diff_generic_(prefix2, prefix1, px_len);
                        timestamp(&ts2);
                        tssum[1] += ts2 - ts1;
                        ASSERT(res1 == ref);
                        if (glob.sse2_supported) {
                            timestamp(&ts1);
                            int8_t res2 = mrx_prefix_find_first_diff_sse2_(prefix2, prefix1, px_len);
                            timestamp(&ts2);
                            tssum[2] += ts2 - ts1;
                            ASSERT(res2 == ref);
                        }
                        if (glob.sse42_supported) {
                            timestamp(&ts1);
                            int8_t res3 = mrx_prefix_find_first_diff_sse42_(prefix2, prefix1, px_len);
                            timestamp(&ts2);
                            tssum[3] += ts2 - ts1;
                            ASSERT(res3 == ref);
                        }
#if BUILD_AVX2 - 0 != 0
                        if (glob.avx2_supported) {
                            timestamp(&ts1);
                            int8_t res_avx2 = mrx_prefix_find_first_diff_avx2_(prefix2, prefix1, px_len);
                            timestamp(&ts2);
                            tssum[4] += ts2 - ts1;
                            ASSERT(res_avx2 == ref);
                        }
#endif
                        // check that header bytes haven't been destroyed
                        ASSERT(prefix1[-1] == 0xFE && prefix1[-2] == 0xFE);
                        ASSERT(prefix2[-1] == 0xFE && prefix2[-2] == 0xFE);
                        test_count++;
                    }
                }
            }
        }
        fprintf(stderr, "\n");
        fprintf(stderr, "  prefix_find_first_diff_ref_impl() %llu cc\n", (long long)(tssum[0] - tssum_ref) / test_count);
        fprintf(stderr, "  prefix_find_first_diff_generic() %llu cc\n", (long long)(tssum[1] - tssum_ref) / test_count);
        if (glob.sse2_supported) {
            fprintf(stderr, "  prefix_find_first_diff_sse2() %llu cc\n", (long long)(tssum[2] - tssum_ref) / test_count);
        }
        if (glob.sse42_supported) {
            fprintf(stderr, "  prefix_find_first_diff_sse42() %llu cc\n", (long long)(tssum[3] - tssum_ref) / test_count);
        }
        if (glob.avx2_supported) {
            fprintf(stderr, "  prefix_find_first_diff_avx2() %llu cc\n", (long long)(tssum[4] - tssum_ref) / test_count);
        }
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: find_branch_*(), find_new_branch_pos_*()...");
    {
        uint64_t tssum[5] = {0,0,0,0,0};
        uint64_t tssum_ref = 0;
        uint64_t tssum1[4] = {0,0,0,0};
        uint64_t tssum1_ref = 0;
        unsigned test1_count = 0;
        uint8_t br_[64 + SCAN_NODE_MAX_BRANCH_COUNT];
        uint8_t *br;
        const int test_size = 100000;
        for (int i = 0; i < test_size; i++) {
            int len = tausrand(taus_state) % (SCAN_NODE_MAX_BRANCH_COUNT + 1);
            // to test worst case performance, set length to maximum value
            // len = SCAN_NODE_MAX_BRANCH_COUNT;

            br = &br_[tausrand(taus_state) % 16]; // random alignment
            uint64_t mask[4] = {0,0,0,0};
            for (int j = 0; j < len; j++) {
                bit64_set(mask, tausrand(taus_state) & 0xFF);
            }
            // make sorted branch array
            int idx = -1;
            len = 0;
            while ((idx = barr64_bsf(mask, idx + 1, 255)) != -1) {
                br[len++] = idx;
            }
            uint64_t ts1, ts2;
            // find_branch_*()
            for (int c = 0; c < 256; c++) {
                timestamp(&ts1);
                timestamp(&ts2);
                tssum_ref += ts2 - ts1;
                timestamp(&ts1);
                int8_t ref = mrx_find_branch_ref_impl_(c, br, len);
                timestamp(&ts2);
                tssum[0] += ts2 - ts1;
                timestamp(&ts1);
                int8_t res_generic = mrx_find_branch_generic_(c, br, len);
                timestamp(&ts2);
                tssum[1] += ts2 - ts1;
                ASSERT(res_generic == ref);
                if (glob.sse2_supported) {
                    timestamp(&ts1);
                    int8_t res_sse2 = mrx_find_branch_sse2_(c, br, len);
                    timestamp(&ts2);
                    tssum[2] += ts2 - ts1;
                    ASSERT(res_sse2 == ref);
                }
                if (glob.sse42_supported) {
                    timestamp(&ts1);
                    int8_t res_sse42 = mrx_find_branch_sse42_(c, br, len);
                    timestamp(&ts2);
                    tssum[3] += ts2 - ts1;
                    ASSERT(res_sse42 == ref);
                }
#if BUILD_AVX2 - 0 != 0
                if (glob.avx2_supported) {
                    timestamp(&ts1);
                    int8_t res_avx2 = mrx_find_branch_avx2_(c, br, len);
                    timestamp(&ts2);
                    tssum[4] += ts2 - ts1;
                    ASSERT(res_avx2 == ref);
                }
#endif
            }

            // find_new_branch_pos_*()
            for (int c = 0; c < 256; c++) {
                if (bit64_isset(mask, c)) {
                    continue;
                }
                test1_count++;
                timestamp(&ts1);
                timestamp(&ts2);
                tssum1_ref += ts2 - ts1;
                timestamp(&ts1);
                uint8_t ref = mrx_find_new_branch_pos_ref_impl_(c, br, len);
                timestamp(&ts2);
                tssum1[0] += ts2 - ts1;
                timestamp(&ts1);
                int8_t res_generic = mrx_find_new_branch_pos_generic_(c, br, len);
                timestamp(&ts2);
                tssum1[1] += ts2 - ts1;
                ASSERT(res_generic == ref);
                if (glob.sse2_supported) {
                    timestamp(&ts1);
                    uint8_t res_sse2 = mrx_find_new_branch_pos_sse2_(c, br, len);
                    timestamp(&ts2);
                    tssum1[2] += ts2 - ts1;
                    ASSERT(res_sse2 == ref);
                }
            }
        }
        fprintf(stderr, "\n");
        fprintf(stderr, "  note: performance averaged over all branch array lengths,\n"
                "     largest difference is found for long lengths with matching octet towards the back,\n"
                "     also test program needs to be compiled with -O3 to get valid performance results\n");
        fprintf(stderr, "  find_branch_ref_impl() %llu cc\n", (long long)(tssum[0] - tssum_ref) / test_size / 256);
        fprintf(stderr, "  find_branch_generic() %llu cc\n", (long long)(tssum[1] - tssum_ref) / test_size / 256);
        if (glob.sse2_supported) {
            fprintf(stderr, "  find_branch_sse2() %llu cc\n", (long long)(tssum[2] - tssum_ref) / test_size / 256);
        }
        if (glob.sse42_supported) {
            fprintf(stderr, "  find_branch_sse42() %llu cc\n", (long long)(tssum[3] - tssum_ref) / test_size / 256);
        }
        if (glob.avx2_supported) {
            fprintf(stderr, "  find_branch_avx2() %llu cc\n", (long long)(tssum[4] - tssum_ref) / test_size / 256);
        }
        fprintf(stderr, "\n");
        fprintf(stderr, "  find_new_branch_pos_ref_impl() %llu cc\n", (long long)(tssum1[0] - tssum1_ref) / test1_count);
        fprintf(stderr, "  find_new_branch_pos_generic() %llu cc\n", (long long)(tssum1[1] - tssum1_ref) / test1_count);
        if (glob.sse2_supported) {
            fprintf(stderr, "  find_new_branch_pos_sse2() %llu cc\n", (long long)(tssum1[2] - tssum1_ref) / test1_count);
        }
    }
    fprintf(stderr, "pass\n");
}

static inline char *
make_string_key(char str[],
                uintptr_t key)
{
    uint32_t num = ((key & 0xFF) ^
                    (key & 0xFF00) >> 8 ^
                    (key & 0xFF0000) >> 16 ^
                    (key & 0xFF000000) >> 24);
    char keyc = '_';
    char keyd = '-';
    if (isprint(num)) {
        keyc = num;
    }
    if (isprint(255-num)) {
        keyd = 255-num;
    }
    sprintf(str, "%c%c%016llx%c%llu%c%lld",
            keyc,
            keyd,
            (unsigned long long)key,
            keyc,
            (unsigned long long)key,
            keyd,
            (unsigned long long)key);
    int c = 18 + bit64_count((uint64_t)key);
    str[c] = '\0';
    return str;
}

static int
random_cmp(const void *a,
           const void *b)
{
    return (tausrand(taus_state) & 1) == 0 ? 1 : -1;
}

static uintptr_t
random_key(void)
{
#if ARCH_SIZEOF_PTR == 4
    return (uintptr_t)tausrand(taus_state);
#else
    return ((uintptr_t)tausrand(taus_state) << 32) | tausrand(taus_state);
#endif
}

static void
mrx_random_tree_tests(void)
{
    fprintf(stderr, "Test: random tree to cover callable insert/erase/find mrx_base.h functions...");
    {
        mrx_t *tt = mrx_new(~0u);

        { // test full capacity limit
            uintptr_t capacity = tt->mrx.capacity;
            tt->mrx.capacity = 0;
            void *ret1 = mrx_insertnt(tt, "full", (void *)0x1);
            void *ret2 = mrx_insert(tt, "full", 4, (void *)0x1);
            ASSERT(ret1 == NULL);
            ASSERT(ret2 == NULL);
            tt->mrx.capacity = capacity;
        }

        const int test_size = 100000;
        uintptr_t *keys = malloc(test_size * sizeof(uintptr_t));
        for (int i = 0; i < test_size; i++) {
            keys[i] = random_key();
        }
        uint32_t *random_index = malloc(test_size * sizeof(random_index[0]));
        for (int i = 0; i < test_size; i++) {
            random_index[i] = i;
        }
        qsort(random_index, test_size, sizeof(uint32_t), random_cmp);
        int erase_index = 0;
        char string_key[MRX_MAX_KEY_LENGTH_FOR_PATH_ON_STACK + 1024];
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 100;
            if (operation < 50) {
                // find
                uint32_t k = tausrand(taus_state) % (i+1);
                uintptr_t key = keys[random_index[k]];
                make_string_key(string_key, key);
                if (tausrand(taus_state) % 10 == 0) {
                    k = tausrand(taus_state) % strlen(string_key);
                    if (tausrand(taus_state) % 4 == 0) {
                        string_key[k] = '\0';
                    } else {
                        string_key[k] = 'a' + tausrand(taus_state) % 26;
                    }
                    int mlen;
                    void *ret = mrx_findnearnt(tt, string_key, &mlen);
                    if (ret != NULL) {
                        ASSERT(mrx_find(tt, string_key, mlen) == ret);
                    } else {
                        ASSERT(mrx_findnt(tt, string_key) == NULL);
                    }
                }
                void *ret = mrx_findnt(tt, string_key);
                void *ret1 = mrx_find(tt, string_key, strlen(string_key));
                ASSERT(ret == ret1);
                if (ret != NULL) {
                    int mlen = strlen(string_key);
                    int mlen2;
                    int mlen3;
                    if (tausrand(taus_state) % 10 != 0) {
                        strcat(string_key, "miss");
                    }
                    void *ret2 = mrx_findnearnt(tt, string_key, &mlen2);
                    void *ret3 = mrx_findnear(tt, string_key, strlen(string_key), &mlen3);
                    ASSERT(ret2 == ret);
                    ASSERT(ret3 == ret);
                    ASSERT(mlen2 == mlen);
                    ASSERT(mlen3 == mlen);
                }
            } else if (operation < 80) {
                // insert
                uintptr_t key = keys[random_index[i]];
                make_string_key(string_key, key);
                if (tausrand(taus_state) % 1000 == 0) {
                    int len = strlen(string_key);
                    int j;
                    for (j = 0; j < MRX_MAX_KEY_LENGTH_FOR_PATH_ON_STACK + 10; j++) {
                        string_key[len+j] = 'a';
                    }
                    string_key[len+j] = '\0';
                }
                mrx_insertnt(tt, string_key, (void *)(uintptr_t)key);
                if (tausrand(taus_state) % 100 == 0) {
                    // test replacing value
                    mrx_insertnt(tt, string_key, (void *)0x1);
                    ASSERT(mrx_findnt(tt, string_key) == (void *)0x1);
                    mrx_insertnt(tt, string_key, (void *)(uintptr_t)key);
                    ASSERT(mrx_findnt(tt, string_key) == (void *)(uintptr_t)key);

                    // test inserting value matching a node without value
                    int len = strlen(string_key);
                    string_key[len+1] = '\0';
                    string_key[len] = 'a';
                    mrx_insertnt(tt, string_key, (void *)0x1);
                    string_key[len] = 'b';
                    mrx_insertnt(tt, string_key, (void *)0x2);
                    string_key[len] = '\0';
                    mrx_erasent(tt, string_key);
                    ASSERT(mrx_findnt(tt, string_key) == NULL);
                    mrx_insertnt(tt, string_key, (void *)(uintptr_t)key);
                    ASSERT(mrx_findnt(tt, string_key) == (void *)(uintptr_t)key);
                }
            } else {
                // erase
                uintptr_t key = keys[random_index[erase_index]];
                erase_index++;
                make_string_key(string_key, key);
                if (tausrand(taus_state) % 20 == 0) {
                    strcat(string_key, "miss");
                }
                mrx_erasent(tt, string_key);
            }
            if (i % 1000 == 0) {
                mrx_debug_sanity_check_str2ref(&tt->mrx);
            }
        }
        mrx_debug_sanity_check_str2ref(&tt->mrx);
        mrx_delete(tt);
        free(keys);
        free(random_index);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: mrx integer tree...");
    {
        mrxi_t *tt = mrxi_new(~0u);

        const int test_size = 100000;
        uintptr_t *keys = malloc(test_size * sizeof(uintptr_t));
        for (int i = 0; i < test_size; i++) {
            keys[i] = random_key();
        }
        uint32_t *random_index = malloc(test_size * sizeof(random_index[0]));
        for (int i = 0; i < test_size; i++) {
            random_index[i] = i;
        }
        qsort(random_index, test_size, sizeof(uint32_t), random_cmp);
        int erase_index = 0;
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 100;
            if (operation < 50) {
                // find
                uint32_t k = tausrand(taus_state) % (i+1);
                uintptr_t key = keys[random_index[k]];
                void *ret = mrxi_find(tt, key);
                if (ret != NULL) {
                    ASSERT((uintptr_t)ret == key);
                }
            } else if (operation < 80) {
                // insert
                uintptr_t key = keys[random_index[i]];
                mrxi_insert(tt, key, (void *)(uintptr_t)key);
            } else {
                // erase
                uintptr_t key = keys[random_index[erase_index]];
                erase_index++;
                if (tausrand(taus_state) % 20 == 0) {
                    key |= tausrand(taus_state);
                }
                mrxi_erase(tt, key);
            }
            if (i % 1000 == 0) {
                mrx_debug_sanity_check_int2ref(&tt->mrx);
            }
        }
        mrx_debug_sanity_check_int2ref(&tt->mrx);

        { // test integer sorting
            uintptr_t prev_key = 0;
            for (mrxi_it_t *it = mrxi_beginst(tt, alloca(mrxi_itsize(tt))); it != mrxi_end(); it = mrxi_next(it)) {
                ASSERT(mrxi_key(it) > prev_key);
                ASSERT((uintptr_t)mrxi_val(it) == mrxi_key(it));
                prev_key = mrxi_key(it);
            }
            mrxi_itdelete(mrxi_begin(tt)); // just to cover heap iterator code
        }

        mrxi_delete(tt);
        free(keys);
        free(random_index);
    }
    fprintf(stderr, "pass\n");


    fprintf(stderr, "Test: mrx integer tree with allocated values...");
    {
        mrxa_t *tt = mrxa_new(~0u);

        const int test_size = 100000;
        uintptr_t *keys = malloc(test_size * sizeof(uintptr_t));
        for (int i = 0; i < test_size; i++) {
            keys[i] = random_key();
        }
        uint32_t *random_index = malloc(test_size * sizeof(random_index[0]));
        for (int i = 0; i < test_size; i++) {
            random_index[i] = i;
        }
        qsort(random_index, test_size, sizeof(uint32_t), random_cmp);
        int erase_index = 0;
        char value[1024];
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 100;
            if (operation < 50) {
                // find
                uint32_t k = tausrand(taus_state) % (i+1);
                uintptr_t key = keys[random_index[k]];
                make_string_key(value, key);
                char *ret = mrxa_find(tt, key);
                if (ret != NULL) {
                    ASSERT(strcmp(value, ret) == 0);
                }
            } else if (operation < 80) {
                // insert
                uintptr_t key = keys[random_index[i]];
                make_string_key(value, key);
                mrxa_insert(tt, key, value);
                if (tausrand(taus_state) % 100 == 0) {
                    // try replace existing value
                    mrxa_insert(tt, key, value);
                }
            } else {
                // erase
                uintptr_t key = keys[random_index[erase_index]];
                erase_index++;
                if (tausrand(taus_state) % 20 == 0) {
                    key |= tausrand(taus_state);
                }
                mrxa_erase(tt, key);
            }
        }
        mrxa_delete(tt);
        free(keys);
        free(random_index);
    }
    fprintf(stderr, "pass\n");
}

static void
mrx_alloc_free_tests(mrx_base_t *mrx)
{
    const bool compact_mode = ((mrx->max_keylen_n_flags & MRX_FLAG_IS_COMPACT_) != 0);
    const int alloc_test_size = 10000;
    mrx_sp_t *nodes[alloc_test_size];
    mrx_sp_t *shadow_nodes[alloc_test_size];
    unsigned p2s[alloc_test_size];
    for (int i = 0; i < alloc_test_size; i++) {
        int p2 = MIN_P2 + tausrand(taus_state) % (MAX_P2 - MIN_P2 + 1);
        mrx_sp_t *node = mrx_alloc_node_(mrx, p2 == MAX_P2 ? p2 + tausrand(taus_state) % 10: p2);
        unsigned size = (8u << p2) / sizeof(mrx_sp_t);
        if (compact_mode) {
            node[0] = tausrand(taus_state);
        } else {
            node[0] |= tausrand(taus_state) & SP_MASK; // test that free bit is not colliding with short pointer
        }
        for (int j = 1; j < size; j++) {
            node[j] = tausrand(taus_state);
        }
        uintptr_t mask = compact_mode ? 2 * sizeof(void *) - 1 : (8u << p2) - 1;
        ASSERT(((uintptr_t)node & mask) == 0);
        shadow_nodes[i] = malloc(8u << p2);
        memcpy(shadow_nodes[i], node, 8u << p2);
        nodes[i] = node;
        p2s[i] = p2;
        if (tausrand(taus_state) % 4 == 0) {
            int j = tausrand(taus_state) % (i + 1);
            if (nodes[j] != NULL) {
                ASSERT(memcmp(shadow_nodes[j], nodes[j], 8u << p2s[j]) == 0);
                free(shadow_nodes[j]);
                p2 = p2s[j] == MAX_P2 ? p2s[j] + tausrand(taus_state) % 10: p2s[j];
                mrx_free_node_(mrx, nodes[j], p2);
                nodes[j] = NULL;
            }
        }
    }

    for (int i = 0; i < alloc_test_size; i++) {
        if (nodes[i] != NULL) {
            ASSERT(memcmp(shadow_nodes[i], nodes[i], 8u << p2s[i]) == 0);
            free(shadow_nodes[i]);
            if (compact_mode) {
                mrx_free_node_(mrx, nodes[i], p2s[i]);
                nodes[i] = NULL;
            }
        }
    }

    for (int p2 = MIN_P2; p2 <= MAX_P2; p2++) {
        for (int i = 0; i < alloc_test_size; i++) {
            void *node = mrx_alloc_node_(mrx, p2);
            uintptr_t mask = compact_mode ? 2 * sizeof(void *) - 1 : (8u << p2) - 1;
            ASSERT(((uintptr_t)node & mask) == 0);
            nodes[i] = node;
            if (tausrand(taus_state) % 10 == 0) {
                for (int k = 0; k < tausrand(taus_state) % 10; k++) {
                    int j = tausrand(taus_state) % (i + 1);
                    if (nodes[j] != NULL) {
                        mrx_free_node_(mrx, nodes[j], p2);
                        nodes[j] = NULL;
                    }
                }
            }
        }
        for (int i = 0; i < alloc_test_size; i++) {
            if (nodes[i] != NULL && compact_mode) {
                mrx_free_node_(mrx, nodes[i], p2s[i]);
            }
        }
    }

}

static void
mrx_allocator_tests(void)
{
    fprintf(stderr, "Test: mrx_allocator functions...");
    {
        mrx_base_t *mrx;

        // test compact mode
        mrx = malloc(sizeof(*mrx));
        memset(mrx, 0xFEu, sizeof(*mrx));

        mrx_init_(mrx, 1000, true);

        ASSERT(mrx->root == NULL);
        ASSERT(mrx->count == 0);
        ASSERT(mrx->capacity == 1000);
        ASSERT((mrx->max_keylen_n_flags & MRX_FLAG_IS_COMPACT_) != 0);

        mrx->root = mrx_alloc_node_(mrx, MAX_P2);
        memset(mrx->root, 0, 128);
        HDR_SET_NSZ(mrx->root->hdr, MRX_NODE_MASK_NODE_NSZ_VALUE_);
        mrx->count = 1000;
        mrx->max_keylen_n_flags = 1000u | (mrx->max_keylen_n_flags & MRX_FLAGS_MASK_);

        mrx_clear_(mrx);

        ASSERT(mrx->root == NULL);
        ASSERT(mrx->count == 0);
        ASSERT(mrx->capacity == 1000);
        ASSERT(mrx->max_keylen_n_flags == MRX_FLAG_IS_COMPACT_);

        mrx_alloc_free_tests(mrx);

        struct mrx_debug_allocator_stats *stats = malloc(sizeof(*stats));
        mrx_alloc_debug_stats(mrx, stats);
        struct mrx_debug_allocator_stats cmp_stats = {0};
        // in compact mode the stats should just be zero
        ASSERT(memcmp(&cmp_stats, stats, sizeof(*stats)) == 0);
        free(stats);

        mrx_delete_(mrx);
        free(mrx);

        // performance mode
        mrx = malloc(sizeof(*mrx) + sizeof(struct mrx_buddyalloc));
        memset(mrx, 0xFEu, sizeof(*mrx) + sizeof(struct mrx_buddyalloc));

        mrx_init_(mrx, 1000, false);

        ASSERT(mrx->root == NULL);
        ASSERT(mrx->count == 0);
        ASSERT(mrx->capacity == 1000);
        ASSERT(mrx->max_keylen_n_flags == 0);
        ASSERT(mrx->nodealloc->nonempty_freelists == 0);

        mrx->root = (void *)0x1;
        mrx->count = 1000;
        mrx->max_keylen_n_flags = 1000 | (mrx->max_keylen_n_flags & MRX_FLAGS_MASK_);

        mrx_clear_(mrx);

        ASSERT(mrx->root == NULL);
        ASSERT(mrx->count == 0);
        ASSERT(mrx->capacity == 1000);
        ASSERT((mrx->max_keylen_n_flags & MRX_FLAG_IS_COMPACT_) == 0);
        ASSERT(mrx->nodealloc->nonempty_freelists == 0);

        mrx_alloc_free_tests(mrx);

        stats = malloc(sizeof(*stats));
        mrx_alloc_debug_stats(mrx, stats);
        free(stats);

        mrx_delete_(mrx);
        free(mrx);
    }
    fprintf(stderr, "pass\n");
}

static void
mrx_complementary_tests(void)
{
    fprintf(stderr, "Test: complementary tests to complete mrx_tmpl.h coverage...");
    {
        // mrx_delete on null pointer, erase empty etc
        mrx_delete(NULL);
        mrx_t *tt = mrx_new(~0);
        ASSERT(mrx_erasent(tt, "miss") == NULL);
        ASSERT(mrx_erase(tt, "miss", 4) == NULL);
        int mlen = 1;
        ASSERT(mrx_findnearnt(tt, "miss", &mlen) == NULL);
        ASSERT(mlen == 0);
        ASSERT(mrx_findnearnt(tt, "miss", NULL) == NULL);
        ASSERT(mrx_begin(tt) == NULL);
        ASSERT(mrx_beginst(tt, alloca(mrx_itsize(tt))) == NULL);
        mrx_clear(tt);
        mrx_delete(tt);
        mrx_itdelete(NULL);
    }
    fprintf(stderr, "pass\n");
}

#if TRACKMEM_DEBUG - 0 != 0
extern trackmem_t *buddyalloc_tm;
trackmem_t *buddyalloc_tm;
trackmem_t *nodepool_tm;
#endif

int
main(void)
{
#if TRACKMEM_DEBUG - 0 != 0
    buddyalloc_tm = trackmem_new();
    nodepool_tm = trackmem_new();
#endif
#if __has_builtin(__builtin_cpu_supports)
    glob.sse42_supported = __builtin_cpu_supports("sse4.2") != 0;
    glob.sse2_supported = __builtin_cpu_supports("sse2") != 0;
#if BUILD_AVX2 - 0 != 0
    glob.avx2_supported = __builtin_cpu_supports("avx2") != 0;
#endif
#endif
#if ARCH_X86 - 0 != 0
    // at the time of writing we haven't bothered to make an unaligned version for 32 bit mode
    glob.sse42_supported = false;
#endif
    tausrand_init(taus_state, 0);

    mrx_scan_tests();
    mrx_allocator_tests();
    mrx_random_tree_tests();
    mrx_complementary_tests();

#if TRACKMEM_DEBUG - 0 != 0
    trackmem_delete(nodepool_tm);
    trackmem_delete(buddyalloc_tm);
#endif
    return 0;
}
