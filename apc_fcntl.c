/* ==================================================================
 * APC Cache
 * Copyright (c) 2000-2001 Community Connect, Inc.
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


#include "apc_fcntl.h"

/* lock_reg fcntl wrapper */
int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
  struct flock lock;

  lock.l_type = type;
  lock.l_start = offset;
  lock.l_whence = whence;
  lock.l_len = len;

  return( fcntl(fd, cmd, &lock) );
}

int apc_unlink(char *filename)
{
	int fd;

	fd = open(filename, O_RDONLY);
	writew_lock(fd, 0, SEEK_SET, 0);  //we only need to lock 1 byte
	unlink(filename);
	close(fd);
	un_lock(fd, 0, SEEK_SET, 0);
}
