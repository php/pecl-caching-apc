#include "apc_cache.h"
#include "apc_shm.h"
#include "apc_smm.h"
#include "apc_rwlock.h"
#include "apc_crc32.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAX_KEY_LEN	128		/* must be >= maximum path length */

enum { EMPTY = -1, UNUSED = -2 };

typedef struct segment_t segment_t;
struct segment_t {
	int shmid;	/* shared memory id of the segment */
};

/* bucket_t. When a bucket is empty, its shmid is equal to EMPTY. After a
 * bucket is removed, its shmid is set to UNUSED, which is considered
 * non-empty when searching, but empty when inserting. Both values are
 * guaranteed to be less than zero */
typedef struct bucket_t bucket_t;
struct bucket_t {
	char key[MAX_KEY_LEN+1];	/* bucket key */
	int shmid;					/* shm segment where data is stored */
	int offset;					/* pointer to data in shm segment */
	int length;					/* length of stored data in bytes */
	int lastaccess;				/* time of last access (unix timestamp) */
	int hitcount;				/* number of hits to this bucket */
	int expiretime;				/* time of expiration (Unix timestamp) */
	unsigned int checksum;		/* checksum of stored data */
};

typedef struct header_t header_t;
struct header_t {
	int magic;		/* magic number, indicates initialization state */
	int nbuckets;	/* number of buckets in cache */
	int maxseg;		/* maximum number of segments for cached data */
	int segsize;	/* size of each shared memory segment */
	int ttl;		/* time to live for cache entries */
	int hits;		/* total successful hits in cache */
	int misses;		/* total unsuccessful hits in cache */
};

/*typedef struct apc_cache_t apc_cache_t;*/
struct apc_cache_t {
	header_t* header;	/* cache header, stored in shared memory */
	char* pathname;		/* pathname used to create cache */
	apc_rwlock_t* lock;	/* readers-writer lock for entire cache */
	int shmid;			/* shared memory segment of cache */
	void* shmaddr;		/* process (local) address of cache shm segment */
	segment_t* segments;/* start of segment_t array */
	bucket_t* buckets;	/* start of bucket_t array */
};

#define MAGIC_INIT 0xD1A5B8E2

/* computecachesize: compute size of cache, given nbuckets and maxseg */
static int computecachesize(int nbuckets, int maxseg)
{
	return sizeof(header_t) + maxseg*sizeof(segment_t) +
		nbuckets*sizeof(bucket_t);
}

/* initcache: perform full initialization of the cache. should execute once
 * for all processes (not for each process) */
static void initcache(apc_cache_t* cache, const char* pathname,
	int nbuckets, int maxseg, int segsize, int ttl)
{
	int cachesize;	/* total size of the cache */
	header_t* header;
	int i;

	cachesize = computecachesize(nbuckets, maxseg);

	memset(cache->shmaddr, 0, cachesize);
	header = cache->header;
	header->magic    = MAGIC_INIT;
	header->nbuckets = nbuckets;
	header->maxseg   = maxseg;
	header->segsize  = segsize;
	header->ttl      = ttl;
	header->hits     = 0;
	header->misses   = 0;

	/* initialize all the buckets */
	for (i = 0; i < nbuckets; i++) {
		cache->buckets[i].shmid = EMPTY;
	}

	/* create the first shared memory segment */
	cache->segments[0].shmid  = apc_shm_create(pathname, 1, segsize);
	apc_smm_initsegment(cache->segments[0].shmid, segsize);
}

/* resetcache: clears all entries from the cache without destroying existing
 * shared memory segments */
static void resetcache(apc_cache_t* cache)
{
	int nbuckets;
	int maxseg;
	int segsize;
	int i;

	/* reset buckets */
	nbuckets = cache->header->nbuckets;
	for (i = 0; i < nbuckets; i++) {
		if (cache->buckets[i].shmid >= 0) {
			void* shmaddr = (char*) apc_smm_attach(cache->buckets[i].shmid);
			apc_smm_free(shmaddr, cache->buckets[i].offset);
			cache->buckets[i].shmid = EMPTY;
		}
	}

	/* reset shared memory segments */
	maxseg  = cache->header->maxseg;
	segsize = cache->header->segsize;
	for (i = 0; i < maxseg; i++) {
		if (cache->segments[i].shmid == 0) {
			break;
		}
		apc_smm_initsegment(cache->segments[i].shmid, segsize);
	}
}

/* hash: compute hash value of a string */
static unsigned int hash(const char* v)
{
	unsigned int h = 0;
	for (; *v != 0; v++) {
		h = 127*h + *v;
	}
	return h;
}

/* hashtwo: second hash function for double hashing */
static unsigned int hashtwo(const char* v)
{
	unsigned int h = 0;
	for (; *v != 0; v++) {
		h = 37*h + *v;
	}
	return (h % 97) + 1; /* works well when cache size is <97 */
}

/* apc_cache_create: */
apc_cache_t* apc_cache_create(const char* pathname, int nbuckets,
	int maxseg, int segsize, int ttl)
{
	apc_cache_t* cache;
	int cachesize;

	cache = (apc_cache_t*) apc_emalloc(sizeof(apc_cache_t));
	cachesize = computecachesize(nbuckets, maxseg);

	/* per-process initialization */
	cache->pathname = (char*) apc_estrdup(pathname);
	cache->lock     = apc_rwl_create(pathname);
	cache->shmid    = apc_shm_create(pathname, 0, cachesize);
	cache->shmaddr  = apc_shm_attach(cache->shmid);
	cache->header   = (header_t*) cache->shmaddr;

	cache->segments = (segment_t*) (cache->shmaddr + sizeof(header_t));
	cache->buckets =  (bucket_t*)  (cache->shmaddr + sizeof(header_t) +
	                                maxseg*sizeof(segment_t));

	/* instruct the OS to destroy the shm segment as soon as no processes
	 * are attached to it */
	apc_shm_destroy(cache->shmid);

	/* perform full initialization if necessary */
	if (cache->header->magic != MAGIC_INIT) {
		apc_rwl_writelock(cache->lock);
		if (cache->header->magic != MAGIC_INIT) {
			/* cache not initialized */
			initcache(cache, pathname, nbuckets, maxseg, segsize, ttl);
		}
		apc_rwl_unlock(cache->lock);
	}

	return cache;
}

/* apc_cache_destroy: destroys a cache */
void apc_cache_destroy(apc_cache_t* cache)
{
	int i;
	int maxseg;

	apc_rwl_writelock(cache->lock);

	/* first, destroy the shared memory segments */
	maxseg = cache->header->maxseg;
	for (i = 0; i < maxseg; i++) {
		if (cache->segments[i].shmid != 0) {
			apc_shm_destroy(cache->segments[i].shmid);
		}
	}
	apc_shm_detach(cache->shmaddr);
	apc_shm_destroy(cache->shmid);

	apc_rwl_unlock(cache->lock);	/* there is a race condition here */
	apc_rwl_destroy(cache->lock);

	apc_efree(cache);
}

/* apc_cache_clear: clears the cache */
void apc_cache_clear(apc_cache_t* cache)
{
	apc_rwl_writelock(cache->lock);
	resetcache(cache);
	apc_rwl_unlock(cache->lock);
}

/* apc_cache_search: return 1 if key exists in cache, else 0 */
int apc_cache_search(apc_cache_t* cache, const char* key)
{
	unsigned slot;		/* initial hash value */
	unsigned k;			/* second hash value, for open-addressing */
	int nprobe;			/* running count of cache probes */
	bucket_t* buckets;
	int nbuckets;

	//apc_rwl_writelock(cache->lock);
	apc_rwl_readlock(cache->lock);

	buckets  = cache->buckets;
	nbuckets = cache->header->nbuckets;

	slot = hash(key) % nbuckets;
	k = hashtwo(key) % nbuckets;

	nprobe = 0;
	while (buckets[slot].shmid != EMPTY && nprobe++ < nbuckets) {
		if (buckets[slot].shmid == UNUSED) {
			continue;
		}
		if (strcmp(buckets[slot].key, key) == 0) {
			if (time(0) > buckets[slot].expiretime) {
				break; /* the entry has expired */
			}
			apc_rwl_unlock(cache->lock);
			return 1;
		}
		slot = (slot+k) % nbuckets;
	}
	apc_rwl_unlock(cache->lock);
	return 0; /* not found */
}

/* apc_cache_retrieve: */
int apc_cache_retrieve(apc_cache_t* cache, const char* key, char** dataptr,
	int* length, int* maxsize)
{
	unsigned slot;		/* initial hash value */
	unsigned k;			/* second hash value, for open-addressing */
	int nprobe;			/* running count of cache probes */
	char* shmaddr;		/* attached addr of data segment */
	bucket_t* buckets;
	int nbuckets;
	unsigned int checksum;
	int curtime;

	//apc_rwl_writelock(cache->lock);
	apc_rwl_readlock(cache->lock);

	buckets  = cache->buckets;
	nbuckets = cache->header->nbuckets;

	slot = hash(key) % nbuckets;
	k = hashtwo(key) % nbuckets;

	nprobe = 0;
	while (buckets[slot].shmid != EMPTY && nprobe++ < nbuckets) {
		if (buckets[slot].shmid == UNUSED) {
			continue;
		}
		if (strcmp(buckets[slot].key, key) == 0) {
			if ((curtime = time(0)) > buckets[slot].expiretime) {
				break; /* the entry has expired */
			}
			shmaddr = (char*) apc_smm_attach(buckets[slot].shmid);
			*length = buckets[slot].length;
			if (*maxsize < *length) {
				/* dataptr is too small, so expand it */
				*maxsize = *length;
				*dataptr = realloc(*dataptr, *maxsize);
			}
			memcpy(*dataptr, shmaddr + buckets[slot].offset, *length);

			/* update the cache */
			cache->header->hits++;
			buckets[slot].lastaccess = curtime;
			buckets[slot].hitcount++;
			checksum = buckets[slot].checksum;

			/* compare checksums */
			if (checksum != apc_crc32(*dataptr, *length)) {
				apc_eprint("checksum failed! data length is %d\n", *length);
				return 0; /* return failure */
			}

			/* we're done */
			apc_rwl_unlock(cache->lock);
			return 1;
		}
		slot = (slot+k) % nbuckets;
	}
	cache->header->misses++;
	apc_rwl_unlock(cache->lock);
	return 0;
}

/* apc_cache_insert: */
int apc_cache_insert(apc_cache_t* cache, const char* key,
	const char* data, int size)
{
	int i;
	unsigned slot;		/* initial hash value */
	unsigned k;			/* second hash value, for open-addressing */
	int nprobe;			/* running count of cache probes */
	char* shmaddr;		/* attached addr of current segment */
	bucket_t* buckets;
	int nbuckets;
	segment_t* segments;
	int maxseg;
	int segsize;
	int offset;
	unsigned int checksum;

	/* compute checksum of data (before locking) */
	checksum = apc_crc32(data, size);

	apc_rwl_writelock(cache->lock);
	
	/* copy these values out of shared memory, for convenience */
	buckets  = cache->buckets;
	nbuckets = cache->header->nbuckets;
	segments = cache->segments;
	maxseg   = cache->header->maxseg;
	segsize  = cache->header->segsize;

	slot = hash(key) % nbuckets;
	k = hashtwo(key) % nbuckets;

	nprobe = 0;
	while (buckets[slot].shmid >= 0 && nprobe++ < nbuckets) {
		if (strcmp(buckets[slot].key, key) == 0) {
			break;	/* overwrite existing entry */
		}
		slot = (slot+k) % nbuckets;
	}
	if (nprobe == nbuckets) {	/* did we find a slot? */
		apc_rwl_unlock(cache->lock);	/* no, return failure */
		return -1;
	}

	shmaddr = 0;
	offset  = 0;
	for (i = 0; i < maxseg; i++) {
		if (segments[i].shmid == 0) { /* segment not initialized */
			segments[i].shmid = apc_shm_create(cache->pathname, i+1, segsize);
			apc_smm_initsegment(segments[i].shmid, segsize);
		}
		shmaddr = apc_smm_attach(segments[i].shmid);
		offset = apc_smm_alloc(shmaddr, size);
		if (offset >= 0) {
			break;
		}
	}
	if (i == maxseg) {
		/* not enough shared memory available */
		apc_rwl_unlock(cache->lock);
		return -1;
	}

	/* update the cache */
	strncpy(buckets[slot].key, key, MAX_KEY_LEN);
	buckets[slot].shmid      = segments[i].shmid;
	buckets[slot].offset     = offset;
	buckets[slot].length     = size;
	buckets[slot].lastaccess = time(0);
	buckets[slot].hitcount   = 0;
	buckets[slot].expiretime = buckets[slot].lastaccess + cache->header->ttl;
	buckets[slot].checksum   = checksum;

	/* store data in segment and update its record */
	memcpy(shmaddr + offset, data, size);

	apc_rwl_unlock(cache->lock);
	return 0;
}

/* apc_cache_remove: */
int apc_cache_remove(apc_cache_t* cache, const char* key)
{
	unsigned slot;		/* initial hash value */
	unsigned k;			/* second hash value, for open-addressing */
	int nprobe;			/* running count of cache probes */
	char* shmaddr;		/* attached addr of data segment */
	bucket_t* buckets;
	int nbuckets;

	apc_rwl_writelock(cache->lock);

	buckets  = cache->buckets;
	nbuckets = cache->header->nbuckets;

	slot = hash(key) % nbuckets;
	k = hashtwo(key) % nbuckets;

	nprobe = 0;
	while (buckets[slot].shmid != EMPTY && nprobe++ < nbuckets) {
		if (buckets[slot].shmid == UNUSED) {
			continue;
		}
		if (strcmp(buckets[slot].key, key) == 0) {
			/* found the key */
			shmaddr = (char*) apc_smm_attach(buckets[slot].shmid);
			apc_smm_free(shmaddr, buckets[slot].offset);
			buckets[slot].shmid = UNUSED;
			apc_rwl_unlock(cache->lock);
			return 1;
		}
		slot = (slot+k) % nbuckets;
	}
	apc_rwl_unlock(cache->lock);
	return 0;	/* not found */
}

/* apc_cache_dump: */
void apc_cache_dump(apc_outputfn_t outputfn, apc_cache_t* cache)
{
	int i;

	//apc_rwl_writelock(cache->lock);
	apc_rwl_readlock(cache->lock);
	outputfn("<html>\n<head>\n\t<title>APC-SHM Cache Info\n</title>\n</head>\n");
	outputfn("*** begin cache info ***<br>\n");
	outputfn("header.magic: %x<br>\n", cache->header->magic);
	outputfn("header.nbuckets: %d<br>\n", cache->header->nbuckets);
	outputfn("header.maxseg: %d<br>\n", cache->header->maxseg);
	outputfn("header.segsize: %d<br>\n", cache->header->segsize);
	outputfn("header.ttl: %d<br>\n", cache->header->ttl);
	outputfn("header.hits: %d<br>\n", cache->header->hits);
	outputfn("header.misses: %d<br>\n", cache->header->misses);
	outputfn("<br>\n");
	outputfn("shmid is %d (local addr %p)<br>\n", cache->shmid, cache->shmaddr);
	outputfn("creation pathname: '%s'<br>\n", cache->pathname);
	outputfn("<br>\n");

	outputfn("<table BORDER=0 CELLSPACING=0 CELLPADDING=0 WIDTH=\"98%\"
            BGCOLOR=\"#006666\" >");
	outputfn("<tr BGCOLOR=\"#0000FF\">\n");
	outputfn("<td>Bucket</td><td>Key</td><td>offset</td><td>length</td><td>
					lastaccess</td><td>hitcount</td><td>expiretime</td><td>checksum</td>\n");
	outputfn("</tr>\n");
	for (i = 0; i < cache->header->nbuckets; i++) {
		if (cache->buckets[i].shmid >= 0) {
			outputfn("<tr>\n");
			outputfn("<td>%d</td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td>
						<td>%d</td><td>%u</td>\n", i, cache->buckets[i].key, 
						cache->buckets[i].offset, cache->buckets[i].length,
						cache->buckets[i].lastaccess, cache->buckets[i].hitcount, 
						cache->buckets[i].expiretime, cache->buckets[i].checksum);
			outputfn("</tr>\n");
/*
			outputfn("=> bucket %d (key='%s', h%%N=%u, h2%%N=%u)<br>\n",
				i, cache->buckets[i].key,
				hash(cache->buckets[i].key) % cache->header->nbuckets,
				hashtwo(cache->buckets[i].key) % cache->header->nbuckets);
			outputfn("   bucket %d (offset=%d, length=%d, lastaccess=%d)<br>\n",
				i, cache->buckets[i].offset, cache->buckets[i].length,
				cache->buckets[i].lastaccess);
			outputfn("   bucket %d (hitcount=%d, expiretime=%d, checksum=%u)<br>\n",
				i, cache->buckets[i].hitcount, cache->buckets[i].expiretime,
				cache->buckets[i].checksum);
*/
		}
	}
	outputfn("</tr>\n");
//	outputfn("<br>\n");
	outputfn("<tr BGCOLOR=\"#0000FF\">\n");
	outputfn("<center><td>Cache segment info</td></tr>\n");
	for (i = 0; i < cache->header->maxseg; i++) {
		if (cache->segments[i].shmid > 0) {
			outputfn("=> contents of segment %d:<br>\n", i);
			apc_smm_dump(outputfn, apc_smm_attach(cache->segments[i].shmid));
		}
	}

//	outputfn("*** end cache info ***<br>\n");

	apc_rwl_unlock(cache->lock);
}
