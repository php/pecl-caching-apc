#ifndef INCLUDED_APC_SMM
#define INCLUDED_APC_SMM

#include "apc_lib.h"

/* simple shared memory manager */

void  apc_smm_init();
void  apc_smm_initsegment(int shmid, int segsize);
void  apc_smm_cleanup();
void* apc_smm_attach(int shmid);
void  apc_smm_detach(void* shmaddr);
int	  apc_smm_alloc(void* shmaddr, int size);
void  apc_smm_free(void* shmaddr, int offset);
void  apc_smm_dump(apc_outputfn_t outputfn, void* shmaddr);

#endif
