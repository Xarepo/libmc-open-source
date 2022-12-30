/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <unittest_helpers.h>

#include <mv_tmpl.h>

#define MC_PREFIX mva
#define MC_VALUE_T char *
#define MC_COPY_VALUE(dest, src) dest = strdup(src)
#define MC_FREE_VALUE(value) free(value)
#include <mv_tmpl.h>

static uintptr_t
inc_sizing(uintptr_t max_capacity, uintptr_t current_capacity, uintptr_t count)
{
    if (count <= current_capacity) {
        return current_capacity;
    }
    uintptr_t cap = 1;
    while (cap < count) {
        if (cap > 4096) {
            cap += 4096;
        } else {
            cap *= 2;
        }
    }
    return cap > max_capacity ? max_capacity : cap;
}

static uintptr_t
dec_sizing(uintptr_t current_capacity, uintptr_t count)
{
    uintptr_t cap = 1 << bit32_bsr(current_capacity);
    if (count <= cap >> 1) {
        cap = cap - cap / 4;
        ASSERT(cap >= count);
        return cap;
    }
    return current_capacity;
}

static void
mv_basic_tests(void)
{
    fprintf(stderr, "Test: basic tests of all mv functions with default configuration...");
    {
        mv_t *tt = mv_new(~0, 8);

        ASSERT(mv_empty(tt));
        ASSERT(mv_max_size(tt) == (uintptr_t)~0 >> 2);
        ASSERT(mv_size(tt) == 0);
        ASSERT(mv_front(tt) == NULL);
        ASSERT(mv_back(tt) == NULL);
        ASSERT(mv_data(tt) != NULL);
        const uintptr_t test_size = 10000;
        for (uintptr_t i = 0; i < test_size; i++) {
            mv_push_back(tt, (void *)(i + 1));
            ASSERT(tt->current_capacity == inc_sizing(~0, tt->current_capacity, i+1));
            ASSERT(mv_size(tt) == i + 1);
            ASSERT(!mv_empty(tt));
        }
        ASSERT(mv_size(tt) == test_size);
        ASSERT(mv_front(tt) == (void *)1);
        ASSERT(mv_back(tt) == (void *)test_size);
        for (uintptr_t i = 0; i < test_size; i++) {
            ASSERT(mv_at(tt, i) == (void *)(i+1));
            ASSERT(mv_data(tt)[i] == (void *)(i+1));
        }
        {
            uintptr_t i = 1;
            for (mv_it_t *it = mv_begin(tt); it != mv_end(tt); it = mv_next(it)) {
                ASSERT(mv_val(it) == (void *)(i));
                ASSERT(mv_setval(it, (void *)(i + 1)) == (void *)(i + 1));
                i++;
            }
            ASSERT(i == test_size+1);
            for (mv_it_t *it = mv_rbegin(tt); it != mv_rend(tt); it = mv_prev(it)) {
                ASSERT(mv_val(it) == (void *)(i));
                ASSERT(mv_setval(it, (void *)(i - 1)) == (void *)(i - 1));
                i--;
            }
            ASSERT(i == 1);
        }
        for (uintptr_t i = 0; i < test_size; i++) {
            void *val = mv_pop_back(tt);
            ASSERT(val == (void *)(test_size - i));
            ASSERT(tt->current_capacity == dec_sizing(tt->current_capacity, mv_size(tt)+1));
            ASSERT(mv_size(tt) == test_size - i - 1);
            if (i < test_size - 1) {
                ASSERT(!mv_empty(tt));
            } else {
                ASSERT(mv_empty(tt));
            }
        }
        ASSERT(mv_size(tt) == 0);

        mv_delete(tt);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: resizing, automatic and manual...");
    {
        const uintptr_t test_size = 10000;
        mv_t *tt = mv_new(test_size, 0);
        void *dummy_value = (void *)1;

        for (int k = 0; k < 1000; k++) {
            const uintptr_t rsize = 2+k;

            // reserve + increase size
            mv_reserve(tt, rsize);
            ASSERT(mv_size(tt) == 0);
            ASSERT(tt->current_capacity == rsize);
            for (uintptr_t i = 0; i < test_size; i++) {
                mv_push_back(tt, dummy_value);
                if (i < rsize) {
                    ASSERT(tt->current_capacity == rsize);
                } else {
                    ASSERT(tt->current_capacity == inc_sizing(test_size, tt->current_capacity, i+1));
                }
            }

            // decrease size
            mv_resize(tt, rsize);
            ASSERT(mv_size(tt) == rsize);
            ASSERT(tt->current_capacity > rsize);
            mv_shrink_to_fit(tt);
            ASSERT(tt->current_capacity == rsize);
            for (uintptr_t i = 0; i < rsize; i++) {
                mv_pop_back(tt);
                uintptr_t expected_cap = dec_sizing(tt->current_capacity, mv_size(tt)+1);
                ASSERT(tt->current_capacity == expected_cap);
            }

            mv_clear(tt);
        }

        // reduce and increase size, test that there are no flap-points
        mv_clear(tt);
        ASSERT(mv_size(tt) == 0);
        ASSERT(mv_empty(tt));
        ASSERT(tt->values == NULL);
        mv_push_back(tt, dummy_value);
        for (uintptr_t i = 0; i < test_size; i++) {
            uintptr_t cap1 = tt->current_capacity;
            mv_pop_back(tt);
            uintptr_t cap2 = tt->current_capacity;
            mv_push_back(tt, dummy_value);
            uintptr_t cap3 = tt->current_capacity;
            mv_push_back(tt, dummy_value);
            ASSERT(cap1 == cap2 && cap2 == cap3);
        }

        mv_delete(tt);

        // test that resize to larger size zeroes the data
        tt = mv_new(2000, 0);
        mv_resize(tt, 1000);
        for (int i = 0; i < 1000; i++) {
            ASSERT(mv_at(tt, i) == NULL);
        }

        { // reserve tests
            uintptr_t cap = tt->current_capacity;
            mv_reserve(tt, 10);
            ASSERT(mv_size(tt) == 1000);
            ASSERT(tt->current_capacity == cap);
            mv_reserve(tt, mv_max_size(tt) + 1000);
            ASSERT(tt->current_capacity == mv_max_size(tt));
            ASSERT(mv_size(tt) == 1000);
        }

        mv_delete(tt);
        mv_delete(NULL);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: static allocation...");
    {
        const size_t test_size = 1000;
        void *values[test_size];
        mv_t tt;
        mv_init(&tt, values, sizeof(values));
        ASSERT(mv_size(&tt) == 0);
        for (uintptr_t i = 0; i < test_size; i++) {
            ASSERT(mv_push_back(&tt, (void *)(i+1)) == (void *)(i+1));
            ASSERT(mv_max_size(&tt) == test_size);
        }
        ASSERT(mv_push_back(&tt, (void *)(1)) == NULL);
        for (uintptr_t i = 0; i < test_size; i++) {
            ASSERT(mv_pop_back(&tt) == (void *)(test_size - i));
            ASSERT(mv_max_size(&tt) == test_size);
        }
        ASSERT(mv_size(&tt) == 0);
        ASSERT(mv_max_size(&tt) == test_size);
        for (uintptr_t i = 0; i < test_size; i++) {
            ASSERT(mv_push_back(&tt, (void *)(i+1)) == (void *)(i+1));
        }
        mv_clear(&tt);
        ASSERT(tt.current_capacity == test_size);
        ASSERT(mv_size(&tt) == 0);
        ASSERT(mv_max_size(&tt) == test_size);

        for (int k = 0; k < 2; k++) {
            if (k == 0) {
                mv_init_span(&tt, values, sizeof(values));
            } else {
                tt = (mv_t)MV_SPAN_INITIALIZER(values);
            }
            ASSERT(mv_size(&tt) == test_size);
            ASSERT(mv_push_back(&tt, (void *)(1)) == NULL);
            ASSERT(mv_pop_back(&tt) == NULL);
            uintptr_t i = 1;
            for (mv_it_t *it = mv_begin(&tt); it != mv_end(&tt); it = mv_next(it)) {
                mv_setval(it, (void *)(i));
                ASSERT(mv_at(&tt, i-1) == (void *)(i));
                i++;
            }
            ASSERT(i == test_size + 1);
            mv_clear(&tt);
            ASSERT(mv_size(&tt) == test_size);
            i = 1;
            for (mv_it_t *it = mv_begin(&tt); it != mv_end(&tt); it = mv_next(it)) {
                ASSERT(mv_at(&tt, i-1) == NULL);
                i++;
            }
            ASSERT(i == test_size + 1);
        }

    }
    fprintf(stderr, "pass\n");
}

static void
mv_alt_configs(void)
{
    fprintf(stderr, "Test: mv functions with alternate configurations...");
    {
        mva_t *tt = mva_new(~0u, 0);
        ASSERT(strcmp(mva_push_back(tt, "abcd"), "abcd") == 0);
        ASSERT(strcmp(mva_back(tt), "abcd") == 0);
        ASSERT(mva_pop_back(tt) != NULL);
        mva_push_back(tt, "abcd");
        ASSERT(strcmp(mva_setval(mva_begin(tt), "abcde"), "abcde") == 0);
        ASSERT(strcmp(mva_back(tt), "abcde") == 0);
        mva_clear(tt);
        mva_push_back(tt, "abcd");
        mva_delete(tt);
    }
    fprintf(stderr, "pass\n");
}

int
main(void)
{
    mv_basic_tests();
    mv_alt_configs();
    return 0;
}
