/* ==================================================================
 * APC Cache
 * Copyright (c) 2000-2001 Community Connect, Inc.
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


#ifndef INCLUDED_APC_SEM
#define INCLUDED_APC_SEM

/* semaphore wrappers */

/* apc_sem_create: create a semaphore. if it does not already exist, set its
 * value to initval */
extern int  apc_sem_create(const char* pathname, int proj, int initval);

/* apc_sem_destroy: destroy a semaphore */
extern void apc_sem_destroy(int semid);

/* apc_sem_lock: acquire lock on semaphore */
extern void apc_sem_lock(int semid);

/* apc_sem_unlock: release lock on semaphore */
extern void apc_sem_unlock(int semid);

/* apc_sem_waitforzero: wait for semaphore count to reach zero */
extern void apc_sem_waitforzero(int semid);

/* apc_sem_getvalue: return the value of a semaphore */
extern int  apc_sem_getvalue(int semid);

#endif
