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

/*
 * apc_smm_init: initialize the management system
 */
extern void apc_smm_init();

/*
 * apc_smm_initsegment: initialize the specified shared memory segment
 */
extern void apc_smm_initsegment(int shmid, int segsize);

/*
 * apc_smm_cleanup: cleans up the index and attached segments, invalidating
 * all pointers into shared memory managed by this system
 */
extern void apc_smm_cleanup();

/*
 * apc_smm_attach: return address associated with ID shmid
 */
extern void* apc_smm_attach(int shmid);

/*
 * apc_shm_detach: doesn't currently do anything
 */
extern void apc_smm_detach(void* shmaddr);

/*
 * apc_smm_alloc: return offset to size bytes of contiguous memory 
 * (relative to shmaddr, the start address (and location of the segment
 * header, or -1 if not enough memory is available in the segment.
 */
extern int apc_smm_alloc(void* shmaddr, int size);

/*
 * apc_smm_free: frees the memory at given offset, which must have been
 * returned by apc_smm_alloc
 */
extern void apc_smm_free(void* shmaddr, int offset);

/*
 * apc_smm_dump: print segment information to file stream.  We call this
 * from apcinfo() to generate the cache information page.
 */
extern void apc_smm_dump(void* shmaddr, apc_outputfn_t outputfn);

#endif
