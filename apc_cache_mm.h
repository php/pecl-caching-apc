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


#ifndef INCLUDED_APC_CACHE_MM
#define INCLUDED_APC_CACHE_MM
#include "apc_iface.h"
#include "zend.h"
#include "zend_hash.h"

/* The mm_fl_element struct is the bucket element for each child's cache of
 * mmap'd files. */
struct mm_fl_element {
        char *cache_filename; 	/* Where the cached file exists */
        int inputlen;			/* How long is the file */
        long inode;				/* What is the file's inode */
        time_t mtime;			/* When was the file last updated */
		int hitcounter;			/* How many times has THIS child accessed the file */
        char *input;			/* What's the address of the file in memory */
};

/* apc_mmap_dump generates output for apcinfo() */
void apc_mmap_dump(apc_outputfn_t outputfn, HashTable* cache);
#endif
	
	
