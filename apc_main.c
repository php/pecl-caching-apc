/* 
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/3_0.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "apc_php.h"
#include "apc_main.h"
#include "apc.h"
#include "apc_cache.h"
#include "apc_compile.h"
#include "apc_globals.h"
#include "apc_sma.h"
#include "apc_stack.h"
#include "apc_zend.h"
#include "SAPI.h"
#if HAVE_APACHE
#undef XtOffsetOf
#include "httpd.h"
#endif

/* {{{ module variables */

/* pointer to the original Zend engine compile_file function */
static zend_op_array* (*old_compile_file)
    (zend_file_handle*, int TSRMLS_DC);

/* }}} */

/* {{{ install_function */
static int install_function(apc_function_t fn TSRMLS_DC)
{
    int status =
        zend_hash_add(EG(function_table),
                      fn.name,
                      fn.name_len+1,
                      apc_copy_function_for_execution(fn.function),
                      sizeof(fn.function[0]),
                      NULL);

    if (status == FAILURE) {
        /* XXX: is there any way for us to handle this case? */
    }

    return status;
}
/* }}} */

/* {{{ install_class */
static int install_class(apc_class_t cl TSRMLS_DC)
{
    zend_class_entry* class_entry = cl.class_entry;
    zend_class_entry* parent;
    int status;

    class_entry =
        apc_copy_class_entry_for_execution(cl.class_entry,
                                           cl.is_derived);

    /* restore parent class pointer for compile-time inheritance */
    if (cl.parent_name != NULL) {
        status = zend_hash_find(EG(class_table),
                                cl.parent_name,
                                strlen(cl.parent_name)+1,
                                (void**) &parent);
        
        if (status == FAILURE) {
            class_entry->parent = NULL;
        }
        else {
            class_entry->parent = parent;
        }
    }

    status = zend_hash_add(EG(class_table),
                           cl.name,
                           cl.name_len+1,
                           class_entry,
                           sizeof(zend_class_entry),
                           NULL);
    return status;
}
/* }}} */

/* {{{ cached_compile */
static zend_op_array* cached_compile(TSRMLS_D)
{
    apc_cache_entry_t* cache_entry;
    int i;

    cache_entry = (apc_cache_entry_t*) apc_stack_top(APCG(cache_stack));
    assert(cache_entry != NULL);

    if (cache_entry->data.file.functions) {
        for (i = 0; cache_entry->data.file.functions[i].function != NULL; i++) {
            install_function(cache_entry->data.file.functions[i] TSRMLS_CC);
        }
    }

    if (cache_entry->data.file.classes) {
        for (i = 0; cache_entry->data.file.classes[i].class_entry != NULL; i++) {
            install_class(cache_entry->data.file.classes[i] TSRMLS_CC);
        }
    }

    return apc_copy_op_array_for_execution(cache_entry->data.file.op_array);
}
/* }}} */

/* {{{ my_compile_file
   Overrides zend_compile_file */
static zend_op_array* my_compile_file(zend_file_handle* h,
                                               int type TSRMLS_DC)
{
    apc_cache_key_t key;
    apc_cache_entry_t* cache_entry;
    zend_op_array* op_array;
    int num_functions, num_classes, ret;
    zend_op_array* alloc_op_array;
    apc_function_t* alloc_functions;
    apc_class_t* alloc_classes;
    time_t t;

    /* check our regular expression filters first */
    if (APCG(compiled_filters)) {
        int ret = apc_regex_match_array(APCG(compiled_filters), h->filename);
        if(ret == APC_NEGATIVE_MATCH || (ret != APC_POSITIVE_MATCH && !APCG(cache_by_default))) {
            return old_compile_file(h, type TSRMLS_CC);
        }
    } else if(!APCG(cache_by_default)) {
        return old_compile_file(h, type TSRMLS_CC);
    }

#if HAVE_APACHE
    /* Save a syscall here under Apache.  This should actually be a SAPI call instead
     * but we don't have that yet.  Once that is introduced in PHP this can be cleaned up  -Rasmus */
    t = ((request_rec *)SG(server_context))->request_time;
#else
    t = time(0);
#endif

    /* try to create a cache key; if we fail, give up on caching */
    if (!apc_cache_make_file_key(&key, h->filename, PG(include_path), t TSRMLS_CC)) {
        return old_compile_file(h, type TSRMLS_CC);
    }
    
    /* search for the file in the cache */
    cache_entry = apc_cache_find(APCG(cache), key, t);
    if (cache_entry != NULL) {
        int dummy = 1;
        if (h->opened_path == NULL) {
            h->opened_path = estrdup(cache_entry->data.file.filename);
        }
        zend_hash_add(&EG(included_files), h->opened_path, strlen(h->opened_path)+1, (void *)&dummy, sizeof(int), NULL);
        zend_llist_add_element(&CG(open_files), h); /* XXX kludge */
        apc_stack_push(APCG(cache_stack), cache_entry);
        return cached_compile(TSRMLS_C);
    }

    /* remember how many functions and classes existed before compilation */
    num_functions = zend_hash_num_elements(CG(function_table));
    num_classes   = zend_hash_num_elements(CG(class_table));
    
    /* compile the file using the default compile function */
    op_array = old_compile_file(h, type TSRMLS_CC);
    if (op_array == NULL) {
        return NULL;
    }

    HANDLE_BLOCK_INTERRUPTIONS();
    if(!(alloc_op_array = apc_copy_op_array(NULL, op_array, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_cache_expunge(APCG(cache),t);
        HANDLE_UNBLOCK_INTERRUPTIONS();
        apc_log(APC_WARNING, "(apc_copy_op_array) unable to cache '%s': insufficient " "shared memory available", h->opened_path?h->opened_path:"");
        return op_array;
    }
    
    if(!(alloc_functions = apc_copy_new_functions(num_functions, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_cache_expunge(APCG(cache),t);
        HANDLE_UNBLOCK_INTERRUPTIONS();
        apc_log(APC_WARNING, "(apc_copy_new_functions) unable to cache '%s': insufficient " "shared memory available", h->opened_path?h->opened_path:"");
        return op_array;
    }
    if(!(alloc_classes = apc_copy_new_classes(op_array, num_classes, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_free_functions(alloc_functions, apc_sma_free);
        apc_cache_expunge(APCG(cache),t);
        HANDLE_UNBLOCK_INTERRUPTIONS();
        apc_log(APC_WARNING, "(apc_copy_new_classes) unable to cache '%s': insufficient " "shared memory available", h->opened_path?h->opened_path:"");
        return op_array;
    }

    if(!(cache_entry = apc_cache_make_file_entry(h->opened_path, alloc_op_array, alloc_functions, alloc_classes))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_free_functions(alloc_functions, apc_sma_free);
        apc_free_classes(alloc_classes, apc_sma_free);
        apc_cache_expunge(APCG(cache),t);
        HANDLE_UNBLOCK_INTERRUPTIONS();
        apc_log(APC_WARNING, "(apc_cache_make_file_entry) unable to cache '%s': insufficient " "shared memory available", h->opened_path?h->opened_path:"");
        return op_array;
    }
    HANDLE_UNBLOCK_INTERRUPTIONS();

    if ((ret = apc_cache_insert(APCG(cache), key, cache_entry, t)) != 1) {
        apc_cache_free_entry(cache_entry);
        if(ret==-1) {
            apc_cache_expunge(APCG(cache),t);
            apc_log(APC_WARNING, "(apc_cache_insert) unable to cache '%s': insufficient " "shared memory available", h->opened_path?h->opened_path:"");
        }
    }

    return op_array;
}
/* }}} */

/* {{{ module init and shutdown */

int apc_module_init()
{
    TSRMLS_FETCH();
    /* apc initialization */
#if APC_MMAP
    apc_sma_init(APCG(shm_segments), APCG(shm_size)*1024*1024, APCG(mmap_file_mask));
#else
    apc_sma_init(APCG(shm_segments), APCG(shm_size)*1024*1024, NULL);
#endif
    APCG(cache) = apc_cache_create(APCG(num_files_hint), APCG(gc_ttl), APCG(ttl));
    APCG(user_cache) = apc_cache_create(APCG(user_entries_hint), APCG(gc_ttl), APCG(user_ttl));
    APCG(user_cache_stack) = apc_stack_create(0);
    APCG(cache_stack) = apc_stack_create(0);
    APCG(compiled_filters) = apc_regex_compile_array(APCG(filters));

    /* override compilation */
    old_compile_file = zend_compile_file;
    zend_compile_file = my_compile_file;
    
    APCG(initialized) = 1;
    return 0;
}

int apc_module_shutdown()
{
	TSRMLS_FETCH();
    if (!APCG(initialized))
        return 0;

    /* restore compilation */
    zend_compile_file = old_compile_file;

    /* 
     * In case we got interrupted by a SIGTERM or something else during execution
     * we may have cache entries left on the stack that we need to check to make
     * sure that any functions or classes these may have added to the global function
     * and class tables are removed before we blow away the memory that hold them
     */
    while (apc_stack_size(APCG(cache_stack)) > 0) {
        int i;
        apc_cache_entry_t* cache_entry = (apc_cache_entry_t*) apc_stack_pop(APCG(cache_stack));
        if (cache_entry->data.file.functions) {
            for (i = 0; cache_entry->data.file.functions[i].function != NULL; i++) {
                zend_hash_del(EG(function_table),
                cache_entry->data.file.functions[i].name,
                cache_entry->data.file.functions[i].name_len+1);
            }
        }
        if (cache_entry->data.file.classes) {
            for (i = 0; cache_entry->data.file.classes[i].class_entry != NULL; i++) {
                zend_hash_del(EG(class_table),
                cache_entry->data.file.classes[i].name,
                cache_entry->data.file.classes[i].name_len+1);
            }
        }
        apc_cache_free_entry(cache_entry);
    }

    /* apc cleanup */
    apc_stack_destroy(APCG(cache_stack));
    apc_stack_destroy(APCG(user_cache_stack));
    apc_cache_destroy(APCG(cache));
    apc_cache_destroy(APCG(user_cache));
    apc_sma_cleanup();

    APCG(initialized) = 0;
    return 0;
}

/* }}} */

/* {{{ request init and shutdown */

int apc_request_init()
{
	TSRMLS_FETCH();
    apc_stack_clear(APCG(cache_stack));
    apc_stack_clear(APCG(user_cache_stack));
    return 0;
}

int apc_request_shutdown()
{
    apc_deactivate();
    return 0;
}

/* }}} */

/* {{{ apc_deactivate */
void apc_deactivate()
{
	TSRMLS_FETCH();
    /* The execution stack was unwound, which prevented us from decrementing
     * the reference counts on active cache entries in `my_execute`.
     */
    while (apc_stack_size(APCG(cache_stack)) > 0) {
        apc_cache_entry_t* cache_entry =
            (apc_cache_entry_t*) apc_stack_pop(APCG(cache_stack));
        apc_cache_release(APCG(cache), cache_entry);
    }
    while (apc_stack_size(APCG(user_cache_stack)) > 0) {
        apc_cache_entry_t* cache_entry =
            (apc_cache_entry_t*) apc_stack_pop(APCG(user_cache_stack));
        apc_cache_release(APCG(user_cache), cache_entry);
    }
}
/* }}} */

/* {{{ apc_version */
const char* apc_version()
{
    return "2.1.0";
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
