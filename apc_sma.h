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
#ifndef INCLUDED_APC_SMA
#define INCLUDED_APC_SMA

/* Simple shared memory allocator. */

extern void apc_sma_init(int numseg, int segsize);
extern void apc_sma_cleanup();
extern void* apc_sma_malloc(int size);
extern void apc_sma_free(void* p);
//extern void apc_sma_toshared(void* p, int* shmid, int* off);
//extern void* apc_sma_tolocal(int shmid, int off);

#endif
