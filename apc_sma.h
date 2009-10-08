/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2008 The PHP Group                                |
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

#define ALLOC_DISTRIBUTION 0

#include "apc.h"

/* Simple shared memory allocator */

/* {{{ struct definition: apc_sma_shm_segment */
typedef struct apc_segment_t apc_segment_t;
struct apc_segment_t {
	int initialized;    /* true if the sma has been initialized */
    int unmap;          /* true if we should unmap segment on a segfault */
	size_t size;        /* size of shm segment */
	void* shmaddr;      /* shm segment addresses */
#ifdef APC_MEMPROTECT
    void* roaddr;
#endif
    struct apc_segment_t *next;  /* next segment */
};

extern void* apc_sma_protect(void *p);
extern void* apc_sma_unprotect(void *p);

/* {{{ struct definition: apc_sma_link_t */
typedef struct apc_sma_link_t apc_sma_link_t;
struct apc_sma_link_t {
    long size;               /* size of this free block */
    long offset;             /* offset in segment of this block */
    apc_sma_link_t* next;   /* link to next free block */
};
/* }}} */

/* {{{ struct definition: apc_sma_seginfo_t */
typedef struct apc_sma_seginfo_t apc_sma_seginfo_t;
struct apc_sma_seginfo_t {
    int num_seg;            /* number of shared memory segments */
    size_t size;            /* size of each shared memory segment */
    size_t avail;           /* avail mem of each shared memory segment */
    int unmap;              /* unmap the segment on segfault signal ? */
    apc_sma_link_t* list;   /* list of blocks for this segment */
    size_t fragmap[256];    /* fragmentation stat map */
    size_t num_frags;
    size_t freemap[256][2];    /* fragmentation stat map */
    size_t allocmap[256][2];    /* fragmentation stat map */
#if ALLOC_DISTRIBUTION
    size_t adist[30];
#endif
};
/* }}} */

/* {{{ struct definition: apc_sma_info_t */
typedef struct apc_sma_info_t apc_sma_info_t;
struct apc_sma_info_t {
    int num_seg;                 /* number of shared memory segments */
    apc_sma_seginfo_t* seginfo;  /* array of per segment info */
};
/* }}} */

extern void apc_sma_init(apc_segment_t *segment, char *mmap_file_mask);
extern void apc_sma_cleanup(apc_segment_t *segment);
extern void* apc_sma_malloc(size_t size);
extern void* apc_sma_realloc(void* p, size_t size);
extern void apc_sma_free(void* p);
#if ALLOC_DISTRIBUTION
extern size_t *apc_sma_get_alloc_distribution();
#endif

extern apc_sma_info_t* apc_sma_info(zend_bool limited TSRMLS_DC);
extern void apc_sma_free_info(apc_sma_info_t* info);

extern void apc_sma_check_integrity();

/* {{{ ALIGNWORD: pad up x, aligned to the system's word boundary */
typedef union { void* p; int i; long l; double d; void (*f)(); } apc_word_t;
#define ALIGNSIZE(x, size) ((size) * (1 + (((x)-1)/(size))))
#define ALIGNWORD(x) ALIGNSIZE(x, sizeof(apc_word_t))
/* }}} */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
