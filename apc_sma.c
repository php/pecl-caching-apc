#include "apc_sma.h"
#include "apc_shm.h"
#include "apc_smm.h"
#include "apc_rwlock.h"
#include <assert.h>

enum {
	SEGSIZE = 1024*1024,	/* default segment size (FIXME: make ini entry) */
	MAXSEG  = 10,			/* maximum number of shared memory segments */
};

static int				s_initialized		= 0;
static apc_rwlock_t*	s_lock				= 0;
static int				s_maxseg			= 0;
static int				s_shmid[MAXSEG];
static void*			s_shmaddr[MAXSEG];

/*
 * newseg: Creates a new shm segment and return non-zero on success. Returns
 * zero if a new segment could not be created.
 */
static int newseg()
{
	int		shmid;
	void*	shmaddr;

	if (s_maxseg >= MAXSEG) {
		return 0;
	}
	shmid = apc_shm_create(NULL, 0, SEGSIZE);
	apc_smm_initsegment(shmid, SEGSIZE);
	shmaddr = apc_smm_attach(shmid);
	s_shmid[s_maxseg] = shmid;
	s_shmaddr[s_maxseg] = shmaddr;
	s_maxseg++;
	return 1;
}

/*
 * init: Initializes the shared memory allocator.
 */
static void init()
{
	int rc;

	apc_smm_init();
	s_lock = apc_rwl_create(NULL);
	rc = newseg();
	assert(rc);
	s_initialized = 1;
}

void apc_sma_cleanup()
{
	if (s_initialized) {
		apc_smm_cleanup();
		apc_rwl_destroy(s_lock);
		s_initialized = 0;
	}
}

void apc_sma_readlock()
{
	if (!s_initialized) init();
	apc_rwl_readlock(s_lock);
}

void apc_sma_writelock()
{
	if (!s_initialized) init();
	apc_rwl_writelock(s_lock);
}

void apc_sma_unlock()
{
	if (!s_initialized) init();
	apc_rwl_unlock(s_lock);
}

void* apc_sma_alloc(int n)
{
	int		off;
	int		i;

	if (!s_initialized) init();

	for (i = s_maxseg-1; i >= 0; i--) {
		off = apc_smm_alloc(s_shmaddr[i], n, 1);
		if (off != -1) {
			return s_shmaddr[i] + off;
		}
	}

	if (newseg()) {
		off = apc_smm_alloc(s_shmaddr[s_maxseg-1], n, 1);
		if (off != -1) {
			return s_shmaddr[s_maxseg-1] + off;
		}
	}

	assert(0);	/* out of memory */
}

/*
char* apc_sma_strdup(const char* s)
{
	int		n;
	char*	t;

	if (!s_initialized) init();

	n = strlen(s) + 1;
	t = (char*) apc_sma_alloc(n);
	memcpy(t, s, n);
	return t;
}
*/
