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

typedef struct apc_nametable_t apc_nametable_t;

/*
 * apc_nametable_create: creates a new name table with the specified
 * number of buckets
 */
apc_nametable_t* apc_nametable_create(int nbuckets);

/*
 * apc_nametable_destroy: frees all memory associated with a name
 * table, includings its keys
 */
void apc_nametable_destroy(apc_nametable_t* table);

/*
 * apc_nametable_insert: adds a new key to a name table. returns 1 if
 * the key was successfully added, or 0 if the key is a duplicate
 */
int apc_nametable_insert(apc_nametable_t* table, const char* key);

/*
 * apc_nametable_search: returns true if the specified key exists in
 * the table
 */
int apc_nametable_search(apc_nametable_t* table, const char* key);

/*
 * apc_nametable_remove: removes the specified key from the table.
 * returns true if the key existed and was removed, 0 if it did not
 */
int apc_nametable_remove(apc_nametable_t* table, const char* key);

/*
 * apc_nametable_clear: removes all keys from the table and frees
 * their associated memory
 */
void apc_nametable_clear(apc_nametable_t* table);

#endif
