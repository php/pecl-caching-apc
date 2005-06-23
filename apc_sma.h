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
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$ */

#ifndef APC_SMA_H
#define APC_SMA_H

#include "apc.h"

/* Simple shared memory allocator */

extern void apc_sma_init(int numseg, int segsize, char *mmap_file_mask);
extern void apc_sma_cleanup();
extern void* apc_sma_malloc(size_t size);
extern void* apc_sma_realloc(void* p, size_t size);
extern char* apc_sma_strdup(const char *s);
extern void apc_sma_free(void* p);

/* {{{ struct definition: apc_sma_link_t */
typedef struct apc_sma_link_t apc_sma_link_t;
struct apc_sma_link_t {
    int size;               /* size of this free block */
    int offset;             /* offset in segment of this block */
    apc_sma_link_t* next;   /* link to next free block */
};
/* }}} */

/* {{{ struct definition: apc_sma_info_t */
typedef struct apc_sma_info_t apc_sma_info_t;
struct apc_sma_info_t {
    int num_seg;            /* number of shared memory segments */
    int seg_size;           /* size of each shared memory segment */
    apc_sma_link_t** list;  /* there is one list per segment */
};
/* }}} */

extern apc_sma_info_t* apc_sma_info();
extern void apc_sma_free_info(apc_sma_info_t* info);

extern int apc_sma_get_avail_mem();
extern void apc_sma_check_integrity();

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
