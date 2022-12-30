/*
 * Copyright (c) 2012 - 2013 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  A few examples of more advanced use of the containers by using custom
  configuration and combining. This is by no means an exhaustive list of
  examples, there is much more you can do.

  Read the specific template header of the container you are interested
  in to see what is possible to do with it.
*/


/*

  Example 1: combining containers

  Say you want to have elements in a specific order (sequence container) but
  at the same time want to be able to quickly find a specific element in it
  (associative container), you can.

  Just use a list for storing and use a red-black tree for finding.

*/

struct example_1_value {
    int data[16];
};


/* Define a list that stores the values directly in the nodes, but returns
   pointer to the values, which is typically more practical for large struct
   values. We also store a reference to the corresponding tree node to be able
   to make erases without needing to search in the tree. Note that it in this
   special case it would be more practical if also the sequence containers were
   split into key+value so we would not have to make this wrapper struct, but
   such a design of sequence containers to support only this special case is
   not worthwhile.

   Since the ex1mp_it_t iterator type is not typedef'd until we declare the
   next data type, we need to use a forward declaration, and for that we need
   to use the special dummy struct that exists just for this purpose,
   struct ex1mp_it_t_ will be the same as ex1mp_it_t after we declare tree
   further down. */
struct ex1mp_it_t_;
struct ex1ls_value {
    struct example_1_value val;
    struct ex1mp_it_t_ *mp_ref;
};
/* As an example we enable the performance memory model, which is good for the
   case when you have not too many instances of the data structure, but each
   instance can be large. Nodes are allocated in bulk which is one of the tricks
   used to gain performance, but that also gives more overhead per instance. */
#define MC_MM_MODE MC_MM_PERFORMANCE
/* Each container have a default value for block size when block/bulk
   allocation of nodes is used (i e performance mode), but it can also be
   overriden like this by specifying MC_MM_BLOCK_SIZE, which should be a value
   in bytes power-of-two, the reason for overriding the default would typically
   to specify a larger value if we know that there will be very few and very
   large instances. */
#define MC_MM_BLOCK_SIZE 16384
#define MC_PREFIX ex1ls
#define MC_VALUE_T struct ex1ls_value
#define MC_VALUE_RETURN_REF 1
#define MC_VALUE_NO_INSERT_ARG 1
#include <mld_tmpl.h>

/* define a tree that uses the example_1_value as key, and use references to
   the list as values. We don't store the keys in the tree to avoid duplicate
   storage, but instead point to the values stored directly in the list. */
#define MC_MM_MODE MC_MM_PERFORMANCE
#define MC_PREFIX ex1mp
#define MC_KEY_T const struct example_1_value *
#define MC_VALUE_T ex1ls_it_t *
#define MRB_KEYCMP(result, key1, key2) \
    result = memcmp(key1, key2, sizeof(struct example_1_value))
#include <mrb_tmpl.h>

static void
example_1(void)
{
    ex1ls_t *ls = ex1ls_new(~0);
    ex1mp_t *mp = ex1mp_new(~0);
    struct ex1ls_value *val;

    /* you can of course make wrapping functions of this combined container so
       it becomes a bit simpler to use */

    /* example insert */
    for (int i = 0; i < 50; i++) {
        val = ex1ls_push_front(ls);
        /* we pushed to the front, so the corresponding iterator will be the
           first */
        ex1ls_it_t *ls_ref = ex1ls_begin(ls);
        for (int j = 0;
             j < sizeof(val->val.data)/sizeof(val->val.data[0]);
             j++)
        {
            val->val.data[j] = random();
        }
        val->mp_ref = ex1mp_itinsert(mp, &val->val, ls_ref);
    }

    /* example erase, note that we erase from the tree first to avoid accessing
       freed memory */
    for (int i = 0; i < 10; i++) {
        ex1ls_it_t *ls_ref = ex1ls_begin(ls);
        val = ex1ls_val(ls_ref);
        ex1mp_iterase(mp, val->mp_ref);
        ex1ls_erase(ls, ls_ref);
    }

    /* example erase when finding the element in the tree. Since we have node
       references in both directions, erases are efficient regardless if we
       find the element via the list or the tree. */
    for (int i = 0; i < 10; i++) {
        ex1mp_it_t *mp_ref = ex1mp_begin(mp);
        ex1ls_it_t *ls_ref = ex1mp_val(mp_ref);
        ex1mp_iterase(mp, mp_ref);
        ex1ls_erase(ls, ls_ref);
    }

    ex1mp_delete(mp);
    ex1ls_delete(ls);
}

/*

  Example 2: using static storage

  In some cases you may want to have pre-allocated nodes, to avoid calls to
  a node allocator (typically malloc is used). In real-time software this can
  be useful or when performance is very important.

 */

/* Set MC_MM_MODE to static so we get a static version of the red-black tree. */
#define MC_MM_MODE MC_MM_STATIC
#include <mrb_tmpl.h>
/* The queue only supports static allocation so we don't need to specify the
   MC_MM_MODE here. */
#include <mq_tmpl.h>

static struct {
    /* Red-black tree. We put the nodes directly after the base structure, the
       red-black tree does not really require that but it is a good convention.
       We allocate 1024 nodes, which means that the tree cannot contain more
       nodes than that. */
    mrb_t mrb;
    struct mrb_node_kv mrb_nodes[1024];
    /* Lifo/fifo queue, this container actually supports only pre-allocation,
       and the value array must be directly after the base struct, and the
       value array size must be a power of two even if the capacity is not
       going to be. */
    mq_t mq;
    void *mq_values[1024];
    int member_a;
    int member_b;
    int member_c;
} ex_2;

static void
example_2(void)
{
    /* Initialize the red-black tree and the queue in the static struct. There
       are no static initializers to use if you want them to be allocated from
       start, if so you need to make use of constructors, a feature existing in
       most modern C compilers. */
    mrb_init(&ex_2.mrb, ex_2.mrb_nodes, sizeof(ex_2.mrb_nodes));
    mq_init(&ex_2.mq, ex_2.mq_values, 1000, sizeof(ex_2.mq_values));

    /* Although the lifo/fifo queue only supports pre-allocation, it does not
       need to be in a static struct, you can pre-allocate the whole structure
       using a new call, but note that all nodes are pre-allocated so you must
       limit the size of it. */
    mq_t *mq = mq_new(1000);

    mq_delete(mq);
}

/*

  Example 3: using complex key and value alloc/free functions

  Associative containers are often used as lookup structures for elements
  allocated elsewhere and point directly to them, but sometimes it is
  preferable that the container itself allocates and frees the keys and values.

  In this example we show how to use custom allocate and free functions in a
  case when the keys/values are more complex than simple statically sized
  structs.

*/

#include <string.h>

struct example_3_value {
    void *alloc_member1;
    size_t alloc_member1_size;
    void *alloc_member2;
    size_t alloc_member2_size;
};

static struct example_3_value *
ex3_copy_value(const struct example_3_value *src)
{
    struct example_3_value *dst = malloc(sizeof(*dst));
    dst->alloc_member1 = malloc(src->alloc_member1_size);
    memcpy(dst->alloc_member1, src->alloc_member1, src->alloc_member1_size);
    dst->alloc_member1_size = src->alloc_member1_size;
    dst->alloc_member2 = malloc(src->alloc_member2_size);
    memcpy(dst->alloc_member2, src->alloc_member2, src->alloc_member2_size);
    dst->alloc_member2_size = src->alloc_member2_size;
    return dst;
}

static void
ex3_free_value(struct example_3_value *val)
{
    free(val->alloc_member1);
    free(val->alloc_member2);
    free(val);
}

#define MC_PREFIX ex3
/* Const values and keys are often beneficial, it indicates that the container
   itself will not modify incoming keys/values and it gives some protection
   that the user of the container does not do the same. However, to be able
   to free the values later on we need to "deconst" the pointers which we do
   using the MC_DECONST() macro. */
#define MC_KEY_T const char *
#define MC_VALUE_T const struct example_3_value *
#define MC_COPY_KEY(dest, src) dest = strdup(src)
#define MC_FREE_KEY(key) free(MC_DECONST(void *, key))
/* To avoid code bloat, we make a function calls for the copy/free functions
   rather than complex macros. */
#define MC_COPY_VALUE(dest, src) dest = ex3_copy_value(src)
#define MC_FREE_VALUE(value) \
    ex3_free_value(MC_DECONST(struct example_3_value *, value))
/* Keys are strings in this example, so we need to provide a comparison macro,
   which calls the strcmp() function. The mrb_tmpl.h header describes the
   properties the MRB_KEYCMP() macro needs to fulfil. */
#define MRB_KEYCMP(result, key1, key2) result = strcmp(key1, key2)
#include <mrb_tmpl.h>

static void
example_3(void)
{
    ex3_t *ex3 = ex3_new(~0);
    struct example_3_value val;
    const struct example_3_value *pval;
    uint8_t alloc_members[2][10];
    val.alloc_member1 = alloc_members[0];
    val.alloc_member1_size = sizeof(alloc_members[0]);
    val.alloc_member2 = alloc_members[0];
    val.alloc_member2_size = sizeof(alloc_members[0]);

    /* example insert, both key and value will be copied. */
    ex3_insert(ex3, "key1", &val);

    /* example find, the copied value is returned. */
    pval = ex3_find(ex3, "key1");
    (void)pval;

    /* example erase, the copied keys and values are freed. */
    ex3_erase(ex3, "key1");

    ex3_delete(ex3);
}

int
main(void)
{
    example_1();
    example_2();
    example_3();

    return 0;
}
