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


#ifndef INCLUDED_APC_LIB
#define INCLUDED_APC_LIB

#include <stdlib.h>

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

extern int apc_ropen(const char *pathname, int flags, int mode);

#endif
