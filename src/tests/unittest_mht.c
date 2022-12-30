/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <unittest_helpers.h>

#include <mrb_tmpl.h>
#include <mht_tmpl.h>
#include <mv_tmpl.h>

#define MC_PREFIX mrba
#define MC_KEY_T const char *
#define MC_VALUE_T char *
#define MC_COPY_KEY(dest, src) dest = strdup(src)
#define MC_FREE_KEY(key) free(MC_DECONST(void *, key))
#define MC_COPY_VALUE(dest, src) dest = strdup(src)
#define MC_FREE_VALUE(value) free(value)
#define MRB_KEYCMP(result, key1, key2) result = strcmp(key1, key2)
#include <mrb_tmpl.h>

static inline void
mhta_hashfunc(uint32_t *hash, const char *key)
{
    *hash = strlen(key); // bad hash, but should work with that too
}

static inline int
mhta_keyequal(const char *key1, const char *key2)
{
    if (key1 == NULL || key2 == NULL) {
        return key1 == key2;
    }
    return strcmp(key1, key2) == 0;
}
#define MC_PREFIX mhta
#define MC_KEY_T const char *
#define MC_COPY_KEY(dest, src) dest = strdup(src)
#define MC_FREE_KEY(key) free(MC_DECONST(void *, key))
#define MC_KEY_UNDEFINED NULL
#define MC_KEY_DIFFERENT_FROM_UNDEFINED ""
#define MC_VALUE_T char *
#define MC_COPY_VALUE(dest, src) dest = strdup(src)
#define MC_FREE_VALUE(value) free(value)
#define MHT_KEYEQUAL mhta_keyequal
#define MHT_HASHFUNC mhta_hashfunc
#include <mht_tmpl.h>

#define MC_PREFIX mhts
#define MC_KEY_T intptr_t
#define MC_KEY_UNDEFINED (-1)
#define MC_KEY_DIFFERENT_FROM_UNDEFINED 0
#define MC_NO_VALUE 1
#include <mht_tmpl.h>

static uint32_t taus_state[3];

static void
mht_basic_tests(void)
{
    fprintf(stderr, "Test: basic tests of all mht functions with default configuration...");
    {
        const int test_size = 10000;
        mht_t *tt = mht_new(test_size);
        mrb_t *kv = mrb_new(test_size);
        mv_t *erased_keys = mv_new(~0, 16);

        ASSERT(mht_empty(tt));
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 3;
            uintptr_t key = tausrand(taus_state) % test_size;
            if (mht_size(tt) > 0 && tausrand(taus_state) % 4 == 0) {
                // make colliding key.
                key = mrb_key(mrb_begin(kv));
                uint32_t pos;
                uint32_t table_size = mht_capacity_to_table_size_(test_size);
                MHT_HASHFUNC_U32(&pos, key);
                pos &= table_size - 1;
                for (uintptr_t alt_key = 0; alt_key < INT_MAX; alt_key++) {
                    uint32_t alt_pos;
                    MHT_HASHFUNC_U32(&alt_pos, alt_key);
                    alt_pos &= table_size - 1;
                    if (alt_pos == pos && alt_key != key) {
                        key = alt_key;
                        break;
                    }
                }
            }
            if (operation == 0) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mht_insert(tt, key, value);
                mrb_insert(kv, key, value);
            } else if (operation == 1) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mht_it_t *it = mht_itinsert(tt, key, value);
                ASSERT(it != mht_end(tt));
                mrb_insert(kv, key, value);
                ASSERT(mht_key(it) == key);
                ASSERT(*mht_keyp(it) == key);
                ASSERT(mht_val(it) == value);
            } else if (operation == 2) {
                mht_it_t *it = mht_itfind(tt, key);
                mrb_erase(kv, key);
                mv_push_back(erased_keys, (void *)key);
                if (it != mht_end(tt) && (tausrand(taus_state) % 2) == 0) {
                    mht_iterase(tt, it);
                } else {
                    void *value = mht_erase(tt, key);
                    mrb_it_t *it1 = mrb_itfind(kv, key);
                    if (it1 != NULL) {
                        ASSERT(value == mrb_val(it1));
                    }
                }
            }
            size_t size = mrb_size(kv);
            for (mrb_it_t *it = mrb_begin(kv); it != mrb_end(); it = mrb_next(it)) {
                uintptr_t k = mrb_key(it);
                ASSERT(mht_val(mht_itfind(tt, k)) == mrb_val(it));
                ASSERT(mht_find(tt, k) == mrb_val(it));
            }
            size_t size2 = 0;
            for (mht_it_t *it = mht_begin(tt); it != mht_end(tt); it = mht_next(it)) {
                ASSERT(mht_val(it) == mrb_find(kv, mht_key(it)));
                size2++;
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mht_setval(it, value);
                mrb_insert(kv, mht_key(it), value);
                ASSERT(mht_val(it) == value);
            }
            ASSERT(mht_size(tt) == size);
            ASSERT(size2 == size);
            ASSERT(mht_empty(tt) == (size == 0));
            for (mv_it_t *it = mv_begin(erased_keys); it != mv_end(erased_keys); it = mv_next(it)) {
                uintptr_t k = (uintptr_t)mv_val(it);
                if (mrb_itfind(kv, k) == NULL) {
                    ASSERT(mht_itfind(tt, k) == mht_end(tt));
                    ASSERT(mht_find(tt, k) == NULL);
                }
            }
        }
        mrb_delete(kv);
        mv_delete(erased_keys);

        while (mht_size(tt) < test_size) {
            void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
            uintptr_t key = tausrand(taus_state);
            mht_insert(tt, key, value);
        }
        {
            void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
            uintptr_t key = tausrand(taus_state);
            ASSERT(mht_insert(tt, key, value) == NULL);
            ASSERT(mht_itinsert(tt, key, value) == mht_end(tt));
        }

        ASSERT(mht_size(tt) > 0);
        mht_clear(tt);
        ASSERT(mht_size(tt) == 0);
        ASSERT(mht_empty(tt));
        ASSERT(mht_begin(tt) == mht_end(tt));

        // delete empty container
        mht_delete(tt);
        mht_delete(NULL);

        // delete container with elements
        tt = mht_new(5000);
        for (int i = 0; i < 1000; i++) {
            uintptr_t key = tausrand(taus_state);
            mht_insert(tt, key, NULL);
        }
        mht_delete(tt);
        mht_delete(NULL);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: basic tests of all mht functions with no value (set) configuration...");
    {
        const int test_size = 10000;
        mhts_t *tt = mhts_new(test_size);
        mrb_t *kv = mrb_new(test_size);
        mv_t *erased_keys = mv_new(~0, 16);

        ASSERT(mhts_empty(tt));
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 3;
            uintptr_t key = tausrand(taus_state) % test_size;
            if (mhts_size(tt) > 0 && tausrand(taus_state) % 4 == 0) {
                // make colliding key.
                key = mrb_key(mrb_begin(kv));
                uint32_t pos;
                uint32_t table_size = mhts_capacity_to_table_size_(test_size);
                MHT_HASHFUNC_U32(&pos, key);
                pos &= table_size - 1;
                for (uintptr_t alt_key = 0; alt_key < INT_MAX; alt_key++) {
                    uint32_t alt_pos;
                    MHT_HASHFUNC_U32(&alt_pos, alt_key);
                    alt_pos &= table_size - 1;
                    if (alt_pos == pos && alt_key != key) {
                        key = alt_key;
                        break;
                    }
                }
            }
            if (operation == 0) {
                mhts_insert(tt, key);
                mrb_insert(kv, key, NULL);
            } else if (operation == 1) {
                mhts_it_t *it = mhts_itinsert(tt, key);
                ASSERT(it != mhts_end(tt));
                mrb_insert(kv, key, NULL);
                ASSERT(mhts_key(it) == key);
                ASSERT(*mhts_keyp(it) == key);
            } else if (operation == 2) {
                mhts_it_t *it = mhts_itfind(tt, key);
                mrb_erase(kv, key);
                mv_push_back(erased_keys, (void *)key);
                if (it != mhts_end(tt) && (tausrand(taus_state) % 2) == 0) {
                    mhts_iterase(tt, it);
                } else {
                    intptr_t k = mhts_erase(tt, key);
                    mrb_it_t *it1 = mrb_itfind(kv, key);
                    if (it1 != NULL) {
                        ASSERT(k == mrb_key(it1));
                    }
                }
            }
            size_t size = mrb_size(kv);
            for (mrb_it_t *it = mrb_begin(kv); it != mrb_end(); it = mrb_next(it)) {
                uintptr_t k = mrb_key(it);
                ASSERT(mhts_val(mhts_itfind(tt, k)) == mrb_key(it));
                ASSERT(mhts_find(tt, k) == mrb_key(it));
            }
            size_t size2 = 0;
            for (mhts_it_t *it = mhts_begin(tt); it != mhts_end(tt); it = mhts_next(it)) {
                ASSERT(mhts_val(it) == mrb_key(mrb_itfind(kv, mhts_key(it))));
                size2++;
            }
            ASSERT(mhts_size(tt) == size);
            ASSERT(size2 == size);
            ASSERT(mhts_empty(tt) == (size == 0));
            for (mv_it_t *it = mv_begin(erased_keys); it != mv_end(erased_keys); it = mv_next(it)) {
                uintptr_t k = (uintptr_t)mv_val(it);
                if (mrb_itfind(kv, k) == NULL) {
                    ASSERT(mhts_itfind(tt, k) == mhts_end(tt));
                    ASSERT(mhts_find(tt, k) == -1);
                }
            }
        }
        mrb_delete(kv);
        mv_delete(erased_keys);

        while (mhts_size(tt) < test_size) {
            uintptr_t key = tausrand(taus_state);
            mhts_insert(tt, key);
        }
        {
            uintptr_t key = tausrand(taus_state);
            ASSERT(mhts_insert(tt, key) == -1);
            ASSERT(mhts_itinsert(tt, key) == mhts_end(tt));
        }

        ASSERT(mhts_size(tt) > 0);
        mhts_clear(tt);
        ASSERT(mhts_size(tt) == 0);
        ASSERT(mhts_empty(tt));
        ASSERT(mhts_begin(tt) == mhts_end(tt));

        // delete empty container
        mhts_delete(tt);
        mhts_delete(NULL);

        // delete container with elements
        tt = mhts_new(5000);
        for (int i = 0; i < 1000; i++) {
            uintptr_t key = tausrand(taus_state);
            mhts_insert(tt, key);
        }
        mhts_delete(tt);
        mhts_delete(NULL);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: basic tests of all mht function for custom config...");
    {
        const int test_size = 1000;
        mhta_t *tt = mhta_new(test_size);
        mrba_t *kv = mrba_new(test_size);
        mv_t *erased_keys = mv_new(~0, 16);

        ASSERT(mhta_empty(tt));
        char skey[64];
        char svalue[64];
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 3;
            uintptr_t key = tausrand(taus_state) % test_size;
            if (mhta_size(tt) > 0 && tausrand(taus_state) % 4 == 0) {
                // make colliding key.
                key = atol(mrba_key(mrba_begin(kv)));
                uint32_t pos;
                uint32_t table_size = mhta_capacity_to_table_size_(test_size);
                sprintf(skey, "%ld", (long)key);
                mhta_hashfunc(&pos, skey);
                pos &= table_size - 1;
                for (uintptr_t alt_key = 0; alt_key < INT_MAX; alt_key++) {
                    uint32_t alt_pos;
                    sprintf(skey, "%ld", (long)alt_key);
                    mhta_hashfunc(&alt_pos, skey);
                    alt_pos &= table_size - 1;
                    if (alt_pos == pos && alt_key != key) {
                        key = alt_key;
                        break;
                    }
                }
            }
            sprintf(skey, "%ld", (long)key);
            if (operation == 0) {
                uintptr_t value = (uintptr_t)(tausrand(taus_state) | 1);
                sprintf(svalue, "%ld", (long)value);
                mhta_insert(tt, skey, svalue);
                mrba_insert(kv, skey, svalue);
            } else if (operation == 1) {
                uintptr_t value = (uintptr_t)(tausrand(taus_state) | 1);
                sprintf(svalue, "%ld", (long)value);
                mhta_it_t *it = mhta_itinsert(tt, skey, svalue);
                ASSERT(it != mhta_end(tt));
                mrba_insert(kv, skey, svalue);
                ASSERT(strcmp(mhta_key(it), skey) == 0);
                ASSERT(strcmp(*mhta_keyp(it), skey) == 0);
                ASSERT(strcmp(mhta_val(it), svalue) == 0);
            } else if (operation == 2) {
                mhta_it_t *it = mhta_itfind(tt, skey);
                mrba_erase(kv, skey);
                mv_push_back(erased_keys, strdup(skey));
                if (it != mhta_end(tt) && (tausrand(taus_state) % 2) == 0) {
                    mhta_iterase(tt, it);
                } else {
                    mhta_erase(tt, skey);
                }
            }
            size_t size = mrba_size(kv);
            for (mrba_it_t *it = mrba_begin(kv); it != mrba_end(); it = mrba_next(it)) {
                const char *k = mrba_key(it);
                mhta_it_t *it1 = mhta_itfind(tt, k);
                ASSERT(strcmp(mhta_val(it1), mrba_val(it)) == 0);
                ASSERT(strcmp(mhta_find(tt, k), mrba_val(it)) == 0);
            }
            size_t size2 = 0;
            for (mhta_it_t *it = mhta_begin(tt); it != mhta_end(tt); it = mhta_next(it)) {
                ASSERT(strcmp(mhta_val(it), mrba_find(kv, mhta_key(it))) == 0);
                size2++;
                uintptr_t value = (uintptr_t)(tausrand(taus_state) | 1);
                sprintf(svalue, "%ld", (long)value);
                mhta_setval(it, svalue);
                mrba_insert(kv, mhta_key(it), svalue);
                ASSERT(strcmp(mhta_val(it), svalue) == 0);
            }
            ASSERT(mhta_size(tt) == size);
            ASSERT(size2 == size);
            ASSERT(mhta_empty(tt) == (size == 0));
            for (mv_it_t *it = mv_begin(erased_keys); it != mv_end(erased_keys); it = mv_next(it)) {
                const char *k = mv_val(it);
                if (mrba_itfind(kv, k) == NULL) {
                    ASSERT(mhta_itfind(tt, k) == mhta_end(tt));
                    ASSERT(mhta_find(tt, k) == NULL);
                }
            }
        }
        mrba_delete(kv);
        for (mv_it_t *it = mv_begin(erased_keys); it != mv_end(erased_keys); it = mv_next(it)) {
            free(mv_val(it));
        }
        mv_delete(erased_keys);

        while (mhta_size(tt) < test_size) {
            uintptr_t value = (uintptr_t)(tausrand(taus_state) | 1);
            sprintf(svalue, "%ld", (long)value);
            uintptr_t key = tausrand(taus_state);
            sprintf(skey, "%ld", (long)key);
            mhta_insert(tt, skey, svalue);
        }
        {
            uintptr_t value = (uintptr_t)(tausrand(taus_state) | 1);
            sprintf(svalue, "%ld", (long)value);
            uintptr_t key = tausrand(taus_state);
            sprintf(skey, "%ld", (long)key);
            ASSERT(mhta_insert(tt, skey, svalue) == NULL);
            ASSERT(mhta_itinsert(tt, skey, svalue) == mhta_end(tt));
        }

        ASSERT(mhta_size(tt) > 0);
        mhta_clear(tt);
        ASSERT(mhta_size(tt) == 0);
        ASSERT(mhta_empty(tt));
        ASSERT(mhta_begin(tt) == mhta_end(tt));

        // delete empty container
        mhta_delete(tt);

        // delete container with elements
        tt = mhta_new(5000);
        for (int i = 0; i < 1000; i++) {
            uintptr_t key = tausrand(taus_state);
            sprintf(skey, "%ld", (long)key);
            mhta_insert(tt, skey, skey);
        }
        mhta_delete(tt);
        mhta_delete(NULL);
    }
    fprintf(stderr, "pass\n");

}

int
main(void)
{
    tausrand_init(taus_state, 0);
    mht_basic_tests();
    return 0;
}
