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
   |          George Schlossnagle <george@lethargy.org>                   |
   +----------------------------------------------------------------------+
*/

#include "apc_sem.h"
#include "apc_lib.h"
#include "apc_phpdeps.h"
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#if !HAVE_SEMUN
union semun {
	int val;                    /* value for SETVAL */
	struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
	unsigned short int *array;  /* array for GETALL, SETALL */
	struct seminfo *__buf;      /* buffer for IPC_INFO */
};
#undef HAVE_SEMUN
#define HAVE_SEMUN 1
#endif

# define APC_SEM_R 0444	/* read permission */
# define APC_SEM_A 0222	/* write permission */

/* always use SEM_UNDO, otherwise we risk deadlock */
#define USE_SEM_UNDO

#ifdef USE_SEM_UNDO
# define UNDO SEM_UNDO
#else
# define UNDO 0
#endif

/* apc_sem_create: create a semaphore. if it does not already exist, set its
 * value to initval */
int apc_sem_create(const char* pathname, int proj, int initval)
{
	int semid;
	int perms;
	union semun arg;
	key_t key;

	perms = APC_SEM_R | APC_SEM_A;

	key = IPC_PRIVATE;
	if (pathname != NULL) {
		if ((key = ftok(pathname, proj)) < 0) {
			apc_eprint("apc_sem_create: ftok(%s,%d) failed:", pathname, proj);
		}
	}
	
	if ((semid = semget(key, 1, IPC_CREAT | IPC_EXCL | perms)) >= 0) {
		/* sempahore created for the first time, initialize now */
		arg.val = initval;
		if (semctl(semid, 0, SETVAL, arg) < 0) {
			apc_eprint("apc_sem_create: semctl(%d,...) failed:", semid);
		}
	}
	else if (errno == EEXIST) {
		/* sempahore already exists, don't initialize */
		if ((semid = semget(key, 1, perms)) < 0) {
			apc_eprint("apc_sem_create: semget(%u,...) failed:", key);
		}
		/* insert <sleazy way to avoid race condition> here */
	}
	else {
		apc_eprint("apc_sem_create: semget(%u,...) failed:", key);
	}

	return semid;
}

/* apc_sem_destroy: destroy a semaphore */
void apc_sem_destroy(int semid)
{
	/* we expect this call to fail often, so we do not check */
	semctl(semid, 0, IPC_RMID);
}

/* apc_sem_lock: acquire lock on semaphore */
void apc_sem_lock(int semid)
{
	struct sembuf op;

	op.sem_num = 0;
	op.sem_op  = -1;
	op.sem_flg = UNDO;

	if (semop(semid, &op, 1) < 0) {
		if (errno != EINTR) {
			apc_eprint("apc_sem_lock: semop(%d) failed:", semid);
		}
	}
}

/* apc_sem_unlock: release lock on semaphore */
void apc_sem_unlock(int semid)
{
	struct sembuf op;

	op.sem_num = 0;
	op.sem_op  = 1;
	op.sem_flg = UNDO;

	if (semop(semid, &op, 1) < 0) {
		if (errno != EINTR) {
			apc_eprint("apc_sem_unlock: semop(%d) failed:", semid);
		}
	}
}

/* apc_sem_waitforzero: wait for semaphore count to reach zero */
void apc_sem_waitforzero(int semid)
{
	struct sembuf op;

	op.sem_num = 0;
	op.sem_op  = 0;
	op.sem_flg = UNDO;

	if (semop(semid, &op, 1) < 0) {
		if (errno != EINTR) {
			apc_eprint("apc_sem_waitforzero: semop(%d) failed:", semid);
		}
	}
}

/* apc_sem_getvalue: return the value of a semaphore */
int apc_sem_getvalue(int semid)
{
	union semun arg;
	unsigned short val[1];

	arg.array = val;
	if (semctl(semid, 0, GETALL, arg) < 0) {
		apc_eprint("apc_sem_getvalue: semctl(%d,...) failed:", semid);
	}
	return val[0];
}

