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
#include <errno.h>
#include <string.h>

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

int apc_flock_create(const char* filename)
{
	return open(filename, O_RDWR|O_CREAT, 0666);
}

int apc_unlink(char *filename)
{
	int fd;
	int res;

	fd = open(filename, O_RDONLY);
	writew_lock(fd, 0, SEEK_SET, 0);  /* we only need to lock 1 byte */
	res = unlink(filename);
	close(fd);
	un_lock(fd, 0, SEEK_SET, 0);
	return res;
}

int apc_writew_lock_key(const char* key, apc_nametable_t* locktable,
						apc_nametable_t* opentable)
{
	int fd;
	if(fd = (int) apc_nametable_retrieve(locktable, key)) {
		return fd;
	}
    if((fd = (int) apc_nametable_retrieve(opentable, key)) == 0)  {
        if((fd =  open(key, O_RDWR)) < 0) {
            fprintf(stderr, "failed opene\n");
            return 0;
        }
		fprintf(stderr, "failed to fond open copy of %s\n", key);
        apc_nametable_insert(opentable, key, (void *) fd);
    }
	if(writew_lock(fd, 0, SEEK_SET, 0)) {
		close(fd);
	 	return 0;
	}
	return fd;
}

int apc_write_lock_key(const char* key, apc_nametable_t* locktable, 
						apc_nametable_t* opentable)
{
	int fd;
	int err;
	if(fd = (int) apc_nametable_retrieve(locktable, key)) {
		return fd;
	}
	if((fd = (int) apc_nametable_retrieve(opentable, key)) == 0)  {
		if((fd =  open(key, O_RDWR)) < 0) {
			fprintf(stderr, "failed opene\n");
			return 0;
		}
		fprintf(stderr, "failed to fond open copy of %s\n", key);
		apc_nametable_insert(opentable, key, (void *) fd);
	}
	if(err = write_lock(fd, 0, SEEK_SET, 0)) {
		close(fd);
		apc_nametable_remove(opentable, key);
		fprintf(stderr, "failure to lock %d\n", err);
	 	return 0;
	}
	return fd;
}

int apc_readw_lock_key(const char* key, apc_nametable_t* locktable,
						apc_nametable_t* opentable)
{
	int fd;
	if(fd = (int) apc_nametable_retrieve(locktable, key)) {
		return fd;
	}
    if((fd = (int) apc_nametable_retrieve(opentable, key)) == 0)  {
        if((fd =  open(key, O_RDWR)) < 0) {
            fprintf(stderr, "failed opene\n");
            return 0;
        }
		fprintf(stderr, "failed to fond open copy of %s\n", key);
    	apc_nametable_insert(opentable, key, (void *) fd);
    }
	if(readw_lock(fd, 0, SEEK_SET, 0)) {
		close(fd);
	 	return 0;
	}
	return fd;
}

int apc_read_lock_key(const char* key, apc_nametable_t* locktable,
						apc_nametable_t* opentable)
{
	int fd;
	if(fd = (int) apc_nametable_retrieve(locktable, key)) {
		return fd;
	}
    if((fd = (int) apc_nametable_retrieve(opentable, key)) == 0)  {
        if((fd =  open(key, O_RDWR)) < 0) {
            fprintf(stderr, "failed opene\n");
            return 0;
        }
		fprintf(stderr, "failed to fond open copy of %s\n", key);
    	apc_nametable_insert(opentable, key, (void *) fd);
	}
	if(read_lock(fd, 0, SEEK_SET, 0)) {
		close(fd);
		fprintf(stderr, "failure\n");
	 	return 0;
	}
	return fd;
}

int apc_un_lock_key(const char* key, apc_nametable_t* locktable, 
					apc_nametable_t* opentable)
{
	int fd;
	if( fd = (int) apc_nametable_retrieve(locktable, key) ){
		un_lock(fd, 0, SEEK_SET, 0);
		apc_nametable_remove(locktable, key);
	}
	return 1;
}

void apc_un_lock_nametable(char *key, void* fd)
{
	un_lock((int) fd, 0, SEEK_SET, 0);
}
