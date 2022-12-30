/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <string.h>

#include <unittest_helpers.h>

#include <mld_tmpl.h>
#include <mq_tmpl.h>

#define MC_PREFIX mqnv
#define MC_VALUE_NO_INSERT_ARG 1
#define MC_VALUE_RETURN_REF 1
#define MC_VALUE_T uint32_t
#include <mq_tmpl.h>

#define MC_PREFIX mqa
#define MC_VALUE_T char *
#define MC_COPY_VALUE(dest, src) dest = strdup(src)
#define MC_FREE_VALUE(value) free(value)
#include <mq_tmpl.h>

static uint32_t taus_state[3];

static void
mq_basic_tests(void)
{
    fprintf(stderr, "Test: basic tests of all mq functions with default configuration...");
    {
        const int test_size = 1000;
        mq_t *tt = mq_new(test_size);
        mld_t *ts = mld_new(test_size);

        ASSERT(mq_front(tt) == NULL);
        ASSERT(mq_back(tt) == NULL);
        ASSERT(mq_pop_front(tt) == NULL);
        ASSERT(mq_pop_back(tt) == NULL);
        ASSERT(mq_empty(tt));
        ASSERT(mq_size(tt) == 0);
        ASSERT(mq_max_size(tt) == test_size);

        for (int i = 0; i < test_size; i++) {
            void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
            int operation = tausrand(taus_state) % 5;
            if (operation == 0 || operation == 4) {
                ASSERT(mq_push_front(tt, value) == value);
                mld_push_front(ts, value);
            } else if (operation == 1) {
                ASSERT(mq_push_back(tt, value) == value);
                mld_push_back(ts, value);
            } else if (operation == 2 && !mq_empty(tt)) {
                ASSERT(mq_pop_front(tt) == mld_pop_front(ts));
            } else if (operation == 3 && !mq_empty(tt)) {
                ASSERT(mq_pop_back(tt) == mld_pop_back(ts));
            }
            mld_it_t *it1 = mld_begin(ts);
            for (mq_it_t *it = mq_begin(tt); it != mq_end(tt); it = mq_next(it)) {
                ASSERT(mq_val(tt, it) == mld_val(it1));
                it1 = mld_next(it1);
                void *val = mq_val(tt, it);
                mq_setval(tt, it, (void *)0x1);
                ASSERT(mq_val(tt, it) == (void *)0x1);
                mq_setval(tt, it, val);
            }
            ASSERT(mq_front(tt) == mld_front(ts));
            ASSERT(mq_back(tt) == mld_back(ts));
            ASSERT(it1 == mld_end());
            it1 = mld_rbegin(ts);
            for (mq_it_t *it = mq_rbegin(tt); it != mq_rend(tt); it = mq_prev(it)) {
                ASSERT(mq_val(tt, it) == mld_val(it1));
                it1 = mld_prev(it1);
            }
            ASSERT(it1 == mld_end());
            ASSERT(mq_size(tt) == mld_size(ts));
        }

        mq_clear(tt);

        for (int i = 0; i < mq_max_size(tt); i++) {
            void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
            ASSERT(mq_push_front(tt, value) == value);
        }
        ASSERT(mq_push_front(tt, (void *)0x1) == NULL);
        ASSERT(mq_push_back(tt, (void *)0x1) == NULL);
        ASSERT(mq_size(tt) == mq_max_size(tt));

        mq_delete(tt);
        mq_delete(NULL);
        mld_delete(ts);
    }
    fprintf(stderr, "pass\n");
}

static void
mq_alt_configs(void)
{
    fprintf(stderr, "Test: mq functions with alternate configurations...");
    {
        // just call testing, satisfied with verification made in default config
        const int test_size = 5000;
        mqnv_t *tt = mqnv_new(test_size);
        *mqnv_push_front(tt) = tausrand(taus_state);
        *mqnv_push_back(tt) = tausrand(taus_state);
        *mqnv_push_back(tt) = tausrand(taus_state);
        *mqnv_push_back(tt) = tausrand(taus_state);
        uint32_t dummy = *mqnv_pop_back(tt) + *mqnv_pop_front(tt);
        *mqnv_push_back(tt) = dummy;
        dummy = *mqnv_back(tt) + *mqnv_front(tt);
        *mqnv_push_back(tt) = dummy;
        *mqnv_front(tt) = dummy;
        *mqnv_back(tt) = dummy;
        *mqnv_val(tt, mqnv_begin(tt)) = tausrand(taus_state);
        *mqnv_push_back(tt) = tausrand(taus_state);
        mqnv_clear(tt);
        *mqnv_push_back(tt) = tausrand(taus_state);
        mqnv_delete(tt);
    }
    {
        mqa_t *tt = mqa_new(5000);
        ASSERT(strcmp(mqa_push_back(tt, "abcd"), "abcd") == 0);
        ASSERT(strcmp(mqa_back(tt), "abcd") == 0);
        ASSERT(mqa_pop_back(tt) != NULL);
        ASSERT(strcmp(mqa_push_front(tt, "abcd"), "abcd") == 0);
        ASSERT(strcmp(mqa_front(tt), "abcd") == 0);
        ASSERT(mqa_pop_front(tt) != NULL);
        mqa_push_back(tt, "abcd");
        ASSERT(strcmp(mqa_setval(tt, mqa_begin(tt), "abcde"), "abcde") == 0);
        ASSERT(strcmp(mqa_back(tt), "abcde") == 0);
        mqa_clear(tt);
        mqa_push_back(tt, "abcd");
        mqa_delete(tt);
    }
    fprintf(stderr, "pass\n");
}

int
main(void)
{
    tausrand_init(taus_state, 0);
    mq_basic_tests();
    mq_alt_configs();
    return 0;
}
