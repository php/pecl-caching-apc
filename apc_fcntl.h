/* ==================================================================
 * APC Cache
 * Copyright (c) 2000 Community Connect, Inc.
 * All rights reserved.
 * ==================================================================
 * This source code is made available free and without charge subject
 * to the terms of the QPL as detailed in bundled LICENSE file, which
 * is also available at http://apc.communityconnect.com/LICENSE.
 * ==================================================================
 * Daniel Cowgill <dan@mail.communityconnect.com>
 * George Schlossnagle <george@lethargy.org>
 * ==================================================================
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

/* lockreg: fcntl wrapper */
static int lockreg(int fd, int cmd, int type, off_t offset,
	int whence, off_t len)
{
	struct flock lock;

	lock.l_type   = type;
	lock.l_start  = offset;
	lock.l_whence = whence;
	lock.l_len    = len;
	
	return fcntl(fd, cmd, &lock);
}

void apc_fcntl_readlock(int fd);

void apc_fcntl_readlockwait(int fd);

void apc_fcntl_writelock(int fd);

void apc_fcntl_writelockwait(int fd);


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
#endif
