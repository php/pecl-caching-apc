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


#include "apc_lib.h"
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define DEBUG 1
#undef DEBUG

/* apc_emalloc: malloc that dies on failure */
void* apc_emalloc(size_t n)
{
	void* p = malloc(n);
	if (p == NULL) {
		apc_eprint("apc_emalloc: malloc failed to allocate %u bytes:", n);
	}
	return p;
}

/* apc_erealloc: realloc that dies on failure */
void* apc_erealloc(void* p, size_t n)
{
	p = realloc(p, n);
	if (p == NULL) {
		apc_eprint("apc_erealloc: realloc failed to allocate %u bytes:", n);
	}
	return p;
}

/* apc_efree: free that bombs when given a null pointer */
void apc_efree(void* p)
{
	if (p == NULL) {
		apc_eprint("apc_efree: attempt to free null pointer");
	}
	free(p);
}

/* apc_estrdup: strdup that dies on failure */
char* apc_estrdup(const char* s)
{
	int len;
	char* dup;

	if (s == NULL) {
		return NULL;
	}
	len = strlen(s);
	dup = (char*) malloc(len+1);
	if (dup == NULL) {
		apc_eprint("apc_estrdup: malloc failed to allocate %u bytes:", len+1);
	}
	memcpy(dup, s, len);
	dup[len] = '\0';
	return dup;
}

/* apc_eprint: print error message and exit */
void apc_eprint(char *fmt, ...)
{
	va_list args;

	fflush(stdout);
	
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':') {
		fprintf(stderr, " %s", strerror(errno));
	}
	fprintf(stderr, "\n");
	exit(2);
}

/* apc_dprint: print messages if DEBUG is defined */
void apc_dprint(char *fmt, ...)
{
#ifdef DEBUG
	va_list args;

	printf("DEBUG: ");

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':') {
		printf(" %s", strerror(errno));
	}
#endif
}


static double start, end;

/* apc_timerstart: start the timer */
void apc_timerstart()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	start = tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* apc_timerstop: stop the timer */
void apc_timerstop()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	end = tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* apc_timerreport: print and return time elapsed */
double apc_timerreport()
{
	printf("elapsed time: %.3g seconds\n", end - start);
	return end - start;
}

/* recursive open */

apc_ropen(const char *pathname, int flags, int mode)
{
	int fd;

	if ((fd = open(pathname, flags, mode)) < 0 )
	{
  	char  *cp2;
  	cp2 = strchr(pathname, '/');
  	while(cp2!=NULL)
  	{
    	errno = 0;
    	cp2 = strchr(++cp2, '/');
    	if(cp2 != NULL)
    	{
      	*cp2 = '\0';
      	if(mkdir(pathname, 0755) != 0)
      	{
        	if(errno != EEXIST)
        	{
          	*cp2 = '/';
          	return -1;
        	}
      	}
      	*cp2 = '/';
    	}
  	}
  	return open(pathname, flags, mode);
	}
	else return fd;
}


