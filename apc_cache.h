/* 
   +----------------------------------------------------------------------+
   | APC
   +----------------------------------------------------------------------+
   | Copyright (c) 2000-2002 Community Connect Inc.
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   |          George Schlossnagle <george@lethargy.org>                   |
   +----------------------------------------------------------------------+
*/

#ifndef INCLUDED_APC_CACHE
#define INCLUDED_APC_CACHE

#include "zend.h"
#include "apc_lib.h"

#define T apc_cache_t*
typedef struct apc_cache_t apc_cache_t; /* opaque cache type */

enum {	/* (see apc_cache_setretrievaltype below) */
	APC_CACHE_RT_SAFE,	/* "correct" method */
	APC_CACHE_RT_FAST	/* potentially unsafe but efficient */
};

/*
 * apc_cache_create: creates a new shared cache. nbuckets is the number
 * of buckets used in the cache's hashtable and should be about 25% - 50%
 * greater than the number of files expected to be cached. ttl is the
 * time-to-live for cache entries. if ttl is zero, entries do not expire
 */
extern T apc_cache_create(const char* pathname, int nbuckets, int ttl);

/*
 * apc_cache_destroy: destroys an existing cache. Does not modify shared
 * memory and only attempts to remove shared memory segments and semaphores.
 * (Thus, if the current process does not have permission to destroy those
 * IPC objects, this function may be safely called multiple times without
 * altering/invalidating the cache
 */
extern void apc_cache_destroy(T cache);

/*
 * apc_cache_setretrievaltype: set the preferred method for cache index
 * lookup and retrieval used in apc_cache_retrieve (see below). returns
 * 0 if the specified type is supported, non-zero otherwise. note: the
 * default retrieval type is APC_CACHE_RT_SAFE
 */
extern int apc_cache_setretrievaltype(int type);

/*
 * apc_cache_clear: removes all entries from the cache
 */
extern void apc_cache_clear(T cache);

/*
 * apc_cache_search: returns true if key exists in cache, else false
 */
extern int apc_cache_search(T cache, const char* key);

/*
 * apc_cache_retrieve: searches for key in cache. Returns null if not found,
 * otherwise stores associated data in *dataptr, expanding array as necessary.
 * *length will be set to the length of data stored in dataptr. *maxsize must
 * contain the current size of the *dataptr array; it will be set to the new
 * size of *dataptr if it is expanded. The current modification time of the
 * file may be optionally supplied, and if it is greater than the old time
 * the entry is expired (set mtime to zero to disable this check)
 *
 * note that apc_cache_retrieve is a pointer to the implementation function.
 * the precise behavior of this function is controlled by
 * apc_cache_setretrievaltype.
 */
extern int (*apc_cache_retrieve)(T cache, const char* key, char** dataptr,
                                 int* length, int* maxsize, int mtime);

/*
 * apc_cache_insert: adds a new mapping to cache. If the key already has a
 * mapping, it is removed and replaced with the new one. The key is
 * associated with the first size bytes stored in data. If the current
 * modification time of the file is supplied in mtime, it can be compared
 * subsequently in apc_cache_retrieve. Returns true on success, else false
 */
extern int apc_cache_insert(T cache, const char* key, const char* data,
                            int size, int mtime);

/*
 * apc_shm_rm: removes a mapping from the cache. Returns true on
 * success, else false
 */
extern int apc_shm_rm(T cache, const char* key);

/*
 * apc_cache_set_object_ttl: sets the ttl for an individual object. Returns
 * true on success, else false
 */
extern int apc_cache_set_object_ttl(T cache, const char* key, int ttl);

/*
 * apc_cache_dump: display information about a cache
 */
extern void apc_cache_dump(T cache, const char* linkurl,
                           apc_outputfn_t outputfn);

/*
 * apc_cache_dump_entry: display information about a specified entry
 * in the cache. Returns 0 if the entry was found, else non-zero.
 */
extern int apc_cache_dump_entry(T cache, const char* key,
                                apc_outputfn_t outputfn);

/*
 * apc_cache_index_shm: creates a hash keyed with all the objects in the cache
 * and containing all the cache object details
 */
extern int apc_cache_index_shm(apc_cache_t* cache, zval **hash); 

/*
 * apc_cache_info_shm: creates a hash keyed with information about the cache
 * status
 */
extern int apc_cache_info_shm(apc_cache_t* cache, zval **hash);


/*
 * apc_object_info_shm: creates a hash keyed with information about the 
 * functions stored in object named by filename
 */
extern int apc_object_info_shm(apc_cache_t* cache, char const*filename, zval **hash);

#undef T
#endif
