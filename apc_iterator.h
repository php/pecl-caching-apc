/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2008 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Brian Shire <shire@.php.net>                                |
  +----------------------------------------------------------------------+

 */

/* $Id$ */

#ifndef APC_ITERATOR_H
#define APC_ITERATOR_H

#include "apc.h"
#include "apc_stack.h"

#if HAVE_PCRE || HAVE_BUNDLED_PCRE
#include "ext/pcre/php_pcre.h"
#include "ext/standard/php_smart_str.h"
#endif


#define APC_ITERATOR_NAME "APCIterator"

#define APC_DEFAULT_CHUNK_SIZE 100

#define APC_LIST_ACTIVE   0x1
#define APC_LIST_DELETED  0x2

#define APC_ITER_KEY    0x01
#define APC_ITER_VALUE  0x02
#define APC_ITER_INFO   0x04
#define APC_ITER_ALL    (APC_ITER_KEY | APC_ITER_VALUE | APC_ITER_INFO)

typedef void* (*apc_iterator_item_cb_t)(slot_t **slot);


/* {{{ apc_iterator_t */
typedef struct _apc_iterator_t {
    zend_object obj;         /* must always be first */
    short int initialized;   /* sanity check in case __construct failed */
    long format;             /* format bitmask of the return values ie: key, value, info */
    int (*fetch)(struct _apc_iterator_t *iterator);
                             /* fetch callback to fetch items from cache slots or lists */
    apc_cache_t *cache;      /* cache which we are iterating on */
    long slot_idx;           /* index to the slot array or linked list */
    long chunk_size;         /* number of entries to pull down per fetch */
    apc_stack_t *stack;      /* stack of entries pulled from cache */
    int stack_idx;           /* index into the current stack */
    pcre *re;                /* regex filter on entry identifiers */
    char *regex;             /* original regex expression or NULL */
    int regex_len;           /* regex length */
    long key_idx;            /* incrementing index for numerical keys */
    short int totals_flag;   /* flag if totals have been calculated */
    long hits;               /* hit total */
    size_t size;             /* size total */
    long count;              /* count total */
} apc_iterator_t;
/* }}} */

/* {{{ apc_iterator_item */
typedef struct _apc_iterator_item_t {
    char *key;              /* string key */
    long key_len;           /* strlen of key */
    zval *info;             /* array of entry info */
    zval *value;
} apc_iterator_item_t;
/* }}} */


extern int apc_iterator_init(int module_number TSRMLS_DC);
extern int apc_iterator_delete(zval *zobj TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
