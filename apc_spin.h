/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2008 The PHP Group                                     |
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

#ifndef APC_SPIN_H
#define APC_SPIN_H

#include "apc.h"

#ifdef APC_SPIN_LOCKS 

#include "pgsql_s_lock.h"

pthread_mutex_t *apc_spin_create();
void apc_spin_destroy(pthread_mutex_t *lock);
void apc_spin_lock(pthread_mutex_t *lock);
void apc_spin_unlock(pthread_mutex_t *lock);
zend_bool apc_spin_nonblocking_lock(pthread_mutex_t *lock);

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
