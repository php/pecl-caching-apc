/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2009 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
  |          Rasmus Lerdorf <rasmus@php.net>                             |
  |          Arun C. Murthy <arunc@yahoo-inc.com>                        |
  |          Gopal Vijayaraghavan <gopalv@yahoo-inc.com>                 |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$ */

#include "apc_compile.h"
#include "apc_globals.h"
#include "apc_zend.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"

#ifndef IS_CONSTANT_TYPE_MASK
#define IS_CONSTANT_TYPE_MASK (~IS_CONSTANT_INDEX)
#endif

typedef void* (*ht_copy_fun_t)(void*, void*, apc_context_t*);
//typedef void  (*ht_free_fun_t)(void*, apc_context_t*);
typedef int (*ht_check_copy_fun_t)(Bucket*, va_list);

typedef void (*ht_fixup_fun_t)(Bucket*, zend_class_entry*, zend_class_entry*);

#define CHECK(p) { if ((p) == NULL) return NULL; }

/* {{{ internal function declarations */

static zend_function* my_bitwise_copy_function(zend_function*, zend_function*, apc_context_t*);

/*
 * The "copy" functions perform deep-copies on a particular data structure
 * (passed as the second argument). They also optionally allocate space for
 * the destination data structure if the first argument is null.
 */
static zval** my_copy_zval_ptr(zval**, const zval**, apc_context_t*);
static zval* my_copy_zval(zval*, const zval*, apc_context_t*);
static znode* my_copy_znode(znode*, znode*, apc_context_t*);
static zend_op* my_copy_zend_op(zend_op*, zend_op*, apc_context_t*);
static zend_function* my_copy_function(zend_function*, zend_function*, apc_context_t*);
static zend_function_entry* my_copy_function_entry(zend_function_entry*, const zend_function_entry*, apc_context_t*);
static zend_class_entry* my_copy_class_entry(zend_class_entry*, zend_class_entry*, apc_context_t*);
static HashTable* my_copy_hashtable_ex(HashTable*, HashTable*, ht_copy_fun_t, int, apc_context_t*, ht_check_copy_fun_t, ...);
#define my_copy_hashtable( dst, src, copy_fn, holds_ptr, ctxt) \
    my_copy_hashtable_ex(dst, src, copy_fn, holds_ptr, ctxt, NULL)
static HashTable* my_copy_static_variables(zend_op_array* src, apc_context_t*);
static zend_property_info* my_copy_property_info(zend_property_info* dst, zend_property_info* src, apc_context_t*);
static zend_arg_info* my_copy_arg_info_array(zend_arg_info*, const zend_arg_info*, uint, apc_context_t*);
static zend_arg_info* my_copy_arg_info(zend_arg_info*, const zend_arg_info*, apc_context_t*);

/*
 * The "fixup" functions need for ZEND_ENGINE_2
 */
static void my_fixup_function( Bucket *p, zend_class_entry *src, zend_class_entry *dst );
static void my_fixup_hashtable( HashTable *ht, ht_fixup_fun_t fixup, zend_class_entry *src, zend_class_entry *dst );
/* my_fixup_function_for_execution is the same as my_fixup_function
 * but named differently for clarity
 */
#define my_fixup_function_for_execution my_fixup_function

#ifdef ZEND_ENGINE_2_2
static void my_fixup_property_info( Bucket *p, zend_class_entry *src, zend_class_entry *dst );
#define my_fixup_property_info_for_execution my_fixup_property_info
#endif

/*
 * These functions return "1" if the member/function is
 * defined/overridden in the 'current' class and not inherited.
 */
static int my_check_copy_function(Bucket* src, va_list args);
static int my_check_copy_default_property(Bucket* p, va_list args);
static int my_check_copy_property_info(Bucket* src, va_list args);
static int my_check_copy_static_member(Bucket* src, va_list args);

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

/* {{{ my_bitwise_copy_function */
static zend_function* my_bitwise_copy_function(zend_function* dst, zend_function* src, apc_context_t* ctxt)
{
    apc_pool* pool = ctxt->pool;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_function*) apc_pool_alloc(pool, sizeof(src[0])));
    }

    /* We only need to do a bitwise copy */
    memcpy(dst, src, sizeof(src[0]));

    return dst;
}
/* }}} */

/* {{{ my_copy_zval_ptr */
static zval** my_copy_zval_ptr(zval** dst, const zval** src, apc_context_t* ctxt)
{
    zval* dst_new;
    apc_pool* pool = ctxt->pool;
    int usegc = (ctxt->copy == APC_COPY_OUT_OPCODE) || (ctxt->copy == APC_COPY_OUT_USER);

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zval**) apc_pool_alloc(pool, sizeof(zval*)));
    }

    if(usegc) {
        ALLOC_ZVAL(dst[0]);
        CHECK(dst[0]);
    } else {
        CHECK((dst[0] = (zval*) apc_pool_alloc(pool, sizeof(zval))));
    }

    CHECK((dst_new = my_copy_zval(*dst, *src, ctxt)));

    if(dst_new != *dst) {
        if(usegc) {
            TSRMLS_FETCH();
            FREE_ZVAL(dst[0]);
        }
        *dst = dst_new;
    }

    return dst;
}
/* }}} */

/* {{{ my_serialize_object */
static zval* my_serialize_object(zval* dst, const zval* src, apc_context_t* ctxt)
{
    smart_str buf = {0};
    php_serialize_data_t var_hash;
    apc_pool* pool = ctxt->pool;

    TSRMLS_FETCH();

    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&buf, (zval**)&src, &var_hash TSRMLS_CC);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);

    if(buf.c) {
        dst->type = src->type & ~IS_CONSTANT_INDEX;
        dst->value.str.len = buf.len;
        CHECK(dst->value.str.val = apc_pmemcpy(buf.c, buf.len+1, pool));
        dst->type = src->type;
        smart_str_free(&buf);
    }

    return dst;
}
/* }}} */

/* {{{ my_unserialize_object */
static zval* my_unserialize_object(zval* dst, const zval* src, apc_context_t* ctxt)
{
    php_unserialize_data_t var_hash;
    const unsigned char *p = (unsigned char*)Z_STRVAL_P(src);

    TSRMLS_FETCH();

    PHP_VAR_UNSERIALIZE_INIT(var_hash);
    if(!php_var_unserialize(&dst, &p, p + Z_STRLEN_P(src), &var_hash TSRMLS_CC)) {
        PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
        zval_dtor(dst);
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Error at offset %ld of %d bytes", (long)((char*)p - Z_STRVAL_P(src)), Z_STRLEN_P(src));
        dst->type = IS_NULL;
    }
    PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
    return dst;
}
/* }}} */

/* {{{ my_copy_zval */
static zval* my_copy_zval(zval* dst, const zval* src, apc_context_t* ctxt)
{
    zval **tmp;
    apc_pool* pool = ctxt->pool;
    TSRMLS_FETCH();

    assert(dst != NULL);
    assert(src != NULL);

    memcpy(dst, src, sizeof(src[0]));

    if(APCG(copied_zvals).nTableSize) {
        if(zend_hash_index_find(&APCG(copied_zvals), (ulong)src, (void**)&tmp) == SUCCESS) {
            if(Z_ISREF_P((zval*)src)) {
                Z_SET_ISREF_PP(tmp);
            }
            Z_ADDREF_PP(tmp);
            return *tmp;
        }

        zend_hash_index_update(&APCG(copied_zvals), (ulong)src, (void**)&dst, sizeof(zval*), NULL);
    }


    if(ctxt->copy == APC_COPY_OUT_USER || ctxt->copy == APC_COPY_IN_USER) {
        /* deep copies are refcount(1), but moved up for recursive 
         * arrays,  which end up being add_ref'd during its copy. */
        Z_SET_REFCOUNT_P(dst, 1);
        Z_UNSET_ISREF_P(dst);
    } else {
        /* code uses refcount=2 for consts */
        Z_SET_REFCOUNT_P(dst, Z_REFCOUNT_P((zval*)src));
        Z_SET_ISREF_TO_P(dst, Z_ISREF_P((zval*)src));
    }

    switch (src->type & IS_CONSTANT_TYPE_MASK) {
    case IS_RESOURCE:
    case IS_BOOL:
    case IS_LONG:
    case IS_DOUBLE:
    case IS_NULL:
        break;

    case IS_CONSTANT:
    case IS_STRING:
        if (src->value.str.val) {
            CHECK(dst->value.str.val = apc_pmemcpy(src->value.str.val,
                                                   src->value.str.len+1,
                                                   pool));
        }
        break;

    case IS_ARRAY:
    case IS_CONSTANT_ARRAY:

        CHECK(dst->value.ht =
            my_copy_hashtable(NULL,
                              src->value.ht,
                              (ht_copy_fun_t) my_copy_zval_ptr,
                              1,
                              ctxt));
        break;

    case IS_OBJECT:
    
        dst->type = IS_NULL;
        if(ctxt->copy == APC_COPY_IN_USER) {
            dst = my_serialize_object(dst, src, ctxt);
        } else if(ctxt->copy == APC_COPY_OUT_USER) {
            dst = my_unserialize_object(dst, src, ctxt);
        }
        break;

    default:
        assert(0);
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_znode */
static znode* my_copy_znode(znode* dst, znode* src, apc_context_t* ctxt)
{
    assert(dst != NULL);
    assert(src != NULL);

    memcpy(dst, src, sizeof(src[0]));

#ifdef IS_CV
    assert(dst ->op_type == IS_CONST ||
           dst ->op_type == IS_VAR ||
           dst ->op_type == IS_CV ||
           dst ->op_type == IS_TMP_VAR ||
           dst ->op_type == IS_UNUSED);
#else
    assert(dst ->op_type == IS_CONST ||
           dst ->op_type == IS_VAR ||
           dst ->op_type == IS_TMP_VAR ||
           dst ->op_type == IS_UNUSED);
#endif

    if (src->op_type == IS_CONST) {
        if(!my_copy_zval(&dst->u.constant, &src->u.constant, ctxt)) {
            return NULL;
        }
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_zend_op */
static zend_op* my_copy_zend_op(zend_op* dst, zend_op* src, apc_context_t* ctxt)
{
    assert(dst != NULL);
    assert(src != NULL);

    memcpy(dst, src, sizeof(src[0]));

    CHECK(my_copy_znode(&dst->result, &src->result, ctxt));
    CHECK(my_copy_znode(&dst->op1, &src->op1, ctxt));
    CHECK(my_copy_znode(&dst->op2, &src->op2, ctxt));

    return dst;
}
/* }}} */

/* {{{ my_copy_function */
static zend_function* my_copy_function(zend_function* dst, zend_function* src, apc_context_t* ctxt)
{
    TSRMLS_FETCH();

    assert(src != NULL);

    CHECK(dst = my_bitwise_copy_function(dst, src, ctxt));

    switch (src->type) {
    case ZEND_INTERNAL_FUNCTION:
    case ZEND_OVERLOADED_FUNCTION:
        /* shallow copy because op_array is internal */
        dst->op_array = src->op_array;
        break;

    case ZEND_USER_FUNCTION:
    case ZEND_EVAL_CODE:
        CHECK(apc_copy_op_array(&dst->op_array,
                                &src->op_array,
                                ctxt TSRMLS_CC));
        break;

    default:
        assert(0);
    }
    /*
     * op_array bitwise copying overwrites what ever you modified
     * before apc_copy_op_array - which is why this code is outside 
     * my_bitwise_copy_function.
     */

    /* zend_do_inheritance will re-look this up, because the pointers
     * in prototype are from a function table of another class. It just
     * helps if that one is from EG(class_table).
     */
    dst->common.prototype = NULL;

    /* once a method is marked as ZEND_ACC_IMPLEMENTED_ABSTRACT then you
     * have to carry around a prototype. Thankfully zend_do_inheritance
     * sets this properly as well
     */
    dst->common.fn_flags = src->common.fn_flags & (~ZEND_ACC_IMPLEMENTED_ABSTRACT);


    return dst;
}
/* }}} */

/* {{{ my_copy_function_entry */
static zend_function_entry* my_copy_function_entry(zend_function_entry* dst, const zend_function_entry* src, apc_context_t* ctxt)
{
    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_function_entry*) apc_pool_alloc(ctxt->pool, sizeof(src[0])));
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(src[0]));

    dst->fname = NULL;
    dst->arg_info = NULL;

    if (src->fname) {
        CHECK((dst->fname = apc_pstrdup(src->fname, ctxt->pool)));
    }

    if (src->arg_info) {
        CHECK((dst->arg_info = my_copy_arg_info_array(NULL,
                                                src->arg_info,
                                                src->num_args,
                                                ctxt)));
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_property_info */
static zend_property_info* my_copy_property_info(zend_property_info* dst, zend_property_info* src, apc_context_t* ctxt)
{
    apc_pool* pool = ctxt->pool;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_property_info*) apc_pool_alloc(pool, sizeof(*src)));
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(*src));

    dst->name = NULL;
#if defined(ZEND_ENGINE_2) && PHP_MINOR_VERSION > 0
    dst->doc_comment = NULL;
#endif

    if (src->name) {
        /* private members are stored inside property_info as a mangled
         * string of the form:
         *      \0<classname>\0<membername>\0
         */
        CHECK((dst->name = apc_pmemcpy(src->name, src->name_length+1, pool)));
    }

#if defined(ZEND_ENGINE_2) && PHP_MINOR_VERSION > 0
    if (src->doc_comment) {
        CHECK((dst->doc_comment = apc_pmemcpy(src->doc_comment, src->doc_comment_len+1, pool)));
    }
#endif

    return dst;
}
/* }}} */

/* {{{ my_copy_property_info_for_execution */
static zend_property_info* my_copy_property_info_for_execution(zend_property_info* dst, zend_property_info* src, apc_context_t* ctxt)
{
    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_property_info*) apc_pool_alloc(ctxt->pool, (sizeof(*src))));
    }

    /* We need only a shallow copy */
    memcpy(dst, src, sizeof(*src));

    return dst;
}
/* }}} */

/* {{{ my_copy_arg_info_array */
static zend_arg_info* my_copy_arg_info_array(zend_arg_info* dst, const zend_arg_info* src, uint num_args, apc_context_t* ctxt)
{
    uint i = 0;


    if (!dst) {
        CHECK(dst = (zend_arg_info*) apc_pool_alloc(ctxt->pool, sizeof(*src)*num_args));
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(*src)*num_args);

    for(i=0; i < num_args; i++) {
        CHECK((my_copy_arg_info( &dst[i], &src[i], ctxt)));
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_arg_info */
static zend_arg_info* my_copy_arg_info(zend_arg_info* dst, const zend_arg_info* src, apc_context_t* ctxt)
{
    apc_pool* pool = ctxt->pool;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_arg_info*) apc_pool_alloc(pool, sizeof(*src)));
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(*src));

    dst->name = NULL;
    dst->class_name = NULL;

    if (src->name) {
        CHECK((dst->name = apc_pmemcpy(src->name, src->name_len+1, pool)));
    }

    if (src->class_name) {
        CHECK((dst->class_name = apc_pmemcpy(src->class_name, src->class_name_len+1, pool)));
    }

    return dst;
}
/* }}} */

/* {{{ apc_copy_class_entry */
zend_class_entry* apc_copy_class_entry(zend_class_entry* dst, zend_class_entry* src, apc_context_t* ctxt)
{
    return my_copy_class_entry(dst, src, ctxt);
}

/* {{{ my_copy_class_entry */
static zend_class_entry* my_copy_class_entry(zend_class_entry* dst, zend_class_entry* src, apc_context_t* ctxt)
{
    uint i = 0;
    apc_pool* pool = ctxt->pool;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_class_entry*) apc_pool_alloc(pool, sizeof(*src)));
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(*src));

    dst->name = NULL;
    dst->builtin_functions = NULL;
    memset(&dst->function_table, 0, sizeof(dst->function_table));
    memset(&dst->default_properties, 0, sizeof(dst->default_properties));
    dst->static_members = NULL;
    dst->doc_comment = NULL;
    dst->filename = NULL;
    memset(&dst->properties_info, 0, sizeof(dst->properties_info));
    memset(&dst->constants_table, 0, sizeof(dst->constants_table));
    memset(&dst->default_static_members, 0, sizeof(dst->default_static_members));

    if (src->name) {
        CHECK((dst->name = apc_pstrdup(src->name, pool)));
    }

    if(!(my_copy_hashtable_ex(&dst->function_table,
                            &src->function_table,
                            (ht_copy_fun_t) my_copy_function,
                            0,
                            ctxt,
                            (ht_check_copy_fun_t) my_check_copy_function,
                            src))) {
        return NULL;
    }

    /* the interfaces are populated at runtime using ADD_INTERFACE */
    dst->interfaces = NULL; 

    /* the current count includes inherited interfaces as well,
       the real dynamic ones are the first <n> which are zero'd
       out in zend_do_end_class_declaration */
    for(i = 0 ; i < src->num_interfaces ; i++) {
        if(src->interfaces[i])
        {
            dst->num_interfaces = i;
            break;
        }
    }

    /* these will either be set inside my_fixup_hashtable or 
     * they will be copied out from parent inside zend_do_inheritance 
     */
    dst->parent = NULL;
    dst->constructor =  NULL;
    dst->destructor = NULL;
    dst->clone = NULL;
    dst->__get = NULL;
    dst->__set = NULL;
    dst->__unset = NULL;
    dst->__isset = NULL;
    dst->__call = NULL;
#ifdef ZEND_ENGINE_2_2
    dst->__tostring = NULL;
#endif
#ifdef ZEND_ENGINE_2_3
	dst->__callstatic = NULL;
#endif

    /* unset function proxies */
    dst->serialize_func = NULL;
    dst->unserialize_func = NULL;

    my_fixup_hashtable(&dst->function_table, (ht_fixup_fun_t)my_fixup_function, src, dst);

    CHECK((my_copy_hashtable_ex(&dst->default_properties,
                            &src->default_properties,
                            (ht_copy_fun_t) my_copy_zval_ptr,
                            1,
                            ctxt,
                            (ht_check_copy_fun_t) my_check_copy_default_property,
                            src)));

    CHECK((my_copy_hashtable_ex(&dst->properties_info,
                            &src->properties_info,
                            (ht_copy_fun_t) my_copy_property_info,
                            0,
                            ctxt,
                            (ht_check_copy_fun_t) my_check_copy_property_info,
                            src)));

#ifdef ZEND_ENGINE_2_2
    /* php5.2 introduced a scope attribute for property info */
    my_fixup_hashtable(&dst->properties_info, (ht_fixup_fun_t)my_fixup_property_info_for_execution, src, dst);
#endif

    CHECK(my_copy_hashtable_ex(&dst->default_static_members,
                            &src->default_static_members,
                            (ht_copy_fun_t) my_copy_zval_ptr,
                            1,
                            ctxt,
                            (ht_check_copy_fun_t) my_check_copy_static_member,
                            src,
                            &src->default_static_members));

    if(src->static_members != &src->default_static_members)
    {
        CHECK((dst->static_members = my_copy_hashtable_ex(NULL,
                            src->static_members,
                            (ht_copy_fun_t) my_copy_zval_ptr,
                            1,
                            ctxt,
                            (ht_check_copy_fun_t) my_check_copy_static_member,
                            src,
                            src->static_members)));
    }
    else
    {
        dst->static_members = &dst->default_static_members;
    }

    CHECK((my_copy_hashtable(&dst->constants_table,
                            &src->constants_table,
                            (ht_copy_fun_t) my_copy_zval_ptr,
                            1,
                            ctxt)));

    if (src->doc_comment) {
        CHECK(dst->doc_comment =
                    apc_pmemcpy(src->doc_comment, src->doc_comment_len+1, pool));
    }

    if (src->builtin_functions) {
        int i, n;

        for (n = 0; src->type == ZEND_INTERNAL_CLASS && src->builtin_functions[n].fname != NULL; n++) {}

        CHECK((dst->builtin_functions =
                (zend_function_entry*) apc_pool_alloc(pool, (n + 1) * sizeof(zend_function_entry))));

        for (i = 0; i < n; i++) {
            CHECK(my_copy_function_entry((zend_function_entry*)(&dst->builtin_functions[i]),
                                   &src->builtin_functions[i],
                                   ctxt));
        }
        *(char**)&(dst->builtin_functions[n].fname) = NULL;
    }

    if (src->filename) {
        CHECK((dst->filename = apc_pstrdup(src->filename, pool)));
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_hashtable */
static HashTable* my_copy_hashtable_ex(HashTable* dst,
                                    HashTable* src,
                                    ht_copy_fun_t copy_fn,
                                    int holds_ptrs,
                                    apc_context_t* ctxt,
                                    ht_check_copy_fun_t check_fn,
                                    ...)
{
    Bucket* curr = NULL;
    Bucket* prev = NULL;
    Bucket* newp = NULL;
    int first = 1;
    apc_pool* pool = ctxt->pool;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (HashTable*) apc_pool_alloc(pool, sizeof(src[0])));
    }

    memcpy(dst, src, sizeof(src[0]));

    /* allocate buckets for the new hashtable */
    CHECK((dst->arBuckets = apc_pool_alloc(pool, dst->nTableSize * sizeof(Bucket*))));

    memset(dst->arBuckets, 0, dst->nTableSize * sizeof(Bucket*));
    dst->pInternalPointer = NULL;
    dst->pListHead = NULL;

    for (curr = src->pListHead; curr != NULL; curr = curr->pListNext) {
        int n = curr->h % dst->nTableSize;

        if(check_fn) {
            va_list args;
            va_start(args, check_fn);

            /* Call the check_fn to see if the current bucket
             * needs to be copied out
             */
            if(!check_fn(curr, args)) {
                dst->nNumOfElements--;
                continue;
            }

            va_end(args);
        }

        /* create a copy of the bucket 'curr' */
        CHECK((newp = (Bucket*) apc_pmemcpy(curr,
                                  sizeof(Bucket) + curr->nKeyLength - 1,
                                  pool)));

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
        CHECK((newp->pData = copy_fn(NULL, curr->pData, ctxt)));

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
static HashTable* my_copy_static_variables(zend_op_array* src, apc_context_t* ctxt)
{
    if (src->static_variables == NULL) {
        return NULL;
    }

    return my_copy_hashtable(NULL,
                             src->static_variables,
                             (ht_copy_fun_t) my_copy_zval_ptr,
                             1,
                             ctxt);
}
/* }}} */

/* {{{ apc_copy_zval */
zval* apc_copy_zval(zval* dst, const zval* src, apc_context_t* ctxt)
{
    apc_pool* pool = ctxt->pool;
    int usegc = (ctxt->copy == APC_COPY_OUT_OPCODE) || (ctxt->copy == APC_COPY_OUT_USER);

    assert(src != NULL);

    if (!dst) {
        if(usegc) {
            ALLOC_ZVAL(dst);
            CHECK(dst);
        } else {
            CHECK(dst = (zval*) apc_pool_alloc(pool, sizeof(zval)));
        }
    }

    CHECK(dst = my_copy_zval(dst, src, ctxt));
    return dst;
}
/* }}} */

/* {{{ apc_fixup_op_array_jumps */
static void apc_fixup_op_array_jumps(zend_op_array *dst, zend_op_array *src )
{
    uint i;

    for (i=0; i < dst->last; ++i) {
        zend_op *zo = &(dst->opcodes[i]);
        /*convert opline number to jump address*/
        switch (zo->opcode) {
            case ZEND_JMP:
                /*Note: if src->opcodes != dst->opcodes then we need to the opline according to src*/
                zo->op1.u.jmp_addr = dst->opcodes + (zo->op1.u.jmp_addr - src->opcodes);
                break;
            case ZEND_JMPZ:
            case ZEND_JMPNZ:
            case ZEND_JMPZ_EX:
            case ZEND_JMPNZ_EX:
#ifdef ZEND_ENGINE_2_3
            case ZEND_JMP_SET:
#endif
                zo->op2.u.jmp_addr = dst->opcodes + (zo->op2.u.jmp_addr - src->opcodes);
                break;
            default:
                break;
        }
    }
}
/* }}} */

/* {{{ apc_copy_op_array */
zend_op_array* apc_copy_op_array(zend_op_array* dst, zend_op_array* src, apc_context_t* ctxt TSRMLS_DC)
{
    int i;
    apc_fileinfo_t fileinfo;
    char canon_path[MAXPATHLEN];
    char *fullpath = NULL;
    apc_opflags_t * flags = NULL;
    apc_pool* pool = ctxt->pool;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_op_array*) apc_pool_alloc(pool, sizeof(src[0])));
    }

    if(APCG(apc_optimize_function)) {
        APCG(apc_optimize_function)(src TSRMLS_CC);
    }

    /* start with a bitwise copy of the array */
    memcpy(dst, src, sizeof(src[0]));

    dst->function_name = NULL;
    dst->filename = NULL;
    dst->refcount = NULL;
    dst->opcodes = NULL;
    dst->brk_cont_array = NULL;
    dst->static_variables = NULL;
    dst->try_catch_array = NULL;
    dst->arg_info = NULL;
    dst->doc_comment = NULL;
#ifdef ZEND_ENGINE_2_1
    dst->vars = NULL;
#endif

    /* copy the arg types array (if set) */
    if (src->arg_info) {
        CHECK(dst->arg_info = my_copy_arg_info_array(NULL,
                                                src->arg_info,
                                                src->num_args,
                                                ctxt));
    }

    if (src->function_name) {
        CHECK(dst->function_name = apc_pstrdup(src->function_name, pool));
    }
    if (src->filename) {
        CHECK(dst->filename = apc_pstrdup(src->filename, pool));
    }

    CHECK(dst->refcount = apc_pmemcpy(src->refcount,
                                      sizeof(src->refcount[0]),
                                      pool));

    /* deep-copy the opcodes */
    CHECK(dst->opcodes = (zend_op*) apc_pool_alloc(pool, sizeof(zend_op) * src->last));

    if(apc_reserved_offset != -1) {
        /* Insanity alert: the void* pointer is cast into an apc_opflags_t 
         * struct. apc_zend_init() checks to ensure that it fits in a void* */
        flags = (apc_opflags_t*) & (dst->reserved[apc_reserved_offset]);
        memset(flags, 0, sizeof(apc_opflags_t));
        /* assert(sizeof(apc_opflags_t) < sizeof(dst->reserved)); */
    }

    for (i = 0; (uint) i < src->last; i++) {
        zend_op *zo = &(src->opcodes[i]);
        /* a lot of files are merely constant arrays with no jumps */
        switch (zo->opcode) {
            case ZEND_JMP:
            case ZEND_JMPZ:
            case ZEND_JMPNZ:
            case ZEND_JMPZ_EX:
            case ZEND_JMPNZ_EX:
#ifdef ZEND_ENGINE_2_3
            case ZEND_JMP_SET:
#endif
                if(flags != NULL) {
                    flags->has_jumps = 1;
                }
                break;
            /* auto_globals_jit was not in php-4.3.* */
            case ZEND_FETCH_R:
            case ZEND_FETCH_W:
            case ZEND_FETCH_IS:
            case ZEND_FETCH_FUNC_ARG:
                if(PG(auto_globals_jit) && flags != NULL)
                {
                     /* The fetch is only required if auto_globals_jit=1  */
                    if(zo->op2.u.EA.type == ZEND_FETCH_GLOBAL &&
                            zo->op1.op_type == IS_CONST && 
                            zo->op1.u.constant.type == IS_STRING) {
                        znode * varname = &zo->op1;
                        if (varname->u.constant.value.str.val[0] == '_') {
#define SET_IF_AUTOGLOBAL(member) \
    if(!strcmp(varname->u.constant.value.str.val, #member)) \
        flags->member = 1 /* no ';' here */
                            SET_IF_AUTOGLOBAL(_GET);
                            else SET_IF_AUTOGLOBAL(_POST);
                            else SET_IF_AUTOGLOBAL(_COOKIE);
                            else SET_IF_AUTOGLOBAL(_SERVER);
                            else SET_IF_AUTOGLOBAL(_ENV);
                            else SET_IF_AUTOGLOBAL(_FILES);
                            else SET_IF_AUTOGLOBAL(_REQUEST);
                            else if(zend_is_auto_global(
                                            varname->u.constant.value.str.val,
                                            varname->u.constant.value.str.len
                                            TSRMLS_CC))
                            {
                                flags->unknown_global = 1;
                            }
                        }
                    }
                }
                break;
            case ZEND_RECV_INIT:
                if(zo->op2.op_type == IS_CONST &&
                    zo->op2.u.constant.type == IS_CONSTANT_ARRAY) {
                    if(flags != NULL) {
                        flags->deep_copy = 1;
                    }
                }
                break;
            default:
                if((zo->op1.op_type == IS_CONST &&
                    zo->op1.u.constant.type == IS_CONSTANT_ARRAY) ||
                    (zo->op2.op_type == IS_CONST &&
                        zo->op2.u.constant.type == IS_CONSTANT_ARRAY)) {
                    if(flags != NULL) {
                        flags->deep_copy = 1;
                    }
                }
                break;
        }

        if(!(my_copy_zend_op(dst->opcodes+i, src->opcodes+i, ctxt))) {
            return NULL;
        }

/* This code breaks apc's rule#1 - cache what you compile */
        if(APCG(current_cache)->fpstat==0 && APCG(canonicalize)) {
            if((zo->opcode == ZEND_INCLUDE_OR_EVAL) && 
                (zo->op1.op_type == IS_CONST && zo->op1.u.constant.type == IS_STRING)) {
                /* constant includes */
                if(!IS_ABSOLUTE_PATH(Z_STRVAL_P(&zo->op1.u.constant),Z_STRLEN_P(&zo->op1.u.constant))) { 
                    if (apc_search_paths(Z_STRVAL_P(&zo->op1.u.constant), PG(include_path), &fileinfo) == 0) {
                        if((fullpath = realpath(fileinfo.fullpath, canon_path))) {
                            /* everything has to go through a realpath() */
                            zend_op *dzo = &(dst->opcodes[i]);
                            dzo->op1.u.constant.value.str.len = strlen(fullpath);
                            dzo->op1.u.constant.value.str.val = apc_pstrdup(fullpath, pool);
                        }
                    }
                }
            }
        }
    }

    if(flags == NULL || flags->has_jumps) {
        apc_fixup_op_array_jumps(dst,src);
    }

    /* copy the break-continue array */
    if (src->brk_cont_array) {
        CHECK(dst->brk_cont_array = apc_pmemcpy(src->brk_cont_array,
                                    sizeof(src->brk_cont_array[0]) * src->last_brk_cont,
                                    pool));
    }

    /* copy the table of static variables */
    if (src->static_variables) {
        CHECK(dst->static_variables = my_copy_static_variables(src, ctxt));
    }

    if (src->try_catch_array) {
        CHECK(dst->try_catch_array = apc_pmemcpy(src->try_catch_array,
                                        sizeof(src->try_catch_array[0]) * src->last_try_catch,
                                        pool));
    }

#ifdef ZEND_ENGINE_2_1 /* PHP 5.1 */
    if (src->vars) {
        CHECK(dst->vars = apc_pmemcpy(src->vars,
                            sizeof(src->vars[0]) * src->last_var,
                            pool));

        for(i = 0; i <  src->last_var; i++) dst->vars[i].name = NULL;

        for(i = 0; i <  src->last_var; i++) {
            CHECK(dst->vars[i].name = apc_pmemcpy(src->vars[i].name,
                                src->vars[i].name_len + 1,
                                pool));
        }
    }
#endif

    if (src->doc_comment) {
        CHECK(dst->doc_comment
                = apc_pmemcpy(src->doc_comment, src->doc_comment_len+1, pool));
    }

    return dst;
}
/* }}} */


/* {{{ apc_copy_new_functions */
apc_function_t* apc_copy_new_functions(int old_count, apc_context_t* ctxt TSRMLS_DC)
{
    apc_function_t* array;
    int new_count;              /* number of new functions in table */
    int i;
    apc_pool* pool = ctxt->pool;

    new_count = zend_hash_num_elements(CG(function_table)) - old_count;
    assert(new_count >= 0);

    CHECK(array =
        (apc_function_t*)
            apc_pool_alloc(pool, sizeof(apc_function_t) * (new_count+1)));

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

        CHECK(array[i].name = apc_pmemcpy(key, (int) key_size, pool));
        array[i].name_len = (int) key_size-1;
        CHECK(array[i].function = my_copy_function(NULL, fun, ctxt));
        zend_hash_move_forward(CG(function_table));
    }

    array[i].function = NULL;
    return array;
}
/* }}} */

/* {{{ apc_copy_new_classes */
apc_class_t* apc_copy_new_classes(zend_op_array* op_array, int old_count, apc_context_t *ctxt TSRMLS_DC)
{
    apc_class_t* array;
    int new_count;              /* number of new classes in table */
    int i;
    apc_pool* pool = ctxt->pool;

    new_count = zend_hash_num_elements(CG(class_table)) - old_count;
    assert(new_count >= 0);

    CHECK(array =
        (apc_class_t*)
            apc_pool_alloc(pool, sizeof(apc_class_t)*(new_count+1)));

    if (new_count == 0) {
        array[0].class_entry = NULL;
        return array;
    }

    /* Skip the first `old_count` classes in the table */
    zend_hash_internal_pointer_reset(CG(class_table));
    for (i = 0; i < old_count; i++) {
        zend_hash_move_forward(CG(class_table));
    }

    /* Add the next `new_count` classes to our array */
    for (i = 0; i < new_count; i++) {
        char* key;
        uint key_size;
        zend_class_entry* elem = NULL;

        array[i].class_entry = NULL;

        zend_hash_get_current_key_ex(CG(class_table),
                                     &key,
                                     &key_size,
                                     NULL,
                                     0,
                                     NULL);

        zend_hash_get_current_data(CG(class_table), (void**) &elem);


        elem = *((zend_class_entry**)elem);

        CHECK(array[i].name = apc_pmemcpy(key, (int) key_size, pool));
        array[i].name_len = (int) key_size-1;
        CHECK(array[i].class_entry = my_copy_class_entry(NULL, elem, ctxt));

        /*
         * If the class has a pointer to its parent class, save the parent
         * name so that we can enable compile-time inheritance when we reload
         * the child class; otherwise, set the parent name to null and scan
         * the op_array to determine if this class inherits from some base
         * class at execution-time.
         */

        if (elem->parent) {
            CHECK(array[i].parent_name = apc_pstrdup(elem->parent->name, pool));
        }
        else {
            array[i].parent_name = NULL;
        }

        zend_hash_move_forward(CG(class_table));
    }

    array[i].class_entry = NULL;
    return array;
}
/* }}} */

/* Used only by my_prepare_op_array_for_execution */
#define APC_PREPARE_FETCH_GLOBAL_FOR_EXECUTION()                                                \
                         /* The fetch is only required if auto_globals_jit=1  */                \
                        if(zo->op2.u.EA.type == ZEND_FETCH_GLOBAL &&                            \
                            zo->op1.op_type == IS_CONST &&                                      \
                            zo->op1.u.constant.type == IS_STRING &&                             \
                            zo->op1.u.constant.value.str.val[0] == '_') {                       \
                                                                                                \
                            znode* varname = &zo->op1;                                          \
                            (void)zend_is_auto_global(varname->u.constant.value.str.val,        \
                                                          varname->u.constant.value.str.len     \
                                                          TSRMLS_CC);                           \
                        }                                                                       \

/* {{{ my_prepare_op_array_for_execution */
static int my_prepare_op_array_for_execution(zend_op_array* dst, zend_op_array* src, apc_context_t* ctxt TSRMLS_DC) 
{
    /* combine my_fetch_global_vars and my_copy_data_exceptions.
     *   - Pre-fetch superglobals which would've been pre-fetched in parse phase.
     *   - If the opcode stream contain mutable data, ensure a copy.
     *   - Fixup array jumps in the same loop.
     */
    int i=src->last;
    zend_op *zo;
    zend_op *dzo;
    apc_opflags_t * flags = apc_reserved_offset  != -1 ? 
                                (apc_opflags_t*) & (src->reserved[apc_reserved_offset]) : NULL;
    int needcopy = flags ? flags->deep_copy : 1;
    /* auto_globals_jit was not in php4 */
    int do_prepare_fetch_global = PG(auto_globals_jit) && (flags == NULL || flags->unknown_global);

#define FETCH_AUTOGLOBAL(member) do { \
    if(flags && flags->member == 1) { \
        zend_is_auto_global(#member,\
                            (sizeof(#member) - 1)\
                            TSRMLS_CC);\
    } \
} while(0);

    FETCH_AUTOGLOBAL(_GET);
    FETCH_AUTOGLOBAL(_POST);
    FETCH_AUTOGLOBAL(_COOKIE);
    FETCH_AUTOGLOBAL(_SERVER);
    FETCH_AUTOGLOBAL(_ENV);
    FETCH_AUTOGLOBAL(_FILES);
    FETCH_AUTOGLOBAL(_REQUEST);

    if(needcopy) {

        dst->opcodes = (zend_op*) apc_xmemcpy(src->opcodes,
                                    sizeof(zend_op) * src->last,
                                    apc_php_malloc);
        zo = src->opcodes;
        dzo = dst->opcodes;
        while(i > 0) {

            if( ((zo->op1.op_type == IS_CONST &&
                  zo->op1.u.constant.type == IS_CONSTANT_ARRAY)) ||
                ((zo->op2.op_type == IS_CONST &&
                  zo->op2.u.constant.type == IS_CONSTANT_ARRAY))) {

                if(!(my_copy_zend_op(dzo, zo, ctxt))) {
                    assert(0); /* emalloc failed or a bad constant array */
                }
            }

            switch(zo->opcode) {
                case ZEND_JMP:
                    dzo->op1.u.jmp_addr = dst->opcodes +
                                            (zo->op1.u.jmp_addr - src->opcodes);
                    break;
                case ZEND_JMPZ:
                case ZEND_JMPNZ:
                case ZEND_JMPZ_EX:
                case ZEND_JMPNZ_EX:
#ifdef ZEND_ENGINE_2_3
                case ZEND_JMP_SET:
#endif
                    dzo->op2.u.jmp_addr = dst->opcodes +
                                            (zo->op2.u.jmp_addr - src->opcodes);
                    break;
                case ZEND_FETCH_R:
                case ZEND_FETCH_W:
                case ZEND_FETCH_IS:
                case ZEND_FETCH_FUNC_ARG:
                    if(do_prepare_fetch_global)
                    {
                        APC_PREPARE_FETCH_GLOBAL_FOR_EXECUTION();
                    }
                    break;
                default:
                    break;
            }
            i--;
            zo++;
            dzo++;
        }
    } else {  /* !needcopy */
        /* The fetch is only required if auto_globals_jit=1  */
        if(do_prepare_fetch_global)
        {
            zo = src->opcodes;
            while(i > 0) {

                if(zo->opcode == ZEND_FETCH_R ||
                   zo->opcode == ZEND_FETCH_W ||
                   zo->opcode == ZEND_FETCH_IS ||
                   zo->opcode == ZEND_FETCH_FUNC_ARG
                  ) {
                    APC_PREPARE_FETCH_GLOBAL_FOR_EXECUTION();
                }

                i--;
                zo++;
            }
        }
    }
    return 1;
}
/* }}} */

/* {{{ apc_copy_op_array_for_execution */
zend_op_array* apc_copy_op_array_for_execution(zend_op_array* dst, zend_op_array* src, apc_context_t* ctxt TSRMLS_DC)
{
    if(dst == NULL) {
        dst = (zend_op_array*) emalloc(sizeof(src[0]));
    }
    memcpy(dst, src, sizeof(src[0]));
    dst->static_variables = my_copy_static_variables(src, ctxt);

    /* memory leak */
    dst->refcount = apc_pmemcpy(src->refcount,
                                      sizeof(src->refcount[0]),
                                      ctxt->pool);

    my_prepare_op_array_for_execution(dst,src, ctxt TSRMLS_CC);

    return dst;
}
/* }}} */

/* {{{ apc_copy_function_for_execution */
zend_function* apc_copy_function_for_execution(zend_function* src, apc_context_t* ctxt TSRMLS_DC)
{
    zend_function* dst;
    TSRMLS_FETCH();

    dst = (zend_function*) emalloc(sizeof(src[0]));
    memcpy(dst, src, sizeof(src[0]));
    apc_copy_op_array_for_execution(&(dst->op_array), &(src->op_array), ctxt TSRMLS_CC);
    return dst;
}
/* }}} */

/* {{{ apc_copy_function_for_execution_ex */
zend_function* apc_copy_function_for_execution_ex(void *dummy, zend_function* src, apc_context_t* ctxt TSRMLS_DC)
{
    if(src->type==ZEND_INTERNAL_FUNCTION || src->type==ZEND_OVERLOADED_FUNCTION) return src;
    return apc_copy_function_for_execution(src, ctxt TSRMLS_CC);
}
/* }}} */

/* {{{ apc_copy_class_entry_for_execution */
zend_class_entry* apc_copy_class_entry_for_execution(zend_class_entry* src, apc_context_t* ctxt)
{
    zend_class_entry* dst = (zend_class_entry*) apc_pool_alloc(ctxt->pool, sizeof(src[0]));
    memcpy(dst, src, sizeof(src[0]));

    if(src->num_interfaces)
    {
        /* These are slots to be populated later by ADD_INTERFACE insns */
        dst->interfaces = apc_php_malloc(
                            sizeof(zend_class_entry*) * src->num_interfaces);
        memset(dst->interfaces, 0, 
                            sizeof(zend_class_entry*) * src->num_interfaces);
    }
    else
    {
        /* assert(dst->interfaces == NULL); */
    }

    /* Deep-copy the class properties, because they will be modified */

    my_copy_hashtable(&dst->default_properties,
                      &src->default_properties,
                      (ht_copy_fun_t) my_copy_zval_ptr,
                      1,
                      ctxt);

    /* For derived classes, we must also copy the function hashtable (although
     * we can merely bitwise copy the functions it contains) */

    my_copy_hashtable(&dst->function_table,
                      &src->function_table,
                      (ht_copy_fun_t) apc_copy_function_for_execution_ex,
                      0,
                      ctxt);

    my_fixup_hashtable(&dst->function_table, (ht_fixup_fun_t)my_fixup_function_for_execution, src, dst);

    /* zend_do_inheritance merges properties_info.
     * Need only shallow copying as it doesn't hold the pointers.
     */
    my_copy_hashtable(&dst->properties_info,
                      &src->properties_info,
                      (ht_copy_fun_t) my_copy_property_info_for_execution,
                      0,
                      ctxt);

#ifdef ZEND_ENGINE_2_2
    /* php5.2 introduced a scope attribute for property info */
    my_fixup_hashtable(&dst->properties_info, (ht_fixup_fun_t)my_fixup_property_info_for_execution, src, dst);
#endif

    /* if inheritance results in a hash_del, it might result in
     * a pefree() of the pointers here. Deep copying required. 
     */

    my_copy_hashtable(&dst->constants_table,
                      &src->constants_table,
                      (ht_copy_fun_t) my_copy_zval_ptr,
                      1,
                      ctxt);

    my_copy_hashtable(&dst->default_static_members,
                      &src->default_static_members,
                      (ht_copy_fun_t) my_copy_zval_ptr,
                      1,
                      ctxt);

    if(src->static_members != &(src->default_static_members))
    {
        dst->static_members = my_copy_hashtable(NULL,
                          src->static_members,
                          (ht_copy_fun_t) my_copy_zval_ptr,
                          1,
                          ctxt);
    }
    else 
    {
        dst->static_members = &(dst->default_static_members);
    }


    return dst;
}
/* }}} */

/* {{{ apc_free_class_entry_after_execution */
void apc_free_class_entry_after_execution(zend_class_entry* src)
{
    if(src->num_interfaces > 0 && src->interfaces) {
        apc_php_free(src->interfaces);
        src->interfaces = NULL;
        src->num_interfaces = 0;
    }
    /* my_destroy_hashtable() does not play nice with refcounts */

    zend_hash_clean(&src->default_static_members);
    if(src->static_members != &(src->default_static_members))
    {
        zend_hash_destroy(src->static_members);
        apc_php_free(src->static_members);
        src->static_members = NULL;
    }
    else
    {
        src->static_members = NULL;
    }
    zend_hash_clean(&src->default_properties);
    zend_hash_clean(&src->constants_table);

    /* TODO: more cleanup */
}
/* }}} */

/* {{{ apc_file_halt_offset */
long apc_file_halt_offset(const char *filename)
{
    zend_constant *c;
    char *name;
    int len;
    char haltoff[] = "__COMPILER_HALT_OFFSET__";
    long value = -1;
    TSRMLS_FETCH();

    zend_mangle_property_name(&name, &len, haltoff, sizeof(haltoff) - 1, filename, strlen(filename), 0);
    
    if (zend_hash_find(EG(zend_constants), name, len+1, (void **) &c) == SUCCESS) {
        value = Z_LVAL(c->value);
    }
    
    pefree(name, 0);

    return value;
}
/* }}} */

/* {{{ apc_do_halt_compiler_register */
void apc_do_halt_compiler_register(const char *filename, long halt_offset TSRMLS_DC)
{
    char *name;
    char haltoff[] = "__COMPILER_HALT_OFFSET__";
    int len;
   
    if(halt_offset > 0) {
        
        zend_mangle_property_name(&name, &len, haltoff, sizeof(haltoff) - 1, 
                                    filename, strlen(filename), 0);
        
        zend_register_long_constant(name, len+1, halt_offset, CONST_CS, 0 TSRMLS_CC);

        pefree(name, 0);
    }
}
/* }}} */



/* {{{ my_fixup_function */
static void my_fixup_function(Bucket *p, zend_class_entry *src, zend_class_entry *dst)
{
    zend_function* zf = p->pData;

    #define SET_IF_SAME_NAME(member) \
    do { \
        if(src->member && !strcmp(zf->common.function_name, src->member->common.function_name)) { \
            dst->member = zf; \
        } \
    } \
    while(0)

    if(zf->common.scope == src)
    {

        /* Fixing up the default functions for objects here since
         * we need to compare with the newly allocated functions
         *
         * caveat: a sub-class method can have the same name as the
         * parent's constructor and create problems.
         */

        if(zf->common.fn_flags & ZEND_ACC_CTOR) dst->constructor = zf;
        else if(zf->common.fn_flags & ZEND_ACC_DTOR) dst->destructor = zf;
        else if(zf->common.fn_flags & ZEND_ACC_CLONE) dst->clone = zf;
        else
        {
            SET_IF_SAME_NAME(__get);
            SET_IF_SAME_NAME(__set);
            SET_IF_SAME_NAME(__unset);
            SET_IF_SAME_NAME(__isset);
            SET_IF_SAME_NAME(__call);
#ifdef ZEND_ENGINE_2_2
            SET_IF_SAME_NAME(__tostring);
#endif
#ifdef ZEND_ENGINE_2_3
            SET_IF_SAME_NAME(__callstatic);
#endif
        }
        zf->common.scope = dst;
    }
    else
    {
        /* no other function should reach here */
        assert(0);
    }

    #undef SET_IF_SAME_NAME
}
/* }}} */

#ifdef ZEND_ENGINE_2_2
/* {{{ my_fixup_property_info */
static void my_fixup_property_info(Bucket *p, zend_class_entry *src, zend_class_entry *dst)
{
    zend_property_info* property_info = (zend_property_info*)p->pData;

    if(property_info->ce == src)
    {
        property_info->ce = dst;
    }
    else
    {
        assert(0); /* should never happen */
    }
}
/* }}} */
#endif

/* {{{ my_fixup_hashtable */
static void my_fixup_hashtable(HashTable *ht, ht_fixup_fun_t fixup, zend_class_entry *src, zend_class_entry *dst)
{
    Bucket *p;
    uint i;

    for (i = 0; i < ht->nTableSize; i++) {
        if(!ht->arBuckets) break;
        p = ht->arBuckets[i];
        while (p != NULL) {
            fixup(p, src, dst);
            p = p->pNext;
        }
    }
}
/* }}} */


/* {{{ my_check_copy_function */
static int my_check_copy_function(Bucket* p, va_list args)
{
    zend_class_entry* src = va_arg(args, zend_class_entry*);
    zend_function* zf = (zend_function*)p->pData;

    return (zf->common.scope == src);
}
/* }}} */

/* {{{ my_check_copy_default_property */
static int my_check_copy_default_property(Bucket* p, va_list args)
{
    zend_class_entry* src = va_arg(args, zend_class_entry*);
    zend_class_entry* parent = src->parent;
    zval ** child_prop = (zval**)p->pData;
    zval ** parent_prop = NULL;

    if (parent &&
        zend_hash_quick_find(&parent->default_properties, p->arKey, 
            p->nKeyLength, p->h, (void **) &parent_prop)==SUCCESS) {

        if((parent_prop && child_prop) && (*parent_prop) == (*child_prop))
        {
            return 0;
        }
    }

    /* possibly not in the parent */
    return 1;
}
/* }}} */


/* {{{ my_check_copy_property_info */
static int my_check_copy_property_info(Bucket* p, va_list args)
{
    zend_class_entry* src = va_arg(args, zend_class_entry*);
    zend_class_entry* parent = src->parent;
    zend_property_info* child_info = (zend_property_info*)p->pData;
    zend_property_info* parent_info = NULL;

#ifdef ZEND_ENGINE_2_2
    /* so much easier */
    return (child_info->ce == src);
#endif

    if (parent &&
        zend_hash_quick_find(&parent->properties_info, p->arKey, p->nKeyLength,
            p->h, (void **) &parent_info)==SUCCESS) {
        if(parent_info->flags & ZEND_ACC_PRIVATE)
        {
            return 1;
        }
        if((parent_info->flags & ZEND_ACC_PPP_MASK) !=
            (child_info->flags & ZEND_ACC_PPP_MASK))
        {
            /* TODO: figure out whether ACC_CHANGED is more appropriate
             * here */
            return 1;
        }
        return 0;
    }

    /* property doesn't exist in parent, copy into cached child */
    return 1;
}
/* }}} */

/* {{{ my_check_copy_static_member */
static int my_check_copy_static_member(Bucket* p, va_list args)
{
    zend_class_entry* src = va_arg(args, zend_class_entry*);
    HashTable * ht = va_arg(args, HashTable*);
    zend_class_entry* parent = src->parent;
    HashTable * parent_ht = NULL;
    char * member_name;
    char * class_name = NULL;

    zend_property_info *parent_info = NULL;
    zend_property_info *child_info = NULL;
    zval ** parent_prop = NULL;
    zval ** child_prop = (zval**)(p->pData);

    if(!parent) {
        return 1;
    }

    /* these do not need free'ing */
#ifdef ZEND_ENGINE_2_2
    zend_unmangle_property_name(p->arKey, p->nKeyLength-1, &class_name, &member_name);
#else
    zend_unmangle_property_name(p->arKey, &class_name, &member_name);
#endif

    /* please refer do_inherit_property_access_check in zend_compile.c
     * to understand why we lookup in properties_info.
     */
    if((zend_hash_find(&parent->properties_info, member_name,
                        strlen(member_name)+1, (void**)&parent_info) == SUCCESS)
        &&
        (zend_hash_find(&src->properties_info, member_name,
                        strlen(member_name)+1, (void**)&child_info) == SUCCESS))
    {
        if(ht == &(src->default_static_members))
        {
            parent_ht = &parent->default_static_members;
        }
        else
        {
            parent_ht = parent->static_members;
        }

        if(zend_hash_quick_find(parent_ht, p->arKey,
                       p->nKeyLength, p->h, (void**)&parent_prop) == SUCCESS)
        {
            /* they point to the same zval */
            if(*parent_prop == *child_prop)
            {
                return 0;
            }
        }
    }

    return 1;
}
/* }}} */

/* {{{ apc_register_optimizer(apc_optimize_function_t optimizer)
 *      register a optimizer callback function, returns the previous callback
 */
apc_optimize_function_t apc_register_optimizer(apc_optimize_function_t optimizer TSRMLS_DC) {
    apc_optimize_function_t old_optimizer = APCG(apc_optimize_function);
    APCG(apc_optimize_function) = optimizer;
    return old_optimizer;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
