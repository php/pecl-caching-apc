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

#include "apc_version.h"
/* for function declarations */
#include "apc_iface.h"
#include "apc_cache.h"  /* for APC_CACHE_RT enum */

/* This comes from php install tree */
#include "ext/standard/info.h"

/* declarations of functions to be exported */
PHP_FUNCTION(apcinfo);
PHP_FUNCTION(apc_rm);
PHP_FUNCTION(apc_reset_cache);
PHP_FUNCTION(apc_set_my_ttl);
PHP_FUNCTION(apc_dump_cache_object);
PHP_FUNCTION(apc_cache_index);
PHP_FUNCTION(apc_cache_info);
PHP_FUNCTION(apc_object_info);

/* list of exported functions */
static unsigned char a2_arg_force_ref[] = { 2, BYREF_NONE, BYREF_FORCE };

function_entry apc_functions[] = {
	PHP_FE(apcinfo, NULL)
	PHP_FE(apc_rm, NULL)
	PHP_FE(apc_reset_cache, NULL)
	PHP_FE(apc_set_my_ttl, NULL)
	PHP_FE(apc_dump_cache_object, NULL)
	PHP_FE(apc_cache_index, first_arg_force_ref)
	PHP_FE(apc_cache_info, first_arg_force_ref)
	PHP_FE(apc_object_info, a2_arg_force_ref)
	{NULL, NULL, NULL}
};


/* declarations of module functions */
PHP_MINIT_FUNCTION(apc);
PHP_MSHUTDOWN_FUNCTION(apc);
PHP_RINIT_FUNCTION(apc);
PHP_RSHUTDOWN_FUNCTION(apc);
PHP_MINFO_FUNCTION(apc);


/* module entry */
zend_module_entry apc_module_entry = {
	"APC",
	apc_functions,
	PHP_MINIT(apc),
	PHP_MSHUTDOWN(apc),
	PHP_RINIT(apc),
	PHP_RSHUTDOWN(apc),
	PHP_MINFO(apc),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

zend_apc_globals apc_globals;

#if COMPILE_DL_APC
ZEND_GET_MODULE(apc)
#endif


/* initialization file support */

/* set mode of the cache - possible values:
   off
   shm
   mmap
 */
static PHP_INI_MH(set_mode)
{
        if (new_value==NULL) {
          APCG(mode) = OFF_MODE;
        }
        else if (strcasecmp(new_value, "shm") == 0) {
          APCG(mode) = SHM_MODE;
        }
        else if (strcasecmp(new_value, "mmap") == 0) {
          APCG(mode) = MMAP_MODE;
        }
        else {
          // be nice to wrongly configured apc - just switch off
          APCG(mode) = OFF_MODE;
        }
        
        return SUCCESS;
}

/* set the cache-retrieval policy for shared memory cache (shm).
 * has no effect if running mmap implementation. */
static PHP_INI_MH(set_cache_rt)
{
  static const char SAFE[] = "safe";
  static const char FAST[] = "fast";

  if (new_value == NULL || strcmp(new_value, SAFE) == 0) {
    APCG(cache_rt) = APC_CACHE_RT_SAFE;
  }
  else if (strcmp(new_value, FAST) == 0) {
    APCG(cache_rt) = APC_CACHE_RT_FAST;
  }
  else {
    APCG(cache_rt) = APC_CACHE_RT_SAFE; /* default to safe policy */
  }
  return SUCCESS;
}


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
	char *p;
	char *q;
	int i;

	if (new_value == 0) {
		return SUCCESS;
	}
	p = new_value;
	i = 0;
	while(((q = strchr(p, ':')) != NULL) && i < 10)
	{
		*q = '\0';
		if(regcomp(&APCG(regex)[i], p, REG_EXTENDED | REG_ICASE) == 0) {
			APCG(regex_text)[i] = p;
			i++;
		}
		if(*(q+1) != '\0') {
			p = q + 1;
		}
		else {
			return SUCCESS;
		}
	}
	if(regcomp(&APCG(regex)[i], p, REG_EXTENDED | REG_ICASE) == 0) {
		APCG(regex_text)[i] = p;
		i++;
	}
	APCG(nmatches) = i;
	return SUCCESS;
}

/* set the check_mtime flag in apc_globals (used in the shm impl.) */

PHP_INI_BEGIN()
	PHP_INI_ENTRY("apc.mode",        NULL, PHP_INI_ALL, set_mode)
	PHP_INI_ENTRY("apc.cache_rt",    NULL, PHP_INI_ALL, set_cache_rt)
	PHP_INI_ENTRY("apc.ttl",         NULL, PHP_INI_ALL, set_ttl)
	PHP_INI_ENTRY("apc.cachedir",    NULL, PHP_INI_ALL, set_cachedir)
	PHP_INI_ENTRY("apc.regex",       NULL, PHP_INI_ALL, set_regex)

	/* Flag to always check file modification time */
	STD_PHP_INI_ENTRY("apc.check_mtime", "0", PHP_INI_ALL, 
		OnUpdateInt, check_mtime, zend_apc_globals, apc_globals)

	/* file to provide generic support for relative includes */
	STD_PHP_INI_ENTRY("apc.relative_includes", "1", PHP_INI_ALL,     
    OnUpdateInt, relative_includes, zend_apc_globals, apc_globals)
	
	/* Set no. of buckets in the shared cache index. Ignored under mmap. */
	STD_PHP_INI_ENTRY("apc.hash_buckets", "1024", PHP_INI_ALL, 
		OnUpdateInt, hash_buckets, zend_apc_globals, apc_globals)

	/* Set size of shared memory segments. Ignored under mmap. */
	STD_PHP_INI_ENTRY("apc.shm_segment_size", "33554431", PHP_INI_ALL, 
		OnUpdateInt, shm_segment_size, zend_apc_globals, apc_globals)

	/* Set maximum no. of shared memory segments. Ignored under mmap. */
	STD_PHP_INI_ENTRY("apc.shm_segments", "2", PHP_INI_ALL, 
		OnUpdateInt, shm_segments, zend_apc_globals, apc_globals)

	/* Allow for compiled mmap-style files to be used as 'source' files */
	STD_PHP_INI_ENTRY("apc.check_compiled_source", "0", PHP_INI_ALL, 
		OnUpdateInt, check_compiled_source, zend_apc_globals, apc_globals)
PHP_INI_END()

/* printf style interface to zend_error */
static int printlog(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	zend_error(E_WARNING, fmt, args);
	va_end(args);
	return 0;
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
//    php_log_err(log_buffer);
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

/* {{{ proto int apc_cache_index(array &output)
    Generate detailed information about the cache indexed files.
    */
PHP_FUNCTION(apc_cache_index)
{
	zval **hash;
	int ac = ZEND_NUM_ARGS();

	if(ac != 1 || zend_get_parameters_ex(ac, &hash) == FAILURE) {
  	WRONG_PARAM_COUNT;
	}
	if( array_init(*hash) == FAILURE) {
  	zend_error(E_WARNING, "Couldn't convert arg1 to array");
  	RETURN_FALSE;
	}

	if(apc_cache_index(hash)) {
		RETURN_FALSE;
	}
	else {
		RETURN_TRUE;
	}
}	
/* }}} */

/* {{{ proto int apc_cache_index(array &output)
    Generate detailed information about the cache.
    */
PHP_FUNCTION(apc_cache_info)
{
        zval **hash;
        int ac = ZEND_NUM_ARGS();

        if(ac != 1 || zend_get_parameters_ex(ac, &hash) == FAILURE) {
        WRONG_PARAM_COUNT;
        }
        if( array_init(*hash) == FAILURE) {
        zend_error(E_WARNING, "Couldn't convert arg1 to array");
        RETURN_FALSE;
        }

        if(apc_cache_info(hash)) {
                RETURN_FALSE;
        }
        else {
                RETURN_TRUE;
	}
}	
/* }}} */

/* {{{ proto int apc_object_info(string object, array &output)
    Generate detailed information about functions in a object
    */
PHP_FUNCTION(apc_object_info)
{
        char *filename;
        zval **hash;
        zval **filenameParam;
        int ac = ZEND_NUM_ARGS();

        if(ac != 2 || zend_get_parameters_ex(ac, &filenameParam, &hash) == FAILURE) {
          WRONG_PARAM_COUNT;
        }
        convert_to_string_ex(filenameParam);
        filename = (*filenameParam)->value.str.val;
        if( array_init(*hash) == FAILURE) {
          zend_error(E_WARNING, "Couldn't convert arg1 to array");
          RETURN_FALSE;
        }

        if(apc_object_info(filename, hash)) {
                RETURN_FALSE;
        }
        else {
                RETURN_TRUE;
        }
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
	APC_VERSION,
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
	
