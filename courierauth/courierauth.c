/*
    +--------------------------------------------------------------------+
    | PECL :: courierauth                                                |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2006, Michael Wallner <mike@php.net>                 |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_courierauth.h"

#include <courierauth.h>

/* {{{ courierauth_functions[] */
zend_function_entry courierauth_functions[] = {
	PHP_FE(courierauth_login, NULL)
	PHP_FE(courierauth_enumerate, NULL)
	PHP_FE(courierauth_getuserinfo, NULL)
	PHP_FE(courierauth_passwd, NULL)
	PHP_FE(courierauth_getoption, NULL)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ courierauth_module_entry
 */
zend_module_entry courierauth_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"courierauth",
	courierauth_functions,
	NULL,
	NULL,
	NULL,
	NULL,
	PHP_MINFO(courierauth),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_COURIERAUTH_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_COURIERAUTH
ZEND_GET_MODULE(courierauth)
#endif

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(courierauth)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "courierauth support", "enabled");
	php_info_print_table_row(2, "courierauth version", courierauth_h_rcsid);
	php_info_print_table_end();
}
/* }}} */

typedef struct {
	zval *rv;
#ifdef ZTS
	void ***tsrm_ls;
#endif
	uint success:1;
	uint _res:31;
} php_courierauth_data;

#ifdef ZTS
#define php_courierauth_data_init {return_value, tsrm_ls, 0, 0}
#else
#define TSRMLS_SET_CTX(c)
#define TSRMLS_FETCH_FROM_CTX(c)
#define php_courierauth_data_init {return_value, 0, 0}
#endif

static int php_courierauth_callback(struct authinfo *ai, void *arg)
{
	php_courierauth_data *pa = (php_courierauth_data *) arg;
	TSRMLS_FETCH_FROM_CTX(pa->tsrm_ls);
	
	convert_to_object(pa->rv);
	
	if (ai->sysusername) {
		add_property_string(pa->rv, "sysusername", (char *) ai->sysusername, 1);
		add_property_null(pa->rv, "sysuserid");
		add_property_null(pa->rv, "sysgroupid");
	} else if (ai->sysuserid) {
		add_property_null(pa->rv, "sysusername");
		add_property_long(pa->rv, "sysuserid", *ai->sysuserid);
		add_property_long(pa->rv, "sysgroupid", ai->sysgroupid);
	} else {
		add_property_null(pa->rv, "sysusername");
		add_property_null(pa->rv, "sysuserid");
		add_property_null(pa->rv, "sysgroupid");
	}
#define ADD_STRING(s) \
	if (ai->s) { \
		add_property_string(pa->rv, #s, (char *) ai->s, 1); \
	} else { \
		add_property_null(pa->rv, #s); \
	}
	ADD_STRING(homedir);
	ADD_STRING(address);
	ADD_STRING(fullname);
	ADD_STRING(maildir);
	ADD_STRING(quota);
	ADD_STRING(passwd);
#if PHP_COURIERAUTH_SECURITY_RISK
	ADD_STRING(clearpasswd);
#else
	add_property_null(pa->rv, "clearpasswd");
#endif
	ADD_STRING(options);
	
	return 0;
}

static void php_courierauth_enumeration_callback(const char *sysusername, uid_t sysuserid, gid_t sysgroupid, const char *homedir, const char *maildir, const char *options, void *arg)
{
	zval *entry, *array;
	struct authinfo ai = {NULL};
	php_courierauth_data *pa = (php_courierauth_data *) arg;
	TSRMLS_FETCH_FROM_CTX(pa->tsrm_ls);
	
	if (!sysusername && !sysuserid && !homedir && !maildir && !options) {
		pa->success = 1;
	} else {
		array = pa->rv;
		convert_to_array(array);
		MAKE_STD_ZVAL(entry);
		ZVAL_NULL(entry);
		pa->rv = entry;
		
		ai.sysusername = sysusername;
		ai.sysuserid = &sysuserid;
		ai.homedir = homedir;
		ai.maildir = maildir;
		ai.options = options;
		
		php_courierauth_callback(&ai, (void *) pa);
		
		add_next_index_zval(array, entry);
		pa->rv = array;
	}
}

/* {{{ proto object courierauth_login(string service, string user, string pass)
   Login and return account info on success */
PHP_FUNCTION(courierauth_login)
{
	char *svc_str, *user_str, *pass_str;
	int svc_len, user_len, pass_len;
	php_courierauth_data ca = php_courierauth_data_init;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &svc_str, &svc_len, &user_str, &user_len, &pass_str, &pass_len)) {
		RETURN_FALSE;
	}
	
	if (0 != auth_login(svc_str, user_str, pass_str, php_courierauth_callback, &ca)) {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto object courierauth_getuserinfo(string service, string user)
	Get account info for a user */
PHP_FUNCTION(courierauth_getuserinfo)
{
	char *svc_str, *user_str;
	int svc_len, user_len;
	php_courierauth_data ca = php_courierauth_data_init;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &svc_str, &svc_len, &user_str, &user_len)) {
		RETURN_FALSE;
	}
	
	if (0 != auth_getuserinfo(svc_str, user_str, php_courierauth_callback, &ca)) {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array courierauth_enumerate(void)
	List all users */
PHP_FUNCTION(courierauth_enumerate)
{
	php_courierauth_data ca = php_courierauth_data_init;
	
	zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "");
	
	auth_enumerate(php_courierauth_enumeration_callback, &ca);
	if (!ca.success) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "auth_enumerate() aborted or unavailable");
	}
}
/* }}} */

/* {{{ proto bool courierauth_passwd(string service, string user, string old_pass, string new_pass)
	Change the password of a user */
PHP_FUNCTION(courierauth_passwd)
{
	char *svc_str, *user_str, *oldpw_str, *newpw_str;
	int svc_len, user_len, oldpw_len, newpw_len;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssss", &svc_str, &svc_len, &user_str, &user_len, &oldpw_str, &oldpw_len, &newpw_str, &newpw_len)) {
		RETURN_FALSE;
	}
	
	RETURN_BOOL(0 == auth_passwd(svc_str, user_str, oldpw_str, newpw_str));
}

/* {{{ proto string courierauth_getoption(string options, string key)
	Get the value of a key from options string */
PHP_FUNCTION(courierauth_getoption)
{
	char *opt_str, *key_str, *val_str;
	int opt_len, key_len;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &opt_str, &opt_len, &key_str, &key_len)) {
		RETURN_FALSE;
	}
	
	if ((val_str = auth_getoption(opt_str, key_str))) {
		RETVAL_STRING(val_str, 1);
		free(val_str);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
