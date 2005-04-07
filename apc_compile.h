/* 
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,       |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/3_0.txt.                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef APC_COMPILE_H
#define APC_COMPILE_H

/*
 * This module encapsulates most of the complexity involved in deep-copying
 * the Zend compiler data structures. The routines are allocator-agnostic, so
 * the same function can be used for copying to and from shared memory.
 */

#include "apc.h"
#include "apc_php.h"

/* {{{ struct definition: apc_function_t */
typedef struct apc_function_t apc_function_t;
struct apc_function_t {
    char* name;                 /* the function name */
    int name_len;               /* length of name */
    zend_function* function;    /* the zend function data structure */
};
/* }}} */

/* {{{ struct definition: apc_class_t */
typedef struct apc_class_t apc_class_t;
struct apc_class_t {
    char* name;                     /* the class name */
    int name_len;                   /* length of name */
    int is_derived;                 /* true if this is a derived class */
    char* parent_name;              /* the parent class name */
    zend_class_entry* class_entry;  /* the zend class data structure */
};
/* }}} */

/*
 * These are the top-level copy functions.
 */

extern zend_op_array* apc_copy_op_array(zend_op_array* dst, zend_op_array* src, apc_malloc_t allocate, apc_free_t deallocate TSRMLS_DC);
extern zend_class_entry* apc_copy_class_entry(zend_class_entry* dst, zend_class_entry* src, apc_malloc_t allocate, apc_free_t deallocate);
extern apc_function_t* apc_copy_new_functions(int old_count, apc_malloc_t allocate, apc_free_t deallocate TSRMLS_DC);
extern apc_class_t* apc_copy_new_classes(zend_op_array* op_array, int old_count, apc_malloc_t allocate, apc_free_t deallocate TSRMLS_DC);
extern zval* apc_copy_zval(zval* dst, const zval* src, apc_malloc_t allocate, apc_free_t deallocate);

/*
 * Deallocation functions corresponding to the copy functions above.
 */

extern void apc_free_op_array(zend_op_array* src, apc_free_t deallocate);
extern void apc_free_functions(apc_function_t* src, apc_free_t deallocate);
extern void apc_free_classes(apc_class_t* src, apc_free_t deallocate);
extern void apc_free_zval(zval* src, apc_free_t deallocate);

/*
 * These "copy-for-execution" functions must be called after retrieving an
 * object from the shared cache. They do the minimal amount of work necessary
 * to allow multiple processes to concurrently execute the same VM data
 * structures.
 */

extern zend_op_array* apc_copy_op_array_for_execution(zend_op_array* src);
extern zend_function* apc_copy_function_for_execution(zend_function* src);
extern zend_class_entry* apc_copy_class_entry_for_execution(zend_class_entry* src, int is_derived);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
