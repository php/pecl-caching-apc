/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$*/

#include "apc_pair.h"
#include <assert.h>
#include <stdlib.h>

struct Pair {
    int v;
    Pair* n;
};

Pair* cons(int x, Pair* p)
{
    Pair* q = (Pair*) malloc(sizeof(Pair));
    q->v = x;
    q->n = p;
    return q;
}

int car(Pair* p)
{
    assert(p);
    return p->v;
}

Pair* cdr(Pair* p)
{
    assert(p);
    return p->n;
}

void pair_set_car(Pair* p, int v)
{
    assert(p);
    p->v = v;
}

int pair_length(Pair* p)
{
    return p ? 1 + pair_length(cdr(p)) : 0;
}

void pair_destroy(Pair* p)
{
    while (p) {
        Pair* q = p;
        p = cdr(p);
        free(q);
    }
}

Pair* pair_copy(Pair* p)
{
    return p ? cons(car(p), pair_copy(cdr(p))) : 0;
}

Pair* pair_filter(int (*pred)(int), Pair* p)
{
    return p
        ? pred(car(p))
            ? pair_filter(pred, cdr(p))
            : cons(car(p), pair_filter(pred, cdr(p)))
        : 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
