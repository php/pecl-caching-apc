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
#if PHP_API_VERSION <= 20020918
#if HAVE_APACHE
#undef XtOffsetOf
#include "httpd.h"
#endif
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
#ifdef ZEND_ENGINE_2    
    /*
     * XXX: We need to free this somewhere...
     */
    zend_class_entry** allocated_ce = apc_php_malloc(sizeof(zend_class_entry*));    

    if(!allocated_ce) {
#ifdef __DEBUG_APC__
        fprintf(stderr, "Failed to allocate memory for the allocated_ce!\n");
#endif
        return FAILURE;
    }

    *allocated_ce = 
#endif        
    class_entry =
        apc_copy_class_entry_for_execution(cl.class_entry,
                                           cl.is_derived);


    /* restore parent class pointer for compile-time inheritance */
    if (cl.parent_name != NULL) {
#ifdef ZEND_ENGINE_2    
        zend_class_entry** parent_ptr = NULL;
        /*
         * zend_lookup_class has to be due to presence of __autoload, 
         * just looking up the EG(class_table) is not enough in php5!
         * Aside: Do NOT pass *strlen(cl.parent_name)+1* because 
         * zend_lookup_class does it internally anyway!
         */
        status = zend_lookup_class(cl.parent_name,
                                    strlen(cl.parent_name),
                                    &parent_ptr);
#else
        status = zend_hash_find(EG(class_table),
                                cl.parent_name,
                                strlen(cl.parent_name)+1,
                                (void**) &parent);
#endif
#ifdef __DEBUG_APC__
        my_zend_hash_display(EG(class_table), "\t");
        fprintf(stderr, "<zend_hash_find> for %s got <%s, %d> and returned: %d\n", class_entry->name, cl.parent_name, strlen(cl.parent_name)+1, status);
#endif
        
        if (status == FAILURE) {
            class_entry->parent = NULL;
        }
        else {
#ifdef ZEND_ENGINE_2            
            parent = *parent_ptr;
#endif 
            class_entry->parent = parent;
#ifdef ZEND_ENGINE_2            
#ifdef __DEBUG_APC__
            fprintf(stderr, "<install_class> for <%s, %p>, parent: <%s, %p> --> %d\n", class_entry->name, class_entry, cl.parent_name, parent, status);
#endif 
            zend_do_inheritance(class_entry, parent);
#endif            
        }


    }

#ifdef ZEND_ENGINE_2                           
    status = zend_hash_add(EG(class_table),
                           cl.name,
                           cl.name_len+1,
                           allocated_ce,
                           sizeof(zend_class_entry*),
                           NULL);
#ifdef __DEBUG_APC__
        fprintf(stderr, "<zend_hash_add> <%s, %d> %d\n", cl.name, cl.name_len+1, status);
#endif
#else                           
    status = zend_hash_add(EG(class_table),
                           cl.name,
                           cl.name_len+1,
                           class_entry,
                           sizeof(zend_class_entry),
                           NULL);
#endif                           
    
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

    if (!APCG(enabled)) return old_compile_file(h, type TSRMLS_CC);

    /* check our regular expression filters */
    if (APCG(compiled_filters)) {
        int ret = apc_regex_match_array(APCG(compiled_filters), h->filename);
        if(ret == APC_NEGATIVE_MATCH || (ret != APC_POSITIVE_MATCH && !APCG(cache_by_default))) {
            return old_compile_file(h, type TSRMLS_CC);
        }
    } else if(!APCG(cache_by_default)) {
        return old_compile_file(h, type TSRMLS_CC);
    }

#if PHP_API_VERSION <= 20020918
#if HAVE_APACHE
    t = ((request_rec *)SG(server_context))->request_time;
#else 
    t = time(0);
#endif
#else 
    t = sapi_get_request_time(TSRMLS_C);
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
    /*
     * Basically this will cause a file only to be cached on a percentage 
     * of the attempts.  This is to avoid cache slams when starting up a
     * very busy server or when modifying files on a very busy live server.
     * There is no point having many processes all trying to cache the same
     * file at the same time.  By introducing a chance of being cached
     * we theoretically cut the cache slam problem by the given percentage.
     * For example if apc.slam_defense is set to 66 then 2/3 of the attempts
     * to cache an uncached file will be ignored.
     */
    if(APCG(slam_defense) && (int)(100.0*rand()/(RAND_MAX+1.0)) < APCG(slam_defense))
      return op_array;

    HANDLE_BLOCK_INTERRUPTIONS();
    if(!(alloc_op_array = apc_copy_op_array(NULL, op_array, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_cache_expunge(APCG(cache),t);
        apc_cache_expunge(APCG(user_cache),t);
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }
    
    if(!(alloc_functions = apc_copy_new_functions(num_functions, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_cache_expunge(APCG(cache),t);
        apc_cache_expunge(APCG(user_cache),t);
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }
    if(!(alloc_classes = apc_copy_new_classes(op_array, num_classes, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_free_functions(alloc_functions, apc_sma_free);
        apc_cache_expunge(APCG(cache),t);
        apc_cache_expunge(APCG(user_cache),t);
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }

    if(!(cache_entry = apc_cache_make_file_entry(h->opened_path, alloc_op_array, alloc_functions, alloc_classes))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_free_functions(alloc_functions, apc_sma_free);
        apc_free_classes(alloc_classes, apc_sma_free);
        apc_cache_expunge(APCG(cache),t);
        apc_cache_expunge(APCG(user_cache),t);
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }
    HANDLE_UNBLOCK_INTERRUPTIONS();

    if ((ret = apc_cache_insert(APCG(cache), key, cache_entry, t)) != 1) {
        apc_cache_free_entry(cache_entry);
        if(ret==-1) {
            apc_cache_expunge(APCG(cache),t);
            apc_cache_expunge(APCG(user_cache),t);
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

int apc_request_init(TSRMLS_D)
{
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
    return "3.0.0";
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
