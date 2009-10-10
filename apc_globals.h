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
  |          George Schlossnagle <george@omniti.com>                     |
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

#ifndef APC_GLOBALS_H
#define APC_GLOBALS_H

#include "apc_cache.h"
#include "apc_sma.h"
#include "apc_php.h"

/* {{{ struct apc_rfc1867_data */

typedef struct _apc_rfc1867_data apc_rfc1867_data;

struct _apc_rfc1867_data {
    char tracking_key[64];
    int  key_length;
    size_t content_length;
    char filename[128];
    char name[64];
    char *temp_filename;
    int cancel_upload;
    double start_time;
    size_t bytes_processed;
    size_t prev_bytes_processed;
    int update_freq;
    double rate;
};
/* }}} */

ZEND_BEGIN_MODULE_GLOBALS(apc)
    /* configuration parameters */
    zend_bool enabled;      /* if true, apc is enabled (defaults to true) */
    char *mmap_file_mask;   /* mktemp-style file-mask to pass to mmap */

    /* module variables */
    zend_bool initialized;       /* true if module was initialized */
    zend_bool enable_cli;        /* Flag to override turning APC off for CLI */
    zend_bool canonicalize;      /* true if relative paths should be canonicalized in no-stat mode */
    zend_bool slam_defense;      /* true for user cache slam defense */
    long slam_rand;              /* A place to store the slam rand value for the request */
    zend_bool report_autofilter; /* true for auto-filter warnings */
    zend_bool include_once;      /* Override the ZEND_INCLUDE_OR_EVAL opcode handler to avoid pointless fopen()s [still experimental] */
    apc_optimize_function_t apc_optimize_function;   /* optimizer function callback */
#ifdef MULTIPART_EVENT_FORMDATA
    zend_bool rfc1867;            /* Flag to enable rfc1867 handler */
    apc_cache_t *rfc1867_cache;   /* pointer to apc cache where rfc1867 updates should be stored */
    char* rfc1867_prefix;         /* Key prefix */
    char* rfc1867_name;           /* Name of hidden field to activate upload progress/key suffix */
    double rfc1867_freq;          /* Update frequency as percentage or bytes */
    long rfc1867_ttl;             /* TTL for rfc1867 entries */
    apc_rfc1867_data rfc1867_data;/* Per-request data */
#endif
    HashTable copied_zvals;      /* my_copy recursion detection list */
    zend_bool force_file_update; /* force files to be updated during apc_compile_file */
    char canon_path[MAXPATHLEN]; /* canonical path for key data */
#if APC_FILEHITS
    zval *filehits;              /* Files that came from the cache for this request */
#endif
    zend_bool coredump_unmap;    /* Trap signals that coredump and unmap shared memory */
    apc_cache_t *current_cache;  /* current cache being modified/read */
    char *preload_path;
    zend_bool file_md5;          /* record md5 hash of files */
    void *apc_bd_alloc_ptr;      /* bindump alloc() ptr */
    void *apc_bd_alloc_ubptr;    /* bindump alloc() upper bound ptr */
    HashTable apc_bd_alloc_list; /* bindump alloc() ptr list */
    zend_bool use_request_time;  /* use the SAPI request start time for TTL */
    HashTable *lazy_function_table;  /* lazy function entry table */
    HashTable *lazy_class_table;     /* number of 'fake' classes added for apc_compile_file */
    int num_file_caches;         /* Number of user caches in the caches array */
    apc_cache_t *file_caches;    /* Array of user caches, NULL terminated */
    int num_user_caches;         /* Number of file caches in the caches array */
    apc_cache_t *user_caches;    /* Array of file caches, NULL terminated */
    apc_cache_t *default_user_cache;       /* Default user cache */
    apc_cache_t *default_file_cache;       /* Default file cache*/
    apc_segment_t *sma_segments_head;  /* linked list of segments for easy info/cleanup */
ZEND_END_MODULE_GLOBALS(apc)

/* (the following declaration is defined in php_apc.c) */
ZEND_EXTERN_MODULE_GLOBALS(apc)

#ifdef ZTS
# define APCG(v) TSRMG(apc_globals_id, zend_apc_globals *, v)
#else
# define APCG(v) (apc_globals.v)
#endif

/* True globals */
extern void* apc_compiled_filters;   /* compiled filters */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
