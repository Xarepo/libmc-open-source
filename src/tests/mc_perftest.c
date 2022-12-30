/*
 * Copyright (c) 2013, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

/*
  This is a simple datatype performance test program
 */
#ifndef __cplusplus
#define _GNU_SOURCE // for CPU_ZERO() etc.
#endif
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <ctype.h>

#include <bitops.h>
#include <unittest_helpers.h>

#ifndef timersub
#define timersub(a, b, result)                                                \
  do {                                                                        \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                             \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                          \
    if ((result)->tv_usec < 0) {                                              \
      --(result)->tv_sec;                                                     \
      (result)->tv_usec += 1000000;                                           \
    }                                                                         \
  } while (0)
#endif

static int
uint64_cmp(const void *a,
           const void *b)
{
    if (*(const uint64_t *)a < *(const uint64_t *)b) {
        return -1;
    } else if (*(const uint64_t *)a > *(const uint64_t *)b) {
        return 1;
    }
    return 0;
}

static unsigned int
measure_timestamp_overhead(void)
{
    uint64_t t1, t2, min;
    unsigned int i;

    min = ~0;
    for (i = 0; i < 10000; i++) {
        timestamp(&t1);
        timestamp(&t2);
        t2 -= t1;
        if (t2 < min) {
            min = t2;
        }
    }
    return (unsigned int)min;
}

#define get_process_stats(a)
#define print_process_stats_diff(a, b)
#if 0
static void
get_process_stats(struct rusage *ru)
{
    getrusage(0, ru);
}

static void
print_process_stats_diff(const struct rusage *a, const struct rusage *b)
{
    struct timeval utime;
    struct timeval stime;
    timersub(&b->ru_utime, &a->ru_utime, &utime);
    timersub(&b->ru_stime, &a->ru_stime, &stime);
    fprintf(stderr, "diff ru_utime %ld %ld\n", utime.tv_sec, utime.tv_usec);
    fprintf(stderr, "diff ru_stime %ld %ld\n", stime.tv_sec, stime.tv_usec);
    fprintf(stderr, "diff ru_maxrss %ld\n", b->ru_maxrss - a->ru_maxrss);
    fprintf(stderr, "diff ru_ixrss %ld\n", b->ru_ixrss - a->ru_ixrss);
    fprintf(stderr, "diff ru_idrss %ld\n", b->ru_idrss - a->ru_idrss);
    fprintf(stderr, "diff ru_isrss %ld\n", b->ru_isrss - a->ru_isrss);
    fprintf(stderr, "diff ru_minflt %ld\n", b->ru_minflt - a->ru_minflt);
    fprintf(stderr, "diff ru_majflt %ld\n", b->ru_majflt - a->ru_majflt);
    fprintf(stderr, "diff ru_nswap %ld\n", b->ru_nswap - a->ru_nswap);
    fprintf(stderr, "diff ru_inblock %ld\n", b->ru_inblock - a->ru_inblock);
    fprintf(stderr, "diff ru_oublock %ld\n", b->ru_oublock - a->ru_oublock);
    fprintf(stderr, "diff ru_msgsnd %ld\n", b->ru_msgsnd - a->ru_msgsnd);
    fprintf(stderr, "diff ru_msgrcv %ld\n", b->ru_msgrcv - a->ru_msgrcv);
    fprintf(stderr, "diff ru_nsignals %ld\n", b->ru_nsignals - a->ru_nsignals);
    fprintf(stderr, "diff ru_nvcsw %ld\n", b->ru_nvcsw - a->ru_nvcsw);
    fprintf(stderr, "diff ru_nivcsw %ld\n", b->ru_nivcsw - a->ru_nivcsw);
}
#endif

static void
write_tsa(uint64_t tsa[],
          unsigned int count,
          const char filename[])
{
    uint64_t min, max, tot, avg, avg_p99, mean, p90, p99;
    unsigned int i;

    /*
    FILE *stream;
    if ((stream = fopen(filename, "wt")) == NULL) {
        fprintf(stderr, "Could not open \"%s\" for writing: %s.\n", filename, strerror(errno));
        return;
    }
    */

    min = ~0;
    max = 0;
    tot = 0;
    for (i = 0; i < count; i++) {
        //fprintf(stream, "%llu\n", (unsigned long long)tsa[i]);
        if (tsa[i] > 9223372036854775807) {
            // overflow due to time smaller than measured timestamp overhead: snap to 0
            tsa[i] = 0;
        }
        if (tsa[i] < min) {
            min = tsa[i];
        }
        if (tsa[i] > max) {
            max = tsa[i];
        }
        tot += tsa[i];
    }
    //fclose(stream);

    qsort(tsa, count, sizeof(uint64_t), uint64_cmp);
    avg = tot / count;
    mean = tsa[count / 2];
    p90 = tsa[(90 * count) / 100];
    p99 = tsa[(99 * count) / 100];

    // avg_p99: average with p99 excluded
    while (tsa[count-1] >= p99) {
        tot -= tsa[count-1];
        count--;
    }
    avg_p99 = tot / count;

    fprintf(stderr, "%20s:\tavg %5lld; avg99 %5lld; mean %5lld; p90 %5lld; p99 %5lld; "
            "min %5lld; max %8lld; tot %10lld\n",
            filename,
            (unsigned long long)avg, (unsigned long long)avg_p99, (unsigned long long)mean,
            (unsigned long long)p90, (unsigned long long)p99,
            (unsigned long long)min, (unsigned long long)max,
            (unsigned long long)tot);
}

#ifdef __cplusplus
extern "C" {
#endif
    int branch_predictor_messup(void);
#ifdef __cplusplus
}
#endif

static inline void
clflush(const void *p)
{
#if __has_builtin(__builtin_ia32_clflush)
    __builtin_ia32_clflush(p);
#else
    fprintf(stderr, "clflush not available\n");
    abort();
#endif
}

static void
remove_workset_from_cache(void)
{
    // can be implemented with clflush and a nonportable way to get all memory
    // maps
    //__builtin___clear_cache();
#ifdef __linux__
    FILE* stream = fopen("/proc/sys/vm/drop_caches", "w");
    if (stream == NULL) {
        fprintf(stderr, "failed to open /proc/sys/vm/drop_caches for writing: %s\n", strerror(errno));
        abort();
    }
    fprintf(stream, "1");
    fclose(stream);
#else
    fprintf(stderr, "not implemented!\n");
    abort();
#endif
}

enum set_type {
    SET_TYPE_RANDOM,
    SET_TYPE_LINEAR,
    SET_TYPE_HILINEAR
};

static const char *
set_type_toa(enum set_type set_type)
{
    switch (set_type) {
    case SET_TYPE_RANDOM: return "random";
    case SET_TYPE_LINEAR: return "linear";
    case SET_TYPE_HILINEAR: return "hilinear";
    }
    return "UNKNOWN";
}

static uint32_t taus_state[3];

static int
random_cmp(const void *a,
           const void *b)
{
    return (tausrand(taus_state) & 1) == 0 ? 1 : -1;
}

static uintptr_t *
generate_set(unsigned int set_size,
             enum set_type set_type)
{
    uintptr_t i, j, shift;
    uintptr_t *set;
    tausrand_init(taus_state, 0);
    set = (uintptr_t *)calloc(1, set_size * sizeof(*set));
    switch (set_type) {
    case SET_TYPE_RANDOM:
        j = 1;
        for (i = 0; i < set_size; i++) {
            set[i] = j;
#if ARCH_SIZEOF_PTR == 4
            /* Not complete random over full 32 bit range, since we want to
               allow for a - b type of key comparisons without overflow */
            j += 1 + (tausrand(taus_state) & 0xF);
#else
            j += 1 + tausrand(taus_state) +
                ((uintptr_t)tausrand(taus_state) << 30);
#endif
        }
        qsort(set, set_size, sizeof(uintptr_t), random_cmp);
        break;
    case SET_TYPE_LINEAR:
        for (i = 0; i < set_size; i++) {
            set[i] = i + 1;
        }
        break;
    case SET_TYPE_HILINEAR:
        for (i = set_size, shift = 0; i != 0; i >>= 1, shift++);
        for (i = 0; i < set_size; i++) {
            set[i] = (i + 1) << shift;
        }
        break;
    }
    return set;
}

#ifdef PERFTEST_MRB
#define MC_MM_MODE MC_MM_PERFORMANCE
#define MC_PREFIX mrb
#define MC_KEY_T uintptr_t
#define MC_VALUE_T void *
#include <mrb_tmpl.h>

#define TESTTYPE_NAME "mrb"
#define TESTTYPE_INIT(base_key_count, iter_count) \
   mrb_t *tt = mrb_new(base_key_count + iter_count + 1)
#define TESTTYPE_INSERT(key) mrb_insert(tt, key, (void *)(uintptr_t)key)
#define TESTTYPE_FIND(ret, key) ret = mrb_find(tt, key)
#define TESTTYPE_ERASE(key) mrb_erase(tt, key)
#define TESTTYPE_DELETE() mrb_delete(tt)

#endif

#ifdef PERFTEST_MRB_STR
#define STRING_KEYS 1
#define MC_MM_MODE MC_MM_PERFORMANCE
#define MC_PREFIX mrb
#define MC_KEY_T const char *
#define MC_VALUE_T void *
#define MC_COPY_KEY(dest, src) dest = strdup(src)
#define MC_FREE_KEY(key) free(MC_DECONST(void *, key))
#define MRB_KEYCMP(result, key1, key2) result = strcmp(key1, key2)
#include <mrb_tmpl.h>

#define TESTTYPE_NAME "mrb_str"
#define TESTTYPE_INIT(base_key_count, iter_count) \
   mrb_t *tt = mrb_new(base_key_count + iter_count + 1)
#define TESTTYPE_INSERT(key) mrb_insert(tt, string_key, (void *)(uintptr_t)key)
#define TESTTYPE_FIND(ret, key) ret = mrb_find(tt, string_key)
#define TESTTYPE_ERASE(key) mrb_erase(tt, string_key)
#define TESTTYPE_DELETE() mrb_delete(tt)

#endif // PERFTEST_MRB_STR

#ifdef PERFTEST_MHT
#define MC_PREFIX mht
#define MC_KEY_T uintptr_t
#define MC_KEY_UNDEFINED ((intptr_t)-1)
#define MC_KEY_DIFFERENT_FROM_UNDEFINED 0
#define MC_VALUE_T void *
#define MHT_HASHFUNC MHT_HASHFUNC_PTR
#include <mht_tmpl.h>

#define TESTTYPE_NAME "mht"
#define TESTTYPE_INIT(base_key_count, iter_count) \
   mht_t *tt = mht_new(base_key_count + iter_count + 1)
#define TESTTYPE_INSERT(key) mht_insert(tt, key, (void *)(uintptr_t)key)
#define TESTTYPE_FIND(ret, key) ret = mht_find(tt, key)
#define TESTTYPE_ERASE(key) mht_erase(tt, key)
#define TESTTYPE_DELETE() mht_delete(tt)

#endif // PERFTEST_MHT

#ifdef PERFTEST_MRX_INT
#define MC_MM_MODE MC_MM_PERFORMANCE
#define MC_PREFIX mrx
#define MC_KEY_T uintptr_t
#define MC_VALUE_T void *
#define MRX_KEY_SORTINT 1
#include <mrx_tmpl.h>

#define TESTTYPE_NAME "mrx"
#define TESTTYPE_INIT(base_key_count, iter_count) \
   mrx_t *tt = mrx_new(base_key_count + iter_count + 1)
#define TESTTYPE_INSERT(key) mrx_insert(tt, key, (void *)(uintptr_t)key)
#define TESTTYPE_FIND(ret, key) ret = mrx_find(tt, key)
#define TESTTYPE_ERASE(key) mrx_erase(tt, key)
#define TESTTYPE_DELETE() mrx_delete(tt)

#endif

#ifdef PERFTEST_MRX_INT_SLOW
#define MC_MM_MODE MC_MM_COMPACT
#define MC_PREFIX mrx
#define MC_KEY_T uintptr_t
#define MC_VALUE_T void *
#define MRX_KEY_SORTINT 1
#include <mrx_tmpl.h>

#define TESTTYPE_NAME "mrx"
#define TESTTYPE_INIT(base_key_count, iter_count) \
    mrx_disable_simd(); \
   mrx_t *tt = mrx_new(base_key_count + iter_count + 1)
#define TESTTYPE_INSERT(key) mrx_insert(tt, key, (void *)(uintptr_t)key)
#define TESTTYPE_FIND(ret, key) ret = mrx_find(tt, key)
#define TESTTYPE_ERASE(key) mrx_erase(tt, key)
#define TESTTYPE_DELETE() mrx_delete(tt)

#endif

#ifdef PERFTEST_MRX
#define STRING_KEYS 1
#define MC_MM_MODE MC_MM_PERFORMANCE
#define MC_PREFIX mrx
#define MC_KEY_T const char *
#define MC_VALUE_T void *
#define MRX_KEY_VARSIZE 1
#include <mrx_tmpl.h>

#define TESTTYPE_NAME "mrx"
#define TESTTYPE_INIT(base_key_count, iter_count) \
    mrx_t *tt = mrx_new(base_key_count + iter_count + 1)
#define TESTTYPE_INSERT(key) mrx_insertnt(tt, string_key, (void *)(uintptr_t)key)
#define TESTTYPE_FIND(ret, key) ret = mrx_findnt(tt, string_key)
#define TESTTYPE_ERASE(key) mrx_erasent(tt, string_key)
#define TESTTYPE_DELETE() mrx_delete(tt)

#endif

#ifdef PERFTEST_MRX_SLOW
#define STRING_KEYS 1
#define MC_MM_MODE MC_MM_COMPACT
#define MC_PREFIX mrx
#define MC_KEY_T const char *
#define MC_VALUE_T void *
#define MRX_KEY_VARSIZE 1
#include <mrx_tmpl.h>

#define TESTTYPE_NAME "mrx"
#define TESTTYPE_INIT(base_key_count, iter_count) \
    mrx_disable_simd(); \
    mrx_t *tt = mrx_new(base_key_count + iter_count + 1)
#define TESTTYPE_INSERT(key) mrx_insertnt(tt, string_key, (void *)(uintptr_t)key)
#define TESTTYPE_FIND(ret, key) ret = mrx_findnt(tt, string_key)
#define TESTTYPE_ERASE(key) mrx_erasent(tt, string_key)
#define TESTTYPE_DELETE() mrx_delete(tt)

#endif

#ifdef PERFTEST_JUDY_INT
#include <Judy.h>
#define TESTTYPE_NAME "Judy"
#define TESTTYPE_INIT(base_key_count, iter_count) \
    int rcint; void *pvalue, *tt = NULL
#define TESTTYPE_INSERT(key) JLI(pvalue, tt, key); *(Word_t *)pvalue = key;
#define TESTTYPE_FIND(ret, key) \
    JLG(pvalue, tt, key); ret = (pvalue == NULL) ? \
                              NULL : (void *)*(Word_t *)pvalue;
#define TESTTYPE_ERASE(key) JLD(rcint, tt, key)
static inline void
judy_sizeof_test_(void)
{
    switch(0){case 0:break;case sizeof(uintptr_t)==sizeof(Word_t):break;}
}
#endif

#ifdef PERFTEST_JUDY
#define STRING_KEYS 1
#include <Judy.h>
#define TESTTYPE_NAME "Judy"
#define TESTTYPE_INIT(base_key_count, iter_count) \
    int rcint; void *pvalue, *tt = NULL
#define TESTTYPE_INSERT(key) \
    JSLI(pvalue, tt, (uint8_t *)string_key); *(Word_t *)pvalue = key;
#define TESTTYPE_FIND(ret, key) \
    JSLG(pvalue, tt, (uint8_t *)string_key); ret = (pvalue == NULL) ?    \
                              NULL : (void *)*(Word_t *)pvalue;
#define TESTTYPE_ERASE(key) JSLD(rcint, tt, (uint8_t *)string_key)
static inline void
judy_sizeof_test_(void)
{
    switch(0){case 0:break;case sizeof(uintptr_t)==sizeof(Word_t):break;}
}
#endif

#ifdef PERFTEST_STLMAP
#include <map>
#define TESTTYPE_NAME "C++ STL map"
#define TESTTYPE_INIT(base_key_count, iter_count) std::map<uintptr_t, void *> umap
#define TESTTYPE_INSERT(key) umap[key] = (void *)key
#define TESTTYPE_FIND(ret, key) ret = umap[key]
#define TESTTYPE_ERASE(key) umap.erase(key)
#endif

#ifdef PERFTEST_STLUMAP
#include <unordered_map>
#define TESTTYPE_NAME "C++ STL unordered_map"
#define TESTTYPE_INIT(base_key_count, iter_count) std::unordered_map<uintptr_t, void *> umap
#define TESTTYPE_INSERT(key) umap[key] = (void *)key
#define TESTTYPE_FIND(ret, key) ret = umap[key]
#define TESTTYPE_ERASE(key) umap.erase(key)
#endif

static inline void
make_string_key(char str[],
                uintptr_t key,
                int should_miss)
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
    if (should_miss) {
        // add a long dummy tail to make it cost more if search algorithm looks
        // at whole string despite it should miss
        strcat(str, "012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890012345678900123456789001234567890");
    }
}

#if defined(PERFTEST_MRX_INT)
extern int mrx_debug_supress_print;
extern int mrx_debug_supress_check;
#endif

static unsigned int
test(unsigned int ts_ohd,
     unsigned int base_key_count,
     unsigned int iter_count,
     enum set_type set_type,
     int clear_cache,
     int flush_bph,
     int realtime)
{
    //struct rusage rua, rub;
#ifdef STRING_KEYS
    char string_key[1024];
#define STRINGIFY_KEY(key, should_miss) make_string_key(string_key, key, should_miss);
#else
#define STRINGIFY_KEY(key, should_miss)
#endif
    uintptr_t *ukeys1, *ukeys2, *ubasekeys;
    uint64_t t1, t2, *tsa;
    uint32_t i, dummy, *random_index;
    void *p;

    TESTTYPE_INIT(base_key_count, iter_count);

    dummy = 0;

    ukeys1 = generate_set(iter_count * 2 + base_key_count, set_type);
    ukeys2 = &ukeys1[iter_count];
    ubasekeys = &ukeys2[iter_count];

    for (i = 0; i < base_key_count; i++) {
        STRINGIFY_KEY(ubasekeys[i], 0);
        TESTTYPE_INSERT(ubasekeys[i]);
    }

#if defined(PERFTEST_MRX)
    //mrx_debug_memory_stats(&tt->mrx, 0);
    //mrx_debug_sanity_check_str2ref(&tt->mrx);
#endif
#if defined(PERFTEST_MRX_INT)
    //mrx_debug_memory_stats(&tt->mrx, sizeof(uintptr_t));
    //mrx_debug_sanity_check_int2ref(&tt->mrx);
#endif
    tsa = (uint64_t *)malloc(iter_count * sizeof(uint64_t));

    random_index = (uint32_t *)malloc(iter_count * sizeof(random_index[0]));
    for (i = 0; i < iter_count; i++) {
        random_index[i] = i;
    }
    qsort(random_index, iter_count, sizeof(uint32_t), random_cmp);

    if (realtime) {
        int ret;
        ret = mlockall(MCL_CURRENT | MCL_FUTURE);
        if (ret != 0) {
            fprintf(stderr, "could not lock memory: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        struct sched_param schp;
        memset(&schp, 0, sizeof(schp));
        schp.sched_priority = sched_get_priority_min(SCHED_FIFO);
        ret = sched_setscheduler(0, SCHED_FIFO, &schp);
        if (ret != 0) {
            fprintf(stderr, "could not enable FIFO scheduling: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "Locked memory and enabled FIFO scheduling.\n");
    }

    get_process_stats(&rua);
    for (i = 0; i < iter_count; i++) {
        if (clear_cache) {
            remove_workset_from_cache();
        }
        if (flush_bph) {
            branch_predictor_messup();
        }
        uintptr_t key = ukeys1[i];
        STRINGIFY_KEY(key, 0);
        timestamp(&t1);
        TESTTYPE_INSERT(key);
        timestamp(&t2);
        tsa[i] = t2 - t1 - ts_ohd;
    }
    get_process_stats(&rub);
    write_tsa(tsa, iter_count, "insert");
    print_process_stats_diff(&rua, &rub);
    usleep(10000);

    /*
    for (i = 0; i < iter_count; i++) {
        if (clear_cache) {
            remove_workset_from_cache();
        }
        uintptr_t key = ukeys1[i];
        timestamp(&t1);
        timestamp(&t2);
    }
    */
    //write_tsa(tsa, iter_count, "traverse");

    /* if we do not do anything with the looked up value, then there's a risk
       that code related to it is optimized away, that's why we have the dummy
       variable and return it. */
    for (i = 0; i < iter_count; i++) {
        if (clear_cache) {
            remove_workset_from_cache();
        }
        if (flush_bph) {
            branch_predictor_messup();
        }
        uintptr_t key = ukeys1[i];
        STRINGIFY_KEY(key, 0);
        timestamp(&t1);
        TESTTYPE_FIND(p, key);
        timestamp(&t2);
        tsa[i] = t2 - t1 - ts_ohd;
        dummy += (unsigned long)p;
    }
    write_tsa(tsa, iter_count, "find-hit-linear");
    usleep(10000);

    for (i = 0; i < iter_count; i++) {
        if (clear_cache) {
            remove_workset_from_cache();
        }
        if (flush_bph) {
            branch_predictor_messup();
        }
        uintptr_t key =
            ukeys1[i / ((iter_count / 10 == 0) ? 1 : iter_count / 10)];
        STRINGIFY_KEY(key, 0);
        timestamp(&t1);
        TESTTYPE_FIND(p, key);
        timestamp(&t2);
        tsa[i] = t2 - t1 - ts_ohd;
        dummy += (unsigned long)p;
    }
    write_tsa(tsa, iter_count, "find-hit-10xsame");
    usleep(10000);

    for (i = 0; i < iter_count; i++) {
        if (clear_cache) {
            remove_workset_from_cache();
        }
        if (flush_bph) {
            branch_predictor_messup();
        }
        uintptr_t key = ukeys1[iter_count - i - 1];
        STRINGIFY_KEY(key, 0);
        timestamp(&t1);
        TESTTYPE_FIND(p, key);
        timestamp(&t2);
        tsa[i] = t2 - t1 - ts_ohd;
        dummy += (unsigned long)p;
    }
    write_tsa(tsa, iter_count, "find-hit-reverse");
    usleep(10000);

    get_process_stats(&rua);
    for (i = 0; i < iter_count; i++) {
        if (clear_cache) {
            remove_workset_from_cache();
        }
        if (flush_bph) {
            branch_predictor_messup();
        }
        uintptr_t key = ukeys1[random_index[i]];
        STRINGIFY_KEY(key, 0);
        timestamp(&t1);
        TESTTYPE_FIND(p, key);
        timestamp(&t2);
        tsa[i] = t2 - t1 - ts_ohd;
        dummy += (unsigned long)p;
    }
    get_process_stats(&rub);
    write_tsa(tsa, iter_count, "find-hit-random");
    print_process_stats_diff(&rua, &rub);
    usleep(10000);

    for (i = 0; i < iter_count; i++) {
        if (clear_cache) {
            remove_workset_from_cache();
        }
        if (flush_bph) {
            branch_predictor_messup();
        }
        uintptr_t key = ukeys2[i];
        STRINGIFY_KEY(key, 1);
        timestamp(&t1);
        TESTTYPE_FIND(p, key);
        timestamp(&t2);
        tsa[i] = t2 - t1 - ts_ohd;
        dummy += (unsigned long)p;
    }
    write_tsa(tsa, iter_count, "find-miss");
    usleep(10000);

    for (i = 0; i < iter_count; i++) {
        if (clear_cache) {
            remove_workset_from_cache();
        }
        if (flush_bph) {
            branch_predictor_messup();
        }
        uintptr_t key = ukeys1[i];
        STRINGIFY_KEY(key, 0);
        timestamp(&t1);
        TESTTYPE_ERASE(key);
        timestamp(&t2);
        tsa[i] = t2 - t1 - ts_ohd;
    }
    write_tsa(tsa, iter_count, "erase-linear");
    usleep(10000);
#if defined(PERFTEST_MRX_INT)
    //mrx_debug_memory_stats(&tt->mrx, sizeof(uintptr_t));
    //mrx_debug_sanity_check_int2ref(&tt->mrx);
#endif
#if defined(PERFTEST_MRX)
    //mrx_debug_memory_stats(&tt->mrx, 0);
    //mrx_debug_sanity_check_str2ref(&tt->mrx);
#endif

    for (i = 0; i < iter_count; i++) {
        TESTTYPE_INSERT(ukeys1[i]);
    }
    for (i = 0; i < iter_count; i++) {
        if (clear_cache) {
            remove_workset_from_cache();
        }
        if (flush_bph) {
            branch_predictor_messup();
        }
        uintptr_t key = ukeys1[iter_count - i - 1];
        STRINGIFY_KEY(key, 0);
        timestamp(&t1);
        TESTTYPE_ERASE(key);
        timestamp(&t2);
        tsa[i] = t2 - t1 - ts_ohd;
    }
    write_tsa(tsa, iter_count, "erase-reverse");
    usleep(10000);
#if defined(PERFTEST_MRX_INT)
    //mrx_debug_memory_stats(&tt->mrx, sizeof(uintptr_t));
    //mrx_debug_sanity_check_int2ref(&tt->mrx);
#endif
#if defined(PERFTEST_MRX)
    //mrx_debug_memory_stats(&tt->mrx, 0);
    //mrx_debug_sanity_check_str2ref(&tt->mrx);
#endif

    for (i = 0; i < iter_count; i++) {
        TESTTYPE_INSERT(ukeys1[i]);
    }
#if defined(PERFTEST_MRX_INT)
    //mrx_debug_sanity_check_int2ref(&tt->mrx);
#endif
    get_process_stats(&rua);
    for (i = 0; i < iter_count; i++) {
        if (clear_cache) {
            remove_workset_from_cache();
        }
        if (flush_bph) {
            branch_predictor_messup();
        }
        uintptr_t key = ukeys1[random_index[i]];
        STRINGIFY_KEY(key, 0);
        timestamp(&t1);
        TESTTYPE_ERASE(key);
        timestamp(&t2);
        tsa[i] = t2 - t1 - ts_ohd;
    }
    get_process_stats(&rub);
    write_tsa(tsa, iter_count, "erase-random");
    print_process_stats_diff(&rua, &rub);
#if defined(PERFTEST_MRX_INT)
    //mrx_debug_memory_stats(&tt->mrx, sizeof(uintptr_t));
    //mrx_debug_sanity_check_int2ref(&tt->mrx);
#endif
#if defined(PERFTEST_MRX)
    //mrx_debug_memory_stats(&tt->mrx, 0);
    //mrx_debug_sanity_check_str2ref(&tt->mrx);
#endif

    {
        uint32_t tstate[3];
        tausrand_init(tstate, 0);
        uint32_t j = 0;
        for (i = 0; i < iter_count; i++) {
            if (clear_cache) {
                remove_workset_from_cache();
            }
            if (flush_bph) {
                branch_predictor_messup();
            }
            int operation = tausrand(tstate) % 100;
            uintptr_t key;
            if (operation < 50) {
                // find
                uint32_t k = tausrand(tstate) % (i+1);
                key = ukeys1[random_index[k]];
                STRINGIFY_KEY(key, 0);
                timestamp(&t1);
                TESTTYPE_FIND(p, key);
                timestamp(&t2);
                dummy += (unsigned long)p;
            } else if (operation < 80) {
                // insert
                key = ukeys1[random_index[i]];
                STRINGIFY_KEY(key, 0);
                timestamp(&t1);
                TESTTYPE_INSERT(key);
                timestamp(&t2);
            } else {
                // erase
                key = ukeys1[random_index[j]];
                j++;
                timestamp(&t1);
                TESTTYPE_ERASE(key);
                timestamp(&t2);
            }
            tsa[i] = t2 - t1 - ts_ohd;
        }
        write_tsa(tsa, iter_count, "mixed-random");
    }

    fprintf(stderr, "dummy: %ld\n", (long)dummy);

    if (realtime) {
        struct sched_param schp;

        memset(&schp, 0, sizeof(schp));
        sched_setscheduler(0, SCHED_OTHER, &schp);
    }

/*
    FILE *stream;
    if ((stream = fopen("gpscript", "wt")) == NULL) {
        fprintf(stderr, "Could not open \"gpscript\" for writing: %s.\n",
                strerror(errno));
        return 0;
    }

    fprintf(stream, "set title '%s measurements [%u base keys, "
            "%u iterations, key set type %s, %s]'\n",
            TESTTYPE_NAME,
            base_key_count, iter_count, set_type_toa(set_type),
            clear_cache ? "uncached" : "cached");
    fprintf(stream, "set grid\n");
    fprintf(stream, "set xlabel 'iteration'\n");
    fprintf(stream, "set ylabel 'execution time (clock cycles)'\n");
#if 0    
    fprintf(
        stream,
        "plot "
        "'insert' ti 'insert' w p, "
//        "'traverse' ti 'traverse' w p, "
        "'find-hit-linear' ti 'find hit linear order' w p, "
        "'find-hit-reverse' ti 'find hit reverse order' w p, "
        "'find-hit-random' ti 'find hit random order' w p, "
        "'find-miss' ti 'find miss' w p, "
        "'erase-linear' ti 'erase linear order' w p, "
        "'erase-reverse' ti 'erase reverse order' w p, "
        "'erase-random' ti 'erase random order' w p\n");
#endif
    fprintf(
        stream,
        "plot "
        "'erase-random' ti 'erase random order' w p, "
        "'insert' ti 'insert' w p, "
        "'find-miss' ti 'find miss' w p, "
        "'erase-linear' ti 'erase linear order' w p\n");

    fclose(stream);
*/

#ifdef TESTTYPE_DELETE
    TESTTYPE_DELETE();
#endif
    free(ukeys1);
    free(tsa);
    free(random_index);

    return dummy;
}

static void
grow_stack(size_t size)
{
    char *dummy;

    dummy = (char *)alloca(size);
    memset(dummy, 0, size);
}

#if TRACKMEM_DEBUG - 0 != 0
extern trackmem_t *buddyalloc_tm;
trackmem_t *buddyalloc_tm;
trackmem_t *nodepool_tm;
#endif

int
main(int argc,
     char *argv[])
{
    unsigned int dummy;
    unsigned int ts_ohd, key_count, iteration_count;
    enum set_type set_type;
    int clear_cache, flush_bph, realtime;

#if TRACKMEM_DEBUG - 0 != 0
    buddyalloc_tm = trackmem_new();
    nodepool_tm = trackmem_new();
#endif

#ifdef __linux__
    cpu_set_t cpuset;
    int cpu, ret = -1;
    for (cpu = 0, errno = EPERM; errno == EPERM; cpu++) {
        CPU_ZERO(&cpuset);
        CPU_SET(cpu, &cpuset);
        ret = sched_setaffinity(0, sizeof(cpuset), &cpuset);
        if (ret == 0) {
            break;
        }
    }
    if (ret == 0) {
        fprintf(stderr, "Locked to CPU index %d (pid %u)\n",
                cpu, (int)getpid());
    } else {
        fprintf(stderr, "Could not lock to a cpu: %s\n", strerror(errno));
    }
#endif

    grow_stack(64 * 4096); // to get better realtime properties

    fprintf(stderr, "pid %u\n", (int)getpid());

    if (argc != 7) {
        fprintf(stderr, "usage %s <base key count> <iteration count> "
                "<key set type> <uncached 0|1> <flush bph 0|1> "
                "<realtime 0|1>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    key_count = atoi(argv[1]);
    iteration_count = atoi(argv[2]);
    set_type = SET_TYPE_RANDOM;
    if (strcmp(argv[3], set_type_toa(SET_TYPE_RANDOM)) == 0) {
        set_type = SET_TYPE_RANDOM;
    } else if (strcmp(argv[3], set_type_toa(SET_TYPE_LINEAR)) == 0) {
        set_type = SET_TYPE_LINEAR;
    } else if (strcmp(argv[3], set_type_toa(SET_TYPE_HILINEAR)) == 0) {
        set_type = SET_TYPE_HILINEAR;
    } else {
        fprintf(stderr, "invalid key set type '%s', pick one of %s %s and %s\n",
                argv[3], set_type_toa(SET_TYPE_RANDOM),
                set_type_toa(SET_TYPE_LINEAR),
                set_type_toa(SET_TYPE_HILINEAR));
        exit(EXIT_FAILURE);
    }

    clear_cache = !!atoi(argv[4]);
    flush_bph = !!atoi(argv[5]);
    realtime = !!atoi(argv[6]);

    ts_ohd = measure_timestamp_overhead();
    fprintf(stderr, "clock cycle counter retrieval overhead is %u cc\n",
            ts_ohd);

    /* we just do something with 'dummy' to make sure compiler does not
       optimize away result */
    dummy = test(ts_ohd, key_count, iteration_count, set_type, clear_cache,
                 flush_bph, realtime);
    (void)dummy;

#if TRACKMEM_DEBUG - 0 != 0
    trackmem_delete(nodepool_tm);
    trackmem_delete(buddyalloc_tm);
#endif
    return 0;
}
