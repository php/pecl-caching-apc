/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
  |          Rasmus Lerdorf <rasmus@php.net>                             |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$ */

#include "apc_sma.h"
#include "apc.h"
#include "apc_globals.h"
#include "apc_lock.h"
#include "apc_shm.h"
#include <limits.h>
#if APC_MMAP
void *apc_mmap(char *file_mask, int size);
void apc_unmap(void* shmaddr, int size);
#endif

/* {{{ locking macros */
#define LOCK(c)         { HANDLE_BLOCK_INTERRUPTIONS(); apc_lck_lock(c); }
#define RDLOCK(c)       { HANDLE_BLOCK_INTERRUPTIONS(); apc_lck_rdlock(c); }
#define UNLOCK(c)       { apc_lck_unlock(c); HANDLE_UNBLOCK_INTERRUPTIONS(); }
/* }}} */

enum { DEFAULT_NUMSEG=1, DEFAULT_SEGSIZE=30*1024*1024 };

static int sma_initialized = 0;     /* true if the sma has been initialized */
static unsigned int sma_numseg;     /* number of shm segments to allow */
static size_t sma_segsize;          /* size of each shm segment */
static int* sma_segments;           /* array of shm segment ids */
static void** sma_shmaddrs;         /* array of shm segment addresses */
static int sma_lastseg = 0;         /* index of MRU segment */

typedef struct header_t header_t;
struct header_t {
    apc_lck_t sma_lock;     /* segment lock, MUST BE ALIGNED for futex locks */
    size_t segsize;         /* size of entire segment */
    size_t avail;           /* bytes available (not necessarily contiguous) */
    size_t nfoffset;        /* start next fit search from this offset       */
#if ALLOC_DISTRIBUTION
    size_t adist[30];
#endif
};


/* do not enable for threaded http servers */
/* #define __APC_SMA_DEBUG__ 1 */

#ifdef __APC_SMA_DEBUG__
/* global counter for identifying blocks 
 * Technically it is possible to do the same
 * using offsets, but double allocations of the
 * same offset can happen. */
static volatile size_t block_id = 0;
#endif

typedef struct block_t block_t;
struct block_t {
    size_t size;       /* size of this block */
    size_t next;       /* offset in segment of next free block */
    size_t canary;     /* canary to check for memory overwrites */
#ifdef __APC_SMA_DEBUG__
    size_t id;         /* identifier for the memory block */ 
#endif
};

/* The macros BLOCKAT and OFFSET are used for convenience throughout this
 * module. Both assume the presence of a variable shmaddr that points to the
 * beginning of the shared memory segment in question. */

#define BLOCKAT(offset) ((block_t*)((char *)shmaddr + offset))
#define OFFSET(block) ((size_t)(((char*)block) - (char*)shmaddr))

/* Canary macros for setting, checking and resetting memory canaries */
#define SET_CANARY(v) (v)->canary = 0x42424242
#define CHECK_CANARY(v) assert((v)->canary == 0x42424242)
#define RESET_CANARY(v) (v)->canary = -42

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
static int sma_allocate(void* shmaddr, size_t size)
{
    header_t* header;       /* header of shared memory segment */
    block_t* prv;           /* block prior to working block */
    block_t* cur;           /* working block in list */
    block_t* prvnextfit;    /* block before next fit */
    size_t realsize;        /* actual size of block needed, including header */
    size_t last_offset;     /* save the last search offset */
    int wrapped=0;
    size_t block_size = alignword(sizeof(struct block_t));

    realsize = alignword(size + block_size);

    /*
     * First, insure that the segment contains at least realsize free bytes,
     * even if they are not contiguous.
     */
    header = (header_t*) shmaddr;
    if (header->avail < realsize) {
        return -1;
    }

    prvnextfit = 0;     /* initially null (no fit) */
    last_offset = 0;

    /* If we have a next fit offset, start searching from there */
    if(header->nfoffset) {
        prv = BLOCKAT(header->nfoffset);
    } else {    
        prv = BLOCKAT(sizeof(header_t));
    }
   
    CHECK_CANARY(prv);

    while (prv->next != header->nfoffset) {
        cur = BLOCKAT(prv->next);
#ifdef __APC_SMA_DEBUG__
        CHECK_CANARY(cur);
#endif
        /* If it fits perfectly or it fits after a split, stop searching */
        if (cur->size == realsize || (cur->size > (block_size + realsize))) {
            prvnextfit = prv;
            break;
        }
        last_offset = prv->next;
        prv = cur;
        if(wrapped && (prv->next >= header->nfoffset)) break;

        /* Check to see if we need to wrap around and search from the top */
        if(header->nfoffset && prv->next == 0) {
            prv = BLOCKAT(sizeof(header_t));
#ifdef __APC_SMA_DEBUG__
            CHECK_CANARY(prv);
#endif
            last_offset = 0;
            wrapped = 1;
        } 
    }

    if (prvnextfit == 0) {
        header->nfoffset = 0;
        return -1;
    }

    prv = prvnextfit;
    cur = BLOCKAT(prv->next);

    CHECK_CANARY(prv);
    CHECK_CANARY(cur);

    /* update the block header */
    header->avail -= realsize;
#if ALLOC_DISTRIBUTION
    header->adist[(int)(log(size)/log(2))]++;
#endif

    if (cur->size == realsize) {
        /* cur is a perfect fit for realsize; just unlink it */
        prv->next = cur->next;
    }
    else {
        block_t* nxt;      /* the new block (chopped part of cur) */
        size_t nxtoffset;  /* offset of the block currently after cur */
        size_t oldsize;    /* size of cur before split */

        /* nextfit is too big; split it into two smaller blocks */
        nxtoffset = cur->next;
        oldsize = cur->size;
        prv->next += realsize;  /* skip over newly allocated block */
        cur->size = realsize;   /* Set the size of this new block */
        nxt = BLOCKAT(prv->next);
        nxt->next = nxtoffset;  /* Re-link the shortened block */
        nxt->size = oldsize - realsize;  /* and fix the size */
        SET_CANARY(nxt);
#ifdef __APC_SMA_DEBUG__
        nxt->id = -1;
#endif
    }
    header->nfoffset = last_offset;

    SET_CANARY(cur);
#ifdef __APC_SMA_DEBUG__
    cur->id = ++block_id;
    fprintf(stderr, "allocate(realsize=%d,size=%d,id=%d)\n", (int)(size), (int)(cur->size), cur->id);
#endif

    return OFFSET(cur) + block_size;
}
/* }}} */

/* {{{ sma_deallocate: deallocates the block at the given offset */
static int sma_deallocate(void* shmaddr, int offset)
{
    header_t* header;   /* header of shared memory segment */
    block_t* cur;       /* the new block to insert */
    block_t* prv;       /* the block before cur */
    block_t* nxt;       /* the block after cur */
    size_t size;        /* size of deallocated block */

    offset -= alignword(sizeof(struct block_t));
    assert(offset >= 0);

    /* find position of new block in free list */
    cur = BLOCKAT(offset);
    prv = BLOCKAT(sizeof(header_t));
   
    CHECK_CANARY(cur);

#ifdef __APC_SMA_DEBUG__
    CHECK_CANARY(prv);
    fprintf(stderr, "free(%p, size=%d,id=%d)\n", cur, (int)(cur->size), cur->id);
#endif
    while (prv->next != 0 && prv->next < offset) {
        prv = BLOCKAT(prv->next);
#ifdef __APC_SMA_DEBUG__
        CHECK_CANARY(prv);
#endif
    }
    
    CHECK_CANARY(prv);

    /* insert new block after prv */
    cur->next = prv->next;
    prv->next = offset;

#ifdef __APC_SMA_DEBUG__
    CHECK_CANARY(cur);
    cur->id = -1;
#endif
    
    /* update the block header */
    header = (header_t*) shmaddr;
    header->avail += cur->size;
    size = cur->size;

    if (((char *)prv) + prv->size == (char *) cur) {
        /* cur and prv share an edge, combine them */
        prv->size += cur->size;
        prv->next = cur->next;
        RESET_CANARY(cur);
        cur = prv;
    }

    nxt = BLOCKAT(cur->next);

    if (((char *)cur) + cur->size == (char *) nxt) {
        /* cur and nxt shared an edge, combine them */
        cur->size += nxt->size;
        cur->next = nxt->next;
#ifdef __APC_SMA_DEBUG__
        CHECK_CANARY(nxt);
        nxt->id = -1; /* assert this or set it ? */
#endif
        RESET_CANARY(nxt);
    }
    header->nfoffset = 0;  /* Reset the next fit search marker */

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
    
    for (i = 0; i < sma_numseg; i++) {
        header_t*   header;
        block_t*    block;
        void*       shmaddr;

#if APC_MMAP
        sma_segments[i] = sma_segsize;
        sma_shmaddrs[i] = apc_mmap(mmap_file_mask, sma_segsize);
        if(sma_numseg != 1) memcpy(&mmap_file_mask[strlen(mmap_file_mask)-6], "XXXXXX", 6);
#else
        sma_segments[i] = apc_shm_create(NULL, i, sma_segsize);
        sma_shmaddrs[i] = apc_shm_attach(sma_segments[i]);
#endif
        shmaddr = sma_shmaddrs[i];
    
        header = (header_t*) shmaddr;
        apc_lck_create(NULL, 0, 1, header->sma_lock);
        header->segsize = sma_segsize;
        header->avail = sma_segsize - sizeof(header_t) - sizeof(block_t) -
                        alignword(sizeof(int));
        header->nfoffset = 0;
#if ALLOC_DISTRIBUTION
       	{
           int j;
           for(j=0; j<30; j++) header->adist[j] = 0; 
        }
#endif 
        block = BLOCKAT(sizeof(header_t));
        block->size = 0;
        block->next = sizeof(header_t) + sizeof(block_t);
        SET_CANARY(block);
#ifdef __APC_SMA_DEBUG__
        block->id = -1;
#endif
        block = BLOCKAT(block->next);
        block->size = header->avail;
        block->next = 0;
        SET_CANARY(block);
#ifdef __APC_SMA_DEBUG__
        block->id = -1;
#endif
    }
}
/* }}} */

/* {{{ apc_sma_cleanup */
void apc_sma_cleanup()
{
    int i;

    assert(sma_initialized);

    for (i = 0; i < sma_numseg; i++) {
        apc_lck_destroy(((header_t*)sma_shmaddrs[i])->sma_lock);
#if APC_MMAP
        apc_unmap(sma_shmaddrs[i], sma_segments[i]);
#else
        apc_shm_detach(sma_shmaddrs[i]);
#endif
    }
    sma_initialized = 0;
    apc_efree(sma_segments);
    apc_efree(sma_shmaddrs);
}
/* }}} */

/* {{{ apc_sma_malloc */
void* apc_sma_malloc(size_t n)
{
    int off;
    int i;

    TSRMLS_FETCH();
    assert(sma_initialized);
    LOCK(((header_t*)sma_shmaddrs[sma_lastseg])->sma_lock);

    off = sma_allocate(sma_shmaddrs[sma_lastseg], n);
    if (off != -1) {
        void* p = (void *)(((char *)(sma_shmaddrs[sma_lastseg])) + off);
        if (APCG(mem_size_ptr) != NULL) { *(APCG(mem_size_ptr)) += n; }
        UNLOCK(((header_t*)sma_shmaddrs[sma_lastseg])->sma_lock);
        return p;
    }
    UNLOCK(((header_t*)sma_shmaddrs[sma_lastseg])->sma_lock);

    for (i = 0; i < sma_numseg; i++) {
        LOCK(((header_t*)sma_shmaddrs[i])->sma_lock);
        if (i == sma_lastseg) {
            continue;
        }
        off = sma_allocate(sma_shmaddrs[i], n);
        if (off != -1) {
            void* p = (void *)(((char *)(sma_shmaddrs[i])) + off);
            if (APCG(mem_size_ptr) != NULL) { *(APCG(mem_size_ptr)) += n; }
            UNLOCK(((header_t*)sma_shmaddrs[i])->sma_lock);
            sma_lastseg = i;
            return p;
        }
        UNLOCK(((header_t*)sma_shmaddrs[i])->sma_lock);
    }

    return NULL;
}
/* }}} */

/* {{{ apc_sma_realloc */
void* apc_sma_realloc(void *p, size_t n)
{
    apc_sma_free(p);
    return apc_sma_malloc(n);
}
/* }}} */

/* {{{ apc_sma_strdup */
char* apc_sma_strdup(const char* s)
{
    void* q;
    int len;

    if(!s) return NULL;

    len = strlen(s)+1;
    q = apc_sma_malloc(len);
    if(!q) return NULL;
    memcpy(q, s, len);
    return q;
}
/* }}} */

/* {{{ apc_sma_free */
void apc_sma_free(void* p)
{
    int i;
    size_t offset;
    size_t d_size;
    TSRMLS_FETCH();

    if (p == NULL) {
        return;
    }

    assert(sma_initialized);

    for (i = 0; i < sma_numseg; i++) {
        LOCK(((header_t*)sma_shmaddrs[i])->sma_lock);
        offset = (size_t)((char *)p - (char *)(sma_shmaddrs[i]));
        if (p >= sma_shmaddrs[i] && offset < sma_segsize) {
            d_size = sma_deallocate(sma_shmaddrs[i], offset);
            if (APCG(mem_size_ptr) != NULL) { *(APCG(mem_size_ptr)) -= d_size; }
            UNLOCK(((header_t*)sma_shmaddrs[i])->sma_lock);
            return;
        }
        UNLOCK(((header_t*)sma_shmaddrs[i])->sma_lock);
    }

    apc_eprint("apc_sma_free: could not locate address %p", p);
}
/* }}} */

/* {{{ apc_sma_info */
apc_sma_info_t* apc_sma_info(zend_bool limited)
{
    apc_sma_info_t* info;
    apc_sma_link_t** link;
    int i;
	char* shmaddr;
	block_t* prv;
	
    if (!sma_initialized) {
        return NULL;
    }

    info = (apc_sma_info_t*) apc_emalloc(sizeof(apc_sma_info_t));
    info->num_seg = sma_numseg;
    info->seg_size = sma_segsize - sizeof(header_t) - sizeof(block_t) - alignword(sizeof(int));

    info->list = apc_emalloc(info->num_seg * sizeof(apc_sma_link_t*));
    for (i = 0; i < sma_numseg; i++) {
        info->list[i] = NULL;
    }

    if(limited) return info;

    /* For each segment */
    for (i = 0; i < sma_numseg; i++) {
        RDLOCK(((header_t*)sma_shmaddrs[i])->sma_lock);
        shmaddr = sma_shmaddrs[i];
        prv = BLOCKAT(sizeof(header_t));

        link = &info->list[i];

        /* For each block in this segment */
        while (prv->next != 0) {
            block_t* cur = BLOCKAT(prv->next);
#ifdef __APC_SMA_DEBUG__
            CHECK_CANARY(cur);
#endif

            *link = apc_emalloc(sizeof(apc_sma_link_t));
            (*link)->size = cur->size;
            (*link)->offset = prv->next;
            (*link)->next = NULL;
            link = &(*link)->next;

            prv = cur;
        }
        UNLOCK(((header_t*)sma_shmaddrs[i])->sma_lock);
    }

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

#if ALLOC_DISTRIBUTION
size_t *apc_sma_get_alloc_distribution(void) {
    header_t* header = (header_t*) sma_shmaddrs[0];
    return header->adist; 
}
#endif

#if 0
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
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
