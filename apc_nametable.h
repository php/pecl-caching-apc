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
#ifndef INCLUDED_APC_NAMETABLE
#define INCLUDED_APC_NAMETABLE

#include "apc_lib.h"

typedef struct apc_nametable_t apc_nametable_t;
typedef void (*apc_nametable_destructor_t)(void*);

/*
 * apc_nametable_create: creates a new name table with the specified
 * number of buckets
 */
apc_nametable_t* apc_nametable_create(int nbuckets);

/*
 * apc_nametable_destroy: frees all memory associated with a name
 * table, includings its keys
 */
extern void apc_nametable_destroy(apc_nametable_t* table);

/*
 * apc_nametable_insert: adds a new key-value mapping to a name table.
 * returns 1 if the key was successfully added, or 0 if the key is a
 * duplicate
 */
extern int apc_nametable_insert(apc_nametable_t* table,
	const char* key, void* value);

/*
 * apc_nametable_search: returns true if the specified key exists in
 * the table
 */
extern int apc_nametable_search(apc_nametable_t* table, const char* key);

/*
 * apc_nametable_retrieve: returns the value associated with the
 * specified key, or null if the key does not exist
 */
extern void* apc_nametable_retrieve(apc_nametable_t* table, const char* key);

/*
 * apc_nametable_remove: removes the specified key from the table.
 * returns true if the key existed and was removed, 0 if it did not
 */
extern int apc_nametable_remove(apc_nametable_t* table, const char* key);

/*
 * apc_nametable_clear: removes all keys from the table and frees
 * their associated memory. optionally provide a destructor function
 * that will be called for every value in the table
 */
extern void apc_nametable_clear(apc_nametable_t* table,
	apc_nametable_destructor_t destructor);

/*
 * apc_nametable_union: inserts all elements in table b into table
 * a, if and only if they do not already exist in table a
 */
extern void apc_nametable_union(apc_nametable_t* a, apc_nametable_t* b);

/*
 * apc_nametable_difference: removes all elements in table b from
 * table a
 */
extern void apc_nametable_difference(apc_nametable_t* a, apc_nametable_t* b);

/*
 * apc_nametable_size: returns number of elements in table
 */
extern int apc_nametable_size(apc_nametable_t* table);

/*
 * apc_nametable_dump: debugging display function
 */
extern void apc_nametable_dump(apc_nametable_t* table,
	apc_outputfn_t outputfn);

#endif
