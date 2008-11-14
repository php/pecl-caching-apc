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

#include "php_apc.h"
#include "apc_iterator.h"
#include "apc_cache.h"
#include "apc_zend.h"

#include "zend_interfaces.h"

zend_class_entry *apc_iterator_ce;
zend_object_handlers apc_iterator_object_handlers;

/* {{{ apc_iterator_item_key */
static void apc_iterator_item_key(slot_t **slot_pp, apc_iterator_item_t *item) {
    slot_t *slot = *slot_pp;

    if (slot->key.type == APC_CACHE_KEY_FILE) {
        item->key = estrdup(slot->value->data.file.filename);
        item->key_len = strlen(item->key);
    } else if (slot->key.type == APC_CACHE_KEY_USER) {
        item->key = estrndup((char*)slot->key.data.user.identifier, slot->key.data.user.identifier_len);
        item->key_len = slot->key.data.user.identifier_len;
    } else if (slot->key.type == APC_CACHE_KEY_FPFILE) {
        item->key = estrndup((char*)slot->key.data.fpfile.fullpath, slot->key.data.fpfile.fullpath_len);
        item->key_len = slot->key.data.fpfile.fullpath_len;
    } else {
        apc_eprint("Internal error, invalid entry type.");
    }
    item->info = NULL;

    return;
}
/* }}} */

/* {{{ apc_iterator_item_info */
static void apc_iterator_item_value(slot_t **slot_pp, apc_iterator_item_t *item) {
    slot_t *slot = *slot_pp;
    zval *zvalue;
    apc_context_t ctxt = {0, };

    ctxt.pool = apc_pool_create(APC_UNPOOL, apc_php_malloc, apc_php_free);
    ctxt.copy = APC_COPY_OUT_USER;

    MAKE_STD_ZVAL(zvalue);
    if(slot->value->type == APC_CACHE_ENTRY_FILE) {
        ZVAL_NULL(zvalue);
    } else if(slot->value->type == APC_CACHE_ENTRY_USER) {
        apc_cache_fetch_zval(zvalue, slot->value->data.user.val, &ctxt);
    }

    item->value = zvalue;
    apc_pool_destroy(ctxt.pool);

    return;

}
/* }}} */

/* {{{ apc_iterator_item_info */
static void apc_iterator_item_info(slot_t **slot_pp, apc_iterator_item_t *item) {
    slot_t *slot = *slot_pp;

    ALLOC_INIT_ZVAL(item->info);
    array_init(item->info);
    if(slot->value->type == APC_CACHE_ENTRY_FILE) {
        if (slot->key.type == APC_CACHE_KEY_FILE) {
          add_assoc_string(item->info, "filename", slot->value->data.file.filename, 1);
        } else {  /* APC_CACHE_FPFILE */
          add_assoc_string(item->info, "filename", slot->key.data.fpfile.fullpath, 1);
        }
        add_assoc_long(item->info, "device", slot->key.data.file.device);
        add_assoc_long(item->info, "inode", slot->key.data.file.inode);
        add_assoc_string(item->info, "type", "file", 1);
    } else if(slot->value->type == APC_CACHE_ENTRY_USER) {
        add_assoc_string(item->info, "info", slot->value->data.user.info, 1);
        add_assoc_long(item->info, "ttl", (long)slot->value->data.user.ttl);
        add_assoc_string(item->info, "type", "user", 1);
    }
    add_assoc_long(item->info, "num_hits", slot->num_hits);
    add_assoc_long(item->info, "mtime", slot->key.mtime);
    add_assoc_long(item->info, "creation_time", slot->creation_time);
    add_assoc_long(item->info, "deletion_time", slot->deletion_time);
    add_assoc_long(item->info, "access_time", slot->access_time);
    add_assoc_long(item->info, "ref_count", slot->value->ref_count);
    add_assoc_long(item->info, "mem_size", slot->value->mem_size);
    if (item->value) {
        add_assoc_zval(item->info, "value", item->value);
        Z_ADDREF_P(item->value);
    }

    return;
}
/* }}} */

/* {{{ apc_iterator_item */
static apc_iterator_item_t* apc_iterator_item_ctor(apc_iterator_t *iterator, slot_t **slot_pp) {
    apc_iterator_item_t *item = ecalloc(1, sizeof(apc_iterator_item_t));

    if ((iterator->format & APC_ITER_KEY) == APC_ITER_KEY) {
        apc_iterator_item_key(slot_pp, item);
    }

    if ((iterator->format & APC_ITER_VALUE) == APC_ITER_VALUE) {
        apc_iterator_item_value(slot_pp, item);
    }

    if ((iterator->format & APC_ITER_INFO) == APC_ITER_INFO) {
        apc_iterator_item_info(slot_pp, item);
    }

    return item;
}
/* }}} */

/* {{{ apc_iterator_clone */
static zend_object_value apc_iterator_clone(zval *zobject TSRMLS_DC) {
    zend_object_value value;
    apc_eprint("APCIterator object cannot be cloned.");
    return value;
}
/* }}} */

/* {{{ apc_iterator_item_dtor */
static void apc_iterator_item_dtor(apc_iterator_item_t *item) {
    if (item->key) {
        efree(item->key);
    }
    if (item->info) {
        zval_ptr_dtor(&item->info);
    }
    if (item->value) {
        zval_ptr_dtor(&item->value);
    }
    efree(item);
}
/* }}} */

/* {{{ apc_iterator_destroy */
static void apc_iterator_destroy(void *object, zend_object_handle handle TSRMLS_DC) {
    apc_iterator_t *iterator = (apc_iterator_t*)object;

    if (iterator->initialized == 0) {
        return;
    }

    while (apc_stack_size(iterator->stack) > 0) {
        apc_iterator_item_dtor(apc_stack_pop(iterator->stack));
    }
    if (iterator->regex)
        efree(iterator->regex);
    iterator->initialized = 0;

}
/* }}} */

/* {{{ acp_iterator_free */
static void apc_iterator_free(void *object TSRMLS_DC) {
    zend_object_std_dtor(object TSRMLS_CC);
    efree(object);
}
/* }}} */

/* {{{ apc_iterator_create */
static zend_object_value apc_iterator_create(zend_class_entry *ce TSRMLS_DC) {
    zend_object_value retval;
    apc_iterator_t *iterator;

    iterator = emalloc(sizeof(apc_iterator_t));
    iterator->obj.ce = ce;
    ALLOC_HASHTABLE(iterator->obj.properties);
    zend_hash_init(iterator->obj.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
    iterator->obj.guards = NULL;
    iterator->initialized = 0;
    retval.handle = zend_objects_store_put(iterator, apc_iterator_destroy, apc_iterator_free, NULL TSRMLS_CC);
    retval.handlers = &apc_iterator_object_handlers;

    return retval;
}
/* }}} */

/* {{{ apc_iterator_fetch_active */
static int apc_iterator_fetch_active(apc_iterator_t *iterator) {
    int count=0;
    slot_t **slot;
    char *key;
    apc_iterator_item_t *item;

    while (apc_stack_size(iterator->stack) > 0) {
        apc_iterator_item_dtor(apc_stack_pop(iterator->stack));
    }

    CACHE_LOCK(iterator->cache);
    while(count <= iterator->chunk_size && iterator->slot_idx < iterator->cache->num_slots) {
        slot = &iterator->cache->slots[iterator->slot_idx];
        while(*slot) {
            if ((*slot)->key.type == APC_CACHE_KEY_FILE) {
                key = (*slot)->value->data.file.filename;
            } else if ((*slot)->key.type == APC_CACHE_KEY_USER) {
                key = (char*)(*slot)->key.data.user.identifier;
            } else if ((*slot)->key.type == APC_CACHE_KEY_FPFILE) {
                key = (char*)(*slot)->key.data.fpfile.fullpath;
            }
#ifdef ITERATOR_PCRE
            if (!iterator->regex || pcre_exec(iterator->re, NULL, key, strlen(key), 0, 0, NULL, 0) >= 0) {
#endif
                count++;
                item = apc_iterator_item_ctor(iterator, slot);
                if (item) {
                    apc_stack_push(iterator->stack, item);
                }
#ifdef ITERATOR_PCRE
            }
#endif
            slot = &(*slot)->next;
        }
        iterator->slot_idx++;
    }
    CACHE_UNLOCK(iterator->cache);
    iterator->stack_idx = 0;
    return count;
}
/* }}} */

/* {{{ apc_iterator_fetch_deleted */
static int apc_iterator_fetch_deleted(apc_iterator_t *iterator) {
    int count=0;
    slot_t **slot;
    char *key;
    apc_iterator_item_t *item;

    CACHE_LOCK(iterator->cache);
    slot = &iterator->cache->header->deleted_list;
    while ((*slot) && count <= iterator->slot_idx) {
        count++;
        slot = &(*slot)->next;
    }
    count = 0;
    while ((*slot) && count < iterator->chunk_size) {
        if ((*slot)->key.type == APC_CACHE_KEY_FILE) {
            key = (*slot)->value->data.file.filename;
        } else if ((*slot)->key.type == APC_CACHE_KEY_USER) {
            key = (char*)(*slot)->key.data.user.identifier;
        } else if ((*slot)->key.type == APC_CACHE_KEY_FPFILE) {
            key = (char*)(*slot)->key.data.fpfile.fullpath;
        }
#ifdef ITERATOR_PCRE
        if (!iterator->regex || pcre_exec(iterator->re, NULL, key, strlen(key), 0, 0, NULL, 0) >= 0) {
#endif
            count++;
            item = apc_iterator_item_ctor(iterator, slot);
            if (item) {
                apc_stack_push(iterator->stack, item);
            }
#ifdef ITERATOR_PCRE
        }
#endif
        slot = &(*slot)->next;
    }
    CACHE_UNLOCK(iterator->cache);
    iterator->slot_idx += count;
    iterator->stack_idx = 0;
    return count;
}
/* }}} */

/* {{{ apc_iterator_totals */
static void apc_iterator_totals(apc_iterator_t *iterator) {
    slot_t **slot;
    int i;
    char *key;

    CACHE_LOCK(iterator->cache);
    for (i=0; i < iterator->cache->num_slots; i++) {
        slot = &iterator->cache->slots[i];
        while((*slot)) {
            if ((*slot)->key.type == APC_CACHE_KEY_FILE) {
                key = (*slot)->value->data.file.filename;
            } else if ((*slot)->key.type == APC_CACHE_KEY_USER) {
                key = (char*)(*slot)->key.data.user.identifier;
            } else if ((*slot)->key.type == APC_CACHE_KEY_FPFILE) {
                key = (char*)(*slot)->key.data.fpfile.fullpath;
            }
#ifdef ITERATOR_PCRE
            if (!iterator->regex || pcre_exec(iterator->re, NULL, key, strlen(key), 0, 0, NULL, 0) >= 0) {
#endif
                iterator->size += (*slot)->value->mem_size;
                iterator->hits += (*slot)->num_hits;
                iterator->count++;
            }
            slot = &(*slot)->next;
#ifdef ITERATOR_PCRE
        }
#endif
    }
    CACHE_UNLOCK(iterator->cache);
    iterator->totals_flag = 1;
}
/* }}} */

/* {{{ proto object APCIterator::__costruct(string cache [, string regex [, long format [, long chunk_size [, long list ]]]]) */
PHP_METHOD(apc_iterator, __construct) {
    zval *object = getThis();
    apc_iterator_t *iterator = (apc_iterator_t*)zend_object_store_get_object(object TSRMLS_CC);
    char *cachetype;
    int cachetype_len;
    long format = APC_ITER_ALL;
    long chunk_size=0;
    char *regex = NULL;
    int regex_len = 0;
    long list = APC_LIST_ACTIVE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|slll", &cachetype, &cachetype_len, &regex, &regex_len, &format, &chunk_size, &list) == FAILURE) {
        return;
    }

    if (chunk_size < 0) {
        apc_eprint("APCIterator chunk size must be greater than 0.");
        return;
    }

    if (format > APC_ITER_ALL) {
        apc_eprint("APCIterator format is invalid.");
        return;
    }

    if (list == APC_LIST_ACTIVE) {
        iterator->fetch = apc_iterator_fetch_active;
    } else if (list == APC_LIST_DELETED) {
        iterator->fetch = apc_iterator_fetch_deleted;
    } else {
        apc_wprint("APCIterator invalid list type.");
        return;
    }

    if(!strcasecmp(cachetype,"user")) {
        iterator->cache = apc_user_cache;
    } else {
        iterator->cache = apc_cache;
    }

    iterator->slot_idx = 0;
    iterator->chunk_size = chunk_size == 0 ? APC_DEFAULT_CHUNK_SIZE : chunk_size;
    iterator->stack = apc_stack_create(chunk_size);
    iterator->format = format;
    iterator->totals_flag = 0;
    iterator->count = 0;
    iterator->size = 0;
    iterator->hits = 0;
    if (regex_len) {
#ifdef ITERATOR_PCRE
        iterator->regex = estrndup(regex, regex_len);
        iterator->regex_len = regex_len;
        iterator->re = pcre_get_compiled_regex(regex, NULL, NULL TSRMLS_CC);

        if(!iterator->re) {
            apc_eprint("Could not compile regular expression: %s", regex);
        }
#else
        apc_eprint("Regular expressions support is not enabled, please enable PCRE for APCIterator regex support");
#endif
    } else {
        iterator->regex = NULL;
        iterator->regex_len = 0;
    }
    iterator->initialized = 1;
}
/* }}} */

/* {{{ proto APCIterator::rewind() */
PHP_METHOD(apc_iterator, rewind) {
    zval *object = getThis();
    apc_iterator_t *iterator = (apc_iterator_t*)zend_object_store_get_object(object TSRMLS_CC);

    if (iterator->initialized == 0) {
        RETURN_FALSE;
    }

    iterator->slot_idx = 0;
    iterator->stack_idx = 0;
    iterator->key_idx = 0;
    iterator->fetch(iterator);
}
/* }}} */

/* {{{ proto boolean APCIterator::valid() */
PHP_METHOD(apc_iterator, valid) {
    zval *object = getThis();
    apc_iterator_t *iterator = (apc_iterator_t*)zend_object_store_get_object(object TSRMLS_CC);

    if (iterator->initialized == 0) {
        RETURN_FALSE;
    }

    if (apc_stack_size(iterator->stack) == iterator->stack_idx) {
        iterator->fetch(iterator);
    }

    RETURN_BOOL(apc_stack_size(iterator->stack) == 0 ? 0 : 1);
}
/* }}} */

/* {{{ proto mixed APCIterator::current() */
PHP_METHOD(apc_iterator, current) {
    zval *object = getThis();
    apc_iterator_item_t *item;
    apc_iterator_t *iterator = (apc_iterator_t*)zend_object_store_get_object(object TSRMLS_CC);
    if (iterator->initialized == 0) {
        RETURN_FALSE;
    }
    if (apc_stack_size(iterator->stack) == iterator->stack_idx) {
        iterator->fetch(iterator);
    }
    item = apc_stack_get(iterator->stack, iterator->stack_idx);
    if (item->info) {
        RETURN_ZVAL(item->info, 1, 0);
    } else if (item->value) {
        RETURN_ZVAL(item->value, 1, 0);
    } else {
        RETURN_NULL();
    }
}
/* }}} */

/* {{{ proto string APCIterator::key() */
PHP_METHOD(apc_iterator, key) {
    zval *object = getThis();
    apc_iterator_item_t *item;
    apc_iterator_t *iterator = (apc_iterator_t*)zend_object_store_get_object(object TSRMLS_CC);
    if (iterator->initialized == 0) {
        RETURN_FALSE;
    }
    item = apc_stack_get(iterator->stack, iterator->stack_idx);
    if (item->key) {
        RETURN_STRINGL(item->key, item->key_len, 1);
    } else {
        RETURN_LONG(iterator->key_idx);
    }
}
/* }}} */

/* {{{ proto APCIterator::next() */
PHP_METHOD(apc_iterator, next) {
    zval *object = getThis();
    apc_iterator_t *iterator = (apc_iterator_t*)zend_object_store_get_object(object TSRMLS_CC);
    if (iterator->initialized == 0) {
        RETURN_FALSE;
    }
    iterator->stack_idx++;
    iterator->key_idx++;
    return;
}
/* }}} */

/* {{{ proto long APCIterator::getTotalHits() */
PHP_METHOD(apc_iterator, getTotalHits) {
    zval *object = getThis();
    apc_iterator_t *iterator = (apc_iterator_t*)zend_object_store_get_object(object TSRMLS_CC);

    if (iterator->initialized == 0) {
        RETURN_FALSE;
    }

    if (iterator->totals_flag == 0) {
        apc_iterator_totals(iterator);
    }

    RETURN_LONG(iterator->hits);
}
/* }}} */

/* {{{ proto long APCIterator:;getTotalSize() */
PHP_METHOD(apc_iterator, getTotalSize) {
    zval *object = getThis();
    apc_iterator_t *iterator = (apc_iterator_t*)zend_object_store_get_object(object TSRMLS_CC);

    if (iterator->initialized == 0) {
        RETURN_FALSE;
    }

    if (iterator->totals_flag == 0) {
        apc_iterator_totals(iterator);
    }

    RETURN_LONG(iterator->size);
}
/* }}} */

/* {{{ proto long APCIterator::getTotalCount() */
PHP_METHOD(apc_iterator, getTotalCount) {
    zval *object = getThis();
    apc_iterator_t *iterator = (apc_iterator_t*)zend_object_store_get_object(object TSRMLS_CC);

    if (iterator->initialized == 0) {
        RETURN_FALSE;
    }

    if (iterator->totals_flag == 0) {
        apc_iterator_totals(iterator);
    }

    RETURN_LONG(iterator->count);
}
/* }}} */

/* {{{ apc_iterator_functions */
static function_entry apc_iterator_functions[] = {
    PHP_ME(apc_iterator, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
    PHP_ME(apc_iterator, rewind, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(apc_iterator, current, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(apc_iterator, key, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(apc_iterator, next, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(apc_iterator, valid, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(apc_iterator, getTotalHits, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(apc_iterator, getTotalSize, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(apc_iterator, getTotalCount, NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ apc_iterator_init */
int apc_iterator_init(int module_number TSRMLS_DC) {
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, APC_ITERATOR_NAME, apc_iterator_functions);
    apc_iterator_ce = zend_register_internal_class(&ce TSRMLS_CC);
    apc_iterator_ce->create_object = apc_iterator_create;
    zend_class_implements(apc_iterator_ce TSRMLS_CC, 1, zend_ce_iterator);

    zend_register_long_constant("APC_LIST_ACTIVE", sizeof("APC_LIST_ACTIVE"), APC_LIST_ACTIVE, CONST_PERSISTENT | CONST_CS, module_number TSRMLS_CC);
    zend_register_long_constant("APC_LIST_DELETED", sizeof("APC_LIST_DELETED"), APC_LIST_DELETED, CONST_PERSISTENT | CONST_CS, module_number TSRMLS_CC);

    zend_register_long_constant("APC_ITER_KEY", sizeof("APC_ITER_KEY"), APC_ITER_KEY, CONST_PERSISTENT | CONST_CS, module_number TSRMLS_CC);
    zend_register_long_constant("APC_ITER_VALUE", sizeof("APC_ITER_VALUE"), APC_ITER_VALUE, CONST_PERSISTENT | CONST_CS, module_number TSRMLS_CC);
    zend_register_long_constant("APC_ITER_INFO", sizeof("APC_ITER_INFO"), APC_ITER_INFO, CONST_PERSISTENT | CONST_CS, module_number TSRMLS_CC);
    zend_register_long_constant("APC_ITER_ALL", sizeof("APC_ITER_ALL"), APC_ITER_ALL, CONST_PERSISTENT | CONST_CS, module_number TSRMLS_CC);

    memcpy(&apc_iterator_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    apc_iterator_object_handlers.clone_obj = apc_iterator_clone;

    return SUCCESS;
}
/* }}} */


int apc_iterator_delete(zval *zobj TSRMLS_DC) {
    apc_iterator_t *iterator;
    zend_class_entry *ce = Z_OBJCE_P(zobj);
    apc_iterator_item_t *item;

    if (!ce || !instanceof_function(ce, apc_iterator_ce TSRMLS_CC)) {
        apc_eprint("apc_delete object argument must be instance of APCIterator");
        return 0;
    }
    iterator = (apc_iterator_t*)zend_object_store_get_object(zobj TSRMLS_CC);

    if (iterator->initialized == 0) {
        return 0;
    }

    while (iterator->fetch(iterator)) {
        while (iterator->stack_idx < apc_stack_size(iterator->stack)) {
            item = apc_stack_get(iterator->stack, iterator->stack_idx++);
            apc_cache_user_delete(apc_user_cache, item->key, item->key_len);
        }
    }

    return 1;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
