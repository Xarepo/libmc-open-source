/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <unittest_helpers.h>

// hack to get access to and test the local functions
#define rotate_nodes TEST_rotate_nodes
#define erase_rebalance TEST_erase_rebalance
#define mrb_insert_node_ TEST_mrb_insert_node_
#define mrb_erase_node_ TEST_mrb_erase_node_
#include <mrb_base.c>

#include <mrb_tmpl.h>

#define MC_MM_MODE MC_MM_PERFORMANCE
#define MC_PREFIX mrbp
#define MC_KEY_T uintptr_t
#define MC_VALUE_T void *
#include <mrb_tmpl.h>

#define MC_MM_MODE MC_MM_STATIC
#define MC_PREFIX mrbs
#define MC_KEY_T uintptr_t
#define MC_VALUE_T void *
#include <mrb_tmpl.h>

#define MRB_PRESET_const_str_TO_REF_COPY_KEY
#include <mrb_tmpl.h>

#define MRB_PRESET_const_str_COPY
#include <mrb_tmpl.h>

#define MC_PREFIX mrbnv
#define MC_NO_VALUE 1
#define MC_VALUE_RETURN_REF 1
#define MC_KEY_T uint32_t
#include <mrb_tmpl.h>

static uint32_t taus_state[3];

static void
mrb_base_tests(void)
{
    fprintf(stderr, "Test: rotate_nodes()...");
    {
        struct mrb_node a = {0}, b = {0}, c = {0}, x = {0}, y = {0}, z = {0};
        struct mrb_node *root = NULL;
        a.child[LEFT] = &b;
        a.child[RIGHT] = &z;
        b.child[LEFT] = &x;
        b.child[RIGHT] = &y;
        c.child[LEFT] = &a;
        parent_set(&a, &c);
        parent_set(&b, &a);
        parent_set(&x, &b);
        parent_set(&y, &b);
        parent_set(&z, &a);

        TEST_rotate_nodes(&root, &a, RIGHT);

        ASSERT(root == NULL);
        ASSERT(parent_get(&a) == &b);
        ASSERT(parent_get(&b) == &c);
        ASSERT(parent_get(&x) == &b);
        ASSERT(parent_get(&y) == &a);
        ASSERT(parent_get(&z) == &a);
        ASSERT(a.child[LEFT] == &y);
        ASSERT(a.child[RIGHT] == &z);
        ASSERT(b.child[LEFT] == &x);
        ASSERT(b.child[RIGHT] == &a);
        ASSERT(c.child[LEFT] == &b);
        ASSERT(c.child[RIGHT] == NULL);

        TEST_rotate_nodes(&root, &b, LEFT);

        ASSERT(root == NULL);
        ASSERT(parent_get(&a) == &c);
        ASSERT(parent_get(&b) == &a);
        ASSERT(parent_get(&x) == &b);
        ASSERT(parent_get(&y) == &b);
        ASSERT(parent_get(&z) == &a);
        ASSERT(a.child[LEFT] == &b);
        ASSERT(a.child[RIGHT] == &z);
        ASSERT(b.child[LEFT] == &x);
        ASSERT(b.child[RIGHT] == &y);
        ASSERT(c.child[LEFT] == &a);
        ASSERT(c.child[RIGHT] == NULL);

        a.child[LEFT] = &b;
        a.child[RIGHT] = NULL;
        b.child[LEFT] = NULL;
        b.child[RIGHT] = NULL;
        parent_set(&a, NULL);
        parent_set(&b, &a);

        TEST_rotate_nodes(&root, &a, RIGHT);

        ASSERT(root == &b);
        ASSERT(parent_get(&a) == &b);
        ASSERT(parent_get(&b) == NULL);
        ASSERT(a.child[LEFT] == NULL);
        ASSERT(a.child[RIGHT] == NULL);
        ASSERT(b.child[LEFT] == NULL);
        ASSERT(b.child[RIGHT] == &a);

        TEST_rotate_nodes(&root, &b, LEFT);

        ASSERT(root == &a);
        ASSERT(parent_get(&a) == NULL);
        ASSERT(parent_get(&b) == &a);
        ASSERT(a.child[LEFT] == &b);
        ASSERT(a.child[RIGHT] == NULL);
        ASSERT(b.child[LEFT] == NULL);
        ASSERT(b.child[RIGHT] == NULL);
    }
    fprintf(stderr, "pass\n");

    // No specific tests written, coverage is achieved in mrb_basic_tests()...
    /*
    fprintf(stderr, "Test: erase_rebalance()...");
    {
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: mrb_insert_node_()...");
    {
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: mrb_erase_node_()...");
    {
    }
    fprintf(stderr, "pass\n");
    */
}

static void
mrb_basic_tests(void)
{
    fprintf(stderr, "Test: basic tests of all mrb functions with default configuration...");
    {
        const int test_size = 5000;
        mrb_t *tt = mrb_new(~0);
        void **kv = calloc(test_size, sizeof(*kv));

        ASSERT(mrb_empty(tt));
        int find_near_test_count = 0;
        for (int i = 0; i < 5000; i++) {
            int operation = tausrand(taus_state) % 3;
            uintptr_t key = tausrand(taus_state) % test_size;
            if (operation == 0) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mrb_insert(tt, key, value);
                kv[key] = value;
            } else if (operation == 1) {
                mrb_it_t *it = mrb_itfindnear(tt, key);
                if (it == mrb_end()) {
                    ASSERT(mrb_empty(tt));
                } else {
                    ASSERT(mrb_val(it) != NULL && mrb_val(it) == kv[mrb_key(it)]);
                    if (mrb_key(it) != key) {
                        uintptr_t k;
                        for (k = key; k < test_size; k++) {
                            if (kv[k] != NULL) {
                                break;
                            }
                        }
                        if (mrb_key(it) != k) {
                            for (k = key; k >= 0 && k < test_size; k--) {
                                if (kv[k] != NULL) {
                                    break;
                                }
                            }
                            ASSERT(mrb_key(it) == k);
                        }
                        find_near_test_count++;
                    }
                    void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                    it = mrb_itinsert(tt, key, value);
                    kv[key] = value;
                    ASSERT(mrb_key(it) == key);
                    ASSERT(*mrb_keyp(it) == key);
                    ASSERT(mrb_val(it) == value);
                }
            } else if (operation == 2) {
                mrb_it_t *it = mrb_itfind(tt, key);
                if (it != mrb_end() && (tausrand(taus_state) % 2) == 0) {
                    mrb_it_t *next_it = mrb_next(it);
                    mrb_it_t *next_it1 = mrb_iterase(tt, it);
                    ASSERT(next_it1 == next_it);
                } else {
                    mrb_erase(tt, key);
                }
                kv[key] = NULL;
            }
            size_t size = 0;
            for (uintptr_t k = 0; k < test_size; k++) {
                if (kv[k] != NULL) {
                    size++;
                    ASSERT(mrb_val(mrb_itfind(tt, k)) == kv[k]);
                } else {
                    ASSERT(mrb_itfind(tt, k) == mrb_end());
                }
                ASSERT(mrb_find(tt, k) == kv[k]);
            }
            intptr_t prev_key = -1;
            size_t size2 = 0;
            for (mrb_it_t *it = mrb_begin(tt); it != mrb_end(); it = mrb_next(it)) {
                ASSERT((intptr_t)mrb_key(it) > prev_key);
                ASSERT(mrb_val(it) == kv[mrb_key(it)]);
                prev_key = mrb_key(it);
                size2++;
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mrb_setval(it, value);
                kv[mrb_key(it)] = value;
                ASSERT(mrb_val(it) == value);
            }
            ASSERT(mrb_size(tt) == size);
            ASSERT(size2 == size);
            ASSERT(mrb_empty(tt) == (size == 0));

            size2 = 0;
            prev_key = test_size;
            for (mrb_it_t *it = mrb_rbegin(tt); it != mrb_rend(); it = mrb_prev(it)) {
                ASSERT((intptr_t)mrb_key(it) < prev_key);
                ASSERT(mrb_val(it) == kv[mrb_key(it)]);
                prev_key = mrb_key(it);
                size2++;
            }
            ASSERT(size2 == size);
        }
        free(kv);
        ASSERT(find_near_test_count > 0);

        ASSERT(mrb_size(tt) > 0);
        mrb_clear(tt);
        ASSERT(mrb_size(tt) == 0);
        ASSERT(mrb_empty(tt));
        ASSERT(mrb_begin(tt) == NULL);
        ASSERT(mrb_rbegin(tt) == NULL);

        // delete empty container
        mrb_delete(tt);
        mrb_delete(NULL);

        // delete container with elements
        tt = mrb_new(~0);
        for (int i = 0; i < 1000; i++) {
            uintptr_t key = tausrand(taus_state);
            mrb_insert(tt, key, NULL);
        }
        mrb_delete(tt);

    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: basic tests of all mrb functions with performance memory management...");
    // NOTE: exact copy paste of first test case, except mrb_* => mrbp_*
    {
        const int test_size = 5000;
        mrbp_t *tt = mrbp_new(~0);
        void **kv = calloc(test_size, sizeof(*kv));

        ASSERT(mrbp_empty(tt));
        int find_near_test_count = 0;
        for (int i = 0; i < 5000; i++) {
            int operation = tausrand(taus_state) % 3;
            uintptr_t key = tausrand(taus_state) % test_size;
            if (operation == 0) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mrbp_insert(tt, key, value);
                kv[key] = value;
            } else if (operation == 1) {
                mrbp_it_t *it = mrbp_itfindnear(tt, key);
                if (it == mrbp_end()) {
                    ASSERT(mrbp_empty(tt));
                } else {
                    ASSERT(mrbp_val(it) != NULL && mrbp_val(it) == kv[mrbp_key(it)]);
                    if (mrbp_key(it) != key) {
                        uintptr_t k;
                        for (k = key; k < test_size; k++) {
                            if (kv[k] != NULL) {
                                break;
                            }
                        }
                        if (mrbp_key(it) != k) {
                            for (k = key; k >= 0 && k < test_size; k--) {
                                if (kv[k] != NULL) {
                                    break;
                                }
                            }
                            ASSERT(mrbp_key(it) == k);
                        }
                        find_near_test_count++;
                    }
                    void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                    it = mrbp_itinsert(tt, key, value);
                    kv[key] = value;
                    ASSERT(mrbp_key(it) == key);
                    ASSERT(*mrbp_keyp(it) == key);
                    ASSERT(mrbp_val(it) == value);
                }
            } else if (operation == 2) {
                mrbp_it_t *it = mrbp_itfind(tt, key);
                if (it != mrbp_end() && (tausrand(taus_state) % 2) == 0) {
                    mrbp_it_t *next_it = mrbp_next(it);
                    mrbp_it_t *next_it1 = mrbp_iterase(tt, it);
                    ASSERT(next_it1 == next_it);
                } else {
                    mrbp_erase(tt, key);
                }
                kv[key] = NULL;
            }
            size_t size = 0;
            for (uintptr_t k = 0; k < test_size; k++) {
                if (kv[k] != NULL) {
                    size++;
                    ASSERT(mrbp_val(mrbp_itfind(tt, k)) == kv[k]);
                } else {
                    ASSERT(mrbp_itfind(tt, k) == mrbp_end());
                }
                ASSERT(mrbp_find(tt, k) == kv[k]);
            }
            intptr_t prev_key = -1;
            size_t size2 = 0;
            for (mrbp_it_t *it = mrbp_begin(tt); it != mrbp_end(); it = mrbp_next(it)) {
                ASSERT((intptr_t)mrbp_key(it) > prev_key);
                ASSERT(mrbp_val(it) == kv[mrbp_key(it)]);
                prev_key = mrbp_key(it);
                size2++;
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mrbp_setval(it, value);
                kv[mrbp_key(it)] = value;
                ASSERT(mrbp_val(it) == value);
            }
            ASSERT(mrbp_size(tt) == size);
            ASSERT(size2 == size);
            ASSERT(mrbp_empty(tt) == (size == 0));

            size2 = 0;
            prev_key = test_size;
            for (mrbp_it_t *it = mrbp_rbegin(tt); it != mrbp_rend(); it = mrbp_prev(it)) {
                ASSERT((intptr_t)mrbp_key(it) < prev_key);
                ASSERT(mrbp_val(it) == kv[mrbp_key(it)]);
                prev_key = mrbp_key(it);
                size2++;
            }
            ASSERT(size2 == size);
        }
        free(kv);
        ASSERT(find_near_test_count > 0);

        ASSERT(mrbp_size(tt) > 0);
        mrbp_clear(tt);
        ASSERT(mrbp_size(tt) == 0);
        ASSERT(mrbp_empty(tt));
        ASSERT(mrbp_begin(tt) == NULL);
        ASSERT(mrbp_rbegin(tt) == NULL);

        // delete empty container
        mrbp_delete(tt);

        // delete container with elements
        tt = mrbp_new(~0);
        for (int i = 0; i < 1000; i++) {
            uintptr_t key = tausrand(taus_state);
            mrbp_insert(tt, key, NULL);
        }
        mrbp_delete(tt);

    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: basic tests of all mrb functions with static memory management...");
    // NOTE: exact copy paste of first test case, except mrb_* => mrbs_* plus slight init difference
    {
        const int test_size = 5000;
        void **kv = calloc(test_size, sizeof(*kv));

         // mrbs_ specific init
        mrbs_t tt_;
        struct mrbs_node_kv tt_nodes[test_size];
        mrbs_t *tt = mrbs_init(&tt_, tt_nodes, sizeof(tt_nodes));

        ASSERT(mrbs_empty(tt));
        int find_near_test_count = 0;
        for (int i = 0; i < 5000; i++) {
            int operation = tausrand(taus_state) % 3;
            uintptr_t key = tausrand(taus_state) % test_size;
            if (operation == 0) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mrbs_insert(tt, key, value);
                kv[key] = value;
            } else if (operation == 1) {
                mrbs_it_t *it = mrbs_itfindnear(tt, key);
                if (it == mrbs_end()) {
                    ASSERT(mrbs_empty(tt));
                } else {
                    ASSERT(mrbs_val(it) != NULL && mrbs_val(it) == kv[mrbs_key(it)]);
                    if (mrbs_key(it) != key) {
                        uintptr_t k;
                        for (k = key; k < test_size; k++) {
                            if (kv[k] != NULL) {
                                break;
                            }
                        }
                        if (mrbs_key(it) != k) {
                            for (k = key; k >= 0 && k < test_size; k--) {
                                if (kv[k] != NULL) {
                                    break;
                                }
                            }
                            ASSERT(mrbs_key(it) == k);
                        }
                        find_near_test_count++;
                    }
                    void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                    it = mrbs_itinsert(tt, key, value);
                    kv[key] = value;
                    ASSERT(mrbs_key(it) == key);
                    ASSERT(*mrbs_keyp(it) == key);
                    ASSERT(mrbs_val(it) == value);
                }
            } else if (operation == 2) {
                mrbs_it_t *it = mrbs_itfind(tt, key);
                if (it != mrbs_end() && (tausrand(taus_state) % 2) == 0) {
                    mrbs_it_t *next_it = mrbs_next(it);
                    mrbs_it_t *next_it1 = mrbs_iterase(tt, it);
                    ASSERT(next_it1 == next_it);
                } else {
                    mrbs_erase(tt, key);
                }
                kv[key] = NULL;
            }
            size_t size = 0;
            for (uintptr_t k = 0; k < test_size; k++) {
                if (kv[k] != NULL) {
                    size++;
                    ASSERT(mrbs_val(mrbs_itfind(tt, k)) == kv[k]);
                } else {
                    ASSERT(mrbs_itfind(tt, k) == mrbs_end());
                }
                ASSERT(mrbs_find(tt, k) == kv[k]);
            }
            intptr_t prev_key = -1;
            size_t size2 = 0;
            for (mrbs_it_t *it = mrbs_begin(tt); it != mrbs_end(); it = mrbs_next(it)) {
                ASSERT((intptr_t)mrbs_key(it) > prev_key);
                ASSERT(mrbs_val(it) == kv[mrbs_key(it)]);
                prev_key = mrbs_key(it);
                size2++;
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mrbs_setval(it, value);
                kv[mrbs_key(it)] = value;
                ASSERT(mrbs_val(it) == value);
            }
            ASSERT(mrbs_size(tt) == size);
            ASSERT(size2 == size);
            ASSERT(mrbs_empty(tt) == (size == 0));

            size2 = 0;
            prev_key = test_size;
            for (mrbs_it_t *it = mrbs_rbegin(tt); it != mrbs_rend(); it = mrbs_prev(it)) {
                ASSERT((intptr_t)mrbs_key(it) < prev_key);
                ASSERT(mrbs_val(it) == kv[mrbs_key(it)]);
                prev_key = mrbs_key(it);
                size2++;
            }
            ASSERT(size2 == size);
        }
        free(kv);
        ASSERT(find_near_test_count > 0);

        ASSERT(mrbs_size(tt) > 0);
        mrbs_clear(tt);
        ASSERT(mrbs_size(tt) == 0);
        ASSERT(mrbs_empty(tt));
        ASSERT(mrbs_begin(tt) == NULL);
        ASSERT(mrbs_rbegin(tt) == NULL);
    }
    fprintf(stderr, "pass\n");
}

static void
mrb_alt_configs(void)
{
    fprintf(stderr, "Test: mrb functions with alternate configurations...");
    {
        // just call testing, satisfied with verification made in default config
        mrbnv_t *tt = mrbnv_new(100);
        // note: doesn't really make sense to do this, as we change the key here,
        // the config is intended for key/value not sets
        uint32_t k = tausrand(taus_state);
        *mrbnv_val(mrbnv_itinsert(tt, k)) = k;
        *mrbnv_insert(tt, k) = k;
        ASSERT(mrbnv_erase(tt, k) != NULL);
        mrbnv_delete(tt);
    }
    {
        str2ref_t *tt = str2ref_new(~0u);
        ASSERT(str2ref_insert(tt, "abcd", (void *)0x1) == (void *)0x1);
        ASSERT(str2ref_insert(tt, "abcd", (void *)0x1) == (void *)0x1);
        ASSERT(str2ref_erase(tt, "abcd") == (void *)0x1);
        ASSERT(str2ref_val(str2ref_itinsert(tt, "abcd", (void *)0x1)) == (void *)0x1);
        ASSERT(strcmp(str2ref_key(str2ref_itinsert(tt, "abcd", (void *)0x1)), "abcd") == 0);
        ASSERT(str2ref_insert(tt, "abcde", (void *)0x2) == (void *)0x2);
        ASSERT(str2ref_size(tt) == 2);
        ASSERT(str2ref_val(str2ref_iterase(tt, str2ref_itfind(tt, "abcd"))) == (void *)0x2);
        str2ref_delete(tt);
    }
    {
        strset_t *tt = strset_new(~0u);
        ASSERT(strcmp(strset_insert(tt, "abcd"), "abcd") == 0);
        ASSERT(strcmp(strset_insert(tt, "abcd"), "abcd") == 0);
        ASSERT(strcmp(strset_val(strset_itinsert(tt, "abcd")), "abcd") == 0);
        ASSERT(strcmp(strset_val(strset_itinsert(tt, "abcd")), "abcd") == 0);
        ASSERT(strset_erase(tt, "abcd") != NULL);
        strset_insert(tt, "abcd");
        strset_insert(tt, "abcde");
        ASSERT(strset_size(tt) == 2);
        ASSERT(strcmp(strset_val(strset_iterase(tt, strset_itfind(tt, "abcd"))), "abcde") == 0);
        strset_delete(tt);
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
    tausrand_init(taus_state, 0);
    mrb_base_tests();
    mrb_basic_tests();
    mrb_alt_configs();
#if TRACKMEM_DEBUG - 0 != 0
    trackmem_delete(nodepool_tm);
    trackmem_delete(buddyalloc_tm);
#endif
    return 0;
}
