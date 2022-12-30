/*
 * Copyright (c) 2011 - 2012, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#ifndef NPSTATIC_TMPL_ONCE_
#define NPSTATIC_TMPL_ONCE_
#include <stddef.h>
#include <stdint.h>

struct npstatic_freenode {
    struct npstatic_freenode *next;
};

struct npstatic {
    struct npstatic_freenode *freelist;
    uintptr_t fresh_ptr;
    uintptr_t start_ptr;
};
#endif // NPSTATIC_TMPL_ONCE_

#ifndef NPSTATIC_NODE_TYPE
 #error "NPSTATIC_NODE_TYPE not defined"
#endif

static inline void
MC_FUN_(npstatic_sizeof_compile_time_test_)(void)
{
    // if this fails at compile time, the node size is too small
    switch(0){case 0:break;case sizeof(NPSTATIC_NODE_TYPE) >= sizeof(struct npstatic_freenode):break;}
}

static inline void
MC_FUN_(npstatic_init)(struct npstatic *nps,
                       NPSTATIC_NODE_TYPE *start)
{
    nps->freelist = NULL;
    nps->fresh_ptr = (uintptr_t)start;
    nps->start_ptr = (uintptr_t)start;
}

static inline void
MC_FUN_(npstatic_clear)(struct npstatic *nps)
{
    nps->freelist = NULL;
    nps->fresh_ptr = nps->start_ptr;
}

static inline NPSTATIC_NODE_TYPE *
MC_FUN_(npstatic_alloc)(struct npstatic *nps)
{
    NPSTATIC_NODE_TYPE *node;
    if (nps->freelist != NULL) {
        node = (NPSTATIC_NODE_TYPE *)nps->freelist;
        nps->freelist = nps->freelist->next;
    } else {
        node = (NPSTATIC_NODE_TYPE *)nps->fresh_ptr;
        nps->fresh_ptr += sizeof(*node);
    }
    return node;
}

static inline void
MC_FUN_(npstatic_free)(struct npstatic *nps,
                       NPSTATIC_NODE_TYPE *node)
{
    ((struct npstatic_freenode *)node)->next = nps->freelist;
    nps->freelist = (struct npstatic_freenode *)node;
}

#undef NPSTATIC_NODE_TYPE
