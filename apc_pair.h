/*
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
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
   +----------------------------------------------------------------------+
*/

#ifndef APC_PAIR_H
#define APC_PAIR_H

typedef struct Pair Pair;

/* prepends x to p */
extern Pair* cons(int x, Pair* p);

/* returns the list head */
extern int car(Pair* p);

/* returns the list tail */
extern Pair* cdr(Pair* p);

/* changes value of list head */
extern void pair_set_car(Pair* p, int v);

/* returns the length of the list */
extern int pair_length(Pair* p);

/* deallocates a list */
extern void pair_destroy(Pair* p);

/* returns a copy of the given list */
extern Pair* pair_copy(Pair* p);

/* filters elements from p where predicate is true (returns a list copy) */
extern Pair* pair_filter(int (*pred)(int), Pair* p);

#define make_pair(x, y) cons(x, cons(y, 0))
#define cadr(p) car(cdr(p))
#define cddr(p) cdr(cdr(p))
#define caddr(p) car(cdr(cdr(p)))
#define cdddr(p) cdr(cdr(cdr(p)))
#define cadddr(p) car(cdr(cdr(cdr(p))))

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
