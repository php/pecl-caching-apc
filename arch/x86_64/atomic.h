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


#include <sys/syscall.h>
#include <sys/time.h>
#include <linux/futex.h>

/* int sys_futex (void *futex, int op, int val, const struct timespec *timeout); */
static inline long int apc_sys_futex(void *futex, int op, int val, const struct timespec *timeout) {

  long int ret;

  /* x86_64 system calls are performed with the faster SYSCALL operation.
   *  the argument order is D, S, d, c, b, a rather than
   *  a, b, c, d, S, D as on the i386 int 80h call. 
  */ 
  asm volatile ("syscall" 
       : "=a" (ret)
       : "0" (SYS_futex), 
         "D" (futex), 
         "S" (op), 
         "d" (val),
         "c" (timeout)
       : "r11", "rcx", "memory"
      );

  return ret;

} 


static inline int apc_cmpxchg(volatile int *ptr, int old, int new) {

    int prev;

    asm volatile ("LOCK cmpxchgl %1, %2"
                   : "=a" (prev)
                   : "r" (new), 
                     "m" (*(ptr)), 
                     "0"(old)
                   : "memory", "cc"
                 );

    return prev;
}

static inline int apc_xchg(volatile int *ptr, int new) {

    int ret;
  
    asm volatile ("LOCK xchgl %[new], %[ptr]"
                  : "=a" (ret)
                  : [new] "0" (new), 
                    [ptr] "m" (*(ptr))
                  : "memory"
                 );

    return ret;
    
}

