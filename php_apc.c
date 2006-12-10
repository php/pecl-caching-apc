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
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$ */

#include "apc_zend.h"
#include "apc_cache.h"
#include "apc_main.h"
#include "apc_sma.h"
#include "apc_lock.h"
#include "php_globals.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "SAPI.h"
#include "rfc1867.h"
#include "php_apc.h"
#if PHP_API_VERSION <= 20020918
#if HAVE_APACHE
#ifdef APC_PHP4_STAT
#undef XtOffsetOf
#include "httpd.h"
#endif
#endif
#endif

/* {{{ PHP_FUNCTION declarations */
PHP_FUNCTION(apc_cache_info);
PHP_FUNCTION(apc_clear_cache);
PHP_FUNCTION(apc_sma_info);
PHP_FUNCTION(apc_store);
PHP_FUNCTION(apc_fetch);
PHP_FUNCTION(apc_delete);
PHP_FUNCTION(apc_define_constants);
PHP_FUNCTION(apc_load_constants);
/* }}} */

/* {{{ ZEND_DECLARE_MODULE_GLOBALS(apc) */
ZEND_DECLARE_MODULE_GLOBALS(apc)

/* True globals */
apc_cache_t* apc_cache = NULL;       
apc_cache_t* apc_user_cache = NULL;
void* apc_compiled_filters = NULL;

static void php_apc_init_globals(zend_apc_globals* apc_globals TSRMLS_DC)
{
    apc_globals->filters = NULL;
    apc_globals->initialized = 0;
    apc_globals->cache_stack = apc_stack_create(0);
    apc_globals->cache_by_default = 1;
    apc_globals->slam_defense = 0;
    apc_globals->mem_size_ptr = NULL;
    apc_globals->fpstat = 1;
    apc_globals->stat_ctime = 0;
    apc_globals->write_lock = 1;
    apc_globals->report_autofilter = 0;
#ifdef MULTIPART_EVENT_FORMDATA
    apc_globals->rfc1867 = 0;
#endif
    apc_globals->copied_zvals = NULL;
#ifdef ZEND_ENGINE_2
    apc_globals->reserved_offset = -1;
#endif
}

static void php_apc_shutdown_globals(zend_apc_globals* apc_globals TSRMLS_DC)
{
    /* deallocate the ignore patterns */
    if (apc_globals->filters != NULL) {
        int i;
        for (i=0; apc_globals->filters[i] != NULL; i++) {
            apc_efree(apc_globals->filters[i]);
        }
        apc_efree(apc_globals->filters);
    }

    /* the stack should be empty */
    assert(apc_stack_size(apc_globals->cache_stack) == 0); 

    /* apc cleanup */
    apc_stack_destroy(apc_globals->cache_stack);

    /* the rest of the globals are cleaned up in apc_module_shutdown() */
}

/* }}} */

/* {{{ PHP_INI */

static PHP_INI_MH(OnUpdate_filters)
{
    APCG(filters) = apc_tokenize(new_value, ',');
    return SUCCESS;
}

#ifdef ZEND_ENGINE_2
#define OnUpdateInt OnUpdateLong
#endif

PHP_INI_BEGIN()
STD_PHP_INI_BOOLEAN("apc.enabled",      "1",    PHP_INI_SYSTEM, OnUpdateBool,              enabled,         zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.shm_segments",   "1",    PHP_INI_SYSTEM, OnUpdateInt,            shm_segments,    zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.shm_size",       "30",   PHP_INI_SYSTEM, OnUpdateInt,            shm_size,        zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN("apc.include_once_override", "0", PHP_INI_SYSTEM, OnUpdateBool,     include_once,    zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.num_files_hint", "1000", PHP_INI_SYSTEM, OnUpdateInt,            num_files_hint,  zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.user_entries_hint", "100", PHP_INI_SYSTEM, OnUpdateInt,          user_entries_hint, zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.gc_ttl",         "3600", PHP_INI_SYSTEM, OnUpdateInt,            gc_ttl,           zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.ttl",            "0",    PHP_INI_SYSTEM, OnUpdateInt,            ttl,              zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.user_ttl",       "0",    PHP_INI_SYSTEM, OnUpdateInt,            user_ttl,         zend_apc_globals, apc_globals)
#if APC_MMAP
STD_PHP_INI_ENTRY("apc.mmap_file_mask",  NULL,  PHP_INI_SYSTEM, OnUpdateString,         mmap_file_mask,   zend_apc_globals, apc_globals)
#endif
PHP_INI_ENTRY("apc.filters",        NULL,     PHP_INI_SYSTEM, OnUpdate_filters)
STD_PHP_INI_BOOLEAN("apc.cache_by_default", "1",  PHP_INI_ALL, OnUpdateBool,         cache_by_default, zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.slam_defense", "0",      PHP_INI_SYSTEM, OnUpdateInt,            slam_defense,     zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.file_update_protection", "2", PHP_INI_SYSTEM, OnUpdateInt,file_update_protection,  zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN("apc.enable_cli", "0",      PHP_INI_SYSTEM, OnUpdateBool,           enable_cli,       zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.max_file_size", "1M",    PHP_INI_SYSTEM, OnUpdateInt,            max_file_size,    zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN("apc.stat", "1",            PHP_INI_SYSTEM, OnUpdateBool,           fpstat,           zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN("apc.stat_ctime", "0",      PHP_INI_SYSTEM, OnUpdateBool,           stat_ctime,       zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN("apc.write_lock", "1",      PHP_INI_SYSTEM, OnUpdateBool,           write_lock,       zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN("apc.report_autofilter", "0", PHP_INI_SYSTEM, OnUpdateBool,         report_autofilter,zend_apc_globals, apc_globals)
#ifdef MULTIPART_EVENT_FORMDATA
STD_PHP_INI_BOOLEAN("apc.rfc1867", "0", PHP_INI_SYSTEM, OnUpdateBool, rfc1867, zend_apc_globals, apc_globals)
#endif
PHP_INI_END()

/* }}} */

/* {{{ PHP_MINFO_FUNCTION(apc) */
static PHP_MINFO_FUNCTION(apc)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "APC Support", APCG(enabled) ? "enabled" : "disabled");
    php_info_print_table_row(2, "Version", APC_VERSION);
#if APC_MMAP
    php_info_print_table_row(2, "MMAP Support", "Enabled");
    php_info_print_table_row(2, "MMAP File Mask", APCG(mmap_file_mask));
#else
    php_info_print_table_row(2, "MMAP Support", "Disabled");
#endif
#if APC_SEM_LOCKS
    php_info_print_table_row(2, "Locking type", "IPC Semaphore");
#elif APC_FUTEX_LOCKS
    php_info_print_table_row(2, "Locking type", "Linux Futex Locks");
#else
    php_info_print_table_row(2, "Locking type", "File Locks");
#endif
    php_info_print_table_row(2, "Revision", "$Revision$");
    php_info_print_table_row(2, "Build Date", __DATE__ " " __TIME__);
    php_info_print_table_end();
    DISPLAY_INI_ENTRIES();
}
/* }}} */

#ifdef MULTIPART_EVENT_FORMDATA
extern int apc_rfc1867_progress(unsigned int event, void *event_data, void **extra TSRMLS_DC);
#endif

/* {{{ PHP_MINIT_FUNCTION(apc) */
static PHP_MINIT_FUNCTION(apc)
{
    ZEND_INIT_MODULE_GLOBALS(apc, php_apc_init_globals, php_apc_shutdown_globals);

    REGISTER_INI_ENTRIES();

    /* Disable APC in cli mode unless overridden by apc.enable_cli */
    if(!APCG(enable_cli) && !strcmp(sapi_module.name, "cli")) {
	APCG(enabled) = 0;
    }

    if (APCG(enabled)) {
        apc_module_init(module_number TSRMLS_CC);
        apc_zend_init(TSRMLS_C);
    }

#ifdef MULTIPART_EVENT_FORMDATA
    /* File upload progress tracking */
    if(APCG(rfc1867)) {
        php_rfc1867_callback = apc_rfc1867_progress;
    }
#endif

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION(apc) */
static PHP_MSHUTDOWN_FUNCTION(apc)
{
    if(APCG(enabled)) {
        apc_zend_shutdown(TSRMLS_C);
        apc_module_shutdown(TSRMLS_C);
#ifdef ZTS
        ts_free_id(apc_globals_id);
#else
        php_apc_shutdown_globals(&apc_globals);
#endif
    }
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION(apc) */
static PHP_RINIT_FUNCTION(apc)
{
    if(APCG(enabled)) {
        apc_request_init(TSRMLS_C);
    }
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION(apc) */
static PHP_RSHUTDOWN_FUNCTION(apc)
{
    if(APCG(enabled)) {
        apc_request_shutdown(TSRMLS_C);
    }
    return SUCCESS;
}
/* }}} */

/* {{{ proto array apc_cache_info([string type] [, bool limited]) */
PHP_FUNCTION(apc_cache_info)
{
    apc_cache_info_t* info;
    apc_cache_link_t* p;
    zval* list;
    char *cache_type;
    int ct_len;
    zend_bool limited=0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sb", &cache_type, &ct_len, &limited) == FAILURE) {
        return;
    }

    if(ZEND_NUM_ARGS()) {
        if(!strcasecmp(cache_type,"user")) {
            info = apc_cache_info(apc_user_cache, limited);
        } else {
            info = apc_cache_info(apc_cache, limited);
        }
    } else info = apc_cache_info(apc_cache, limited);

    if(!info) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "No APC info available.  Perhaps APC is not enabled? Check apc.enabled in your ini file");
        RETURN_FALSE;
    }

    array_init(return_value);
    add_assoc_long(return_value, "num_slots", info->num_slots);
    add_assoc_long(return_value, "ttl", info->ttl);
    add_assoc_long(return_value, "num_hits", info->num_hits);
    add_assoc_long(return_value, "num_misses", info->num_misses);
    add_assoc_long(return_value, "start_time", info->start_time);
    add_assoc_long(return_value, "expunges", info->expunges);
    add_assoc_long(return_value, "mem_size", info->mem_size);
    add_assoc_long(return_value, "num_entries", info->num_entries);
    add_assoc_long(return_value, "num_inserts", info->num_inserts);
#ifdef MULTIPART_EVENT_FORMDATA
    add_assoc_long(return_value, "file_upload_progress", 1);
#else
    add_assoc_long(return_value, "file_upload_progress", 0);
#endif
#if APC_MMAP
    add_assoc_stringl(return_value, "memory_type", "mmap", sizeof("mmap"), 1);
#else
    add_assoc_stringl(return_value, "memory_type", "IPC shared", sizeof("IPC shared"), 1);
#endif
#if APC_SEM_LOCKS
    add_assoc_stringl(return_value, "locking_type", "IPC semaphore", sizeof("IPC semaphore"), 1);
#elif APC_FUTEX_LOCKS
    add_assoc_stringl(return_value, "locking_type", "Linux Futex", sizeof("Linux Futex"), 1);
#else
    add_assoc_stringl(return_value, "locking_type", "file", sizeof("file"), 1);
#endif
    if(limited) {
        apc_cache_free_info(info);
        return;
    }
    
    ALLOC_INIT_ZVAL(list);
    array_init(list);

    for (p = info->list; p != NULL; p = p->next) {
        zval* link;

        ALLOC_INIT_ZVAL(link);
        array_init(link);

        if(p->type == APC_CACHE_ENTRY_FILE) {
            add_assoc_string(link, "filename", p->data.file.filename, 1);
            add_assoc_long(link, "device", p->data.file.device);
            add_assoc_long(link, "inode", p->data.file.inode);
            add_assoc_string(link, "type", "file", 1);
        } else if(p->type == APC_CACHE_ENTRY_USER) {
            add_assoc_string(link, "info", p->data.user.info, 1);
            add_assoc_long(link, "ttl", (long)p->data.user.ttl);
            add_assoc_string(link, "type", "user", 1);
        }
        add_assoc_long(link, "num_hits", p->num_hits);
        add_assoc_long(link, "mtime", p->mtime);
        add_assoc_long(link, "creation_time", p->creation_time);
        add_assoc_long(link, "deletion_time", p->deletion_time);
        add_assoc_long(link, "access_time", p->access_time);
        add_assoc_long(link, "ref_count", p->ref_count);
        add_assoc_long(link, "mem_size", p->mem_size);
        add_next_index_zval(list, link);
    }
    add_assoc_zval(return_value, "cache_list", list);

    ALLOC_INIT_ZVAL(list);
    array_init(list);

    for (p = info->deleted_list; p != NULL; p = p->next) {
        zval* link;

        ALLOC_INIT_ZVAL(link);
        array_init(link);

        if(p->type == APC_CACHE_ENTRY_FILE) {
            add_assoc_string(link, "filename", p->data.file.filename, 1);
            add_assoc_long(link, "device", p->data.file.device);
            add_assoc_long(link, "inode", p->data.file.inode);
            add_assoc_string(link, "type", "file", 1);
        } else if(p->type == APC_CACHE_ENTRY_USER) {
            add_assoc_string(link, "info", p->data.user.info, 1);
            add_assoc_long(link, "ttl", (long)p->data.user.ttl);
            add_assoc_string(link, "type", "user", 1);
        }
        add_assoc_long(link, "num_hits", p->num_hits);
        add_assoc_long(link, "mtime", p->mtime);
        add_assoc_long(link, "creation_time", p->creation_time);
        add_assoc_long(link, "deletion_time", p->deletion_time);
        add_assoc_long(link, "access_time", p->access_time);
        add_assoc_long(link, "ref_count", p->ref_count);
        add_assoc_long(link, "mem_size", p->mem_size);
        add_next_index_zval(list, link);
    }
    add_assoc_zval(return_value, "deleted_list", list);

    apc_cache_free_info(info);
}
/* }}} */

/* {{{ proto void apc_clear_cache() */
PHP_FUNCTION(apc_clear_cache)
{
    char *cache_type;
    int ct_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &cache_type, &ct_len) == FAILURE) {
        return;
    }

    if(ZEND_NUM_ARGS()) {
        if(!strcasecmp(cache_type,"user")) {
            apc_cache_clear(apc_user_cache);
            RETURN_TRUE;
        }
    }
    apc_cache_clear(apc_cache);
}
/* }}} */

/* {{{ proto array apc_sma_info() */
PHP_FUNCTION(apc_sma_info)
{
    apc_sma_info_t* info;
    zval* block_lists;
    int i;

    if (ZEND_NUM_ARGS() != 0) {
        WRONG_PARAM_COUNT;
    }

    info = apc_sma_info();

    if(!info) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "No APC SMA info available.  Perhaps APC is disabled via apc.enabled?");
        RETURN_FALSE;
    }

    array_init(return_value);
    add_assoc_long(return_value, "num_seg", info->num_seg);
    add_assoc_long(return_value, "seg_size", info->seg_size);
    add_assoc_long(return_value, "avail_mem", apc_sma_get_avail_mem());
#if ALLOC_DISTRIBUTION
    {
        size_t *adist = apc_sma_get_alloc_distribution();
        zval* list;
        ALLOC_INIT_ZVAL(list);
        array_init(list);
        for(i=0; i<30; i++) {
            add_next_index_long(list, adist[i]);
        }
        add_assoc_zval(return_value, "adist", list);
    }
#endif
    ALLOC_INIT_ZVAL(block_lists);
    array_init(block_lists);

    for (i = 0; i < info->num_seg; i++) {
        apc_sma_link_t* p;
        zval* list;

        ALLOC_INIT_ZVAL(list);
        array_init(list);

        for (p = info->list[i]; p != NULL; p = p->next) {
            zval* link;

            ALLOC_INIT_ZVAL(link);
            array_init(link);

            add_assoc_long(link, "size", p->size);
            add_assoc_long(link, "offset", p->offset);
            add_next_index_zval(list, link);
        }
        add_next_index_zval(block_lists, list);
    }
    add_assoc_zval(return_value, "block_lists", block_lists);
    apc_sma_free_info(info);
}
/* }}} */

/* {{{ _apc_store */
int _apc_store(char *strkey, int strkey_len, const zval *val, const unsigned int ttl TSRMLS_DC) {
    apc_cache_entry_t *entry;
    apc_cache_key_t key;
    time_t t;
    size_t mem_size = 0;

#if PHP_API_VERSION <= 20041225
#if HAVE_APACHE && defined(APC_PHP4_STAT)
    t = ((request_rec *)SG(server_context))->request_time;
#else
    t = time(0);
#endif
#else
    t = sapi_get_request_time(TSRMLS_C);
#endif

    if(!APCG(enabled)) return 0;

    HANDLE_BLOCK_INTERRUPTIONS();

    APCG(mem_size_ptr) = &mem_size;
    if (!(entry = apc_cache_make_user_entry(strkey, strkey_len + 1, val, ttl))) {
        APCG(mem_size_ptr) = NULL;
        apc_cache_expunge(apc_cache,t);
        apc_cache_expunge(apc_user_cache,t);
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return 0;
    }

    if (!apc_cache_make_user_key(&key, strkey, strkey_len + 1, t)) {
        APCG(mem_size_ptr) = NULL;
        apc_cache_free_entry(entry);
        apc_cache_expunge(apc_cache,t);
        apc_cache_expunge(apc_user_cache,t);
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return 0;
    }

    if (!apc_cache_user_insert(apc_user_cache, key, entry, t TSRMLS_CC)) {
        APCG(mem_size_ptr) = NULL;
        apc_cache_free_entry(entry);
        apc_cache_expunge(apc_cache,t);
        apc_cache_expunge(apc_user_cache,t);
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return 0;
    }

    APCG(mem_size_ptr) = NULL;

    HANDLE_UNBLOCK_INTERRUPTIONS();

    return 1;
}
/* }}} */

/* {{{ proto int apc_store(string key, zval var [, ttl ])
 */
PHP_FUNCTION(apc_store) {
    zval *val;
    char *strkey;
    int strkey_len;
    long ttl = 0L;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|l", &strkey, &strkey_len, &val, &ttl) == FAILURE) {
        return;
    }

    if(!strkey_len) RETURN_FALSE;

    if(_apc_store(strkey, strkey_len, val, (unsigned int)ttl TSRMLS_CC)) RETURN_TRUE;
    RETURN_FALSE;
}
/* }}} */

void *apc_erealloc_wrapper(void *ptr, size_t size) {
    return _erealloc(ptr, size, 0 ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC);
}

/* {{{ proto mixed apc_fetch(mixed key)
 */
PHP_FUNCTION(apc_fetch) {
    zval *key;
    HashTable *hash;
    HashPosition hpos;
    zval **hentry;
    zval *result;
    zval *result_entry;
    char *strkey;
    int strkey_len;
    apc_cache_entry_t* entry;
    time_t t;

    if(!APCG(enabled)) RETURN_FALSE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &key) == FAILURE) {
        return;
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

    if(Z_TYPE_P(key) != IS_STRING && Z_TYPE_P(key) != IS_ARRAY) {
        convert_to_string(key);
    }
    
    if(Z_TYPE_P(key) == IS_STRING) {
        strkey = Z_STRVAL_P(key);
        strkey_len = Z_STRLEN_P(key);
        if(!strkey_len) RETURN_FALSE;
        entry = apc_cache_user_find(apc_user_cache, strkey, strkey_len + 1, t);
        if(entry) {
            /* deep-copy returned shm zval to emalloc'ed return_value */
            apc_cache_fetch_zval(return_value, entry->data.user.val, apc_php_malloc, apc_php_free);
            apc_cache_release(apc_user_cache, entry);
        } else {
            RETURN_FALSE;
        }
    } else if(Z_TYPE_P(key) == IS_ARRAY) {
        hash = Z_ARRVAL_P(key);
        MAKE_STD_ZVAL(result);
        array_init(result); 
        zend_hash_internal_pointer_reset_ex(hash, &hpos);
        while(zend_hash_get_current_data_ex(hash, (void**)&hentry, &hpos) == SUCCESS) {
            if(Z_TYPE_PP(hentry) != IS_STRING) {
                apc_wprint("apc_fetch() expects a string or array of strings.");
                RETURN_FALSE;
            }
            entry = apc_cache_user_find(apc_user_cache, Z_STRVAL_PP(hentry), Z_STRLEN_PP(hentry) + 1, t);
            if(entry) {
                /* deep-copy returned shm zval to emalloc'ed return_value */
                MAKE_STD_ZVAL(result_entry);
                apc_cache_fetch_zval(result_entry, entry->data.user.val, apc_php_malloc, apc_php_free);
                apc_cache_release(apc_user_cache, entry);
                zend_hash_add(Z_ARRVAL_P(result), Z_STRVAL_PP(hentry), Z_STRLEN_PP(hentry) +1, &result_entry, sizeof(zval*), NULL);
            } /* don't set values we didn't find */
            zend_hash_move_forward_ex(hash, &hpos);
        }
        RETURN_ZVAL(result, 0, 1);
    } else {
        apc_wprint("apc_fetch() expects a string or array of strings.");
        RETURN_FALSE;
    }

    return;
}
/* }}} */

/* {{{ proto mixed apc_delete(string key)
 */
PHP_FUNCTION(apc_delete) {
    char *strkey;
    int strkey_len;

    if(!APCG(enabled)) RETURN_FALSE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &strkey, &strkey_len) == FAILURE) {
        return;
    }

    if(!strkey_len) RETURN_FALSE;

    if(apc_cache_user_delete(apc_user_cache, strkey, strkey_len + 1)) {
        RETURN_TRUE;
    } else {
        RETURN_FALSE;
    }
}
/* }}} */

static void _apc_define_constants(zval *constants, zend_bool case_sensitive TSRMLS_DC) {
    char *const_key;
    unsigned int const_key_len;
    zval **entry;
    HashPosition pos;

    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(constants), &pos);
    while (zend_hash_get_current_data_ex(Z_ARRVAL_P(constants), (void**)&entry, &pos) == SUCCESS) {
        zend_constant c;
        int key_type;
        ulong num_key;

        key_type = zend_hash_get_current_key_ex(Z_ARRVAL_P(constants), &const_key, &const_key_len, &num_key, 0, &pos);
        if(key_type != HASH_KEY_IS_STRING) {
            zend_hash_move_forward_ex(Z_ARRVAL_P(constants), &pos);
            continue;
        }
        switch(Z_TYPE_PP(entry)) {
            case IS_LONG:
            case IS_DOUBLE:
            case IS_STRING:
            case IS_BOOL:
            case IS_RESOURCE:
            case IS_NULL:
                break;
            default:
                zend_hash_move_forward_ex(Z_ARRVAL_P(constants), &pos);
                continue;
        }
        c.value = **entry;
        zval_copy_ctor(&c.value);
        c.flags = case_sensitive;
        c.name = zend_strndup(const_key, const_key_len);
        c.name_len = const_key_len;
#ifdef ZEND_ENGINE_2
        c.module_number = PHP_USER_CONSTANT;
#endif
        zend_register_constant(&c TSRMLS_CC);

        zend_hash_move_forward_ex(Z_ARRVAL_P(constants), &pos);
    }
}

/* {{{ proto mixed apc_define_constants(string key, array constants [,bool case-sensitive])
 */
PHP_FUNCTION(apc_define_constants) {
    char *strkey;
    int strkey_len;
    zval *constants = NULL;
    zend_bool case_sensitive = 1;
    int argc = ZEND_NUM_ARGS();

    if (zend_parse_parameters(argc TSRMLS_CC, "sa|b", &strkey, &strkey_len, &constants, &case_sensitive) == FAILURE) {
        return;
    }

    if(!strkey_len) RETURN_FALSE;

    _apc_define_constants(constants, case_sensitive TSRMLS_CC);
    if(_apc_store(strkey, strkey_len, constants, 0 TSRMLS_CC)) RETURN_TRUE;
    RETURN_FALSE;
} /* }}} */

/* {{{ proto mixed apc_load_constants(string key [, bool case-sensitive])
 */
PHP_FUNCTION(apc_load_constants) {
    char *strkey;
    int strkey_len;
    apc_cache_entry_t* entry;
    time_t t;
    zend_bool case_sensitive = 1;

    if(!APCG(enabled)) RETURN_FALSE;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &strkey, &strkey_len, &case_sensitive) == FAILURE) {
        return;
    }

    if(!strkey_len) RETURN_FALSE;

#if PHP_API_VERSION <= 20041225
#if HAVE_APACHE && defined(APC_PHP4_STAT)
    t = ((request_rec *)SG(server_context))->request_time;
#else 
    t = time(0);
#endif
#else 
    t = sapi_get_request_time(TSRMLS_C);
#endif

    entry = apc_cache_user_find(apc_user_cache, strkey, strkey_len + 1, t);

    if(entry) {
        _apc_define_constants(entry->data.user.val, case_sensitive TSRMLS_CC);
        apc_cache_release(apc_user_cache, entry);
        RETURN_TRUE;
    } else {
        RETURN_FALSE;
    }
}
/* }}} */

/* {{{ apc_functions[] */
function_entry apc_functions[] = {
	PHP_FE(apc_cache_info,          NULL)
	PHP_FE(apc_clear_cache,         NULL)
	PHP_FE(apc_sma_info,            NULL)
	PHP_FE(apc_store,               NULL)
	PHP_FE(apc_fetch,               NULL)
	PHP_FE(apc_delete,              NULL)
	PHP_FE(apc_define_constants,    NULL)
	PHP_FE(apc_load_constants,      NULL)
	{NULL, 		NULL,				NULL}
};
/* }}} */

/* {{{ module definition structure */

zend_module_entry apc_module_entry = {
	STANDARD_MODULE_HEADER,
	"apc",
	apc_functions,
	PHP_MINIT(apc),
	PHP_MSHUTDOWN(apc),
	PHP_RINIT(apc),
	PHP_RSHUTDOWN(apc),
	PHP_MINFO(apc),
	APC_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_APC
ZEND_GET_MODULE(apc)
#endif
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
