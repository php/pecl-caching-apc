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


#ifndef APC_SERIALIZE_H
#define APC_SERIALIZE_H

#include "apc_nametable.h"
#include "zend.h"
#include "zend_compile.h"
#include "zend_llist.h"
#include "zend_hash.h"
#include <stdio.h>

extern void apc_init_serializer();
extern void apc_init_deserializer(char* input, int size);
extern void apc_serialize_debug(FILE* out);
extern void apc_get_serialized_data(char** bufptr, int* length);

/* ... */
extern void apc_serialize_debug(FILE* out);
extern void apc_save(const char* filename);
extern int apc_load(const char* filename);

/* general */
extern void apc_serialize_string(char* string);
extern void apc_create_string(char** string);
extern void apc_serialize_arg_types(zend_uchar* arg_types);
extern void apc_create_arg_types(zend_uchar** arg_types);

/* zend_llist.h */
extern void apc_serialize_zend_llist(zend_llist* list);
extern void apc_deserialize_zend_llist(zend_llist* list);
extern void apc_create_zend_llist(zend_llist** list);

/* zend_hash.h */
extern void apc_serialize_hashtable(HashTable* ht, void* funcptr);
extern void apc_deserialize_hashtable(HashTable* ht, void* funcptr, int datasize);
extern void apc_create_hashtable(HashTable** ht, void* funcptr, int datasize);

/* zend.h */
extern void apc_serialize_zvalue_value(zvalue_value* zv, int type);
extern void apc_deserialize_zvalue_value(zvalue_value* zv, int type);
extern void apc_serialize_zval(zval* zv);
extern void apc_deserialize_zval(zval* zv);
extern void apc_create_zval(zval** zv);
extern void apc_serialize_zend_function_entry(zend_function_entry* zfe);
extern void apc_deserialize_zend_function_entry(zend_function_entry* zfe);
extern void apc_serialize_zend_property_reference(zend_property_reference* zpr);
extern void apc_deserialize_zend_property_reference(zend_property_reference* zpr);
extern void apc_serialize_zend_overloaded_element(zend_overloaded_element* zoe);
extern void apc_deserialize_zend_overloaded_element(zend_overloaded_element* zoe);
extern void apc_serialize_zend_class_entry(zend_class_entry* zce);
extern void apc_deserialize_zend_class_entry(zend_class_entry* zce);
extern void apc_create_zend_class_entry(zend_class_entry** zce);
extern void apc_serialize_zend_utility_functions(zend_utility_functions* zuf);
extern void apc_deserialize_zend_utility_functions(zend_utility_functions* zuf);
extern void apc_serialize_zend_utility_values(zend_utility_values* zuv);
extern void apc_deserialize_zend_utility_values(zend_utility_values* zuv);

/* zend_compile.h */
extern void apc_serialize_znode(znode* zn);
extern void apc_deserialize_znode(znode* zn);
extern void apc_serialize_zend_op(zend_op* zo);
extern void apc_deserialize_zend_op(zend_op* zo);
extern void apc_serialize_zend_op_array(zend_op_array* zoa);
extern void apc_deserialize_zend_op_array(zend_op_array* zoa);
extern void apc_serialize_zend_internal_function(zend_internal_function* zif);
extern void apc_deserialize_zend_internal_function(zend_internal_function* zif);
extern void apc_serialize_zend_overloaded_function(zend_overloaded_function* zof);
extern void apc_deserialize_zend_overloaded_function(zend_overloaded_function* zof);
extern void apc_serialize_zend_function(zend_function* zf);
extern void apc_deserialize_zend_function(zend_function* zf);
extern void apc_create_zend_function(zend_function** zf);

/* special purpose */
extern void apc_serialize_zend_function_table(HashTable* gft, apc_nametable_t* acc);
extern void apc_deserialize_zend_function_table(HashTable* gft, apc_nametable_t* acc);
extern void apc_serialize_zend_class_table(HashTable* gct, apc_nametable_t* acc);
extern void apc_deserialize_zend_class_table(HashTable* gct, apc_nametable_t* acc);

#endif
