/* 
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,       |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/3_0.txt.                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   |          Rasmus Lerdorf <rasmus@php.net>                             |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "apc_cache.h"
#include "apc_lock.h"
#include "apc_sma.h"
#include "SAPI.h"

/* TODO: rehash when load factor exceeds threshold */

/* {{{ locking macros */
#define CREATE_LOCK     apc_lck_create(NULL, 0, 1)
#define DESTROY_LOCK(c) apc_lck_destroy(c->lock)
#define LOCK(c)         { HANDLE_BLOCK_INTERRUPTIONS(); apc_lck_lock(c->lock); }
#define UNLOCK(c)       { apc_lck_unlock(c->lock); HANDLE_UNBLOCK_INTERRUPTIONS(); }
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
    int num_hits;               /* total successful hits in cache */
    int num_misses;             /* total unsuccessful hits in cache */
    slot_t* deleted_list;       /* linked list of to-be-deleted slots */
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
    int lock;                   /* global semaphore lock */
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
	register unsigned int n;

	if (len > 3) {
		if (len & 3) {
			h = *(unsigned int *)(s + len - 4);
		}
		len /= 4;
		for (n = 0; n < len; n++) {
			h+= iv[n];
			h = (h << 7) | (h >> (32 - 7));
		}
	} else {
		if (len > 1) {
			h += s[1];
			if (len == 3) h += s[2];
		}
		h += s[0];
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
    apc_cache_free_entry(slot->value);
    apc_sma_free(slot);
}
/* }}} */

/* {{{ remove_slot */
static void remove_slot(apc_cache_t* cache, slot_t** slot)
{
    slot_t* dead = *slot;
    *slot = (*slot)->next;

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

    entry->data.file.op_array->refcount[0] = BIG_VALUE;
    if (entry->data.file.functions) {
        int i;
        apc_function_t* fns = entry->data.file.functions;
        for (i=0; fns[i].function != NULL; i++) {
            fns[i].function->op_array.refcount[0] = BIG_VALUE;
        }
    }
    if (entry->data.file.classes) {
        int i;
        apc_class_t* classes = entry->data.file.classes;
        for (i=0; classes[i].class_entry != NULL; i++) {
            classes[i].class_entry->refcount[0] = BIG_VALUE;
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
    memset(cache->shmaddr, 0, cache_size);

    cache->header = (header_t*) cache->shmaddr;
    cache->header->num_hits = 0;
    cache->header->num_misses = 0;
    cache->header->deleted_list = NULL;

    cache->slots = (slot_t**) (((char*) cache->shmaddr) + sizeof(header_t));
    cache->num_slots = num_slots;
    cache->gc_ttl = gc_ttl;
    cache->ttl = ttl;
    cache->lock = CREATE_LOCK;

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

    cache->header->num_hits = 0;
    cache->header->num_misses = 0;

    for (i = 0; i < cache->num_slots; i++) {
        slot_t* p = cache->slots[i];
        while (p) {
            remove_slot(cache, &p);
        }
        cache->slots[i] = NULL;
    }
    
    UNLOCK(cache);
}
/* }}} */

/* {{{ apc_cache_expunge */
void apc_cache_expunge(apc_cache_t* cache, time_t t)
{
    int i;
    slot_t *p;

    if(!cache || (cache && !cache->ttl)) return;

    LOCK(cache);
    for (i = 0; i < cache->num_slots; i++) {
        p = cache->slots[i];
        while(p) {
            if(p->access_time < (t - cache->ttl)) {
                remove_slot(cache, &p);
                continue;
            }
            p = p->next;
        }
    }
    UNLOCK(cache);
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

    LOCK(cache);
    process_pending_removals(cache);

    slot = &cache->slots[hash(key) % cache->num_slots];

    while (*slot) {
        if (key_equals((*slot)->key.data.file, key.data.file)) {
            /* If existing slot for the same device+inode is older, remove it and insert the new version */
            if ((*slot)->key.mtime < key.mtime) {
                remove_slot(cache, slot);
                break;
            }
            UNLOCK(cache);
            return 0;
        } else if(cache->ttl && (*slot)->access_time < (t - cache->ttl)) {
            remove_slot(cache, slot);
            continue;
        }
        slot = &(*slot)->next;
    }

    if ((*slot = make_slot(key, value, *slot, t)) == NULL) {
        UNLOCK(cache);
        return 0;
    }

    UNLOCK(cache);
    return 1;
}
/* }}} */

/* {{{ apc_cache_user_insert */
int apc_cache_user_insert(apc_cache_t* cache, apc_cache_key_t key, apc_cache_entry_t* value, time_t t)
{
    slot_t** slot;
    int ilen;

    if (!value) {
        return 0;
    }

    LOCK(cache);
    process_pending_removals(cache);

    ilen = strlen(key.data.user.identifier);
    slot = &cache->slots[string_nhash_8(key.data.user.identifier, ilen) % cache->num_slots];

    while (*slot) {
        if (!strncmp((*slot)->key.data.user.identifier, key.data.user.identifier, ilen)) {
            /* If a slot with the same identifier already exists, remove it */
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

    if ((*slot = make_slot(key, value, *slot, t)) == NULL) {
        UNLOCK(cache);
        return 0;
    }

    UNLOCK(cache);
    return 1;
}
/* }}} */

/* {{{ apc_cache_find */
apc_cache_entry_t* apc_cache_find(apc_cache_t* cache, apc_cache_key_t key, time_t t)
{
    slot_t** slot;

    LOCK(cache);

    slot = &cache->slots[hash(key) % cache->num_slots];

    while (*slot) {
        if (key_equals((*slot)->key.data.file, key.data.file)) {
            if ((*slot)->key.mtime < key.mtime) {
                remove_slot(cache, slot);
                break;
            }

            (*slot)->num_hits++;
            (*slot)->value->ref_count++;
            (*slot)->access_time = t;
            prevent_garbage_collection((*slot)->value);

            cache->header->num_hits++;
            UNLOCK(cache);
            return (*slot)->value;
        }
        slot = &(*slot)->next;
    }

    cache->header->num_misses++;
    UNLOCK(cache);
    return NULL;
}
/* }}} */

/* {{{ apc_cache_user_find */
apc_cache_entry_t* apc_cache_user_find(apc_cache_t* cache, char *strkey, int keylen, time_t t)
{
    slot_t** slot;

    LOCK(cache);

    slot = &cache->slots[string_nhash_8(strkey, keylen) % cache->num_slots];

    while (*slot) {
        if (!strncmp((*slot)->key.data.user.identifier, strkey, keylen)) {
            /* Check to make sure this entry isn't expired by a hard TTL */
            if((*slot)->value->data.user.ttl && ((*slot)->creation_time + (*slot)->value->data.user.ttl) < t) {
                remove_slot(cache, slot);
                break;
            }
            /* Otherwise we are fine, increase counters and return the cache entry */
            (*slot)->num_hits++;
            (*slot)->value->ref_count++;
            (*slot)->access_time = t;

            cache->header->num_hits++;
            UNLOCK(cache);
            return (*slot)->value;
        }
        slot = &(*slot)->next;
    }

    cache->header->num_misses++;
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
        if (!strncmp((*slot)->key.data.user.identifier, strkey, keylen)) {
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
    LOCK(cache);
    entry->ref_count--;
    UNLOCK(cache);
}
/* }}} */

/* {{{ apc_cache_make_file_key */
int apc_cache_make_file_key(apc_cache_key_t* key,
                       const char* filename,
                       const char* include_path
					   TSRMLS_DC)
{
    struct stat buf, *tmp_buf=NULL;

    assert(key != NULL);

    if (!filename || !SG(request_info).path_translated)
        return 0;

    if(!strcmp(SG(request_info).path_translated, filename)) {
        tmp_buf = sapi_get_stat(TSRMLS_C);  /* Apache has already done this stat() for us */
    }
    if(tmp_buf) buf = *tmp_buf;
    else if (stat(filename, &buf) != 0 &&
        apc_stat_paths(filename, include_path, &buf) != 0)
    {
        return 0;
    }

    key->data.file.device = buf.st_dev;
    key->data.file.inode  = buf.st_ino;
    key->mtime  = (buf.st_ctime > buf.st_mtime) ? buf.st_ctime : buf.st_mtime;
    return 1;
}
/* }}} */

/* {{{ apc_cache_make_user_key */
int apc_cache_make_user_key(apc_cache_key_t* key, const char* identifier, const time_t t)
{
    assert(key != NULL);

    if (!identifier)
        return 0;

    key->data.user.identifier = apc_xstrdup(identifier, apc_sma_malloc);
    key->mtime = t;
    return 1;
}
/* }}} */

/* {{{ apc_cache_free_user_key */
int apc_cache_free_user_key(apc_cache_key_t* key)
{
    assert(key != NULL);

    apc_sma_free(key->data.user.identifier);
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
        apc_sma_free(entry);
        return NULL;
    }
    entry->data.file.op_array  = op_array;
    entry->data.file.functions = functions;
    entry->data.file.classes   = classes;
    entry->type = APC_CACHE_ENTRY_FILE;
    entry->ref_count = 0;
    return entry;
}
/* }}} */

/* {{{ apc_cache_make_user_entry */
apc_cache_entry_t* apc_cache_make_user_entry(const char* info, const zval* val, const unsigned int ttl)
{
    apc_cache_entry_t* entry;

    entry = (apc_cache_entry_t*) apc_sma_malloc(sizeof(apc_cache_entry_t));
    if (!entry) return NULL;

    entry->data.user.info  = apc_xstrdup(info, apc_sma_malloc);
    if(!entry->data.user.info) {
        apc_sma_free(entry);
        return NULL;
    }
    entry->data.user.val = apc_copy_zval(NULL, val, apc_sma_malloc, apc_sma_free);
    if(!entry->data.user.val) {
        apc_sma_free(entry->data.user.info);
        apc_sma_free(entry);
        return NULL;
    }
    entry->data.user.ttl = ttl;
    entry->type = APC_CACHE_ENTRY_USER;
    entry->ref_count = 0;
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
                apc_free_zval(entry->data.user.val, apc_sma_free);
                break;
        }
        apc_sma_free(entry);
    }
}
/* }}} */

/* {{{ apc_cache_info */
apc_cache_info_t* apc_cache_info(apc_cache_t* cache)
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
                link->data.user.info = apc_xstrdup(p->value->data.user.info, apc_emalloc);
                link->data.user.ttl = p->value->data.user.ttl;
                link->type = APC_CACHE_ENTRY_USER;
            }
            link->num_hits = p->num_hits;
            link->mtime = p->key.mtime;
            link->creation_time = p->creation_time;
            link->deletion_time = p->deletion_time;
            link->access_time = p->access_time;
            link->ref_count = p->value->ref_count;
            link->next = info->list;
            info->list = link;
        }
    }

    /* For each slot pending deletion */
    for (p = cache->header->deleted_list; p != NULL; p = p->next) {
        apc_cache_link_t* link = (apc_cache_link_t*)
            apc_emalloc(sizeof(apc_cache_link_t));

        if(p->value->type == APC_CACHE_ENTRY_FILE) {
            link->data.file.filename = apc_xstrdup(p->value->data.file.filename, apc_emalloc);
            link->data.file.device = p->key.data.file.device;
            link->data.file.inode = p->key.data.file.inode;
            link->type = APC_CACHE_ENTRY_FILE;
        } else if(p->value->type == APC_CACHE_ENTRY_USER) {
            link->data.user.info = apc_xstrdup(p->value->data.user.info, apc_emalloc);
            link->data.user.ttl = p->value->data.user.ttl;
            link->type = APC_CACHE_ENTRY_USER;
        }
        link->num_hits = p->num_hits;
        link->mtime = p->key.mtime;
        link->creation_time = p->creation_time;
        link->deletion_time = p->deletion_time;
        link->access_time = p->access_time;
        link->ref_count = p->value->ref_count;
        link->next = info->deleted_list;
        info->deleted_list = link;
    }

    UNLOCK(cache);
    return info;
}
/* }}} */

/* {{{ apc_cache_free_info */
void apc_cache_free_info(apc_cache_info_t* info)
{
    apc_cache_link_t* p = info->list;
    while (p != NULL) {
        apc_cache_link_t* q = p;
        p = p->next;
        if(q->type == APC_CACHE_ENTRY_FILE) apc_efree(q->data.file.filename);
        else if(q->type == APC_CACHE_ENTRY_USER) apc_efree(q->data.user.info);
        apc_efree(q);
    }
    apc_efree(info);
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
