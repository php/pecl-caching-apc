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


#ifndef PHP_APC_H
#define PHP_APC_H

#include "php.h"
#include "php_ini.h"

#include "zend.h"
#include "zend_API.h"
#include "zend_compile.h"
#include "zend_extensions.h"

#include <regex.h>

ZEND_BEGIN_MODULE_GLOBALS(apc)
	int	ttl;
	char *cachedir;
	char *regex_text;
	regex_t regex;
	int nmatches;
	int hash_buckets;
	int shm_segments;
	int shm_segment_size;
ZEND_END_MODULE_GLOBALS(apc)

#define APCG(v) (apc_globals.v)
#endif
