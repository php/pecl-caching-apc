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
  |          Arun C. Murthy <arunc@yahoo-inc.com>                        |
  |          Gopal Vijayaraghavan <gopalv@yahoo-inc.com>                 |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$ */

#include "apc_cache.h"
#include "apc_lock.h"
#include "apc_sma.h"
#include "apc_globals.h"
#include "SAPI.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"

/* TODO: rehash when load factor exceeds threshold */

#define CHECK(p) { if ((p) == NULL) return NULL; }

/* {{{ locking macros */
#define CREATE_LOCK(lock)     apc_lck_create(NULL, 0, 1, lock)
#define DESTROY_LOCK(c) apc_lck_destroy(c->header->lock)
#define LOCK(c)         { HANDLE_BLOCK_INTERRUPTIONS(); apc_lck_lock(c->header->lock); }
#define RDLOCK(c)       { HANDLE_BLOCK_INTERRUPTIONS(); apc_lck_rdlock(c->header->lock); }
#define UNLOCK(c)       { apc_lck_unlock(c->header->lock); HANDLE_UNBLOCK_INTERRUPTIONS(); }
/* }}} */

/* {{{ struct definition: slot_t */
typedef struct slot_t slot_t;
struct slot_t {
    apc_cache_key_t key;        /* slot key */
    apc_cache_entry_t* value;   /* slot value */
    slot_t* next;               /* next slot in linked list */
    int num_hits;               /* number of hits to this bucket */
    time_t creation_time;       /* time slot was initialized */
    time_t deletion_time;       /* time slot was removed from cache */
    time_t access_time;         /* time slot was last accessed */
};
/* }}} */

/* {{{ struct definition: header_t
   Any values that must be shared among processes should go in here. */
typedef struct header_t header_t;
struct header_t {
    apc_lck_t lock;              /* read/write lock (exclusive blocking cache lock) */
    apc_lck_t wrlock;           /* write lock (non-blocking used to prevent cache slams) */
    int num_hits;               /* total successful hits in cache */
    int num_misses;             /* total unsuccessful hits in cache */
    int num_inserts;            /* total successful inserts in cache */
    slot_t* deleted_list;       /* linked list of to-be-deleted slots */
    time_t start_time;          /* time the above counters were reset */
    int expunges;               /* total number of expunges */
    zend_bool busy;             /* Flag to tell clients when we are busy cleaning the cache */
    int num_entries;            /* Statistic on the number of entries */
    size_t mem_size;            /* Statistic on the memory size used by this cache */
};
/* }}} */

/* {{{ struct definition: apc_cache_t */
struct apc_cache_t {
    void* shmaddr;              /* process (local) address of shared cache */
    header_t* header;           /* cache header (stored in SHM) */
    slot_t** slots;             /* array of cache slots (stored in SHM) */
    int num_slots;              /* number of slots in cache */
    int gc_ttl;                 /* maximum time on GC list for a slot */
    int ttl;                    /* if slot is needed and entry's access time is older than this ttl, remove it */
};
/* }}} */

/* {{{ struct definition local_slot_t */
typedef struct local_slot_t local_slot_t;
struct local_slot_t {
    slot_t *original;           /* the original slot in shm */
    int num_hits;               /* number of hits */
    time_t creation_time;       /* creation time */
    apc_cache_entry_t *value;   /* shallow copy of slot->value */
    local_slot_t *next;         /* only for dead list */
};
/* }}} */
/* {{{ struct definition apc_local_cache_t */
struct apc_local_cache_t {
    apc_cache_t* shmcache;      /* the real cache in shm */
    local_slot_t* slots;        /* process (local) cache of objects */
    local_slot_t* dead_list;    /* list of objects pending removal */
    int num_slots;              /* number of slots in cache */
    int ttl;                    /* time to live */
    int num_hits;               /* number of hits */
    int generation;             /* every generation lives between expunges */
};
/* }}} */

/* {{{ key_equals */
#define key_equals(a, b) (a.inode==b.inode && a.device==b.device)
/* }}} */

/* {{{ hash */
static unsigned int hash(apc_cache_key_t key)
{
    return key.data.file.device + key.data.file.inode;
}
/* }}} */

/* {{{ string_nhash_8 */
static unsigned int string_nhash_8(const char *s, size_t len)
{
    register const unsigned int *iv = (const unsigned int *)s;
    register unsigned int h = 0;
    register const unsigned int *e  = (const unsigned int *)(s + len - (len % sizeof(unsigned int)));

    for(;iv<e;iv++) {
        h += *iv;
        h = (h << 7) | (h >> ((8*sizeof(unsigned int)) - 7));
    }
    s = (const char *)iv;
    for(len %= sizeof(unsigned int);len;len--) {
        h += *(s++);
    }
    h ^= (h >> 13);
    h ^= (h >> 7);
    return h;
}
/* }}} */

/* {{{ make_slot */
slot_t* make_slot(apc_cache_key_t key, apc_cache_entry_t* value, slot_t* next, time_t t)
{
    slot_t* p = apc_sma_malloc(sizeof(slot_t));
    if (!p) return NULL;

    if(value->type == APC_CACHE_ENTRY_USER) {
        char *identifier = (char*) apc_xstrdup(key.data.user.identifier, apc_sma_malloc);
        if (!identifier) {
            apc_sma_free(p);
            return NULL;
        }
        key.data.user.identifier = identifier;
    } else if(key.type == APC_CACHE_KEY_FPFILE) {
        char *fullpath = (char*) apc_xstrdup(key.data.fpfile.fullpath, apc_sma_malloc);
        if (!fullpath) {
            apc_sma_free(p);
            return NULL;
        }
        key.data.fpfile.fullpath = fullpath;
    }
    p->key = key;
    p->value = value;
    p->next = next;
    p->num_hits = 0;
    p->creation_time = t;
    p->access_time = t;
    p->deletion_time = 0;
    return p;
}
/* }}} */

/* {{{ free_slot */
static void free_slot(slot_t* slot)
{
    if(slot->value->type == APC_CACHE_ENTRY_USER) {
        apc_sma_free((char *)slot->key.data.user.identifier);
    } else if(slot->key.type == APC_CACHE_KEY_FPFILE) {
        apc_sma_free((char *)slot->key.data.fpfile.fullpath);
    }
    apc_cache_free_entry(slot->value);
    apc_sma_free(slot);
}
/* }}} */

/* {{{ remove_slot */
static void remove_slot(apc_cache_t* cache, slot_t** slot)
{
    slot_t* dead = *slot;
    *slot = (*slot)->next;

    cache->header->mem_size -= dead->value->mem_size;
    cache->header->num_entries--;
    if (dead->value->ref_count <= 0) {
        free_slot(dead);
    }
    else {
        dead->next = cache->header->deleted_list;
        dead->deletion_time = time(0);
        cache->header->deleted_list = dead;
    }
}
/* }}} */

/* {{{ process_pending_removals */
static void process_pending_removals(apc_cache_t* cache)
{
    slot_t** slot;
    time_t now;

    /* This function scans the list of removed cache entries and deletes any
     * entry whose reference count is zero (indicating that it is no longer
     * being executed) or that has been on the pending list for more than
     * cache->gc_ttl seconds (we issue a warning in the latter case).
     */

    if (!cache->header->deleted_list)
        return;

    slot = &cache->header->deleted_list;
    now = time(0);

    while (*slot != NULL) {
        int gc_sec = cache->gc_ttl ? (now - (*slot)->deletion_time) : 0;

        if ((*slot)->value->ref_count <= 0 || gc_sec > cache->gc_ttl) {
            slot_t* dead = *slot;

            if (dead->value->ref_count > 0) {
                switch(dead->value->type) {
                    case APC_CACHE_ENTRY_FILE:
                        apc_log(APC_WARNING, "GC cache entry '%s' (dev=%d ino=%d) "
                            "was on gc-list for %d seconds", dead->value->data.file.filename,
                            dead->key.data.file.device, dead->key.data.file.inode, gc_sec);
                        break;
                    case APC_CACHE_ENTRY_USER:
                        apc_log(APC_WARNING, "GC cache entry '%s' "
                            "was on gc-list for %d seconds", dead->value->data.user.info, gc_sec);
                        break;
                }
            }
            *slot = dead->next;
            free_slot(dead);
        }
        else {
            slot = &(*slot)->next;
        }
    }
}
/* }}} */

/* {{{ prevent_garbage_collection */
static void prevent_garbage_collection(apc_cache_entry_t* entry)
{
    /* set reference counts on zend objects to an arbitrarily high value to
     * prevent garbage collection after execution */

    enum { BIG_VALUE = 1000 };

    if(entry->data.file.op_array) {
        entry->data.file.op_array->refcount[0] = BIG_VALUE;
    }
    if (entry->data.file.functions) {
        int i;
        apc_function_t* fns = entry->data.file.functions;
        for (i=0; fns[i].function != NULL; i++) {
#ifdef ZEND_ENGINE_2            
            *(fns[i].function->op_array.refcount) = BIG_VALUE;
#else            
            fns[i].function->op_array.refcount[0] = BIG_VALUE;
#endif            
        }
    }
    if (entry->data.file.classes) {
        int i;
        apc_class_t* classes = entry->data.file.classes;
        for (i=0; classes[i].class_entry != NULL; i++) {
#ifdef ZEND_ENGINE_2            
            classes[i].class_entry->refcount = BIG_VALUE;
#else            
            classes[i].class_entry->refcount[0] = BIG_VALUE;
#endif
        }
    }
}
/* }}} */

/* {{{ apc_cache_create */
apc_cache_t* apc_cache_create(int size_hint, int gc_ttl, int ttl)
{
    apc_cache_t* cache;
    int cache_size;
    int num_slots;
    int i;

    num_slots = size_hint > 0 ? size_hint*2 : 2000;

    cache = (apc_cache_t*) apc_emalloc(sizeof(apc_cache_t));
    cache_size = sizeof(header_t) + num_slots*sizeof(slot_t*);

    cache->shmaddr = apc_sma_malloc(cache_size);
    if(!cache->shmaddr) {
        apc_eprint("Unable to allocate shared memory for cache structures.  (Perhaps your shared memory size isn't large enough?). ");
    }
    memset(cache->shmaddr, 0, cache_size);

    cache->header = (header_t*) cache->shmaddr;
    cache->header->num_hits = 0;
    cache->header->num_misses = 0;
    cache->header->deleted_list = NULL;
    cache->header->start_time = time(NULL);
    cache->header->expunges = 0;
    cache->header->busy = 0;

    cache->slots = (slot_t**) (((char*) cache->shmaddr) + sizeof(header_t));
    cache->num_slots = num_slots;
    cache->gc_ttl = gc_ttl;
    cache->ttl = ttl;
    CREATE_LOCK(cache->header->lock);
#if NONBLOCKING_LOCK_AVAILABLE
    CREATE_LOCK(cache->header->wrlock);
#endif
    for (i = 0; i < num_slots; i++) {
        cache->slots[i] = NULL;
    }

    return cache;
}
/* }}} */

/* {{{ apc_cache_destroy */
void apc_cache_destroy(apc_cache_t* cache)
{
    DESTROY_LOCK(cache);
    apc_efree(cache);
}
/* }}} */

/* {{{ apc_cache_clear */
void apc_cache_clear(apc_cache_t* cache)
{
    int i;

    if(!cache) return;

    LOCK(cache);
    cache->header->busy = 1;
    cache->header->num_hits = 0;
    cache->header->num_misses = 0;
    cache->header->start_time = time(NULL);
    cache->header->expunges = 0;

    for (i = 0; i < cache->num_slots; i++) {
        slot_t* p = cache->slots[i];
        while (p) {
            remove_slot(cache, &p);
        }
        cache->slots[i] = NULL;
    }
    
    cache->header->busy = 0;
    UNLOCK(cache);
}
/* }}} */

/* {{{ apc_cache_expunge */
void apc_cache_expunge(apc_cache_t* cache, time_t t)
{
    int i;

    if(!cache) return;

    if(!cache->ttl) {
        /* 
         * If cache->ttl is not set, we wipe out the entire cache when
         * we run out of space. 
         */
        LOCK(cache);
        cache->header->busy = 1;
        cache->header->expunges++;
        for (i = 0; i < cache->num_slots; i++) {
            slot_t* p = cache->slots[i];
            while (p) {
                remove_slot(cache, &p);
            }
            cache->slots[i] = NULL;
        }
        cache->header->busy = 0;
        UNLOCK(cache);
    } else {
        slot_t **p;

        /*
         * If the ttl for the cache is set we walk through and delete stale 
         * entries.  For the user cache that is slightly confusing since
         * we have the individual entry ttl's we can look at, but that would be
         * too much work.  So if you want the user cache expunged, set a high
         * default apc.user_ttl and still provide a specific ttl for each entry
         * on insert
         */

        LOCK(cache);
        cache->header->busy = 1;
        cache->header->expunges++;
        for (i = 0; i < cache->num_slots; i++) {
            p = &cache->slots[i];
            while(*p) {
                /* 
                 * For the user cache we look at the individual entry ttl values
                 * and if not set fall back to the default ttl for the user cache
                 */
                if((*p)->value->type == APC_CACHE_ENTRY_USER) {
                    if((*p)->value->data.user.ttl) {
                        if((*p)->creation_time + (*p)->value->data.user.ttl < t) {
                            remove_slot(cache, p);
                            continue;
                        }
                    } else if(cache->ttl) {
                        if((*p)->creation_time + cache->ttl < t) {
                            remove_slot(cache, p);
                            continue;
                        }
                    }
                } else if((*p)->access_time < (t - cache->ttl)) {
                    remove_slot(cache, p);
                    continue;
                }
                p = &(*p)->next;
            }
        }
        cache->header->busy = 0;
        UNLOCK(cache);
    }
}
/* }}} */

/* {{{ apc_cache_insert */
int apc_cache_insert(apc_cache_t* cache,
                     apc_cache_key_t key,
                     apc_cache_entry_t* value,
                     time_t t)
{
    slot_t** slot;

    if (!value) {
        return 0;
    }

#ifdef __DEBUG_APC__
    fprintf(stderr,"Inserting [%s]\n", value->data.file.filename);
#endif

    LOCK(cache);
    process_pending_removals(cache);

    if(key.type == APC_CACHE_KEY_FILE) slot = &cache->slots[hash(key) % cache->num_slots];
    else slot = &cache->slots[string_nhash_8(key.data.fpfile.fullpath, key.data.fpfile.fullpath_len) % cache->num_slots];

    while(*slot) {
      if(key.type == (*slot)->key.type) {
        if(key.type == APC_CACHE_KEY_FILE) {
            if(key_equals((*slot)->key.data.file, key.data.file)) {
                /* If existing slot for the same device+inode is different, remove it and insert the new version */
                if ((*slot)->key.mtime != key.mtime) {
                    remove_slot(cache, slot);
                    break;
                }
                UNLOCK(cache);
                return 0;
            } else if(cache->ttl && (*slot)->access_time < (t - cache->ttl)) {
                remove_slot(cache, slot);
                continue;
            }
        } else {   /* APC_CACHE_KEY_FPFILE */
                if(!memcmp((*slot)->key.data.fpfile.fullpath, key.data.fpfile.fullpath, key.data.fpfile.fullpath_len+1)) {
                /* Hrm.. it's already here, remove it and insert new one */
                remove_slot(cache, slot);
                break;
            } else if(cache->ttl && (*slot)->access_time < (t - cache->ttl)) {
                remove_slot(cache, slot);
                continue;
            }
        }
      }
      slot = &(*slot)->next;
    }

    if ((*slot = make_slot(key, value, *slot, t)) == NULL) {
        UNLOCK(cache);
        return -1;
    }
   
    cache->header->mem_size += value->mem_size;
    cache->header->num_entries++;
    cache->header->num_inserts++;
    
    UNLOCK(cache);
    return 1;
}
/* }}} */

/* {{{ apc_cache_user_insert */
int apc_cache_user_insert(apc_cache_t* cache, apc_cache_key_t key, apc_cache_entry_t* value, time_t t, int exclusive TSRMLS_DC)
{
    slot_t** slot;
    size_t* mem_size_ptr = NULL;

    if (!value) {
        return 0;
    }

    LOCK(cache);
    process_pending_removals(cache);

    slot = &cache->slots[string_nhash_8(key.data.user.identifier, key.data.user.identifier_len) % cache->num_slots];

    if (APCG(mem_size_ptr) != NULL) {
        mem_size_ptr = APCG(mem_size_ptr);
        APCG(mem_size_ptr) = NULL;
    }

    while (*slot) {
        if (!memcmp((*slot)->key.data.user.identifier, key.data.user.identifier, key.data.user.identifier_len)) {
            /* 
             * At this point we have found the user cache entry.  If we are doing 
             * an exclusive insert (apc_add) we are going to bail right away if
             * the user entry already exists and it has no ttl, or
             * there is a ttl and the entry has not timed out yet.
             */
            if(exclusive && (  !(*slot)->value->data.user.ttl ||
                              ( (*slot)->value->data.user.ttl && ((*slot)->creation_time + (*slot)->value->data.user.ttl) >= t ) 
                            ) ) {
                UNLOCK(cache);
                return 0;
            }
            remove_slot(cache, slot);
            break;
        } else 
        /* 
         * This is a bit nasty.  The idea here is to do runtime cleanup of the linked list of
         * slot entries so we don't always have to skip past a bunch of stale entries.  We check
         * for staleness here and get rid of them by first checking to see if the cache has a global
         * access ttl on it and removing entries that haven't been accessed for ttl seconds and secondly
         * we see if the entry has a hard ttl on it and remove it if it has been around longer than its ttl
         */
        if((cache->ttl && (*slot)->access_time < (t - cache->ttl)) || 
           ((*slot)->value->data.user.ttl && ((*slot)->creation_time + (*slot)->value->data.user.ttl) < t)) {
            remove_slot(cache, slot);
            continue;
        }
        slot = &(*slot)->next;
    }

    if (mem_size_ptr != NULL) {
        APCG(mem_size_ptr) = mem_size_ptr;
    }

    if ((*slot = make_slot(key, value, *slot, t)) == NULL) {
        UNLOCK(cache);
        return 0;
    }
    if (APCG(mem_size_ptr) != NULL) {
        value->mem_size = *APCG(mem_size_ptr);
        cache->header->mem_size += *APCG(mem_size_ptr);
    }
    cache->header->num_entries++;
    cache->header->num_inserts++;

    UNLOCK(cache);
    return 1;
}
/* }}} */

/* {{{ apc_cache_find_slot */
slot_t* apc_cache_find_slot(apc_cache_t* cache, apc_cache_key_t key, time_t t)
{
    slot_t** slot;
    volatile slot_t* retval = NULL;

    LOCK(cache);
    if(key.type == APC_CACHE_KEY_FILE) slot = &cache->slots[hash(key) % cache->num_slots];
    else slot = &cache->slots[string_nhash_8(key.data.fpfile.fullpath, key.data.fpfile.fullpath_len) % cache->num_slots];

    while (*slot) {
      if(key.type == (*slot)->key.type) {
        if(key.type == APC_CACHE_KEY_FILE) {
            if(key_equals((*slot)->key.data.file, key.data.file)) {
                if((*slot)->key.mtime != key.mtime) {
                    remove_slot(cache, slot);
                    cache->header->num_misses++;
                    UNLOCK(cache);
                    return NULL;
                }
                (*slot)->num_hits++;
                (*slot)->value->ref_count++;
                (*slot)->access_time = t;
                prevent_garbage_collection((*slot)->value);
                cache->header->num_hits++;
                retval = *slot;
                UNLOCK(cache);
                return (slot_t*)retval;
            }
        } else {  /* APC_CACHE_KEY_FPFILE */
            if(!memcmp((*slot)->key.data.fpfile.fullpath, key.data.fpfile.fullpath, key.data.fpfile.fullpath_len+1)) {
                /* TTL Check ? */
                (*slot)->num_hits++;
                (*slot)->value->ref_count++;
                (*slot)->access_time = t;
                prevent_garbage_collection((*slot)->value);
                cache->header->num_hits++;
                retval = *slot;
                UNLOCK(cache);
                return (slot_t*)retval;
            }
        }
      }
      slot = &(*slot)->next;
    }
    cache->header->num_misses++;
    UNLOCK(cache);
    return NULL;
}
/* }}} */

/* {{{ apc_cache_find */
apc_cache_entry_t* apc_cache_find(apc_cache_t* cache, apc_cache_key_t key, time_t t)
{
    slot_t * slot = apc_cache_find_slot(cache, key, t);
    return (slot) ? slot->value : NULL;
}
/* }}} */

/* {{{ apc_cache_user_find */
apc_cache_entry_t* apc_cache_user_find(apc_cache_t* cache, char *strkey, int keylen, time_t t)
{
    slot_t** slot;
    volatile apc_cache_entry_t* value = NULL;

    LOCK(cache);

    slot = &cache->slots[string_nhash_8(strkey, keylen) % cache->num_slots];

    while (*slot) {
        if (!memcmp((*slot)->key.data.user.identifier, strkey, keylen)) {
            /* Check to make sure this entry isn't expired by a hard TTL */
            if((*slot)->value->data.user.ttl && ((*slot)->creation_time + (*slot)->value->data.user.ttl) < t) {
                remove_slot(cache, slot);
                UNLOCK(cache);
                return NULL;
            }
            /* Otherwise we are fine, increase counters and return the cache entry */
            (*slot)->num_hits++;
            (*slot)->value->ref_count++;
            (*slot)->access_time = t;

            cache->header->num_hits++;
            value = (*slot)->value;
            UNLOCK(cache);
            return (apc_cache_entry_t*)value;
        }
        slot = &(*slot)->next;
    }
 
    UNLOCK(cache);
    return NULL;
}
/* }}} */

/* {{{ apc_cache_user_delete */
int apc_cache_user_delete(apc_cache_t* cache, char *strkey, int keylen)
{
    slot_t** slot;

    LOCK(cache);

    slot = &cache->slots[string_nhash_8(strkey, keylen) % cache->num_slots];

    while (*slot) {
        if (!memcmp((*slot)->key.data.user.identifier, strkey, keylen)) {
            remove_slot(cache, slot);
            UNLOCK(cache);
            return 1;
        }
        slot = &(*slot)->next;
    }

    UNLOCK(cache);
    return 0;
}
/* }}} */

/* {{{ apc_cache_release */
void apc_cache_release(apc_cache_t* cache, apc_cache_entry_t* entry)
{
    /* local cache refcount-- is done in apc_local_cache_cleanup */
    if(entry->local) return;

    LOCK(cache);
    entry->ref_count--;
    UNLOCK(cache);
}
/* }}} */

/* {{{ apc_cache_make_file_key */
int apc_cache_make_file_key(apc_cache_key_t* key,
                       const char* filename,
                       const char* include_path,
                       time_t t
					   TSRMLS_DC)
{
    struct stat *tmp_buf=NULL;
    struct apc_fileinfo_t fileinfo = { {0}, };
    int len;

    assert(key != NULL);

    if (!filename || !SG(request_info).path_translated) {
#ifdef __DEBUG_APC__
        fprintf(stderr,"No filename and no path_translated - bailing\n");
#endif
        return 0;
	}

    len = strlen(filename);
    if(APCG(fpstat)==0) {
        if(IS_ABSOLUTE_PATH(filename,len)) {
            key->data.fpfile.fullpath = filename;
            key->data.fpfile.fullpath_len = len;
            key->mtime = t;
            key->type = APC_CACHE_KEY_FPFILE;
        } else {
            if (apc_search_paths(filename, include_path, &fileinfo) != 0) {
                apc_wprint("apc failed to locate %s - bailing", filename);
                return 0;
            }

            if(!realpath(fileinfo.fullpath, APCG(canon_path))) {
                apc_wprint("realpath failed to canonicalize %s - bailing", filename);
                return 0;
            }

            key->data.fpfile.fullpath = APCG(canon_path);
            key->data.fpfile.fullpath_len = strlen(APCG(canon_path));
            key->mtime = t;
            key->type = APC_CACHE_KEY_FPFILE;
        }
        return 1;
    } 

    if(!strcmp(SG(request_info).path_translated, filename)) {
        tmp_buf = sapi_get_stat(TSRMLS_C);  /* Apache has already done this stat() for us */
    }
    if(tmp_buf) { 
		fileinfo.st_buf = *tmp_buf;
    } else {
        if (apc_search_paths(filename, include_path, &fileinfo) != 0) {
#ifdef __DEBUG_APC__
            fprintf(stderr,"Stat failed %s - bailing (%s) (%d)\n",filename,SG(request_info).path_translated);
#endif
            return 0;
        }
    }

    if(APCG(max_file_size) < fileinfo.st_buf.st_size) {
#ifdef __DEBUG_APC__
        fprintf(stderr,"File is too big %s (%d - %ld) - bailing\n",filename,t,fileinfo.st_buf.st_size);
#endif
        return 0;
    }

    /*
     * This is a bit of a hack.
     *
     * Here I am checking to see if the file is at least 2 seconds old.  
     * The idea is that if the file is currently being written to then its
     * mtime is going to match or at most be 1 second off of the current
     * request time and we want to avoid caching files that have not been
     * completely written.  Of course, people should be using atomic 
     * mechanisms to push files onto live web servers, but adding this
     * tiny safety is easier than educating the world.  This is now
     * configurable, but the default is still 2 seconds.
     */
    if(APCG(file_update_protection) && (t - fileinfo.st_buf.st_mtime < APCG(file_update_protection))) { 
#ifdef __DEBUG_APC__
        fprintf(stderr,"File is too new %s (%d - %d) - bailing\n",filename,t,fileinfo.st_buf.st_mtime);
#endif
        return 0;
    }

    key->data.file.device = fileinfo.st_buf.st_dev;
    key->data.file.inode  = fileinfo.st_buf.st_ino;
    /* 
     * If working with content management systems that like to munge the mtime, 
     * it might be appropriate to key off of the ctime to be immune to systems
     * that try to backdate a template.  If the mtime is set to something older
     * than the previous mtime of a template we will obviously never see this
     * "older" template.  At some point the Smarty templating system did this.
     * I generally disagree with using the ctime here because you lose the 
     * ability to warm up new content by saving it to a temporary file, hitting
     * it once to cache it and then renaming it into its permanent location so
     * set the apc.stat_ctime=true to enable this check.
     */
    if(APCG(stat_ctime)) {
        key->mtime  = (fileinfo.st_buf.st_ctime > fileinfo.st_buf.st_mtime) ? fileinfo.st_buf.st_ctime : fileinfo.st_buf.st_mtime; 
    } else {
        key->mtime = fileinfo.st_buf.st_mtime;
    }
    key->type = APC_CACHE_KEY_FILE;
    return 1;
}
/* }}} */

/* {{{ apc_cache_make_user_key */
int apc_cache_make_user_key(apc_cache_key_t* key, char* identifier, int identifier_len, const time_t t)
{
    assert(key != NULL);

    if (!identifier)
        return 0;

    key->data.user.identifier = identifier;
    key->data.user.identifier_len = identifier_len;
    key->mtime = t;
    key->type = APC_CACHE_KEY_USER;
    return 1;
}
/* }}} */

/* {{{ apc_cache_make_file_entry */
apc_cache_entry_t* apc_cache_make_file_entry(const char* filename,
                                        zend_op_array* op_array,
                                        apc_function_t* functions,
                                        apc_class_t* classes)
{
    apc_cache_entry_t* entry;

    entry = (apc_cache_entry_t*) apc_sma_malloc(sizeof(apc_cache_entry_t));
    if (!entry) return NULL;

    entry->data.file.filename  = apc_xstrdup(filename, apc_sma_malloc);
    if(!entry->data.file.filename) {
#ifdef __DEBUG_APC__
        fprintf(stderr,"apc_cache_make_file_entry: entry->data.file.filename is NULL - bailing\n");
#endif
        apc_sma_free(entry);
        return NULL;
    }
#ifdef __DEBUG_APC__
    fprintf(stderr,"apc_cache_make_file_entry: entry->data.file.filename is [%s]\n",entry->data.file.filename);
#endif
    entry->data.file.op_array  = op_array;
    entry->data.file.functions = functions;
    entry->data.file.classes   = classes;
    entry->type = APC_CACHE_ENTRY_FILE;
    entry->ref_count = 0;
    entry->mem_size = 0;
    entry->autofiltered = 0;
    entry->local = 0;
    return entry;
}
/* }}} */

/* {{{ apc_cache_store_zval */
zval* apc_cache_store_zval(zval* dst, const zval* src, apc_malloc_t allocate, apc_free_t deallocate)
{
    smart_str buf = {0};
    php_serialize_data_t var_hash;
    TSRMLS_FETCH();

    if((src->type & ~IS_CONSTANT_INDEX) == IS_OBJECT) {
        if(!dst) {
            CHECK(dst = (zval*) allocate(sizeof(zval)));
        }
		
        PHP_VAR_SERIALIZE_INIT(var_hash);
        php_var_serialize(&buf, (zval**)&src, &var_hash TSRMLS_CC);
        PHP_VAR_SERIALIZE_DESTROY(var_hash);
		
        dst->type = IS_NULL; /* in case we fail */
        if(buf.c) {
            dst->type = src->type & ~IS_CONSTANT_INDEX;
            dst->value.str.len = buf.len;
            CHECK(dst->value.str.val = apc_xmemcpy(buf.c, buf.len+1, allocate));
            dst->type = src->type;
            smart_str_free(&buf);
        }
        return dst; 
    } else {
        
        /* Maintain a list of zvals we've copied to properly handle recursive structures */
        HashTable *old = APCG(copied_zvals);
        APCG(copied_zvals) = emalloc(sizeof(HashTable));
        zend_hash_init(APCG(copied_zvals), 0, NULL, NULL, 0);
        
        dst = apc_copy_zval(dst, src, allocate, deallocate);

        if(APCG(copied_zvals)) {
            zend_hash_destroy(APCG(copied_zvals));
            efree(APCG(copied_zvals));
        }

        APCG(copied_zvals) = old;

        return dst;
    }
}
/* }}} */

/* {{{ apc_cache_fetch_zval */
zval* apc_cache_fetch_zval(zval* dst, const zval* src, apc_malloc_t allocate, apc_free_t deallocate)
{
    TSRMLS_FETCH();
    if((src->type & ~IS_CONSTANT_INDEX) == IS_OBJECT) {
        php_unserialize_data_t var_hash;
        const unsigned char *p = (unsigned char*)Z_STRVAL_P(src);

        PHP_VAR_UNSERIALIZE_INIT(var_hash);
        if(!php_var_unserialize(&dst, &p, p + Z_STRLEN_P(src), &var_hash TSRMLS_CC)) {
            PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
            zval_dtor(dst);
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Error at offset %ld of %d bytes", (long)((char*)p - Z_STRVAL_P(src)), Z_STRLEN_P(src));
            dst->type = IS_NULL;
        }
        PHP_VAR_UNSERIALIZE_DESTROY(var_hash);		
        return dst; 
    } else {
    
        /* Maintain a list of zvals we've copied to properly handle recursive structures */
        HashTable *old = APCG(copied_zvals);
        APCG(copied_zvals) = emalloc(sizeof(HashTable));
        zend_hash_init(APCG(copied_zvals), 0, NULL, NULL, 0);
        
        dst = apc_copy_zval(dst, src, allocate, deallocate);

        if(APCG(copied_zvals)) {
            zend_hash_destroy(APCG(copied_zvals));
            efree(APCG(copied_zvals));
        }

        APCG(copied_zvals) = old;

        return dst;
    }
}
/* }}} */

/* {{{ apc_cache_free_zval */
void apc_cache_free_zval(zval* src, apc_free_t deallocate)
{
    TSRMLS_FETCH();
    if ((src->type & ~IS_CONSTANT_INDEX) == IS_OBJECT) {
        if (src->value.str.val) {
        	deallocate(src->value.str.val);
        }
        deallocate(src);
    } else {
        /* Maintain a list of zvals we've copied to properly handle recursive structures */
        HashTable *old = APCG(copied_zvals);
        APCG(copied_zvals) = emalloc(sizeof(HashTable));
        zend_hash_init(APCG(copied_zvals), 0, NULL, NULL, 0);
        
        apc_free_zval(src, deallocate);

        if(APCG(copied_zvals)) {
            zend_hash_destroy(APCG(copied_zvals));
            efree(APCG(copied_zvals));
        }

        APCG(copied_zvals) = old;
    }
}
/* }}} */

/* {{{ apc_cache_make_user_entry */
apc_cache_entry_t* apc_cache_make_user_entry(const char* info, int info_len, const zval* val, const unsigned int ttl)
{
    apc_cache_entry_t* entry;

    entry = (apc_cache_entry_t*) apc_sma_malloc(sizeof(apc_cache_entry_t));
    if (!entry) return NULL;

    entry->data.user.info = apc_xmemcpy(info, info_len, apc_sma_malloc);
    entry->data.user.info_len = info_len;
    if(!entry->data.user.info) {
        apc_sma_free(entry);
        return NULL;
    }
    entry->data.user.val = apc_cache_store_zval(NULL, val, apc_sma_malloc, apc_sma_free);
    if(!entry->data.user.val) {
        apc_sma_free(entry->data.user.info);
        apc_sma_free(entry);
        return NULL;
    }
    INIT_PZVAL(entry->data.user.val);
    entry->data.user.ttl = ttl;
    entry->type = APC_CACHE_ENTRY_USER;
    entry->ref_count = 0;
    entry->mem_size = 0;
    entry->autofiltered = 0;
    entry->local = 0;
    return entry;
}
/* }}} */

/* {{{ apc_cache_free_entry */
void apc_cache_free_entry(apc_cache_entry_t* entry)
{
    if (entry != NULL) {
        assert(entry->ref_count == 0);
        switch(entry->type) {
            case APC_CACHE_ENTRY_FILE:
                apc_sma_free(entry->data.file.filename);
                apc_free_op_array(entry->data.file.op_array, apc_sma_free);
                apc_free_functions(entry->data.file.functions, apc_sma_free);
                apc_free_classes(entry->data.file.classes, apc_sma_free);
                break;
            case APC_CACHE_ENTRY_USER:
                apc_sma_free(entry->data.user.info);
                apc_cache_free_zval(entry->data.user.val, apc_sma_free);
                break;
        }
        apc_sma_free(entry);
    }
}
/* }}} */

/* {{{ apc_cache_info */
apc_cache_info_t* apc_cache_info(apc_cache_t* cache, zend_bool limited)
{
    apc_cache_info_t* info;
    slot_t* p;
    int i;

    if(!cache) return NULL;

    LOCK(cache);

    info = (apc_cache_info_t*) apc_emalloc(sizeof(apc_cache_info_t));
    if(!info) {
        UNLOCK(cache);
        return NULL;
    }
    info->num_slots = cache->num_slots;
    info->ttl = cache->ttl;
    info->num_hits = cache->header->num_hits;
    info->num_misses = cache->header->num_misses;
    info->list = NULL;
    info->deleted_list = NULL;
    info->start_time = cache->header->start_time;
    info->expunges = cache->header->expunges;
    info->mem_size = cache->header->mem_size;
    info->num_entries = cache->header->num_entries;
    info->num_inserts = cache->header->num_inserts;

    if(!limited) {
        /* For each hashtable slot */
        for (i = 0; i < info->num_slots; i++) {
            p = cache->slots[i];
            for (; p != NULL; p = p->next) {
                apc_cache_link_t* link = (apc_cache_link_t*) apc_emalloc(sizeof(apc_cache_link_t));

                if(p->value->type == APC_CACHE_ENTRY_FILE) {
                    link->data.file.filename = apc_xstrdup(p->value->data.file.filename, apc_emalloc);
                    link->data.file.device = p->key.data.file.device;
                    link->data.file.inode = p->key.data.file.inode;
                    link->type = APC_CACHE_ENTRY_FILE;
                } else if(p->value->type == APC_CACHE_ENTRY_USER) {
                    link->data.user.info = apc_xmemcpy(p->value->data.user.info, p->value->data.user.info_len, apc_emalloc);
                    link->data.user.ttl = p->value->data.user.ttl;
                    link->type = APC_CACHE_ENTRY_USER;
                }
                link->num_hits = p->num_hits;
                link->mtime = p->key.mtime;
                link->creation_time = p->creation_time;
                link->deletion_time = p->deletion_time;
                link->access_time = p->access_time;
                link->ref_count = p->value->ref_count;
                link->mem_size = p->value->mem_size;
                link->next = info->list;
                info->list = link;
            }
        }

        /* For each slot pending deletion */
        for (p = cache->header->deleted_list; p != NULL; p = p->next) {
            apc_cache_link_t* link = (apc_cache_link_t*) apc_emalloc(sizeof(apc_cache_link_t));

            if(p->value->type == APC_CACHE_ENTRY_FILE) {
                link->data.file.filename = apc_xstrdup(p->value->data.file.filename, apc_emalloc);
                if(p->key.type == APC_CACHE_KEY_FILE) {
                    link->data.file.device = p->key.data.file.device;
                    link->data.file.inode = p->key.data.file.inode;
                } else { /* This is a no-stat fullpath file entry */
                    link->data.file.device = 0;
                    link->data.file.inode = 0;
                }
                link->type = APC_CACHE_ENTRY_FILE;
            } else if(p->value->type == APC_CACHE_ENTRY_USER) {
                link->data.user.info = apc_xmemcpy(p->value->data.user.info, p->value->data.user.info_len, apc_emalloc);
                link->data.user.ttl = p->value->data.user.ttl;
                link->type = APC_CACHE_ENTRY_USER;
            }
            link->num_hits = p->num_hits;
            link->mtime = p->key.mtime;
            link->creation_time = p->creation_time;
            link->deletion_time = p->deletion_time;
            link->access_time = p->access_time;
            link->ref_count = p->value->ref_count;
            link->mem_size = p->value->mem_size;
            link->next = info->deleted_list;
            info->deleted_list = link;
        }
    }

    UNLOCK(cache);
    return info;
}
/* }}} */

/* {{{ apc_cache_free_info */
void apc_cache_free_info(apc_cache_info_t* info)
{
    apc_cache_link_t* p = info->list;
    apc_cache_link_t* q = NULL;
    while (p != NULL) {
        q = p;
        p = p->next;
        if(q->type == APC_CACHE_ENTRY_FILE) apc_efree(q->data.file.filename);
        else if(q->type == APC_CACHE_ENTRY_USER) apc_efree(q->data.user.info);
        apc_efree(q);
    }
    p = info->deleted_list;
    while (p != NULL) {
        q = p;
        p = p->next;
        if(q->type == APC_CACHE_ENTRY_FILE) apc_efree(q->data.file.filename);
        else if(q->type == APC_CACHE_ENTRY_USER) apc_efree(q->data.user.info);
        apc_efree(q);
    }
    apc_efree(info);
}
/* }}} */

/* {{{ apc_cache_unlock */
void apc_cache_unlock(apc_cache_t* cache)
{
    UNLOCK(cache);
}
/* }}} */

/* {{{ apc_cache_busy */
zend_bool apc_cache_busy(apc_cache_t* cache)
{
    return cache->header->busy;
}
/* }}} */

#if NONBLOCKING_LOCK_AVAILABLE
/* {{{ apc_cache_write_lock */
zend_bool apc_cache_write_lock(apc_cache_t* cache)
{
    return apc_lck_nb_lock(cache->header->wrlock);
}
/* }}} */

/* {{{ apc_cache_write_unlock */
void apc_cache_write_unlock(apc_cache_t* cache)
{
    apc_lck_unlock(cache->header->wrlock);
}
/* }}} */
#endif

/* {{{ make_local_slot */
static local_slot_t* make_local_slot(apc_local_cache_t* cache, local_slot_t* lslot, slot_t* slot, time_t t) 
{
    apc_cache_entry_t* value;

    value = apc_emalloc(sizeof(apc_cache_entry_t));
    memcpy(value, slot->value, sizeof(apc_cache_entry_t)); /* bitwise copy */
    value->local = 1;

    lslot->original = slot;
    lslot->value = value;
    lslot->num_hits = 0;
    lslot->creation_time = t;

    return lslot; /* for what joy ? ... consistency */
}
/* }}} */

/* {{{ free_local_slot */
static void free_local_slot(apc_local_cache_t* cache, local_slot_t* lslot) 
{
    local_slot_t * dead = NULL;
    if(!lslot->original) return;

    /* TODO: Bad design to allocate memory in a free_* - fix when bored (hehe) */
    dead = apc_emalloc(sizeof(local_slot_t));
    memcpy(dead, lslot, sizeof(local_slot_t)); /* bitwise copy */

    lslot->original = NULL;
    lslot->value = NULL;

    dead->next = cache->dead_list;
    cache->dead_list = dead;
}
/* }}} */

/* {{{ apc_local_cache_create */
apc_local_cache_t* apc_local_cache_create(apc_cache_t *shmcache, int num_slots, int ttl)
{
    apc_local_cache_t* cache = NULL;

    cache = (apc_local_cache_t*) apc_emalloc(sizeof(apc_local_cache_t));

    cache->slots = (local_slot_t*) (apc_emalloc(sizeof(local_slot_t) * num_slots));
    memset(cache->slots, 0, sizeof(local_slot_t) * num_slots);

    cache->shmcache = shmcache;
    cache->num_slots = num_slots;
    cache->ttl = ttl;
    cache->num_hits = 0;
    cache->generation = shmcache->header->expunges;
    cache->dead_list = NULL;

    return cache;
}
/* }}} */

/* {{{ apc_local_cache_cleanup */
void apc_local_cache_cleanup(apc_local_cache_t* cache) {
    local_slot_t * lslot;
    time_t t = time(0);
    
    int i;
    for(i = 0; i < cache->num_slots; i++) {
        lslot = &cache->slots[i];
        /* every slot lives for exactly TTL seconds */
        if((lslot->original && lslot->creation_time < (t - cache->ttl)) ||
                cache->generation != cache->shmcache->header->expunges) {
            free_local_slot(cache, lslot);
        }
    }

    LOCK(cache->shmcache);
    for(lslot = cache->dead_list; lslot != NULL; lslot = lslot->next) {
        lslot->original->num_hits += lslot->num_hits;
        lslot->original->value->ref_count--; /* apc_cache_release(cache->shmcache, lslot->original->value); */
        apc_efree(lslot->value);
    }
    UNLOCK(cache->shmcache);

    cache->dead_list = NULL;
}
/* }}} */

/* {{{ apc_local_cache_destroy */
void apc_local_cache_destroy(apc_local_cache_t* cache)
{
    int i;
    for(i = 0; i < cache->num_slots; i++) {
        free_local_slot(cache, &cache->slots[i]);
    }

    apc_local_cache_cleanup(cache);

    LOCK(cache->shmcache);
    cache->shmcache->header->num_hits += cache->num_hits;
    UNLOCK(cache->shmcache);

    apc_efree(cache->slots);
    apc_efree(cache);
}
/* }}} */

/* {{{ apc_local_cache_find */
apc_cache_entry_t* apc_local_cache_find(apc_local_cache_t* cache, apc_cache_key_t key, time_t t)
{
    slot_t* slot;
    local_slot_t* lslot; 

    if(key.type == APC_CACHE_KEY_FILE) lslot = &cache->slots[hash(key) % cache->num_slots];
    else lslot = &cache->slots[string_nhash_8(key.data.fpfile.fullpath, key.data.fpfile.fullpath_len) % cache->num_slots];

    slot = lslot->original;

    if(slot && key.type == slot->key.type) {
        if(slot->access_time < (t - cache->ttl)) {
            goto not_found;
        }
        if(key.type == APC_CACHE_KEY_FILE && 
           key_equals(slot->key.data.file, key.data.file)) {
            if(slot->key.mtime != key.mtime) {
                free_local_slot(cache, lslot);
                goto not_found;
            }
            cache->num_hits++;
            lslot->num_hits++;
            lslot->original->access_time = t; /* unlocked write, but last write wins */
            return lslot->value;
        } else if(key.type == APC_CACHE_KEY_FPFILE) {
            if(!memcmp(slot->key.data.fpfile.fullpath, key.data.fpfile.fullpath, key.data.fpfile.fullpath_len+1)) {
                cache->num_hits++;
                lslot->num_hits++;
                lslot->original->access_time = t; /* unlocked write, but last write wins */
                return lslot->value;
            }
        }
    }
not_found:
    if(apc_cache_busy(cache->shmcache)) {
        return NULL;
    }

    slot = apc_cache_find_slot(cache->shmcache, key, t);

    if(!slot) return NULL;
   
    /* i.e maintain a sort of top list */
    if(lslot->original == NULL || (lslot->original->num_hits + lslot->num_hits)  < slot->num_hits) {
        free_local_slot(cache, lslot);
        make_local_slot(cache, lslot, slot, t); 
        return lslot->value;
    }
    return slot->value;
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
