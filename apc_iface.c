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
 * Mike Bretz <mike@metropolis-ag.de>
 * ==================================================================
*/


#include "apc_iface.h"
#include "apc_fcntl.h"
#include "apc_cache.h"
#include "apc_cache_mm.h"
#include "apc_serialize.h"
#include "apc_sma.h"
#include "apc_lib.h"
#include "apc_list.h"
#include "apc_sma.h"
#include "apc_crc32.h"
#include "apc_nametable.h"
#include "apc_version.h"
#include "php_apc.h"

/* must include zend headers */
#include "zend.h"
#include "zend_compile.h"
#include "zend_constants.h"

/* include php_globals.h for PG() macro */
#include "php_globals.h"

#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>


/* undef FCNTL_ADVISORY_LOCKING to remove all advisory locking.  This
        _should_ be safe, but is not fully tested.  It will provide a speed
        increase under systems which experience high rates of context switching.
*/

#define FCNTL_ADVISORY_LOCKING

/* PATH_MAX is used with realpath(3), but is not defined on all systems.
 * If it is undefined, choose a reasonable default value. */
#ifndef PATH_MAX
# define PATH_MAX 2048
#endif

extern zend_apc_globals apc_globals;
char VERSION_STRING[100];

/* pointer to the previous compile_file function */
static ZEND_API zend_op_array* (*old_compile_file)(zend_file_handle*, int CLS_DC);

/* out compile_file function */
static ZEND_API zend_op_array* apc_compile_file(zend_file_handle*, int CLS_DC);
static ZEND_API zend_op_array* apc_shm_compile_file(zend_file_handle*, int CLS_DC);
static ZEND_API zend_op_array* apc_mmap_compile_file(zend_file_handle*, int CLS_DC);

/* pointer to the previous execute function */
static ZEND_API void (*old_execute)(zend_op_array* op_array ELS_DC);

/* our execute function */
static ZEND_API void apc_execute(zend_op_array* op_array ELS_DC);

enum {
	FILE_TABLE_SIZE          = 97,	/* buckets in file table */
	ACC_FUNCTION_TABLE_SIZE  = 97,	/* buckets in accumulator function table */
	ACC_CLASS_TABLE_SIZE     = 97,	/* buckets in accumulator class table */
	PRIV_FUNCTION_TABLE_SIZE = 17,	/* buckets in private function table */
	PRIV_CLASS_TABLE_SIZE    = 1,	/* buckets in private class table */
};

/* pointer to the shared cache */
static apc_cache_t* cache;

/* the acc_*table variables are used during deserialization. they
 * accumulate the names of defined objects, which allows us to
 * detect differences in the global function/class tables during
 * the inclusion and parsing of source files */
static apc_nametable_t* filetable;
static apc_nametable_t* acc_functiontable;
static apc_nametable_t* acc_classtable;

/* these variables are used to deserialize the top-level zend_op_array */
static char* inputbuf;
static int inputlen;
static int inputsize;

/* The apc_mm_fl table is used by each child to keep track of the cache files it
 * currently has mmap'd. */

static HashTable apc_mm_fl;
static void mm_dtor(void* p)
{
        struct mm_fl_element *element;
        element = (struct mm_fl_element* ) p;
        free(element->cache_filename);
        return;
}

/* function prototypes */
int apc_mmap_rm(const char *filename);
extern int apc_cache_info_mmap(zval **hash);

/* generate_key: generate the cache key based on the actual filename. If
 * we support relative includes, we must generate a unique key for the
 * file; otherwise, we simply return the filename to the caller */
static const char* generate_key(const char* filename, struct stat *buf)
{
	static char resolved_path[PATH_MAX];	/* storage for realpath */
	struct stat st;
	struct stat *st_ptr;

	if (buf == 0) {
		st_ptr= &st;
	}
	else {
		st_ptr = buf;
	}
	if(APCG(relative_includes)) {
		const char* realname = apc_rstat(filename, PG(include_path), st_ptr);
		if (!realname || realpath(realname, resolved_path)  == NULL) {
			return 0; /* error */
		}
		return resolved_path;
	}
	else if( APCG(check_mtime) && buf != NULL) {
		stat(filename, st_ptr);
	}
	return filename;
}

static apc_outputfn_t outputfn = printf;	/* by default */

static void tabledestructor(void* p)
{
	apc_nametable_t** tables = (apc_nametable_t**) p;
	apc_nametable_destroy(tables[0]);
	apc_nametable_destroy(tables[1]);
	free(tables);
}

/* apc_setoutputfn: set the output function for this module */
void apc_setoutputfn(apc_outputfn_t fn)
{
	outputfn = fn;
}

void apc_module_init()
{
	apc_dprint("apc_module_init()\n");

        switch (APCG(mode)) {
          case SHM_MODE: snprintf(VERSION_STRING, sizeof(VERSION_STRING)-1, "%s (%s)", APC_VERSION, "SHM"); break;
          case MMAP_MODE: snprintf(VERSION_STRING, sizeof(VERSION_STRING)-1, "%s (%s)", APC_VERSION, "MMAP"); break;
          default: snprintf(VERSION_STRING, sizeof(VERSION_STRING)-1, "%s (%s)", APC_VERSION, "OFF"); break;
        }

	/* replace zend_compile_file with our function */
	old_compile_file = zend_compile_file;
	zend_compile_file = apc_compile_file;
 	old_execute = zend_execute;
	zend_execute = apc_execute;

	/* initialize the accumulator tables */
        if (APC_MMAP_MODE)
          zend_hash_init(&apc_mm_fl, 10, NULL, mm_dtor, 1);

	filetable          = apc_nametable_create(FILE_TABLE_SIZE);
	acc_functiontable  = apc_nametable_create(ACC_FUNCTION_TABLE_SIZE);
	acc_classtable     = apc_nametable_create(ACC_CLASS_TABLE_SIZE);

	/* do APC one-time initialization */
	if (APC_SHM_MODE) {
		apc_sma_init(APCG(shm_segments), APCG(shm_segment_size));
		cache = apc_cache_create(NULL, APCG(hash_buckets), APCG(ttl));
	}

	/* initialize serialization buffers */
	inputsize = 1;
	inputbuf  = (char*) malloc(inputsize);
	inputlen  = 0;
}

void apc_module_shutdown()
{
	apc_dprint("apc_module_shutdown()\n");
	
	/* restore the state of zend_compile_file */
	zend_compile_file = old_compile_file;

	/* do apc cleanup */
	free(inputbuf);

        if (APC_MMAP_MODE) {
          zend_hash_clean(&apc_mm_fl);	
		}
	if (APC_SHM_MODE) {
		  apc_sma_cleanup();
          apc_cache_destroy(cache);
	}
	apc_nametable_clear(filetable, tabledestructor);
	apc_nametable_destroy(filetable);
	apc_nametable_destroy(acc_functiontable);
	apc_nametable_destroy(acc_classtable);
}

void apc_request_init()
{
	apc_dprint("apc_request_init()\n");

	/* clear all elements from the accumulator tables */
	apc_nametable_clear(filetable, tabledestructor);
	apc_nametable_clear(acc_functiontable, 0);
	apc_nametable_clear(acc_classtable, 0);

	/* initialize serializer module */
	apc_serializer_request_init();
}

void apc_request_shutdown()
{
	apc_dprint("apc_request_shutdown()\n");

	/* clean up serializer module */
	apc_serializer_request_shutdown();
}

void apc_module_info(const char* url)
{
  if (APC_SHM_MODE) {
    apc_cache_dump(cache, url, zend_printf);
    return;
  }

  if (APC_MMAP_MODE) {
    apc_mmap_dump(&apc_mm_fl, url, zend_printf);
    return;
  }

  zend_printf("<HR>\n");
  zend_printf("APC Cache is switched off\n");
  zend_printf("<HR>\n");
}

const char* apc_version()
{
	return VERSION_STRING;
}

int apc_rm(const char* filename)
{
  if (APC_MMAP_MODE)
    return apc_mmap_rm(filename);

  if (APC_SHM_MODE)
    return apc_shm_rm(cache, generate_key(filename, NULL));

  return -1;
}

void apc_reset_cache(void)
{
   if (APC_SHM_MODE)
	apc_cache_clear(cache);
}

void apc_set_object_ttl(const char* filename, int ttl)
{
   if (APC_SHM_MODE)
	apc_cache_set_object_ttl(cache, generate_key(filename, NULL), ttl);
}

int apc_dump_cache_object(const char* filename, apc_outputfn_t outputfn)
{
   if (APC_SHM_MODE)
	return apc_cache_dump_entry(cache, filename, outputfn);

   if (APC_MMAP_MODE) {
     char realname[PATH_MAX];
     const char *name;
     if(APCG(relative_includes)) {
       if (realpath(filename, realname) == NULL)
         name = filename;
       else
         name = realname;
     }
     else {
       name = filename;
     }
     return apc_mmap_dump_entry(name, outputfn);
   }
   return -1;
}

int apc_cache_index(zval** hash)
{
  switch(APCG(mode)) {
   case SHM_MODE:
	return apc_cache_index_shm(cache, hash);
   case MMAP_MODE:
        return apc_cache_index_mmap(&apc_mm_fl, hash);
  }
  return -1;
}

int apc_cache_info(zval** hash)
{
  switch(APCG(mode)) {
    case SHM_MODE:
        return apc_cache_info_shm(cache, hash);
    case MMAP_MODE:
        return apc_cache_info_mmap(hash);
  }
  return -1;
}

int apc_object_info(char const *filename, zval** hash)
{
   if (APC_SHM_MODE)
        return apc_object_info_shm(cache, filename, hash);

   return 0;
}

/* dummy function to reset ref counts */
static void increment_refcount(void *d) 
{
	uint *refcount;
	refcount = (uint *)d;
	refcount[0] = 2;
}

/* apc_execute: replacement for zend_compile_file to allow for refcount reset*/
static ZEND_API void apc_execute(zend_op_array* op_array ELS_DC)
{
	old_execute(op_array ELS_DC);
	apc_list_apply((apc_list*)op_array->reserved[0], increment_refcount);
}

/* apc_compile_file: replacement for zend_compile_file */
ZEND_API zend_op_array* apc_compile_file(zend_file_handle *file_handle,
	int type CLS_DC)
{
    if (APC_OFF_MODE)
        return old_compile_file(file_handle, type CLS_CC);

    /* hack to allow included urls to be handled unmolested */
    if(!strncasecmp(file_handle->filename, "http://", 7)) { 
        return old_compile_file(file_handle, type CLS_CC);
    }

    if (APC_SHM_MODE)
        return apc_shm_compile_file(file_handle, type CLS_DC);

    if (APC_MMAP_MODE)
        return apc_mmap_compile_file(file_handle, type CLS_DC);

    /* Should not happen -> But to be sure something is called call the original compiler */
    return old_compile_file(file_handle, type CLS_CC);
}

/* apc_compile_file: replacement for zend_compile_file */
ZEND_API zend_op_array* apc_shm_compile_file(zend_file_handle *file_handle,
	int type CLS_DC)
{
	const char* key;			/* key into the cache index */
	zend_op_array* op_array;	/* the instruction sequence for the file */
	apc_nametable_t** tables;	/* private tables for the file (see below) */
	int seen;					/* seen this file before, this request? */
	int mtime;					/* modification time of the file */
	int numclasses;


	/* If the user has set the check_mtime ini entry to true, we must
	 * compare the current modification time of the every file against
	 * its last known value, and if it has been modified, we recompile
	 * it. Otherwise, we set mtime to zero, which disables the check */
	struct stat st;
	mtime = 0;	/* assume we do not check */

	if (!(key = generate_key(file_handle->filename, &st))) {
		/* We can't create a valid cache key for this file. The only/best
		 * recourse is to compile the file and return immediately. */

		return old_compile_file(file_handle, type CLS_CC);
	}

	if (APCG(check_mtime)) {
		mtime = st.st_mtime;
	}

	/* Retrieve the private function and class tables for this file.
	 * If they do not already exist, create them now and map them to
	 * the file in filetable. */

	tables = (apc_nametable_t**) apc_nametable_retrieve(filetable, key);
	
	if (tables == 0) {
		tables = (apc_nametable_t**) apc_emalloc(2*sizeof(apc_nametable_t*));
		apc_nametable_insert(filetable, key, tables);
		tables[0] = apc_nametable_create(PRIV_FUNCTION_TABLE_SIZE);
		tables[1] = apc_nametable_create(PRIV_CLASS_TABLE_SIZE);
		seen = 0; 
	}
	else {
		seen = 1; /* the tables already exist: we've encounterd this
		           * file before, during this very request */
	}

	/* Lookup the file in the shared cache. if it exists, deserialize it
	 * and return, skipping the compile step. */

	if (apc_cache_retrieve(cache, key, &inputbuf, &inputlen,
		&inputsize, mtime) == 1)
	{
		char *opkey;
		int dummy, dummy2;
		zend_op_array *new_op_array;
		zend_llist_add_element(&CG(open_files), file_handle); /*  FIXME */
		apc_init_deserializer(inputbuf, inputlen);
		op_array = (zend_op_array*) emalloc(sizeof(zend_op_array));

		/* Deserialize the global function/class tables. Every object that
		 * is deserialized is also inserted into the file's private tables
		 * and into the accumulator tables. */
		if(apc_deserialize_magic()) {
			zend_error(E_ERROR, "%s is not a APC compiled object",
				key);
		}
		apc_deserialize_zend_function_table(CG(function_table),
			acc_functiontable, tables[0]);
		numclasses = apc_deserialize_zend_class_table(CG(class_table), 
			acc_classtable, tables[1]);
		opkey = (char *)apc_emalloc(strlen(key) + 4);
		snprintf(opkey, strlen(key) + 4, "op:%s", key);
		if(apc_cache_retrieve(cache, opkey, (char **) &new_op_array, &dummy,
			&dummy2, mtime) != 1) 
		{
			assert(0);
		}
		memcpy(op_array, new_op_array, sizeof(zend_op_array));
		return op_array;
	}

	/* Could not retrieve this file from the cache, compile it now. */
	if( APCG(check_compiled_source) && !apc_check_compiled_file(file_handle->filename, 
		&inputbuf, &inputlen)) 
	{
		assert(0);
		apc_init_deserializer(inputbuf, inputlen);
		op_array = (zend_op_array*) emalloc(sizeof(zend_op_array));
        if(apc_deserialize_magic()) {
            zend_error(E_ERROR, "%s is not a APC compiled object",
                file_handle->filename);
        }
		apc_deserialize_zend_function_table(CG(function_table),
            acc_functiontable, tables[0]);
        numclasses = apc_deserialize_zend_class_table(CG(class_table), acc_classtable,
            tables[1]);
        apc_deserialize_zend_op_array(op_array, numclasses);
		apc_init_serializer();
		if(apc_cache_insert(cache, key, inputbuf, inputlen, mtime) < 0) {
			/* if we fail to insert, clear the whole cache */
			zend_error(E_WARNING, "APC cache has run out of available space and has auto-reset the cache.  You should consider raising apc.shm_segments");
			apc_cache_clear(cache);
		}
		return op_array;

    }
	
	op_array = old_compile_file(file_handle, type CLS_CC);
	if (!op_array) {
		return NULL;
	}

	if (seen) {
		/* This file has been compiled previously during this request. We
		 * must now remove all its declared functions and classes from the
		 * accumulator tables. This prepares the program state for
		 * serialization. And because the serialization routines will also
		 * insert the file's functions and classes into its private tables,
		 * we can clear those, as well. */

		apc_nametable_difference(acc_functiontable, tables[0]);
		apc_nametable_difference(acc_classtable, tables[1]);
		apc_nametable_clear(tables[0], 0);
		apc_nametable_clear(tables[1], 0);
	}

	/* Before serializing the op tree for this file, make sure it does not
	 * match any of our cache filters. */

	if (apc_regexec(key))
	{
		char* buf;	/* will point to serialization buffer */
		char* opkey;
		int len;	/* will be length of serialization buffer */
		zend_op_array *new_op_array;

		apc_init_serializer();

		/* Serialize the compiler's global tables, using the accumulator
		 * tables to compute the differences created by the compilation
		 * of the file. We also insert every serialized object into the
		 * file's private tables for later use (see above). */
		apc_serialize_magic();
		apc_serialize_zend_function_table(CG(function_table),
			acc_functiontable, tables[0]);
		apc_serialize_zend_class_table(CG(class_table),
			acc_classtable, tables[1]);
		new_op_array = apc_copy_op_array(NULL, op_array);
		opkey = apc_emalloc(strlen(key) + 4);
		snprintf(opkey, strlen(key) + 4, "op:%s", key);
		apc_cache_insert(cache, opkey, (const char *) new_op_array, sizeof(zend_op_array), mtime);
		apc_cache_insert(cache, key, buf, len, mtime);
		apc_efree(opkey);
		memcpy(op_array, new_op_array, sizeof(zend_op_array));
	}
	
	return op_array;
}

/*
 * =========================================================================
 * MMAP specific funktions follow here
 * =========================================================================
 */


int apc_mmap_rm(const char *filename)
{
	int fd;
	char realname[1024];
	const char *name;
	struct mm_fl_element *in_elem;
	
	if(APCG(relative_includes)) {
          if (realpath(filename, realname) == NULL) 
            name = filename;
          else
            name = realname;
        }
        else {
          name = filename;
        }
	if(zend_hash_find(&apc_mm_fl, (char*) name,
    strlen(name)+1, (void**)&in_elem) == SUCCESS)
	{	
		fd = open(in_elem->cache_filename, O_RDONLY);
#ifdef FCNTL_ADVISORY_LOCKING
		writew_lock(fd, 0, SEEK_SET, 0);
#endif
		unlink(in_elem->cache_filename);
#ifdef FCNTL_ADVISORY_LOCKING
		un_lock(fd, 0, SEEK_SET, 0); 
#endif
		close(fd);
			return 0;
	}
	else 
		return -1;
}

/* apc_mmap_compile_file: replacement for zend_compile_file */
ZEND_API zend_op_array* apc_mmap_compile_file(zend_file_handle *file_handle,
	int type CLS_DC)
{
	char* buf;
	char* cache_filename;
	char *filename;
	char* my_cache_filename;
	char* mmapaddr;
	int len;
	int fd;
	int n;
	int seen;
	int numclasses;
	struct stat statbuf, srcstatbuf;
	zend_op_array* op_array;
	struct mm_fl_element *in_elem;
	apc_nametable_t** tables;
	char realname[1024];

	if(APCG(relative_includes)) {
		const char *relative_name;
		relative_name = apc_rstat(file_handle->filename, PG(include_path), 
			&srcstatbuf);
		if(realpath(relative_name, realname) == NULL) {
			return old_compile_file(file_handle, type CLS_CC);
		}
		else {
			filename = realname;
		}
	}
	else {
		filename = file_handle->filename;
	}
	
	if(APCG(check_mtime) && !APCG(relative_includes)) {
		stat(file_handle->filename, &srcstatbuf);
	}
	op_array = (zend_op_array*) emalloc(sizeof(zend_op_array));

	/* if the requested file matches our exclusioin regex, bypass the cache completely */
	if(apc_regexec(filename) == 0)
	{
		op_array = old_compile_file(file_handle, type CLS_CC);
		return op_array;
	}
	cache_filename = apc_generate_cache_filename(filename);

	tables = (apc_nametable_t**) apc_nametable_retrieve(filetable,
        filename);
   
	/* if we haven't processed this file already on this request, set up 
	 * the private  function|class tables now.  Otherwise, mark this file 
	 * as having been 'seen' twice */

    if (tables == 0) {
        tables = (apc_nametable_t**) apc_emalloc(2*sizeof(apc_nametable_t*));
        apc_nametable_insert(filetable, filename, tables);
        tables[0] = apc_nametable_create(PRIV_FUNCTION_TABLE_SIZE);
        tables[1] = apc_nametable_create(PRIV_CLASS_TABLE_SIZE);
        seen = 0;
    }
    else {
        seen = 1;
    }

	/* Look up the requested file in our private hash table */

	if(zend_hash_find(&apc_mm_fl, filename, 
		strlen(filename)+1, (void**)&in_elem) == SUCCESS)
	{
		n = stat(in_elem->cache_filename, &statbuf);
		
		/* if we've specified a ttl, check the age of the cache file and expire it 
		 * if necessary. */
		if((APCG(ttl) && (time(NULL) - statbuf.st_mtime > APCG(ttl))) || 
			(APCG(check_mtime) && srcstatbuf.st_mtime != in_elem->srcmtime)) 
		{
			apc_unlink(in_elem->cache_filename);
			n = -1;	
		}
		if(APCG(check_mtime) || APCG(relative_includes)) {
			in_elem->srcmtime = srcstatbuf.st_mtime;
		}
		if(n == 0)
		{
			if(in_elem->inode == statbuf.st_ino && 
					in_elem->mtime == statbuf.st_mtime ) /* file exists and matches */
			{
				zend_llist_add_element(&CG(open_files), file_handle); /*  FIXME */
				apc_init_deserializer(in_elem->input, in_elem->inputlen);

				/* deserialize the global function/class tables. 
				 * every object that is deserialized is also inserted 
				 * into the file's private tables. Afterwards, we copy 
				 * every element in those private tables into our 
				 * accumulator tables */
				if(apc_deserialize_magic()) {
					zend_error(E_ERROR, "%s is not a APC compiled object",
						in_elem->cache_filename);
				}
				apc_deserialize_zend_function_table(CG(function_table), 
					acc_functiontable, tables[0]);
				numclasses = apc_deserialize_zend_class_table(CG(class_table), 
					acc_classtable, tables[1]);
				apc_deserialize_zend_op_array(op_array, numclasses);

				in_elem->hitcounter++;
				return op_array;
			}
			/* requested file has been removed by another process and 
			 * recreated */
			else
			{
				munmap(in_elem->input, in_elem->inputlen);
				/* We don't need to realloc() because cache_filename 
				 * doesnt change and everything else is statically allocated;
				 */
				in_elem->inputlen = statbuf.st_size;
				in_elem->inode = statbuf.st_ino;
				in_elem->mtime = statbuf.st_mtime;
				in_elem->hitcounter = 1;

				fd = open(in_elem->cache_filename, O_RDONLY);
#ifdef FCNTL_ADVISORY_LOCKING
				readw_lock(fd, 0, SEEK_SET, 0);
#endif
				in_elem->input = (char*) mmap(0, in_elem->inputlen, PROT_READ, 
					MAP_SHARED, fd, 0);

				if(in_elem->input == (caddr_t) -1)
				{
					apc_dprint("Failed to mmap %s\n",in_elem->cache_filename);
					op_array = old_compile_file(file_handle, type CLS_CC);
					return op_array;
				}
				close(fd);
				zend_llist_add_element(&CG(open_files), file_handle); 
				apc_init_deserializer(in_elem->input, in_elem->inputlen);

				/* deserialize the global function/class tables. 
				 * every object that is deserialized is also inserted 
				 * into the file's private tables.  Afterwards, we copy 
				 * every element in those private tables into
				 * our accumulator tables */
				if(apc_deserialize_magic()) {
                    zend_error(E_ERROR, "%s is not a APC compiled object",
                        in_elem->cache_filename);
                }
				apc_deserialize_zend_function_table(CG(function_table), 
					acc_functiontable, tables[0]);
				numclasses = apc_deserialize_zend_class_table(CG(class_table), 
					acc_classtable, tables[1]);
				apc_deserialize_zend_op_array(op_array, numclasses);
				
				/* update our private cache hash with the new objects 
				 * information */
#ifdef FCNTL_ADVISORY_LOCKING
					un_lock(fd, 0, SEEK_SET, 0);
#endif
		      	return op_array;
			}
		}
		/* if the file no longer exists, when purge it from our private cache, and re-write it below */
		else 
		{
			munmap(in_elem->input, in_elem->inputlen);
			zend_hash_del(&apc_mm_fl, filename,
                     strlen(filename)+1);
		}
	}
	printf("Failed to find local cache entry\n");
	if(stat(cache_filename,&statbuf) < 0 || (APCG(check_mtime) && (statbuf.st_mtime < srcstatbuf.st_mtime)))
	{	
		/* handle race condition by serializing to a fixed temporary 
		 * file and relinking it */
		my_cache_filename = (char*)malloc((strlen(cache_filename)+5)*sizeof(char));
		snprintf(my_cache_filename, strlen(cache_filename)+5,"%s.tmp",
			cache_filename);
		fd = apc_ropen(my_cache_filename, O_RDWR | O_EXCL | O_CREAT,
			S_IRUSR | S_IRGRP | S_IROTH);
		if(fd > 0)
		{

				/* if a file is included multiple times, then it is 
				 *possible that it could be removed in the middle of 
				 * a request, after it's initial read.  This would 
				 * cause an inconsistency in the global function|class 
				 * table.  We avoid this by checking here whether or not 
				 * the file was included previously during this request.  
				 * If so, we clear all it's contributed functions
				 * from the accumulator table, so that they will be 
				 * serialized in it's copy of the global function table.  
				 * Got all that? */
				if( APCG(check_compiled_source) && 
					!apc_check_compiled_file(file_handle->filename, &inputbuf, 
					&inputlen)) 
				{
					apc_init_deserializer(inputbuf, inputlen);
					op_array = (zend_op_array*) emalloc(sizeof(zend_op_array));
					if(apc_deserialize_magic()) {
                    	zend_error(E_ERROR, "%s is not a APC compiled object",
                        	cache_filename);
                	}
					apc_deserialize_zend_function_table(CG(function_table),
			            acc_functiontable, tables[0]);
        			numclasses = apc_deserialize_zend_class_table(CG(class_table), acc_classtable,
            			tables[1]);
        			apc_deserialize_zend_op_array(op_array, numclasses);
					write(fd, inputbuf, inputlen);
					close(fd);
					rename(my_cache_filename,cache_filename);
#ifdef FCNTL_ADVISORY_LOCKING
	                un_lock(fd, 0, SEEK_SET, 0);
#endif
    	            free(my_cache_filename);
        	        return op_array;
        		}
    				else {
							if((op_array = old_compile_file(file_handle, type CLS_CC))==NULL) 
							{
								unlink(my_cache_filename);
    	          free(my_cache_filename);
								return op_array	;
							}
						}
				if (seen) 
				{
		        	apc_nametable_difference(acc_functiontable, tables[0]);
        			apc_nametable_difference(acc_classtable, tables[1]);
        			apc_nametable_clear(tables[0], 0);
        			apc_nametable_clear(tables[1], 0);
    			}

				/* compile the file */

				apc_init_serializer();
				apc_serialize_magic();
				apc_serialize_zend_function_table(CG(function_table), acc_functiontable,tables[0]);
				apc_serialize_zend_class_table(CG(class_table), acc_classtable, tables[1]);
    			apc_serialize_zend_op_array(op_array);

				apc_get_serialized_data(&buf, &len);
				write(fd, buf, len);
				close(fd);
				rename(my_cache_filename,cache_filename);

				/* we mmap and add the file to our cache only when we deserialize it the first time,
				 * so here we remove it from our cached-files hash in case we has inserted it 
				 * previously. */
				zend_hash_del(&apc_mm_fl, filename, 
					strlen(filename)+1);
#ifdef FCNTL_ADVISORY_LOCKING
				un_lock(fd, 0, SEEK_SET, 0);
#endif
				free(my_cache_filename);
    			return op_array;
		}
		zend_error(E_NOTICE, "failed to open %s for writing.  Another process may be writing to the file or their may be a permision problem.\n", my_cache_filename);
		free(my_cache_filename);
	}
	if( (fd = open(cache_filename, O_RDONLY)) < 0)
	{
		return old_compile_file(file_handle, type CLS_CC);
	}
  	stat(cache_filename,&statbuf);
		in_elem = (struct mm_fl_element*) malloc(sizeof(struct mm_fl_element));
  	in_elem->inputlen = statbuf.st_size;
  	in_elem->inode = statbuf.st_ino;
  	in_elem->mtime = statbuf.st_mtime;
	if(APCG(check_mtime)) {
		in_elem->srcmtime = srcstatbuf.st_mtime;
	}
	else {
		in_elem->srcmtime = 0;
	}
	in_elem->cache_filename = apc_estrdup(cache_filename);
	in_elem->hitcounter = 1;
	if((mmapaddr = (char *)mmap(0, in_elem->inputlen, PROT_READ,
                               MAP_SHARED, fd, 0)) == (caddr_t)-1 )
	{
		return old_compile_file(file_handle, type CLS_CC);
	}
	close(fd);
	in_elem->input = mmapaddr;
	zend_llist_add_element(&CG(open_files), file_handle); /*  FIXME */
	apc_init_deserializer(in_elem->input, in_elem->inputlen);
	
	/* deserialize the global function/class tables. every object that
	 * is deserialized is also inserted into the file's private tables.
	 * afterwards, we copy every element in those private tables into
	 * our accumulator tables */

	if(apc_deserialize_magic()) {
		zend_error(E_ERROR, "%s is not a APC compiled object",
			cache_filename);
	}
	apc_deserialize_zend_function_table(CG(function_table), 
		acc_functiontable, tables[0]);
	numclasses = apc_deserialize_zend_class_table(CG(class_table), acc_classtable, tables[1]);
	apc_deserialize_zend_op_array(op_array, numclasses);
	zend_hash_update(&apc_mm_fl, filename, 
		strlen(filename)+1, in_elem, 
		sizeof(struct mm_fl_element), NULL);
#ifdef FCNTL_ADVISORY_LOCKING
	un_lock(fd, 0, SEEK_SET, 0);
#endif
	return op_array;
}
