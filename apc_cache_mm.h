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
#ifndef INCLUDED_APC_CACHE_MM
#define INCLUDED_APC_CACHE_MM

#include "apc_lib.h"
#include "zend.h"
#include "zend_hash.h"

/* The mm_fl_element struct is the bucket element for each child's cache of
 * mmap'd files. */
struct mm_fl_element {
	char *cache_filename; 	/* path to cached file */
	int inputlen;			/* length of the file */
	long inode;				/* inode of the file */
	time_t mtime;			/* time of last update to file */
	time_t srcmtime;		/* time of last update to src file */
	int hitcounter;			/* number of accesses by THIS child */
	char *input;			/* mmap'd address of the file */
};

extern char *apc_generate_cache_filename(const char *filename);

/*
 * apc_mmap_dump: generates output for apcinfo()
 */
extern void apc_mmap_dump(HashTable* cache, const char * url, apc_outputfn_t outputfn);

/*
 * apc_mmap_dump_entry: prints information about a specified
 * cache entry
 */
extern int apc_mmap_dump_entry(const char* filename, apc_outputfn_t outputfn);

/*
 * apc_cache_index_mmap: creates a hash keyed with all the objects in the cache
 * and containing all the cache object details
 */
extern int apc_cache_index_mmap(HashTable* cache, zval** hash);
#endif
	
	
