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
  | Authors: Brian Shire <shire@php.net>                                 |
  +----------------------------------------------------------------------+

 */

/* $Id$ */

#ifndef APC_FUTEX_H
#define APC_FUTEX_H

#include "apc.h"

#ifdef APC_FUTEX_LOCKS 

#include <asm/types.h>
#include <unistd.h>
#include <linux/futex.h>

#include "arch/atomic.h"

#define sys_futex(futex, op, val, timeout) syscall(SYS_futex, futex, op, val, timeout)
#define apc_futex_wait(val, oldval) sys_futex((void*)val, FUTEX_WAIT, oldval, NULL)
#define apc_futex_wake(val, count) sys_futex((void*)val, FUTEX_WAKE, count, NULL)

int apc_futex_create();
void apc_futex_destroy(volatile int* lock);
void apc_futex_lock(volatile int* lock);
void apc_futex_unlock(volatile int* lock);

#endif

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
