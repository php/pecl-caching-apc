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


#ifndef APC_IFACE_H
#define APC_IFACE_H

#include "apc_lib.h"

/*
 * apc_setoutputfn: specify which function should be used for
 * output in the apc module
 */
extern void apc_setoutputfn(apc_outputfn_t fn);

/*
 * apc_module_init, apc_module_shutdown: module initialization and
 * shutdown functions, called once per process
 */
extern void apc_module_init(void);
extern void apc_module_shutdown(void);

/*
 * apc_request_init, apc_request_shutdown: request initialization
 * and shutdown functions, called once per request
 */
extern void apc_request_init(void);
extern void apc_request_shutdown(void);

/*
 * apc_module_info: display information about the apc module to
 * the page
 */
extern void apc_module_info(void);

/*
 * apc_version: returns version string
 */
extern const char* apc_version(void);

/*
 * apc_global_init, apc_global_shutdown: global initialization and
 * shutdown functions
 */
extern void apc_global_init(void);
extern void apc_global_shutdown(void);

/*
 * apc_rm: removes a cache entry by name. returns true if the entry
 * was successfully removed, else false
 */
extern int apc_rm(const char* name);

/*
 * apc_reset_cache: removes all entries from the shared cache
 */
extern void apc_reset_cache(void);

/*
 * apc_set_object_ttl: sets the time-to-live for a specified cache
 * object.
 */
extern void apc_set_object_ttl(const char* name, int ttl);

#endif
