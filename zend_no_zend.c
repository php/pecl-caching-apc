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
#include "apc_serialize.h"

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

void dump_zend_op(zend_op* zo)
{
  fprintf(stderr, "Dumping %s\n", getOpcodeName(zo->opcode));
  if(zo->result.op_type == IS_CONST) {
    zval *zv = &zo->result.u.constant;
    switch(zo->result.u.constant.type) {
      case IS_RESOURCE:
      case IS_BOOL:
      case IS_LONG:
      case IS_DOUBLE:
        fprintf(stderr, "\tresult->op_type = IS_LONG|BOOL|RESOURCE\n");
        fprintf(stderr, "\tresult->zval = %d\n", zv->value.lval);
        break;
      case IS_NULL:
        fprintf(stderr, "\tresult->op_type = IS_NULL\n");
        break;
      case IS_CONSTANT:
        fprintf(stderr, "\tresult->op_type = IS_CONSTANT\n");
        fprintf(stderr, "\tresult->zval = %s\n", zv->value.str.val);
        break;
      case IS_STRING:
        fprintf(stderr, "\tresult->op_type = IS_STRING\n");
        fprintf(stderr, "\tresult->zval = %s\n", zv->value.str.val);
        break;
      case FLAG_IS_BC:
        fprintf(stderr, "\tresult->op_type = FLAG_IS_BC\n");
        fprintf(stderr, "\tresult->zval = %s\n", zv->value.str.val);
        break;
      case IS_ARRAY:
        fprintf(stderr, "\tresult->op_type = IS_ARRAY\n");
        break;
      case IS_CONSTANT_ARRAY:
        fprintf(stderr, "\tresult->op_type = IS_CONSTANT_ARRAY\n");
        break;
      case IS_OBJECT:
        fprintf(stderr, "\tresult->op_type = IS_OBJECT\n");
        break;
      default:
    }
  } else {
    fprintf(stderr, "\tresult->op_type != IS_CONST\n");
  }
  if(zo->op1.op_type == IS_CONST) {
    zval *zv = &zo->op1.u.constant;
    switch(zo->op1.u.constant.type) {
      case IS_RESOURCE:
      case IS_BOOL:
      case IS_LONG:
      case IS_DOUBLE:
        fprintf(stderr, "\top1->op_type = IS_LONG|BOOL|RESOURCE\n");
        fprintf(stderr, "\top1->zval = %d\n", zv->value.lval);
        break;
      case IS_NULL:
        fprintf(stderr, "\top1->op_type = IS_NULL\n");
        break;
      case IS_CONSTANT:
        fprintf(stderr, "\top1->op_type = IS_CONSTANT\n");
        fprintf(stderr, "\top1->zval = %s\n", zv->value.str.val);
        break;
      case IS_STRING:
        fprintf(stderr, "\top1->op_type = IS_STRING\n");
        fprintf(stderr, "\top1->zval = %s\n", zv->value.str.val);
        break;
      case FLAG_IS_BC:
        fprintf(stderr, "\top1->op_type = FLAG_IS_BC\n");
        fprintf(stderr, "\top1->zval = %s\n", zv->value.str.val);
        break;
      case IS_ARRAY:
        fprintf(stderr, "\top1->op_type = IS_ARRAY\n");
        break;
      case IS_CONSTANT_ARRAY:
        fprintf(stderr, "\top1->op_type = IS_CONSTANT_ARRAY\n");
        break;
      case IS_OBJECT:
        fprintf(stderr, "\top1->op_type = IS_OBJECT\n");
        break;
      default:
    }
  } else {
    fprintf(stderr, "\top1->op_type != IS_CONST\n");
  }
  fprintf(stderr, "\tDumping op2\n");
  if(zo->op2.op_type == IS_CONST) {
    zval *zv = &zo->op2.u.constant;
    switch(zo->op2.u.constant.type) {
      case IS_RESOURCE:
      case IS_BOOL:
      case IS_LONG:
      case IS_DOUBLE:
        fprintf(stderr, "\top2->op_type = IS_LONG|BOOL|RESOURCE\n");
        fprintf(stderr, "\top2->zval = %d\n", zv->value.lval);
        break;
      case IS_NULL:
        fprintf(stderr, "\top2->op_type = IS_NULL\n");
        break;
      case IS_CONSTANT:
        fprintf(stderr, "\top2->op_type = IS_CONSTANT\n");
        fprintf(stderr, "\top2->zval = %s\n", zv->value.str.val);
        break;
      case IS_STRING:
        fprintf(stderr, "\top2->op_type = IS_STRING\n");
        fprintf(stderr, "\top2->zval = %s\n", zv->value.str.val);
        break;
      case FLAG_IS_BC:
        fprintf(stderr, "\top2->op_type = FLAG_IS_BC\n");
        fprintf(stderr, "\top2->zval = %s\n", zv->value.str.val);
        break;
      case IS_ARRAY:
        fprintf(stderr, "\top2->op_type = IS_ARRAY\n");
        break;
      case IS_CONSTANT_ARRAY:
        fprintf(stderr, "\top2->op_type = IS_CONSTANT_ARRAY\n");
        break;
      case IS_OBJECT:
        fprintf(stderr, "\top2->op_type = IS_OBJECT\n");
        break;
      default:
    }
  } else {
    fprintf(stderr, "\top2->op_type != IS_CONST\n");
  }
}    

void zend_hash_display(HashTable *ht)
{
	Bucket *p;
	p=ht->pListHead;
	fprintf(stderr, "dumping hash (%p)\n", ht);
	while(p != NULL) {
		fprintf(stderr, "\t(%s) => (%p), (%p)\n", p->arKey, p->pData, p->pDataPtr);
		p = p->pListNext;
	}
}
