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

#include "apc_stack.h"
#include "apc.h"

struct apc_stack_t {
    void** data;
    int capacity;
    int size;
};

apc_stack_t* apc_stack_create(int size_hint)
{
    apc_stack_t* stack = (apc_stack_t*) apc_emalloc(sizeof(apc_stack_t));

    stack->capacity = (size_hint > 0) ? size_hint : 10;
    stack->size = 0;
    stack->data = (void**) apc_emalloc(sizeof(void*) * stack->capacity);

    return stack;
}

void apc_stack_destroy(apc_stack_t* stack)
{
    if (stack != NULL) {
        apc_efree(stack->data);
        apc_efree(stack);
    }
}

void apc_stack_clear(apc_stack_t* stack)
{
    assert(stack != NULL);
    stack->size = 0;
}

void apc_stack_push(apc_stack_t* stack, void* item)
{
    assert(stack != NULL);
    if (stack->size == stack->capacity) {
        stack->capacity *= 2;
        stack->data = apc_erealloc(stack->data, sizeof(void*)*stack->capacity);
    }
    stack->data[stack->size++] = item;
}

void* apc_stack_pop(apc_stack_t* stack)
{
    assert(stack != NULL && stack->size > 0);
    return stack->data[--stack->size];
}

void* apc_stack_top(apc_stack_t* stack)
{
    assert(stack != NULL && stack->size > 0);
    return stack->data[stack->size-1];
}

void* apc_stack_get(apc_stack_t* stack, int n)
{
    assert(stack != NULL && stack->size > n);
    return stack->data[n];
}

int apc_stack_size(apc_stack_t* stack)
{
    assert(stack != NULL);
    return stack->size;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
