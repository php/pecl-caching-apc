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

#ifndef APC_SMA_H
#define APC_SMA_H

#include "apc.h"

/* Simple shared memory allocator */

#if APC_MMAP
extern void apc_sma_init(int numseg, int segsize, char *mmap_file_mask);
#else
extern void apc_sma_init(int numseg, int segsize);
#endif
extern void apc_sma_cleanup();
extern void* apc_sma_malloc(size_t size);
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
