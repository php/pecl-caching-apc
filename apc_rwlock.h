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


#ifndef INCLUDED_APC_RWLOCK
#define INCLUDED_APC_RWLOCK

/* readers-writer lock implementation; gives preference to waiting
 * writers over readers */

#define T apc_rwlock_t*
typedef struct apc_rwlock_t apc_rwlock_t;

extern T    apc_rwl_create(const char* pathname);
extern void apc_rwl_destroy(T lock);
extern void apc_rwl_readlock(T lock);
extern void apc_rwl_writelock(T lock);
extern void apc_rwl_unlock(T lock);

#undef T
#endif
