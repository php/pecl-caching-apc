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

#ifdef PHP_WIN32
/* XXX lame hack, for some reason, the ini entries are all messed
   up if PHP_EXPORTS is not defined, however, this should *not*
   be defined in extensions, and if defined for the project, core_globals_id
   is not properly imported (used by the PG macro) */
#define PHP_EXPORTS
#endif

#include "php_apc.h"
#include "apc_cache.h"
#include "apc_main.h"
#include "apc_sma.h"
#include "php_globals.h"
#include "php_ini.h"
#include "zend_extensions.h"
#include "ext/standard/info.h"

/* {{{ PHP_FUNCTION declarations */
PHP_FUNCTION(apc_cache_info);
PHP_FUNCTION(apc_clear_cache);
PHP_FUNCTION(apc_sma_info);
/* }}} */

/* {{{ ZEND_DECLARE_MODULE_GLOBALS(apc) */

PHPAPI ZEND_DECLARE_MODULE_GLOBALS(apc)

static void php_apc_init_globals(zend_apc_globals* apc_globals TSRMLS_DC)
{
    apc_globals->filters = NULL;
    apc_globals->initialized = 0;
    apc_globals->cache = NULL;
    apc_globals->cache_stack = NULL;
    apc_globals->compiled_filters = NULL;
}

#ifdef ZTS
static void php_apc_shutdown_globals(zend_apc_globals* apc_globals TSRMLS_DC)
{
	char* p;
    /* deallocate the ignore patterns */
    if (apc_globals->filters != NULL) {
        for (p = apc_globals->filters[0]; p != NULL; p++) {
            apc_efree(p);
        }
        apc_efree(apc_globals->filters);
    }

    /* the rest of the globals are cleaned up in apc_module_shutdown() */
}
#endif

/* }}} */

/* {{{ PHP_INI */

static PHP_INI_MH(OnUpdate_filters)
{
    APCG(filters) = apc_tokenize(new_value, ',');
    return SUCCESS;
}

PHP_INI_BEGIN()
STD_PHP_INI_ENTRY("apc.enabled",        "1",    PHP_INI_SYSTEM, OnUpdateInt,            enabled,        zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.shm_segments",   "1",    PHP_INI_SYSTEM, OnUpdateInt,            shm_segments,   zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.shm_size",       "30",   PHP_INI_SYSTEM, OnUpdateInt,            shm_size,       zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.optimization",   "0",    PHP_INI_SYSTEM, OnUpdateInt,            optimization,   zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.num_files_hint", "1000", PHP_INI_SYSTEM, OnUpdateInt,            num_files_hint, zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.gc_ttl",         "3600", PHP_INI_SYSTEM, OnUpdateInt,            gc_ttl,         zend_apc_globals, apc_globals)
#if APC_MMAP
STD_PHP_INI_ENTRY("apc.mmap_file_mask",  NULL,  PHP_INI_SYSTEM, OnUpdateString,         mmap_file_mask, zend_apc_globals, apc_globals)
#endif
    PHP_INI_ENTRY("apc.filters",        "",     PHP_INI_SYSTEM, OnUpdate_filters)
PHP_INI_END()

/* }}} */

/* {{{ PHP_MINFO_FUNCTION(apc) */
static PHP_MINFO_FUNCTION(apc)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "APC Support", APCG(enabled) ? "enabled" : "disabled");
	php_info_print_table_row(2, "Version", apc_version());
#if APC_MMAP
	php_info_print_table_row(2, "MMAP Support", "Enabled");
	php_info_print_table_row(2, "MMAP File Mask", APCG(mmap_file_mask));
#else
	php_info_print_table_row(2, "MMAP Support", "Disabled");
#endif
	php_info_print_table_row(2, "Revision", "$Revision$");
	php_info_print_table_row(2, "Build Date", __DATE__ " " __TIME__);
    DISPLAY_INI_ENTRIES();
	php_info_print_table_end();
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION(apc) */
static PHP_MINIT_FUNCTION(apc)
{
	ZEND_INIT_MODULE_GLOBALS(apc, php_apc_init_globals,
                             php_apc_shutdown_globals);

    REGISTER_INI_ENTRIES();

    if (APCG(enabled))
        apc_module_init();

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION(apc) */
static PHP_MSHUTDOWN_FUNCTION(apc)
{
    apc_module_shutdown();
    UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION(apc) */
static PHP_RINIT_FUNCTION(apc)
{
    apc_request_init();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION(apc) */
static PHP_RSHUTDOWN_FUNCTION(apc)
{
    apc_request_shutdown();
	return SUCCESS;
}
/* }}} */

/* {{{ proto array apc_cache_info() */
PHP_FUNCTION(apc_cache_info)
{
    apc_cache_info_t* info;
    apc_cache_link_t* p;
    zval* list;

    if (ZEND_NUM_ARGS() != 0) {
        WRONG_PARAM_COUNT;
    }

    info = apc_cache_info(APCG(cache));

    array_init(return_value);
    add_assoc_long(return_value, "num_slots", info->num_slots);
    add_assoc_long(return_value, "num_hits", info->num_hits);
    add_assoc_long(return_value, "num_misses", info->num_misses);

    ALLOC_INIT_ZVAL(list);
    array_init(list);

    for (p = info->list; p != NULL; p = p->next) {
        zval* link;

        ALLOC_INIT_ZVAL(link);
        array_init(link);

        add_assoc_string(link, "filename", p->filename, 1);
        add_assoc_long(link, "device", p->device);
        add_assoc_long(link, "inode", p->inode);
        add_assoc_long(link, "num_hits", p->num_hits);
        add_assoc_long(link, "mtime", p->mtime);
        add_assoc_long(link, "creation_time", p->creation_time);
        add_assoc_long(link, "deletion_time", p->deletion_time);
        add_assoc_long(link, "ref_count", p->ref_count);
        add_next_index_zval(list, link);
    }
    add_assoc_zval(return_value, "cache_list", list);

    ALLOC_INIT_ZVAL(list);
    array_init(list);

    for (p = info->deleted_list; p != NULL; p = p->next) {
        zval* link;

        ALLOC_INIT_ZVAL(link);
        array_init(link);

        add_assoc_string(link, "filename", p->filename, 1);
        add_assoc_long(link, "device", p->device);
        add_assoc_long(link, "inode", p->inode);
        add_assoc_long(link, "num_hits", p->num_hits);
        add_assoc_long(link, "mtime", p->mtime);
        add_assoc_long(link, "creation_time", p->creation_time);
        add_assoc_long(link, "deletion_time", p->deletion_time);
        add_assoc_long(link, "ref_count", p->ref_count);
        add_next_index_zval(list, link);
    }
    add_assoc_zval(return_value, "deleted_list", list);

    apc_cache_free_info(info);
}
/* }}} */

/* {{{ proto void apc_clear_cache() */
PHP_FUNCTION(apc_clear_cache)
{
    if (ZEND_NUM_ARGS() != 0) {
        WRONG_PARAM_COUNT;
    }
    apc_cache_clear(APCG(cache));
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

    array_init(return_value);
    add_assoc_long(return_value, "num_seg", info->num_seg);
    add_assoc_long(return_value, "seg_size", info->seg_size);
    add_assoc_long(return_value, "avail_mem", apc_sma_get_avail_mem());

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

/* {{{ apc_functions[] */
function_entry apc_functions[] = {
	PHP_FE(apc_cache_info,          NULL)
	PHP_FE(apc_clear_cache,         NULL)
	PHP_FE(apc_sma_info,            NULL)
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
	NO_VERSION_YET,
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
