/*
 * Copyright (c) 2011 - 2012, 2022 Xarepo AB. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */

#ifndef MC_TMPL_ONCE_
#define MC_TMPL_ONCE_

#include <inttypes.h> // uintptr_t etc
#include <stdlib.h> // posix_memalign()

#define MC_DECONST(type, var) ((type)(uintptr_t)(const void *)(var))

#define MC_CACHE_LINE_SIZE 64

#define MC_MM_COMPACT 0x1
#define MC_MM_STATIC 0x2
#define MC_MM_PERFORMANCE 0x4

#define MC_CONCAT_EVAL_(a, b) a ## b
#define MC_CONCAT_(a, b) MC_CONCAT_EVAL_(a, b)
#define MC_FUN_(name) MC_CONCAT_(MC_CONCAT_(MC_PREFIX, _), name)

#endif // MC_TMPL_ONCE_

#ifndef MC_PREFIX
 #error "MC_PREFIX not defined"
#endif

#define MC_T MC_CONCAT_(MC_PREFIX, _t)
#define MC_T_ MC_CONCAT_(MC_PREFIX, _t_)
#define MC_ITERATOR_T MC_CONCAT_(MC_PREFIX, _it_t)
#if MC_CUSTOM_ITERATOR_ - 0 == 0
 #define MC_ITERATOR_T_ MC_CONCAT_(MC_PREFIX, _it_t_)
 struct MC_ITERATOR_T_;
 typedef struct MC_ITERATOR_T_ MC_ITERATOR_T;
#endif

#if MC_SEQUENCE_CONTAINER_ - 0 != 0

    #ifdef MC_KEY_T
     #error "MC_KEY_T should not be defined for a sequence container"
    #endif

#elif MC_ASSOCIATIVE_CONTAINER_ - 0 != 0

    #ifndef MC_KEY_T
     #error "MC_KEY_T not defined"
    #endif

    #if MC_NEED_KEY_UNDEFINED_ - 0 != 0
     #ifndef MC_KEY_UNDEFINED
      #error "MC_KEY_UNDEFINED not defined"
     #endif
    #endif

    #if MC_NEED_KEY_DIFFERENT_FROM_UNDEFINED_ - 0 != 0
     #ifndef MC_KEY_DIFFERENT_FROM_UNDEFINED
      #error "MC_KEY_DIFFERENT_FROM_UNDEFINED not defined"
     #endif

     #define MC_DEF_KEY_DIFF_UNDEF_                                          \
         MC_KEY_T const diff_undef_key = MC_KEY_DIFFERENT_FROM_UNDEFINED
    #endif

    #ifdef MC_COPY_KEY
     #define MC_ASSIGN_KEY_(dest, src) MC_COPY_KEY(dest, src)
    #else
     #define MC_ASSIGN_KEY_(dest, src) dest = src
    #endif

    #ifdef MC_FREE_KEY
     #define MC_OPT_FREE_KEY_(key) MC_FREE_KEY(key)
    #else
     #define MC_OPT_FREE_KEY_(key)
    #endif

#else
    #error "Unspecified container type - bug in container template"
#endif

#ifndef MC_VALUE_UNDEFINED
    #if MC_NO_VALUE - 0 == 0 || !defined(MC_KEY_UNDEFINED)
     #define MC_VALUE_UNDEFINED (MC_VALUE_T MC_OPT_PTR_)0
    #else
     #define MC_VALUE_UNDEFINED MC_KEY_UNDEFINED
    #endif
#endif

#define MC_DEF_VALUE_UNDEF_ \
    MC_VALUE_T MC_OPT_PTR_ const undef_value = MC_VALUE_UNDEFINED
#define MC_DEF_KEY_UNDEF_ \
    MC_KEY_T const undef_key = MC_KEY_UNDEFINED

#if MC_NO_VALUE - 0 == 0
    #ifndef MC_VALUE_T
     #error "MC_VALUE_T not defined"
    #endif
#else
    #define MC_VALUE_NO_INSERT_ARG 1
    #define MC_VALUE_T MC_KEY_T
#endif

#if MC_VALUE_RETURN_REF - 0 != 0
    #define MC_OPT_PTR_ *
    #define MC_OPT_ADDROF_ &
    #define MC_OPT_DREF_
#else
    #define MC_OPT_PTR_
    #define MC_OPT_ADDROF_
    #define MC_OPT_DREF_ *
#endif

#if MC_VALUE_NO_INSERT_ARG - 0 != 0
    #define MC_OPT_VALUE_INSERT_ARG_
    #define MC_OPT_ASSIGN_VALUE_(dest, src)
    #define MC_OPT_FREE_VALUE_(val)
    #define MC_OPT_VALUE_PARAM_
    #if MC_VALUE_RETURN_REF - 0 == 0 && MC_NO_VALUE - 0 == 0
     #error "MC_VALUE_NO_INSERT_ARG=1 requires MC_VALUE_RETURN_REF=1"
    #endif
    #ifdef MC_COPY_VALUE
     #error "MC_VALUE_NO_INSERT_ARG=1 and MC_COPY_VALUE defined simultaneously"
    #endif
#else
    #ifdef MC_COPY_VALUE
     #define MC_OPT_ASSIGN_VALUE_(dest, src) MC_COPY_VALUE(dest, src)
     #define MC_OPT_VALUE_INSERT_ARG_ , const MC_VALUE_T const value
     #define MC_OPT_VALUE_PARAM_ , value
    #else
     #define MC_OPT_ASSIGN_VALUE_(dest, src) dest = src
     #define MC_OPT_VALUE_INSERT_ARG_ , MC_VALUE_T const value
     #define MC_OPT_VALUE_PARAM_ , value
    #endif
    #ifdef MC_FREE_VALUE
     #define MC_OPT_FREE_VALUE_(val) MC_FREE_VALUE(val)
    #else
     #define MC_OPT_FREE_VALUE_(val)
    #endif
#endif

#if !defined(MC_MM_SUPPORT_)
 #error "Unspecified MM mode support"
#endif
#if !defined(MC_MM_DEFAULT_)
 #error "Unspecified default MM mode"
#endif
#if !defined(MC_MM_MODE)
 #define MC_MM_MODE MC_MM_DEFAULT_
#endif
#if (MC_MM_MODE & MC_MM_SUPPORT_) == 0
 #error "Container does not support the specified MC_MM_MODE."
#endif
#if MC_MM_MODE == MC_MM_PERFORMANCE
 #ifndef MC_MM_BLOCK_SIZE
  #define MC_MM_BLOCK_SIZE MC_MM_DEFAULT_BLOCK_SIZE_
 #endif
#endif
