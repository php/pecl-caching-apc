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

struct mm_fl_element {
        char *cache_filename;
        int inputlen;
        long inode;
        time_t mtime;
	int hitcounter;
        char *input;
};

void apc_mmap_dump(apc_outputfn_t outputfn, HashTable* cache);
#endif
	
	
