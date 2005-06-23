/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2005 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
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
#include "apc_optimizer.h"

typedef void* (*ht_copy_fun_t)(void*, void*, apc_malloc_t, apc_free_t);
typedef void  (*ht_free_fun_t)(void*, apc_free_t);
typedef int (*ht_check_copy_fun_t)(Bucket*, va_list);

#ifdef ZEND_ENGINE_2
typedef void (*ht_fixup_fun_t)(Bucket*, zend_class_entry*, zend_class_entry*);
#endif

#define CHECK(p) { if ((p) == NULL) return NULL; }

/* {{{ internal function declarations */

static int is_derived_class(zend_op_array* op_array, const char* key, int key_size);

static zend_function* my_bitwise_copy_function(zend_function*, zend_function*, apc_malloc_t);

/*
 * The "copy" functions perform deep-copies on a particular data structure
 * (passed as the second argument). They also optionally allocate space for
 * the destination data structure if the first argument is null.
 */
static zval** my_copy_zval_ptr(zval**, const zval**, apc_malloc_t, apc_free_t);
static zval* my_copy_zval(zval*, const zval*, apc_malloc_t, apc_free_t);
static znode* my_copy_znode(znode*, znode*, apc_malloc_t, apc_free_t);
static zend_op* my_copy_zend_op(zend_op*, zend_op*, apc_malloc_t, apc_free_t);
static zend_function* my_copy_function(zend_function*, zend_function*, apc_malloc_t, apc_free_t);
static zend_function_entry* my_copy_function_entry(zend_function_entry*, zend_function_entry*, apc_malloc_t, apc_free_t);
static zend_class_entry* my_copy_class_entry(zend_class_entry*, zend_class_entry*, apc_malloc_t, apc_free_t);
static HashTable* my_copy_hashtable_ex(HashTable*, HashTable*, ht_copy_fun_t, ht_free_fun_t, int, apc_malloc_t, apc_free_t, ht_check_copy_fun_t, ...);
#define my_copy_hashtable( dst, src, copy_fn, free_fn, holds_ptr, allocate, deallocate) \
    my_copy_hashtable_ex(dst, src, copy_fn, free_fn, holds_ptr, allocate, deallocate, NULL)
static HashTable* my_copy_static_variables(zend_op_array* src, apc_malloc_t allocate, apc_free_t deallocate);
#ifdef ZEND_ENGINE_2
static zend_property_info* my_copy_property_info(zend_property_info* dst, zend_property_info* src, apc_malloc_t allocate, apc_free_t deallocate);
static zend_arg_info* my_copy_arg_info_array(zend_arg_info*, zend_arg_info*, uint, apc_malloc_t, apc_free_t);
static zend_arg_info* my_copy_arg_info(zend_arg_info*, zend_arg_info*, apc_malloc_t, apc_free_t);
#endif
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
#ifdef ZEND_ENGINE_2
static void my_destroy_property_info(zend_property_info*, apc_free_t);
static void my_destroy_arg_info_array(zend_arg_info* src, uint, apc_free_t);
static void my_destroy_arg_info(zend_arg_info*, apc_free_t);
#endif

/*
 * The "free" functions work exactly like their "destroy" counterparts (see
 * above) but also free the pointer to the data structure.
 */
static void my_free_zval_ptr(zval**, apc_free_t);
static void my_free_function(zend_function*, apc_free_t);
static void my_free_hashtable(HashTable*, ht_free_fun_t, apc_free_t);
#ifdef ZEND_ENGINE_2
static void my_free_property_info(zend_property_info* src, apc_free_t);
static void my_free_arg_info_array(zend_arg_info*, uint, apc_free_t);
static void my_free_arg_info(zend_arg_info*, apc_free_t);
#endif

/*
 * The "fixup" functions need for ZEND_ENGINE_2
 */
#ifdef ZEND_ENGINE_2
static void my_fixup_function( Bucket *p, zend_class_entry *src, zend_class_entry *dst );
static void my_fixup_hashtable( HashTable *ht, ht_fixup_fun_t fixup, zend_class_entry *src, zend_class_entry *dst );
/* my_fixup_function_for_execution is the same as my_fixup_function
 * but named differently for clarity
 */
#define my_fixup_function_for_execution my_fixup_function
#endif

/*
 * The "check for copy" functions need for ZEND_ENGINE_2
 * These functions return "1" if the member/function is
 * defined/overridden in the 'current' class and not inherited
 */
#ifdef ZEND_ENGINE_2
static int my_check_copy_function(Bucket* src, va_list args);
static int my_check_copy_property_info(Bucket* src, va_list args);
static int my_check_copy_static_member(Bucket* src, va_list args);
#endif

/* }}} */

#ifdef __DEBUG_APC__

/*
 * Function to dump a "struct HashTable" for debugging purposes... copied from zend_hash.c
 */
void my_zend_hash_display(HashTable *ht, char* separator)
{
	Bucket *p;
	uint i;
   
    fprintf(stderr, "%s", separator);
    fprintf(stderr, "\tht->nTableSize: %d\n", ht->nTableSize);
    fprintf(stderr, "%s", separator);
    fprintf(stderr, "\tht->nNumOfElements: %d\n", ht->nNumOfElements);
	for (i = 0; i < ht->nTableSize; i++) {
		if(!ht->arBuckets) break;
        p = ht->arBuckets[i];
        if(p) {
            fprintf(stderr, "%s", separator);
            fprintf(stderr, "\tht->arBuckets[%d]\n", i);
        }
		while (p != NULL) {
            fprintf(stderr, "%s", separator);
            fprintf(stderr, "\t\t%s:%d <", __FILE__, __LINE__);
            if(1)
            {
                int k = 0;
                for(k = 0; k < p->nKeyLength; k++)
                {
                    if(p->arKey[k])
                        fprintf(stderr, "%c", p->arKey[k]);
                    else
                        fprintf(stderr, "\\0");
                }
            }
            fprintf(stderr, "> <==> %p\n", p->pData);
			p = p->pNext;
		}
	}

	p = ht->pListTail;
    if(p) {
        fprintf(stderr, "%s", separator);
        fprintf(stderr, "\tht->pListTail: %p\n", p);
    }
	while (p != NULL) {
            fprintf(stderr, "%s", separator);
            fprintf(stderr, "\t\t%s:%d <", __FILE__, __LINE__);
            if(1)
            {
                int k = 0;
                for(k = 0; k < p->nKeyLength; k++)
                {
                    if(p->arKey[k])
                        fprintf(stderr, "%c", p->arKey[k]);
                    else
                        fprintf(stderr, "\\0");
                }
            }
            fprintf(stderr, "> <==> %p\n", p->pData);
		p = p->pListLast;
	}
}


/*
 * Function to dump a "struct _zend_class_entry" for debugging purposes
 */
static void my_dump_zend_class_entry(zend_class_entry* zce)
{
    fprintf(stderr, "Dumping zend_class_entry* %p...\n", zce);
    fprintf(stderr, "zce->type: %i\n", (int)zce->type);
   
    if(zce->name)
    {
        fprintf(stderr, "zce->name: %s, zce->name_length: %d\n", zce->name, zce->name_length);
    }
    else
    {
        fprintf(stderr, "zce->name: <null>\n");
    }
    
    fprintf(stderr, "zce->parent: %p\n", zce->parent);
    fprintf(stderr, "zce->refcount: %i\n", zce->refcount);
    fprintf(stderr, "zce->constants_updated: %i\n", (int)zce->constants_updated);
    fprintf(stderr, "zce->ce_flags: %i\n", (int)zce->ce_flags);
/*    fprintf(stderr, "zce->: %\n", zce-> );*/
    
/*    fprintf(stderr, "zce->function_table:\n" );*/
/*    my_zend_hash_display(zce->static_members);*/

    fprintf(stderr, "\n\n");
}

#endif

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

#ifdef ZEND_ENGINE_2        
        if (op->opcode == ZEND_DECLARE_CLASS &&
            op->extended_value == ZEND_DECLARE_INHERITED_CLASS)
#else            
        if (op->opcode == ZEND_DECLARE_FUNCTION_OR_CLASS &&
            op->extended_value == ZEND_DECLARE_INHERITED_CLASS)
#endif            
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
static zval** my_copy_zval_ptr(zval** dst, const zval** src, apc_malloc_t allocate, apc_free_t deallocate)
{
    int local_dst_alloc = 0;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zval**) allocate(sizeof(zval*)));
        local_dst_alloc = 1;
    }

    if(!(dst[0] = (zval*) allocate(sizeof(zval)))) {
        if(local_dst_alloc) deallocate(dst);
        return NULL;
    }
    if(!my_copy_zval(*dst, *src, allocate, deallocate)) return NULL;

    /* deep-copying ensures that there is only one reference to this in memory */
    (*dst)->refcount = 1;
    
    return dst;
}
/* }}} */

/* {{{ my_copy_zval */
static zval* my_copy_zval(zval* dst, const zval* src, apc_malloc_t allocate, apc_free_t deallocate)
{
    assert(dst != NULL);
    assert(src != NULL);

    memcpy(dst, src, sizeof(src[0]));

    switch (src->type & ~IS_CONSTANT_INDEX) {
    case IS_RESOURCE:
    case IS_BOOL:
    case IS_LONG:
    case IS_DOUBLE:
    case IS_NULL:
        break;

    case IS_CONSTANT:
    case IS_STRING:
#ifndef ZEND_ENGINE_2        
    case FLAG_IS_BC:
#endif        
        if (src->value.str.val) {
            CHECK(dst->value.str.val = apc_xmemcpy(src->value.str.val,
                                                   src->value.str.len+1,
                                                   allocate));
        }
        break;
    
    case IS_ARRAY:
    case IS_CONSTANT_ARRAY:
        CHECK(dst->value.ht =
            my_copy_hashtable(NULL,
                              src->value.ht,
                              (ht_copy_fun_t) my_copy_zval_ptr,
                              (ht_free_fun_t) my_free_zval_ptr,
                              1,
                              allocate, deallocate));
        break;

    case IS_OBJECT:
#ifndef ZEND_ENGINE_2        
        CHECK(dst->value.obj.ce =
            my_copy_class_entry(NULL, src->value.obj.ce, allocate, deallocate));

        if(!(dst->value.obj.properties = my_copy_hashtable(NULL,
                              src->value.obj.properties,
                              (ht_copy_fun_t) my_copy_zval_ptr,
                              (ht_free_fun_t) my_free_zval_ptr,
                              1,
                              allocate, deallocate))) {
            my_destroy_class_entry(dst->value.obj.ce, deallocate);
            return NULL;
        }
#endif        
        break;

    default:
        assert(0);
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_znode */
static znode* my_copy_znode(znode* dst, znode* src, apc_malloc_t allocate, apc_free_t deallocate)
{
    assert(dst != NULL);
    assert(src != NULL);

    memcpy(dst, src, sizeof(src[0]));

    assert(dst ->op_type == IS_CONST ||
           dst ->op_type == IS_VAR ||
           dst ->op_type == IS_TMP_VAR ||
           dst ->op_type == IS_UNUSED);

    if (src->op_type == IS_CONST) {
        if(!my_copy_zval(&dst->u.constant, &src->u.constant, allocate, deallocate)) {
            return NULL;
        }
    }

    return dst;
}
/* }}} */

/* {{{ my_copy_zend_op */
static zend_op* my_copy_zend_op(zend_op* dst, zend_op* src, apc_malloc_t allocate, apc_free_t deallocate)
{
    assert(dst != NULL);
    assert(src != NULL);

    memcpy(dst, src, sizeof(src[0]));
    my_copy_znode(&dst->result, &src->result, allocate, deallocate);
    my_copy_znode(&dst->op1, &src->op1, allocate, deallocate);
    my_copy_znode(&dst->op2, &src->op2, allocate, deallocate);

    return dst;
}
/* }}} */

/* {{{ my_copy_function */
static zend_function* my_copy_function(zend_function* dst, zend_function* src, apc_malloc_t allocate, apc_free_t deallocate)
{
    int local_dst_alloc = 0;
	TSRMLS_FETCH();

    assert(src != NULL);

    if(!dst) local_dst_alloc = 1;
    CHECK(dst = my_bitwise_copy_function(dst, src, allocate));

    switch (src->type) {
    case ZEND_INTERNAL_FUNCTION:
    case ZEND_OVERLOADED_FUNCTION:
        /* shallow copy because op_array is internal */
        dst->op_array = src->op_array;
        break;
        
    case ZEND_USER_FUNCTION:
    case ZEND_EVAL_CODE:
        if(!apc_copy_op_array(&dst->op_array,
                                &src->op_array,
                                allocate, deallocate TSRMLS_CC)) {
            if(local_dst_alloc) deallocate(dst);
            return NULL;
        }
        break;

    default:
        assert(0);
    }
#ifdef ZEND_ENGINE_2
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
#endif


    return dst;
}
/* }}} */

/* {{{ my_copy_function_entry */
static zend_function_entry* my_copy_function_entry(zend_function_entry* dst, zend_function_entry* src, apc_malloc_t allocate, apc_free_t deallocate)
{
    int local_dst_alloc = 0;
    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_function_entry*) allocate(sizeof(src[0])));
        local_dst_alloc = 1;
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(src[0]));

    dst->fname = NULL;
#ifdef ZEND_ENGINE_2
    dst->arg_info = NULL;
#else
    dst->func_arg_types = NULL;
#endif

    if (src->fname) {
        if(!(dst->fname = apc_xstrdup(src->fname, allocate))) {
            goto cleanup;
        }
    }

#ifdef ZEND_ENGINE_2    
    if (src->arg_info) {
        if(!(dst->arg_info = my_copy_arg_info_array(NULL,
                                                src->arg_info,
                                                src->num_args,
                                                allocate,
                                                deallocate))) {
            goto cleanup;
        }
    }
#else    
    if (src->func_arg_types) {
        if(!(dst->func_arg_types = apc_xmemcpy(src->func_arg_types,
                                                src->func_arg_types[0]+1,
                                                allocate))) {
            goto cleanup;
        }
    }
#endif

    return dst;

cleanup:
    if(dst->fname) deallocate(dst->fname);
    if(local_dst_alloc) deallocate(dst);
    return NULL;
}
/* }}} */

#ifdef ZEND_ENGINE_2
/* {{{ my_copy_property_info */
static zend_property_info* my_copy_property_info(zend_property_info* dst, zend_property_info* src, apc_malloc_t allocate, apc_free_t deallocate)
{
    int local_dst_alloc = 0;
    
    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_property_info*) allocate(sizeof(*src)));
        local_dst_alloc = 1;
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(*src));

    dst->name = NULL;
#ifdef ZEND_ENGINE_2
    dst->doc_comment = NULL;
#endif

    if (src->name) {
        /* private members are stored inside property_info as a mangled
         * string of the form:
         *      \0<classname>\0<membername>\0
         */
        if(!(dst->name = 
                    apc_xmemcpy(src->name, src->name_length+1, allocate))) {
            goto cleanup;
        }
    }

#ifdef ZEND_ENGINE_2
    if (src->doc_comment) {
        if( !(dst->doc_comment =
                    apc_xmemcpy(src->doc_comment, src->doc_comment_len+1, allocate))) {
            goto cleanup;
        }
    }
#endif

    return dst;

cleanup:
    if(dst->name) deallocate(dst->name);
    if(dst->doc_comment) deallocate(dst->doc_comment);
    if(local_dst_alloc) deallocate(dst);
    return NULL;
}
/* }}} */

/* {{{ my_copy_arg_info_array */
static zend_arg_info* my_copy_arg_info_array(zend_arg_info* dst, zend_arg_info* src, uint num_args, apc_malloc_t allocate, apc_free_t deallocate)
{
    int local_dst_alloc = 0;
    int i = 0;

    
    if (!dst) {
        CHECK(dst = (zend_arg_info*) allocate(sizeof(*src)*num_args));
        local_dst_alloc = 1;
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(*src)*num_args);

    for(i=0; i < num_args; i++) {
        if(!(my_copy_arg_info( &dst[i], &src[i], allocate, deallocate))) {            
            if(i) my_destroy_arg_info_array(dst, i-1, deallocate);
            if(local_dst_alloc) deallocate(dst);
            return NULL;
        }
    }

    return dst;    
}
/* }}} */

/* {{{ my_copy_arg_info */
static zend_arg_info* my_copy_arg_info(zend_arg_info* dst, zend_arg_info* src, apc_malloc_t allocate, apc_free_t deallocate)
{
    int local_dst_alloc = 0;
    
    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_arg_info*) allocate(sizeof(*src)));
        local_dst_alloc = 1;
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(*src));

    dst->name = NULL;
    dst->class_name = NULL;

    if (src->name) {
        if(!(dst->name = 
                    apc_xmemcpy(src->name, src->name_len+1, allocate))) {
            goto cleanup;
        }
    }

    if (src->class_name) {
        if(!(dst->class_name = 
                    apc_xmemcpy(src->class_name, src->class_name_len+1, allocate))) {
            goto cleanup;
        }
    }

    return dst;

cleanup:
    if(dst->name) deallocate(dst->name);
    if(dst->class_name) deallocate(dst->name);
    if(local_dst_alloc) deallocate(dst);
    return NULL;
}
/* }}} */
#endif

/* {{{ my_copy_class_entry */
static zend_class_entry* my_copy_class_entry(zend_class_entry* dst, zend_class_entry* src, apc_malloc_t allocate, apc_free_t deallocate)
{
    int local_dst_alloc = 0;

    assert(src != NULL);

#ifdef __DEBUG_APC__
    fprintf(stderr, "\n\n<my_copy_class_entry>... \n" );
    fprintf(stderr, "\tAbout to dump \"src\"... \n" );
    my_dump_zend_class_entry(src);
#endif

    if (!dst) {
        CHECK(dst = (zend_class_entry*) allocate(sizeof(*src)));
        local_dst_alloc = 1;
    }

    /* Start with a bitwise copy */
    memcpy(dst, src, sizeof(*src));

    dst->name = NULL;
    dst->builtin_functions = NULL;
    memset(&dst->function_table, 0, sizeof(dst->function_table));
    memset(&dst->default_properties, 0, sizeof(dst->default_properties));
#ifndef ZEND_ENGINE_2
    dst->refcount = NULL;
#else
    dst->static_members = NULL;
    dst->doc_comment = NULL;
    memset(&dst->properties_info, 0, sizeof(dst->properties_info));
    memset(&dst->constants_table, 0, sizeof(dst->constants_table));
#endif

    if (src->name) {
        if(!(dst->name = apc_xstrdup(src->name, allocate))) {
            goto cleanup;
        }
    }

#ifndef ZEND_ENGINE_2    
    if(!(dst->refcount = apc_xmemcpy(src->refcount,
                                      sizeof(src->refcount[0]),
                                      allocate))) {
        goto cleanup;
    }
#endif        

    #ifdef __DEBUG_APC__
    fprintf( stderr, "\tAbout to copy the zend_class_entry:function_table... \n" );
    #endif    
    if(!(my_copy_hashtable_ex(&dst->function_table,
                            &src->function_table,
                            (ht_copy_fun_t) my_copy_function,
                            (ht_free_fun_t) my_free_function,
                            0,
                            allocate, deallocate,
                            (ht_check_copy_fun_t) my_check_copy_function,
                            src))) {
        goto cleanup;
    }
    #ifdef __DEBUG_APC__
    fprintf( stderr, "\tSUCCESS!\n\n" );
    #endif    

#ifdef ZEND_ENGINE_2

    /* the interfaces are populated at runtime using ADD_INTERFACE */
    dst->interfaces = NULL; 
   
    /* these will either be set inside my_fixup_hashtable or 
     * they will be copied out from parent inside zend_do_inheritance 
     */
    dst->constructor =  NULL;
    dst->destructor = NULL;
    dst->clone = NULL;
    dst->__get = NULL;
    dst->__set = NULL;
    dst->__call = NULL;
    
    my_fixup_hashtable(&dst->function_table, (ht_fixup_fun_t)my_fixup_function, src, dst);
#endif

    #ifdef __DEBUG_APC__
    fprintf( stderr, "\tAbout to copy the zend_class_entry:default_properties... \n" );
    #endif    
    if(!(my_copy_hashtable(&dst->default_properties,
                            &src->default_properties,
                            (ht_copy_fun_t) my_copy_zval_ptr,
                            (ht_free_fun_t) my_free_zval_ptr,
                            1,
                            allocate,deallocate))) {
        goto cleanup;
    }
    #ifdef __DEBUG_APC__
    fprintf( stderr, "\tSUCCESS!\n\n" );
    #endif    

#ifdef ZEND_ENGINE_2
    
    #ifdef __DEBUG_APC__
    fprintf( stderr, "\tAbout to copy the zend_class_entry:properties_info... \n" );
    #endif    
    if(!(my_copy_hashtable_ex(&dst->properties_info,
                            &src->properties_info,
                            (ht_copy_fun_t) my_copy_property_info,
                            (ht_free_fun_t) my_free_property_info,
                            0,
                            allocate, deallocate,
                            (ht_check_copy_fun_t) my_check_copy_property_info,
                            src))) {
        goto cleanup;
    }
    #ifdef __DEBUG_APC__
    fprintf( stderr, "\tSUCCESS!\n\n" );
    #endif    
    
	/* not sure if statics should be dumped... */
    #ifdef __DEBUG_APC__
    fprintf( stderr, "\tAbout to copy the zend_class_entry:static_members... \n" );
    #endif   

    if(!(dst->static_members = my_copy_hashtable_ex(NULL,
                            src->static_members,
                            (ht_copy_fun_t) my_copy_zval_ptr,
                            (ht_free_fun_t) my_free_zval_ptr,
                            1,
                            allocate, deallocate,
                            (ht_check_copy_fun_t) my_check_copy_static_member,
                            src))) {
        goto cleanup;
    }
    #ifdef __DEBUG_APC__
    fprintf( stderr, "\tSUCCESS!\n\n" );
    #endif    
    
    #ifdef __DEBUG_APC__
    fprintf( stderr, "\tAbout to copy the zend_class_entry:constants_table... \n" );
    #endif    
    if(!(my_copy_hashtable(&dst->constants_table,
                            &src->constants_table,
                            (ht_copy_fun_t) my_copy_zval_ptr,
                            (ht_free_fun_t) my_free_zval_ptr,
                            1,
                            allocate, deallocate))) {
        goto cleanup;
    }
    #ifdef __DEBUG_APC__
    fprintf( stderr, "\tSUCCESS!\n\n" );
    #endif    

    if (src->doc_comment) {
        if(!(dst->doc_comment =
                    apc_xmemcpy(src->doc_comment, src->doc_comment_len+1, allocate))) {
            goto cleanup;
        }
    }
#endif
    
    if (src->builtin_functions) {
        int i, n;

        for (n = 0; src->type == ZEND_INTERNAL_CLASS && src->builtin_functions[n].fname != NULL; n++) {}

        if(!(dst->builtin_functions =
            (zend_function_entry*)
                allocate((n + 1) * sizeof(zend_function_entry)))) {
            goto cleanup;
        }


        for (i = 0; i < n; i++) {
            if(!my_copy_function_entry(&dst->builtin_functions[i],
                                   &src->builtin_functions[i],
                                   allocate, deallocate)) {
                int ii;

                for(ii=i-1; i>=0; i--) my_destroy_function_entry(&dst->builtin_functions[ii], deallocate);
                goto cleanup;
            }
        }
        dst->builtin_functions[n].fname = NULL;
    }
   
#ifdef __DEBUG_APC__
    fprintf(stderr, "\nAbout to dump \"dst\"... \n" );
    my_dump_zend_class_entry(dst);
#endif

    return dst;


cleanup:
    if(dst->name) deallocate(dst->name);
#ifndef ZEND_ENGINE_2
    if(dst->refcount) deallocate(dst->refcount);
#else
    if(dst->doc_comment) deallocate(dst->doc_comment);
#endif
    
    if(dst->builtin_functions) deallocate(dst->builtin_functions);
    if(dst->function_table.arBuckets) my_destroy_hashtable(&dst->function_table, (ht_free_fun_t) my_free_function, deallocate);
    if(dst->default_properties.arBuckets) my_destroy_hashtable(&dst->default_properties, (ht_free_fun_t) my_free_zval_ptr, deallocate);

#ifdef ZEND_ENGINE_2
    if(dst->properties_info.arBuckets) my_destroy_hashtable(&dst->properties_info, (ht_free_fun_t) my_free_property_info, deallocate);
    if(dst->static_members) my_free_hashtable(dst->static_members, (ht_free_fun_t) my_free_zval_ptr, deallocate);
    if(dst->constants_table.arBuckets) my_destroy_hashtable(&dst->constants_table, (ht_free_fun_t) my_free_zval_ptr, deallocate);
#endif
    if(local_dst_alloc) deallocate(dst);

    return NULL;
}
/* }}} */

/* {{{ my_copy_hashtable */
static HashTable* my_copy_hashtable_ex(HashTable* dst,
                                    HashTable* src,
                                    ht_copy_fun_t copy_fn,
                                    ht_free_fun_t free_fn,
                                    int holds_ptrs,
                                    apc_malloc_t allocate, 
                                    apc_free_t deallocate,
                                    ht_check_copy_fun_t check_fn,
                                    ...)
{
    Bucket* curr = NULL;
    Bucket* prev = NULL;
    Bucket* newp = NULL;
    int first = 1;
    int local_dst_alloc = 0;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (HashTable*) allocate(sizeof(src[0])));
        local_dst_alloc = 1;
    }

    memcpy(dst, src, sizeof(src[0]));

#ifdef __DEBUG_APC__
    fprintf(stderr, "\t<my_copy_hashtable>: About to dump src: %p\n", src);
    my_zend_hash_display(src, "\t\t");
    fprintf(stderr,"\n");
#endif
    
    /* allocate buckets for the new hashtable */
    if(!(dst->arBuckets = allocate(dst->nTableSize * sizeof(Bucket*)))) {
        if(local_dst_alloc) deallocate(dst);
        return NULL;
    }

#ifdef __DEBUG_APC__
/*    fprintf(stderr, "\t\t<my_copy_hashtable>: dst->arBuckets: %p\n", dst->arBuckets);*/
#endif
    
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
        if(!(newp =
            (Bucket*) apc_xmemcpy(curr,
                                  sizeof(Bucket) + curr->nKeyLength - 1,
                                  allocate))) {
            curr = curr->pListLast;
            while(curr) {
                int nn = curr->h % dst->nTableSize;
                if(dst->arBuckets[nn]) deallocate(dst->arBuckets[nn]);
                curr = curr->pListLast;
            }
            deallocate(dst->arBuckets);
            if(local_dst_alloc) deallocate(dst);
            return NULL;
        }

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
        if(!(newp->pData = copy_fn(NULL, curr->pData, allocate, deallocate))) {

            deallocate(newp);
            curr = curr->pListLast;
            while(curr) {
                int nn = curr->h % dst->nTableSize;
                if(dst->arBuckets[nn]) {
                    if(free_fn && dst->arBuckets[nn]->pData) free_fn(dst->arBuckets[nn]->pData, deallocate);
                    deallocate(dst->arBuckets[nn]);
                }
                curr = curr->pListLast;
            }
            deallocate(dst->arBuckets);
            if(local_dst_alloc) deallocate(dst);
            else dst->arBuckets = NULL; /* invalid ptr */
            return NULL;
        }

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

#ifdef __DEBUG_APC__
    fprintf(stderr, "\t<my_copy_hashtable>: About to dump dst: %p\n", dst);
    my_zend_hash_display(dst, "\t\t");
    fprintf(stderr,"\n");
#endif

    return dst;
}
/* }}} */

/* {{{ my_copy_static_variables */
static HashTable* my_copy_static_variables(zend_op_array* src, apc_malloc_t allocate, apc_free_t deallocate)
{ 
    if (src->static_variables == NULL) {
        return NULL;
    }

    return my_copy_hashtable(NULL,
                             src->static_variables,
                             (ht_copy_fun_t) my_copy_zval_ptr,
                             (ht_free_fun_t) my_free_zval_ptr,
                             1,
                             allocate, deallocate);
}
/* }}} */

/* {{{ apc_copy_zval */
zval* apc_copy_zval(zval* dst, const zval* src, apc_malloc_t allocate, apc_free_t deallocate)
{
    int local_dst_alloc = 0;
    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zval*) allocate(sizeof(zval)));
        local_dst_alloc = 1;
    }

    if(!my_copy_zval(dst, src, allocate, deallocate)) {
        if(local_dst_alloc) deallocate(dst);
        return NULL;
    }
    return dst; 
}
/* }}} */

#ifdef ZEND_ENGINE_2
/* {{{ apc_fixup_op_array_jumps */
static void apc_fixup_op_array_jumps(zend_op_array *dst, zend_op_array *src )
{
    int i;

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
                zo->op2.u.jmp_addr = dst->opcodes + (zo->op2.u.jmp_addr - src->opcodes);
                break;
            default:
                break;
        }
    }
}
/* }}} */
#endif

/* {{{ apc_copy_op_array */
zend_op_array* apc_copy_op_array(zend_op_array* dst, zend_op_array* src, apc_malloc_t allocate, apc_free_t deallocate TSRMLS_DC)
{
    int i;
    int local_dst_alloc = 0;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zend_op_array*) allocate(sizeof(src[0])));
        local_dst_alloc = 1;
    }
    if(APCG(optimization)) {
        apc_optimize_op_array(src);
    }

    /* start with a bitwise copy of the array */
    memcpy(dst, src, sizeof(src[0]));

    dst->function_name = NULL;
    dst->filename = NULL;
    dst->refcount = NULL;
    dst->opcodes = NULL;
    dst->brk_cont_array = NULL;
    dst->static_variables = NULL;
#ifdef ZEND_ENGINE_2
    dst->try_catch_array = NULL;
    dst->arg_info = NULL;
    dst->doc_comment = NULL;
#else
    dst->arg_types = NULL;
#endif
#ifdef ZEND_ENGINE_2_1
    dst->vars = NULL;
#endif

    /* copy the arg types array (if set) */
#ifdef ZEND_ENGINE_2
    if (src->arg_info) {
        if(!(dst->arg_info = my_copy_arg_info_array(NULL,
                                                src->arg_info,
                                                src->num_args,
                                                allocate,
                                                deallocate))) {
            goto cleanup;
        }
    }
#else    
    if (src->arg_types) {
        if(!(dst->arg_types = apc_xmemcpy(src->arg_types,
                        sizeof(src->arg_types[0]) * (src->arg_types[0]+1),
                        allocate))) {
            goto cleanup;
        }
    }
#endif

    if (src->function_name) {
        if(!(dst->function_name = apc_xstrdup(src->function_name, allocate))) {
            goto cleanup;
        }
    }
    if (src->filename) {
        if(!(dst->filename = apc_xstrdup(src->filename, allocate))) {
            goto cleanup;
        }
    }

    if(!(dst->refcount = apc_xmemcpy(src->refcount,
                                      sizeof(src->refcount[0]),
                                      allocate))) {
        goto cleanup;
    }

    /* deep-copy the opcodes */
    if(!(dst->opcodes = (zend_op*) allocate(sizeof(zend_op) * src->last))) {
        goto cleanup;
    }
    
    for (i = 0; i < src->last; i++) {
        if(!(my_copy_zend_op(dst->opcodes+i, src->opcodes+i, allocate, deallocate))) {
            int ii;
            for(ii = i-1; i>=0; ii--) my_destroy_zend_op(dst->opcodes+ii, deallocate);
            goto  cleanup;
        }
    }

#ifdef ZEND_ENGINE_2
    apc_fixup_op_array_jumps(dst,src);
#endif

    /* copy the break-continue array */
    if (src->brk_cont_array) {
        if(!(dst->brk_cont_array =
            apc_xmemcpy(src->brk_cont_array,
                        sizeof(src->brk_cont_array[0]) * src->last_brk_cont,
                        allocate))) {
            goto cleanup_opcodes;
        }
    }

    /* copy the table of static variables */
    if (src->static_variables) {
        if(!(dst->static_variables = my_copy_static_variables(src, allocate, deallocate))) {
            goto cleanup_opcodes;
        }
    }
    
#ifdef ZEND_ENGINE_2
    if (src->try_catch_array) {
        if(!(dst->try_catch_array = 
                apc_xmemcpy(src->try_catch_array,
                        sizeof(src->try_catch_array[0]) * src->last_try_catch,
                        allocate))) {
            goto cleanup_opcodes;
        }
    }
#endif

#ifdef ZEND_ENGINE_2_1 /* PHP 5.1 */
    if (src->vars) {
        if(!(dst->vars = apc_xmemcpy(src->vars,
                            sizeof(src->vars[0]) * src->last_var,
                            allocate))) {
            goto cleanup_opcodes;
        }
        
        for(i = 0; i <  src->last_var; i++) dst->vars[i].name = NULL;
        
        for(i = 0; i <  src->last_var; i++) {
            if(!(dst->vars[i].name = apc_xmemcpy(src->vars[i].name,
                                src->vars[i].name_len + 1,
                                allocate))) {
                goto cleanup_opcodes;
            }
        }
    }
#endif

#ifdef ZEND_ENGINE_2
    if (src->doc_comment) {
        if (!(dst->doc_comment 
                = apc_xmemcpy(src->doc_comment, src->doc_comment_len+1, allocate))) {
            goto cleanup_opcodes;
        }
    }
#endif
    
    return dst;

cleanup_opcodes:
    if(dst->opcodes) {
        for(i=0; i < src->last; i++) my_destroy_zend_op(dst->opcodes+i, deallocate);
    }
cleanup:
    if(dst->function_name) deallocate(dst->function_name);
    deallocate(dst->refcount);
    if(dst->filename) deallocate(dst->filename);
#ifdef ZEND_ENGINE_2
    if(dst->arg_info) my_free_arg_info_array(dst->arg_info, dst->num_args, deallocate);
    if(dst->try_catch_array) deallocate(dst->try_catch_array);
    if(dst->doc_comment) deallocate(dst->doc_comment);
#else
    if(dst->arg_types) deallocate(dst->arg_types);
#endif
    if(dst->opcodes) deallocate(dst->opcodes);
    if(dst->brk_cont_array) deallocate(dst->brk_cont_array);
    if(dst->static_variables) my_free_hashtable(dst->static_variables, (ht_free_fun_t)my_free_zval_ptr, (apc_free_t)deallocate);
#ifdef ZEND_ENGINE_2_1
     for(i=0; i < src->last_var; i++) {
         if(dst->vars[i].name) deallocate(dst->vars[i].name);
    }
#endif
    if(local_dst_alloc) deallocate(dst);
    return NULL;
}
/* }}} */

/* {{{ apc_copy_new_functions */
apc_function_t* apc_copy_new_functions(int old_count, apc_malloc_t allocate, apc_free_t deallocate TSRMLS_DC)
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

        if(!(array[i].name = apc_xmemcpy(key, (int) key_size, allocate))) {
            int ii;
            for(ii=i-1; ii>=0; ii--) {
                deallocate(array[ii].name);
                my_free_function(array[ii].function, deallocate);
            }
            deallocate(array);
            return NULL;
        }
        array[i].name_len = (int) key_size-1;
        if(!(array[i].function = my_copy_function(NULL, fun, allocate, deallocate))) {
            int ii;
            deallocate(array[i].name);
            for(ii=i-1; ii>=0; ii--) {
                deallocate(array[ii].name);
                my_free_function(array[ii].function, deallocate);
            }
            deallocate(array);
            return NULL;
        }
        zend_hash_move_forward(CG(function_table));
    }

    array[i].function = NULL;
    return array;
}
/* }}} */

/* {{{ apc_copy_new_classes */
apc_class_t* apc_copy_new_classes(zend_op_array* op_array, int old_count, apc_malloc_t allocate, apc_free_t deallocate TSRMLS_DC)
{
    apc_class_t* array;
    int new_count;              /* number of new classes in table */
    int i;
    HashPosition pos;
#ifdef __DEBUG_APC__
    int debug_ret_val = 0;
#endif
    
    new_count = zend_hash_num_elements(CG(class_table)) - old_count;
    assert(new_count >= 0);

    CHECK(array =
        (apc_class_t*)
            allocate(sizeof(apc_class_t)*(new_count+1)));
    
    if (new_count == 0) {
        array[0].class_entry = NULL;
        return array;
    }

#ifdef __DEBUG_APC__
    fprintf(stderr, "<apc_copy_new_classes> new_count: %d\n", new_count);
#endif

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

#ifdef __DEBUG_APC__
       debug_ret_val =
#endif
        zend_hash_get_current_key_ex(CG(class_table),
                                     &key,
                                     &key_size,
                                     NULL,
                                     0,
                                     NULL);

       zend_hash_get_current_data(CG(class_table), (void**) &elem);
  
#ifdef __DEBUG_APC__
        fprintf(stderr, "<apc_copy_new_classes> zend_hash_get_current_key_ex: %d -> <%s, %u>\n", debug_ret_val, key, key_size);
#endif       
        
#ifdef ZEND_ENGINE_2
		elem = *((zend_class_entry**)elem);
#endif
        
        if(!(array[i].name = apc_xmemcpy(key, (int) key_size, allocate))) {
            int ii;

            for(ii=i-1; ii>=0; ii--) {
                deallocate(array[ii].name);
                my_destroy_class_entry(array[ii].class_entry, deallocate);
                deallocate(array[ii].class_entry);
            }
            deallocate(array);
            return NULL;
        }
        array[i].name_len = (int) key_size-1;
        if(!(array[i].class_entry = my_copy_class_entry(NULL, elem, allocate, deallocate))) {
            int ii;
            
            deallocate(array[i].name);
            for(ii=i-1; ii>=0; ii--) {
                deallocate(array[ii].name);
                my_destroy_class_entry(array[ii].class_entry, deallocate);
                deallocate(array[ii].class_entry);
            }
            deallocate(array);
#ifdef __DEBUG_APC__
            fprintf(stderr, "Oops! my_copy_class_entry failed! :-(\n");
#endif
            return NULL;
        }

        /*
         * If the class has a pointer to its parent class, save the parent
         * name so that we can enable compile-time inheritance when we reload
         * the child class; otherwise, set the parent name to null and scan
         * the op_array to determine if this class inherits from some base
         * class at execution-time.
         */

        if (elem->parent) {
            if(!(array[i].parent_name =
                apc_xstrdup(elem->parent->name, allocate))) {
                int ii;
                 
                for(ii=i; ii>=0; ii--) {
                    deallocate(array[ii].name);
                    my_destroy_class_entry(array[ii].class_entry, deallocate);
                    deallocate(array[ii].class_entry);
                    if(ii==i) continue;
                    if(array[ii].parent_name) deallocate(array[ii].parent_name);
                }
                deallocate(array);
                return NULL;
            }
            array[i].is_derived = 1;
#ifdef ZEND_ENGINE_2
            /*
             * CG(class_table) is in lower-case but 
             * elem->parent->name preserves case
             */
        	zend_str_tolower_copy(array[i].parent_name, elem->parent->name, strlen(elem->parent->name)+1);            
#endif
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
    switch (src->type & ~IS_CONSTANT_INDEX) {
    case IS_RESOURCE:
    case IS_BOOL:
    case IS_LONG:
    case IS_DOUBLE:
    case IS_NULL:
        break;

    case IS_CONSTANT:
    case IS_STRING:
#ifndef ZEND_ENGINE_2        
    case FLAG_IS_BC:
#endif        
        deallocate(src->value.str.val);
        break;
    
    case IS_ARRAY:
    case IS_CONSTANT_ARRAY:
        my_free_hashtable(src->value.ht,
                          (ht_free_fun_t) my_free_zval_ptr,
                          deallocate);
        break;

    case IS_OBJECT:
#ifndef ZEND_ENGINE_2        
        my_destroy_class_entry(src->value.obj.ce, deallocate);
        deallocate(src->value.obj.ce);
        my_free_hashtable(src->value.obj.properties,
                          (ht_free_fun_t) my_free_zval_ptr,
                          deallocate);
#endif        
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
        break;
        
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
#ifdef ZEND_ENGINE_2    
    if (src->arg_info) {
            my_free_arg_info_array(src->arg_info, src->num_args, deallocate);
    }
#else
    if (src->func_arg_types) {
        deallocate(src->func_arg_types);
    }
#endif    
}
/* }}} */

#ifdef ZEND_ENGINE_2    
/* {{{ my_destroy_property_info*/
static void my_destroy_property_info(zend_property_info* src, apc_free_t deallocate)
{
    assert(src != NULL);

    deallocate(src->name);
    if(src->doc_comment) deallocate(src->doc_comment);
}
/* }}} */

/* {{{ my_destroy_arg_info_array */
static void my_destroy_arg_info_array(zend_arg_info* src, uint num_args, apc_free_t deallocate)
{
    int i = 0;
    
    assert(src != NULL);

    for(i=0; i < num_args; i++) {
        my_destroy_arg_info(&src[i], deallocate);
    }
}
/* }}} */

/* {{{ my_destroy_arg_info */
static void my_destroy_arg_info(zend_arg_info* src, apc_free_t deallocate)
{
    assert(src != NULL);

    deallocate(src->name);
    deallocate(src->class_name);
}
/* }}} */
#endif    

/* {{{ my_destroy_class_entry */
static void my_destroy_class_entry(zend_class_entry* src, apc_free_t deallocate)
{
    int i;

    assert(src != NULL);

    deallocate(src->name);
#ifndef ZEND_ENGINE_2    
    deallocate(src->refcount);
#endif

    my_destroy_hashtable(&src->function_table,
                         (ht_free_fun_t) my_free_function,
                         deallocate);

    my_destroy_hashtable(&src->default_properties,
                         (ht_free_fun_t) my_free_zval_ptr,
                         deallocate);

#ifdef ZEND_ENGINE_2
    my_destroy_hashtable(src->static_members,
                         (ht_free_fun_t) my_free_zval_ptr,
                         deallocate);
    deallocate(src->static_members);
#endif

    if (src->builtin_functions) {
        for (i = 0; src->builtin_functions[i].fname != NULL; i++) {
            my_destroy_function_entry(&src->builtin_functions[i], deallocate);
        }
        deallocate(src->builtin_functions);
    }

#ifdef ZEND_ENGINE_2
#warning "TODO: my_destroy_class_entry leaks memory in ZEND_ENGINE_2!"
#endif
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

#ifdef ZEND_ENGINE_2
    if (src->arg_info) {
        my_free_arg_info_array(src->arg_info, src->num_args, deallocate);
    }
#else    
    if (src->arg_types) {
        deallocate(src->arg_types);
    }
#endif

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
    
#ifdef ZEND_ENGINE_2
#warning "TODO: my_destroy_op_array leaks memory of vars & try-catch in ZEND_ENGINE_2!"
    if (src->doc_comment) {
        deallocate(src->doc_comment);
    }
#endif
}
/* }}} */

/* {{{ my_free_zval_ptr */
static void my_free_zval_ptr(zval** src, apc_free_t deallocate)
{
    my_destroy_zval_ptr(src, deallocate);
    deallocate(src);
}
/* }}} */

#ifdef ZEND_ENGINE_2
/* {{{ my_free_property_info */
static void my_free_property_info(zend_property_info* src, apc_free_t deallocate)
{
    my_destroy_property_info(src, deallocate);
    deallocate(src);
}
/* }}} */

/* {{{ my_free_arg_info_array */
static void my_free_arg_info_array(zend_arg_info* src, uint num_args, apc_free_t deallocate)
{
    my_destroy_arg_info_array(src, num_args, deallocate);
    deallocate(src);
}
/* }}} */

/* {{{ my_free_arg_info */
static void my_free_arg_info(zend_arg_info* src, apc_free_t deallocate)
{
    my_destroy_arg_info(src, deallocate);
    deallocate(src);
}
/* }}} */
#endif

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

/* {{{ apc_free_zval */
void apc_free_zval(zval* src, apc_free_t deallocate)
{
    if (src != NULL) {
        my_destroy_zval(src, deallocate);
        deallocate(src);
    }
}
/* }}} */

/* {{{ apc_copy_op_array_for_execution */
zend_op_array* apc_copy_op_array_for_execution(zend_op_array* src)
{
    zend_op_array* dst = (zend_op_array*) emalloc(sizeof(src[0]));
    memcpy(dst, src, sizeof(src[0]));
    dst->static_variables = my_copy_static_variables(src, apc_php_malloc, apc_php_free);
    /*check_op_array_integrity(dst);*/
    return dst;
}
/* }}} */

/* {{{ apc_copy_function_for_execution */
zend_function* apc_copy_function_for_execution(zend_function* src)
{
    zend_function* dst = (zend_function*) emalloc(sizeof(src[0]));
    memcpy(dst, src, sizeof(src[0]));
    dst->op_array.static_variables = my_copy_static_variables(&dst->op_array, apc_php_malloc, apc_php_free);
    /*check_op_array_integrity(&dst->op_array);*/
    return dst;
}
/* }}} */

/* {{{ apc_copy_function_for_execution_ex */
zend_function* apc_copy_function_for_execution_ex(void *dummy, zend_function* src)
{
    if(src->type==ZEND_INTERNAL_FUNCTION || src->type==ZEND_OVERLOADED_FUNCTION) return src;
    return apc_copy_function_for_execution(src);
}
/* }}} */

/* {{{ apc_copy_class_entry_for_execution */
zend_class_entry* apc_copy_class_entry_for_execution(zend_class_entry* src, int is_derived)
{
    zend_class_entry* dst = (zend_class_entry*) emalloc(sizeof(src[0]));
    memcpy(dst, src, sizeof(src[0]));

#ifdef __DEBUG_APC__
    fprintf(stderr, "<apc_copy_class_entry_for_execution>\n");
#endif

#ifdef ZEND_ENGINE_2
    /* These are slots to be populated later by ADD_INTERFACE insns */
    dst->interfaces = apc_php_malloc(sizeof(zend_class_entry*) * 
                                        src->num_interfaces);
    memset(dst->interfaces, 0, sizeof(zend_class_entry*) * src->num_interfaces);
#endif

    /* Deep-copy the class properties, because they will be modified */

#ifdef __DEBUG_APC__
    fprintf(stderr, "About to copy default_properties...\n");
#endif
    my_copy_hashtable(&dst->default_properties,
                      &src->default_properties,
                      (ht_copy_fun_t) my_copy_zval_ptr,
                      (ht_free_fun_t) my_free_zval_ptr,
                      1,
                      apc_php_malloc, apc_php_free);

#ifdef __DEBUG_APC__
    fprintf(stderr, "About to copy static_members...\n");
#endif
    dst->static_members = my_copy_hashtable(NULL,
                      src->static_members,
                      (ht_copy_fun_t) my_copy_zval_ptr,
                      (ht_free_fun_t) my_free_zval_ptr,
                      1,
                      apc_php_malloc, apc_php_free);

    /* For derived classes, we must also copy the function hashtable (although
     * we can merely bitwise copy the functions it contains) */

#ifdef __DEBUG_APC__
    fprintf(stderr, "About to copy function_table...\n");
#endif
    my_copy_hashtable(&dst->function_table,
                      &src->function_table,
                      (ht_copy_fun_t) apc_copy_function_for_execution_ex,
                      NULL,
                      0,
                      apc_php_malloc, apc_php_free);
#ifdef ZEND_ENGINE_2
    my_fixup_hashtable(&dst->function_table, (ht_fixup_fun_t)my_fixup_function_for_execution, src, dst);

    /* zend_do_inheritance merges properties_info.
     * TODO: figure out whether we really need to deep-copy.
     */
    my_copy_hashtable(&dst->properties_info,
                      &src->properties_info,
                      (ht_copy_fun_t) my_copy_property_info,
                      NULL,
                      0,
                      apc_php_malloc, apc_php_free);
#endif

    return dst;
}
/* }}} */

#ifdef ZEND_ENGINE_2

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
         */
        SET_IF_SAME_NAME(constructor);
        SET_IF_SAME_NAME(destructor);
        SET_IF_SAME_NAME(clone);
        SET_IF_SAME_NAME(__get);
        SET_IF_SAME_NAME(__set);
        SET_IF_SAME_NAME(__call);
        zf->common.scope = dst;
    }
    else
    {
        /* no other function should reach here */
        assert(0);
    }

#ifdef __DEBUG_APC__
    fprintf(stderr, " <%s>->scope = %s\n" , zf->common.function_name, zf->common.scope->name);
#endif
    #undef SET_IF_SAME_NAME
}
/* }}} */

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
    
    if (zf->common.scope == src) {
        return 1;
    }
    
    return 0;
}
/* }}} */

/* {{{ my_check_copy_property_info */
static int my_check_copy_property_info(Bucket* p, va_list args)
{
    zend_class_entry* src = va_arg(args, zend_class_entry*);
    zend_class_entry* parent = src->parent;
    zend_property_info* child_info = (zend_property_info*)p->pData;
    zend_property_info* parent_info = NULL;

	if (parent &&
        zend_hash_quick_find(&src->properties_info, p->arKey, p->nKeyLength, 
            p->h, (void **) &parent_info)==FAILURE) {
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
    zend_class_entry* parent = src->parent;
    char * member_name;
    char * class_name;

    zend_property_info *parent_info = NULL;
    zend_property_info *child_info = NULL;

    if(!parent) {
        return 1;
    }

    /* these do not need free'ing */
    zend_unmangle_property_name(p->arKey, &class_name, &member_name);

    /* please refer do_inherit_property_access_check in zend_compile.c
     * to understand why we lookup in properties_info.
     */
    if((zend_hash_find(&parent->properties_info, member_name, 
                        strlen(member_name)+1, (void**)&parent_info) == SUCCESS)
        &&
        (zend_hash_find(&src->properties_info, member_name,
                        strlen(member_name)+1, (void**)&child_info) == SUCCESS))
    {
        if(child_info->flags & ZEND_ACC_STATIC &&    
            (parent_info->flags & ZEND_ACC_PROTECTED &&
            child_info->flags & ZEND_ACC_PUBLIC))
        {
            /* Do not copy into static_members. zend_do_inheritance
             * will automatically insert a NULL value.
             * TODO: decrement refcount or fixup when copying out for exec ? 
             */ 
            return 0;
        }
    }
    
    return 1;
}
/* }}} */
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
