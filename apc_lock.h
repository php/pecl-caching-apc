#ifndef APC_LOCK
#define APC_LOCK

#include "apc_sem.h"
#include "apc_fcntl.h"
#include "config.h"

#ifdef APC_FCNTL_LOCKS
#define apc_lck_create(a,b,c) apc_fcntl_create((a))
#define apc_lck_destroy(a)    apc_fcntl_destroy(a)
#define apc_lck_lock(a)       apc_fcntl_lock(a)
#define apc_lck_unlock(a)     apc_fcntl_unlock(a)
#else
#define apc_lck_create(a,b,c) apc_sem_create((a),(b),(c))
#define apc_lck_destroy(a)    apc_sem_destroy(a)
#define apc_lck_lock(a)       apc_sem_lock(a)
#define apc_lck_unlock(a)     apc_sem_unlock(a)
#endif

#endif
