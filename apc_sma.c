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

#include "apc_sma.h"
#include "apc_lib.h"
#include "apc_sem.h"
#include "apc_shm.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int sma_initialized = 0;
static int sma_numseg;
static int sma_segsize;
//static int sma_shmid;
//static void* sma_shmaddr;
static int* sma_segments;
static void** sma_shmaddrs;
static int sma_lastseg = 0;
static int sma_lock;

typedef struct header_t header_t;
typedef struct block_t block_t;

struct header_t {
	int segsize;	/* size of entire segment */
	int avail;		/* bytes available (not necessarily contiguous) */
};

struct block_t {
	int size;		/* size of this block */
	int next;		/* offset in segment of next free block */
};

/* The macros BLOCKAT and OFFSET are used for convenience throughout this
 * module. Both assume the presence of a variable shmaddr that points to the
 * beginning of the shared memory segment in question. */

#define BLOCKAT(offset) ((block_t*)((char *)shmaddr + offset))
#define OFFSET(block) ((int)(((char*)block) - (char*)shmaddr))

/* max: return maximum of two integers */
static int max(int a, int b)
{
	return a > b ? a : b;
}

static int sma_allocate(void* shmaddr, int size, int smallblock)
{
	header_t* header;		/* header of shared memory segment */
	block_t* prv;			/* block prior to working block */
	block_t* cur;			/* working block in list */
	block_t* prvbestfit;	/* block before best fit */
	int realsize;			/* actual size of block needed, including header */
	int minsize;			/* for finding best fit */

	/* realsize must be aligned to a word boundary on some architectures */
	realsize = alignword(max(size + sizeof(int), sizeof(block_t)));
	
	/* set realsize to the smallest power of 2 greater than or equal to
	 * realsize. this increases the likelihood that neighboring blocks
	 * can be coalesced, reducing memory fragmentation */
	if (!smallblock) {
		int p = 1;

		while (p < realsize) {
			p <<= 1;
		}
		realsize = p;
	}

	/* first insure that the segment contains at least realsize free
	 * bytes, even if they are not contiguous */
	header = (header_t*) shmaddr;
	if (header->avail < realsize) {
		return -1;
	}

	prvbestfit = 0;		/* initially null (no fit) */
	minsize = INT_MAX;	/* used to find best fit */

	prv = BLOCKAT(sizeof(header_t));
//fprintf(stderr, "start, prv=%d, prv->next=%d\n", prv, prv->next);
	while (prv->next != 0) {
		cur = BLOCKAT(prv->next);
//fprintf(stderr, "prv->next=%d, cur=%d\n", prv->next, cur);
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

	/* update header */
	header->avail -= realsize;

	if (cur->size == realsize) {
		/* cur is a perfect fit for realsize. just unlink it */
		prv->next = cur->next;
	}
	else {
		block_t* nxt;	/* the new block (chopped part of cur) */
		int nxtoffset;	/* offset of the block currently after cur */
		int oldsize;	/* size of cur before split */

		/* bestfit is too big. split it into two smaller blocks */
		nxtoffset = cur->next;
		oldsize = cur->size;
		prv->next += realsize;
		cur->size = realsize;
		nxt = BLOCKAT(prv->next);
		nxt->next = nxtoffset;
		nxt->size = oldsize - realsize;
	}

	return OFFSET(cur) + sizeof(int);
}

static int sma_deallocate(void* shmaddr, int offset)
{
	header_t* header;	/* header of shared memory segment */
	block_t* cur;		/* the new block to insert */
	block_t* prv;		/* the block before cur */
	block_t* nxt;		/* the block after cur */
	int size;			/* size of deallocated block */

	offset -= sizeof(int);
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
	
	/* update header */
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

void apc_sma_init(int numseg, int segsize)
{
	int i;

	if (sma_initialized) {
		return;
	}
	sma_initialized = 1;

	sma_numseg = numseg;
	sma_segsize = segsize;

//	size = sma_numseg*sizeof(int);
//	sma_shmid = apc_shm_create(NULL, 0, size);
//	sma_shmaddr = apc_shm_attach(sma_shmid);
//	memset(sma_shmaddr, 0, size);

	sma_segments = (int*) apc_emalloc(sma_numseg*sizeof(int));
	sma_shmaddrs = (void**) apc_emalloc(sma_numseg*sizeof(void*));

	sma_lock = apc_sem_create(NULL, 0, 1);

	for (i = 0; i < sma_numseg; i++) {
		header_t*	header;
		block_t*	block;
		void*		shmaddr;

		sma_segments[i] = apc_shm_create(NULL, i, sma_segsize);
		sma_shmaddrs[i] = apc_shm_attach(sma_segments[i]);
		shmaddr = sma_shmaddrs[i];
	
		header = (header_t*) shmaddr;
		header->segsize = sma_segsize;
		header->avail = sma_segsize - sizeof(header_t) -
			sizeof(block_t) - sizeof(int);
	
		block = BLOCKAT(sizeof(header_t));
		block->size = 0;
		block->next = sizeof(header_t) + sizeof(block_t);
	
		block = BLOCKAT(block->next);
		block->size = header->avail;
		block->next = 0;
	}
}

void apc_sma_cleanup()
{
	int i;

	assert(sma_initialized);

	for (i = 0; i < sma_numseg; i++) {
		apc_shm_detach(sma_shmaddrs[i]);
	}
	/* FIXME: apc_sma_init(); */
	apc_sem_destroy(sma_lock);
	sma_initialized = 0;
}

//void apc_sma_readlock()
//{
//	assert(sma_initialized);
//	apc_rwl_readlock(sma_lock);
//}
//
//void apc_sma_writelock()
//{
//	assert(sma_initialized);
//	apc_rwl_writelock(sma_lock);
//}
//
//void apc_sma_unlock()
//{
//	assert(sma_initialized);
//	apc_rwl_unlock(sma_lock);
//}

void* apc_sma_malloc(int n)
{
	enum { SMALL_BLOCK = 0 };
	int off;
	int i;

	apc_sem_lock(sma_lock);
	assert(sma_initialized);

	off = sma_allocate(sma_shmaddrs[sma_lastseg], n, SMALL_BLOCK);
	if (off != -1) {
		void* p = sma_shmaddrs[sma_lastseg] + off;
		apc_sem_unlock(sma_lock);
		return p;
	}

	for (i = 0; i < sma_numseg; i++) {
		if (i == sma_lastseg) {
			continue;
		}
		off = sma_allocate(sma_shmaddrs[i], n, SMALL_BLOCK);
		if (off != -1) {
			void* p = sma_shmaddrs[i] + off;
			apc_sem_unlock(sma_lock);
			sma_lastseg = i;
			return p;
		}
	}

	apc_eprint("apc_sma_malloc: unable to allocate %d bytes", n);
	apc_sem_unlock(sma_lock);
	return 0;
}

void apc_sma_free(void* p)
{
	int i;

	apc_sem_lock(sma_lock);
	assert(sma_initialized);

	for (i = 0; i < sma_numseg; i++) {
		if (p >= sma_shmaddrs[i] && (p - sma_shmaddrs[i]) < sma_segsize) {
			sma_deallocate(sma_shmaddrs[i], p - sma_shmaddrs[i]);
			apc_sem_unlock(sma_lock);
			return;
		}
	}

	apc_eprint("apc_sma_free: could not locate address %p", p);
	apc_sem_unlock(sma_lock);
}

void apc_sma_toshared(void* p, int* shmid, int* off)
{
	int i;

	apc_sem_lock(sma_lock);
	assert(sma_initialized);

	for (i = 0; i < sma_numseg; i++) {
		if (p >= sma_shmaddrs[i] && (p - sma_shmaddrs[i]) < sma_segsize) {
			*shmid = sma_segments[i];
			*off = p - sma_shmaddrs[i];
			apc_sem_unlock(sma_lock);
			return;
		}
	}

	apc_eprint("apc_sma_toshared: could not locate address %p", p);
	apc_sem_unlock(sma_lock);
}
	
void* apc_sma_tolocal(int shmid, int off)
{
	int i;

	apc_sem_lock(sma_lock);
	assert(sma_initialized);

	for (i = 0; i < sma_numseg; i++) {
		if (sma_segments[i] == shmid) {
			void* p = sma_shmaddrs[i] + off;
			apc_sem_unlock(sma_lock);
			return p;
		}
	}

	apc_eprint("apc_sma_tolocal: could not locate shmid %d", shmid);
	apc_sem_unlock(sma_lock);
	return 0;
}
