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
	
	
