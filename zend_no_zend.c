/* ==================================================================
 * APC Cache
 * Copyright (c) 2000-2001 Community Connect, Inc.
 * All rights reserved.
 * ==================================================================
 * This source code is made available free and without charge subject
 * to the terms of the QPL as detailed in bundled LICENSE file, which
 * is also available at http://apc.communityconnect.com/LICENSE. 
 * ==================================================================
 * Daniel Cowgill <dan@mail.communityconnect.com>
 * George Schlossnagle <george@lethargy.org>
 * ==================================================================
*/
/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2001 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 0.92 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.zend.com/license/0_92.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@zend.com>                                |
   |          Zeev Suraski <zeev@zend.com>                                |
   +----------------------------------------------------------------------+
*/

/* This file contains functions for manipulating Zend engine data 
 * structures which either should be part of Zend, or are, but aren't 
 * exported
*/

#include "zend_no_zend.h"

static Bucket *zend_hash_apply_deleter_no_data(HashTable *ht, Bucket *p)
{
    Bucket *retval;

    HANDLE_BLOCK_INTERRUPTIONS();

    if (ht->pDestructor) {
        ht->pDestructor(p->pData);
    }
    retval = p->pListNext;

    if (p->pLast) {
        p->pLast->pNext = p->pNext;
    } else {
        uint nIndex;

        nIndex = p->h % ht->nTableSize;
        ht->arBuckets[nIndex] = p->pNext;
    }
    if (p->pNext) {
        p->pNext->pLast = p->pLast;
    } else {
        /* Nothing to do as this list doesn't have a tail */
    }

    if (p->pListLast != NULL) {
        p->pListLast->pListNext = p->pListNext;
    } else {
        /* Deleting the head of the list */
        ht->pListHead = p->pListNext;
    }
    if (p->pListNext != NULL) {
        p->pListNext->pListLast = p->pListLast;
    } else {
        ht->pListTail = p->pListLast;
    }
    if (ht->pInternalPointer == p) {
        ht->pInternalPointer = p->pListNext;
    }
    pefree(p, ht->persistent);
    HANDLE_UNBLOCK_INTERRUPTIONS();
    ht->nNumOfElements--;

    return retval;
}


void apc_zend_hash_diff(HashTable *outer, HashTable *inner)
{
    Bucket *p;
    void *tmp;

    p = outer->pListHead;
    while (p != NULL) {
        Bucket *q;
        fprintf(stderr, "Looking for %s\n", p->arKey);
        if(!zend_hash_find(inner, p->arKey, p->nKeyLength, &tmp))
        {
            p = zend_hash_apply_deleter_no_data(outer, p);
        }
        else {
            p = p->pListNext;
        }
    }
}

ZEND_API void apc_destroy_zend_function(zend_function *function)
{
  switch (function->type) {
    case ZEND_USER_FUNCTION: {
		zend_op_array* zoa = (zend_op_array *) function;;
	  	if ( (int) zoa->reserved[0] != APC_ZEND_USER_FUNCTION_OP )
		{
			/* destroy if not apc handled */
      		destroy_op_array((zend_op_array *) function);
		}
	} break;
    case ZEND_INTERNAL_FUNCTION:
      /* do nothing */
      break;
  }
}

ZEND_API void apc_dont_destroy(void *ptr)
{
}

