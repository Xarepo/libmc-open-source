/*
 * Copyright (c) 2012 - 2013, 2022 Xarepo. All rights reserved.
 *
 * This program is open source under the ISC License.
 *
 */
#include <stdio.h>
#include <alloca.h>
#include <stdlib.h>
#include <string.h>

/*
  This simple include with no pre-definitions will define the default mrb
  datatype which is a map with 'intptr_t' keys to 'void *', with NULL as
  undefined value. It is implemented as a red-black tree.

  The red-black tree is the type of map with best all-around properties. 90%
  of the time you need a map (associative container) you will want to use this.
*/
#include <mrb_tmpl.h>

struct value {
    int member1;
    int member2;
};

/*
  This include shows basic configuration of the red-black tree, a new set of
  functions with int2val_ prefix for a map with int to struct value mapping, and
  the pointer to the value will be returned in the function returning values
  (enabled by MC_VALUE_RETURN_REF 1). The insert function will not take a value
  argument (MC_VALUE_NO_INSERT_ARG 1), but instead the allocated value is
  returned for the user to fill in.
 */
#define MC_PREFIX int2val
#define MC_KEY_T int
#define MC_VALUE_T struct value
#define MC_VALUE_RETURN_REF 1
#define MC_VALUE_NO_INSERT_ARG 1
#include <mrb_tmpl.h>

/*
  This include shows how the red-black tree is configured to not have any
  values, (or only values depending on how you see it), and thus it works like a
  searchable set of values (keys).
 */
#define MC_PREFIX refset
#define MC_KEY_T void *
#define MC_NO_VALUE 1
#include <mrb_tmpl.h>

static void
mrb_examples(void)
{

    {
        /* capacity given as a parameter to new() parameter, ~0u means unlimited
           (until memory runs out). */
        mrb_t *tt = mrb_new(~0u);

        int key;
        int values[50];
        memset(values, 0, sizeof(values));

        /* Demonstrate insert function by inserting a set of dummy key/value
           pairs. */
        for (int i = 0; i < 50; i++) {
            key = i;
            mrb_insert(tt, key, &values[i]);
        }

        /* Demonstrate the find and erase functions */
        if (mrb_find(tt, 5) != &values[5]) {
            /* this would be a bug */
            abort();
        }
        mrb_erase(tt, 5);
        if (mrb_find(tt, 5) != NULL) {
            /* this would be a bug */
            abort();
        }

        /* Demonstrate use of iterators when traversing the container */
        for (mrb_it_t *it = mrb_begin(tt);
             it != mrb_end(); it = mrb_next(it))
        {
            printf("%zd -> %d\n", mrb_key(it), *(int *)mrb_val(it));
        }

        /* Going through container backwards */
        for (mrb_it_t *it = mrb_rbegin(tt);
             it != mrb_rend(); it = mrb_prev(it))
        {
            printf("%zd -> %d\n", mrb_key(it), *(int *)mrb_val(it));
        }

        /* Demonstrate other iterator-based functions */

        /* Insert a node, get reference to the node, and change the value
           afterwards using setval */
        mrb_it_t *it = mrb_itinsert(tt, 5, &values[4]);
        mrb_setval(it, &values[5]);

        /* Return the iterator instead of the value. */
        it = mrb_itfind(tt, 5);

        /* Erase node, this erase is efficient since there is no search required
           to find key/value to erase. */
        mrb_iterase(tt, it);

        /* Test if container is empty or not */
        if (mrb_empty(tt)) {
            /* tt is not empty, so this should not happen */
            abort();
        }

        if (mrb_size(tt) != 49) {
            /* tt should have 49 elements in it by now */
            abort();
        }

        /* Clear the container without deleting it, useful when reusing a
           container over and over again, a bit more efficient than doing
           new/delete */
        mrb_clear(tt);

        /* Delete the container and all its contents (that is not necessary
           to run clear before as in this example) */
        mrb_delete(tt);
    }

    {
        /* Demonstrate use of the generated int2val map */

        int2val_t *tt = int2val_new(~0u);
        struct value *v;
        int key;

        for (int i = 0; i < 50; i++) {
            key = i;
            /* We have configured the datatype to not take a value parameter
               for inserts, which often is useful for struct datatypes. Instead
               we fill in the value directly in place after insertion into the
               tree. */
            v = int2val_insert(tt, key);
            v->member1 = i;
            v->member2 = i;
        }

        int2val_delete(tt);
    }

    {
        /* Demonstrate use of the generated refset set */

        refset_t *tt = refset_new(~0u);

        for (intptr_t i = 0; i < 50; i++) {
            refset_insert(tt, (void *)i);
        }

        /* The find function for a set is equalt to testing if the key exists
           or not. The find function will return the same key as looked up. */
        if (refset_find(tt, (void *)5) == NULL) {
            /* We should have inserted this key above so it exists */
            abort();
        }

        refset_delete(tt);
    }
}

/*
  This include with no pre-defines will define the default dynamic array (vector)
  which contains void * values;

  A dynamic array is often the better choice over single-linked list (mls) or
  double-linked list (mld), as long as your use case works with insert/erase at
  the end of the array.
 */
#include <mv_tmpl.h>

static void
mv_examples(void)
{
    {
        /* capacity given as a parameter to new() parameter, ~0u means unlimited
           (until memory runs out). One can also specify how many values should
           be pre-allocated at first, examplified with 16 here. */
        mv_t *tt = mv_new(~0u, 16);

        int values[50];
        memset(values, 0, sizeof(values));

        /* Demonstrate push. */
        for (int i = 0; i < 50; i++) {
            /* pushing to the back of the vector - since it is just a single
               block of memory we can only push at the back */
            mv_push_back(tt, &values[i]);
        }

        void *value;
        for (int i = 0; i < 50; i++) {
            /* popping from the back of the vector */
            value = mv_pop_back(tt);
        }
        (void)value;

        /* peek at the front or back of the vector */
        value = mv_front(tt);
        value = mv_back(tt);

        /* Demonstrate use of iterators when traversing the container */
        for (mv_it_t *it = mv_begin(tt); it != mv_end(tt); it = mv_next(it)) {
            printf("%d\n", *(int *)mv_val(it));
        }

        /* Going through container backwards */
        for (mv_it_t *it = mv_rbegin(tt); it != mv_rend(tt); it = mv_prev(it)) {
            printf("%d\n", *(int *)mv_val(it));
        }

        /* Reserve a specific size (works like C++ std::vector::reserve) */
        mv_reserve(tt, 100);

        // Accessing elements using index
        for (int i = 0; i < mv_size(tt); i++) {
            // when using mv_data() to get direct access, no boundary checking is performed
            value = mv_data(tt)[i];

            // mv_at() will perform boundary check
            value = mv_at(tt, i);
        }

        // Demonstration of danger of storing mv_data() array for later use
        {
            void **array = mv_data(tt);
            mv_push_back(tt, (void *)0x1);
            // access to 'array' here after modifying the vector is illegal as the
            // array may have been reallocated to a different address.
            (void)array;
        }

        mv_delete(tt);
    }
}


/*
  This include with no pre-defines will define the default double-linked list,
  that is a list with void * values.
 */
#include <mld_tmpl.h>

static void
mld_examples(void)
{

    {
        /* capacity given as a parameter to new() parameter, ~0u means unlimited
           (until memory runs out). */
        mld_t *tt = mld_new(~0u);

        int values[50];
        memset(values, 0, sizeof(values));

        /* Demonstrate push and pop. */
        for (int i = 0; i < 50; i++) {
            /* pushing to the front of the list */
            mld_push_front(tt, &values[i]);

            /* pushing to the back of the list */
            mld_push_back(tt, &values[i]);
        }

        void *value;
        for (int i = 0; i < 25; i++) {
            /* popping from the front of the list */
            value = mld_pop_front(tt);

            /* popping from the back of the list */
            value = mld_pop_back(tt);
        }
        (void)value;

        /* peek at the front or back of the list */
        value = mld_front(tt);
        value = mld_back(tt);

        /* In the mrb example we demonstrated the mrb_find() function, but there
           is actually no mld_find(), why? Because a list is not a suitable
           data-structure for searches. If you need searches, use an associative
           container, the 'refset' mrb example show when it is configured as a
           set, that is only keys and no values, which is typically what you
           need if you need to store a bunch of values in no particular order
           but you need to search in it.
        */

        /* Demonstrate use of iterators when traversing the container */
        for (mld_it_t *it = mld_begin(tt);
             it != mld_end(); it = mld_next(it))
        {
            printf("%d\n", *(int *)mld_val(it));
        }

        /* Going through container backwards */
        for (mld_it_t *it = mld_rbegin(tt);
             it != mld_rend(); it = mld_prev(it))
        {
            printf("%d\n", *(int *)mld_val(it));
        }

        {
            /* Get reference to first node (mld_begin()) and then insert a value
               before it (mld_insert()). */
            mld_it_t *it = mld_begin(tt);
            mld_insert(tt, it, &values[0]);

            /* Erase an element from the list using iterator. */
            mld_erase(tt, it);
        }

        /* Going through the list while erasing some elements. Note that not
           all containers support this type of operation, some containers
           will invalidate the iterator when the container is modified in some
           way. */
        for (mld_it_t *it = mld_begin(tt); it != mld_end(); ) {
            if ((random() & 1) != 0) {
                it = mld_erase(tt, it);
            } else {
                it = mld_next(it);
            }
        }

        mld_delete(tt);
    }

}

/*
  This include with no pre-defines will define the default single-linked list,
  that is a list with void * values.
 */
#include <mls_tmpl.h>

static void
mls_examples(void)
{
    {
        /* capacity given as a parameter to new() parameter, ~0u means unlimited
           (until memory runs out). */
        mls_t *tt = mls_new(~0u);

        int values[50];
        memset(values, 0, sizeof(values));

        /* Demonstrate push and pop. A single-linked list is typically used as
           a stack (LIFO queue) */
        for (int i = 0; i < 50; i++) {
            /* pushing to the front of the list - since it is single-linked
               we can only push at the front, there is no push_back() like
               on the double-linked list */
            mls_push_front(tt, &values[i]);
        }

        void *value;
        for (int i = 0; i < 50; i++) {
            /* popping from the front of the list */
            value = mls_pop_front(tt);
        }
        (void)value;

        /* peek at the front of the list */
        value = mls_front(tt);

        /* Demonstrate use of iterators when traversing the container. Since
           it is single-linked, we can only traverse it from the front to back.
        */
        for (mls_it_t *it = mls_begin(tt);
             it != mls_end(); it = mls_next(it))
        {
            printf("%d\n", *(int *)mls_val(it));
        }

        {
            /* Get reference to first node (mls_begin()) and then insert a value
               after it (mls_insert_after()). We can't insert a value before the
               iterator like in the double-linked list, because there is no link
               pointing to the previous element, only the next. */
            mls_it_t *it = mls_begin(tt);
            mls_insert_after(tt, it, &values[0]);

            /* Erase an element after the current using iterator. This might seem
               an odd operation, but since it is a single-linked list we cannot
               erase the current element - there's no link to the previous. */
            mls_erase_after(tt, it);
        }

        /* Going through the list while erasing some elements. Since there
           only is an erase_after() function we need to keep the previous
           iterator. Note that we set initial value to mls_end(). The
           erase_after() function will erase the head if an empty iterator
           (mls_end()) is given as argument. */
        mls_it_t *prev_it = mls_end();
        for (mls_it_t *it = mls_begin(tt); it != mls_end(); ) {
            if ((random() & 1) != 0) {
                /* Erase current element. Since there's only an erase_after()
                   function we need to use the previous iterator. */
                mls_erase_after(tt, prev_it);
                it = prev_it;
            } else {
                it = mls_next(it);
            }
        }

        mls_delete(tt);
    }
}

/*
  This include with the MC_VALUE_T set to int will create a queue type for
  integers. We name it 'intq' by setting MC_PREFIX. The queue is a statically
  allocated structure and is thus less flexible than a double-linked list which
  also can be used as a queue. This queue uses less clock cycles though, so when
  performance is important it may be wiser to use this container.
 */
#define MC_VALUE_T int
#define MC_PREFIX intq
#include <mq_tmpl.h>

static void
mq_examples(void)
{
    {
        /* The mq is based on a static array and thus supports only static
           allocation, which means we need to specify the size when we create
           the queue. If you need a dynamically sized queue the single-linked
           or double-linked lists should be used instead. The queue size can
           be set to any value, but a power-of-two size will always be
           allocated (more optimized queue), so a power-of-two size utilizes
           the space ideally. */
        intq_t *tt = intq_new(64);

        int values[50];
        memset(values, 0, sizeof(values));

        /* Demonstrate push and pop. The queue has like the double-linked list
           functions to access both from the front and the back of the queue,
           so it can be used both as a LIFO and a FIFO queue. */
        for (int i = 0; i < 50; i++) {
            /* pushing to the front of the queue */
            intq_push_front(tt, values[i]);

            /* pushing to the back of the queue */
            intq_push_back(tt, values[i]);
        }

        int value;
        for (int i = 0; i < 25; i++) {
            /* popping from the front of the queue */
            value = intq_pop_front(tt);

            /* popping from the back of the queue */
            value = intq_pop_back(tt);
        }
        (void)value;

        /* peek at the front or back of the list */
        value = intq_front(tt);
        value = intq_back(tt);

        /* Demonstrate use of iterators when traversing the container. Note
           that this container unlike the double-linked list for example
           requires the container as an argument to some of the iterator
           functions. Containers designed around arrays typically need that. */
        for (intq_it_t *it = intq_begin(tt);
             it != intq_end(tt); it = intq_next(it))
        {
            printf("%d\n", intq_val(tt, it));
        }

        /* Going through container backwards */
        for (intq_it_t *it = intq_rbegin(tt);
             it != intq_rend(tt); it = intq_prev(it))
        {
            printf("%d\n", intq_val(tt, it));

            /* demonstrate the setval() function, which sets the value at the
               position specified with the iterator */
            intq_setval(tt, it, 1);
        }

        /* Unlike a list, there is no erase functions, the only way to remove
           elements is to pop elements from the front or back of the queue. */

        intq_delete(tt);
    }
}

/*
  The hash-table is single-hashing open-adressed, which means that it can
  do very fast lookups, but in poor conditions (poor hash function, many
  collisions) performance may be really bad. This means that it should only
  be used in well-controlled situations when you know the key space and that
  the hash function will perform well. In most cases it is wiser to use a
  red-black tree instead.

  No pre-defines means intptr_t keys (-1 undefined) and void * values, with
  a Knuth multiplicative method hash function.
 */
#include <mht_tmpl.h>

static void
mht_examples(void)
{
    {
        /* Capacity given as a parameter to new() parameter, and since tables
           are statically allocated we should make as small table as
           possible. If a dynamic size is needed, employ a standard tree
           instead */
        mht_t *tt = mht_new(50);

        int key;
        int values[50];

        /* Demonstrate insert function by inserting a set of dummy key/value
           pairs. */
        for (int i = 0; i < 50; i++) {
            key = i;
            values[i] = i;
            mht_insert(tt, key, &values[i]);
        }

        /* Demonstrate the find and erase functions. If the hash function works
           well with the key set the performance of the find function will be
           very good. */
        if (mht_find(tt, 5) != &values[5]) {
            /* this would be a bug */
            abort();
        }
        /* In the erase function, rehash takes place, so it can take some
           time. In a well-hashed function only a very small portion of the
           table need to be rehashed though. */
        mht_erase(tt, 5);
        if (mht_find(tt, 5) != NULL) {
            /* this would be a bug */
            abort();
        }

        /* Traverse the table using iterator. Note that while this is possible
           to do, it is not efficient since the implementation needs to scan
           the table, looking at all empty positions too. Thus in performance-
           critical code hashtables should not be traversed. A hashtable is
           not sorted, so the elements will be returned in the order they
           happen to be hashed. There is no support to traverse backwards. */
        for (mht_it_t *it = mht_begin(tt);
             it != mht_end(tt); it = mht_next(it))
        {
            printf("%zd -> %d\n", mht_key(it), *(int *)mht_val(it));
        }

        /* Demonstrate other iterator-based functions */
        {
            /* Insert a node, get reference to the node, and change the value
               afterwards using setval */
            mht_it_t *it = mht_itinsert(tt, 5, &values[4]);
            mht_setval(it, &values[5]);

            /* Return the iterator instead of the value. */
            it = mht_itfind(tt, 5);

            /* Erase node, this erase is efficient since there is no search required
               to find key/value to erase. */
            mht_iterase(tt, it);
        }

        mht_delete(tt);
    }
}


/*
  This include with no pre-defines (except specifying the performance memory mode)
  will define the default radix tree which stores string keys and pointer values,
  NULL as undefined value.

  The radix tree is a very fast data structure, it outperforms the red-black tree
  both for string keys and integer keys. It is less flexible though and not as
  general-purpose as a red-black tree.

  When performance is important for larger search trees the radix tree is a good
  choice, especially with performance memory mode enabled.

  (Note the implementation of a fast radix tree is complex, thousands of lines of
  code, while the red-black implementation is small. So if small code size and
  low complexity is important, the red-black tree is a better choice.)
 */
#define MC_MM_MODE MC_MM_PERFORMANCE
#include <mrx_tmpl.h>

static void
mrx_examples(void)
{
    {
        // capacity given as a parameter to new() parameter, ~0u means unlimited
        // (until memory runs out).
        mrx_t *tt = mrx_new(~0u);

        char string_key[32];
        int values[50];
        memset(values, 0, sizeof(values));

        // Demonstrate insert function by inserting a set of dummy key/value pairs.
        for (int i = 0; i < 50; i++) {
            sprintf(string_key, "%d", i);
            if (i % 2 == 0) {
                mrx_insert(tt, string_key, strlen(string_key), &values[i]);
            } else {
                // if we use null-terminated strings as key, we can also insert
                // using the 'nt' version, where we don't need to provide the
                // length of the key:
                mrx_insertnt(tt, string_key, &values[i]);
            }
        }

        // Demonstrate the find and erase functions
        if (mrx_find(tt, "5", 1) != &values[5]) {
            // this would be a bug
            abort();
        }
        mrx_erasent(tt, "5");
        if (mrx_findnt(tt, "5") != NULL) {
            // this would be a bug
            abort();
        }

        // Find nearest match.
        int match_len;
        if (mrx_findnearnt(tt, "6a", &match_len) != &values[6] || match_len != 1) {
            // this would be a bug
            abort();
        }

        // Demonstrate use of iterators when traversing the container. Just like
        // a red-black tree a radix tree is sorted by nature.
        //
        // While red-black tree iterators are just a pointer to a tree node, the
        // radix tree iterator is a complex stateful object. It can be allocated
        // on the stack or heap. Here is the stack case:
        for (mrx_it_t *it = mrx_beginst(tt, alloca(mrx_itsize(tt))); it != mrx_end(); it = mrx_next(it)) {
            printf("%s -> %d\n", mrx_key(it), *(int *)mrx_val(it));
        }

        // As alloca() is not available on all systems or may not be allowed due to
        // coding style reasons, there is also a heap version. Note that we do not need
        // to delete the iterator as it's automatically deleted by mrx_next() when
        // reaching the end. However, if we break early the iterator needs to be deleted
        // with  mrx_itdelete(it);
        //
        // The iterator contains space to store the longest key that exists in the tree
        // so if the tree contains huge keys using heap allocation is preferable anyway.
        for (mrx_it_t *it = mrx_begin(tt); it != mrx_end(); it = mrx_next(it)) {
            printf("%s -> %d\n", mrx_key(it), *(int *)mrx_val(it));
        }

        // A radix tree may transform and reorganize its nodes on insert and erase,
        // meaning that iterators will go invalid as soon as the tree is modified.
        // Thus there are no rich iterator based functionality with a radix tree as
        // the more flexible red-black tree, ie no iterase() etc.

        // Test if container is empty or not
        if (mrx_empty(tt)) {
            // tt is not empty, so this should not happen
            abort();
        }

        if (mrx_size(tt) != 49) {
            // tt should have 49 elements in it by now
            abort();
        }

        // Clear the container without deleting it, useful when reusing a
        // container over and over again, a bit more efficient than doing
        // new/delete
        mrx_clear(tt);

        // Delete the container and all its contents (that is not necessary
        // to run clear before as in this example)
        mrx_delete(tt);
    }
}

int
main(void)
{
    mrb_examples();
    mv_examples();
    mld_examples();
    mls_examples();
    mq_examples();
    mht_examples();
    mrx_examples();

    return 0;
}
