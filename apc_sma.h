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

#ifndef INCLUDED_APC_SMA
#define INCLUDED_APC_SMA

/* Simple shared memory allocator. */

/*
 * apc_sma_init: Initialize the shared memory allocator. Must be called once,
 * and only by the parent process -- before it forks any child processes.
 */
extern void apc_sma_init(int numseg, int segsize);

/*
 * apc_sma_cleanup: Cleans up the shared memory allocator. Should be called
 * only once, and only by the parent process.
 */
extern void apc_sma_cleanup();

/*
 * apc_sma_malloc: Allocates size bytes of shared memory. (Aborts the program
 * if insufficient resources are available to fulfill the request.)
 */
extern void* apc_sma_malloc(int size);

/*
 * apc_sma_free: Frees memory previously allocated by apc_sma_malloc. (Aborts
 * the program if the supplied address was not returned by apc_sma_malloc.)
 */
extern void apc_sma_free(void* p);

#endif
