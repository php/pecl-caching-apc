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
#define LOCK(c)         apc_lck_lock(c->lock)
#define UNLOCK(c)       apc_lck_unlock(c->lock)
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
    int lock;                   /* global semaphore lock */
};
/* }}} */

/* {{{ key_equals */
#define key_equals(a, b) (a.inode==b.inode && a.device==b.device)
/* }}} */

/* {{{ hash */
static unsigned int hash(apc_cache_key_t key)
{
    return key.device + key.inode;
}
/* }}} */

/* {{{ make_slot */
slot_t* make_slot(apc_cache_key_t key, apc_cache_entry_t* value, slot_t* next)
{
    slot_t* p = apc_sma_malloc(sizeof(slot_t));
    if (!p) return NULL;
    p->key = key;
    p->value = value;
    p->next = next;
    p->num_hits = 0;
    p->creation_time = time(0);
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

            if (dead->value->ref_count > 0)
                apc_log(APC_WARNING, "GC cache entry '%s' (dev=%d ino=%d) "
                        "was on gc-list for %d seconds", dead->value->filename,
                        dead->key.device, dead->key.inode, gc_sec);

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

    entry->op_array->refcount[0] = BIG_VALUE;
    if (entry->functions) {
        int i;
        apc_function_t* fns = entry->functions;
        for (i=0; fns[i].function != NULL; i++) {
            fns[i].function->op_array.refcount[0] = BIG_VALUE;
        }
    }
    if (entry->classes) {
        int i;
        apc_class_t* classes = entry->classes;
        for (i=0; classes[i].class_entry != NULL; i++) {
            classes[i].class_entry->refcount[0] = BIG_VALUE;
        }
    }
}
/* }}} */

/* {{{ apc_cache_create */
apc_cache_t* apc_cache_create(int size_hint, int gc_ttl)
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

/* {{{ apc_cache_insert */
int apc_cache_insert(apc_cache_t* cache,
                     apc_cache_key_t key,
                     apc_cache_entry_t* value)
{
    slot_t** slot;

    if (!value) {
        return 0;
    }

    LOCK(cache);
    process_pending_removals(cache);

    slot = &cache->slots[hash(key) % cache->num_slots];
    while (*slot) {
        if (key_equals((*slot)->key, key)) {
            if ((*slot)->key.mtime < key.mtime) {
                remove_slot(cache, slot);
                break;
            }
            UNLOCK(cache);
            return 0;
        }
        slot = &(*slot)->next;
    }

    if ((*slot = make_slot(key, value, *slot)) == NULL) {
        UNLOCK(cache);
        return 0;
    }

    UNLOCK(cache);
    return 1;
}
/* }}} */

/* {{{ apc_cache_find */
apc_cache_entry_t* apc_cache_find(apc_cache_t* cache, apc_cache_key_t key)
{
    slot_t** slot;

    LOCK(cache);

    slot = &cache->slots[hash(key) % cache->num_slots];
    while (*slot) {
        if (key_equals((*slot)->key, key)) {
            if ((*slot)->key.mtime < key.mtime) {
                remove_slot(cache, slot);
                break;
            }

            (*slot)->num_hits++;
            (*slot)->value->ref_count++;
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

/* {{{ apc_cache_release */
void apc_cache_release(apc_cache_t* cache, apc_cache_entry_t* entry)
{
    LOCK(cache);
    entry->ref_count--;
    UNLOCK(cache);
}
/* }}} */

/* {{{ apc_cache_make_key */
int apc_cache_make_key(apc_cache_key_t* key,
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

    key->device = buf.st_dev;
    key->inode  = buf.st_ino;
    key->mtime  = buf.st_mtime;
    return 1;
}
/* }}} */

/* {{{ apc_cache_make_entry */
apc_cache_entry_t* apc_cache_make_entry(const char* filename,
                                        zend_op_array* op_array,
                                        apc_function_t* functions,
                                        apc_class_t* classes)
{
    apc_cache_entry_t* entry;

    entry = (apc_cache_entry_t*) apc_sma_malloc(sizeof(apc_cache_entry_t));
    if (!entry) return NULL;
    entry->filename  = apc_xstrdup(filename, apc_sma_malloc);
    entry->op_array  = op_array;
    entry->functions = functions;
    entry->classes   = classes;
    entry->ref_count = 0;
    return entry;
}
/* }}} */

/* {{{ apc_cache_free_entry */
void apc_cache_free_entry(apc_cache_entry_t* entry)
{
    if (entry != NULL) {
        assert(entry->ref_count == 0);
        apc_sma_free(entry->filename);
        apc_free_op_array(entry->op_array, apc_sma_free);
        apc_free_functions(entry->functions, apc_sma_free);
        apc_free_classes(entry->classes, apc_sma_free);
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

    LOCK(cache);

    info = (apc_cache_info_t*) apc_emalloc(sizeof(apc_cache_info_t));
    info->num_slots = cache->num_slots;
    info->num_hits = cache->header->num_hits;
    info->num_misses = cache->header->num_misses;
    info->list = NULL;
    info->deleted_list = NULL;

    /* For each hashtable slot */
    for (i = 0; i < info->num_slots; i++) {
        p = cache->slots[i];
        for (; p != NULL; p = p->next) {
            apc_cache_link_t* link = (apc_cache_link_t*)
                apc_emalloc(sizeof(apc_cache_link_t));

            link->filename = apc_xstrdup(p->value->filename, apc_emalloc);
            link->device = p->key.device;
            link->inode = p->key.inode;
            link->num_hits = p->num_hits;
            link->mtime = p->key.mtime;
            link->creation_time = p->creation_time;
            link->deletion_time = p->deletion_time;
            link->ref_count = p->value->ref_count;
            link->next = info->list;
            info->list = link;
        }
    }

    /* For each slot pending deletion */
    for (p = cache->header->deleted_list; p != NULL; p = p->next) {
        apc_cache_link_t* link = (apc_cache_link_t*)
            apc_emalloc(sizeof(apc_cache_link_t));

        link->filename = apc_xstrdup(p->value->filename, apc_emalloc);
        link->device = p->key.device;
        link->inode = p->key.inode;
        link->num_hits = p->num_hits;
        link->mtime = p->key.mtime;
        link->creation_time = p->creation_time;
        link->deletion_time = p->deletion_time;
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
        apc_efree(q->filename);
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
