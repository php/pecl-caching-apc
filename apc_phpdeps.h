/* 
   +----------------------------------------------------------------------+
   | APC
   +----------------------------------------------------------------------+
   | Copyright (c) 2000-2002 Community Connect Inc.
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   |          George Schlossnagle <george@lethargy.org>                   |
   +----------------------------------------------------------------------+
*/

#ifndef INCLUDED_APC_PHPDEPS
#define INCLUDED_APC_PHPDEPS

#include "zend_extensions.h"

#if ZEND_EXTENSION_API_NO < 20001120
# define ZEND_DO_INHERITANCE do_inheritance
#else
# define ZEND_DO_INHERITANCE zend_do_inheritance
#endif

#if ZEND_EXTENSION_API_NO < 20001224
# define APC_ZEND_GET_INI_ENTRIES zuf->get_ini_entry
# include "php_config.h"
#else
# define APC_ZEND_GET_INI_ENTRIES zuf->get_configuration_directive
#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# include "php_config.h"
#endif
#endif

#if ZEND_EXTENSION_API_NO >= 20010710
# define APC_MUST_DEFINE_START_OP
# define APC_CLS_DC TSRMLS_DC
# define APC_CLS_CC TSRMLS_CC
# define APC_ELS_DC TSRMLS_DC
#else
# define APC_CLS_DC CLS_DC
# define APC_CLS_CC CLS_CC
# define APC_ELS_DC ELS_DC
#endif

#endif

