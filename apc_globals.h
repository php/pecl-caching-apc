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

#ifndef APC_GLOBALS_H
#define APC_GLOBALS_H

#include "apc_cache.h"
#include "apc_stack.h"
#include "apc_php.h"

ZEND_BEGIN_MODULE_GLOBALS(apc)
    /* configuration parameters */
    int enabled;            /* if true, apc is enabled (defaults to true) */
    int shm_segments;       /* number of shared memory segments to use */
    int shm_size;           /* size of each shared memory segment (in MB) */
    int optimization;       /* optimizer level (higher = more aggressive) */
    int num_files_hint;     /* parameter to apc_cache_create */
    int gc_ttl;             /* parameter to apc_cache_create */
#if APC_MMAP
    char *mmap_file_mask;   /* mktemp-style file-mask to pass to mmap */
#endif
    char** filters;         /* array of regex filters that prevent caching */

    /* module variables */
    int initialized;        /* true if module was initialized */
    apc_cache_t* cache;     /* the global compiler cache */
    apc_stack_t* cache_stack; /* the stack of cached executable code */
    void* compiled_filters; /* compiled filters */
    int cache_by_default;   /* true if files should be cached unless filtered out */
                            /* false if files should only be cached if filtered in */
ZEND_END_MODULE_GLOBALS(apc)

/* (the following declaration is defined in php_apc.c) */
PHPAPI ZEND_EXTERN_MODULE_GLOBALS(apc)

#ifdef ZTS
# define APCG(v) TSRMG(apc_globals_id, zend_apc_globals *, v)
#else
# define APCG(v) (apc_globals.v)
#endif

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
