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

/* {{{ module variables */

/* pointer to the original Zend engine compile_file function */
static zend_op_array* (*old_compile_file)
    (zend_file_handle*, int TSRMLS_DC);

/* pointer to the original Zend engine execute function */
static void (*old_execute)(zend_op_array* TSRMLS_DC);

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

    if (cache_entry->functions) {
        for (i = 0; cache_entry->functions[i].function != NULL; i++) {
            install_function(cache_entry->functions[i] TSRMLS_CC);
        }
    }

    if (cache_entry->classes) {
        for (i = 0; cache_entry->classes[i].class_entry != NULL; i++) {
            install_class(cache_entry->classes[i] TSRMLS_CC);
        }
    }

    return apc_copy_op_array_for_execution(cache_entry->op_array);
}
/* }}} */

/* {{{ cache_compile_results */
static void cache_compile_results(apc_cache_key_t key,
                                  const char* filename,
                                  zend_op_array* op_array,
                                  apc_function_t* functions,
                                  apc_class_t* classes)
{
    apc_cache_entry_t* cache_entry;
	TSRMLS_FETCH();

    /* Abort if any component could not be allocated */
    if (!op_array || !functions || !classes) {
        apc_log(APC_WARNING, "unable to cache '%s': insufficient "
                "shared memory available", filename);
        return;
    }

    cache_entry = apc_cache_make_entry(filename, op_array, functions, classes);
    if (!apc_cache_insert(APCG(cache), key, cache_entry)) {
        apc_cache_free_entry(cache_entry);
    }
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
    int num_functions, num_classes;

    /* check our regular expression filters first */
    if (APCG(compiled_filters)) {
        int ret = apc_regex_match_array(APCG(compiled_filters), h->filename);
        if(ret == APC_NEGATIVE_MATCH || (ret != APC_POSITIVE_MATCH && !APCG(cache_by_default))) {
            return old_compile_file(h, type TSRMLS_CC);
        }
    } else if(!APCG(cache_by_default)) {
        return old_compile_file(h, type TSRMLS_CC);
    }

    /* try to create a cache key; if we fail, give up on caching */
    if (!apc_cache_make_key(&key, h->filename, PG(include_path) TSRMLS_CC)) {
        return old_compile_file(h, type TSRMLS_CC);
    }
    
    /* search for the file in the cache */
    cache_entry = apc_cache_find(APCG(cache), key);
    if (cache_entry != NULL) {
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

    /* cache the compiler results */
    cache_compile_results(
        key,
        h->opened_path,
        apc_copy_op_array(NULL, op_array, apc_sma_malloc TSRMLS_CC),
        apc_copy_new_functions(num_functions, apc_sma_malloc TSRMLS_CC),
        apc_copy_new_classes(op_array, num_classes, apc_sma_malloc TSRMLS_CC));

    return op_array;
}
/* }}} */

/* {{{ my_execute */
static void my_execute(zend_op_array* op_array TSRMLS_DC)
{
    old_execute(op_array TSRMLS_CC);

    if (apc_stack_size(APCG(cache_stack)) > 0) {
        apc_cache_entry_t* cache_entry =
            (apc_cache_entry_t*) apc_stack_top(APCG(cache_stack));

        /* compare pointers to determine if op_array's are same */
        if (cache_entry->op_array->opcodes == op_array->opcodes) {
            apc_stack_pop(APCG(cache_stack));
            apc_cache_release(APCG(cache), cache_entry);
        }
    }
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
    apc_sma_init(APCG(shm_segments), APCG(shm_size)*1024*1024);
#endif
    APCG(cache) = apc_cache_create(APCG(num_files_hint), APCG(gc_ttl));
    APCG(cache_stack) = apc_stack_create(0);
    APCG(compiled_filters) = apc_regex_compile_array(APCG(filters));

    /* override compilation */
    old_compile_file = zend_compile_file;
    zend_compile_file = my_compile_file;
    
    /* override execution */
    old_execute = zend_execute;
    zend_execute = my_execute;
    
#if 0
	/* startup output is not good for stuff like fastcgi */
    apc_log(APC_NOTICE, "APC version %s -- startup complete", apc_version());
#endif

    APCG(initialized) = 1;
    return 0;
}

int apc_module_shutdown()
{
	TSRMLS_FETCH();
    if (!APCG(initialized))
        return 0;

    /* restore execution */
    zend_execute = old_execute;
    
    /* restore compilation */
    zend_compile_file = old_compile_file;

    /* apc cleanup */
    apc_stack_destroy(APCG(cache_stack));
    apc_cache_destroy(APCG(cache));
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
}
/* }}} */

/* {{{ apc_version */
const char* apc_version()
{
    return "2.0.3";
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
