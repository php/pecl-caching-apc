/*
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: George Schlossnagle <george@omniti.com>                     |
   +----------------------------------------------------------------------+
*/
#ifndef APC_LOCK
#define APC_LOCK

#include "apc_sem.h"
#include "apc_fcntl.h"
#include "config.h"

#ifdef APC_SEM_LOCKS
#define apc_lck_create(a,b,c) apc_sem_create(NULL,(b),(c))
#define apc_lck_destroy(a)    apc_sem_destroy(a)
#define apc_lck_lock(a)       apc_sem_lock(a)
#define apc_lck_unlock(a)     apc_sem_unlock(a)
#else
#define apc_lck_create(a,b,c) apc_fcntl_create((a))
#define apc_lck_destroy(a)    apc_fcntl_destroy(a)
#define apc_lck_lock(a)       apc_fcntl_lock(a)
#define apc_lck_unlock(a)     apc_fcntl_unlock(a)
#endif

#endif
