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

/*
 * apc_sma_init: Initialize the shared memory allocator. Must be called once,
 * and only by the parent process -- before it forks any child processes.
 */
extern void apc_sma_init(int numseg, int segsize);

/*
 * apc_sma_cleanup: Cleans up the shared memory allocator. Should be called
 * only once, and only by the parent process.
 */
extern void apc_sma_cleanup();

/*
 * apc_sma_malloc: Allocates size bytes of shared memory. (Aborts the program
 * if insufficient resources are available to fulfill the request.)
 */
extern void* apc_sma_malloc(int size);

/*
 * apc_sma_free: Frees memory previously allocated by apc_sma_malloc. (Aborts
 * the program if the supplied address was not returned by apc_sma_malloc.)
 */
extern void apc_sma_free(void* p);

#endif
