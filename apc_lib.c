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


#include "apc_lib.h"
#include "php_apc.h"
#include "zend.h"
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#undef DEBUG

extern zend_apc_globals apc_globals;

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

enum { PATH_SEPARATOR = '/' };

int apc_ropen(const char* pathname, int flags, int mode)
{
	int fd;
	char* p;

	if ((fd = open(pathname, flags, mode)) >= 0) {
		return fd;
	}

	/* under the assumption that the file could not be opened because
	 * intermediate directories to it need to be created, move along
	 * the pathname and create those directories */

	if(*pathname == PATH_SEPARATOR) {
		p = strchr(pathname +1, PATH_SEPARATOR);
	}
	else {
		p = strchr(pathname, PATH_SEPARATOR);
	}
	while (p != 0) {
		*p = '\0';
		if (mkdir(pathname, 0755) < 0 && errno != EEXIST) {
			*p = PATH_SEPARATOR;
			return -1;
		}
		*p = PATH_SEPARATOR;
		p = strchr(p + 1, PATH_SEPARATOR);
	}

	return open(pathname, flags, mode);
}

int apc_regexec(char *filename) {
	int i;
	printf("DEBUG APCG(nmatches) = %d\n", APCG(nmatches)); 
	if(!APCG(nmatches)) {
		printf("DEBUG APCG(nmatches) = 0\n"); 
		return 1;
	}
	for(i = 0; i < APCG(nmatches); i++) {
		int n;
		if(n = (regexec(&APCG(regex)[i], filename, 0, NULL, 0)) == 0) {
			printf("DEBUG %s matched %s", filename, APCG(regex_text)[i]);
			return 0;
		}
			printf("DEBUG %s didnt matched %s", filename, APCG(regex_text)[i]);
	}
	return 1;
}

const char* apc_rstat(const char* filename, const char* searchpath, struct stat *buf)
{
	static const char SEPARATOR = ':';
	static char try[1024];	/* filename to try */
	char* path;				/* working copy of searchpath */
	char* p;				/* start of current path string */
	char* q;				/* pointer to next SEPARATOR in path */

	if(!filename) {
		return filename;
	}
	/* if this is an absolute path return immediately */
	if(*filename == '.' || *filename == '/') {
		if(stat(filename, buf) == 0) {
			return filename;
		}
	}
	if (!searchpath) {
			if(stat(filename, buf) == 0) {
	        	return filename;
			}	
			else {
				return NULL;
			}
    }

	p = path = apc_estrdup(searchpath);

	do {
		if ((q = strchr(p, SEPARATOR)) != 0) {
			*q = '\0';
		}
		snprintf(try, sizeof(try), "%s/%s", p, filename);
		if (stat(try, buf) == 0) {
			free(path);
			return try;
		}
		if (q != 0) {
			q++;
		}
	} while ((p = q) != 0);

	free(path);
	return NULL;
}

int apc_check_compiled_file(const char *filename, char **dataptr, int *length)
{
	struct stat statbuf;
	int fd;
	const char *realname;
	char *buffer;
	char FIXME[1024];
	char temp[32];
	if((realname = apc_rstat(filename, PG(include_path), &statbuf)) == NULL) {
		return -1;
	}
	if((fd = open(realname, O_RDONLY)) < 0) {
		return -1;
	}
	if((buffer = (char*) mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, 
		fd, 0)) == (caddr_t) -1)
	{
		return -1;
	}
	else {
		snprintf(temp, strlen(APC_MAGIC_HEADER)+1, "%s", APC_MAGIC_HEADER);
		if(strncmp(buffer + sizeof(int) , temp, strlen(APC_MAGIC_HEADER))) {
			return -1;
		}
		else {
			*dataptr = (char *) realloc(*dataptr, 
				statbuf.st_size);
			memcpy(*dataptr, buffer, 
				statbuf.st_size);
			*length = statbuf.st_size;
		}
		munmap(buffer, statbuf.st_size);
	}
	close(fd);
	return 0;
}

/* zend stuff */

/* Following is an array that maps opcodes to textual names. Note that a
 * future release of the Zend engine may invalidate some or all of these
 * names. Fortunately, this is not mission-critical functionality.
 * Caveat emptor. */

static const char* opnames[] = {
	"ZEND_NOP",                       /* 0 */
	"ZEND_ADD",                       /* 1 */
	"ZEND_SUB",                       /* 2 */
	"ZEND_MUL",                       /* 3 */
	"ZEND_DIV",                       /* 4 */
	"ZEND_MOD",                       /* 5 */
	"ZEND_SL",                        /* 6 */
	"ZEND_SR",                        /* 7 */
	"ZEND_CONCAT",                    /* 8 */
	"ZEND_BW_OR",                     /* 9 */
	"ZEND_BW_AND",                    /* 10 */
	"ZEND_BW_XOR",                    /* 11 */
	"ZEND_BW_NOT",                    /* 12 */
	"ZEND_BOOL_NOT",                  /* 13 */
	"ZEND_BOOL_XOR",                  /* 14 */
	"ZEND_IS_IDENTICAL",              /* 15 */
	"ZEND_IS_NOT_IDENTICAL",          /* 16 */
	"ZEND_IS_EQUAL",                  /* 17 */
	"ZEND_IS_NOT_EQUAL",              /* 18 */
	"ZEND_IS_SMALLER",                /* 19 */
	"ZEND_IS_SMALLER_OR_EQUAL",       /* 20 */
	"ZEND_CAST",                      /* 21 */
	"ZEND_QM_ASSIGN",                 /* 22 */
	"ZEND_ASSIGN_ADD",                /* 23 */
	"ZEND_ASSIGN_SUB",                /* 24 */
	"ZEND_ASSIGN_MUL",                /* 25 */
	"ZEND_ASSIGN_DIV",                /* 26 */
	"ZEND_ASSIGN_MOD",                /* 27 */
	"ZEND_ASSIGN_SL",                 /* 28 */
	"ZEND_ASSIGN_SR",                 /* 29 */
	"ZEND_ASSIGN_CONCAT",             /* 30 */
	"ZEND_ASSIGN_BW_OR",              /* 31 */
	"ZEND_ASSIGN_BW_AND",             /* 32 */
	"ZEND_ASSIGN_BW_XOR",             /* 33 */
	"ZEND_PRE_INC",                   /* 34 */
	"ZEND_PRE_DEC",                   /* 35 */
	"ZEND_POST_INC",                  /* 36 */
	"ZEND_POST_DEC",                  /* 37 */
	"ZEND_ASSIGN",                    /* 38 */
	"ZEND_ASSIGN_REF",                /* 39 */
	"ZEND_ECHO",                      /* 40 */
	"ZEND_PRINT",                     /* 41 */
	"ZEND_JMP",                       /* 42 */
	"ZEND_JMPZ",                      /* 43 */
	"ZEND_JMPNZ",                     /* 44 */
	"ZEND_JMPZNZ",                    /* 45 */
	"ZEND_JMPZ_EX",                   /* 46 */
	"ZEND_JMPNZ_EX",                  /* 47 */
	"ZEND_CASE",                      /* 48 */
	"ZEND_SWITCH_FREE",               /* 49 */
	"ZEND_BRK",                       /* 50 */
	"ZEND_CONT",                      /* 51 */
	"ZEND_BOOL",                      /* 52 */
	"ZEND_INIT_STRING",               /* 53 */
	"ZEND_ADD_CHAR",                  /* 54 */
	"ZEND_ADD_STRING",                /* 55 */
	"ZEND_ADD_VAR",                   /* 56 */
	"ZEND_BEGIN_SILENCE",             /* 57 */
	"ZEND_END_SILENCE",               /* 58 */
	"ZEND_INIT_FCALL_BY_NAME",        /* 59 */
	"ZEND_DO_FCALL",                  /* 60 */
	"ZEND_DO_FCALL_BY_NAME",          /* 61 */
	"ZEND_RETURN",                    /* 62 */
	"ZEND_RECV",                      /* 63 */
	"ZEND_RECV_INIT",                 /* 64 */
	"ZEND_SEND_VAL",                  /* 65 */
	"ZEND_SEND_VAR",                  /* 66 */
	"ZEND_SEND_REF",                  /* 67 */
	"ZEND_NEW",                       /* 68 */
	"ZEND_JMP_NO_CTOR",               /* 69 */
	"ZEND_FREE",                      /* 70 */
	"ZEND_INIT_ARRAY",                /* 71 */
	"ZEND_ADD_ARRAY_ELEMENT",         /* 72 */
	"ZEND_INCLUDE_OR_EVAL",           /* 73 */
	"ZEND_UNSET_VAR",                 /* 74 */
	"ZEND_UNSET_DIM_OBJ",             /* 75 */
	"ZEND_ISSET_ISEMPTY",             /* 76 */
	"ZEND_FE_RESET",                  /* 77 */
	"ZEND_FE_FETCH",                  /* 78 */
	"ZEND_EXIT",                      /* 79 */
	"ZEND_FETCH_R",                   /* 80 */
	"ZEND_FETCH_DIM_R",               /* 81 */
	"ZEND_FETCH_OBJ_R",               /* 82 */
	"ZEND_FETCH_W",                   /* 83 */
	"ZEND_FETCH_DIM_W",               /* 84 */
	"ZEND_FETCH_OBJ_W",               /* 85 */
	"ZEND_FETCH_RW",                  /* 86 */
	"ZEND_FETCH_DIM_RW",              /* 87 */
	"ZEND_FETCH_OBJ_RW",              /* 88 */
	"ZEND_FETCH_IS",                  /* 89 */
	"ZEND_FETCH_DIM_IS",              /* 90 */
	"ZEND_FETCH_OBJ_IS",              /* 91 */
	"ZEND_FETCH_FUNC_ARG",            /* 92 */
	"ZEND_FETCH_DIM_FUNC_ARG",        /* 93 */
	"ZEND_FETCH_OBJ_FUNC_ARG",        /* 94 */
	"ZEND_FETCH_UNSET",               /* 95 */
	"ZEND_FETCH_DIM_UNSET",           /* 96 */
	"ZEND_FETCH_OBJ_UNSET",           /* 97 */
	"ZEND_FETCH_DIM_TMP_VAR",         /* 98 */
	"ZEND_FETCH_CONSTANT",            /* 99 */
	"ZEND_DECLARE_FUNCTION_OR_CLASS", /* 100 */
	"ZEND_EXT_STMT",                  /* 101 */
	"ZEND_EXT_FCALL_BEGIN",           /* 102 */
	"ZEND_EXT_FCALL_END",             /* 103 */
	"ZEND_EXT_NOP",                   /* 104 */
	"ZEND_TICKS",                     /* 105 */
	"ZEND_SEND_VAR_NO_REF"            /* 106 */
};

static const char* extopnames[] = {
	"",                               /* 0 */
	"ZEND_DECLARE_CLASS",             /* 1 */
	"ZEND_DECLARE_FUNCTION",          /* 2 */
	"ZEND_DECLARE_INHERITED_CLASS"    /* 3 */
};

static const char UNKNOWN_OP[] = "(unrecognized opcode)";

#define NELEMS(a) (sizeof(a) / sizeof(a[0]))

/* apc_get_zend_opname: return the name for the given opcode */
const char* apc_get_zend_opname(int opcode)
{
	if (opcode < 0 || opcode >= NELEMS(opnames)) {
		return UNKNOWN_OP;
	}
	return opnames[opcode];
}

/* apc_get_zend_extopname: return the name for the given extended pcode */
const char* apc_get_zend_extopname(int opcode)
{
	if (opcode < 0 || opcode >= NELEMS(extopnames)) {
		return UNKNOWN_OP;
	}
	return extopnames[opcode];
}

