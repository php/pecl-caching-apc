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
  | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$ */

#include "apc.h"

#if APC_MMAP

#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

/* 
 * Some operating systems (like FreeBSD) have a MAP_NOSYNC flag that
 * tells whatever update daemons might be running to not flush dirty
 * vm pages to disk unless absolutely necessary.  My guess is that
 * most systems that don't have this probably default to only synching
 * to disk when absolutely necessary.
 */
#ifndef MAP_NOSYNC
#define MAP_NOSYNC 0
#endif

void *apc_mmap(char *file_mask, size_t size)
{
    void* shmaddr;  /* the shared memory address */

    /* If no filename was provided, do an anonymous mmap */
    if(!file_mask || (file_mask && !strlen(file_mask))) {
        shmaddr = (void *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    } else {
        int fd;

        /* 
         * If the filemask contains .shm we try to do a POSIX-compliant shared memory
         * backed mmap which should avoid synchs on some platforms.  At least on
         * FreeBSD this implies MAP_NOSYNC and on Linux it is equivalent of mmap'ing
         * a file in a mounted shmfs.  For this to work on Linux you need to make sure
         * you actually have shmfs mounted.  Also on Linux, make sure the file_mask you
         * pass in has a leading / and no other /'s.  eg.  /apc.shm.XXXXXX
         * On FreeBSD these are mapped onto the regular filesystem so you can put whatever
         * path you want here.
         */
        if(strstr(file_mask,".shm")) {
            mktemp(file_mask);
            fd = shm_open(file_mask, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
            if(fd == -1) {
                apc_eprint("apc_mmap: shm_open on %s failed:", file_mask);
                return (void *)-1;
            }
            if (ftruncate(fd, size) < 0) {
                close(fd);
                shm_unlink(file_mask);
                apc_eprint("apc_mmap: ftruncate failed:");
                return (void *)-1;
            }
            shmaddr = (void *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            shm_unlink(file_mask);
            close(fd);
        }
        /*
         * Support anonymous mmap through the /dev/zero interface as well
         */
        else if(!strcmp(file_mask,"/dev/zero")) {
            fd = open("/dev/zero", O_RDWR, S_IRUSR | S_IWUSR);
            if(fd == -1) {
                apc_eprint("apc_mmap: open on /dev/zero failed:");
                return (void *)-1;
            }
            shmaddr = (void *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            close(fd);
        }
        /* 
         * Otherwise we do a normal filesystem mmap
         */
        else {
            fd = mkstemp(file_mask);
            if(fd == -1) {
                apc_eprint("apc_mmap: mkstemp on %s failed:", file_mask);
                return (void *)-1;
            }
            if (ftruncate(fd, size) < 0) {
                close(fd);
                unlink(file_mask);
                apc_eprint("apc_mmap: ftruncate failed:");
            }
            shmaddr = (void *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NOSYNC, fd, 0);
            close(fd);
            unlink(file_mask);
        }
    }
    if((int)shmaddr == -1) {
        apc_eprint("apc_mmap: mmap failed:");
    }
    return shmaddr;
}

void apc_unmap(void* shmaddr, size_t size)
{
    munmap(shmaddr, size);
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
