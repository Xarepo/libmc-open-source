#if defined(LIBMC_MINI) || defined(LIBMC_COMPACT) || defined(LIBMC_FULL)
#include <bitops.h>
#include <mht_tmpl.h>
#include <mld_tmpl.h>
#include <mls_tmpl.h>
#include <mq_tmpl.h>
#include <mrb_base.h>
#include <mrb_tmpl.h>
#include <mv_tmpl.h>
#endif

#if defined(LIBMC_COMPACT) || defined(LIBMC_FULL)

#define MC_PREFIX mldp
#define MC_VALUE_T void *
#define MC_MM_MODE MC_MM_PERFORMANCE
#include <mld_tmpl.h>

#define MC_PREFIX mlsp
#define MC_VALUE_T void *
#define MC_MM_MODE MC_MM_PERFORMANCE
#include <mls_tmpl.h>

#define MC_PREFIX mrbp
#define MC_KEY_T intptr_t
#define MC_VALUE_T void *
#define MC_MM_MODE MC_MM_PERFORMANCE
#include <mrb_tmpl.h>

#endif

#if defined(LIBMC_FULL)
#include <mrx_tmpl.h>

#define MC_PREFIX mrxp
#define MC_KEY_T const char *
#define MC_VALUE_T void *
#define MRX_KEY_VARSIZE 1
#define MC_MM_MODE MC_MM_PERFORMANCE
#include <mrx_tmpl.h>
#endif

#if defined(LIBMC_COMPACT)
static buddyalloc_t nodepool_mem_ = BUDDYALLOC_INITIALIZER(0);
buddyalloc_t *nodepool_mem = &nodepool_mem_;
#endif

int
main(void)
{
    mht_t *mht = mht_new(~0u);
    mht_insert(mht, 1, (void *)1);
    mht_delete(mht);

    mrb_t *mrb = mrb_new(~0u);
    mrb_insert(mrb, 1, (void *)1);
    mrb_delete(mrb);

    mls_t *mls = mls_new(~0u);
    mls_push_front(mls, (void *)1);
    mls_delete(mls);

    mld_t *mld = mld_new(~0u);
    mld_push_front(mld, (void *)1);
    mld_delete(mld);

    mv_t *mv = mv_new(~0u, 0);
    mv_push_back(mv, (void *)1);
    mv_delete(mv);

    mq_t *mq = mq_new(10);
    mq_push_front(mq, (void *)1);
    mq_delete(mq);

#if defined(LIBMC_COMPACT) || defined(LIBMC_FULL)
    mlsp_t *mlsp = mlsp_new(~0u);
    mlsp_push_front(mlsp, (void *)1);
    mlsp_delete(mlsp);

    mldp_t *mldp = mldp_new(~0u);
    mldp_push_front(mldp, (void *)1);
    mldp_delete(mldp);

    mrbp_t *mrbp = mrbp_new(~0u);
    mrbp_insert(mrbp, 1, (void *)1);
    mrbp_delete(mrbp);
#endif

#ifdef LIBMC_FULL
    mrx_t *mrx = mrx_new(~0u);
    mrx_insertnt(mrx, "1", (void *)1);
    mrx_delete(mrx);

    mrxp_t *mrxp = mrxp_new(~0u);
    mrxp_insertnt(mrxp, "1", (void *)1);
    mrxp_delete(mrxp);
#endif
    return 0;
}
