/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2005 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: George Schlossnagle <george@omniti.com>                     |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$ */

#include "apc_fcntl.h"
#include "apc.h"
#include <php.h>
#include <win32/flock.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int apc_fcntl_create(const char* pathname)
{
	char *lock_file;
	HANDLE fd;
	static int i=0;
	
	spprintf(&lock_file, 0, "/tmp/apc.lock.%d", i++);
	
	fd = CreateFile(lock_file,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
        

	if (fd == INVALID_HANDLE_VALUE) {
		apc_eprint("apc_fcntl_create: could not open %s", lock_file);
		efree(lock_file);
		return -1;
	}
	
	efree(lock_file);
	return (int)fd;
}

void apc_fcntl_destroy(int fd)
{
	CloseHandle((HANDLE)fd);
}

void apc_fcntl_lock(int fd)
{
	OVERLAPPED offset =	{0, 0, 0, 0, NULL};
	
	if (!LockFileEx((HANDLE)fd, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &offset)) {
		apc_eprint("apc_fcntl_lock failed errno:%d", GetLastError());
	}
}

void apc_fcntl_unlock(int fd)
{
	OVERLAPPED offset =	{0, 0, 0, 0, NULL};

	if (!UnlockFileEx((HANDLE)fd, 0, 1, 0, &offset)) {
		apc_eprint("apc_fcntl_unlock failed errno:%d", GetLastError());
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */