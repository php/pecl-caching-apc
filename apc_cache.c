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


#include "apc_cache.h"
#include "apc_crc32.h"
#include "apc_nametable.h"
#include "apc_rwlock.h"
#include "apc_sem.h"
#include "apc_serialize.h"
#include "apc_shm.h"
#include "apc_smm.h"
#include "php_apc.h"
#include "zend.h"
#include "zend_hash.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define USE_RWLOCK	/* synchronize the cache with a readers-writer lock;
                     * this should be more efficient in many cases, and
					 * where it is not necessary, its extra overhead is
					 * not significant */

enum { MAX_KEY_LEN = 256 };			/* must be >= maximum path length */
enum { DO_CHECKSUM = 0 };			/* if this is true, perform checksums */

extern zend_apc_globals apc_globals;

typedef struct segment_t segment_t;
struct segment_t {
	int shmid;	/* shared memory id of the segment */
};

/* bucket_t. When a bucket is empty, its shmid is equal to EMPTY. After a
 * bucket is removed, its shmid is set to UNUSED, which is considered
 * non-empty when searching, but empty when inserting. Both values are
 * guaranteed to be less than zero */
enum { EMPTY = -1, UNUSED = -2 };
typedef struct bucket_t bucket_t;
struct bucket_t {
	char key[MAX_KEY_LEN+1];	/* bucket key */
	int shmid;					/* shm segment where data is stored */
	int offset;					/* pointer to data in shm segment */
	int length;					/* length of stored data in bytes */
	int lastaccess;				/* time of last access (unix timestamp) */
	int hitcount;				/* number of hits to this bucket */
	int createtime;				/* time of creation (Unix timestamp) */
	int ttl;					/* private time-to-live */
	int mtime;					/* modification time of the source file */
	unsigned int checksum;		/* checksum of stored data */
};

typedef struct header_t header_t;
struct header_t {
	int magic;		/* magic number, indicates initialization state */
	int nbuckets;	/* number of buckets in cache */
	int maxseg;		/* maximum number of segments for cached data */
	int segsize;	/* size of each shared memory segment */
	int ttl;		/* default time to live for cache entries */
	int hits;		/* total successful hits in cache */
	int misses;		/* total unsuccessful hits in cache */
};

struct apc_cache_t {
	header_t* header;	/* cache header, stored in shared memory */
	char* pathname;		/* pathname used to create cache */
  #ifdef USE_RWLOCK
	apc_rwlock_t* lock;	/* readers-writer lock for entire cache */
  #else
	int lock;			/* binary semaphore lock */
  #endif
	int shmid;			/* shared memory segment of cache */
	void* shmaddr;		/* process (local) address of cache shm segment */
	segment_t* segments;/* start of segment_t array */
	bucket_t* buckets;	/* start of bucket_t array */
};


/* if USE_RWLOCK is defined, a readers-write lock (defined in apc_rwlock.c)
 * will be used to synchronize the shared cache. otherwise, a simple binary
 * semaphore will be used. */

#ifdef USE_RWLOCK
# define READLOCK(lock)  apc_rwl_readlock(lock)
# define WRITELOCK(lock) apc_rwl_writelock(lock)
# define UNLOCK(lock)    apc_rwl_unlock(lock)
#else
# define READLOCK(lock)  apc_sem_lock(lock)
# define WRITELOCK(lock) apc_sem_lock(lock)
# define UNLOCK(lock)    apc_sem_unlock(lock)
#endif

enum { MAGIC_INIT = 0xD1A5B8E2 };	/* magic initialization value */

/* computecachesize: compute size of cache, given nbuckets and maxseg */
static int computecachesize(int nbuckets, int maxseg)
{
	return sizeof(header_t) + maxseg*sizeof(segment_t) +
		nbuckets*sizeof(bucket_t);
}

/* isexpired: return true if bucket has expired */
static int isexpired(bucket_t* b, int mtime)
{
	/* if the time-to-live of this entry has been exceeded, or if the
	 * file modification time has increased, the entry is expired.
	 * note that a time-to-live of zero means "never expire" */

	return (b->ttl != 0 && time(0) > b->createtime + b->ttl) ||
	       (mtime > b->mtime);
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
 * shared memory segments. assumes cache is locked for writing */
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
			void* shmaddr = apc_smm_attach(cache->buckets[i].shmid);
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

	/* reset header */
	cache->header->hits = cache->header->misses = 0;
}


/* emptybucket: clean out a bucket and free associated memory. assumes
 * cache is locked for writing*/
static void emptybucket(bucket_t* bucket)
{
	void* shmaddr = apc_smm_attach(bucket->shmid);
	apc_smm_free(shmaddr, bucket->offset);
	bucket->shmid = UNUSED;
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

/* apc_cache_create: create a new cache */
apc_cache_t* apc_cache_create(const char* pathname, int nbuckets,
	int maxseg, int segsize, int ttl)
{
	apc_cache_t* cache;
	int cachesize;

	cache = (apc_cache_t*) apc_emalloc(sizeof(apc_cache_t));
	cachesize = computecachesize(nbuckets, maxseg);

	/* per-process initialization */
	cache->pathname = (char*) apc_estrdup(pathname);
  #ifdef USE_RWLOCK
	cache->lock     = apc_rwl_create(pathname);
  #else
	cache->lock     = apc_sem_create(pathname, 1, 1);
  #endif
	cache->shmid    = apc_shm_create(pathname, 0, cachesize);
	cache->shmaddr  = apc_shm_attach(cache->shmid);
	cache->header   = (header_t*) cache->shmaddr;

	/* cache->segments and cache->buckets are "convenience" pointers
	 * to the beginning of buckets and of segments, respectively, in
	 * shared memory */

	cache->segments = (segment_t*) (cache->shmaddr + sizeof(header_t));
	cache->buckets =  (bucket_t*)  (cache->shmaddr + sizeof(header_t) +
	                                maxseg*sizeof(segment_t));

	/* instruct the OS to destroy the shm segment as soon as no processes
	 * are attached to it */
	apc_shm_destroy(cache->shmid);

	/* perform full initialization if necessary */
	if (cache->header->magic != MAGIC_INIT) {
		WRITELOCK(cache->lock);
		if (cache->header->magic != MAGIC_INIT) {
			/* cache not initialized (check twice to avoid race cond.) */
			initcache(cache, pathname, nbuckets, maxseg, segsize, ttl);
		}
		UNLOCK(cache->lock);
	}

	return cache;
}

/* apc_cache_destroy: destroys a cache */
void apc_cache_destroy(apc_cache_t* cache)
{
	int i;
	int maxseg;

	WRITELOCK(cache->lock);

	/* first, destroy the shared memory segments */
	maxseg = cache->header->maxseg;
	for (i = 0; i < maxseg; i++) {
		if (cache->segments[i].shmid != 0) {
			apc_shm_destroy(cache->segments[i].shmid);
		}
	}
	apc_shm_detach(cache->shmaddr);
	apc_shm_destroy(cache->shmid);

	UNLOCK(cache->lock);	/* race condition here! */

  #ifdef USE_RWLOCK
	apc_rwl_destroy(cache->lock);
  #else
	apc_sem_destroy(cache->lock);
  #endif

	apc_efree(cache);
}

/* apc_cache_clear: clears the cache */
void apc_cache_clear(apc_cache_t* cache)
{
	WRITELOCK(cache->lock);
	resetcache(cache);
	UNLOCK(cache->lock);
}

/* apc_cache_search: return 1 if key exists in cache, else 0 */
int apc_cache_search(apc_cache_t* cache, const char* key)
{
	unsigned slot;		/* initial hash value */
	unsigned k;			/* second hash value, for open-addressing */
	int nprobe;			/* running count of cache probes */
	bucket_t* buckets;
	int nbuckets;

	READLOCK(cache->lock);

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
			if (isexpired(&buckets[slot], 0)) {
				break; /* the entry has expired */
			}
			UNLOCK(cache->lock);
			return 1;
		}
		slot = (slot+k) % nbuckets;
	}
	UNLOCK(cache->lock);
	return 0; /* not found */
}

/* apc_cache_retrieve: retrieve entry from cache */
int apc_cache_retrieve(apc_cache_t* cache, const char* key, char** dataptr,
	int* length, int* maxsize, int mtime)
{
	unsigned slot;		/* initial hash value */
	unsigned k;			/* second hash value, for open-addressing */
	int nprobe;			/* running count of cache probes */
	char* shmaddr;		/* attached addr of data segment */
	bucket_t* buckets;
	int nbuckets;
	unsigned int checksum;

	READLOCK(cache->lock);

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
			if (isexpired(&buckets[slot], mtime)) {
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
			buckets[slot].lastaccess = time(0);
			buckets[slot].hitcount++;

			UNLOCK(cache->lock);

			/* compare checksums */
			if (DO_CHECKSUM && checksum != apc_crc32(*dataptr, *length)) {
				apc_eprint("checksum failed! data length is %d\n", *length);
				return 0; /* return failure */
			}

			/* we're done */
			return 1;
		}
		slot = (slot+k) % nbuckets;
	}
	cache->header->misses++;
	UNLOCK(cache->lock);
	return 0;
}

/* apc_cache_insert: insert entry into cache */
int apc_cache_insert(apc_cache_t* cache, const char* key,
	const char* data, int size, int mtime)
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
	checksum = DO_CHECKSUM ? apc_crc32(data, size) : 0;

	WRITELOCK(cache->lock);
	
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
			emptybucket(&buckets[slot]);
			break;	/* overwrite existing entry */
		}
		if (isexpired(&buckets[slot], 0)) {
			emptybucket(&buckets[slot]);
			break;	/* this entry has expired, overwrite it */
		}
		slot = (slot+k) % nbuckets;
	}
	if (nprobe == nbuckets) {	/* did we find a slot? */
		UNLOCK(cache->lock);	/* no, return failure */
		return -1;
	}

	shmaddr = 0;
	offset = 0;
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
		UNLOCK(cache->lock);
		return -1;
	}

	/* update the cache */
	strncpy(buckets[slot].key, key, MAX_KEY_LEN);
	buckets[slot].shmid      = segments[i].shmid;
	buckets[slot].offset     = offset;
	buckets[slot].length     = size;
	buckets[slot].createtime = time(0);
	buckets[slot].lastaccess = buckets[slot].createtime;
	buckets[slot].hitcount   = 0;
	buckets[slot].checksum   = checksum;
	buckets[slot].ttl		 = cache->header->ttl;
	buckets[slot].mtime      = mtime;
	
	/* store data in segment and update its record */
	memcpy(shmaddr + offset, data, size);

	UNLOCK(cache->lock);
	return 0;
}

/* apc_cache_remove: remove entry from cache */
int apc_cache_remove(apc_cache_t* cache, const char* key)
{
	unsigned slot;		/* initial hash value */
	unsigned k;			/* second hash value, for open-addressing */
	int nprobe;			/* running count of cache probes */
	bucket_t* buckets;
	int nbuckets;

	WRITELOCK(cache->lock);

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
			emptybucket(&buckets[slot]);
			UNLOCK(cache->lock);
			return 1;
		}
		slot = (slot+k) % nbuckets;
	}
	UNLOCK(cache->lock);
	return 0;	/* not found */
}

int apc_cache_set_object_ttl(apc_cache_t* cache, const char* key, int ttl)
{
	unsigned slot;		/* initial hash value */
	unsigned k;			/* second hash value, for open-addressing */
	int nprobe;			/* running count of cache probes */
	bucket_t* buckets;
	int nbuckets;

	WRITELOCK(cache->lock);

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
			buckets[slot].ttl = ttl;
			UNLOCK(cache->lock);
			return 1;
		}
		slot = (slot+k) % nbuckets;
	}
	UNLOCK(cache->lock);
	return 0; /* key doesn't exist */
}

/* apc_cache_dump: outputs cache information as HTML */
void apc_cache_dump(apc_cache_t* cache, const char* linkurl,
	apc_outputfn_t outputfn)
{
	int i;
	double hitrate;

	READLOCK(cache->lock);

	hitrate = (1.0 * cache->header->hits) /
		(cache->header->hits + cache->header->misses);

	outputfn("<html>\n");

	/* display HEAD */
	outputfn("<head>\n");
	outputfn("<title>APC-SHM Cache Info</title>\n");
	outputfn("</head>\n");

	/* display cache header info */
	outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
	outputfn("<tr>\n");
	outputfn("<td colspan=2 bgcolor=#dde4ff>Cache Header</td>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#ffffff>Name</td>\n");
	outputfn("<td bgcolor=#ffffff>Value</td>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>magic</td>\n");
	outputfn("<td bgcolor=#eeeeee>0x%x</td>\n", cache->header->magic);
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>total buckets</td>\n");
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", cache->header->nbuckets);
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>maximum shared memory segments</td>\n");
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", cache->header->maxseg);
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>shared memory segment size (B)</td>\n");
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", cache->header->segsize);
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>time-to-live</td>\n");
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", cache->header->ttl);
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>hits</td>\n");
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", cache->header->hits);
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>misses</td>\n");
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", cache->header->misses);
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>hit rate</td>\n");
	outputfn("<td bgcolor=#eeeeee>%.2f</td>\n", hitrate);
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>cache filter</td>\n");
	outputfn("<td bgcolor=#eeeeee>%s</td>\n", APCG(regex_text)? APCG(regex_text): "(none)");
	outputfn("<tr>\n");
	outputfn("<td colspan=2 bgcolor=#ffffff>local information</td>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>shared memory ID</td>\n");
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", cache->shmid);
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>local shared memory address</td>\n");
	outputfn("<td bgcolor=#eeeeee>%p</td>\n", cache->shmaddr);
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>creation pathname</td>\n");
	outputfn("<td bgcolor=#eeeeee>%s</td>\n",
		cache->pathname ? cache->pathname : "(null)");
	outputfn("</table>\n");
	outputfn("<br>\n");
	outputfn("<br>\n");

	/* display bucket info */
	outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
	outputfn("<tr><form method=post action=%s>\n", linkurl);
	outputfn("<td colspan=%d bgcolor=#dde4ff>Bucket Data</td>\n",
		(linkurl != 0) ? 10 : 9);
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#ffffff>Index</td>\n");
	if (linkurl != 0) {
		outputfn("<td bgcolor=#ffffff>Delete</td>\n");
	}
	outputfn("<td bgcolor=#ffffff>Key</td>\n");
	outputfn("<td bgcolor=#ffffff>Offset</td>\n");
	outputfn("<td bgcolor=#ffffff>Length (B)</td>\n");
	outputfn("<td bgcolor=#ffffff>Last Access</td>\n");
	outputfn("<td bgcolor=#ffffff>Hits</td>\n");
	outputfn("<td bgcolor=#ffffff>Expire Time</td>\n");
	outputfn("<td bgcolor=#ffffff>Modification Time</td>\n");
	outputfn("<td bgcolor=#ffffff>Checksum</td>\n");

	for (i = 0; i < cache->header->nbuckets; i++) {
		bucket_t* bucket;
		if (cache->buckets[i].shmid < 0) {
			continue;
		}
		bucket = &(cache->buckets[i]);
		outputfn("<tr>\n");
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", i);

        if (linkurl != 0) {
            outputfn("<td bgcolor=#eeeeee><input type=checkbox "
			         "name=apc_rm[] value=%s>&nbsp</td>\n",
					 bucket->key);
            outputfn("<td bgcolor=#eeeeee><a href=%s?apc_info=%s>"
			         "%s</a></td>\n", linkurl, bucket->key, bucket->key);
		}
        else {
            outputfn("<td bgcolor=#eeeeee>%s</td>\n", bucket->key);
        }

		outputfn("<td bgcolor=#eeeeee>%d</td>\n", bucket->offset);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", bucket->length);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", bucket->lastaccess);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", bucket->hitcount);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n",
			(bucket->ttl != 0) ? bucket->createtime + bucket->ttl : 0);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", bucket->mtime);
		outputfn("<td bgcolor=#eeeeee>%u</td>\n", bucket->checksum);
	}

	if (linkurl != 0) {
		outputfn("<tr><td><input type=submit name=submit "
		         "value=\"Delete Marked Objects\"></tr></td>\n");
	}
	outputfn("</form>\n");
	outputfn("</table>\n");
	outputfn("<br>\n");
	outputfn("<br>\n");

	/* display shared memory segment info */
	outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#dde4ff>Shared Memory Info</td>\n");
	for (i = 0; i < cache->header->maxseg; i++) {
		if (cache->segments[i].shmid > 0) {
			outputfn("<tr>\n");
			outputfn("<td>\n");
			apc_smm_dump(apc_smm_attach(cache->segments[i].shmid), outputfn);
			outputfn("</td>\n");
		}
	}
	outputfn("</table>\n");

	outputfn("<br>\n");
	outputfn("<br>\n");

	outputfn("</html>\n");

	UNLOCK(cache->lock);
}

int apc_cache_dump_entry(apc_cache_t* cache, const char* key,
	apc_outputfn_t outputfn)
{
	static const char NBSP[] = "&nbsp;";
	unsigned slot;		/* initial hash value */
	unsigned k;			/* second hash value, for open-addressing */
	int nprobe;			/* running count of cache probes */
	bucket_t* bp;		/* pointer to matching bucket */
	bucket_t* buckets;
	int nbuckets;
	int i;

	HashTable function_table;
	HashTable class_table;
	apc_nametable_t* dummy;
	zend_op_array* op_array;
	Bucket* p;
	Bucket* q;

	READLOCK(cache->lock);

	buckets  = cache->buckets;
	nbuckets = cache->header->nbuckets;

	slot = hash(key) % nbuckets;
	k = hashtwo(key) % nbuckets;

	nprobe = 0;
	bp = 0;
	while (buckets[slot].shmid != EMPTY && nprobe++ < nbuckets) {
		if (buckets[slot].shmid == UNUSED) {
			continue;
		}
		if (strcmp(buckets[slot].key, key) == 0) {
			if (isexpired(&buckets[slot], 0)) {
				break; /* the entry has expired */
			}
			bp = &buckets[slot];
			break;
		}
		slot = (slot+k) % nbuckets;
	}

	if (!bp) {
		UNLOCK(cache->lock);
		return -1;
	}

	/* begin outer table */
	outputfn("<table border=0 cellpadding=2 cellspacing=1>\n");

	/* begin first row of outer table */
	outputfn("<tr>\n");

	/* display bucket info */
	outputfn("<td colspan=3 valign=top>");
	outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
	outputfn("<tr>\n");
	outputfn("<td colspan=9 bgcolor=#dde4ff>Bucket Data</td>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#ffffff>Key</td>\n");
	outputfn("<td bgcolor=#ffffff>Offset</td>\n");
	outputfn("<td bgcolor=#ffffff>Length (B)</td>\n");
	outputfn("<td bgcolor=#ffffff>Last Access</td>\n");
	outputfn("<td bgcolor=#ffffff>Hits</td>\n");
	outputfn("<td bgcolor=#ffffff>Expire Time</td>\n");
	outputfn("<td bgcolor=#ffffff>Modification Time</td>\n");
	outputfn("<td bgcolor=#ffffff>Checksum</td>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>%s</td>\n", bp->key);
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", bp->offset);
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", bp->length);
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", bp->lastaccess);
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", bp->hitcount);
	outputfn("<td bgcolor=#eeeeee>%d</td>\n",
			(bp->ttl != 0) ? bp->createtime + bp->ttl : 0);
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", bp->mtime);
	outputfn("<td bgcolor=#eeeeee>%u</td>\n", bp->checksum);
	outputfn("</table>\n");
	outputfn("</td>\n");

	/* end first row of outer table */
	outputfn("</tr>\n");

	op_array = (zend_op_array*) malloc(sizeof(zend_op_array));
	zend_hash_init(&function_table, 13, NULL, NULL, 1);
	zend_hash_init(&class_table, 13, NULL, NULL, 1);
	dummy = apc_nametable_create(97);

	/* deserialize bucket and see what's inside */
	apc_init_deserializer(apc_smm_attach(bp->shmid) + bp->offset, bp->length);
	apc_deserialize_zend_function_table(&function_table, dummy, dummy);
	apc_deserialize_zend_class_table(&class_table, dummy, dummy);
	apc_deserialize_zend_op_array(op_array);
	/* begin second row of outer table */
	outputfn("<tr>\n");

	/* display opcodes in the entry */
	outputfn("<td valign=top>");
	outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
	outputfn("<tr>\n");
	outputfn("<td colspan=3 bgcolor=#dde4ff>OpCodes</td>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#ffffff>Value</td>\n");
	outputfn("<td bgcolor=#ffffff>Extended</td>\n");
	outputfn("<td bgcolor=#ffffff>Line</td>\n");
	for (i = 0; i < op_array->last; i++) {
		const char* name;

		outputfn("<tr>\n");

		/* print the regular opcode, or '&nbsp;' if empty */
		name = apc_get_zend_opname(op_array->opcodes[i].opcode);
		outputfn("<td bgcolor=#eeeeee>%s</td>\n", name);

		/* print the extended opcode, or '&nbsp;' if empty */
		if (op_array->opcodes[i].opcode != ZEND_NOP &&
			op_array->opcodes[i].opcode != ZEND_DECLARE_FUNCTION_OR_CLASS)
		{
			/* this opcode does not have an extended value */
			name = NBSP;
		}
		else {
			name = apc_get_zend_extopname(op_array->opcodes[i].extended_value);
		}
		outputfn("<td bgcolor=#eeeeee>%s</td>\n", name);

		/* print the line number of the opcode */
		outputfn("<td bgcolor=#eeeeee>%u</td>\n", op_array->opcodes[i].lineno);
	}
	outputfn("</table>\n");
	outputfn("</td>\n");

	/* display functions in the entry */
	outputfn("<td valign=top>");
	outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#dde4ff>Functions</td>\n");
	p = function_table.pListHead;
	while (p) {
		zend_function* zf = (zend_function*) p->pData;
		outputfn("<tr>\n");
		outputfn("<td bgcolor=#eeeeee>%s</td>\n",
			zf->common.function_name);
		p = p->pListNext;
	}
	outputfn("</table>\n");
	outputfn("</td>\n");

	/* display classes in the entry */
	outputfn("<td valign=top>");
	outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#dde4ff>Classes</td>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#ffffff>Class</td>\n");
	outputfn("<td bgcolor=#ffffff>Function</td>\n");
	p = class_table.pListHead;
	while (p) {
		zend_class_entry* zc = (zend_class_entry*) p->pData;
		outputfn("<tr>\n");
		outputfn("<td bgcolor=#eeeeee>%s</td><td bgcolor=#eeeeee>&nbsp</td>\n",
			zc->name);
        q = zc->function_table.pListHead;
        while(q) {
            zend_function* zf = (zend_function*) q->pData;
            outputfn("<tr>\n");
            outputfn("<td bgcolor=#eeeeee>&nbsp</td>\n");
            outputfn("<td bgcolor=#eeeeee>%s</td>\n",
                zf->common.function_name);
            q = q->pListNext;
        }

		p = p->pListNext;
	}
	outputfn("</table>\n");
	outputfn("</td>\n");

	/* end second row of outer table */
	outputfn("</tr>\n");

	/* close outer table */
	outputfn("</table>\n");

	/* clean up */
	zend_hash_clean(&class_table);
	zend_hash_clean(&function_table);
	destroy_op_array(op_array);
	free(op_array);

	UNLOCK(cache->lock);
	return 0;
}

