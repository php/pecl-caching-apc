/* 
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
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
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef APC_CACHE_H
#define APC_CACHE_H

/*
 * This module defines the shared memory file cache. Basically all of the
 * logic for storing and retrieving cache entries lives here.
 */

#include "apc.h"
#include "apc_compile.h"

#define T apc_cache_t*
typedef struct apc_cache_t apc_cache_t; /* opaque cache type */

/* {{{ struct definition: apc_cache_key_t */
typedef struct apc_cache_key_t apc_cache_key_t;
struct apc_cache_key_t {
    int device;                 /* the filesystem device */
    int inode;                  /* the filesystem inode */
    int mtime;                  /* the mtime of this cached file */
};
/* }}} */

/* {{{ struct definition: apc_cache_entry_t */
typedef struct apc_cache_entry_t apc_cache_entry_t;
struct apc_cache_entry_t {
    char* filename;             /* absolute path to source file */
    zend_op_array* op_array;    /* op_array allocated in shared memory */
    apc_function_t* functions;  /* array of apc_function_t's */
    apc_class_t* classes;       /* array of apc_class_t's */
    int ref_count;              /* concurrently executing processes */
};
/* }}} */

/*
 * apc_cache_create creates the shared memory compiler cache. This function
 * should be called just once (ideally in the web server parent process, e.g.
 * in apache), otherwise you will end up with multiple caches (which won't
 * necessarily break anything). Returns a pointer to the cache object.
 *
 * size_hint is a "hint" at the total number of source files that will be
 * cached. It determines the physical size of the hash table. Passing 0 for
 * this argument will use a reasonable default value.
 *
 * gc_ttl is the maximum time a cache entry may speed on the garbage
 * collection list. This is basically a work around for the inherent
 * unreliability of our reference counting mechanism (see apc_cache_release).
 */
extern T apc_cache_create(int size_hint, int gc_ttl);

/*
 * apc_cache_destroy releases any OS resources associated with a cache object.
 * Under apache, this function can be safely called by the child processes
 * when they exit.
 */
extern void apc_cache_destroy(T cache);

/*
 * apc_cache_clear empties a cache. This can safely be called at any time,
 * even while other server processes are executing cached source files.
 */
extern void apc_cache_clear(T cache);

/*
 * apc_cache_insert adds an entry to the cache, using a filename as a key.
 * Internally, the filename is translated to a canonical representation, so
 * that relative and absolute filenames will map to a single key. Returns
 * non-zero if the file was successfully inserted, 0 otherwise. If 0 is
 * returned, the caller must free the cache entry by calling
 * apc_cache_free_entry (see below).
 *
 * key is the value created by apc_cache_make_key.
 *
 * value is a cache entry returned by apc_cache_make_entry (see below).
 */
extern int apc_cache_insert(T cache, apc_cache_key_t key,
                            apc_cache_entry_t* value);

/*
 * apc_cache_find searches for a cache entry by filename, and returns a
 * pointer to the entry if found, NULL otherwise.
 *
 * key is a value created by apc_cache_make_key.
 */
extern apc_cache_entry_t* apc_cache_find(T cache, apc_cache_key_t key);

/*
 * apc_cache_release decrements the reference count associated with a cache
 * entry. Calling apc_cache_find automatically increments the reference count,
 * and this function must be called post-execution to return the count to its
 * original value. Failing to do so will prevent the entry from being
 * garbage-collected.
 *
 * entry is the cache entry whose ref count you want to decrement.
 */
extern void apc_cache_release(T cache, apc_cache_entry_t* entry);

/*
 * apc_cache_make_key creates a key object given a relative or absolute
 * filename and an optional list of auxillary paths to search. include_path is
 * searched if the filename cannot be found relative to the current working
 * directory.
 *
 * key points to caller-allocated storage (must not be null).
 *
 * filename is the path to the source file.
 *
 * include_path is a colon-separated list of directories to search.
 */
extern int apc_cache_make_key(apc_cache_key_t* key,
                              const char* filename,
                              const char* include_path);

/*
 * apc_cache_make_entry creates an apc_cache_entry_t object given a filename
 * and the compilation results returned by the PHP compiler.
 */
extern apc_cache_entry_t* apc_cache_make_entry(const char* filename,
                                               zend_op_array* op_array,
                                               apc_function_t* functions,
                                               apc_class_t* classes);

/*
 * Frees all memory associated with an object returned by apc_cache_make_entry
 * (see above).
 */
extern void apc_cache_free_entry(apc_cache_entry_t* entry);


/* {{{ struct definition: apc_cache_link_t */
typedef struct apc_cache_link_t apc_cache_link_t;
struct apc_cache_link_t {
    char* filename;
    int device;
    int inode;
    int num_hits;
    time_t mtime;
    time_t creation_time;
    time_t deletion_time;
    int ref_count;
    apc_cache_link_t* next;
};
/* }}} */

/* {{{ struct definition: apc_cache_info_t */
typedef struct apc_cache_info_t apc_cache_info_t;
struct apc_cache_info_t {
    int num_slots;
    int num_hits;
    int num_misses;
    apc_cache_link_t* list;
    apc_cache_link_t* deleted_list;
};
/* }}} */

extern apc_cache_info_t* apc_cache_info(T cache);
extern void apc_cache_free_info(apc_cache_info_t* info);

#undef T
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
