/* ==================================================================
 * APC Cache
 * Copyright (c) 2000 Community Connect, Inc.
 * All rights reserved.
 * ==================================================================
 * This source code is made available free and without charge subject
 * to the terms of the QPL as detailed in bundled LICENSE file, which
 * is also available at http://apc.communityconnect.com/LICENSE.
 * ==================================================================
 * Daniel Cowgill <dan@mail.communityconnect.com>
 * George Schlossnagle <george@lethargy.org>
 * ==================================================================
*/


#include "php_apc.h"
#include "php_globals.h"
#include "php.h"

/* declarations of functions to be exported */
PHP_FUNCTION(apcinfo);
PHP_FUNCTION(apc_rm);
PHP_FUNCTION(apc_reset_cache);
PHP_FUNCTION(apc_set_my_ttl);

/* list of exported functions */
function_entry apc_functions[] = {
	PHP_FE(apcinfo, NULL)
	PHP_FE(apc_rm, NULL)
	PHP_FE(apc_reset_cache, NULL)
	PHP_FE(apc_set_my_ttl, NULL)
	{NULL, NULL, NULL}
};


/* declarations of module functions */
PHP_MINIT_FUNCTION(apc);
PHP_MSHUTDOWN_FUNCTION(apc);
PHP_RINIT_FUNCTION(apc);
PHP_RSHUTDOWN_FUNCTION(apc);
PHP_MINFO_FUNCTION(apc);
PHP_GINIT_FUNCTION(apc);
PHP_GSHUTDOWN_FUNCTION(apc);


/* module entry */
zend_module_entry apc_module_entry = {
	"APC",
	apc_functions,
	PHP_MINIT(apc),
	PHP_MSHUTDOWN(apc),
	PHP_RINIT(apc),
	PHP_RSHUTDOWN(apc),
	PHP_MINFO(apc),
	PHP_GINIT(apc),
	PHP_GSHUTDOWN(apc),
	STANDARD_MODULE_PROPERTIES_EX
};

zend_apc_globals apc_globals;

#if COMPILE_DL_APC
ZEND_GET_MODULE(apc)
#endif


/* initialization file support */

/* set the global ttl for all cache objects */
static PHP_INI_MH(set_ttl)
{
  if(new_value==NULL)
    APCG(ttl) = 0;
  else
    APCG(ttl) = atoi(new_value);
  return SUCCESS;
}

/* set the directory for compiled files for the mmap implementation.  
 * has no effect if running shm implementation. */
static PHP_INI_MH(set_cachedir)
{
	if(new_value == NULL)
		APCG(cachedir) = NULL;
	else
		APCG(cachedir) = new_value;
	return SUCCESS;
}

/* set a POSIX extended regex to match for NOT serializing objects */
static PHP_INI_MH(set_regex)
{
	if(regcomp(&APCG(regex), new_value, REG_EXTENDED|REG_ICASE) == 0)
	{
		APCG(regex_text) = new_value;
		APCG(nmatches) = 1;
		return SUCCESS;
	}
	else
		return FAILURE;
}
		
PHP_INI_BEGIN()
	PHP_INI_ENTRY("apc.ttl", NULL, PHP_INI_ALL, 
		set_ttl)
	PHP_INI_ENTRY("apc.cachedir", NULL, PHP_INI_ALL,  set_cachedir)
	PHP_INI_ENTRY("apc.regex", NULL, PHP_INI_ALL,  set_regex)

	/* set number of hash_buckets in the master index for shm
	 * implementation.  Ignored under mmap. */
	STD_PHP_INI_ENTRY("apc.hash_buckets", "1024", PHP_INI_ALL, 
		OnUpdateInt, hash_buckets, zend_apc_globals, apc_globals)

	/* set size of shm segments in for shm implementation.  
	 * Ignored under mmap. */
	STD_PHP_INI_ENTRY("apc.shm_segment_size", "33554431", PHP_INI_ALL, 
		OnUpdateInt, shm_segment_size, zend_apc_globals, apc_globals)

  /* set number of shm segments in for shm implementation.  
   * Ignored under mmap. */
	STD_PHP_INI_ENTRY("apc.shm_segments", "10", PHP_INI_ALL, 
		OnUpdateInt, shm_segments, zend_apc_globals, apc_globals)
PHP_INI_END()

/* printf style interface to zend_error */
static int printlog(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	zend_error(E_WARNING, fmt, args);
	va_end(args);
}

static void apc_init_globals(void)
{
	APCG(nmatches) = 0;
}

/* module functions */

/* all apc_ functions here are in apc_iface.c */
PHP_MINIT_FUNCTION(apc)
{
	apc_init_globals();
	REGISTER_INI_ENTRIES();
	apc_module_init();
	apc_setoutputfn(printlog);
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(apc)
{
	UNREGISTER_INI_ENTRIES();
	apc_module_shutdown();
	return SUCCESS;
}

PHP_RINIT_FUNCTION(apc)
{
	apc_request_init();
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(apc)
{
	apc_request_shutdown();
	return SUCCESS;
}

PHP_MINFO_FUNCTION(apc)
{
	php_info_print_table_start();
  php_info_print_table_header(2, "APC Support", "Enabled");
	php_info_print_table_row(2, "APC Version", apc_version());
	php_info_print_table_end();
}

PHP_GINIT_FUNCTION(apc)
{
	apc_global_init();
	return SUCCESS;
}

PHP_GSHUTDOWN_FUNCTION(apc)
{
	apc_global_shutdown();
	return SUCCESS;
}


/* exported function definitions */

/* generates an html page with cache statistics */
PHP_FUNCTION(apcinfo)
{
	apc_module_info();
	RETURN_NULL();
}

/* takes 1 argument (the path to a php file) and removes it's associated
 * entry from the cache, using the implementation-specific method. */
PHP_FUNCTION(apc_rm)
{
	pval **zv;
	char *filename;

	switch(ZEND_NUM_ARGS()) {
		case 1:
			if ( zend_get_parameters_ex(1, &zv) == FAILURE) 
			{
				RETURN_FALSE;
			}
			convert_to_string_ex(zv);
			filename = (*zv)->value.str.val;
			break;
		default:
			WRONG_PARAM_COUNT;
      break;
  }
	if(!apc_remove_cache_object(filename))
	{
		RETURN_FALSE;
	}
	else
	{
		RETURN_TRUE;
	}
}

/* clears all elements from the cache.  Only works on shm implementation */
PHP_FUNCTION(apc_reset_cache)
{
	apc_reset_cache();
	RETURN_TRUE;
}

/* takes an int.  sets the ttl of the calling file to be that length in
 * seconds.  Only supported under shm implementation. */
PHP_FUNCTION(apc_set_my_ttl)
{
	pval **num;
	if(ZEND_NUM_ARGS() !=  1 || zend_get_parameters_ex(1, &num) == FAILURE) 
	{
		WRONG_PARAM_COUNT;
	}
	convert_to_long_ex(num);
	apc_set_object_ttl(zend_get_executed_filename(ELS_C), num);
	RETURN_TRUE;
}

/* zend extension support */

ZEND_DLEXPORT int apc_zend_startup(zend_extension *extension)
{
	return zend_startup_module(&apc_module_entry);
}

ZEND_DLEXPORT void apc_zend_shutdown(zend_extension *extension)
{
}

#ifndef ZEND_EXT_API
#define ZEND_EXT_API	ZEND_DLEXPORT
#endif
ZEND_EXTENSION();

ZEND_DLEXPORT zend_extension zend_extension_entry = {
	"APC Caching",
	"0.1",
	"Dan Cowgill and George Schlossnagle",
	"http://apc.communityconnect.com",
	"Copyright (c) 2000 Community Connect Inc.",
	apc_zend_startup,
	apc_zend_shutdown,
	NULL, /* activate_func_t */
	NULL, /* deactivate_func_t */
	NULL, /* message_handler_func_t */
	NULL, /* op_array_handler_func_t */
	NULL, /* statement_handler_func_t */
	NULL, /* fcall_begin_handler_func_t */
	NULL, /* fcall_end_handler_func_t */
	NULL, /* op_array_ctor_func_t */
	NULL, /* op_array_dtor_func_t */
#ifdef COMPAT_ZEND_EXTENSION_PROPERTIES
	NULL, /* api_no_check */
	COMPAT_ZEND_EXTENSION_PROPERTIES
#else
	STANDARD_ZEND_EXTENSION_PROPERTIES
#endif
};
	
