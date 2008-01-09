/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2008 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Gopal Vijayaraghavan <gopalv@yahoo-inc.com>                 |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Yahoo! Inc. in 2008.
   
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$ */

#ifndef APC_POOL_H
#define APC_POOL_H

#include "apc.h"
#include "apc_sma.h"

typedef enum {
    APC_SMALL_POOL  = 1,
    APC_MEDIUM_POOL = 2,
    APC_LARGE_POOL  = 3
} apc_pool_type;

typedef struct _apc_pool apc_pool;

extern apc_pool* apc_pool_create(apc_pool_type pool_type, 
                            apc_malloc_t allocate, 
                            apc_free_t deallocate);


extern void apc_pool_destroy(apc_pool *pool);
extern void* apc_pool_alloc(apc_pool *pool, size_t size);
extern void apc_pool_free(apc_pool *pool, void *ptr);
extern int apc_pool_check_integrity(apc_pool *pool);

#endif
