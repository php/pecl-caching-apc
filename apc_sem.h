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

#ifndef INCLUDED_APC_SEM
#define INCLUDED_APC_SEM

/* semaphore wrappers */

/* apc_sem_create: create a semaphore. if it does not already exist, set its
 * value to initval */
extern int  apc_sem_create(const char* pathname, int proj, int initval);

/* apc_sem_destroy: destroy a semaphore */
extern void apc_sem_destroy(int semid);

/* apc_sem_lock: acquire lock on semaphore */
extern void apc_sem_lock(int semid);

/* apc_sem_unlock: release lock on semaphore */
extern void apc_sem_unlock(int semid);

/* apc_sem_waitforzero: wait for semaphore count to reach zero */
extern void apc_sem_waitforzero(int semid);

/* apc_sem_getvalue: return the value of a semaphore */
extern int  apc_sem_getvalue(int semid);

#endif
