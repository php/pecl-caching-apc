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
        char *input;
};

void apc_mmap_dump(apc_outputfn_t outputfn, HashTable* cache);
#endif
	
	
