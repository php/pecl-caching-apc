#ifndef INCLUDED_APC_RWLOCK
#define INCLUDED_APC_RWLOCK

/* readers-writer lock implementation; gives preference to waiting
 * writers over readers */

typedef struct apc_rwlock_t apc_rwlock_t;
struct apc_rwlock_t {
	int lock;		/* for locking access to other semaphores */
	int reader;		/* reader count (>0 if one or more readers active) */
	int writer;		/* writer count (>0 if a writer is active) */
	int waiting;	/* waiting writers count (>0 if writers waiting) */
};

extern apc_rwlock_t* apc_rwl_create(const char* pathname);
extern void          apc_rwl_destroy(apc_rwlock_t* lock);
extern void          apc_rwl_readlock(apc_rwlock_t* lock);
extern void          apc_rwl_writelock(apc_rwlock_t* lock);
extern void          apc_rwl_unlock(apc_rwlock_t* lock);

#endif
