/* 
   +----------------------------------------------------------------------+
   | APC
   +----------------------------------------------------------------------+
   | Copyright (c) 2000-2002 Community Connect Inc.
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   |          Ricardo Galli <gallir@uib.es>                               |
   |          George Schlossnagle <george@lethargy.org>                   |
   +----------------------------------------------------------------------+
*/

#include "apc_rwlock.h"
#include "apc_lib.h"
#include "apc_sem.h"

enum {
	LOCK_SEM    = 0x01,
	READER_SEM  = 0x02,
	WRITER_SEM  = 0x03,
};

struct apc_rwlock_t {
	int lock;		/* writer mutex */
	int reader;		/* reader count (>0 if one or more readers active) */
	int writer;		/* writer count (>0 if a writer is active) */
};

/* apc_rwl_create: create a new lock instance */
apc_rwlock_t* apc_rwl_create(const char* pathname)
{
	apc_rwlock_t* lock = (apc_rwlock_t*) apc_emalloc(sizeof(apc_rwlock_t));

	lock->lock   = apc_sem_create(pathname, LOCK_SEM,    1);
	lock->reader = apc_sem_create(pathname, READER_SEM,  0);
	lock->writer = apc_sem_create(pathname, WRITER_SEM,  0);
	return lock;
}

/* apc_rwl_destroy: destroy a lock instance */
void apc_rwl_destroy(apc_rwlock_t* lock)
{
	apc_sem_destroy(lock->lock);
	apc_sem_destroy(lock->reader);
	apc_sem_destroy(lock->writer);
	apc_efree(lock);
}

/* apc_rwl_readlock: acquire a read-only (shared) lock */
void apc_rwl_readlock(apc_rwlock_t* lock)
{
	apc_sem_waitforzero(lock->writer);
	apc_sem_unlock(lock->reader);
}

/* apc_rwl_writelock: acquire a write (exclusive) lock */
void apc_rwl_writelock(apc_rwlock_t* lock)
{
	apc_sem_unlock(lock->writer);
	apc_sem_waitforzero(lock->reader);
	apc_sem_lock(lock->lock);
}

/* apc_rwl_unlock: release any lock */
void apc_rwl_unlock(apc_rwlock_t* lock)
{
	if (apc_sem_getvalue(lock->lock) <= 0) {	/* was write lock */
		apc_sem_unlock(lock->lock);     
		apc_sem_lock(lock->writer);     
	}
	else {
		apc_sem_lock(lock->reader);				/* was a read lock */
	}
}

