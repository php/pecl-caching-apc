/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
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
#include "apc_lock.h"
#include "apc_cache.h"
#include "apc_compile.h"
#include "apc_globals.h"
#include "apc_sma.h"
#include "apc_stack.h"
#include "apc_zend.h"
#include "SAPI.h"
#if PHP_API_VERSION <= 20020918
#if HAVE_APACHE
#ifdef APC_PHP4_STAT
#undef XtOffsetOf
#include "httpd.h"
#endif
#endif
#endif

/* {{{ module variables */

/* pointer to the original Zend engine compile_file function */
typedef zend_op_array* (zend_compile_t)(zend_file_handle*, int TSRMLS_DC);
static zend_compile_t *old_compile_file;

/* }}} */

/* {{{ get/set old_compile_file (to interact with other extensions that need the compile hook) */
static zend_compile_t* set_compile_hook(zend_compile_t *ptr)
{
    zend_compile_t *retval = old_compile_file;

    if (ptr != NULL) old_compile_file = ptr;
    return retval;
}
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
        /* zend_error(E_ERROR, "Cannot redeclare %s()", fn.name); */
    }

    return status;
}
/* }}} */

/* {{{ install_class */
static int install_class(apc_class_t cl TSRMLS_DC)
{
    zend_class_entry* class_entry = cl.class_entry;
    zend_class_entry* parent = NULL;
    int status;
#ifdef ZEND_ENGINE_2
    zend_class_entry** allocated_ce = NULL;
#endif

#ifdef ZEND_ENGINE_2    

    /* Special case for mangled names. Mangled names are unique to a file.
     * There is no way two classes with the same mangled name will occur,
     * unless a file is included twice. And if in case, a file is included
     * twice, all mangled name conflicts can be ignored and the class redeclaration
     * error may be deferred till runtime of the corresponding DECLARE_CLASS
     * calls.
     */

    if(cl.name_len != 0 && cl.name[0] == '\0') {
        if(zend_hash_exists(CG(class_table), cl.name, cl.name_len+1)) {
            return SUCCESS;
        }
    }
    
    /*
     * XXX: We need to free this somewhere...
     */
    allocated_ce = apc_php_malloc(sizeof(zend_class_entry*));    

    if(!allocated_ce) {
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
         * Even more dangerously, thanks to __autoload and people using
         * class names as filepaths for inclusion, this has to be case
         * sensitive. zend_lookup_class automatically does a case_fold
         * internally, but passes the case preserved version to __autoload.
         * Aside: Do NOT pass *strlen(cl.parent_name)+1* because 
         * zend_lookup_class does it internally anyway!
         */
        status = zend_lookup_class(cl.parent_name,
                                    strlen(cl.parent_name),
                                    &parent_ptr TSRMLS_CC);
#else
        status = zend_hash_find(EG(class_table),
                                cl.parent_name,
                                strlen(cl.parent_name)+1,
                                (void**) &parent);
#endif
        if (status == FAILURE) {
            if(APCG(report_autofilter)) {
                zend_error(E_WARNING, "Dynamic inheritance detected for class %s", cl.name);
            }
            class_entry->parent = NULL;
            return status;
        }
        else {
#ifdef ZEND_ENGINE_2            
            parent = *parent_ptr;
#endif 
            class_entry->parent = parent;
#ifdef ZEND_ENGINE_2            
            zend_do_inheritance(class_entry, parent TSRMLS_CC);
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
#else                           
    status = zend_hash_add(EG(class_table),
                           cl.name,
                           cl.name_len+1,
                           class_entry,
                           sizeof(zend_class_entry),
                           NULL);
#endif                           

    if (status == FAILURE) {
        zend_error(E_ERROR, "Cannot redeclare class %s", cl.name);
    } 
    return status;
}
/* }}} */

/* {{{ uninstall_class */
static int uninstall_class(apc_class_t cl TSRMLS_DC)
{
    zend_class_entry* class_entry = cl.class_entry;
    int status;

#ifdef ZEND_ENGINE_2                           
    status = zend_hash_del(EG(class_table),
                           cl.name,
                           cl.name_len+1);
#else                           
    status = zend_hash_del(EG(class_table),
                           cl.name,
                           cl.name_len+1);
#endif                           
    if (status == FAILURE) {
        zend_error(E_ERROR, "Cannot delete class %s", cl.name);
    } 
    return status;
}
/* }}} */

/* {{{ compare_file_handles */
static int compare_file_handles(void* a, void* b)
{
    zend_file_handle* fh1 = (zend_file_handle*)a;
    zend_file_handle* fh2 = (zend_file_handle*)b;
    return (fh1->type == fh2->type && 
            fh1->filename == fh2->filename &&
            fh1->opened_path == fh2->opened_path);
}
/* }}} */

/* {{{ cached_compile */
static zend_op_array* cached_compile(zend_file_handle* h,
                                        int type TSRMLS_DC)
{
    apc_cache_entry_t* cache_entry;
    int i, ii;

    cache_entry = (apc_cache_entry_t*) apc_stack_top(APCG(cache_stack));
    assert(cache_entry != NULL);

    if (cache_entry->data.file.classes) {
        for (i = 0; cache_entry->data.file.classes[i].class_entry != NULL; i++) {
            if(install_class(cache_entry->data.file.classes[i] TSRMLS_CC) == FAILURE) {
                goto default_compile;
            }
        }
    }

    if (cache_entry->data.file.functions) {
        for (i = 0; cache_entry->data.file.functions[i].function != NULL; i++) {
            install_function(cache_entry->data.file.functions[i] TSRMLS_CC);
        }
    }


    return apc_copy_op_array_for_execution(NULL, cache_entry->data.file.op_array TSRMLS_CC);

default_compile:

    cache_entry->autofiltered = 1;
    if(APCG(report_autofilter)) {
        zend_error(E_WARNING, "Autofiltering %s", h->opened_path);
    }

    if(cache_entry->data.file.classes) {
        for(ii = 0; ii < i ; ii++) {
            uninstall_class(cache_entry->data.file.classes[i] TSRMLS_CC);
        }
    }

    /* cannot free up cache data yet, it maybe in use */
    
    zend_llist_del_element(&CG(open_files), h, compare_file_handles); /* XXX: kludge */
    
    h->type = ZEND_HANDLE_FILENAME;
    
    return old_compile_file(h, type TSRMLS_CC);
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
    char *path;
    size_t mem_size;

    if (!APCG(enabled) || apc_cache_busy(apc_cache)) {
		return old_compile_file(h, type TSRMLS_CC);
	}

    /* check our regular expression filters */
    if (apc_compiled_filters) {
        int ret = apc_regex_match_array(apc_compiled_filters, h->filename);
        if(ret == APC_NEGATIVE_MATCH || (ret != APC_POSITIVE_MATCH && !APCG(cache_by_default))) {
            return old_compile_file(h, type TSRMLS_CC);
        }
    } else if(!APCG(cache_by_default)) {
        return old_compile_file(h, type TSRMLS_CC);
    }

#if PHP_API_VERSION <= 20041225
#if HAVE_APACHE && defined(APC_PHP4_STAT)
    t = ((request_rec *)SG(server_context))->request_time;
#else 
    t = time(0);
#endif
#else 
    t = sapi_get_request_time(TSRMLS_C);
#endif

#ifdef __DEBUG_APC__
    fprintf(stderr,"1. h->opened_path=[%s]  h->filename=[%s]\n", h->opened_path?h->opened_path:"null",h->filename);
#endif

    /* try to create a cache key; if we fail, give up on caching */
    if (!apc_cache_make_file_key(&key, h->filename, PG(include_path), t TSRMLS_CC)) {
        return old_compile_file(h, type TSRMLS_CC);
    }
    
    /* search for the file in the cache */
    cache_entry = apc_cache_find(apc_cache, key, t);
    if (cache_entry != NULL && !cache_entry->autofiltered) {
        int dummy = 1;
        if (h->opened_path == NULL) {
            h->opened_path = estrdup(cache_entry->data.file.filename);
        }
        zend_hash_add(&EG(included_files), h->opened_path, strlen(h->opened_path)+1, (void *)&dummy, sizeof(int), NULL);
        zend_llist_add_element(&CG(open_files), h); /* XXX kludge */
        apc_stack_push(APCG(cache_stack), cache_entry);
        return cached_compile(h, type TSRMLS_CC);
    }
    else if(cache_entry != NULL && cache_entry->autofiltered) {
        /* nobody else is using this cache_entry */ 
        if(cache_entry->ref_count == 1) {
            if(cache_entry->data.file.op_array) {
                apc_free_op_array(cache_entry->data.file.op_array, apc_sma_free);
                cache_entry->data.file.op_array = NULL;
            }
            if(cache_entry->data.file.functions) {
                apc_free_functions(cache_entry->data.file.functions, apc_sma_free);
                cache_entry->data.file.functions = NULL;
            }
            if(cache_entry->data.file.classes) {
                apc_free_classes(cache_entry->data.file.classes, apc_sma_free);
                cache_entry->data.file.classes = NULL;        
            }
        }
        /* We never push this into the cache_stack, so we have to do a release */
        apc_cache_release(apc_cache, cache_entry);
        return old_compile_file(h, type TSRMLS_CC);
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
    if(APCG(slam_defense)) {
        if(APCG(slam_rand)==-1) {
            APCG(slam_rand) = (int)(100.0*rand()/(RAND_MAX+1.0));
        }
        if(APCG(slam_rand) < APCG(slam_defense)) {
            return op_array;
        }
    }

    HANDLE_BLOCK_INTERRUPTIONS();

#if NONBLOCKING_LOCK_AVAILABLE
    if(APCG(write_lock)) {
        if(!apc_cache_write_lock(apc_cache)) {
            HANDLE_UNBLOCK_INTERRUPTIONS();
            return op_array;
        }
    }
#endif

    mem_size = 0;
    APCG(mem_size_ptr) = &mem_size;
    if(!(alloc_op_array = apc_copy_op_array(NULL, op_array, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_cache_expunge(apc_cache,t);
        apc_cache_expunge(apc_user_cache,t);
        APCG(mem_size_ptr) = NULL;
#if NONBLOCKING_LOCK_AVAILABLE
        if(APCG(write_lock)) {
            apc_cache_write_unlock(apc_cache);
        }
#endif
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }
    
    if(!(alloc_functions = apc_copy_new_functions(num_functions, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_cache_expunge(apc_cache,t);
        apc_cache_expunge(apc_user_cache,t);
        APCG(mem_size_ptr) = NULL;
#if NONBLOCKING_LOCK_AVAILABLE
        if(APCG(write_lock)) {
            apc_cache_write_unlock(apc_cache);
        }
#endif
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }
    if(!(alloc_classes = apc_copy_new_classes(op_array, num_classes, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_free_functions(alloc_functions, apc_sma_free);
        apc_cache_expunge(apc_cache,t);
        apc_cache_expunge(apc_user_cache,t);
        APCG(mem_size_ptr) = NULL;
#if NONBLOCKING_LOCK_AVAILABLE
        if(APCG(write_lock)) {
            apc_cache_write_unlock(apc_cache);
        }
#endif
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }

    path = h->opened_path;
    if(!path) path=h->filename;

#ifdef __DEBUG_APC__
    fprintf(stderr,"2. h->opened_path=[%s]  h->filename=[%s]\n", h->opened_path?h->opened_path:"null",h->filename);
#endif

    if(!(cache_entry = apc_cache_make_file_entry(path, alloc_op_array, alloc_functions, alloc_classes))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_free_functions(alloc_functions, apc_sma_free);
        apc_free_classes(alloc_classes, apc_sma_free);
        apc_cache_expunge(apc_cache,t);
        apc_cache_expunge(apc_user_cache,t);
        APCG(mem_size_ptr) = NULL;
#if NONBLOCKING_LOCK_AVAILABLE
        if(APCG(write_lock)) {
            apc_cache_write_unlock(apc_cache);
        }
#endif
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }
    APCG(mem_size_ptr) = NULL;
    cache_entry->mem_size = mem_size;
    cache_entry->autofiltered = 0;

    if ((ret = apc_cache_insert(apc_cache, key, cache_entry, t)) != 1) {
        apc_cache_free_entry(cache_entry);
        if(ret==-1) {
            apc_cache_expunge(apc_cache,t);
            apc_cache_expunge(apc_user_cache,t);
        }
    }

#if NONBLOCKING_LOCK_AVAILABLE
    if(APCG(write_lock)) {
        apc_cache_write_unlock(apc_cache);
    }
#endif
    HANDLE_UNBLOCK_INTERRUPTIONS();

    return op_array;
}
/* }}} */

/* {{{ module init and shutdown */

int apc_module_init(int module_number TSRMLS_DC)
{
    /* apc initialization */
#if APC_MMAP
    apc_sma_init(APCG(shm_segments), APCG(shm_size)*1024*1024, APCG(mmap_file_mask));
#else
    apc_sma_init(APCG(shm_segments), APCG(shm_size)*1024*1024, NULL);
#endif
    apc_cache = apc_cache_create(APCG(num_files_hint), APCG(gc_ttl), APCG(ttl));
    apc_user_cache = apc_cache_create(APCG(user_entries_hint), APCG(gc_ttl), APCG(user_ttl));
    apc_compiled_filters = apc_regex_compile_array(APCG(filters));

    /* override compilation */
    old_compile_file = zend_compile_file;
    zend_compile_file = my_compile_file;
    REGISTER_LONG_CONSTANT("\000apc_magic", (long)&set_compile_hook, CONST_PERSISTENT | CONST_CS);

    APCG(initialized) = 1;
    return 0;
}

int apc_module_shutdown(TSRMLS_D)
{
    if (!APCG(initialized))
        return 0;

    /* restore compilation */
    zend_compile_file = old_compile_file;

    apc_cache_destroy(apc_cache);
    apc_cache_destroy(apc_user_cache);
    apc_sma_cleanup();

    APCG(initialized) = 0;
    return 0;
}

/* }}} */

/* {{{ request init and shutdown */

int apc_request_init(TSRMLS_D)
{
    apc_stack_clear(APCG(cache_stack));
    APCG(slam_rand) = -1;
    return 0;
}

int apc_request_shutdown(TSRMLS_D)
{
    apc_deactivate(TSRMLS_C);
    return 0;
}

/* }}} */

/* {{{ apc_deactivate */
void apc_deactivate(TSRMLS_D)
{
    /* The execution stack was unwound, which prevented us from decrementing
     * the reference counts on active cache entries in `my_execute`.
     */
    while (apc_stack_size(APCG(cache_stack)) > 0) {
        apc_cache_entry_t* cache_entry =
            (apc_cache_entry_t*) apc_stack_pop(APCG(cache_stack));
        apc_cache_release(apc_cache, cache_entry);
    }
    /* Safety net */
#if 0
    apc_sma_unlock();
    apc_cache_unlock(apc_cache);
#endif
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
