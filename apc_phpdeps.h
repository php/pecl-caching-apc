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

#endif

