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

#ifndef INCLUDED_APC_SHM
#define INCLUDED_APC_SHM

/* shared memory wrapper. no surprises */

/* apc_shm_create: create a shared memory segment of given size */
extern int   apc_shm_create(const char* pathname, int proj, int size);

/* apc_shm_destroy: remove a shared memory segment */
extern void  apc_shm_destroy(int shmid);

/* apc_shm_attach: get the address of the beginning of a shared
 * memory segment */
extern void* apc_shm_attach(int shmid);

/* apc_shm_detach: detach from a shared memory segment */
extern void  apc_shm_detach(void* shmaddr);

#endif
