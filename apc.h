/* 
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/3_0.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef APC_H
#define APC_H

/*
 * This module defines utilities and helper functions used elsewhere in APC.
 */

/* Commonly needed C library headers. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* UNIX headers (needed for struct stat) */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"

/* log levels constants (see apc_log) */
enum { APC_DEBUG, APC_NOTICE, APC_WARNING, APC_ERROR };

/* typedefs for extensible memory allocators */
typedef void* (*apc_malloc_t)(size_t);
typedef void  (*apc_free_t)  (void*);

/* wrappers for memory allocation routines */
extern void* apc_emalloc(size_t n);
extern void* apc_erealloc(void* p, size_t n);
extern void apc_efree(void* p);
extern char* apc_estrdup(const char* s);
extern void* apc_xstrdup(const char* s, apc_malloc_t f);
extern void* apc_xmemcpy(const void* p, size_t n, apc_malloc_t f);

/* console display functions */
extern void apc_log(int level, const char* fmt, ...);
extern void apc_eprint(const char* fmt, ...);
extern void apc_wprint(const char* fmt, ...);
extern void apc_dprint(const char* fmt, ...);

/* string and text manipulation */
extern char* apc_append(const char* s, const char* t);
extern char* apc_substr(const char* s, int start, int length);
extern char** apc_tokenize(const char* s, char delim);

/* filesystem functions */
extern int apc_stat_paths(const char* filename, const char* path, struct stat*);

/* regular expression wrapper functions */
extern void* apc_regex_compile_array(char* patterns[]);
extern void apc_regex_destroy_array(void* p);
extern int apc_regex_match_array(void* p, const char* input);

/* apc_crc32: returns the CRC-32 checksum of the first len bytes in buf */
extern unsigned int apc_crc32(const char* buf, int len);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
