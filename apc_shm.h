#ifndef INCLUDED_APC_SHM
#define INCLUDED_APC_SHM

/* shared memory wrapper. no surprises */

extern int   apc_shm_create(const char* pathname, int proj, int size);
extern void  apc_shm_destroy(int shmid);
extern void* apc_shm_attach(int shmid);
extern void  apc_shm_detach(void* shmaddr);

#endif
