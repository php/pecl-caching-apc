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

#ifndef INCLUDED_APC_RWLOCK
#define INCLUDED_APC_RWLOCK

/* readers-writer lock implementation; gives preference to waiting
 * writers over readers */

#define T apc_rwlock_t*
typedef struct apc_rwlock_t apc_rwlock_t;

extern T    apc_rwl_create(const char* pathname);
extern void apc_rwl_destroy(T lock);
extern void apc_rwl_readlock(T lock);
extern void apc_rwl_writelock(T lock);
extern void apc_rwl_unlock(T lock);

#undef T
#endif
