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

#ifndef INCLUDED_APC_FCNTL
#define INCLUDED_APC_FCNTL
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "apc_nametable.h"

/* lock_reg: fcntl wrapper */
int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len);

void apc_fcntl_readlock(int fd);

void apc_fcntl_readlockwait(int fd);

void apc_fcntl_writelock(int fd);

void apc_fcntl_writelockwait(int fd);

int apc_unlink(char *filename);

/* simple fcntl wrappers */

#define read_lock(fd, offset, whence, len) \
						lock_reg(fd, F_SETLK, F_RDLCK, offset, whence, len)
#define readw_lock(fd, offset, whence, len) \
						lock_reg(fd, F_SETLKW, F_RDLCK, offset, whence, len)
#define write_lock(fd, offset, whence, len) \
						lock_reg(fd, F_SETLK, F_WRLCK, offset, whence, len)
#define writew_lock(fd, offset, whence, len) \
						lock_reg(fd, F_SETLKW, F_WRLCK, offset, whence, len)
#define un_lock(fd, offset, whence, len) \
						lock_reg(fd, F_SETLK, F_UNLCK, offset, whence, len)

/* lock files by name, cache results in locktable */
extern int apc_writew_lock_key(const char* key, apc_nametable_t* locktable, apc_nametable_t* opentable);
extern int apc_write_lock_key(const char* key, apc_nametable_t* locktable, apc_nametable_t* opentable);
extern int apc_readw_lock_key(const char* key, apc_nametable_t* locktable, apc_nametable_t* opentable);
extern int apc_read_lock_key(const char* key, apc_nametable_t* locktable, apc_nametable_t* opentable);
extern int apc_un_lock_key(const char* key, apc_nametable_t* locktable, apc_nametable_t* opentable);
extern void apc_un_lock_nametable(char *key, void *fd);

#endif
