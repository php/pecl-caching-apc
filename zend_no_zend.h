/* 
   +----------------------------------------------------------------------+
   | APC
   +----------------------------------------------------------------------+
   | Copyright (c) 2000-2002 Community Connect Inc.
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   |          George Schlossnagle <george@lethargy.org>                   |
   +----------------------------------------------------------------------+
*/

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
extern void dump_zend_op(zend_op* op);
extern void zend_hash_display(HashTable *ht);
#endif
