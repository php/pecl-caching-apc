/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2008 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Gopal Vijayaraghavan <gopalv@yahoo-inc.com>                 |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Yahoo! Inc. in 2008.
   
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$ */


#include "apc_pool.h"
#include <assert.h>

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

/* {{{ typedefs */
typedef struct _pool_block
{
    size_t              avail;
    size_t              capacity;
    unsigned char       *mark;
    struct _pool_block  *next;
    unsigned             :0; /* this should align to word */
    unsigned char       data[0]; 
}pool_block;

/*
   parts in ? are optional and turned on for fun, memory loss,
   and for something else that I forgot about ... ah, debugging

                 |--------> data[0]         |<-- non word boundary (too)
   +-------------+--------------+-----------+-------------+-------------->>>
   | pool_block  | ?sizeinfo<1> | block<1>  | ?redzone<1> | ?sizeinfo<2> 
   |             |  (size_t)    |           | padded left |    
   +-------------+--------------+-----------+-------------+-------------->>>
 */

struct _apc_pool
{
    apc_malloc_t allocate;
    apc_free_t   deallocate;

    size_t     dsize;
    void       *owner;

    struct
    {
        unsigned int redzones:1;
        unsigned int sizeinfo:1;
    } options;

    pool_block *head;
};
/* }}} */

/* {{{ redzone code */
static const unsigned char decaff[] =  {
    0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad,
    0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad,
    0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad,
    0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad
};

/* a redzone is at least 4 (0xde,0xca,0xc0,0xff) bytes */
#define REDZONE_SIZE(size) \
    ((ALIGNWORD((size)) > ((size) + 4)) ? \
        (ALIGNWORD((size)) - (size)) : /* does not change realsize */\
        ALIGNWORD((size)) - (size) + ALIGNWORD((sizeof(char)))) /* adds 1 word to realsize */
    
#define SIZEINFO_SIZE ALIGNWORD(sizeof(size_t))

#define MARK_REDZONE(block, redsize) do {\
       memcpy(block, decaff, redsize );\
    } while(0)

#define CHECK_REDZONE(block, redsize) (memcmp(block, decaff, redsize) == 0)

/* }}} */

#define APC_POOL_OPTION(pool, option) ((pool)->options.option)

/* {{{ create_pool_block */
static pool_block* create_pool_block(apc_pool *pool, size_t size)
{
    size_t realsize = sizeof(pool_block) + ALIGNWORD(size);
    
    pool_block* entry = pool->allocate(realsize);

    entry->avail = entry->capacity = size;

    entry->mark = entry->data;

    entry->next = pool->head;

    pool->head = entry;

    return entry;
}
/* }}} */

/* {{{ apc_pool_create */
apc_pool* apc_pool_create(apc_pool_type pool_type, 
                            apc_malloc_t allocate, 
                            apc_free_t deallocate)
{
    apc_pool* pool = NULL;
    size_t dsize = 0;

    /* sanity checks */
    assert(sizeof(decaff) > REDZONE_SIZE(ALIGNWORD(sizeof(char))));
    assert(sizeof(pool_block) == ALIGNWORD(sizeof(pool_block)));

    switch(pool_type) {
        case APC_SMALL_POOL:
            dsize = 512;
            break;

        case APC_LARGE_POOL:
            dsize = 8192;
            break;

        case APC_MEDIUM_POOL:
            dsize = 4096;
            break;

        default:
            return NULL;
    }

    pool = (apc_pool*)allocate(sizeof(apc_pool));

    if(!pool) {
        return NULL;
    }

    pool->allocate = allocate;
    pool->deallocate = deallocate;
    pool->dsize = dsize;
    pool->head = NULL;

    APC_POOL_OPTION(pool, redzones) = 1;
    APC_POOL_OPTION(pool, sizeinfo) = 1;

    if(!create_pool_block(pool, dsize)) {
        deallocate(pool);
        return NULL;
    }

    return pool; 
}
/* }}} */

/* {{{ apc_pool_destroy */
void apc_pool_destroy(apc_pool *pool)
{

    apc_free_t deallocate = pool->deallocate;
    pool_block *entry;
    pool_block *tmp;

    entry = pool->head;

    while(entry != NULL) {
        tmp = entry->next;
        deallocate(entry);
        entry = tmp;
    }

    deallocate(pool);
}
/* }}} */

/* {{{ apc_pool_alloc */
void* apc_pool_alloc(apc_pool *pool, size_t size)
{
    unsigned char *p = NULL;
    size_t realsize = ALIGNWORD(size);
    size_t poolsize;
    unsigned char *redzone  = NULL;
    size_t redsize  = 0;
    size_t *sizeinfo= NULL;

    pool_block *entry;


    if(APC_POOL_OPTION(pool, redzones)) {
        redsize = REDZONE_SIZE(size); /* redsize might be re-using word size padding */
        realsize = size + redsize;    /* recalculating realsize */
    } else {
        redsize = realsize - size; /* use padding space */
    }

    if(APC_POOL_OPTION(pool, sizeinfo)) {
        realsize += ALIGNWORD(sizeof(size_t));
    }


    for(entry = pool->head; entry != NULL; entry = entry->next) {
        if(entry->avail >= realsize) {
            goto found;
        }
    }

    poolsize = ALIGNSIZE(realsize, pool->dsize);

    entry = create_pool_block(pool, poolsize);

    if(!entry) {
        return NULL;
    }

found:
    p = entry->mark;

    if(APC_POOL_OPTION(pool, sizeinfo)) {
        sizeinfo = (size_t*)p;
        p += SIZEINFO_SIZE;
        *sizeinfo = size;
    }
    
    redzone = p + size;

    if(APC_POOL_OPTION(pool, redzones)) {
        MARK_REDZONE(redzone, redsize);
    }

#ifdef VALGRIND_MAKE_MEM_NOACCESS
    if(redsize != 0) {
        VALGRIND_MAKE_MEM_NOACCESS(redzone, redsize);
    }
#endif

    entry->avail -= realsize;
    entry->mark  += realsize;

#ifdef VALGRIND_MAKE_MEM_UNDEFINED
    /* need to write before reading data off this */
    VALGRIND_MAKE_MEM_UNDEFINED(p, size);
#endif

    return (void*)p;
}
/* }}} */

/* {{{ apc_pool_free */
/*
 * free does not do anything other than
 * check for redzone values when free'ing
 * data areas.
 */
void apc_pool_free(apc_pool *pool, void *p)
{
    if(!APC_POOL_OPTION(pool, sizeinfo) || 
        !APC_POOL_OPTION(pool, redzones)) {
    }
}
/* }}} */

/* {{{ apc_pool_check_integrity */
/*
 * Checking integrity at runtime, does an
 * overwrite check only when the sizeinfo
 * is set.
 */
int apc_pool_check_integrity(apc_pool *pool) 
{
    pool_block *entry;
    size_t *sizeinfo = NULL;
    unsigned char *start;
    size_t realsize;
    unsigned char   *redzone;
    size_t redsize;

    for(entry = pool->head; entry != NULL; entry = entry->next) {
        start = (unsigned char *)entry + ALIGNWORD(sizeof(pool_block));
        if((entry->mark - start) != (entry->capacity - entry->avail)) {
            return 0;
        }
    }

    if(!APC_POOL_OPTION(pool, sizeinfo) || 
        !APC_POOL_OPTION(pool, redzones)) {
        return 1;
    }

    for(entry = pool->head; entry != NULL; entry = entry->next) {
        start = (unsigned char *)entry + ALIGNWORD(sizeof(pool_block));
        
        while(start < entry->mark) {
            sizeinfo = (size_t*)start;
            /* redzone starts where real data ends, in a non-word boundary
             * redsize is at least 4 bytes + whatever's needed to make it
             * to another word boundary.
             */
            redzone = start + SIZEINFO_SIZE + (*sizeinfo);
            redsize = REDZONE_SIZE(*sizeinfo);
            if(!CHECK_REDZONE(redzone, redsize))
            {
                /*
                fprintf(stderr, "Redzone check failed for %p\n", 
                                start + ALIGNWORD(sizeof(size_t)));*/
                return 0;
            }
            realsize = SIZEINFO_SIZE + *sizeinfo + redsize;
            start += realsize;
        }
    }

    return 1;
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
