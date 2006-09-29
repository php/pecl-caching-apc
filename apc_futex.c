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
  | Authors: Brian Shire <shire@php.net>                                 |
  +----------------------------------------------------------------------+

 */

/* $Id$ */

#ifdef APC_FUTEX

/***************************************************************************
* Futex (Fast Userspace Mutex) support for APC
* 
* Futex support provides user space locking with system calls only
* for the contended cases.  Some required reading for this functionality is:
*
* 'Fuss, Futexes and Furwocks: Fast Userlevel Locking in Linux' 
*  by Hubertus Franke, Rusty Russell, and Matthew Kirkwood
*   http://www.realitydiluted.com/nptl-uclibc/docs/futex.pdf
*
* 'Futexes are Tricky' by Ulrich Drepper 
*    http://people.redhat.com/drepper/futex.pdf
*
* 
* This implementation is optimized and designed for the i386 and x86_64 
* architectures.  Other architectures may require additional design 
* to efficiently and safely implement this functionality. 
*
* Lock values are:
* 0 = Unlocked
* 1 = Locked without any waiting processes
* 2 = Locked with an unknown number of waiting processes
*
***************************************************************************/

#include "apc_futex.h"
#include "apc.h"

inline int apc_futex_create()
{
    return 0;
}

inline void apc_futex_destroy(volatile int* lock)
{
    return;
}

void apc_futex_lock(volatile int* lock)
{
    int c;
  
    /*  Attempt to obtain a lock if not currently locked.  If the previous
     *  value was not 0 then we did not obtain the lock, and must wait.
     *  If the previous value was 1 (has no waiting processes) then we
     *  set the lock to 2 before blocking on the futex wait operation.  
     *  This implementation suffers from the possible difficulty of 
     *  efficently implementing the atomic xchg operation on some
     *  architectures, and could also cause unecessary wake operations by
     *  setting the lock to 2 when there are no additional waiters.
     */ 
    if((c = apc_cmpxchg(lock, 0, 1)) != 0) {
        if(c != 2) {
            c = apc_xchg(lock, 2);
        }
        while(c != 0) {
            apc_futex_wait(lock, 2);
            c = apc_xchg(lock, 2);
        }
    }
    
}

/* non-blocking lock returns 1 when the lock has been obtained, 0 if it would block */
inline zend_bool apc_futex_nonblocking_lock(volatile int* lock)
{
    return apc_cmpxchg(lock, 0, 1) == 0;
}


inline void apc_futex_unlock(volatile int* lock)
{
    /* set the lock to 0, if it's previous values was not 1 (no waiters)
     * then perform a wake operation on one process letting it know the lock 
     * is available.  This is an optimization to save wake calls if there
     * are no waiting processes for the lock 
     */
    if(apc_xchg(lock,0) != 1) {
        apc_futex_wake(lock, 1);
    }
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
