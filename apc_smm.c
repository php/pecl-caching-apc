#include "apc_smm.h"
#include "apc_shm.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct bucket_t bucket_t;
typedef struct header_t header_t;
typedef struct block_t block_t;

struct bucket_t {
	int shmid;		/* shared memory segment id */
	void* shmaddr;	/* starting address of segment (for this process) */
};

struct header_t {
	int segsize;	/* size of entire segment */
	int avail;		/* bytes available (not necessarily contiguous) */
};

struct block_t {
	int size;		/* size of this block */
	int next;		/* offset in segment of next free block */
};

/* these two macros are used for convenience throughout this module.
 * both assume the presence of a variable shmaddr that points to the
 * beginning of the shared memory segment in question */

#define BLOCKAT(offset) ((block_t*)(shmaddr + offset))
#define OFFSET(block) (((void*)block) - shmaddr)

/* only one segment index per process */
#define NUM_BUCKETS 97
static bucket_t segtable[NUM_BUCKETS];
static int initialized = 0;

/* max: return maximum of two integers */
static int max(int a, int b)
{
	return a > b ? a : b;
}

/* hash: return hash value of an integer */
static unsigned int hash(int x)
{
	return x;
}

/* hashtwo: return second hash value of an integer, for open addressing */
static unsigned int hashtwo(int x)
{
	return (x % 53) + 1;
}

/* alignword: returns x, aligned to the system's word boundary */
static int alignword(int x)
{
	typedef union { void* p; int i; long l; double d; } word_t;
	return sizeof(word_t) * (1 + ((x-1)/sizeof(word_t)));
}

/* apc_smm_init: initialize the management system */
void apc_smm_init()
{
	int i;

	memset(segtable, 0, NUM_BUCKETS*sizeof(bucket_t));
	for (i = 0; i < NUM_BUCKETS; i++) {
		segtable[i].shmid   = -1;
		segtable[i].shmaddr = 0;
	}
	initialized = 1;
}

/* apc_smm_initsegment: initialize the specified shared memory segment */
void apc_smm_initsegment(int shmid, int segsize)
{
	void* shmaddr;
	header_t* header;
	block_t* block;

	if (!initialized) {
		apc_smm_init();
	}
	
	shmaddr = apc_smm_attach(shmid);

	/* write header data */
	header = (header_t*) shmaddr;
	header->segsize = segsize;
	header->avail = segsize - sizeof(header_t) - sizeof(block_t) - sizeof(int);

	/* initialize the head block */
	block = BLOCKAT(sizeof(header_t));
	block->size = 0;
	block->next = sizeof(header_t) + sizeof(block_t);

	/* initialize the first block on the free list */
	block = BLOCKAT(block->next);
	block->size = header->avail;
	block->next = 0;
}

/* apc_smm_cleanup: cleans up the index and attached segments, invalidating
 * all pointers into shared memory managed by this system */
void apc_smm_cleanup()
{
	header_t* header;
	int i;

	for (i = 0; i < NUM_BUCKETS; i++) {
		if (segtable[i].shmaddr != 0) {
			header = (header_t*) segtable[i].shmaddr;
			apc_smm_initsegment(segtable[i].shmid, header->segsize);
			apc_shm_detach(segtable[i].shmaddr);
		}
	}
	apc_smm_init();
}

/* apc_smm_attach: return address associated with ID */
void* apc_smm_attach(int shmid)
{
	int i;		/* initial hash value */
	int k;		/* second hash value, for open-addressing */
	int nprobe;	/* running count of table probes */

	i = hash(shmid) % NUM_BUCKETS;
	k = hashtwo(shmid) % NUM_BUCKETS;

	nprobe = 0;
	while (segtable[i].shmid != -1 && nprobe++ < NUM_BUCKETS) {
		if (segtable[i].shmid == shmid) {
			return segtable[i].shmaddr;	/* already attached */
		}
		i = (i+k) % NUM_BUCKETS;
	}
	if (nprobe == NUM_BUCKETS) {
		apc_eprint("smattach: table full!");
	}

	/* attach and insert into table */
	segtable[i].shmid   = shmid;
	segtable[i].shmaddr = apc_shm_attach(shmid);
	return segtable[i].shmaddr;
}

/* apc_smm_detach: don't do anything */
void apc_smm_detach(void* shmaddr)
{
}

/* apc_smm_alloc: return offset to size bytes of contiguous memory, or -1 if
 * not enough memory is available in the segment */
int apc_smm_alloc(void* shmaddr, int size)
{
	header_t* header;		/* header of shared memory segment */
	block_t* prv;			/* block prior to working block */
	block_t* cur;			/* working block in list */
	block_t* prvbestfit;	/* block before best fit */
	int realsize;			/* actual size of block needed, including header */
	int minsize;			/* for finding best fit */

	/* realsize must be aligned to a word boundary on some architectures */
	realsize = alignword(max(size + sizeof(int), sizeof(block_t)));

	/* first insure that the segment contains at least realsize free
	 * bytes, even if they are not contiguous */
	header = (header_t*) shmaddr;
	if (header->avail < realsize) {
		return -1;
	}

	prvbestfit = 0;		/* initially null (no fit) */
	minsize = INT_MAX;	/* used to find best fit */

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

/* apc_smm_free: frees the memory at given offset, which must have been
 * returned by apc_smm_alloc */
void apc_smm_free(void* shmaddr, int offset)
{
	header_t* header;	/* header of shared memory segment */
	block_t* cur;		/* the new block to insert */
	block_t* prv;		/* the block before cur */
	block_t* nxt;		/* the block after cur */

	offset -= sizeof(int);
	if (offset < 0) {	/* reject invalid offsets */
		return;
	}

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

	if (((void*)prv) + prv->size == cur) {
		/* cur and prv share an edge, combine them */
		prv->size += cur->size;
		prv->next = cur->next;
		cur = prv;
	}

	nxt = BLOCKAT(cur->next);
	if (((void*)cur) + cur->size == nxt) {
		/* cur and nxt shared an edge, combine them */
		cur->size += nxt->size;
		cur->next = nxt->next;
	}
}

/* apc_smm_dump: print segment information to file stream */
void apc_smm_dump(apc_outputfn_t outputfn, void* shmaddr)
{
	header_t* header;	/* header of shared memory segment */
	block_t* cur;		/* working block in list */
	int offset;			/* offset of current block */
	int n;				/* index of current block in list */

	/* display header data */
	header = (header_t*) shmaddr;
	outputfn("*** segment information (local addr %p) ***<br>\n", shmaddr);
	outputfn("total size:      %d<br>\n", header->segsize);
	outputfn("total available: %d<br>\n", header->avail);
	outputfn("***<br>\n");

	/* display information about each block */
	offset = sizeof(header_t);
	n      = 0;
	cur    = BLOCKAT(offset);
	do {
		outputfn("block %d: offset=%d, size=%d, next=%d<br>\n",
			n++, offset, cur->size, cur->next);
		offset = cur->next;
		cur = BLOCKAT(cur->next);
	} while (offset != 0);

	outputfn("*** end ***<br>\n");
}
