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

#include "apc_pair.h"
#include <assert.h>

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
