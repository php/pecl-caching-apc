#ifndef INCLUDED_APC_LIB
#define INCLUDED_APC_LIB

/* generic printf-like function ptr type */

typedef int (*apc_outputfn_t)(const char*, ...);


/* wrappers for memory allocation routines */

extern void* apc_emalloc(size_t n);
extern void* apc_erealloc(void* p, size_t n);
extern void  apc_efree(void* p);
extern char* apc_estrdup(const char* s);


/* simple display facility */

extern void apc_eprint(char *fmt, ...);
extern void apc_dprint(char *fmt, ...);


/* simple timer facility */

extern void apc_timerstart(void);
extern void apc_timerstop(void);
extern double apc_timerreport(void);

#endif
