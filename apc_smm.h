#ifndef INCLUDED_APC_SMM
#define INCLUDED_APC_SMM

#include "apc_lib.h"

/* simple shared memory manager */

extern void  apc_smm_init();
extern void  apc_smm_initsegment(int shmid, int segsize);
extern void  apc_smm_cleanup();
extern void* apc_smm_attach(int shmid);
extern void  apc_smm_detach(void* shmaddr);
extern int	 apc_smm_alloc(void* shmaddr, int size);
extern void  apc_smm_free(void* shmaddr, int offset);
extern void  apc_smm_dump(void* shmaddr, apc_outputfn_t outputfn);

#endif
