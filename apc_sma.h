#ifndef APC_SMA_H
#define APC_SMA_H

/* Simple shared-memory allocator (wrapper for apc_smm). */

extern void		apc_sma_readlock	();
extern void		apc_sma_writelock	();
extern void		apc_sma_unlock		();
extern void*	apc_sma_alloc		(int n);

/*extern void*	apc_sma_free		(void* p);*/
/*extern char*	apc_sma_strdup		(const char* s);*/

extern void		apc_sma_cleanup		();

#endif

