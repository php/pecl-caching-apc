/*
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/3_0.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "apc_sma.h"
#include "apc.h"
#include "apc_lock.h"
#include "apc_shm.h"
#include <limits.h>
#if APC_MMAP
void *apc_mmap(char *file_mask, int size);
void apc_unmap(void* shmaddr, int size);
#endif

enum { POWER_OF_TWO_BLOCKSIZE=0 };  /* force allocated blocks to 2^n? */

enum { DEFAULT_NUMSEG=1, DEFAULT_SEGSIZE=30*1024*1024 };

static int sma_initialized = 0;     /* true if the sma has been initialized */
static int sma_numseg;              /* number of shm segments to allow */
static int sma_segsize;             /* size of each shm segment */
static int* sma_segments;           /* array of shm segment ids */
static void** sma_shmaddrs;         /* array of shm segment addresses */
static int sma_lastseg = 0;         /* index of MRU segment */
static int sma_lock;                /* sempahore to serialize access */

typedef struct header_t header_t;
struct header_t {
    int segsize;    /* size of entire segment */
    int avail;      /* bytes available (not necessarily contiguous) */
};

typedef struct block_t block_t;
struct block_t {
    int size;       /* size of this block */
    int next;       /* offset in segment of next free block */
};

/* The macros BLOCKAT and OFFSET are used for convenience throughout this
 * module. Both assume the presence of a variable shmaddr that points to the
 * beginning of the shared memory segment in question. */

#define BLOCKAT(offset) ((block_t*)((char *)shmaddr + offset))
#define OFFSET(block) ((int)(((char*)block) - (char*)shmaddr))

#ifdef max
#undef max
#endif
#define max(a, b) ((a) > (b) ? (a) : (b))

/* {{{ alignword: returns x, aligned to the system's word boundary */
static int alignword(int x)
{
    typedef union { void* p; int i; long l; double d; void (*f)(); } word_t;
    return sizeof(word_t) * (1 + ((x-1)/sizeof(word_t)));
}
/* }}} */

/* {{{ sma_allocate: tries to allocate size bytes in a segment */
static int sma_allocate(void* shmaddr, int size)
{
    header_t* header;       /* header of shared memory segment */
    block_t* prv;           /* block prior to working block */
    block_t* cur;           /* working block in list */
    block_t* prvbestfit;    /* block before best fit */
    int realsize;           /* actual size of block needed, including header */
    int minsize;            /* for finding best fit */

    /* Realsize must be aligned to a word boundary on some architectures. */
    realsize = alignword(max(size + alignword(sizeof(int)), sizeof(block_t)));
    
    /*
     * Set realsize to the smallest power of 2 greater than or equal to
     * realsize. This increases the likelihood that neighboring blocks can be
     * coalesced, reducing memory fragmentation.
     */
    if (POWER_OF_TWO_BLOCKSIZE) {
        int p = 1;

        while (p < realsize) {
            p <<= 1;
        }
        realsize = p;
    }

    /*
     * First, insure that the segment contains at least realsize free bytes,
     * even if they are not contiguous.
     */
    header = (header_t*) shmaddr;
    if (header->avail < realsize) {
        return -1;
    }

    prvbestfit = 0;     /* initially null (no fit) */
    minsize = INT_MAX;  /* used to find best fit */

    prv = BLOCKAT(sizeof(header_t));
    while (prv->next != 0) {
        cur = BLOCKAT(prv->next);
        if (cur->size == realsize) {
            /* found a perfect fit, stop searching */
            prvbestfit = prv;
            break;
        }
        else if (cur->size > (sizeof(block_t) + realsize) &&
                 cur->size < minsize)
        {
            /* cur is acceptable and is the smallest so far */
            prvbestfit = prv;
            minsize = cur->size;
        }
        prv = cur;
    }

    if (prvbestfit == 0) {
        return -1;
    }

    prv = prvbestfit;
    cur = BLOCKAT(prv->next);

    /* update the block header */
    header->avail -= realsize;

    if (cur->size == realsize) {
        /* cur is a perfect fit for realsize; just unlink it */
        prv->next = cur->next;
    }
    else {
        block_t* nxt;   /* the new block (chopped part of cur) */
        int nxtoffset;  /* offset of the block currently after cur */
        int oldsize;    /* size of cur before split */

        /* bestfit is too big; split it into two smaller blocks */
        nxtoffset = cur->next;
        oldsize = cur->size;
        prv->next += realsize;
        cur->size = realsize;
        nxt = BLOCKAT(prv->next);
        nxt->next = nxtoffset;
        nxt->size = oldsize - realsize;
    }

    return OFFSET(cur) + alignword(sizeof(int));
}
/* }}} */

/* {{{ sma_deallocate: deallocates the block at the given offset */
static int sma_deallocate(void* shmaddr, int offset)
{
    header_t* header;   /* header of shared memory segment */
    block_t* cur;       /* the new block to insert */
    block_t* prv;       /* the block before cur */
    block_t* nxt;       /* the block after cur */
    int size;           /* size of deallocated block */

    offset -= alignword(sizeof(int));
    assert(offset >= 0);

    /* find position of new block in free list */
    prv = BLOCKAT(sizeof(header_t));
    while (prv->next != 0 && prv->next < offset) {
        prv = BLOCKAT(prv->next);
    }

    /* insert new block after prv */
    cur = BLOCKAT(offset);
    cur->next = prv->next;
    prv->next = offset;
    
    /* update the block header */
    header = (header_t*) shmaddr;
    header->avail += cur->size;
    size = cur->size;

    if (((char *)prv) + prv->size == (char *) cur) {
        /* cur and prv share an edge, combine them */
        prv->size += cur->size;
        prv->next = cur->next;
        cur = prv;
    }

    nxt = BLOCKAT(cur->next);
    if (((char *)cur) + cur->size == (char *) nxt) {
        /* cur and nxt shared an edge, combine them */
        cur->size += nxt->size;
        cur->next = nxt->next;
    }

    return size;
}
/* }}} */

/* {{{ apc_sma_init */

void apc_sma_init(int numseg, int segsize, char *mmap_file_mask)
{
    int i;

    if (sma_initialized) {
        return;
    }
    sma_initialized = 1;

#if APC_MMAP
    /*
     * I don't think multiple anonymous mmaps makes any sense
     * so force sma_numseg to 1 in this case
     */
    if(!mmap_file_mask || 
       (mmap_file_mask && !strlen(mmap_file_mask)) ||
       (mmap_file_mask && !strcmp(mmap_file_mask, "/dev/zero"))) {
        sma_numseg = 1;
    } else {
        sma_numseg = numseg > 0 ? numseg : DEFAULT_NUMSEG;
    }
#else
    sma_numseg = numseg > 0 ? numseg : DEFAULT_NUMSEG;
#endif
    sma_segsize = segsize > 0 ? segsize : DEFAULT_SEGSIZE;

    sma_segments = (int*) apc_emalloc(sma_numseg*sizeof(int));
    sma_shmaddrs = (void**) apc_emalloc(sma_numseg*sizeof(void*));
    
    sma_lock = apc_lck_create(NULL, 0, 1);

    for (i = 0; i < sma_numseg; i++) {
        header_t*   header;
        block_t*    block;
        void*       shmaddr;

#if APC_MMAP
        sma_segments[i] = sma_segsize;
        sma_shmaddrs[i] = apc_mmap(mmap_file_mask, sma_segsize);
#else
        sma_segments[i] = apc_shm_create(NULL, i, sma_segsize);
        sma_shmaddrs[i] = apc_shm_attach(sma_segments[i]);
#endif
        shmaddr = sma_shmaddrs[i];
    
        header = (header_t*) shmaddr;
        header->segsize = sma_segsize;
        header->avail = sma_segsize - sizeof(header_t) - sizeof(block_t) -
                        alignword(sizeof(int));
    
        block = BLOCKAT(sizeof(header_t));
        block->size = 0;
        block->next = sizeof(header_t) + sizeof(block_t);
    
        block = BLOCKAT(block->next);
        block->size = header->avail;
        block->next = 0;
    }
}
/* }}} */

/* {{{ apc_sma_cleanup */
void apc_sma_cleanup()
{
    int i;

    assert(sma_initialized);

    for (i = 0; i < sma_numseg; i++) {
#if APC_MMAP
        apc_unmap(sma_shmaddrs[i], sma_segments[i]);
#else
        apc_shm_detach(sma_shmaddrs[i]);
#endif
    }
    apc_lck_destroy(sma_lock);
    sma_initialized = 0;
}
/* }}} */

/* {{{ apc_sma_malloc */
void* apc_sma_malloc(size_t n)
{
    int off;
    int i;

    assert(sma_initialized);
    apc_lck_lock(sma_lock);

    off = sma_allocate(sma_shmaddrs[sma_lastseg], n);
    if (off != -1) {
        void* p = (void *)(((char *)(sma_shmaddrs[sma_lastseg])) + off);
        apc_lck_unlock(sma_lock);
        return p;
    }

    for (i = 0; i < sma_numseg; i++) {
        if (i == sma_lastseg) {
            continue;
        }
        off = sma_allocate(sma_shmaddrs[i], n);
        if (off != -1) {
            void* p = (void *)(((char *)(sma_shmaddrs[i])) + off);
            apc_lck_unlock(sma_lock);
            sma_lastseg = i;
            return p;
        }
    }

    apc_lck_unlock(sma_lock);
    return NULL;
}
/* }}} */

/* {{{ apc_sma_free */
void apc_sma_free(void* p)
{
    int i;

    if (p == NULL) {
        return;
    }

    apc_lck_lock(sma_lock);
    assert(sma_initialized);

    for (i = 0; i < sma_numseg; i++) {
		unsigned int d_size = (unsigned int)((char *)p - (char *)(sma_shmaddrs[i]));
        if (p >= sma_shmaddrs[i] && d_size < sma_segsize) {
            sma_deallocate(sma_shmaddrs[i], d_size);
            apc_lck_unlock(sma_lock);
            return;
        }
    }

    apc_eprint("apc_sma_free: could not locate address %p", p);
    apc_lck_unlock(sma_lock);
}
/* }}} */

/* {{{ apc_sma_info */
apc_sma_info_t* apc_sma_info()
{
    apc_sma_info_t* info;
    apc_sma_link_t** link;
    int i;

    info = (apc_sma_info_t*) apc_emalloc(sizeof(apc_sma_info_t));
    info->num_seg = sma_numseg;
    info->seg_size = sma_segsize;

    info->list = apc_emalloc(info->num_seg * sizeof(apc_sma_link_t*));
    for (i = 0; i < sma_numseg; i++) {
        info->list[i] = NULL;
    }

    apc_lck_lock(sma_lock);

    /* For each segment */
    for (i = 0; i < sma_numseg; i++) {
        char* shmaddr = sma_shmaddrs[i];
        block_t* prv = BLOCKAT(sizeof(header_t));

        link = &info->list[i];

        /* For each block in this segment */
        while (prv->next != 0) {
            block_t* cur = BLOCKAT(prv->next);

            *link = apc_emalloc(sizeof(apc_sma_link_t));
            (*link)->size = cur->size;
            (*link)->offset = prv->next;
            (*link)->next = NULL;
            link = &(*link)->next;

            prv = cur;
        }
    }

    apc_lck_unlock(sma_lock);
    return info;
}
/* }}} */

/* {{{ apc_sma_free_info */
void apc_sma_free_info(apc_sma_info_t* info)
{
    int i;

    for (i = 0; i < info->num_seg; i++) {
        apc_sma_link_t* p = info->list[i];
        while (p) {
            apc_sma_link_t* q = p;
            p = p->next;
            apc_efree(q);
        }
    }
    apc_efree(info->list);
    apc_efree(info);
}
/* }}} */

/* {{{ apc_sma_get_avail_mem */
int apc_sma_get_avail_mem()
{
    int avail_mem = 0;
    int i;
    
    for (i = 0; i < sma_numseg; i++) {
        header_t* header = (header_t*) sma_shmaddrs[i];
        avail_mem += header->avail;
    }
    return avail_mem;
}
/* }}} */

/* {{{ apc_sma_check_integrity */
void apc_sma_check_integrity()
{
    int i;

    /* For each segment */
    for (i = 0; i < sma_numseg; i++) {
        char* shmaddr = sma_shmaddrs[i];
        header_t* header = (header_t*) shmaddr;
        block_t* prv = BLOCKAT(sizeof(header_t));
        int avail = 0;

        /* For each block in this segment */
        while (prv->next != 0) {
            block_t* cur = BLOCKAT(prv->next);
            avail += cur->size;
            prv = cur;
        }

        assert(avail == header->avail);
    }
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
