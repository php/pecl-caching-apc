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
   | Authors: George Schlossnagle <george@omniti.com>                     |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "apc_fcntl.h"
#include "apc.h"
#include <unistd.h>
#include <fcntl.h>

int apc_fcntl_create(const char* pathname)
{
    int fd;
    char *lock_path = pathname;
    if(pathname == NULL) {
        lock_path = malloc(strlen("/tmp/.apc.") + 6);
        snprintf(lock_path, strlen("/tmp/.apc.") + 6, "/tmp/.apc.%d", getpid());
    }
    fd = open(lock_path, O_RDWR|O_CREAT, 0666);
    if(fd > 0 ) {
        unlink(lock_path);
        return fd;
    } else {
        apc_eprint("apc_fcntl_create: open(%s, O_RDWR|O_CREAT, 0666) failed:", lock_path);
    }
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
