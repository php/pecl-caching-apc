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


#ifndef INCLUDED_APC_SHM
#define INCLUDED_APC_SHM

/* shared memory wrapper. no surprises */

extern int   apc_shm_create(const char* pathname, int proj, int size);
extern void  apc_shm_destroy(int shmid);
extern void* apc_shm_attach(int shmid);
extern void  apc_shm_detach(void* shmaddr);

#endif
