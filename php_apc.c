#include "php_apc.h"
#include "php.h"

/* declarations of functions to be exported */
PHP_FUNCTION(apcinfo);
PHP_FUNCTION(apc_rm);

/* list of exported functions */
function_entry apc_functions[] = {
	PHP_FE(apcinfo, NULL)
	PHP_FE(apc_rm, NULL)
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

static PHP_INI_MH(set_ttl)
{
  if(new_value==NULL)
    APCG(ttl) = 0;
  else
    APCG(ttl) = atoi(new_value);
  return SUCCESS;
}

static PHP_INI_MH(set_cachedir)
{
	if(new_value == NULL)
		APCG(cachedir) = NULL;
	else
		APCG(cachedir) = new_value;
	return SUCCESS;
}

static PHP_INI_MH(set_regex)
{
	{
		if(regcomp(&APCG(regex), new_value, REG_EXTENDED|REG_ICASE) == 0)
		{
			APCG(nmatches) = 1;
			return SUCCESS;
		}
		else
			return FAILURE;
	}
}
		
PHP_INI_BEGIN()
	PHP_INI_ENTRY("apc.ttl", NULL, PHP_INI_ALL, 
		set_ttl)
	PHP_INI_ENTRY("apc.cachedir", NULL, PHP_INI_ALL,  set_cachedir)
	PHP_INI_ENTRY("apc.regex", NULL, PHP_INI_ALL,  set_regex)
	STD_PHP_INI_ENTRY("apc.hash_buckets", "1024", PHP_INI_ALL, OnUpdateInt, hash_buckets, zend_apc_globals, apc_globals)
	STD_PHP_INI_ENTRY("apc.shm_segment_size", "33554431", PHP_INI_ALL, OnUpdateInt, shm_segment_size, zend_apc_globals, apc_globals)
	STD_PHP_INI_ENTRY("apc.shm_segments", "10", PHP_INI_ALL, OnUpdateInt, shm_segments, zend_apc_globals, apc_globals)
PHP_INI_END()


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

PHP_MINIT_FUNCTION(apc)
{
	apc_init_globals();
	REGISTER_INI_ENTRIES();
	apc_module_init();
	apc_seterrorfn(printlog);
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
    php_info_print_table_header(2, "Column 1", "Column 2");
	php_info_print_table_row(2, "1x1", "1x2");
	php_info_print_table_row(2, "2x1", "2x2");
	php_info_print_table_row(2, "3x1", "3x2");
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

PHP_FUNCTION(apcinfo)
{
	apc_module_info();
	RETURN_NULL();
}

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
	if(!apc_rm_cache_object(filename))
	{
		RETURN_FALSE;
	}
	else
	{
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
	"0.1",
	"Dan Cowgill and George Schlossnagle",
	NULL,
	"Copyright (c) 2000 Community Connect Inc.",
	apc_zend_startup,
	apc_zend_shutdown,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	STANDARD_ZEND_EXTENSION_PROPERTIES
};
	
