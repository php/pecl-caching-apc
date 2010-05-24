/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2009 The PHP Group                                |
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
#include "apc_iterator.h"
#include "apc_main.h"
#include "apc_sma.h"
#include "apc_lock.h"
#include "apc_bin.h"
#include "php_globals.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/file.h"
#include "ext/standard/flock_compat.h"
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#include "SAPI.h"
#include "rfc1867.h"
#include "php_apc.h"
#include "ext/standard/md5.h"

#if HAVE_SIGACTION
#include "apc_signal.h"
#endif

#ifndef zend_parse_parameters_none
# define zend_parse_parameters_none() zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "")
#endif

/* {{{ PHP_FUNCTION declarations */
PHP_FUNCTION(apc_cache_info);
#ifdef APC_FILEHITS
PHP_FUNCTION(apc_filehits);
#endif
PHP_FUNCTION(apc_clear_cache);
PHP_FUNCTION(apc_sma_info);
PHP_FUNCTION(apc_store);
PHP_FUNCTION(apc_fetch);
PHP_FUNCTION(apc_delete);
PHP_FUNCTION(apc_delete_file);
PHP_FUNCTION(apc_compile_file);
PHP_FUNCTION(apc_define_constants);
PHP_FUNCTION(apc_load_constants);
PHP_FUNCTION(apc_add);
PHP_FUNCTION(apc_inc);
PHP_FUNCTION(apc_dec);
PHP_FUNCTION(apc_cas);
PHP_FUNCTION(apc_bin_dump);
PHP_FUNCTION(apc_bin_load);
PHP_FUNCTION(apc_bin_dumpfile);
PHP_FUNCTION(apc_bin_loadfile);
/* }}} */

/* {{{ ZEND_DECLARE_MODULE_GLOBALS(apc) */
ZEND_DECLARE_MODULE_GLOBALS(apc)

static void php_apc_cache_init(apc_cache_t *cache, char *arg_name, int name_len, long id)
{
    char *name, *const_name;
    int i, c;

    cache->id = id;
    cache->type = id & APC_CACHE_FILE ? APC_CACHE_FILE : APC_CACHE_USER;

    cache->size_hint = 0;
    cache->write_lock = 1;
    cache->ttl = 0;

    /* File cache specific */
    cache->filters = NULL;
    cache->compiled_filters = NULL;
    cache->cache_by_default = 1;
    cache->fpstat = 1;
    cache->stat_ctime = 0;
    cache->gc_ttl = 0;
    cache->file_update_protection = 0;
    cache->max_file_size = 0;
    cache->expunge_method = APC_CACHE_EXPUNGE_FLUSH;

    /* convert name to a constant friendly format
     * must match: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]
     */
    name = apc_emalloc(name_len +1);
    memcpy(name, arg_name, name_len+1);
    const_name = apc_emalloc(sizeof("APC_") + name_len +1);
    memcpy(const_name, "APC_", sizeof("APC_")-1);
    c = sizeof("APC_")-1;
    for (i=0; i < name_len; i++, c++) {
        if (    (name[i] >= 'a' && name[i] <= 'z')
             || (name[i] >= 'A' && name[i] <= 'Z')
             || (name[i] >= '0' && name[i] <= '9')
             || (name[i] == '_')
           ) {
            const_name[c] = toupper(name[i]);
        } else {
            apc_eprint("Cache names must conform to constant naming rules: %s", name);
        }
    }
    const_name[c] = '\0';
    cache->name = name;
    cache->name_len = name_len;
    cache->const_name = const_name;
    cache->sma_segment = NULL;
    cache->cache_stack = apc_stack_create(0);
}

static void php_apc_init_globals(zend_apc_globals* apc_globals TSRMLS_DC)
{
    apc_globals->initialized = 0;
    apc_globals->canonicalize = 1;
    apc_globals->slam_defense = 1;
    apc_globals->report_autofilter = 0;
    apc_globals->include_once = 0;
    apc_globals->apc_optimize_function = NULL;
    apc_globals->mmap_file_mask = NULL;
#ifdef MULTIPART_EVENT_FORMDATA
    apc_globals->rfc1867 = 0;
    memset(&(apc_globals->rfc1867_data), 0, sizeof(apc_rfc1867_data));
    apc_globals->rfc1867_cache = NULL;
#endif
    memset(&apc_globals->copied_zvals, 0, sizeof(HashTable));
    apc_globals->force_file_update = 0;
    apc_globals->coredump_unmap = 0;
    apc_globals->preload_path = NULL;
    apc_globals->use_request_time = 1;
    apc_globals->lazy_class_table = NULL;
    apc_globals->lazy_function_table = NULL;
    apc_globals->file_caches = NULL;
    apc_globals->user_caches = NULL;
    apc_globals->num_file_caches = 0;
    apc_globals->default_file_cache = NULL;
    apc_globals->num_user_caches = 0;
    apc_globals->default_user_cache = NULL;
    apc_globals->sma_segments_head = NULL;

}

static void php_apc_shutdown_globals(zend_apc_globals* apc_globals TSRMLS_DC)
{
    int i;
    apc_cache_t *cache;

    /* deallocate the ignore patterns */
    for (i=0; i < apc_globals->num_file_caches; i++) {
        cache = &(apc_globals->file_caches[i]);
        if (cache->filters != NULL) {
            int i;
            for (i=0; cache->filters[i] != NULL; i++) {
                apc_efree(cache->filters[i]);
            }
            apc_efree(cache->filters);
        }

        /* the stack should be empty */
        assert(apc_stack_size(cache->cache_stack) == 0);

        /* apc cleanup */
        apc_stack_destroy(cache->cache_stack);
    }

    /* the rest of the globals are cleaned up in apc_module_shutdown() */
}

/* }}} */

/* {{{ PHP_INI */

#define PHP_APC_CACHE_INI_BOOL    1
#define PHP_APC_CACHE_INI_LONG    2
#define PHP_APC_CACHE_INI_STRING  3
#define PHP_APC_CACHE_INI_EXPUNGE 4
#define PHP_APC_CACHE_INI_FILTERS 5

#define APC_INI_BEGIN(NAME) zend_ini_entry NAME[] = {
#define APC_INI_END()       PHP_INI_END()

#define PHP_APC_CACHE_INI_ENTRY(name, default_value, modifiable, type, property_name) \
        PHP_INI_ENTRY3(name, default_value, modifiable, OnUpdateCache, (void*)type, (void*)XtOffsetOf(apc_cache_t, property_name), NULL)

static PHP_INI_MH(OnUpdateFilecaches);
static PHP_INI_MH(OnUpdateUsercaches);

/* Lifted and modified from Zend/zend_operators.c */
static size_t apc_atol(const char *str, int str_len) /* {{{ */
{
	double retval;

	if (!str_len) {
		str_len = strlen(str);
	}
	retval = strtod(str, NULL);
	if (str_len>0) {
		switch (str[str_len-1]) {
			case 'g':
			case 'G':
				retval *= (double)1024;
				/* break intentionally missing */
			case 'm':
			case 'M':
				retval *= (double)1024;
				/* break intentionally missing */
			case 'k':
			case 'K':
				retval *= (double)1024;
				break;
		}
	}
	return (size_t)retval;
}
/* }}} */

#ifdef MULTIPART_EVENT_FORMDATA
static PHP_INI_MH(OnUpdateRfc1867Freq) /* {{{ */
{
    int tmp;
    tmp = zend_atoi(new_value, new_value_length);
    if(tmp < 0) {
        apc_eprint("rfc1867_freq must be greater than or equal to zero.");
        return FAILURE;
    }
    if(new_value[new_value_length-1] == '%') {
        if(tmp > 100) {
            apc_eprint("rfc1867_freq cannot be over 100%%");
            return FAILURE;
        }
        APCG(rfc1867_freq) = tmp / 100.0;
    } else {
        APCG(rfc1867_freq) = tmp;
    }
    return SUCCESS;
}
/* }}} */

static PHP_INI_MH(OnUpdateRfc1867Cache) /* {{{ */
{
    int i;
    apc_cache_t *cache;

    if (!APCG(rfc1867)) return SUCCESS;

    if (new_value_length == 0) {
        APCG(rfc1867_cache) = &APCG(user_caches)[0];
        return SUCCESS;
    }

    for (i=0; i < APCG(num_user_caches); i++) { 
        cache = &APCG(user_caches)[i];
        if (strncasecmp(cache->name, new_value, cache->name_len) == 0 || strncasecmp(cache->const_name, new_value, cache->name_len) == 0) {
            APCG(rfc1867_cache) = cache;
            return SUCCESS;
        }
    }

    apc_eprint("apc.rfc1867_cache (%s) does not match any known user cache name or user cache constant name (file caches cannot be used for upload progress).", new_value);
    return FAILURE;
}
/* }}} */
#endif


/* {{{ OnUpdateSegments */
static PHP_INI_MH(OnUpdateSegments)
{
    char *tmp, *cur;
    size_t size;
    apc_segment_t *seg=NULL, *prev_seg=NULL;
    int len;

    if (!new_value || new_value[0] == '\0') {
        return SUCCESS;
    }

    /* ',' separated list of segment sizes */
    cur = tmp = estrndup(new_value, new_value_length+1);
    cur = strtok(tmp, ",");
    while (cur) {
        len = strlen(cur);
        while (len > 0 && cur[len-1] == ' ') { len--; }
        size = apc_atol(cur, len);
        if (!size) {
            apc_eprint("Encountered error while parsing apc.segments value.");
            return FAILURE;
        }
        seg = apc_emalloc(sizeof(apc_segment_t));
        seg->size = size;
        seg->next = NULL;
        seg->initialized = 0;
        seg->unmap = 0;
        if (!prev_seg) {
            APCG(sma_segments_head) = seg;
        } else {
            prev_seg->next = seg;
        }
        prev_seg = seg;
        cur = strtok(NULL, ",");
    }
    efree(tmp);

    return SUCCESS;
}
/* }}} */

/* {{{ OnUpdateUnmap */
static PHP_INI_MH(OnUpdateUnmap)
{
    char *tmp, *cur;
    int id, len, c;
    apc_segment_t *cur_seg;

    if (!new_value || new_value[0] == '\0') {
        return SUCCESS;
    }

    APCG(coredump_unmap) = 1;

    /* ',' separated list of segment ids */
    cur = tmp = estrndup(new_value, new_value_length+1);
    cur = strtok(tmp, ",");
    while (cur) {
        len = strlen(cur);
        while (len > 0 && cur[len-1] == ' ') { len--; }
        id = zend_atoi(cur, len);
        c=0;
        cur_seg = APCG(sma_segments_head);
        while(cur_seg) {
            if (c == id) {
                cur_seg->unmap = 1;
                c = -1; /* found */
                break;
            }
            c++;
            cur_seg = cur_seg->next;
        }
        if (c != -1) {
            apc_eprint("Invalid index %d in apc.coredump_unmap ini.", id);
            return FAILURE;
        }
        cur = strtok(NULL, ",");
    }
    efree(tmp);

    return SUCCESS;
}
/* }}} */


/* {{{ OnUpdateCache */
static PHP_INI_MH(OnUpdateCache) /* {{{ */
{
    apc_cache_t *cache;

    if (!new_value) { return SUCCESS; }


    cache = APCG(current_cache);
    switch ((long)mh_arg1) {
        case PHP_APC_CACHE_INI_BOOL:
            {
                zend_bool *p = (zend_bool*) ( (char*)cache + (size_t)mh_arg2 );
                if (!strcasecmp(new_value, "on") || !strcasecmp(new_value, "yes") || !strcasecmp(new_value, "true")) {
                    *p = (zend_bool) 1;
                } else {
                    *p = (zend_bool) atoi(new_value);
                }
            }
            break;
        case PHP_APC_CACHE_INI_LONG:
            {
                long *p = (long*) ( (char*)cache + (size_t)mh_arg2 );
                *p = zend_atoi(new_value, new_value_length);
            }
            break;
        case PHP_APC_CACHE_INI_STRING:
            {
                char **p = (char**) ( (char*)cache + (size_t)mh_arg2 );
                *p = new_value;
            }
            break;
        case PHP_APC_CACHE_INI_EXPUNGE:
            {
                long *p = (long*) ( (char*)cache + (size_t)mh_arg2 );
                if (!strcasecmp(new_value, "flush")) {
                    *p = APC_CACHE_EXPUNGE_FLUSH;
                } else if (!strcasecmp(new_value, "lfu")) {
#if APC_LFU
                    *p = APC_CACHE_EXPUNGE_LFU;
#else
                    apc_eprint("LFU expunge method specified, but LFU support not enabled.  (Try recompiling with --enable-apc-lfu.)");
#endif
                } else if (!strcasecmp(new_value, "none")) {
                    *p = APC_CACHE_EXPUNGE_NONE;
                } else {
                    apc_eprint("Unrecognized expunge method: %s.", new_value);
                }
            }
            break;
        case PHP_APC_CACHE_INI_FILTERS:
            {
                if (new_value_length) {
                    char ***p = (char***) ( (char*)cache + (size_t)mh_arg2 );
                    *p = apc_tokenize(new_value, ',');
                }
            }
            break;
        default:
            apc_eprint("Unknown type '%d' encountered in PHP_APC_CACHE_INI_ENTRY", mh_arg1);
    }

    return SUCCESS;
}
/* }}} */

/* {{{ File Cache INI Options */
APC_INI_BEGIN(apc_file_cache_entries)
PHP_APC_CACHE_INI_ENTRY(  "apc.segment",                "0",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_LONG,    segment_idx)
PHP_APC_CACHE_INI_ENTRY(  "apc.ttl",                    "0",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_LONG,    ttl)
PHP_APC_CACHE_INI_ENTRY(  "apc.gc_ttl",                 "3600",   PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_LONG,    gc_ttl)
PHP_APC_CACHE_INI_ENTRY(  "apc.entries_hint",           "4096",   PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_LONG,    size_hint)
PHP_APC_CACHE_INI_ENTRY(  "apc.expunge_method",         "flush",  PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_EXPUNGE, expunge_method)
PHP_APC_CACHE_INI_ENTRY(  "apc.stat",                   "1",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_BOOL,    fpstat)
PHP_APC_CACHE_INI_ENTRY(  "apc.stat_ctime",             "0",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_BOOL,    stat_ctime)
PHP_APC_CACHE_INI_ENTRY(  "apc.filters",                NULL,     PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_FILTERS, filters)
PHP_APC_CACHE_INI_ENTRY(  "apc.cache_by_default",       "1",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_BOOL,    cache_by_default)
PHP_APC_CACHE_INI_ENTRY(  "apc.file_update_protection", "2",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_LONG,    file_update_protection)
PHP_APC_CACHE_INI_ENTRY(  "apc.max_file_size",          "1M",     PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_LONG,    max_file_size)
PHP_APC_CACHE_INI_ENTRY(  "apc.write_lock",             "1",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_BOOL,    write_lock)
PHP_APC_CACHE_INI_ENTRY(  "apc.md5",                    "0",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_BOOL,    md5)
PHP_APC_CACHE_INI_ENTRY(  "apc.lazy_functions",         "0",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_BOOL,    lazy_functions)
PHP_APC_CACHE_INI_ENTRY(  "apc.lazy_classes",           "0",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_BOOL,    lazy_classes)
APC_INI_END()
/* }}} */

/* {{{ User Cache INI Options */
APC_INI_BEGIN(apc_user_cache_entries)
PHP_APC_CACHE_INI_ENTRY(  "apc.segment",                "0",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_LONG,    segment_idx)
PHP_APC_CACHE_INI_ENTRY(  "apc.ttl",                    "0",      PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_LONG,    ttl)
PHP_APC_CACHE_INI_ENTRY(  "apc.gc_ttl",                 "3600",   PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_LONG,    gc_ttl)
PHP_APC_CACHE_INI_ENTRY(  "apc.entries_hint",           "4096",   PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_LONG,    size_hint)
PHP_APC_CACHE_INI_ENTRY(  "apc.expunge_method",         "flush",  PHP_INI_SYSTEM,   PHP_APC_CACHE_INI_EXPUNGE, expunge_method)
APC_INI_END()
/* }}} */

/* {{{ Global INI Options */
PHP_INI_BEGIN()
STD_PHP_INI_BOOLEAN(    "apc.enabled",               "1",    PHP_INI_SYSTEM, OnUpdateBool,     enabled,             zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.segments",       "30M",  PHP_INI_SYSTEM, OnUpdateSegments,     sma_segments_head,  zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.coredump_unmap", "",    PHP_INI_SYSTEM, OnUpdateUnmap,         coredump_unmap,     zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.file_caches",    "File", PHP_INI_SYSTEM, OnUpdateFilecaches,   default_file_cache, zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.user_caches",    "User", PHP_INI_SYSTEM, OnUpdateUsercaches,   default_user_cache, zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN("apc.canonicalize", "1",    PHP_INI_SYSTEM, OnUpdateBool,           canonicalize,     zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN("apc.slam_defense", "1",    PHP_INI_SYSTEM, OnUpdateBool,           slam_defense,     zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN(    "apc.include_once_override", "0",    PHP_INI_SYSTEM, OnUpdateBool,     include_once,        zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN(    "apc.report_autofilter",     "0",    PHP_INI_SYSTEM, OnUpdateBool,     report_autofilter,   zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY(      "apc.mmap_file_mask",        NULL,   PHP_INI_SYSTEM, OnUpdateString,   mmap_file_mask,      zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN(    "apc.enable_cli",            "0",    PHP_INI_SYSTEM, OnUpdateBool,     enable_cli,          zend_apc_globals, apc_globals)
#ifdef MULTIPART_EVENT_FORMDATA
STD_PHP_INI_BOOLEAN("apc.rfc1867", "0", PHP_INI_SYSTEM, OnUpdateBool, rfc1867, zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.rfc1867_prefix", "upload_", PHP_INI_SYSTEM, OnUpdateStringUnempty, rfc1867_prefix, zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.rfc1867_name", "APC_UPLOAD_PROGRESS", PHP_INI_SYSTEM, OnUpdateStringUnempty, rfc1867_name, zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.rfc1867_freq", "0", PHP_INI_SYSTEM, OnUpdateRfc1867Freq, rfc1867_freq, zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.rfc1867_ttl", "3600", PHP_INI_SYSTEM, OnUpdateLong, rfc1867_ttl, zend_apc_globals, apc_globals)
STD_PHP_INI_ENTRY("apc.rfc1867_cache", "", PHP_INI_SYSTEM, OnUpdateRfc1867Cache, rfc1867_cache, zend_apc_globals, apc_globals)
#endif
STD_PHP_INI_ENTRY("apc.preload_path", (char*)NULL,              PHP_INI_SYSTEM, OnUpdateString,       preload_path,  zend_apc_globals, apc_globals)
STD_PHP_INI_BOOLEAN("apc.use_request_time", "1", PHP_INI_ALL, OnUpdateBool, use_request_time,  zend_apc_globals, apc_globals)

PHP_INI_END()
/* }}} */

/* {{{ apc_ini_cache_helper */
static int apc_ini_cache_helper(char *new_value, int new_value_length, long type, zend_ini_entry *orig_entries, int *num_caches, apc_cache_t **caches TSRMLS_DC) {

    zend_ini_entry *entries;
    int num_entries, i, c, len;
    char *name;
    apc_cache_t *cache;
    char *tmp, *cur;
    apc_segment_t *seg;

    /* ',' separated list of caches */
    *num_caches = 1;
    for (i=0; i < new_value_length; i++) {
        if (new_value[i] == ',')
            (*num_caches)++;
    }
    *caches = apc_emalloc( sizeof(apc_cache_t) * (*num_caches) );
    cur = tmp = estrndup(new_value, new_value_length+1);
    cur = strtok(tmp, ",");
    i = 0;
    while (cur) {
        len = strlen(cur);
        while (len > 0 && cur[len-1] == ' ') { len--; }
        while (cur && cur[0] == ' ') { cur++; len--; }
        php_apc_cache_init(&(*caches)[i], cur, len, type | (i+1));
        i++;
        cur = strtok(NULL, ",");
    }
    efree(tmp);

    for (i=0; i < *num_caches; i++) {
        for (c=0; c < *num_caches; c++) {
            if (i != c && !strncasecmp((*caches)[i].name, (*caches)[c].name, (*caches)[c].name_len)) {
                apc_eprint("Cache names cannot be the same: %s", (*caches)[i].name);
            }
        }
    }

    /* rename INI entries and register them */
    num_entries = 0;
    while (orig_entries[num_entries].name) {
        num_entries++;
    }
    for(i=0; i < *num_caches; i++) {
        cache = &((*caches)[i]);
        entries = apc_emalloc(sizeof(zend_ini_entry) * (num_entries+1));
        for (c=0; c < num_entries; c++) {  /* replace names with cache specific names */
            entries[c] = orig_entries[c];
            len = orig_entries[c].name_length + cache->name_len;
            name = apc_emalloc(len +1);
            strlcpy(name, orig_entries[c].name, orig_entries[c].name_length);
            memcpy(&name[sizeof("apc")], cache->name, cache->name_len);
            name[sizeof("apc") + cache->name_len] = '.';
            strlcpy(&name[sizeof("apc") + cache->name_len +1], &orig_entries[c].name[sizeof("apc")], orig_entries[c].name_length - sizeof("apc"));
            zend_str_tolower(name, len+1);
            entries[c].name = name;
            entries[c].name_length = len +1;
        }
        entries[num_entries] = orig_entries[num_entries];
        APCG(current_cache) = cache;  /* for use by ini handlers */
        zend_register_ini_entries(entries, apc_module_entry.module_number TSRMLS_CC);
        APCG(current_cache) = NULL;
        cache->ini_entries = entries;  /* so we can free them in MINIT */

        seg = APCG(sma_segments_head);
        for(c=0; c < cache->segment_idx; c++) {
            if (!seg) {
                apc_eprint("Segment index %d does not appear to be valid.", cache->segment_idx);
            }
            seg = seg->next;
        }
        cache->sma_segment = seg;
    }


    return SUCCESS;
}
/* }}} */

/* {{{ OnUpdateFilecaches */
static PHP_INI_MH(OnUpdateFilecaches) /* {{{ */
{
    if (!new_value_length) { 
        apc_eprint("You must specify at least one apc file cache in apc.file_caches.");
        return FAILURE; 
    }
    return apc_ini_cache_helper(new_value, new_value_length, APC_CACHE_FILE, apc_file_cache_entries, &APCG(num_file_caches), &APCG(file_caches) TSRMLS_CC);
}
/* }}} */

/* {{{ OnUpdateUsercaches */
static PHP_INI_MH(OnUpdateUsercaches) /* {{{ */
{
    if (!new_value_length) { 
        apc_eprint("You must specify at least one apc user cache in apc.user_caches.");
        return FAILURE; 
    }
    return apc_ini_cache_helper(new_value, new_value_length, APC_CACHE_USER, apc_user_cache_entries, &APCG(num_user_caches), &APCG(user_caches) TSRMLS_CC);
}
/* }}} */


PHPAPI void php_html_puts(const char *str, uint size TSRMLS_DC);

/* {{{ apc_ini_displayer_cb  (copied from php_ini_displayer_cb)
 */
static void apc_ini_displayer_cb(zend_ini_entry *ini_entry, int type TSRMLS_DC)
{
    if (ini_entry->displayer) {
        ini_entry->displayer(ini_entry, type);
    } else {
        char *display_string;
        uint display_string_length, esc_html=0;

        if (type == ZEND_INI_DISPLAY_ORIG && ini_entry->modified) {
            if (ini_entry->orig_value && ini_entry->orig_value[0]) {
                display_string = ini_entry->orig_value;
                display_string_length = ini_entry->orig_value_length;
                esc_html = !sapi_module.phpinfo_as_text;
            } else {
                if (!sapi_module.phpinfo_as_text) {
                    display_string = "<i>no value</i>";
                    display_string_length = sizeof("<i>no value</i>") - 1;
                } else {
                    display_string = "no value";
                    display_string_length = sizeof("no value") - 1;
                }
            }
        } else if (ini_entry->value && ini_entry->value[0]) {
            display_string = ini_entry->value;
            display_string_length = ini_entry->value_length;
            esc_html = !sapi_module.phpinfo_as_text;
        } else {
            if (!sapi_module.phpinfo_as_text) {
                display_string = "<i>no value</i>";
                display_string_length = sizeof("<i>no value</i>") - 1;
            } else {
                display_string = "no value";
                display_string_length = sizeof("no value") - 1;
            }
        }

        if (esc_html) {
            php_html_puts(display_string, display_string_length TSRMLS_CC);
        } else {
            PHPWRITE(display_string, display_string_length);
        }
    }
}
/* }}} */


/* {{{ apc_ini_displayer (copied from php_ini_displayer)
 */
static int apc_ini_displayer(zend_ini_entry *ini_entry, int module_number TSRMLS_DC)
{
    if (ini_entry->module_number != module_number) {
        return 0;
    }
    if (!sapi_module.phpinfo_as_text) {
        PUTS("<tr>");
        PUTS("<td class=\"e\">");
        PHPWRITE(ini_entry->name, ini_entry->name_length - 1);
        PUTS("</td><td class=\"v\">");
        apc_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ACTIVE TSRMLS_CC);
        PUTS("</td><td class=\"v\">");
        apc_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ORIG TSRMLS_CC);
        PUTS("</td></tr>\n");
    } else {
        PHPWRITE(ini_entry->name, ini_entry->name_length - 1);
        PUTS(" => ");
        apc_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ACTIVE TSRMLS_CC);
        PUTS(" => ");
        apc_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ORIG TSRMLS_CC);
        PUTS("\n");
    }
    return 0;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION(apc) */
static PHP_MINFO_FUNCTION(apc)
{
    zend_ini_entry *entries;
    zend_ini_entry *entry;
    int i, i2;
    int spaces;

    php_info_print_table_start();
    php_info_print_table_header(2, "APC Support", APCG(enabled) ? "enabled" : "disabled");
    php_info_print_table_row(2, "Version", PHP_APC_VERSION);
#if APC_MMAP
    php_info_print_table_row(2, "MMAP Support", "Enabled");
    php_info_print_table_row(2, "MMAP File Mask", APCG(mmap_file_mask));
#else
    php_info_print_table_row(2, "MMAP Support", "Disabled");
#endif
    php_info_print_table_row(2, "Locking type", APC_LOCK_TYPE);
    php_info_print_table_row(2, "Revision", "$Revision$");
    php_info_print_table_row(2, "Build Date", __DATE__ " " __TIME__);
    php_info_print_table_end();

    /* non cache specific configurations */
    php_info_print_table_start();
    php_info_print_table_header(3, "Directive", "Local Value", "Master Value");
    i=0;
    while(ini_entries[i].name) {
        if (zend_hash_find(EG(ini_directives), ini_entries[i].name, ini_entries[i].name_length, (void**) &entry) == SUCCESS) {
            apc_ini_displayer(entry, zend_module->module_number TSRMLS_CC);   
        } else {
            apc_eprint("Internal error, could not find INI entry: %s", ini_entries[i].name);
        }
        i++;
    } 
    php_info_print_table_end();

    /* display ini entries on a per cache basis */
    for(i=0; i < APCG(num_file_caches); i++) { 
      php_info_print_table_start();
      if (!sapi_module.phpinfo_as_text) {
        php_printf("<tr class=\"h\"><th colspan=\"3\">%s: \"%s\" file cache </th></tr>\n", APCG(file_caches)[i].const_name, APCG(file_caches)[i].name );
      } else {
        spaces = (74 - strlen(APCG(file_caches)[i].name));
        php_printf("%*s %s: \"%s\" file cache %*s\n", (int)(spaces/2), " ", APCG(file_caches)[i].const_name, APCG(file_caches)[i].name, (int)(spaces/2), " ");
      }
      php_info_print_table_header(3, "Directive", "Local Value", "Master Value");
      i2=0;
      entries = APCG(file_caches)[i].ini_entries;
      while(entries[i2].name) {
          if (zend_hash_find(EG(ini_directives), entries[i2].name, entries[i2].name_length, (void**) &entry) == SUCCESS) {
              apc_ini_displayer(entry, zend_module->module_number TSRMLS_CC);   
          } else {
              apc_eprint("Internal error, could not find INI entry: %s", entries[i2].name);
          }
          i2++;
      } 
      php_info_print_table_end();
    } 
    for(i=0; i < APCG(num_user_caches); i++) { 
      php_info_print_table_start();
      if (!sapi_module.phpinfo_as_text) {
        php_printf("<tr class=\"h\"><th colspan=\"3\">%s: \"%s\" user cache </th></tr>\n", APCG(user_caches)[i].const_name, APCG(user_caches)[i].name );
      } else {
        spaces = (74 - strlen(APCG(user_caches)[i].name));
        php_printf("%*s %s: \"%s\" user cache %*s\n", (int)(spaces/2), " ", APCG(user_caches)[i].const_name, APCG(user_caches)[i].name, (int)(spaces/2), " ");
      }
      php_info_print_table_header(3, "Directive", "Local Value", "Master Value");
      i2=0;
      entries = APCG(user_caches)[i].ini_entries;
      while(entries[i2].name) {
          if (zend_hash_find(EG(ini_directives), entries[i2].name, entries[i2].name_length, (void**) &entry) == SUCCESS) {
              apc_ini_displayer(entry, zend_module->module_number TSRMLS_CC);   
          } else {
              apc_eprint("Internal error, could not find INI entry: %s", entries[i2].name);
          }
          i2++;
      } 
      php_info_print_table_end();
    } 
    
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
        if(APCG(initialized)) {
            apc_process_init(module_number TSRMLS_CC);
        } else {
            apc_module_init(module_number TSRMLS_CC);
            apc_zend_init(TSRMLS_C);
            apc_process_init(module_number TSRMLS_CC);
#ifdef MULTIPART_EVENT_FORMDATA
            /* File upload progress tracking */
            if(APCG(rfc1867)) {
                php_rfc1867_callback = apc_rfc1867_progress;
            }
#endif
            apc_iterator_init(module_number TSRMLS_CC);
        }

        zend_register_long_constant("APC_BIN_VERIFY_MD5", sizeof("APC_BIN_VERIFY_MD5"), APC_BIN_VERIFY_MD5, (CONST_CS | CONST_PERSISTENT), module_number TSRMLS_CC);
        zend_register_long_constant("APC_BIN_VERIFY_CRC32", sizeof("APC_BIN_VERIFY_CRC32"), APC_BIN_VERIFY_CRC32, (CONST_CS | CONST_PERSISTENT), module_number TSRMLS_CC);
    }

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION(apc) */
static PHP_MSHUTDOWN_FUNCTION(apc)
{
    if(APCG(enabled)) {
        apc_process_shutdown(TSRMLS_C);
        apc_zend_shutdown(TSRMLS_C);
        apc_module_shutdown(TSRMLS_C);
#ifndef ZTS
        php_apc_shutdown_globals(&apc_globals);
#endif
#if HAVE_SIGACTION
        apc_shutdown_signals();
#endif
    }
#ifdef ZTS
    ts_free_id(apc_globals_id);
#endif
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION(apc) */
static PHP_RINIT_FUNCTION(apc)
{
    if(APCG(enabled)) {
        apc_request_init(TSRMLS_C);

#if HAVE_SIGACTION
        apc_set_signals(TSRMLS_C);
#endif
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

/* {{{ proto array apc_filehits()
 *       return array of files that came from the cache.
 */
#ifdef APC_FILEHITS
PHP_FUNCTION(apc_filehits)
{
    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    if (!APCG(initialized)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "No APC info available.  Perhaps APC is not enabled? Check apc.enabled in your ini file");
        RETURN_FALSE;
    }

    RETURN_ZVAL(APCG(filehits), 1, 0);
}
#endif
/* }}} */

/* {{{ proto array apc_cache_info([long cache_id [, bool limited]]) */
PHP_FUNCTION(apc_cache_info)
{
    apc_cache_info_t** info;
    int num_caches = 0;
    int multiple;
    apc_cache_link_t* p;
    zval *list, *cache_value;
    zend_bool limited=0;
    char md5str[33];
    long cache_id = 0;
    int i, j;

    if(!APCG(initialized)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "No APC info available.  Perhaps APC is not enabled? Check apc.enabled in your ini file");
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lb", &cache_id, &limited) == FAILURE) {
        return;
    }

    if (cache_id & APC_CACHE_MASK) {
        multiple = 0;
        num_caches=1;
        info = apc_php_malloc(sizeof(apc_cache_info_t*) * num_caches);
        APCG(current_cache) = apc_get_cache(cache_id, 0 TSRMLS_CC);
        info[0] = apc_cache_info(APCG(current_cache), limited TSRMLS_CC);
        APCG(current_cache) = NULL;
    } else {
        multiple = 1;
        if (!cache_id || cache_id & APC_CACHE_FILE) {
            num_caches += APCG(num_file_caches);
        }
        if (!cache_id || cache_id & APC_CACHE_USER) {
            num_caches += APCG(num_user_caches);
        }
        info = apc_php_malloc(sizeof(apc_cache_info_t*) * num_caches);
        j=0;
        if (!cache_id || cache_id & APC_CACHE_FILE) {
            for(i=0; i < APCG(num_file_caches); i++) {
                APCG(current_cache) = &(APCG(file_caches)[i]);
                info[j] = apc_cache_info(&(APCG(file_caches)[i]), limited TSRMLS_CC);
                APCG(current_cache) = NULL;
                j++;
            }
        }
        if (!cache_id || cache_id & APC_CACHE_USER) {
            for(i=0; i < APCG(num_user_caches); i++) {
                APCG(current_cache) = &(APCG(user_caches)[i]);
                info[j] = apc_cache_info(&(APCG(user_caches)[i]), limited TSRMLS_CC);
                APCG(current_cache) = NULL;
                j++;
            }
        }
    }

    if (multiple) {
        array_init(return_value);
    }

    for (i=0; i < num_caches; i++) {
        MAKE_STD_ZVAL(cache_value);
        array_init(cache_value);
        add_assoc_long(cache_value, "id", info[i]->id);
        add_assoc_string(cache_value, "name", info[i]->name, 1);
        add_assoc_string(cache_value, "const_name", info[i]->const_name, 1);
        add_assoc_long(cache_value, "stat", info[i]->fpstat);
        add_assoc_long(cache_value, "stat_ctime", info[i]->stat_ctime);
        add_assoc_long(cache_value, "size_hint", info[i]->size_hint);

        add_assoc_long(cache_value, "cache_by_default", info[i]->cache_by_default);
        add_assoc_long(cache_value, "file_update_protection", info[i]->file_update_protection);
        add_assoc_long(cache_value, "max_file_size", info[i]->max_file_size);

        add_assoc_long(cache_value, "write_lock", info[i]->write_lock);
        add_assoc_long(cache_value, "num_slots", info[i]->num_slots);
        add_assoc_long(cache_value, "ttl", info[i]->ttl);
        add_assoc_double(cache_value, "num_hits", (double)info[i]->num_hits);
        add_assoc_double(cache_value, "num_misses", (double)info[i]->num_misses);
        add_assoc_long(cache_value, "start_time", info[i]->start_time);
        add_assoc_double(cache_value, "expunges", (double)info[i]->expunges);
        add_assoc_double(cache_value, "mem_size", (double)info[i]->mem_size);
        add_assoc_long(cache_value, "num_entries", info[i]->num_entries);
        add_assoc_double(cache_value, "num_inserts", (double)info[i]->num_inserts);
        add_assoc_long(cache_value, "file_upload_progress", info[i]->file_upload_progress);
        if (info[i]->expunge_method == APC_CACHE_EXPUNGE_FLUSH) {
            add_assoc_string(cache_value, "expunge_method", "Flush", 1);
#ifdef APC_LFU
        } else if (info[i]->expunge_method == APC_CACHE_EXPUNGE_LFU) {
            add_assoc_string(cache_value, "expunge_method", "LFU", 1);
#endif
        } else if (info[i]->expunge_method == APC_CACHE_EXPUNGE_NONE) {
            add_assoc_string(cache_value, "expunge_method", "NONE", 1);
        }

        add_assoc_long(cache_value, "segment_idx", info[i]->segment_idx);
        add_assoc_long(cache_value, "gc_ttl", info[i]->gc_ttl);
        add_assoc_zval(cache_value, "hit_stats", apc_stats_zval(&info[i]->hit_stats));
        add_assoc_zval(cache_value, "miss_stats", apc_stats_zval(&info[i]->miss_stats));
        add_assoc_zval(cache_value, "insert_stats", apc_stats_zval(&info[i]->insert_stats));

        if(!limited) {

            ALLOC_INIT_ZVAL(list);
            array_init(list);

            for (p = info[i]->list; p != NULL; p = p->next) {
                zval* link;
                char md5str[33];

                ALLOC_INIT_ZVAL(link);
                array_init(link);

                if(p->type == APC_CACHE_ENTRY_FILE) {
                    add_assoc_string(link, "filename", p->data.file.filename, 1);
                    add_assoc_long(link, "device", p->data.file.device);
                    add_assoc_long(link, "inode", p->data.file.inode);
                    add_assoc_string(link, "type", "file", 1);
                    if(p->data.file.md5) {
                        make_digest_ex(md5str, p->data.file.md5, 16);
                        add_assoc_string(link, "md5", md5str, 1);
                    }
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
            add_assoc_zval(cache_value, "cache_list", list);

            ALLOC_INIT_ZVAL(list);
            array_init(list);

            for (p = info[i]->deleted_list; p != NULL; p = p->next) {
                zval* link;

                ALLOC_INIT_ZVAL(link);
                array_init(link);

                if(p->type == APC_CACHE_ENTRY_FILE) {
                    add_assoc_string(link, "filename", p->data.file.filename, 1);
                    add_assoc_long(link, "device", p->data.file.device);
                    add_assoc_long(link, "inode", p->data.file.inode);
                    add_assoc_string(link, "type", "file", 1);
                    if(p->data.file.md5) {
                        make_digest_ex(md5str, p->data.file.md5, 16);
                        add_assoc_string(link, "md5", md5str, 1);
                    }
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
            add_assoc_zval(cache_value, "deleted_list", list);

#ifdef APC_LFU
            if (info[i]->expunge_method == APC_CACHE_EXPUNGE_LFU) {
                add_assoc_string(cache_value, "expunge_method", "lfu", 1);

                /* Add entries for LFU list */
                ALLOC_INIT_ZVAL(list);
                array_init(list);

                for (p = info[i]->lfu_list; p != NULL; p = p->next) {
                    zval* link;

                    ALLOC_INIT_ZVAL(link);
                    array_init(link);

                    if(p->type == APC_CACHE_ENTRY_FILE) {
                        add_assoc_string(link, "filename", p->data.file.filename, 1);
                        add_assoc_long(link, "device", p->data.file.device);
                        add_assoc_long(link, "inode", p->data.file.inode);
                        add_assoc_string(link, "type", "file", 1);
                        if(p->data.file.md5) {
                            make_digest_ex(md5str, p->data.file.md5, 16);
                            add_assoc_string(link, "md5", md5str, 1);
                        }
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
                add_assoc_zval(cache_value, "lfu_list", list);
            }
#endif

        } /* !limited */

        if (multiple) {
            add_index_zval(return_value, info[i]->id, cache_value);
            apc_cache_free_info(info[i]);
        } else {
            *return_value = *cache_value;
        }
    }

    apc_php_free(info);

}
/* }}} */

/* {{{ proto void apc_clear_cache([long cache_id]) */
PHP_FUNCTION(apc_clear_cache)
{
    long cache_id = APC_CACHE_FILE | APC_CACHE_USER;
    int i;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &cache_id) == FAILURE) {
        return;
    }

    if (cache_id & APC_CACHE_MASK) {
        APCG(current_cache) = apc_get_cache(cache_id, 0 TSRMLS_CC);
        apc_cache_clear(APCG(current_cache));
        APCG(current_cache) = NULL;
    } else {
        if (cache_id & APC_CACHE_FILE) {
            for(i=0; i < APCG(num_file_caches); i++) {
                APCG(current_cache) = &(APCG(file_caches)[i]);
                apc_cache_clear(APCG(current_cache));
                APCG(current_cache) = NULL;
            }
        }
        if (cache_id & APC_CACHE_USER) {
            for(i=0; i < APCG(num_user_caches); i++) {
                APCG(current_cache) = &(APCG(user_caches)[i]);
                apc_cache_clear(APCG(current_cache));
                APCG(current_cache) = NULL;
            }
        }
    }
    RETURN_TRUE;
}
/* }}} */

/* {{{ proto array apc_sma_info([bool limited]) */
PHP_FUNCTION(apc_sma_info)
{
    apc_sma_info_t* info;
    zval* seg_lists;
    int i, j;
    zend_bool limited = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &limited) == FAILURE) {
        return;
    }

    info = apc_sma_info(limited TSRMLS_CC);

    if(!info) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "No APC SMA info available.  Perhaps APC is disabled via apc.enabled?");
        RETURN_FALSE;
    }

    array_init(return_value);
    add_assoc_long(return_value, "num_seg", info->num_seg);
    add_assoc_stringl(return_value, "memory_type", "mmap", sizeof("mmap")-1, 1);
#if APC_SEM_LOCKS
    add_assoc_stringl(return_value, "locking_type", "IPC semaphore", sizeof("IPC semaphore")-1, 1);
#elif APC_FUTEX_LOCKS
    add_assoc_stringl(return_value, "locking_type", "Linux Futex", sizeof("Linux Futex")-1, 1);
#elif APC_SPIN_LOCKS
    add_assoc_stringl(return_value, "locking_type", "spin", sizeof("spin")-1, 1);
#else
    add_assoc_stringl(return_value, "locking_type", "file", sizeof("file")-1, 1);
#endif

    ALLOC_INIT_ZVAL(seg_lists);
    array_init(seg_lists);

    for (i = 0; i < info->num_seg; i++) {
        apc_sma_link_t* p;
        zval *list, *seginfo;

        ALLOC_INIT_ZVAL(seginfo);
        array_init(seginfo);

        add_assoc_double(seginfo, "size", (double)info->seginfo[i].size);
        add_assoc_double(seginfo, "avail", (double)info->seginfo[i].avail);
        add_assoc_bool(seginfo, "unmap", info->seginfo[i].unmap);

        ALLOC_INIT_ZVAL(list);
        array_init(list);
        for (j=0; j < 256; j++) {
            add_next_index_long(list, info->seginfo[i].fragmap[j]);
        }
        add_assoc_zval(seginfo, "fragmap", list);
        add_assoc_long(seginfo, "num_frags", info->seginfo[i].num_frags);
        ALLOC_INIT_ZVAL(list);
        array_init(list);
        for (j=0; j < 256; j++) {
            add_next_index_long(list, info->seginfo[i].freemap[j][0] / info->seginfo[i].freemap[j][1]);
        }
        add_assoc_zval(seginfo, "freemap", list);
        ALLOC_INIT_ZVAL(list);
        array_init(list);
        for (j=0; j < 256; j++) {
            add_next_index_long(list, info->seginfo[i].allocmap[j][0] / info->seginfo[i].allocmap[j][1]);
        }
        add_assoc_zval(seginfo, "allocmap", list);


        ALLOC_INIT_ZVAL(list);
        array_init(list);

        for (p = info->seginfo[i].list; p != NULL; p = p->next) {
            zval* link;

            ALLOC_INIT_ZVAL(link);
            array_init(link);

            add_assoc_long(link, "size", p->size);
            add_assoc_long(link, "offset", p->offset);
            add_next_index_zval(list, link);
        }
        add_assoc_zval(seginfo, "block_list", list);

#if ALLOC_DISTRIBUTION
        {
            int j;
            ALLOC_INIT_ZVAL(list);
            array_init(list);
            for(j=0; j<30; j++) {
                add_next_index_long(list, info->seginfo[i].adist[j]);
            }
            add_assoc_zval(seginfo, "adist", list);
        }
#endif

        add_next_index_zval(seg_lists, seginfo);
    }
    add_assoc_zval(return_value, "segments", seg_lists);
    apc_sma_free_info(info);
}
/* }}} */

/* {{{ */
int _apc_update(apc_cache_t *cache, char *strkey, int strkey_len, apc_cache_updater_t updater, void* data TSRMLS_DC)
{
	if(!APCG(enabled)) {
		return 0;
	}

    HANDLE_BLOCK_INTERRUPTIONS();
    APCG(current_cache) = cache;
    
    if (!_apc_cache_user_update(cache, strkey, strkey_len+1, updater, data TSRMLS_CC)) {
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return 0;
    }

    APCG(current_cache) = NULL;
    HANDLE_UNBLOCK_INTERRUPTIONS();

    return 1;
}
/* }}} */
    
/* {{{ _apc_store */
int _apc_store(apc_cache_t *cache, char *strkey, int strkey_len, const zval *val, const unsigned int ttl, const int exclusive TSRMLS_DC) {
    apc_cache_entry_t *entry;
    apc_cache_key_t key;
    time_t t;
    apc_context_t ctxt={0,};
    int ret = 1;

    t = apc_time();

    if(!APCG(enabled)) return 0;

    HANDLE_BLOCK_INTERRUPTIONS();

    APCG(current_cache) = cache;

    ctxt.pool = apc_pool_create(APC_SMALL_POOL, apc_sma_malloc, apc_sma_free, apc_sma_protect, apc_sma_unprotect);
    if (!ctxt.pool) {
        apc_wprint("Unable to allocate memory for pool.");
        return 0;
    }
    ctxt.copy = APC_COPY_IN_USER;
    ctxt.force_update = 0;

    if(!ctxt.pool) {
        ret = 0;
        goto nocache;
    }

    if (!apc_cache_make_user_key(&key, strkey, strkey_len, t)) {
        goto freepool;
    }

    if (apc_cache_is_last_key(cache, &key, t TSRMLS_CC)) {
	    goto freepool;
    }

    if (!(entry = apc_cache_make_user_entry(strkey, strkey_len, val, &ctxt, ttl TSRMLS_CC))) {
        goto freepool;
    }

    if (!apc_cache_user_insert(cache, key, entry, &ctxt, t, exclusive TSRMLS_CC)) {
freepool:
        apc_pool_destroy(ctxt.pool);
        ret = 0;
    }

nocache:

    APCG(current_cache) = NULL;

    HANDLE_UNBLOCK_INTERRUPTIONS();

    return ret;
}
/* }}} */

/* {{{ apc_store_helper(INTERNAL_FUNCTION_PARAMETERS, const int exclusive)
 */
static void apc_store_helper(INTERNAL_FUNCTION_PARAMETERS, const int exclusive)
{
    zval *key = NULL;
    zval *val = NULL;
    long ttl = 0L;
    HashTable *hash;
    HashPosition hpos;
    zval **hentry;
    char *hkey=NULL;
    uint hkey_len;
    ulong hkey_idx;
    long cache_id = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zll", &key, &val, &ttl, &cache_id) == FAILURE) {
        return;
    }

    if (!key) RETURN_FALSE;

    if (Z_TYPE_P(key) == IS_ARRAY) {
        hash = Z_ARRVAL_P(key);
        array_init(return_value);
        zend_hash_internal_pointer_reset_ex(hash, &hpos);
        while(zend_hash_get_current_data_ex(hash, (void**)&hentry, &hpos) == SUCCESS) {
            zend_hash_get_current_key_ex(hash, &hkey, &hkey_len, &hkey_idx, 0, &hpos);
            if (hkey) {
                /* hkey_len - 1 for consistency, because it includes '\0', while Z_STRLEN_P() doesn't */
                if(!_apc_store(apc_get_cache(cache_id, APC_CACHE_USER TSRMLS_CC), hkey, hkey_len - 1, *hentry, (unsigned int)ttl, exclusive TSRMLS_CC)) {
                    add_assoc_long_ex(return_value, hkey, hkey_len, -1);  /* -1: insertion error */
                }
                hkey = NULL;
            } else {
                add_index_long(return_value, hkey_idx, -1);  /* -1: insertion error */
            }
            zend_hash_move_forward_ex(hash, &hpos);
        }
        return;
    } else if (Z_TYPE_P(key) == IS_STRING) {
        if (!val) RETURN_FALSE;
        if(_apc_store(apc_get_cache(cache_id, APC_CACHE_USER TSRMLS_CC), Z_STRVAL_P(key), Z_STRLEN_P(key), val, (unsigned int)ttl, exclusive TSRMLS_CC))
            RETURN_TRUE;
    } else {
        apc_wprint("apc_store expects key parameter to be a string or an array of key/value pairs.");
    }

    RETURN_FALSE;
}
/* }}} */

/* {{{ proto int apc_store(string key, mixed var [, long ttl, [int cache_id]])
 */
PHP_FUNCTION(apc_store) {
    apc_store_helper(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto int apc_add(mixed key, mixed var [, long ttl ])
 */
PHP_FUNCTION(apc_add) {
    apc_store_helper(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ inc_updater */

struct _inc_update_args {
    long step;
    long lval;
};

static int inc_updater(apc_cache_t* cache, apc_cache_entry_t* entry, void* data) {

    struct _inc_update_args *args = (struct _inc_update_args*) data;
    
    zval* val = entry->data.user.val;

    if(Z_TYPE_P(val) == IS_LONG) {
        Z_LVAL_P(val) += args->step;
        args->lval = Z_LVAL_P(val);
        return 1;
    }

    return 0;
}
/* }}} */

/* {{{ proto long apc_inc(string key [, long step [, bool& success [, long cache_id]]])
 */
PHP_FUNCTION(apc_inc) {
    char *strkey;
    int strkey_len;
    struct _inc_update_args args = {1L, -1};
    zval *success = NULL;
    long cache_id = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lzl", &strkey, &strkey_len, &(args.step), &success, &cache_id) == FAILURE) {
        return;
    }

    if(_apc_update(apc_get_cache(cache_id, APC_CACHE_USER TSRMLS_CC), strkey, strkey_len, inc_updater, &args TSRMLS_CC)) {
        if(success) ZVAL_TRUE(success);
        RETURN_LONG(args.lval);
    }
    
    if(success) ZVAL_FALSE(success);
    
    RETURN_FALSE;
}
/* }}} */

/* {{{ proto long apc_dec(string key [, long step [, bool &success [, long cache_id]]])
 */
PHP_FUNCTION(apc_dec) {
    char *strkey;
    int strkey_len;
    struct _inc_update_args args = {1L, -1};
    zval *success = NULL;
    long cache_id = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lzl", &strkey, &strkey_len, &(args.step), &success, &cache_id) == FAILURE) {
        return;
    }

    args.step = args.step * -1;

    if(_apc_update(apc_get_cache(cache_id, APC_CACHE_USER TSRMLS_CC), strkey, strkey_len, inc_updater, &args TSRMLS_CC)) {
        if(success) ZVAL_TRUE(success);
        RETURN_LONG(args.lval);
    }
    
    if(success) ZVAL_FALSE(success);
    
    RETURN_FALSE;
}
/* }}} */

/* {{{ cas_updater */
static int cas_updater(apc_cache_t* cache, apc_cache_entry_t* entry, void* data) {
    long* vals = ((long*)data);
    long old = vals[0];
    long new = vals[1];
    zval* val = entry->data.user.val;

    if(Z_TYPE_P(val) == IS_LONG) {
        if(Z_LVAL_P(val) == old) {
            Z_LVAL_P(val) = new;
            return 1;
        }
    }

    return 0;
}
/* }}} */

/* {{{ proto int apc_cas(string key, int old, int new [, cache_id])
 */
PHP_FUNCTION(apc_cas) {
    char *strkey;
    int strkey_len;
    long vals[2];
    long cache_id = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll|l", &strkey, &strkey_len, &vals[0], &vals[1], &cache_id) == FAILURE) {
        return;
    }

    if(_apc_update(apc_get_cache(cache_id, APC_CACHE_USER TSRMLS_CC), strkey, strkey_len, cas_updater, &vals TSRMLS_CC)) RETURN_TRUE;
    RETURN_FALSE;
}
/* }}} */

void *apc_erealloc_wrapper(void *ptr, size_t size) {
    return _erealloc(ptr, size, 0 ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC);
}

/* {{{ proto mixed apc_fetch(mixed key[, bool &success, [int cache_id]])
 */
PHP_FUNCTION(apc_fetch) {
    zval *key;
    zval *success = NULL;
    long cache_id = 0;
    HashTable *hash;
    HashPosition hpos;
    zval **hentry;
    zval *result;
    zval *result_entry;
    char *strkey;
    int strkey_len;
    apc_cache_entry_t* entry;
    apc_cache_t *cache;
    time_t t;
    apc_context_t ctxt = {0,};

    if(!APCG(enabled)) RETURN_FALSE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zl", &key, &success, &cache_id) == FAILURE) {
        return;
    }

    if (success) {
        ZVAL_BOOL(success, 0);
    }

    ctxt.pool = apc_pool_create(APC_UNPOOL, apc_php_malloc, apc_php_free, NULL, NULL);
    if (!ctxt.pool) {
        apc_wprint("Unable to allocate memory for pool.");
        RETURN_FALSE;
    }
    ctxt.copy = APC_COPY_OUT_USER;
    ctxt.force_update = 0;

    t = apc_time();

    cache=apc_get_cache(cache_id, APC_CACHE_USER TSRMLS_CC);
    APCG(current_cache) = apc_get_cache(cache_id, APC_CACHE_USER TSRMLS_CC);

    if(Z_TYPE_P(key) != IS_STRING && Z_TYPE_P(key) != IS_ARRAY) {
        convert_to_string(key);
    }

    if(Z_TYPE_P(key) == IS_STRING) {
        strkey = Z_STRVAL_P(key);
        strkey_len = Z_STRLEN_P(key);
        if(!strkey_len) RETURN_FALSE;
        entry = apc_cache_user_find(cache, strkey, strkey_len + 1, t TSRMLS_CC);
        if(entry) {
            /* deep-copy returned shm zval to emalloc'ed return_value */
            apc_cache_fetch_zval(return_value, entry->data.user.val, &ctxt TSRMLS_CC);
            apc_cache_release(cache, entry);
        } else {
            goto freepool;
        }
    } else if(Z_TYPE_P(key) == IS_ARRAY) {
        hash = Z_ARRVAL_P(key);
        MAKE_STD_ZVAL(result);
        array_init(result); 
        zend_hash_internal_pointer_reset_ex(hash, &hpos);
        while(zend_hash_get_current_data_ex(hash, (void**)&hentry, &hpos) == SUCCESS) {
            if(Z_TYPE_PP(hentry) != IS_STRING) {
                apc_wprint("apc_fetch() expects a string or array of strings.");
                goto freepool;
            }
            entry = apc_cache_user_find(cache, Z_STRVAL_PP(hentry), Z_STRLEN_PP(hentry) + 1, t TSRMLS_CC);
            if(entry) {
                /* deep-copy returned shm zval to emalloc'ed return_value */
                MAKE_STD_ZVAL(result_entry);
                apc_cache_fetch_zval(result_entry, entry->data.user.val, &ctxt TSRMLS_CC);
                apc_cache_release(cache, entry);
                zend_hash_add(Z_ARRVAL_P(result), Z_STRVAL_PP(hentry), Z_STRLEN_PP(hentry) +1, &result_entry, sizeof(zval*), NULL);
            } /* don't set values we didn't find */
            zend_hash_move_forward_ex(hash, &hpos);
        }
        APCG(current_cache) = NULL;
        RETVAL_ZVAL(result, 0, 1);
    } else {
        APCG(current_cache) = NULL;
        apc_wprint("apc_fetch() expects a string or array of strings.");
freepool:
        apc_pool_destroy(ctxt.pool);
        RETURN_FALSE;
    }

    if (success) {
        ZVAL_BOOL(success, 1);
    }

    apc_pool_destroy(ctxt.pool);
    return;
}
/* }}} */


/* {{{ proto mixed apc_delete(string key, [int cache_id])
 */
PHP_FUNCTION(apc_delete) {
    zval *keys;
    long cache_id = 0;

    if(!APCG(enabled)) RETURN_FALSE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|l", &keys, &cache_id) == FAILURE) {
        return;
    }

    APCG(current_cache) = apc_get_cache(cache_id, APC_CACHE_USER TSRMLS_CC);
    if (Z_TYPE_P(keys) == IS_STRING) {
        if (!Z_STRLEN_P(keys)) RETURN_FALSE;
        if(apc_cache_user_delete(APCG(current_cache), Z_STRVAL_P(keys), Z_STRLEN_P(keys) + 1)) {
            RETURN_TRUE;
        } else {
            RETURN_FALSE;
        }
    } else if (Z_TYPE_P(keys) == IS_ARRAY) {
        HashTable *hash = Z_ARRVAL_P(keys);
        HashPosition hpos;
        zval **hentry;
        array_init(return_value);
        zend_hash_internal_pointer_reset_ex(hash, &hpos);
        while(zend_hash_get_current_data_ex(hash, (void**)&hentry, &hpos) == SUCCESS) {
            if(Z_TYPE_PP(hentry) != IS_STRING) {
                apc_wprint("apc_delete() expects a string, array of strings, or APCIterator instance.");
                add_next_index_zval(return_value, *hentry);
                Z_ADDREF_PP(hentry);
            } else if(apc_cache_user_delete(APCG(current_cache), Z_STRVAL_PP(hentry), Z_STRLEN_PP(hentry) + 1) != 1) {
                add_next_index_zval(return_value, *hentry);
                Z_ADDREF_PP(hentry);
            }
            zend_hash_move_forward_ex(hash, &hpos);
        }
        return;
    } else if (Z_TYPE_P(keys) == IS_OBJECT) {
        if (apc_iterator_delete(keys TSRMLS_CC)) {
            RETURN_TRUE;
        } else {
            RETURN_FALSE;
        }
    } else {
        apc_wprint("apc_delete() expects a string, array of strings, or APCIterator instance.");
    }
}
/* }}} */

/* {{{ proto mixed apc_delete_file(mixed keys [, int cache_id])
 *       Deletes the given files from the opcode cache.  
 *       Accepts a string, array of strings, or APCIterator object. 
 *       Returns True/False, or for an Array an Array of failed files.
 */
PHP_FUNCTION(apc_delete_file) {
    zval *keys;
    long cache_id = 0;

    if(!APCG(enabled)) RETURN_FALSE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|l", &keys, &cache_id) == FAILURE) {
        return;
    }

    APCG(current_cache) = apc_get_cache(cache_id, APC_CACHE_FILE TSRMLS_CC);
    if (Z_TYPE_P(keys) == IS_STRING) {
        if (!Z_STRLEN_P(keys)) RETURN_FALSE;
        if(apc_cache_delete(APCG(current_cache), Z_STRVAL_P(keys), Z_STRLEN_P(keys) + 1 TSRMLS_CC) != 1) {
            RETURN_FALSE;
        } else {
            RETURN_TRUE;
        }
    } else if (Z_TYPE_P(keys) == IS_ARRAY) {
        HashTable *hash = Z_ARRVAL_P(keys);
        HashPosition hpos;
        zval **hentry;
        array_init(return_value);
        zend_hash_internal_pointer_reset_ex(hash, &hpos);
        while(zend_hash_get_current_data_ex(hash, (void**)&hentry, &hpos) == SUCCESS) {
            if(Z_TYPE_PP(hentry) != IS_STRING) {
                apc_wprint("apc_delete_file() expects a string, array of strings, or APCIterator instance.");
                add_next_index_zval(return_value, *hentry);
                Z_ADDREF_PP(hentry);
            } else if(apc_cache_delete(APCG(current_cache), Z_STRVAL_PP(hentry), Z_STRLEN_PP(hentry) + 1 TSRMLS_CC) != 1) {
                add_next_index_zval(return_value, *hentry);
                Z_ADDREF_PP(hentry);
            }
            zend_hash_move_forward_ex(hash, &hpos);
        }
        return;
    } else if (Z_TYPE_P(keys) == IS_OBJECT) {
        if (apc_iterator_delete(keys TSRMLS_CC)) {
            RETURN_TRUE;
        } else {
            RETURN_FALSE;
        }
    } else {
        apc_wprint("apc_delete_file() expects a string, array of strings, or APCIterator instance.");
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
        c.module_number = PHP_USER_CONSTANT;
        zend_register_constant(&c TSRMLS_CC);

        zend_hash_move_forward_ex(Z_ARRVAL_P(constants), &pos);
    }
}

/* {{{ proto mixed apc_define_constants(string key, array constants [, bool case-sensitive, [int cache_id]])
 */
PHP_FUNCTION(apc_define_constants) {
    char *strkey;
    int strkey_len;
    zval *constants = NULL;
    zend_bool case_sensitive = 1;
    long cache_id = 0;
    int argc = ZEND_NUM_ARGS();

    if (zend_parse_parameters(argc TSRMLS_CC, "sa|bl", &strkey, &strkey_len, &constants, &case_sensitive, &cache_id) == FAILURE) {
        return;
    }

    if(!strkey_len) RETURN_FALSE;

    RETVAL_FALSE;
    APCG(current_cache) = apc_get_cache(cache_id, APC_CACHE_USER TSRMLS_CC);
    _apc_define_constants(constants, case_sensitive TSRMLS_CC);
    if(_apc_store(APCG(current_cache), strkey, strkey_len, constants, 0, 0 TSRMLS_CC)) RETVAL_TRUE;
    APCG(current_cache) = NULL;
    return;
} /* }}} */

/* {{{ proto mixed apc_load_constants(string key [, bool case-sensitive, [int cache_id]])
 */
PHP_FUNCTION(apc_load_constants) {
    char *strkey;
    int strkey_len;
    long cache_id = 0;
    apc_cache_entry_t* entry;
    apc_cache_t *cache;
    time_t t;
    zend_bool case_sensitive = 1;

    if(!APCG(enabled)) RETURN_FALSE;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|bl", &strkey, &strkey_len, &case_sensitive, &cache_id) == FAILURE) {
        return;
    }

    if(!strkey_len) RETURN_FALSE;

    t = apc_time();

    cache=apc_get_cache(cache_id, APC_CACHE_USER TSRMLS_CC);
    APCG(current_cache) = cache;

    entry = apc_cache_user_find(cache, strkey, strkey_len + 1, t TSRMLS_CC);

    if(entry) {
        _apc_define_constants(entry->data.user.val, case_sensitive TSRMLS_CC);
        apc_cache_release(cache, entry);
        APCG(current_cache) = NULL;
        RETURN_TRUE;
    } else {
        APCG(current_cache) = NULL;
        RETURN_FALSE;
    }
}
/* }}} */

/* {{{ proto mixed apc_compile_file(mixed filenames [, bool atomic, [int cache_id]])
 */
PHP_FUNCTION(apc_compile_file) {
    zval *file;
    long cache_id = 0;
    zend_file_handle file_handle;
    zend_op_array *op_array;
    char** filters = NULL;
    zend_bool cache_by_default = 1;
    HashTable cg_function_table, cg_class_table;
    HashTable *cg_orig_function_table, *cg_orig_class_table, *eg_orig_function_table, *eg_orig_class_table;
    apc_cache_entry_t** cache_entries;
    apc_cache_key_t* keys;
    zend_op_array **op_arrays;
    time_t t;
    zval **hentry;
    HashPosition hpos;
    int i=0, c=0;
    int *rval=NULL;
    int count=0;
    zend_bool atomic=1;
    apc_context_t ctxt = {0,};
    zend_execute_data *orig_current_execute_data;
    int atomic_fail;
    apc_cache_t *cache;

    if(!APCG(enabled)) RETURN_FALSE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|bl", &file, &atomic, &cache_id) == FAILURE) {
        return;
    }

    if (Z_TYPE_P(file) != IS_ARRAY && Z_TYPE_P(file) != IS_STRING) {
        apc_wprint("apc_compile_file argument must be a string or an array of strings");
        RETURN_FALSE;
    }

    cache = apc_get_cache(cache_id, APC_CACHE_FILE TSRMLS_CC);
    APCG(current_cache) = apc_get_cache(cache_id, APC_CACHE_FILE TSRMLS_CC);

    HANDLE_BLOCK_INTERRUPTIONS();

    /* reset filters and cache_by_default */
    filters = cache->filters;
    cache->filters = NULL;
    cache_by_default = cache->cache_by_default;
    cache->cache_by_default = 1;

    /* Replace function/class tables to avoid namespace conflicts */
    zend_hash_init_ex(&cg_function_table, 100, NULL, ZEND_FUNCTION_DTOR, 1, 0);
    cg_orig_function_table = CG(function_table);
    CG(function_table) = &cg_function_table;
    zend_hash_init_ex(&cg_class_table, 10, NULL, ZEND_CLASS_DTOR, 1, 0);
    cg_orig_class_table = CG(class_table);
    CG(class_table) = &cg_class_table;
    eg_orig_function_table = EG(function_table);
    EG(function_table) = CG(function_table);
    eg_orig_class_table = EG(class_table);
    EG(class_table) = CG(class_table);
    APCG(force_file_update) = 1;

    /* Compile the file(s), loading it into the cache */
    if (Z_TYPE_P(file) == IS_STRING) {
        file_handle.type = ZEND_HANDLE_FILENAME;
        file_handle.filename = Z_STRVAL_P(file);
        file_handle.free_filename = 0;
        file_handle.opened_path = NULL;

        orig_current_execute_data = EG(current_execute_data);
        zend_try {
            op_array = zend_compile_file(&file_handle, ZEND_INCLUDE TSRMLS_CC);
        } zend_catch {
            EG(current_execute_data) = orig_current_execute_data;
            EG(in_execution) = 1;
            CG(unclean_shutdown) = 0;
            apc_wprint("Error compiling %s in apc_compile_file.", file_handle.filename);
            op_array = NULL;
        } zend_end_try();
        if(op_array != NULL) {
            /* Free up everything */
            destroy_op_array(op_array TSRMLS_CC);
            efree(op_array);
            RETVAL_TRUE;
        } else {
            RETVAL_FALSE;
        }
        zend_destroy_file_handle(&file_handle TSRMLS_CC);

    } else { /* IS_ARRAY */

        array_init(return_value);

        t = apc_time();

        op_arrays = ecalloc(Z_ARRVAL_P(file)->nNumOfElements, sizeof(zend_op_array*));
        cache_entries = ecalloc(Z_ARRVAL_P(file)->nNumOfElements, sizeof(apc_cache_entry_t*));
        keys = ecalloc(Z_ARRVAL_P(file)->nNumOfElements, sizeof(apc_cache_key_t));
        zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(file), &hpos);
        while(zend_hash_get_current_data_ex(Z_ARRVAL_P(file), (void**)&hentry, &hpos) == SUCCESS) {
            if (Z_TYPE_PP(hentry) != IS_STRING) {
                apc_wprint("apc_compile_file array values must be strings, aborting.");
                break;
            }
            file_handle.type = ZEND_HANDLE_FILENAME;
            file_handle.filename = Z_STRVAL_PP(hentry);
            file_handle.free_filename = 0;
            file_handle.opened_path = NULL;

            if (!apc_cache_make_file_key(cache, &(keys[i]), file_handle.filename, PG(include_path), t TSRMLS_CC)) {
                add_assoc_long(return_value, Z_STRVAL_PP(hentry), -1);  /* -1: compilation error */
                apc_wprint("Error compiling %s in apc_compile_file.", file_handle.filename);
                break;
            }

            if (keys[i].type == APC_CACHE_KEY_FPFILE) {
                keys[i].data.fpfile.fullpath = estrndup(keys[i].data.fpfile.fullpath, keys[i].data.fpfile.fullpath_len);
            } else if (keys[i].type == APC_CACHE_KEY_USER) {
                keys[i].data.user.identifier = estrndup(keys[i].data.user.identifier, keys[i].data.user.identifier_len);
            }

            orig_current_execute_data = EG(current_execute_data);
            zend_try {
                if (apc_compile_cache_entry(cache, &keys[i], &file_handle, ZEND_INCLUDE, t, &op_arrays[i], &cache_entries[i] TSRMLS_CC) != SUCCESS) {
                    op_arrays[i] = NULL;
                    cache_entries[i] = NULL;
                    add_assoc_long(return_value, Z_STRVAL_PP(hentry), -2);  /* -2: input or cache insertion error */
                    apc_wprint("Error compiling %s in apc_compile_file.", file_handle.filename);
                }
            } zend_catch {
                EG(current_execute_data) = orig_current_execute_data;
                EG(in_execution) = 1;
                CG(unclean_shutdown) = 0;
                op_arrays[i] = NULL;
                cache_entries[i] = NULL;
                add_assoc_long(return_value, Z_STRVAL_PP(hentry), -1);  /* -1: compilation error */
                apc_wprint("Error compiling %s in apc_compile_file.", file_handle.filename);
            } zend_end_try();

            zend_destroy_file_handle(&file_handle TSRMLS_CC);
            if(op_arrays[i] != NULL) {
                count++;
            }

            /* clean out the function/class tables */
            zend_hash_clean(&cg_function_table);
            zend_hash_clean(&cg_class_table);

            zend_hash_move_forward_ex(Z_ARRVAL_P(file), &hpos);
            i++;
        }

        /* atomically update the cache if no errors or not atomic */
        ctxt.copy = APC_COPY_IN_OPCODE;
        ctxt.force_update = 1;
        if (count == i || !atomic) {
            rval = apc_cache_insert_mult(cache, keys, cache_entries, &ctxt, t, i TSRMLS_CC);
            atomic_fail = 0;
        } else {
            atomic_fail = 1;
        }

        zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(file), &hpos);
        for(c=0; c < i; c++) {
            zend_hash_get_current_data_ex(Z_ARRVAL_P(file), (void**)&hentry, &hpos);
            if (rval && rval[c] != 1) {
                add_assoc_long(return_value, Z_STRVAL_PP(hentry), -2);  /* -2: input or cache insertion error */
                if (cache_entries[c]) {
                    apc_pool_destroy(cache_entries[c]->pool);
                }
            }
            if (op_arrays[c]) {
                destroy_op_array(op_arrays[c] TSRMLS_CC);
                efree(op_arrays[c]);
            }
            if (atomic_fail && cache_entries[c]) {
                apc_pool_destroy(cache_entries[c]->pool);
            }
            if (keys[c].type == APC_CACHE_KEY_FPFILE) {
                efree((void*)keys[c].data.fpfile.fullpath);
            } else if (keys[c].type == APC_CACHE_KEY_USER) {
                efree((void*)keys[c].data.user.identifier);
            }
            zend_hash_move_forward_ex(Z_ARRVAL_P(file), &hpos);
        }
        efree(op_arrays);
        efree(keys);
        efree(cache_entries);
        if (rval) {
            efree(rval);
        }

    }

    /* Return class/function tables to previous states, destroy temp tables */
    APCG(force_file_update) = 0;
    CG(function_table) = cg_orig_function_table;
    zend_hash_destroy(&cg_function_table);
    CG(class_table) = cg_orig_class_table;
    zend_hash_destroy(&cg_class_table);
    EG(function_table) = eg_orig_function_table;
    EG(class_table) = eg_orig_class_table;

    /* Restore global settings */
    cache->filters = filters;
    cache->cache_by_default = cache_by_default;

    APCG(current_cache) = NULL;
    HANDLE_UNBLOCK_INTERRUPTIONS();

}
/* }}} */

/* {{{ proto mixed apc_bin_dump([int cache_id [, array filter]])
    Returns a binary dump of the given cache_id, allows optional 
    limiting results to those provided in filter, an array of keys/files.
 */
PHP_FUNCTION(apc_bin_dump) {

    zval *zfilter;
    HashTable *hfilter;
    apc_bd_t *bd;
    long cache_id = 0;

    if(!APCG(enabled)) {
        apc_wprint("APC is not enabled, apc_bin_dump not available.");
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|la!", &cache_id, &zfilter) == FAILURE) {
        return;
    }

    hfilter = zfilter ? Z_ARRVAL_P(zfilter) : NULL;
    APCG(current_cache) = apc_get_cache(cache_id, 0 TSRMLS_CC);
    bd = apc_bin_dump(APCG(current_cache), hfilter TSRMLS_CC);
    APCG(current_cache) = NULL;
    if(bd) {
        RETVAL_STRINGL((char*)bd, bd->size-1, 0);
    } else {
        apc_eprint("Unknown error encountered during apc_bin_dump.");
        RETVAL_NULL();
    }

    return;
}

/* {{{ proto mixed apc_bin_dumpfile(int cache_id ,array filter, string filename [, int flags [, resource context]])
    Returns a binary dump to given filename containing the given cache_id, allows 
    limiting results to those provided in filter, an array of keys/files.
 */
PHP_FUNCTION(apc_bin_dumpfile) {

    zval *zfilter;
    HashTable *hfilter;
    long cache_id = 0;
    char *filename;
    int filename_len;
    long flags=0;
    zval *zcontext = NULL;
    php_stream_context *context = NULL;
    php_stream *stream;
    int numbytes = 0;
    apc_bd_t *bd;

    if(!APCG(enabled)) {
        apc_wprint("APC is not enabled, apc_bin_dumpfile not available.");
        RETURN_FALSE;
    }


    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "la!s|lr!", &cache_id, &zfilter, &filename, &filename_len, &flags, &zcontext) == FAILURE) {
        return;
    }

    if(!filename_len) {
        apc_eprint("apc_bin_dumpfile filename argument must be a valid filename.");
        RETURN_FALSE;
    }

    hfilter = zfilter ? Z_ARRVAL_P(zfilter) : NULL;
    APCG(current_cache) = apc_get_cache(cache_id, 0 TSRMLS_CC);
    bd = apc_bin_dump(APCG(current_cache), hfilter TSRMLS_CC);
    APCG(current_cache) = NULL;

    if(!bd) {
        apc_eprint("Unknown error encountered during apc_bin_dumpfile");
        RETURN_FALSE;
    }


    /* Most of the following has been taken from the file_get/put_contents functions */

    context = php_stream_context_from_zval(zcontext, flags & PHP_FILE_NO_DEFAULT_CONTEXT);
    stream = php_stream_open_wrapper_ex(filename, (flags & PHP_FILE_APPEND) ? "ab" : "wb",
                                            ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL, context);
    if (stream == NULL) {
        efree(bd);
        apc_eprint("Unable to write to file in apc_bin_dumpfile.");
        RETURN_FALSE;
    }

    if (flags & LOCK_EX && php_stream_lock(stream, LOCK_EX)) {
        php_stream_close(stream);
        efree(bd);
        apc_eprint("Unable to get a lock on file in apc_bin_dumpfile.");
        RETURN_FALSE;
    }

    numbytes = php_stream_write(stream, (char*)bd, bd->size);
    if(numbytes != bd->size) {
        numbytes = -1;
    }

    php_stream_close(stream);
    efree(bd);

    if(numbytes < 0) {
        apc_wprint("Only %d of %d bytes written, possibly out of free disk space", numbytes, bd->size);
        RETURN_FALSE;
    }

    RETURN_LONG(numbytes);
}

/* {{{ proto mixed apc_bin_load(string data, [int flags [, int cache_id]])
    Load the given binary dump into the APC file/user cache.
 */
PHP_FUNCTION(apc_bin_load) {

    int data_len;
    char *data;
    long flags = 0;
    long cache_id = 0;

    if(!APCG(enabled)) {
        apc_wprint("APC is not enabled, apc_bin_load not available.");
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|ll", &data, &data_len, &flags, &cache_id) == FAILURE) {
        return;
    }

    if(!data_len || data_len != ((apc_bd_t*)data)->size -1) {
        apc_eprint("apc_bin_load string argument does not appear to be a valid APC binary dump due to size (%d vs expected %d).", data_len, ((apc_bd_t*)data)->size -1);
        RETURN_FALSE;
    }

    APCG(current_cache) = apc_get_cache(cache_id, 0 TSRMLS_CC);
    apc_bin_load(APCG(current_cache), (apc_bd_t*)data, (int)flags TSRMLS_CC);
    APCG(current_cache) = NULL;

    RETURN_TRUE;
}

/* {{{ proto mixed apc_bin_loadfile(string filename, [resource context, [int flags [, int cache_id]]])
    Load the given binary dump from the named file into the APC file/user cache.
 */
PHP_FUNCTION(apc_bin_loadfile) {

    char *filename;
    int filename_len;
    zval *zcontext = NULL;
    long flags;
    php_stream_context *context = NULL;
    php_stream *stream;
    char *data;
    int len;
    long cache_id = 0;

    if(!APCG(enabled)) {
        apc_wprint("APC is not enabled, apc_bin_loadfile not available.");
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|r!l", &filename, &filename_len, &zcontext, &flags, &cache_id) == FAILURE) {
        return;
    }

    if(!filename_len) {
        apc_eprint("apc_bin_loadfile filename argument must be a valid filename.");
        RETURN_FALSE;
    }

    context = php_stream_context_from_zval(zcontext, 0);
    stream = php_stream_open_wrapper_ex(filename, "rb",
            ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL, context);
    if (!stream) {
        apc_eprint("Unable to read from file in apc_bin_loadfile.");
        RETURN_FALSE;
    }

    len = php_stream_copy_to_mem(stream, &data, PHP_STREAM_COPY_ALL, 0);
    if(len == 0) {
        apc_wprint("File passed to apc_bin_loadfile was empty: %s.", filename);
        RETURN_FALSE;
    } else if(len < 0) {
        apc_wprint("Error reading file passed to apc_bin_loadfile: %s.", filename);
        RETURN_FALSE;
    } else if(len != ((apc_bd_t*)data)->size) {
        apc_wprint("file passed to apc_bin_loadfile does not appear to be valid due to size (%d vs expected %d).", len, ((apc_bd_t*)data)->size -1);
        RETURN_FALSE;
    }
    php_stream_close(stream);

    APCG(current_cache) = apc_get_cache(cache_id, 0 TSRMLS_CC);
    apc_bin_load(APCG(current_cache), (apc_bd_t*)data, (int)flags TSRMLS_CC);
    APCG(current_cache) = NULL;
    efree(data);

    RETURN_TRUE;
}
/* }}} */

/* {{{ proto boolean apc_default_cache(int cache_id) */
PHP_FUNCTION(apc_default_cache) {
    long cache_id = 0;
    apc_cache_t *cache;

    if(!APCG(enabled)) RETURN_FALSE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &cache_id) == FAILURE) {
        return;
    }

    cache = apc_get_cache(cache_id, 0 TSRMLS_CC);
    if (!cache) {
        RETURN_FALSE;
    }
    APCG(default_user_cache) = cache;

    RETURN_TRUE;
}
/* }}} */


/* {{{ arginfo */
#if (PHP_MAJOR_VERSION >= 6 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3))
# define PHP_APC_ARGINFO
#else
# define PHP_APC_ARGINFO static
#endif

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_store, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, var)
    ZEND_ARG_INFO(0, ttl)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_filehits, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_clear_cache, 0, 0, 0)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_sma_info, 0, 0, 0)
    ZEND_ARG_INFO(0, limited)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_cache_info, 0, 0, 0)
    ZEND_ARG_INFO(0, cache_id)
    ZEND_ARG_INFO(0, limited)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_define_constants, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, constants)
    ZEND_ARG_INFO(0, case_sensitive)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_apc_delete_file, 0)
	ZEND_ARG_INFO(0, keys)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_apc_delete, 0)
	ZEND_ARG_INFO(0, keys)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_fetch, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(1, success)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_inc, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, step)
    ZEND_ARG_INFO(1, success)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_apc_cas, 0)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, old)
    ZEND_ARG_INFO(0, new)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_load_constants, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, case_sensitive)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_compile_file, 0, 0, 1)
    ZEND_ARG_INFO(0, filenames)
    ZEND_ARG_INFO(0, atomic)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_bin_dump, 0, 0, 0)
    ZEND_ARG_INFO(0, cache_id)
    ZEND_ARG_INFO(0, filter)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_bin_dumpfile, 0, 0, 3)
    ZEND_ARG_INFO(0, cache_id)
    ZEND_ARG_INFO(0, filter)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, flags)
    ZEND_ARG_INFO(0, context)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_bin_load, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, flags)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_bin_loadfile, 0, 0, 1)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, context)
    ZEND_ARG_INFO(0, flags)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_default_cache, 0, 0, 1)
    ZEND_ARG_INFO(0, cache_id)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ apc_functions[] */
function_entry apc_functions[] = {
    PHP_FE(apc_cache_info,          arginfo_apc_cache_info)
#ifdef APC_FILEHITS
    PHP_FE(apc_filehits,            arginfo_apc_filehits)
#endif
    PHP_FE(apc_clear_cache,         arginfo_apc_clear_cache)
    PHP_FE(apc_sma_info,            arginfo_apc_sma_info)
    PHP_FE(apc_store,               arginfo_apc_store)
    PHP_FE(apc_fetch,               arginfo_apc_fetch)
    PHP_FE(apc_delete,              arginfo_apc_delete)
    PHP_FE(apc_delete_file,         arginfo_apc_delete_file)
    PHP_FE(apc_define_constants,    arginfo_apc_define_constants)
    PHP_FE(apc_load_constants,      arginfo_apc_load_constants)
    PHP_FE(apc_compile_file,        arginfo_apc_compile_file)
    PHP_FE(apc_add,                 arginfo_apc_store)
    PHP_FE(apc_inc,                 arginfo_apc_inc)
    PHP_FE(apc_dec,                 arginfo_apc_inc)
    PHP_FE(apc_cas,                 arginfo_apc_cas)
    PHP_FE(apc_bin_dump,            arginfo_apc_bin_dump)
    PHP_FE(apc_bin_load,            arginfo_apc_bin_load)
    PHP_FE(apc_bin_dumpfile,        arginfo_apc_bin_dumpfile)
    PHP_FE(apc_bin_loadfile,        arginfo_apc_bin_loadfile)
    PHP_FE(apc_default_cache,       arginfo_apc_default_cache)
    {NULL, NULL, NULL}
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
    PHP_APC_VERSION,
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
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
