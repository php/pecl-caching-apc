/* 
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/3_0.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef APC_STACK_H
#define APC_STACK_H

/* Basic stack datatype */

#define T apc_stack_t*
typedef struct apc_stack_t apc_stack_t; /* opaque stack type */

extern T apc_stack_create(int size_hint);
extern void apc_stack_destroy(T stack);
extern void apc_stack_clear(T stack);
extern void apc_stack_push(T stack, void* item);
extern void* apc_stack_pop(T stack);
extern void* apc_stack_top(T stack);
extern void* apc_stack_get(T stack, int n);
extern int apc_stack_size(T stack);

#undef T
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */