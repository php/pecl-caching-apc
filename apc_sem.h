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


#ifndef INCLUDED_APC_SEM
#define INCLUDED_APC_SEM

/* semaphore wrapper. no surprises */

extern int  apc_sem_create(const char* pathname, int proj, int initval);
extern void apc_sem_destroy(int semid);
extern void apc_sem_lock(int semid);
extern void apc_sem_unlock(int semid);
extern void apc_sem_waitforzero(int semid);
extern int  apc_sem_getvalue(int semid);

#endif
