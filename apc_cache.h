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
#ifndef INCLUDED_APC_CACHE
#define INCLUDED_APC_CACHE

#include "apc_lib.h"

/* TODO: note about apc_cache_destroy(): it does not modify shared memory;
 * it only attempts to remove shared memory segments and semaphores.
 * Thus, if the current process does not have permission to destroy those
 * IPC objects, apc_cache_destroy() may be safely called multiple times
 * without invalidating the cache */

typedef struct apc_cache_t apc_cache_t; /* opaque cache type */

/*
 * apc_cache_create: creates a new shared cache
 */
extern apc_cache_t* apc_cache_create(const char* pathname, int nbuckets,
	int maxseg, int segsize, int ttl);

/*
 * apc_cache_destroy: destroys an existing cache
 */
extern void apc_cache_destroy(apc_cache_t* cache);

/*
 * apc_cache_clear: clears the cache
 */
extern void apc_cache_clear(apc_cache_t* cache);

/*
 * apc_cache_search: returns true if key exists in cache, else false
 */
extern int apc_cache_search(apc_cache_t* cache, const char* key);

/*
 * apc_cache_retrieve: lookups key in cache. Returns null if not found,
 * otherwise stores associated data in dataptr, expanding array as necessary.
 * length and maxsize are updated as appropriate
 */
extern int apc_cache_retrieve(apc_cache_t* cache, const char* key,
	char** dataptr, int* length, int* maxsize);

/*
 * apc_cache_retrieve_nl: searches for key in cache, and if found, sets
 * *dataptr to point to the start of the cached data and *length to the
 * number of bytes cached. Note that this routine should be surrounded
 * by external locking calls (see below)
 */
extern int apc_cache_retrieve_nl(apc_cache_t* cache, const char* key,
	char** dataptr, int* length);

/*
 * apc_cache_insert: adds a new mapping to cache. If the key already has a
 * mapping, it is removed and replaced with the new one
 */
extern int apc_cache_insert(apc_cache_t* cache, const char* key,
	const char* data, int size);

/*
 * apc_cache_remove: removes a mapping from the cache
 */
extern int apc_cache_remove(apc_cache_t* cache, const char* key);

/*
 * apc_cache_set_object_ttl: sets the ttl for an individual object
 */

extern int apc_cache_set_object_ttl(apc_cache_t* cache,
	const char* key, int ttl);

/*
 * routines to externally lock a cache
 */
extern void apc_cache_readlock(apc_cache_t* cache);
extern void apc_cache_writelock(apc_cache_t* cache);
extern void apc_cache_unlock(apc_cache_t* cache);

/*
 * apc_cache_dump: display information about a cache
 */
extern void apc_cache_dump(apc_cache_t* cache, apc_outputfn_t outputfn);

#endif
