/* 
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,       |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/3_0.txt.                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "apc.h"

#if APC_MMAP

#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

void *apc_mmap(char *file_mask, int size)
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
            shmaddr = (void *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            close(fd);
            unlink(file_mask);
        }
    }
    if((int)shmaddr == -1) {
        apc_eprint("apc_mmap: mmap failed:");
    }
    return shmaddr;
}

void apc_unmap(void* shmaddr, int size)
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
