/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <string.h>

#include <unittest_helpers.h>

#include <mls_tmpl.h>

#define MC_MM_MODE MC_MM_PERFORMANCE
#define MC_PREFIX mlsp
#define MC_VALUE_T void *
#include <mls_tmpl.h>

#define MC_MM_MODE MC_MM_STATIC
#define MC_PREFIX mlss
#define MC_VALUE_T void *
#include <mls_tmpl.h>

#define MC_PREFIX mlsc
#define MC_COPY_VALUE(dest, src) dest = malloc(sizeof(int)); *dest = *src;
#define MC_FREE_VALUE(value) free(value);
#define MC_VALUE_T int *
#include <mls_tmpl.h>

#define MC_PREFIX mlsr
#define MC_VALUE_RETURN_REF 1
#define MC_VALUE_T int
#include <mls_tmpl.h>

#define MC_PREFIX mlsa
#define MC_VALUE_NO_INSERT_ARG 1
#define MC_VALUE_RETURN_REF 1
#define MC_VALUE_T int
#include <mls_tmpl.h>

#include <mld_tmpl.h>

#define MC_MM_MODE MC_MM_PERFORMANCE
#define MC_PREFIX mldp
#define MC_VALUE_T void *
#include <mld_tmpl.h>

#define MC_MM_MODE MC_MM_STATIC
#define MC_PREFIX mlds
#define MC_VALUE_T void *
#include <mld_tmpl.h>

#define MC_PREFIX mldc
#define MC_COPY_VALUE(dest, src) dest = malloc(sizeof(int)); *dest = *src;
#define MC_FREE_VALUE(value) free(value);
#define MC_VALUE_T int *
#include <mld_tmpl.h>

#define MC_PREFIX mldr
#define MC_VALUE_RETURN_REF 1
#define MC_VALUE_T int
#include <mld_tmpl.h>

#define MC_PREFIX mlda
#define MC_VALUE_NO_INSERT_ARG 1
#define MC_VALUE_RETURN_REF 1
#define MC_VALUE_T int
#include <mld_tmpl.h>


static uint32_t taus_state[3];

static struct tl_ {
    void **arr;
    int capacity;
    int size;
} tl;

static void
tl_init(size_t capacity)
{
    free(tl.arr);
    tl.arr = calloc(capacity, sizeof(tl.arr[0]));
    tl.capacity = (int)capacity;
    tl.size = 0;

}

static void
tl_deinit(void)
{
    free(tl.arr);
    tl = (struct tl_){0};
}

static void
tl_clear(void)
{
    memset(tl.arr, 0, tl.capacity * sizeof(tl.arr[0]));
    tl.size = 0;
}

// Dumb list implementation used as shadow to test functionality of real lists
static void
tl_insert(int pos, void *value)
{
    tl.size++;
    ASSERT(tl.size <= tl.capacity);
    ASSERT(pos < tl.size);
    memmove(&tl.arr[pos+1], &tl.arr[pos], (tl.size - pos - 1) * sizeof(tl.arr[0]));
    tl.arr[pos] = value;
}

static void
tl_insert_after(int pos, void *value)
{
    tl_insert(pos + 1, value);
}

static void *
tl_erase(int pos)
{
    ASSERT(pos >= 0);
    if (pos >= tl.size) {
        return NULL;
    }
    void *value = tl.arr[pos];
    memmove(&tl.arr[pos], &tl.arr[pos+1], (tl.size - pos - 1) * sizeof(tl.arr[0]));
    tl.size--;
    return value;
}

static void *
tl_erase_after(int pos)
{
    return tl_erase(pos + 1);
}

static void
tl_push_front(void *value)
{
    tl_insert(0, value);
}

static void
tl_push_back(void *value)
{
    tl_insert(tl.size, value);
}

static void *
tl_pop_front(void)
{
    return tl_erase_after(-1);
}

static void *
tl_pop_back(void)
{
    if (tl.size == 0) {
        return NULL;
    }
    void *value = tl.arr[tl.size-1];
    tl.size--;
    return value;
}

static void
tl_to_front(int pos)
{
    if (tl.size == 0) {
        return;
    }
    void *value = tl.arr[pos];
    tl_erase(pos);
    tl_push_front(value);
}

static void
tl_to_back(int pos)
{
    if (tl.size == 0) {
        return;
    }
    void *value = tl.arr[pos];
    tl_erase(pos);
    tl_push_back(value);
}

static void *
tl_front(void)
{
    if (tl.size == 0) {
        return NULL;
    }
    return tl.arr[0];
}

static void *
tl_back(void)
{
    if (tl.size == 0) {
        return NULL;
    }
    return tl.arr[tl.size-1];
}

static int
tl_begin(void)
{
    if (tl.size == 0) {
        return -1;
    }
    return 0;
}
static int
tl_next(int pos) {
    return pos + 1;
}
static int
tl_prev(int pos) {
    return pos - 1;
}
static int
tl_end(void)
{
    if (tl.size == 0) {
        return -1;
    }
    return tl.size;
}
static int
tl_rbegin(void)
{
    return tl.size - 1;
}
static int
tl_rend(void)
{
    return -1;
}
static size_t
tl_size(void)
{
    return tl.size;
}
static void *
tl_val(int pos)
{
    return tl.arr[pos];
}

static void
mls_tests(void)
{
    fprintf(stderr, "Test: basic tests of all mls functions with default configuration...");
    {
        const int test_size = 5000;
        tl_init(test_size);

        mls_t *tt = mls_new(test_size);

        ASSERT(mls_max_size(tt) == test_size);
        ASSERT(mls_size(tt) == 0);
        ASSERT(mls_empty(tt));
        ASSERT(mls_pop_front(tt) == NULL);
        ASSERT(mls_erase_after(tt, NULL) == NULL);

        // few element tests
        for (int size = 0; size < 100; size++) {
            for (int it_pos = 0; it_pos < size + (size == 0 ? 1 : 0); it_pos++) {
                for (int test_type = 0; test_type <= 3; test_type++) {
                    for (int i = 0; i < size; i++) {
                        mls_push_front(tt, (void *)(uintptr_t)(i+1));
                        tl_push_front((void *)(uintptr_t)(i+1));
                    }
                    mls_it_t *it = mls_begin(tt);
                    int pos = tl_begin();
                    for (int j = 0; j < it_pos; j++) {
                        it = mls_next(it);
                        pos = tl_next(pos);
                    }
                    ASSERT(mls_size(tt) == size);
                    ASSERT(mls_front(tt) == tl_front());
                    void *value = (void *)(uintptr_t)(size+1);
                    if (test_type == 0) {
                        mls_push_front(tt, value);
                        tl_push_front(value);
                    } else if (test_type == 1) {
                        value = mls_pop_front(tt);
                        ASSERT(value == tl_pop_front());
                    } else if (test_type == 2) {
                        mls_insert_after(tt, it, value);
                        tl_insert_after(pos, value);
                    } else if (test_type == 3) {
                        value = mls_erase_after(tt, it);
                        ASSERT(value == tl_erase_after(pos));
                    }
                    pos = tl_begin();
                    for (it = mls_begin(tt); it != mls_end(); it = mls_next(it), pos = tl_next(pos)) {
                        ASSERT(mls_val(it) == tl_val(pos));
                    }
                    ASSERT(pos == tl_end());
                    ASSERT(mls_size(tt) == tl_size());

                    tl_clear();
                    mls_clear(tt);
                    ASSERT(mls_size(tt) == 0);
                    ASSERT(mls_front(tt) == NULL);
                }
            }
        }

        // random tests on larger list
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 3;
            if (operation == 0) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mls_push_front(tt, value);
                tl_push_front(value);
            } else if (operation == 1) {
                mls_it_t *it = mls_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mls_next(it);
                    pos = tl_next(pos);
                }
                if (it == mls_end()) {
                    ASSERT(pos == tl_end());
                } else {
                    ASSERT(mls_val(it) == tl_val(pos));
                    void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                    void *ret = mls_insert_after(tt, it, value);
                    ASSERT(ret == value);
                    ASSERT(mls_val(mls_next(it)) == value);
                    tl_insert_after(pos, value);
                }
            } else if (operation == 2 && tl_size() > 0) {
                mls_it_t *it = mls_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mls_next(it);
                    pos = tl_next(pos);
                }
                int erase_op = tausrand(taus_state) % 3;
                if (erase_op == 0 && it != mls_end()) {
                    void *value = mls_erase_after(tt, it);
                    ASSERT(tl_erase_after(pos) == value);
                } else if (erase_op == 1) {
                    void *value = mls_erase_after(tt, NULL); // should work as pop front
                    ASSERT(tl_pop_front() == value);
                } else if (erase_op == 2) {
                    void *value = mls_pop_front(tt);
                    ASSERT(tl_pop_front() == value);
                }
            }
            ASSERT(mls_size(tt) == tl_size());
            size_t size = 0;
            int pos = tl_begin();
            for (mls_it_t *it = mls_begin(tt); it != mls_end(); it = mls_next(it), pos = tl_next(pos)) {
                void *old_value = mls_val(it);
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mls_setval(it, value);
                ASSERT(mls_val(it) == value);
                mls_setval(it, old_value);
                ASSERT(mls_val(it) == tl_val(pos));
                size++;
            }
            ASSERT(size == mls_size(tt));
            ASSERT(mls_front(tt) == tl_front());
        }
        tl_deinit();

        ASSERT(mls_size(tt) > 0);
        mls_clear(tt);
        ASSERT(mls_size(tt) == 0);
        ASSERT(mls_empty(tt));
        ASSERT(mls_begin(tt) == NULL);

        // delete empty container
        mls_delete(tt);
        mls_delete(NULL);

        // test capacity limits
        tt = mls_new(test_size);
        for (int i = 0; i < test_size; i++) {
            void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
            mls_push_front(tt, value);
        }
        ASSERT(mls_push_front(tt, (void *)1) == NULL);
        ASSERT(mls_insert_after(tt, mls_begin(tt), (void *)1) == NULL);
        mls_delete(tt); // delete non-empty container
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: basic tests of all mls functions with performance memory management...");
    // NOTE: exact copy paste of first test case, except mls_* => mlsp_*
    {
        const int test_size = 5000;
        tl_init(test_size);

        mlsp_t *tt = mlsp_new(test_size);

        ASSERT(mlsp_max_size(tt) == test_size);
        ASSERT(mlsp_size(tt) == 0);
        ASSERT(mlsp_empty(tt));
        ASSERT(mlsp_pop_front(tt) == NULL);
        ASSERT(mlsp_erase_after(tt, NULL) == NULL);

        // few element tests
        for (int size = 0; size < 100; size++) {
            for (int it_pos = 0; it_pos < size + (size == 0 ? 1 : 0); it_pos++) {
                for (int test_type = 0; test_type <= 3; test_type++) {
                    for (int i = 0; i < size; i++) {
                        mlsp_push_front(tt, (void *)(uintptr_t)(i+1));
                        tl_push_front((void *)(uintptr_t)(i+1));
                    }
                    mlsp_it_t *it = mlsp_begin(tt);
                    int pos = tl_begin();
                    for (int j = 0; j < it_pos; j++) {
                        it = mlsp_next(it);
                        pos = tl_next(pos);
                    }
                    ASSERT(mlsp_size(tt) == size);
                    ASSERT(mlsp_front(tt) == tl_front());
                    void *value = (void *)(uintptr_t)(size+1);
                    if (test_type == 0) {
                        mlsp_push_front(tt, value);
                        tl_push_front(value);
                    } else if (test_type == 1) {
                        value = mlsp_pop_front(tt);
                        ASSERT(value == tl_pop_front());
                    } else if (test_type == 2) {
                        mlsp_insert_after(tt, it, value);
                        tl_insert_after(pos, value);
                    } else if (test_type == 3) {
                        value = mlsp_erase_after(tt, it);
                        ASSERT(value == tl_erase_after(pos));
                    }
                    pos = tl_begin();
                    for (it = mlsp_begin(tt); it != mlsp_end(); it = mlsp_next(it), pos = tl_next(pos)) {
                        ASSERT(mlsp_val(it) == tl_val(pos));
                    }
                    ASSERT(pos == tl_end());
                    ASSERT(mlsp_size(tt) == tl_size());

                    tl_clear();
                    mlsp_clear(tt);
                    ASSERT(mlsp_size(tt) == 0);
                    ASSERT(mlsp_front(tt) == NULL);
                }
            }
        }

        // random tests on larger list
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 3;
            if (operation == 0) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mlsp_push_front(tt, value);
                tl_push_front(value);
            } else if (operation == 1) {
                mlsp_it_t *it = mlsp_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mlsp_next(it);
                    pos = tl_next(pos);
                }
                if (it == mlsp_end()) {
                    ASSERT(pos == tl_end());
                } else {
                    ASSERT(mlsp_val(it) == tl_val(pos));
                    void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                    void *ret = mlsp_insert_after(tt, it, value);
                    ASSERT(ret == value);
                    ASSERT(mlsp_val(mlsp_next(it)) == value);
                    tl_insert_after(pos, value);
                }
            } else if (operation == 2 && tl_size() > 0) {
                mlsp_it_t *it = mlsp_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mlsp_next(it);
                    pos = tl_next(pos);
                }
                int erase_op = tausrand(taus_state) % 3;
                if (erase_op == 0 && it != mlsp_end()) {
                    void *value = mlsp_erase_after(tt, it);
                    ASSERT(tl_erase_after(pos) == value);
                } else if (erase_op == 1) {
                    void *value = mlsp_erase_after(tt, NULL); // should work as pop front
                    ASSERT(tl_pop_front() == value);
                } else if (erase_op == 2) {
                    void *value = mlsp_pop_front(tt);
                    ASSERT(tl_pop_front() == value);
                }
            }
            ASSERT(mlsp_size(tt) == tl_size());
            size_t size = 0;
            int pos = tl_begin();
            for (mlsp_it_t *it = mlsp_begin(tt); it != mlsp_end(); it = mlsp_next(it), pos = tl_next(pos)) {
                void *old_value = mlsp_val(it);
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mlsp_setval(it, value);
                ASSERT(mlsp_val(it) == value);
                mlsp_setval(it, old_value);
                ASSERT(mlsp_val(it) == tl_val(pos));
                size++;
            }
            ASSERT(size == mlsp_size(tt));
            ASSERT(mlsp_front(tt) == tl_front());
        }
        tl_deinit();

        ASSERT(mlsp_size(tt) > 0);
        mlsp_clear(tt);
        ASSERT(mlsp_size(tt) == 0);
        ASSERT(mlsp_empty(tt));
        ASSERT(mlsp_begin(tt) == NULL);

        // delete empty container
        mlsp_delete(tt);
        mlsp_delete(NULL);

        // test capacity limits
        tt = mlsp_new(test_size);
        for (int i = 0; i < test_size; i++) {
            void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
            mlsp_push_front(tt, value);
        }
        ASSERT(mlsp_push_front(tt, (void *)1) == NULL);
        ASSERT(mlsp_insert_after(tt, mlsp_begin(tt), (void *)1) == NULL);
        mlsp_delete(tt); // delete non-empty container
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: basic tests of all mls functions with static memory management...");
    // NOTE: exact copy paste of first test case, except mls_* => mlss_*
    {
        const int test_size = 5000;
        tl_init(test_size);

        mlss_t *tt = mlss_new(test_size);

        ASSERT(mlss_max_size(tt) == test_size);
        ASSERT(mlss_size(tt) == 0);
        ASSERT(mlss_empty(tt));
        ASSERT(mlss_pop_front(tt) == NULL);
        ASSERT(mlss_erase_after(tt, NULL) == NULL);

        // few element tests
        for (int size = 0; size < 100; size++) {
            for (int it_pos = 0; it_pos < size + (size == 0 ? 1 : 0); it_pos++) {
                for (int test_type = 0; test_type <= 3; test_type++) {
                    for (int i = 0; i < size; i++) {
                        mlss_push_front(tt, (void *)(uintptr_t)(i+1));
                        tl_push_front((void *)(uintptr_t)(i+1));
                    }
                    mlss_it_t *it = mlss_begin(tt);
                    int pos = tl_begin();
                    for (int j = 0; j < it_pos; j++) {
                        it = mlss_next(it);
                        pos = tl_next(pos);
                    }
                    ASSERT(mlss_size(tt) == size);
                    ASSERT(mlss_front(tt) == tl_front());
                    void *value = (void *)(uintptr_t)(size+1);
                    if (test_type == 0) {
                        mlss_push_front(tt, value);
                        tl_push_front(value);
                    } else if (test_type == 1) {
                        value = mlss_pop_front(tt);
                        ASSERT(value == tl_pop_front());
                    } else if (test_type == 2) {
                        mlss_insert_after(tt, it, value);
                        tl_insert_after(pos, value);
                    } else if (test_type == 3) {
                        value = mlss_erase_after(tt, it);
                        ASSERT(value == tl_erase_after(pos));
                    }
                    pos = tl_begin();
                    for (it = mlss_begin(tt); it != mlss_end(); it = mlss_next(it), pos = tl_next(pos)) {
                        ASSERT(mlss_val(it) == tl_val(pos));
                    }
                    ASSERT(pos == tl_end());
                    ASSERT(mlss_size(tt) == tl_size());

                    tl_clear();
                    mlss_clear(tt);
                    ASSERT(mlss_size(tt) == 0);
                    ASSERT(mlss_front(tt) == NULL);
                }
            }
        }

        // random tests on larger list
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 3;
            if (operation == 0) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mlss_push_front(tt, value);
                tl_push_front(value);
            } else if (operation == 1) {
                mlss_it_t *it = mlss_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mlss_next(it);
                    pos = tl_next(pos);
                }
                if (it == mlss_end()) {
                    ASSERT(pos == tl_end());
                } else {
                    ASSERT(mlss_val(it) == tl_val(pos));
                    void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                    void *ret = mlss_insert_after(tt, it, value);
                    ASSERT(ret == value);
                    ASSERT(mlss_val(mlss_next(it)) == value);
                    tl_insert_after(pos, value);
                }
            } else if (operation == 2 && tl_size() > 0) {
                mlss_it_t *it = mlss_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mlss_next(it);
                    pos = tl_next(pos);
                }
                int erase_op = tausrand(taus_state) % 3;
                if (erase_op == 0 && it != mlss_end()) {
                    void *value = mlss_erase_after(tt, it);
                    ASSERT(tl_erase_after(pos) == value);
                } else if (erase_op == 1) {
                    void *value = mlss_erase_after(tt, NULL); // should work as pop front
                    ASSERT(tl_pop_front() == value);
                } else if (erase_op == 2) {
                    void *value = mlss_pop_front(tt);
                    ASSERT(tl_pop_front() == value);
                }
            }
            ASSERT(mlss_size(tt) == tl_size());
            size_t size = 0;
            int pos = tl_begin();
            for (mlss_it_t *it = mlss_begin(tt); it != mlss_end(); it = mlss_next(it), pos = tl_next(pos)) {
                void *old_value = mlss_val(it);
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mlss_setval(it, value);
                ASSERT(mlss_val(it) == value);
                mlss_setval(it, old_value);
                ASSERT(mlss_val(it) == tl_val(pos));
                size++;
            }
            ASSERT(size == mlss_size(tt));
            ASSERT(mlss_front(tt) == tl_front());
        }
        tl_deinit();

        ASSERT(mlss_size(tt) > 0);
        mlss_clear(tt);
        ASSERT(mlss_size(tt) == 0);
        ASSERT(mlss_empty(tt));
        ASSERT(mlss_begin(tt) == NULL);

        // delete empty container
        mlss_delete(tt);
        mlss_delete(NULL);

        // test capacity limits
        tt = mlss_new(test_size);
        for (int i = 0; i < test_size; i++) {
            void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
            mlss_push_front(tt, value);
        }
        ASSERT(mlss_push_front(tt, (void *)1) == NULL);
        ASSERT(mlss_insert_after(tt, mlss_begin(tt), (void *)1) == NULL);
        mlss_delete(tt); // delete non-empty container
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: extra tests for final mls coverage...");
    {
        // static storage with mlss_init()
        {
            mlss_t tt;
            struct mlss_node nodes[64];
            mlss_init(&tt, nodes, sizeof(nodes));
            for (int i = 0; i < 64; i++) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mlss_push_front(&tt, value);
            }
            ASSERT(mlss_push_front(&tt, (void *)1) == NULL);
        }

        // with copy/free macros
        {
            mlsc_t *tt =  mlsc_new(~0);
            int val = 1;
            mlsc_push_front(tt, &val);
            mlsc_push_front(tt, &val);
            mlsc_insert_after(tt, mlsc_begin(tt), &val);
            mlsc_pop_front(tt);
            mlsc_erase_after(tt, mlsc_begin(tt));
            mlsc_clear(tt);
            mlsc_push_front(tt, &val);
            mlsc_setval(mlsc_begin(tt), &val);
            mlsc_delete(tt);
        }

        // with MC_VALUE_RETURN_REF=1
        {
            mlsr_t *tt =  mlsr_new(~0);
            int *res;
            res = mlsr_push_front(tt, 1);
            ASSERT(*res == 1);
            ASSERT(*mlsr_front(tt) == 1);
            ASSERT(*mlsr_insert_after(tt, mlsr_begin(tt), 2) == 2);
            ASSERT(*mlsr_val(mlsr_begin(tt)) == 1);
            ASSERT(*mlsr_setval(mlsr_begin(tt), 3) == 3);
            mlsr_delete(tt);
        }

        // with MC_VALUE_NO_INSERT_ARG=1
        {
            mlsa_t *tt =  mlsa_new(~0);
            *mlsa_push_front(tt) = 1;
            ASSERT(*mlsa_front(tt) == 1);
            *mlsa_insert_after(tt, mlsa_begin(tt)) = 2;
            mlsa_pop_front(tt);
            ASSERT(*mlsa_front(tt) == 2);
            mlsa_delete(tt);
        }

    }
    fprintf(stderr, "pass\n");
}

static void
mld_tests(void)
{
    fprintf(stderr, "Test: basic tests of all mld functions with default configuration...");
    {
        const int test_size = 5000;
        tl_init(test_size);

        mld_t *tt = mld_new(test_size);

        ASSERT(mld_max_size(tt) == test_size);
        ASSERT(mld_size(tt) == 0);
        ASSERT(mld_empty(tt));
        ASSERT(mld_pop_front(tt) == NULL);
        ASSERT(mld_pop_back(tt) == NULL);

        // few element tests
        for (int size = 0; size < 100; size++) {
            for (int it_pos = 0; it_pos < size + (size == 0 ? 1 : 0); it_pos++) {
                for (int test_type = 0; test_type <= 7; test_type++) {
                    for (int i = 0; i < size; i++) {
                        mld_push_front(tt, (void *)(uintptr_t)(i+1));
                        tl_push_front((void *)(uintptr_t)(i+1));
                    }
                    mld_it_t *it = mld_begin(tt);
                    int pos = tl_begin();
                    for (int j = 0; j < it_pos; j++) {
                        it = mld_next(it);
                        pos = tl_next(pos);
                    }
                    ASSERT(mld_size(tt) == size);
                    ASSERT(mld_front(tt) == tl_front());
                    ASSERT(mld_back(tt) == tl_back());
                    void *value = (void *)(uintptr_t)(size+1);
                    if (test_type == 0) {
                        mld_to_front(tt, it);
                        tl_to_front(pos);
                    } else if (test_type == 1) {
                        mld_to_back(tt, it);
                        tl_to_back(pos);
                    } else if (test_type == 2) {
                        mld_push_front(tt, value);
                        tl_push_front(value);
                    } else if (test_type == 3) {
                        mld_push_back(tt, value);
                        tl_push_back(value);
                    } else if (test_type == 4) {
                        value = mld_pop_front(tt);
                        ASSERT(value == tl_pop_front());
                    } else if (test_type == 5) {
                        value = mld_pop_back(tt);
                        ASSERT(value == tl_pop_back());
                    } else if (test_type == 6 && pos != tl_end()) {
                        mld_insert(tt, it, value);
                        tl_insert(pos, value);
                    } else if (test_type == 7 && pos != tl_end()) {
                        mld_it_t *next_it = mld_next(it);
                        it = mld_erase(tt, it);
                        tl_erase(pos);
                        ASSERT(it == next_it);
                    }
                    pos = tl_begin();
                    for (it = mld_begin(tt); it != mld_end(); it = mld_next(it), pos = tl_next(pos)) {
                        ASSERT(mld_val(it) == tl_val(pos));
                    }
                    ASSERT(pos == tl_end());
                    ASSERT(mld_size(tt) == tl_size());

                    tl_clear();
                    mld_clear(tt);
                    ASSERT(mld_size(tt) == 0);
                    ASSERT(mld_front(tt) == NULL);
                }
            }
        }

        // random tests on larger list
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 4;
            if (operation == 0) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                if (tausrand(taus_state) % 1 == 0) {
                    mld_push_front(tt, value);
                    tl_push_front(value);
                    ASSERT(mld_front(tt) == value);
                } else {
                    mld_push_back(tt, value);
                    tl_push_back(value);
                    ASSERT(mld_back(tt) == value);
                }
            } else if (operation == 1) {
                mld_it_t *it = mld_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mld_next(it);
                    pos = tl_next(pos);
                }
                if (it == mld_end()) {
                    ASSERT(pos == tl_end());
                } else {
                    ASSERT(mld_val(it) == tl_val(pos));
                    void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                    void *ret = mld_insert(tt, it, value);
                    ASSERT(ret == value);
                    ASSERT(mld_val(mld_prev(it)) == value);
                    tl_insert(pos, value);
                    ASSERT(mld_size(tt) == tl_size());
                }
            } else if (operation == 2 && tl_size() > 0) {
                mld_it_t *it = mld_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mld_next(it);
                    pos = tl_next(pos);
                }
                int erase_op = tausrand(taus_state) % 3;
                if (erase_op == 0 && it != mld_end()) {
                    mld_it_t *next_it = mld_next(it);
                    it = mld_erase(tt, it);
                    tl_erase(pos);
                    ASSERT(it == next_it);
                } else if (erase_op == 1) {
                    void *value = mld_pop_front(tt);
                    ASSERT(tl_pop_front() == value);
                } else if (erase_op == 2) {
                    void *value = mld_pop_back(tt);
                    ASSERT(tl_pop_back() == value);
                }
            } else if (operation == 3 && tl_size() > 0) {
                mld_it_t *it = mld_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim-1; j++) {
                    it = mld_next(it);
                    pos = tl_next(pos);
                }
                if (tausrand(taus_state) % 1 == 0) {
                    mld_to_front(tt, it);
                    tl_to_front(pos);
                } else {
                    mld_to_back(tt, it);
                    tl_to_back(pos);
                }
            }
            ASSERT(mld_size(tt) == tl_size());
            int pos = tl_begin();
            for (mld_it_t *it = mld_begin(tt); it != mld_end(); it = mld_next(it), pos = tl_next(pos)) {
                void *old_value = mld_val(it);
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mld_setval(it, value);
                ASSERT(mld_val(it) == value);
                mld_setval(it, old_value);
                ASSERT(mld_val(it) == tl_val(pos));
            }
            ASSERT(pos == tl_end());
            pos = tl_rbegin();
            for (mld_it_t *it = mld_rbegin(tt); it != mld_rend(); it = mld_prev(it), pos = tl_prev(pos)) {
                ASSERT(mld_val(it) == tl_val(pos));
            }
            ASSERT(pos == tl_rend());
            ASSERT(mld_front(tt) == tl_front());
            ASSERT(mld_back(tt) == tl_back());
        }
        tl_deinit();

        ASSERT(mld_size(tt) > 0);
        mld_clear(tt);
        ASSERT(mld_size(tt) == 0);
        ASSERT(mld_empty(tt));
        ASSERT(mld_begin(tt) == NULL);
        mld_delete(tt); // delete empty container
        mld_delete(NULL);

        // test capacity limits
        tt = mld_new(test_size);
        for (int i = 0; i < test_size; i++) {
            void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
            mld_push_front(tt, value);
        }
        ASSERT(mld_push_front(tt, (void *)1) == NULL);
        ASSERT(mld_push_back(tt, (void *)1) == NULL);
        ASSERT(mld_insert(tt, mld_begin(tt), (void *)1) == NULL);
        mld_delete(tt); // delete filled container
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: basic tests of all mld functions with performance memory management...");
    // NOTE: exact copy paste of first test case, except mld_* => mldp_*
    {
        const int test_size = 5000;
        tl_init(test_size);

        mldp_t *tt = mldp_new(test_size);

        ASSERT(mldp_max_size(tt) == test_size);
        ASSERT(mldp_size(tt) == 0);
        ASSERT(mldp_empty(tt));
        ASSERT(mldp_pop_front(tt) == NULL);
        ASSERT(mldp_pop_back(tt) == NULL);

        // few element tests
        for (int size = 0; size < 100; size++) {
            for (int it_pos = 0; it_pos < size + (size == 0 ? 1 : 0); it_pos++) {
                for (int test_type = 0; test_type <= 7; test_type++) {
                    for (int i = 0; i < size; i++) {
                        mldp_push_front(tt, (void *)(uintptr_t)(i+1));
                        tl_push_front((void *)(uintptr_t)(i+1));
                    }
                    mldp_it_t *it = mldp_begin(tt);
                    int pos = tl_begin();
                    for (int j = 0; j < it_pos; j++) {
                        it = mldp_next(it);
                        pos = tl_next(pos);
                    }
                    ASSERT(mldp_size(tt) == size);
                    ASSERT(mldp_front(tt) == tl_front());
                    ASSERT(mldp_back(tt) == tl_back());
                    void *value = (void *)(uintptr_t)(size+1);
                    if (test_type == 0) {
                        mldp_to_front(tt, it);
                        tl_to_front(pos);
                    } else if (test_type == 1) {
                        mldp_to_back(tt, it);
                        tl_to_back(pos);
                    } else if (test_type == 2) {
                        mldp_push_front(tt, value);
                        tl_push_front(value);
                    } else if (test_type == 3) {
                        mldp_push_back(tt, value);
                        tl_push_back(value);
                    } else if (test_type == 4) {
                        value = mldp_pop_front(tt);
                        ASSERT(value == tl_pop_front());
                    } else if (test_type == 5) {
                        value = mldp_pop_back(tt);
                        ASSERT(value == tl_pop_back());
                    } else if (test_type == 6 && pos != tl_end()) {
                        mldp_insert(tt, it, value);
                        tl_insert(pos, value);
                    } else if (test_type == 7 && pos != tl_end()) {
                        mldp_it_t *next_it = mldp_next(it);
                        it = mldp_erase(tt, it);
                        tl_erase(pos);
                        ASSERT(it == next_it);
                    }
                    pos = tl_begin();
                    for (it = mldp_begin(tt); it != mldp_end(); it = mldp_next(it), pos = tl_next(pos)) {
                        ASSERT(mldp_val(it) == tl_val(pos));
                    }
                    ASSERT(pos == tl_end());
                    ASSERT(mldp_size(tt) == tl_size());

                    tl_clear();
                    mldp_clear(tt);
                    ASSERT(mldp_size(tt) == 0);
                    ASSERT(mldp_front(tt) == NULL);
                }
            }
        }

        // random tests on larger list
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 4;
            if (operation == 0) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                if (tausrand(taus_state) % 1 == 0) {
                    mldp_push_front(tt, value);
                    tl_push_front(value);
                    ASSERT(mldp_front(tt) == value);
                } else {
                    mldp_push_back(tt, value);
                    tl_push_back(value);
                    ASSERT(mldp_back(tt) == value);
                }
            } else if (operation == 1) {
                mldp_it_t *it = mldp_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mldp_next(it);
                    pos = tl_next(pos);
                }
                if (it == mldp_end()) {
                    ASSERT(pos == tl_end());
                } else {
                    ASSERT(mldp_val(it) == tl_val(pos));
                    void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                    void *ret = mldp_insert(tt, it, value);
                    ASSERT(ret == value);
                    ASSERT(mldp_val(mldp_prev(it)) == value);
                    tl_insert(pos, value);
                    ASSERT(mldp_size(tt) == tl_size());
                }
            } else if (operation == 2 && tl_size() > 0) {
                mldp_it_t *it = mldp_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mldp_next(it);
                    pos = tl_next(pos);
                }
                int erase_op = tausrand(taus_state) % 3;
                if (erase_op == 0 && it != mldp_end()) {
                    mldp_it_t *next_it = mldp_next(it);
                    it = mldp_erase(tt, it);
                    tl_erase(pos);
                    ASSERT(it == next_it);
                } else if (erase_op == 1) {
                    void *value = mldp_pop_front(tt);
                    ASSERT(tl_pop_front() == value);
                } else if (erase_op == 2) {
                    void *value = mldp_pop_back(tt);
                    ASSERT(tl_pop_back() == value);
                }
            } else if (operation == 3 && tl_size() > 0) {
                mldp_it_t *it = mldp_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim-1; j++) {
                    it = mldp_next(it);
                    pos = tl_next(pos);
                }
                if (tausrand(taus_state) % 1 == 0) {
                    mldp_to_front(tt, it);
                    tl_to_front(pos);
                } else {
                    mldp_to_back(tt, it);
                    tl_to_back(pos);
                }
            }
            ASSERT(mldp_size(tt) == tl_size());
            int pos = tl_begin();
            for (mldp_it_t *it = mldp_begin(tt); it != mldp_end(); it = mldp_next(it), pos = tl_next(pos)) {
                void *old_value = mldp_val(it);
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mldp_setval(it, value);
                ASSERT(mldp_val(it) == value);
                mldp_setval(it, old_value);
                ASSERT(mldp_val(it) == tl_val(pos));
            }
            ASSERT(pos == tl_end());
            pos = tl_rbegin();
            for (mldp_it_t *it = mldp_rbegin(tt); it != mldp_rend(); it = mldp_prev(it), pos = tl_prev(pos)) {
                ASSERT(mldp_val(it) == tl_val(pos));
            }
            ASSERT(pos == tl_rend());
            ASSERT(mldp_front(tt) == tl_front());
            ASSERT(mldp_back(tt) == tl_back());
        }
        tl_deinit();

        ASSERT(mldp_size(tt) > 0);
        mldp_clear(tt);
        ASSERT(mldp_size(tt) == 0);
        ASSERT(mldp_empty(tt));
        ASSERT(mldp_begin(tt) == NULL);
        mldp_delete(tt); // delete empty container
        mldp_delete(NULL);

        // test capacity limits
        tt = mldp_new(test_size);
        for (int i = 0; i < test_size; i++) {
            void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
            mldp_push_front(tt, value);
        }
        ASSERT(mldp_push_front(tt, (void *)1) == NULL);
        ASSERT(mldp_push_back(tt, (void *)1) == NULL);
        ASSERT(mldp_insert(tt, mldp_begin(tt), (void *)1) == NULL);
        mldp_delete(tt); // delete filled container
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: basic tests of all mld functions with static memory management...");
    // NOTE: exact copy paste of first test case, except mld_* => mlds_*
    {
        const int test_size = 5000;
        tl_init(test_size);

        mlds_t *tt = mlds_new(test_size);

        ASSERT(mlds_max_size(tt) == test_size);
        ASSERT(mlds_size(tt) == 0);
        ASSERT(mlds_empty(tt));
        ASSERT(mlds_pop_front(tt) == NULL);
        ASSERT(mlds_pop_back(tt) == NULL);

        // few element tests
        for (int size = 0; size < 100; size++) {
            for (int it_pos = 0; it_pos < size + (size == 0 ? 1 : 0); it_pos++) {
                for (int test_type = 0; test_type <= 7; test_type++) {
                    for (int i = 0; i < size; i++) {
                        mlds_push_front(tt, (void *)(uintptr_t)(i+1));
                        tl_push_front((void *)(uintptr_t)(i+1));
                    }
                    mlds_it_t *it = mlds_begin(tt);
                    int pos = tl_begin();
                    for (int j = 0; j < it_pos; j++) {
                        it = mlds_next(it);
                        pos = tl_next(pos);
                    }
                    ASSERT(mlds_size(tt) == size);
                    ASSERT(mlds_front(tt) == tl_front());
                    ASSERT(mlds_back(tt) == tl_back());
                    void *value = (void *)(uintptr_t)(size+1);
                    if (test_type == 0) {
                        mlds_to_front(tt, it);
                        tl_to_front(pos);
                    } else if (test_type == 1) {
                        mlds_to_back(tt, it);
                        tl_to_back(pos);
                    } else if (test_type == 2) {
                        mlds_push_front(tt, value);
                        tl_push_front(value);
                    } else if (test_type == 3) {
                        mlds_push_back(tt, value);
                        tl_push_back(value);
                    } else if (test_type == 4) {
                        value = mlds_pop_front(tt);
                        ASSERT(value == tl_pop_front());
                    } else if (test_type == 5) {
                        value = mlds_pop_back(tt);
                        ASSERT(value == tl_pop_back());
                    } else if (test_type == 6 && pos != tl_end()) {
                        mlds_insert(tt, it, value);
                        tl_insert(pos, value);
                    } else if (test_type == 7 && pos != tl_end()) {
                        mlds_it_t *next_it = mlds_next(it);
                        it = mlds_erase(tt, it);
                        tl_erase(pos);
                        ASSERT(it == next_it);
                    }
                    pos = tl_begin();
                    for (it = mlds_begin(tt); it != mlds_end(); it = mlds_next(it), pos = tl_next(pos)) {
                        ASSERT(mlds_val(it) == tl_val(pos));
                    }
                    ASSERT(pos == tl_end());
                    ASSERT(mlds_size(tt) == tl_size());

                    tl_clear();
                    mlds_clear(tt);
                    ASSERT(mlds_size(tt) == 0);
                    ASSERT(mlds_front(tt) == NULL);
                }
            }
        }

        // random tests on larger list
        for (int i = 0; i < test_size; i++) {
            int operation = tausrand(taus_state) % 4;
            if (operation == 0) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                if (tausrand(taus_state) % 1 == 0) {
                    mlds_push_front(tt, value);
                    tl_push_front(value);
                    ASSERT(mlds_front(tt) == value);
                } else {
                    mlds_push_back(tt, value);
                    tl_push_back(value);
                    ASSERT(mlds_back(tt) == value);
                }
            } else if (operation == 1) {
                mlds_it_t *it = mlds_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mlds_next(it);
                    pos = tl_next(pos);
                }
                if (it == mlds_end()) {
                    ASSERT(pos == tl_end());
                } else {
                    ASSERT(mlds_val(it) == tl_val(pos));
                    void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                    void *ret = mlds_insert(tt, it, value);
                    ASSERT(ret == value);
                    ASSERT(mlds_val(mlds_prev(it)) == value);
                    tl_insert(pos, value);
                    ASSERT(mlds_size(tt) == tl_size());
                }
            } else if (operation == 2 && tl_size() > 0) {
                mlds_it_t *it = mlds_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim; j++) {
                    it = mlds_next(it);
                    pos = tl_next(pos);
                }
                int erase_op = tausrand(taus_state) % 3;
                if (erase_op == 0 && it != mlds_end()) {
                    mlds_it_t *next_it = mlds_next(it);
                    it = mlds_erase(tt, it);
                    tl_erase(pos);
                    ASSERT(it == next_it);
                } else if (erase_op == 1) {
                    void *value = mlds_pop_front(tt);
                    ASSERT(tl_pop_front() == value);
                } else if (erase_op == 2) {
                    void *value = mlds_pop_back(tt);
                    ASSERT(tl_pop_back() == value);
                }
            } else if (operation == 3 && tl_size() > 0) {
                mlds_it_t *it = mlds_begin(tt);
                int pos = tl_begin();
                int lim = tausrand(taus_state) % (tl_size() + 1);
                for (int j = 0; j < lim-1; j++) {
                    it = mlds_next(it);
                    pos = tl_next(pos);
                }
                if (tausrand(taus_state) % 1 == 0) {
                    mlds_to_front(tt, it);
                    tl_to_front(pos);
                } else {
                    mlds_to_back(tt, it);
                    tl_to_back(pos);
                }
            }
            ASSERT(mlds_size(tt) == tl_size());
            int pos = tl_begin();
            for (mlds_it_t *it = mlds_begin(tt); it != mlds_end(); it = mlds_next(it), pos = tl_next(pos)) {
                void *old_value = mlds_val(it);
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mlds_setval(it, value);
                ASSERT(mlds_val(it) == value);
                mlds_setval(it, old_value);
                ASSERT(mlds_val(it) == tl_val(pos));
            }
            ASSERT(pos == tl_end());
            pos = tl_rbegin();
            for (mlds_it_t *it = mlds_rbegin(tt); it != mlds_rend(); it = mlds_prev(it), pos = tl_prev(pos)) {
                ASSERT(mlds_val(it) == tl_val(pos));
            }
            ASSERT(pos == tl_rend());
            ASSERT(mlds_front(tt) == tl_front());
            ASSERT(mlds_back(tt) == tl_back());
        }
        tl_deinit();

        ASSERT(mlds_size(tt) > 0);
        mlds_clear(tt);
        ASSERT(mlds_size(tt) == 0);
        ASSERT(mlds_empty(tt));
        ASSERT(mlds_begin(tt) == NULL);
        mlds_delete(tt); // delete empty container
        mlds_delete(NULL);

        // test capacity limits
        tt = mlds_new(test_size);
        for (int i = 0; i < test_size; i++) {
            void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
            mlds_push_front(tt, value);
        }
        ASSERT(mlds_push_front(tt, (void *)1) == NULL);
        ASSERT(mlds_push_back(tt, (void *)1) == NULL);
        ASSERT(mlds_insert(tt, mlds_begin(tt), (void *)1) == NULL);
        mlds_delete(tt); // delete filled container
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: extra tests for final mls coverage...");
    {
        // static storage with mlds_init()
        {
            mlds_t tt;
            struct mlds_node nodes[64];
            mlds_init(&tt, nodes, sizeof(nodes));
            for (int i = 0; i < 64; i++) {
                void *value = (void *)(uintptr_t)(tausrand(taus_state) | 1);
                mlds_push_front(&tt, value);
            }
            ASSERT(mlds_push_front(&tt, (void *)1) == NULL);
        }

        // with copy/free macros
        {
            mldc_t *tt =  mldc_new(~0);
            int val = 1;
            mldc_push_front(tt, &val);
            mldc_push_front(tt, &val);
            mldc_push_back(tt, &val);
            mldc_insert(tt, mldc_begin(tt), &val);
            mldc_pop_front(tt);
            mldc_pop_back(tt);
            mldc_erase(tt, mldc_begin(tt));
            mldc_clear(tt);
            mldc_push_front(tt, &val);
            mldc_setval(mldc_begin(tt), &val);
            mldc_delete(tt);
        }

        // with MC_VALUE_RETURN_REF=1
        {
            mldr_t *tt =  mldr_new(~0);
            int *res;
            res = mldr_push_front(tt, 1);
            ASSERT(*res == 1);
            res = mldr_push_back(tt, 3);
            ASSERT(*res == 3);
            ASSERT(*mldr_front(tt) == 1);
            ASSERT(*mldr_back(tt) == 3);
            ASSERT(*mldr_insert(tt, mldr_rbegin(tt), 2) == 2);
            ASSERT(*mldr_val(mldr_prev(mldr_rbegin(tt))) == 2);
            ASSERT(*mldr_setval(mldr_begin(tt), 4) == 4);
            mldr_delete(tt);
        }

        // with MC_VALUE_NO_INSERT_ARG=1
        {
            mlda_t *tt =  mlda_new(~0);
            *mlda_push_front(tt) = 1;
            ASSERT(*mlda_front(tt) == 1);
            *mlda_push_back(tt) = 2;
            ASSERT(*mlda_back(tt) == 2);
            *mlda_insert(tt, mlda_begin(tt)) = 3;
            ASSERT(*mlda_front(tt) == 3);
            mlda_delete(tt);
        }

    }
    fprintf(stderr, "pass\n");
}

#if TRACKMEM_DEBUG - 0 != 0
#include <trackmem.h>
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
    mls_tests();
    mld_tests();
#if TRACKMEM_DEBUG - 0 != 0
    trackmem_delete(nodepool_tm);
    trackmem_delete(buddyalloc_tm);
#endif
    return 0;
}
