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
PHP_FUNCTION(apc_dump_cache_object);

/* list of exported functions */
function_entry apc_functions[] = {
	PHP_FE(apcinfo, NULL)
	PHP_FE(apc_rm, NULL)
	PHP_FE(apc_reset_cache, NULL)
	PHP_FE(apc_set_my_ttl, NULL)
	PHP_FE(apc_dump_cache_object, NULL)
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
	if (new_value==NULL) {
		APCG(ttl) = 0;
	}
	else {
		APCG(ttl) = atoi(new_value);
	}
	return SUCCESS;
}

/* set the directory for compiled files for the mmap implementation.  
 * has no effect if running shm implementation. */
static PHP_INI_MH(set_cachedir)
{
	if (new_value == NULL) {
		APCG(cachedir) = NULL;
	}
	else {
		APCG(cachedir) = new_value;
	}
	return SUCCESS;
}

/* set a POSIX extended regex to match for NOT serializing objects */
static PHP_INI_MH(set_regex)
{
	if (new_value == 0) {
		return SUCCESS;
	}

	if (regcomp(&APCG(regex), new_value, REG_EXTENDED | REG_ICASE) == 0) {
		APCG(regex_text) = new_value;
		APCG(nmatches) = 1;
		return SUCCESS;
	}

	return FAILURE;
}

/* set the check_mtime flag in apc_globals (used in the shm impl.) */
static PHP_INI_MH(set_check_mtime)
{
	if (new_value == NULL) {
		APCG(check_mtime) = 0;
	}
	else {
		APCG(check_mtime) = atoi(new_value);
	}
	return SUCCESS;
}

PHP_INI_BEGIN()
	PHP_INI_ENTRY("apc.ttl",         NULL, PHP_INI_ALL, set_ttl)
	PHP_INI_ENTRY("apc.cachedir",    NULL, PHP_INI_ALL, set_cachedir)
	PHP_INI_ENTRY("apc.regex",       NULL, PHP_INI_ALL, set_regex)
	PHP_INI_ENTRY("apc.check_mtime", NULL, PHP_INI_ALL, set_check_mtime)

	/* Set no. of buckets in the shared cache index. Ignored under mmap. */
	STD_PHP_INI_ENTRY("apc.hash_buckets", "1024", PHP_INI_ALL, 
		OnUpdateInt, hash_buckets, zend_apc_globals, apc_globals)

	/* Set size of shared memory segments. Ignored under mmap. */
	STD_PHP_INI_ENTRY("apc.shm_segment_size", "33554431", PHP_INI_ALL, 
		OnUpdateInt, shm_segment_size, zend_apc_globals, apc_globals)

	/* Set maximum no. of shared memory segments. Ignored under mmap. */
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
	char log_buffer[1024];
	apc_init_globals();
	REGISTER_INI_ENTRIES();
	apc_module_init();
    snprintf(log_buffer, 1024, "PHP: Startup: %s", apc_version());
    php_log_err(log_buffer);
	apc_setoutputfn(printlog);
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(apc)
{
	/* This is a hack, necessary since apache registers modules
	 * twice during startup */
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
/* {{{ proto int apcinfo([string uri])
	Generate detailed information about the cache.  If uri is passed, link
	all objects to uri, for detailed object information and deleteion
	tags. */
PHP_FUNCTION(apcinfo)
{
	zval** param;

	switch(ZEND_NUM_ARGS()) {
	  case 0:
		apc_module_info(0);
		break;
	  case 1:
		if (zend_get_parameters_ex(1, &param) == FAILURE) {
			RETURN_FALSE;
		}
		convert_to_string_ex(param);
		apc_module_info((*param)->value.str.val);
		break;
	  default:
		WRONG_PARAM_COUNT;
		break;
	}

	RETURN_NULL();
}
/* }}} */

/* {{{ proto int apc_rm(string /path/to/object) 
	Removes the specified object from the cache */
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
	if(!apc_rm(filename))
	{
		RETURN_FALSE;
	}
	else
	{
		RETURN_TRUE;
	}
}
/* }}} */

/* {{{ proto int apc_reset_cache()
	Removes all cache entries.  Only works under shm implementation */
PHP_FUNCTION(apc_reset_cache)
{
	apc_reset_cache();
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int apc_set_my_ttl(int ttl)
	Sets the ttl of the calling page to ttl seconds.  Only works under shm
	implemntation. */
PHP_FUNCTION(apc_set_my_ttl)
{
	pval **param;

	if(ZEND_NUM_ARGS() !=  1 || zend_get_parameters_ex(1, &param) == FAILURE) 
	{
		WRONG_PARAM_COUNT;
	}
	convert_to_long_ex(param);
	apc_set_object_ttl(zend_get_executed_filename(ELS_C), (*param)->value.lval);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int apc_dump_cache_object(string /path/to/object)
	Dumps the specified objects op_tree and its serialized function/class tables. 
*/
PHP_FUNCTION(apc_dump_cache_object)
{
	zval** param;

	if(ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &param) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(param);

	if (apc_dump_cache_object((*param)->value.str.val, zend_printf) < 0) {
		zend_printf("<b>error:</b> entry '%s' not found<br>\n",
			(*param)->value.str.val);
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

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
	"Copyright (c) 2000-2001 Community Connect Inc.",
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
	
