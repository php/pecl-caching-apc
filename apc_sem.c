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
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "apc_sem.h"
#include "apc.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <unistd.h>

#if HAVE_SEMUN
/* we have semun, no need to define */
#else
union semun {
    int val;                  /* value for SETVAL */
    struct semid_ds *buf;     /* buffer for IPC_STAT, IPC_SET */
    unsigned short *array;    /* array for GETALL, SETALL */
                              /* Linux specific part: */
    struct seminfo *__buf;    /* buffer for IPC_INFO */
};
#endif

#ifndef SEM_R
# define SEM_R 0444
#endif
#ifndef SEM_A
# define SEM_A 0222
#endif

/* always use SEM_UNDO, otherwise we risk deadlock */
#define USE_SEM_UNDO

#ifdef USE_SEM_UNDO
# define UNDO SEM_UNDO
#else
# define UNDO 0
#endif

int apc_sem_create(const char* pathname, int proj, int initval)
{
    int semid;
    int perms;
    union semun arg;
    key_t key;

    perms = 0777;

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

void apc_sem_destroy(int semid)
{
    /* we expect this call to fail often, so we do not check */
    union semun arg;
    semctl(semid, 0, IPC_RMID, arg);
}

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

void apc_sem_wait_for_zero(int semid)
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

int apc_sem_get_value(int semid)
{
    union semun arg;
    unsigned short val[1];

    arg.array = val;
    if (semctl(semid, 0, GETALL, arg) < 0) {
        apc_eprint("apc_sem_getvalue: semctl(%d,...) failed:", semid);
    }
    return val[0];
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
