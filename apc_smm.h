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


#ifndef INCLUDED_APC_SMM
#define INCLUDED_APC_SMM

#include "apc_lib.h"

/* simple shared memory manager */

extern void  apc_smm_init();
extern void  apc_smm_initsegment(int shmid, int segsize);
extern void  apc_smm_cleanup();
extern void* apc_smm_attach(int shmid);
extern void  apc_smm_detach(void* shmaddr);
extern int	 apc_smm_alloc(void* shmaddr, int size);
extern void  apc_smm_free(void* shmaddr, int offset);
extern void  apc_smm_dump(void* shmaddr, apc_outputfn_t outputfn);

#endif
