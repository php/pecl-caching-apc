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

#ifndef APC_LIST_H
#define APC_LIST_H

typedef struct _apc_list_element {
	struct _apc_list_element *next;
	struct _apc_list_element *prev;
	void *data;
} apc_list_element;
	

typedef struct _apc_list {
	apc_list_element *head;
	apc_list_element *tail;
	void* (*list_ctor)(int);
	void (*list_dtor)(void *);
} apc_list;

extern void apc_list_create(apc_list **list, void* (*apc_list_ctor)(int), void (*apc_list_dtor)(void *));
extern void apc_list_prepend(apc_list *list, void *data);
extern void apc_list_append(apc_list *list, void *data);
extern void apc_list_apply(apc_list *list, void (*apply_func)(void *));
extern void apc_list_destroy(apc_list *list);
extern void apc_list_clean(apc_list *list);

#endif
