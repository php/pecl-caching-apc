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


#ifndef INCLUDED_APC_LIB
#define INCLUDED_APC_LIB
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "zend.h"
#include "php_globals.h"
#include "apc_phpdeps.h"

/* generic printf-like function ptr type */

typedef int (*apc_outputfn_t)(const char*, ...);


/* wrappers for memory allocation routines */

extern void* apc_emalloc (size_t n);
extern void* apc_erealloc(void* p, size_t n);
extern void  apc_efree   (void* p);
extern char* apc_estrdup (const char* s);


/* simple display facility */

extern void apc_eprint(char *fmt, ...);
extern void apc_dprint(char *fmt, ...);


/* simple timer facility */

extern void   apc_timerstart (void);
extern void   apc_timerstop  (void);
extern double apc_timerreport(void);


/* filesystem routines */
extern int apc_regexec(char const *filename);

extern int apc_ropen(const char *pathname, int flags, int mode);
extern const char* apc_rstat(const char* filename, const char* searchpath, 
	struct stat *buf);
extern int apc_check_compiled_file(const char *filename, char **dataptr, int *length)
;

/* zend-related */

extern const char* apc_get_zend_opname(int opcode);
extern const char* apc_get_zend_extopname(int opcode);

/* architecture dependence */
extern int alignword(int x);
extern int alignword_int(int x);

#endif
