#ifndef ZEND_NO_ZEND
#define ZEND_NO_ZEND

#include "zend.h"
#include "zend_compile.h"
#include "zend_llist.h"
#include "zend_hash.h"

#define APC_ZEND_OP_ARRAY_OP 1
#define APC_ZEND_USER_FUNCTION_OP 2

ZEND_API void apc_destroy_zend_function(zend_function *function);
ZEND_API void apc_dont_destroy(void *ptr);
extern void apc_zend_hash_diff(HashTable *outer, HashTable *inner);

#endif
