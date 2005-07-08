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
#include <unistd.h>
#include <fcntl.h>

int apc_fcntl_create(const char* pathname)
{
    int fd;
    char *lock_path = NULL;
    if(pathname == NULL) {
        lock_path = malloc(strlen("/tmp/.apc.") + 6);
        snprintf(lock_path, strlen("/tmp/.apc.") + 6, "/tmp/.apc.%d", getpid());
        fd = open(lock_path, O_RDWR|O_CREAT, 0666);
        if(fd > 0 ) {
            unlink(lock_path);
            free(lock_path);
            return fd;
        } else {
            free(lock_path);
            apc_eprint("apc_fcntl_create: open(%s, O_RDWR|O_CREAT, 0666) failed:", lock_path);
            return -1;
        }
    }
    fd = open(pathname, O_RDWR|O_CREAT, 0666);
    if(fd > 0 ) {
        unlink(pathname);
        return fd;
    }
    apc_eprint("apc_fcntl_create: open(%s, O_RDWR|O_CREAT, 0666) failed:", pathname);
    return -1;
}

void apc_fcntl_destroy(int fd)
{
    close(fd);
}

int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
  struct flock lock;

  lock.l_type = type;
  lock.l_start = offset;
  lock.l_whence = whence;
  lock.l_len = len;

  return( fcntl(fd, cmd, &lock) );
}

void apc_fcntl_lock(int fd)
{
    if(lock_reg(fd, F_SETLKW, F_WRLCK, 0, SEEK_SET, 1) < 0) {
        apc_eprint("apc_fcntl_lock failed errno:%d", errno);
    }
}

void apc_fcntl_unlock(int fd)
{
    if(lock_reg(fd, F_SETLK, F_UNLCK, 0, SEEK_SET, 1) < 0) {
        apc_eprint("apc_fcntl_unlock failed errno:%d", errno);
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
