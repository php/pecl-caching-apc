/* 
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "apc_compile.h"
#include "apc_globals.h"
#include "apc_zend.h"

typedef void* (*ht_copy_fun_t)(void*, void*, apc_malloc_t);
typedef void  (*ht_free_fun_t)(void*, apc_free_t);

#define CHECK(p) { if ((p) == NULL) return NULL; }

/* {{{ internal function declarations */

static int is_derived_class(zend_op_array* op_array, const char* key, int key_size);

static zend_function* my_bitwise_copy_function(zend_function*, zend_function*, apc_malloc_t);

/*
 * The "copy" functions perform deep-copies on a particular data structure
 * (passed as the second argument). They also optionally allocate space for
 * the destination data structure if the first argument is null.
 */
static zval** my_copy_zval_ptr(zval**, zval**, apc_malloc_t);
static zval* my_copy_zval(zval*, zval*, apc_malloc_t);
static znode* my_copy_znode(znode*, znode*, apc_malloc_t);
static zend_op* my_copy_zend_op(zend_op*, zend_op*, apc_malloc_t);
static zend_function* my_copy_function(zend_function*, zend_function*, apc_malloc_t);
static zend_function_entry* my_copy_function_entry(zend_function_entry*, zend_function_entry*, apc_malloc_t);
static zend_class_entry* my_copy_class_entry(zend_class_entry*, zend_class_entry*, apc_malloc_t);
static HashTable* my_copy_hashtable(HashTable*, HashTable*, ht_copy_fun_t, int, apc_malloc_t);
static HashTable* my_copy_static_variables(zend_op_array* src, apc_malloc_t allocate);

/*
 * The "destroy" functions free the memory associated with a particular data
 * structure but do not free the pointer to the data structure.
 */
static void my_destroy_zval_ptr(zval**, apc_free_t);
static void my_destroy_zval(zval*, apc_free_t);
static void my_destroy_zend_op(zend_op*, apc_free_t);
static void my_destroy_znode(znode*, apc_free_t);
static void my_destroy_function(zend_function*, apc_free_t);
static void my_destroy_function_entry(zend_function_entry*, apc_free_t);
static void my_destroy_class_entry(zend_class_entry*, apc_free_t);
static void my_destroy_hashtable(HashTable*, ht_free_fun_t, apc_free_t);
static void my_destroy_op_array(zend_op_array*, apc_free_t);

/*
 * The "free" functions work exactly like their "destroy" counterparts (see
 * above) but also free the pointer to the data structure.
 */
static void my_free_zval_ptr(zval**, apc_free_t);
static void my_free_function(zend_function*, apc_free_t);
static void my_free_hashtable(HashTable*, ht_free_fun_t, apc_free_t);

/* }}} */

/* {{{ check_op_array_integrity */
#if 0
static void check_op_array_integrity(zend_op_array* src)
{
    int i, j;

    /* These sorts of checks really aren't particularly effective, but they
     * can provide a welcome sanity check when debugging. Just don't enable
     * for production use!  */

    assert(src->refcount != NULL);
    assert(src->opcodes != NULL);
    assert(src->last > 0);

    for (i = 0; i < src->last; i++) {
        zend_op* op = &src->opcodes[i];
        znode* nodes[] = { &op->result, &op->op1, &op->op2 };
        for (j = 0; j < 3; j++) {
            assert(nodes[j]->op_type == IS_CONST ||
                   nodes[j]->op_type == IS_VAR ||
                   nodes[j]->op_type == IS_TMP_VAR ||
                   nodes[j]->op_type == IS_UNUSED);

            if (nodes[j]->op_type == IS_CONST) {
                int type = nodes[j]->u.constant.type;
                assert(type == IS_RESOURCE ||
                       type == IS_BOOL ||
                       type == IS_LONG ||
                       type == IS_DOUBLE ||
                       type == IS_NULL ||
                       type == IS_CONSTANT ||
                       type == IS_STRING ||
                       type == FLAG_IS_BC ||
                       type == IS_ARRAY ||
                       type == IS_CONSTANT_ARRAY ||
                       type == IS_OBJECT);
            }
        }
    }
}
#endif
/* }}} */

/* {{{ is_derived_class */
static int is_derived_class(zend_op_array* op_array, const char* key, int key_size)
{
    int i;

    /*
     * Scan the op_array for execution-time class declarations of derived
     * classes. If we find one whose key matches our current class key, we
     * know the current class is a derived class.
     *
     * This check is exceedingly inefficient (fortunately it only has to occur
     * once, when the source file is first compiled and cached), but the
     * compiler should save this information for us -- definitely a candidate
     * for a Zend Engine patch.
     *
     * XXX checking for derived classes provides a minimal (albeit measurable)
     * speed up. It may not be worth the added complexity -- considere
     * removing this optimization.
     */

    for (i = 0; i < op_array->last; i++) {
        zend_op* op = &op_array->opcodes[i];

        if (op->opcode == ZEND_DECLARE_FUNCTION_OR_CLASS &&
            op->extended_value == ZEND_DECLARE_INHERITED_CLASS)
        {
            if (op->op1.u.constant.value.str.len == key_size &&
                !memcmp(op->op1.u.constant.value.str.val, key, key_size))
            {
                return 1;
            }
        }
    }

    return 0;
}
/* }}} */

/* {{{ my_bitwise_copy_function */
static zend_function* my_bitwise_copy_function(zend_function* dst, zend_function* src, apc_malloc_t allocate)
{
    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_function*) allocate(sizeof(src[0])));
    }

    /* We only need to do a bitwise copy */
    memcpy(dst, src, sizeof(src[0]));

    return dst;
}
/* }}} */

/* {{{ my_copy_zval_ptr */
static zval** my_copy_zval_ptr(zval** dst, zval** src, apc_malloc_t allocate)
{
    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zval**) allocate(sizeof(zval*)));
    }

    CHECK(dst[0] = (zval*) allocate(sizeof(zval)));
    my_copy_zval(*dst, *src, allocate);
    return dst;
}
/* }}} */

/* {{{ my_copy_zval */
static zval* my_copy_zval(zval* dst, zval* src, apc_malloc_t allocate)
{
    assert(dst != NULL);
    assert(src != NULL);

    memcpy(dst, src, sizeof(src[0]));

    switch (src->type) {
    case IS_RESOURCE:
    case IS_BOOL:
    case IS_LONG:
    case IS_DOUBLE:
    case IS_NULL:
        break;

    case IS_CONSTANT:
    case IS_STRING:
    case FLAG_IS_BC:
        if (src->value.str.val)
            CHECK(dst->value.str.val = apc_xmemcpy(src->value.str.val,
                                                   src->value.str.len+1,
                                                   allocate));
        break;
    
    case IS_ARRAY:
    case IS_CONSTANT_ARRAY:
        CHECK(dst->value.ht =
            my_copy_hashtable(NULL,
                              src->value.ht,
                              (ht_copy_fun_t) my_copy_zval_ptr,
                              1,
                              allocate));
        break;

    case IS_OBJECT:
        CHECK(dst->value.obj.ce =
            my_copy_class_entry(NULL, src->value.obj.ce, allocate));

        CHECK(dst->value.obj.properties =
            my_copy_hashtable(NULL,
                              src->value.obj.properties,
                              (ht_copy_fun_t) my_copy_zval_ptr,
                              1,
                              allocate));
        break;

    default:
        assert(0);
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_znode */
static znode* my_copy_znode(znode* dst, znode* src, apc_malloc_t allocate)
{
    assert(dst != NULL);
    assert(src != NULL);

    memcpy(dst, src, sizeof(src[0]));

    assert(dst ->op_type == IS_CONST ||
           dst ->op_type == IS_VAR ||
           dst ->op_type == IS_TMP_VAR ||
           dst ->op_type == IS_UNUSED);

    if (src->op_type == IS_CONST) {
        my_copy_zval(&dst->u.constant, &src->u.constant, allocate);
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_zend_op */
static zend_op* my_copy_zend_op(zend_op* dst, zend_op* src, apc_malloc_t allocate)
{
    assert(dst != NULL);
    assert(src != NULL);

    memcpy(dst, src, sizeof(src[0]));
    my_copy_znode(&dst->result, &src->result, allocate);
    my_copy_znode(&dst->op1, &src->op1, allocate);
    my_copy_znode(&dst->op2, &src->op2, allocate);

    return dst;
}
/* }}} */

/* {{{ my_copy_function */
static zend_function* my_copy_function(zend_function* dst, zend_function* src, apc_malloc_t allocate)
{
    assert(src != NULL);

    CHECK(dst = my_bitwise_copy_function(dst, src, allocate));

    switch (src->type) {
    case ZEND_INTERNAL_FUNCTION:
    case ZEND_OVERLOADED_FUNCTION:
        assert(0);
        
    case ZEND_USER_FUNCTION:
    case ZEND_EVAL_CODE:
        CHECK(apc_copy_op_array(&dst->op_array,
                                &src->op_array,
                                allocate));
        break;

    default:
        assert(0);
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_function_entry */
static zend_function_entry* my_copy_function_entry(zend_function_entry* dst, zend_function_entry* src, apc_malloc_t allocate)
{
    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_function_entry*) allocate(sizeof(src[0])));
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(src[0]));

    if (src->fname) {
        CHECK(dst->fname = apc_xstrdup(src->fname, allocate));
    }

    if (src->func_arg_types) {
        CHECK(dst->func_arg_types = apc_xmemcpy(src->func_arg_types,
                                                src->func_arg_types[0]+1,
                                                allocate));
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_class_entry */
static zend_class_entry* my_copy_class_entry(zend_class_entry* dst, zend_class_entry* src, apc_malloc_t allocate)
{
    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_class_entry*) allocate(sizeof(src[0])));
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(src[0]));

    if (src->name) {
        CHECK(dst->name = apc_xstrdup(src->name, allocate));
    }

    CHECK(dst->refcount = apc_xmemcpy(src->refcount,
                                      sizeof(src->refcount[0]),
                                      allocate));

    CHECK(my_copy_hashtable(&dst->function_table,
                            &src->function_table,
                            (ht_copy_fun_t) my_copy_function,
                            0,
                            allocate));

    CHECK(my_copy_hashtable(&dst->default_properties,
                            &src->default_properties,
                            (ht_copy_fun_t) my_copy_zval_ptr,
                            1,
                            allocate));

    if (src->builtin_functions) {
        int i, n;

        for (n = 0; src->builtin_functions[n].fname != NULL; n++) {}

        CHECK(dst->builtin_functions =
            (zend_function_entry*)
                allocate((n + 1) * sizeof(zend_function_entry)));

        for (i = 0; i < n; i++) {
            my_copy_function_entry(&dst->builtin_functions[i],
                                   &src->builtin_functions[i],
                                   allocate);
        }

        dst->builtin_functions[n].fname = NULL;
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_hashtable */
static HashTable* my_copy_hashtable(HashTable* dst,
                                    HashTable* src,
                                    ht_copy_fun_t copy_fn,
                                    int holds_ptrs,
                                    apc_malloc_t allocate)
{
    Bucket* curr = NULL;
    Bucket* prev = NULL;
    Bucket* newp = NULL;
    int first = 1;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (HashTable*) allocate(sizeof(src[0])));
    }

    memcpy(dst, src, sizeof(src[0]));

    /* allocate buckets for the new hashtable */
    CHECK(dst->arBuckets = allocate(dst->nTableSize * sizeof(Bucket*)));
    memset(dst->arBuckets, 0, dst->nTableSize * sizeof(Bucket*));
    dst->pInternalPointer = NULL;

    for (curr = src->pListHead; curr != NULL; curr = curr->pListNext) {
        int n = curr->h % dst->nTableSize;

        /* create a copy of the bucket 'curr' */
        CHECK(newp =
            (Bucket*) apc_xmemcpy(curr,
                                  sizeof(Bucket) + curr->nKeyLength - 1,
                                  allocate));

        /* insert 'newp' into the linked list at its hashed index */
        if (dst->arBuckets[n]) {
            newp->pNext = dst->arBuckets[n];
            newp->pLast = NULL;
            newp->pNext->pLast = newp;
        }
        else {
            newp->pNext = newp->pLast = NULL;
        }

        dst->arBuckets[n] = newp;

        /* copy the bucket data using our 'copy_fn' callback function */
        CHECK(newp->pData = copy_fn(NULL, curr->pData, allocate));

        if (holds_ptrs) {
            memcpy(&newp->pDataPtr, newp->pData, sizeof(void*));
        }
        else {
            newp->pDataPtr = NULL;
        }

        /* insert 'newp' into the table-thread linked list */
        newp->pListLast = prev;
        newp->pListNext = NULL;

        if (prev) {
            prev->pListNext = newp;
        }

        if (first) {
            dst->pListHead = newp;
            first = 0;
        }

        prev = newp;
    }

    dst->pListTail = newp;
    return dst;
}
/* }}} */

/* {{{ my_copy_static_variables */
static HashTable* my_copy_static_variables(zend_op_array* src, apc_malloc_t allocate)
{ 
    if (src->static_variables == NULL) {
        return NULL;
    }

    return my_copy_hashtable(NULL,
                             src->static_variables,
                             (ht_copy_fun_t) my_copy_zval_ptr,
                             1,
                             allocate);
}
/* }}} */

/* {{{ apc_copy_op_array */
zend_op_array* apc_copy_op_array(zend_op_array* dst, zend_op_array* src, apc_malloc_t allocate)
{
    int i;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_op_array*) allocate(sizeof(src[0])));
    }
    if(APCG(optimization)) {
        apc_optimize_op_array(src);
    }

    /* start with a bitwise copy of the array */
    memcpy(dst, src, sizeof(src[0]));

    /* copy the arg types array (if set) */
    if (src->arg_types) {
        CHECK(dst->arg_types =
            apc_xmemcpy(src->arg_types,
                        sizeof(src->arg_types[0]) * (src->arg_types[0]+1),
                        allocate));
    }

    if (src->function_name) {
        CHECK(dst->function_name = apc_xstrdup(src->function_name, allocate));
    }
    if (src->filename) {
        CHECK(dst->filename = apc_xstrdup(src->filename, allocate));
    }

    CHECK(dst->refcount = apc_xmemcpy(src->refcount,
                                      sizeof(src->refcount[0]),
                                      allocate));

    /* deep-copy the opcodes */
    CHECK(dst->opcodes = (zend_op*) allocate(sizeof(zend_op) * src->last));
    for (i = 0; i < src->last; i++) {
        CHECK(my_copy_zend_op(dst->opcodes+i, src->opcodes+i, allocate));
    }

    /* copy the break-continue array */
    if (src->brk_cont_array) {
        CHECK(dst->brk_cont_array =
            apc_xmemcpy(src->brk_cont_array,
                        sizeof(src->brk_cont_array[0]) * src->last_brk_cont,
                        allocate));
    }

    /* copy the table of static variables */
    if (src->static_variables) {
        CHECK(dst->static_variables = my_copy_static_variables(src, allocate));
    }

    return dst;
}
/* }}} */

/* {{{ apc_copy_new_functions */
apc_function_t* apc_copy_new_functions(int old_count, apc_malloc_t allocate)
{
    apc_function_t* array;
    int new_count;              /* number of new functions in table */
    int i;

    new_count = zend_hash_num_elements(CG(function_table)) - old_count;
    assert(new_count >= 0);

    CHECK(array =
        (apc_function_t*)
            allocate(sizeof(apc_function_t) * (new_count+1)));

    if (new_count == 0) {
        array[0].function = NULL;
        return array;
    }
    
    /* Skip the first `old_count` functions in the table */
    zend_hash_internal_pointer_reset(CG(function_table));
    for (i = 0; i < old_count; i++) {
        zend_hash_move_forward(CG(function_table));
    }

    /* Add the next `new_count` functions to our array */
    for (i = 0; i < new_count; i++) {
        char* key;
        uint key_size;
        zend_function* fun;

        zend_hash_get_current_key_ex(CG(function_table),
                                     &key,
                                     &key_size,
                                     NULL,
                                     0,
                                     NULL);

        zend_hash_get_current_data(CG(function_table), (void**) &fun);

        CHECK(array[i].name = apc_xmemcpy(key, (int) key_size, allocate));
        array[i].name_len = (int) key_size-1;
        CHECK(array[i].function = my_copy_function(NULL, fun, allocate));
        zend_hash_move_forward(CG(function_table));
    }

    array[i].function = NULL;
    return array;
}
/* }}} */

/* {{{ apc_copy_new_classes */
apc_class_t* apc_copy_new_classes(zend_op_array* op_array, int old_count, apc_malloc_t allocate)
{
    apc_class_t* array;
    int new_count;              /* number of new classs in table */
    int i;

    new_count = zend_hash_num_elements(CG(class_table)) - old_count;
    assert(new_count >= 0);

    CHECK(array =
        (apc_class_t*)
            allocate(sizeof(apc_class_t)*(new_count+1)));
    
    if (new_count == 0) {
        array[0].class_entry = NULL;
        return array;
    }

    /* Skip the first `old_count` classs in the table */
    zend_hash_internal_pointer_reset(CG(class_table));
    for (i = 0; i < old_count; i++) {
        zend_hash_move_forward(CG(class_table));
    }

    /* Add the next `new_count` classs to our array */
    for (i = 0; i < new_count; i++) {
        char* key;
        uint key_size;
        zend_class_entry* elem;

        array[i].class_entry = NULL;

        zend_hash_get_current_key_ex(CG(class_table),
                                     &key,
                                     &key_size,
                                     NULL,
                                     0,
                                     NULL);

        zend_hash_get_current_data(CG(class_table), (void**) &elem);

        CHECK(array[i].name = apc_xmemcpy(key, (int) key_size, allocate));
        array[i].name_len = (int) key_size-1;
        CHECK(array[i].class_entry = my_copy_class_entry(NULL, elem, allocate));

        /*
         * If the class has a pointer to its parent class, save the parent
         * name so that we can enable compile-time inheritance when we reload
         * the child class; otherwise, set the parent name to null and scan
         * the op_array to determine if this class inherits from some base
         * class at execution-time.
         */

        if (elem->parent) {
            CHECK(array[i].parent_name =
                apc_xstrdup(elem->parent->name, allocate));
            array[i].is_derived = 1;
        }
        else {
            array[i].parent_name = NULL;
            array[i].is_derived = is_derived_class(op_array, key, key_size);
        }

        zend_hash_move_forward(CG(class_table));
    }

    array[i].class_entry = NULL;
    return array;
}
/* }}} */

/* {{{ my_destroy_zval_ptr */
static void my_destroy_zval_ptr(zval** src, apc_free_t deallocate)
{
    assert(src != NULL);
    my_destroy_zval(src[0], deallocate);
    deallocate(src[0]);
}
/* }}} */

/* {{{ my_destroy_zval */
static void my_destroy_zval(zval* src, apc_free_t deallocate)
{
    switch (src->type) {
    case IS_RESOURCE:
    case IS_BOOL:
    case IS_LONG:
    case IS_DOUBLE:
    case IS_NULL:
        break;

    case IS_CONSTANT:
    case IS_STRING:
    case FLAG_IS_BC:
        deallocate(src->value.str.val);
        break;
    
    case IS_ARRAY:
    case IS_CONSTANT_ARRAY:
        my_free_hashtable(src->value.ht,
                          (ht_free_fun_t) my_free_zval_ptr,
                          deallocate);
        break;

    case IS_OBJECT:
        my_destroy_class_entry(src->value.obj.ce, deallocate);
        deallocate(src->value.obj.ce);
        my_free_hashtable(src->value.obj.properties,
                          (ht_free_fun_t) my_free_zval_ptr,
                          deallocate);
        break;

    default:
        assert(0);
    }
}
/* }}} */

/* {{{ my_destroy_znode */
static void my_destroy_znode(znode* src, apc_free_t deallocate)
{
    if (src->op_type == IS_CONST) {
        my_destroy_zval(&src->u.constant, deallocate);
    }
}
/* }}} */

/* {{{ my_destroy_zend_op */
static void my_destroy_zend_op(zend_op* src, apc_free_t deallocate)
{
    my_destroy_znode(&src->result, deallocate);
    my_destroy_znode(&src->op1, deallocate);
    my_destroy_znode(&src->op2, deallocate);
}
/* }}} */

/* {{{ my_destroy_function */
static void my_destroy_function(zend_function* src, apc_free_t deallocate)
{
    assert(src != NULL);

    switch (src->type) {
    case ZEND_INTERNAL_FUNCTION:
    case ZEND_OVERLOADED_FUNCTION:
        assert(0);
        
    case ZEND_USER_FUNCTION:
    case ZEND_EVAL_CODE:
        my_destroy_op_array(&src->op_array, deallocate);
        break;

    default:
        assert(0);
    }
}
/* }}} */

/* {{{ my_destroy_function_entry */
static void my_destroy_function_entry(zend_function_entry* src, apc_free_t deallocate)
{
    assert(src != NULL);

    deallocate(src->fname);
    if (src->func_arg_types) {
        deallocate(src->func_arg_types);
    }
}
/* }}} */

/* {{{ my_destroy_class_entry */
static void my_destroy_class_entry(zend_class_entry* src, apc_free_t deallocate)
{
    int n;
    int i;

    assert(src != NULL);

    deallocate(src->name);
    deallocate(src->refcount);

    my_destroy_hashtable(&src->function_table,
                         (ht_free_fun_t) my_free_function,
                         deallocate);

    my_destroy_hashtable(&src->default_properties,
                         (ht_free_fun_t) my_free_zval_ptr,
                         deallocate);

    if (src->builtin_functions) {
        for (i = 0; src->builtin_functions[n].fname != NULL; i++) {
            my_destroy_function_entry(&src->builtin_functions[i], deallocate);
        }
        deallocate(src->builtin_functions);
    }
}
/* }}} */

/* {{{ my_destroy_hashtable */
static void my_destroy_hashtable(HashTable* src, ht_free_fun_t free_fn, apc_free_t deallocate)
{
    int i;

    assert(src != NULL);

    for (i = 0; i < src->nTableSize; i++) {
        Bucket* p = src->arBuckets[i];
        while (p != NULL) {
            Bucket* q = p;
            p = p->pNext;
            free_fn(q->pData, deallocate);
            deallocate(q);
        }
    }

    deallocate(src->arBuckets);
}
/* }}} */

/* {{{ my_destroy_op_array */
static void my_destroy_op_array(zend_op_array* src, apc_free_t deallocate)
{
    int i;

    assert(src != NULL);

    if (src->arg_types) {
        deallocate(src->arg_types);
    }

    deallocate(src->function_name);
    deallocate(src->filename);
    deallocate(src->refcount);

    for (i = 0; i < src->last; i++) {
        my_destroy_zend_op(src->opcodes + i, deallocate);
    }
    deallocate(src->opcodes);

    if (src->brk_cont_array) {
        deallocate(src->brk_cont_array);
    }

    if (src->static_variables) {
        my_free_hashtable(src->static_variables,
                          (ht_free_fun_t) my_free_zval_ptr,
                          deallocate);
    }
}
/* }}} */

/* {{{ my_free_zval_ptr */
static void my_free_zval_ptr(zval** src, apc_free_t deallocate)
{
    my_destroy_zval_ptr(src, deallocate);
    deallocate(src);
}
/* }}} */

/* {{{ my_free_function */
static void my_free_function(zend_function* src, apc_free_t deallocate)
{
    my_destroy_function(src, deallocate);
    deallocate(src);
}
/* }}} */

/* {{{ my_free_hashtable */
static void my_free_hashtable(HashTable* src, ht_free_fun_t free_fn, apc_free_t deallocate)
{
    my_destroy_hashtable(src, free_fn, deallocate);
    deallocate(src);
}
/* }}} */

/* {{{ apc_free_op_array */
void apc_free_op_array(zend_op_array* src, apc_free_t deallocate)
{
    if (src != NULL) {
        my_destroy_op_array(src, deallocate);
        deallocate(src);
    }
}
/* }}} */

/* {{{ apc_free_functions */
void apc_free_functions(apc_function_t* src, apc_free_t deallocate)
{
    int i;

    if (src != NULL) {
        for (i = 0; src[i].function != NULL; i++) {
            deallocate(src[i].name);
            my_destroy_function(src[i].function, deallocate);
            deallocate(src[i].function);
        }   
        deallocate(src);
    }   
}
/* }}} */

/* {{{ apc_free_classes */
void apc_free_classes(apc_class_t* src, apc_free_t deallocate)
{
    int i;

    if (src != NULL) {
        for (i = 0; src[i].class_entry != NULL; i++) {
            deallocate(src[i].name);
            deallocate(src[i].parent_name);
            my_destroy_class_entry(src[i].class_entry, deallocate);
            deallocate(src[i].class_entry);
        }   
        deallocate(src);
    }   
}
/* }}} */

/* {{{ apc_copy_op_array_for_execution */
zend_op_array* apc_copy_op_array_for_execution(zend_op_array* src)
{
    zend_op_array* dst = (zend_op_array*) emalloc(sizeof(src[0]));
    memcpy(dst, src, sizeof(src[0]));
    dst->static_variables = my_copy_static_variables(src, apc_php_malloc);
    /*check_op_array_integrity(dst);*/
    return dst;
}
/* }}} */

/* {{{ apc_copy_function_for_execution */
zend_function* apc_copy_function_for_execution(zend_function* src)
{
    zend_function* dst = (zend_function*) emalloc(sizeof(src[0]));
    memcpy(dst, src, sizeof(src[0]));
    dst->op_array.static_variables = my_copy_static_variables(&dst->op_array, apc_php_malloc);
    /*check_op_array_integrity(&dst->op_array);*/
    return dst;
}
/* }}} */

/* {{{ apc_copy_function_for_execution_ex */
zend_function* apc_copy_function_for_execution_ex(void *dummy, zend_function* src)
{
    return apc_copy_function_for_execution(src);
}
/* }}} */

/* {{{ apc_copy_class_entry_for_execution */
zend_class_entry* apc_copy_class_entry_for_execution(zend_class_entry* src, int is_derived)
{
    zend_class_entry* dst = (zend_class_entry*) emalloc(sizeof(src[0]));
    memcpy(dst, src, sizeof(src[0]));

    /* Deep-copy the class properties, because they will be modified */

    my_copy_hashtable(&dst->default_properties,
                      &src->default_properties,
                      (ht_copy_fun_t) my_copy_zval_ptr,
                      1,
                      apc_php_malloc);

    /* For derived classes, we must also copy the function hashtable (although
     * we can merely bitwise copy the functions it contains) */

    my_copy_hashtable(&dst->function_table,
                      &src->function_table,
                      (ht_copy_fun_t) apc_copy_function_for_execution_ex,
                      0,
                      apc_php_malloc);

    return dst;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
