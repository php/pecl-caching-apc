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

#include "apc_shm.h"
#include "apc.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#ifndef SHM_R
# define SHM_R 0444 /* read permission */
#endif
#ifndef SHM_A
# define SHM_A 0222 /* write permission */
#endif

int apc_shm_create(const char* pathname, int proj, int size)
{
    int shmid;  /* shared memory id */
    int oflag;  /* permissions on shm */
    key_t key;  /* shm key returned by ftok */

    key = IPC_PRIVATE;
    if (pathname != NULL) {
        if ((key = ftok(pathname, proj)) < 0) {
            apc_eprint("apc_shm_create: ftok failed:");
        }
    }

    oflag = IPC_CREAT | SHM_R | SHM_A;
    if ((shmid = shmget(key, size, oflag)) < 0) {
        apc_eprint("apc_shm_create: shmget(%d, %d,%d) failed: %s", key, size, oflag, strerror(errno));
    }

    return shmid;
}

void apc_shm_destroy(int shmid)
{
    /* we expect this call to fail often, so we do not check */
    shmctl(shmid, IPC_RMID, 0);
}

void* apc_shm_attach(int shmid)
{
    void* shmaddr;  /* the shared memory address */

    if ((int)(shmaddr = shmat(shmid, 0, 0)) == -1) {
        apc_eprint("apc_shm_attach: shmat failed:");
    }

    /*
     * We set the shmid for removal immediately after attaching to it. The
     * segment won't disappear until all processes have detached from it.
     */
    apc_shm_destroy(shmid);
    return shmaddr;
}

void apc_shm_detach(void* shmaddr)
{
    if (shmdt(shmaddr) < 0) {
        apc_eprint("apc_shm_detach: shmdt failed:");
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
