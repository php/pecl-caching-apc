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

#ifndef PHP_COURIERAUTH_H
#define PHP_COURIERAUTH_H

#define PHP_COURIERAUTH_VERSION "0.1.0"

extern zend_module_entry courierauth_module_entry;
#define phpext_courierauth_ptr &courierauth_module_entry

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINFO_FUNCTION(courierauth);

PHP_FUNCTION(courierauth_login);
PHP_FUNCTION(courierauth_getuserinfo);
PHP_FUNCTION(courierauth_enumerate);
PHP_FUNCTION(courierauth_passwd);
PHP_FUNCTION(courierauth_getoption);

#endif	/* PHP_COURIERAUTH_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
