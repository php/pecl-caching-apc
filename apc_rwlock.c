/* ==================================================================
 * APC Cache
 * Copyright (c) 2000 Community Connect, Inc.
 * All rights reserved.
 * ==================================================================
 * This source code is made available free and without charge subject
 * to the terms of the QPL as detailed in bundled LICENSE file, which
 * is also available at http://apc.communityconnect.com/LICENSE.
 * ==================================================================
 * Daniel Cowgill <dan@mail.communityconnect.com>
 * George Schlossnagle <george@lethargy.org>
 * ==================================================================
*/


#include "apc_rwlock.h"
#include "apc_sem.h"
#include "apc_lib.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#define LOCK_SEM    0x01
#define READER_SEM  0x02
#define WRITER_SEM  0x03
#define WAITING_SEM 0x04

/* apc_rwl_create: */
apc_rwlock_t* apc_rwl_create(const char* pathname)
{
	apc_rwlock_t* lock = (apc_rwlock_t*) apc_emalloc(sizeof(apc_rwlock_t));

	lock->lock    = apc_sem_create(pathname, LOCK_SEM,    1);
	lock->reader  = apc_sem_create(pathname, READER_SEM,  0);
	lock->writer  = apc_sem_create(pathname, WRITER_SEM,  0);
	lock->waiting = apc_sem_create(pathname, WAITING_SEM, 0);
	return lock;
}

/* apc_rwl_destroy: */
void apc_rwl_destroy(apc_rwlock_t* lock)
{
	apc_sem_destroy(lock->lock);
	apc_sem_destroy(lock->reader);
	apc_sem_destroy(lock->writer);
	apc_sem_destroy(lock->waiting);
	apc_efree(lock);
}

/* apc_rwl_readlock: */
void apc_rwl_readlock(apc_rwlock_t* lock)
{
	apc_sem_lock(lock->lock);

	for (;;) {
		int nwaiting;	/* waiting writers */
		int nwriter;	/* active writers */

		if ((nwaiting = apc_sem_getvalue(lock->waiting)) == 0 &&
		    (nwriter = apc_sem_getvalue(lock->writer)) == 0)
		{
			break;	/* no writers waiting or active */
		}

		apc_sem_unlock(lock->lock);

		if (nwaiting > 0) {
			apc_sem_waitforzero(lock->waiting);
		}
		else {
			apc_sem_waitforzero(lock->writer);
		}

		apc_sem_lock(lock->lock);
	}

	apc_sem_unlock(lock->reader);
	apc_sem_unlock(lock->lock);
}

/* apc_rwl_writelock: */
void apc_rwl_writelock(apc_rwlock_t* lock)
{
	apc_sem_lock(lock->lock);

	for (;;) {
		int nreader;	/* active readers */
		int nwriter;	/* active writers */

		if ((nwriter = apc_sem_getvalue(lock->writer)) == 0 &&
		    (nreader = apc_sem_getvalue(lock->reader)) == 0)
		{
			break;	/* no readers, no writers */
		}

		apc_sem_unlock(lock->waiting);
		apc_sem_unlock(lock->lock);

		if (nwriter > 0) {
			apc_sem_waitforzero(lock->writer);
		}
		else {
			apc_sem_waitforzero(lock->reader);
		}

		apc_sem_lock(lock->lock);
		apc_sem_lock(lock->waiting);
	}

	apc_sem_unlock(lock->writer);
	apc_sem_unlock(lock->lock);
}

/* apc_rwl_unlock: */
void apc_rwl_unlock(apc_rwlock_t* lock)
{
	apc_sem_lock(lock->lock);

	if (apc_sem_getvalue(lock->reader) > 0) {
		apc_sem_lock(lock->reader);	/* one less reader */
	}
	else {
		apc_sem_lock(lock->writer);	/* one less writer */
	}

	apc_sem_unlock(lock->lock);
}

