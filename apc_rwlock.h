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


#ifndef INCLUDED_APC_RWLOCK
#define INCLUDED_APC_RWLOCK

/* readers-writer lock implementation; gives preference to waiting
 * writers over readers */

typedef struct apc_rwlock_t apc_rwlock_t;
struct apc_rwlock_t {
	int lock;		/* for locking access to other semaphores */
	int reader;		/* reader count (>0 if one or more readers active) */
	int writer;		/* writer count (>0 if a writer is active) */
	int waiting;	/* waiting writers count (>0 if writers waiting) */
};

extern apc_rwlock_t* apc_rwl_create(const char* pathname);
extern void          apc_rwl_destroy(apc_rwlock_t* lock);
extern void          apc_rwl_readlock(apc_rwlock_t* lock);
extern void          apc_rwl_writelock(apc_rwlock_t* lock);
extern void          apc_rwl_unlock(apc_rwlock_t* lock);

#endif
