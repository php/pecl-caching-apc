#ifndef INCLUDED_APC_SEM
#define INCLUDED_APC_SEM

/* semaphore wrapper. no surprises */

extern int  apc_sem_create(const char* pathname, int proj, int initval);
extern void apc_sem_destroy(int semid);
extern void apc_sem_lock(int semid);
extern void apc_sem_unlock(int semid);
extern void apc_sem_waitforzero(int semid);
extern int  apc_sem_getvalue(int semid);

#endif
