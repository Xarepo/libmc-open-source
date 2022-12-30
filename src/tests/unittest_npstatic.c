/*
 * Copyright (c) 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <string.h>

#include <unittest_helpers.h>

#define MC_SEQUENCE_CONTAINER_ 1
#define MC_MM_MODE MC_MM_STATIC
#define MC_VALUE_T uintptr_t
#define MC_PREFIX t1
#define MC_MM_DEFAULT_ MC_MM_STATIC
#define MC_MM_SUPPORT_ MC_MM_STATIC
#include <mc_tmpl.h>
#define NPSTATIC_NODE_TYPE uintptr_t
#include <npstatic_tmpl.h>

static uint32_t taus_state[3];

static void
npstatic_tests(void)
{
    // Note: npstatic has no boundary checking as it's designed to be used with a container which has checking

    fprintf(stderr, "Test: npstatic functions...");
    {
        const int test_size = 1000;
        uintptr_t *values = calloc(sizeof(uintptr_t), test_size);
        struct npstatic np;

        t1_npstatic_init(&np, values);

        for (int iteration = 0; iteration < 3; iteration++) {
            void *nodes[test_size];
            memset(nodes, 0, sizeof(nodes[0]) * test_size);

            for (int i = 0; i < test_size; i++) {
                nodes[i] = t1_npstatic_alloc(&np);
                *(uintptr_t *)nodes[i] = ~((uintptr_t)0);

                if (tausrand(taus_state) % 10 == 0) {
                    int free_iterations = tausrand(taus_state) % 10 + 1;
                    for (int k = 0; k < free_iterations; k++) {
                        int pos = tausrand(taus_state) % (i + 1);
                        if (nodes[pos] != NULL) {
                            *(uintptr_t *)nodes[pos] = 0;
                            t1_npstatic_free(&np, nodes[pos]);
                            nodes[pos] = NULL;
                        }
                    }
                }
            }

            ASSERT(np.start_ptr == (uintptr_t)values);
            if (iteration == 0) {
                t1_npstatic_clear(&np);
                ASSERT(np.freelist == NULL);
                ASSERT(np.fresh_ptr == np.start_ptr);
            } else {
                for (int i = 0; i < test_size; i++) {
                    if (nodes[i] != NULL) {
                        t1_npstatic_free(&np, nodes[i]);
                    }
                }
                ASSERT(np.freelist != NULL);
            }
        }
        free(values);
    }
    fprintf(stderr, "pass\n");

    fprintf(stderr, "Test: fill/empty freelist...");
    {
        const int test_size = 1000;
        uintptr_t *values = calloc(sizeof(uintptr_t), test_size);
        struct npstatic np;

        t1_npstatic_init(&np, values);

        void *nodes[test_size];
        memset(nodes, 0, sizeof(nodes[0]) * test_size);

        for (int iteration = 0; iteration < 3; iteration++) {
            for (int i = 0; i < test_size; i++) {
                nodes[i] = t1_npstatic_alloc(&np);
            }
            ASSERT(np.freelist == NULL);
            ASSERT(np.start_ptr == (uintptr_t)values);
            ASSERT(np.fresh_ptr == np.start_ptr + test_size * sizeof(values[0]));
            for (int i = 0; i < test_size; i++) {
                if (nodes[i] != NULL) {
                    t1_npstatic_free(&np, nodes[i]);
                }
            }
            ASSERT(np.freelist != NULL);
            ASSERT(np.start_ptr == (uintptr_t)values);
            ASSERT(np.fresh_ptr == np.start_ptr + test_size * sizeof(values[0]));
        }
        free(values);
    }
    fprintf(stderr, "pass\n");
}

int
main(void)
{
    tausrand_init(taus_state, 0);
    npstatic_tests();
    return 0;
}
