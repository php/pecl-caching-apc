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


#include "apc_serialize.h"
#include "apc_phpdeps.h"
#include <stdlib.h>
#include <assert.h>

#include "zend_variables.h"	// for zval_dtor()

/* namearray_t is used to record deferred inheritance relationships */
typedef struct namearray_t namearray_t;
struct namearray_t {
	char* strings;	/* array of null-terminated names */
	int length;		/* logical size of strings */
	int size;		/* physical size of strings */
};

/* namearray_dtor: used to clean up deferred_inheritance table (see below) */
static void namearray_dtor(void* p)
{
	namearray_t* na = (namearray_t*) p;

	if (na->strings != 0) {
		free(na->strings);
	}
	free(na);
}

/* deferred_inheritance is a mapping from parent class names to
 * namearray_t structures that contain the names of all classes
 * derived from the parent class that were compiled before the
 * parent class definition was encountered... */ 
static apc_nametable_t* deferred_inheritance = 0;

/* inherit: recursively inherit methods from base to all the children
 * of parent, the children of parent's children, and so on */
static void inherit(zend_class_entry* base, zend_class_entry* parent)
{
	namearray_t* children;	/* classes derived directly from parent */
	char* child_name;		/* name of the ith child of parent */
	int i;
	
	children = apc_nametable_retrieve(deferred_inheritance, parent->name);

	if (!children) {
		return;		/* 'parent' has no children, nothing to do */
	}

	/* For each child of parent, resolve the inheritance from base
	 * to the child. */
	
	for (i = 0; i < children->length; i += strlen(child_name) + 1) {
		zend_class_entry* child;
		int result;
		
		child_name = children->strings + i;

		/* look up the child class entry */
		result = zend_hash_find(CG(class_table), child_name,
			strlen(child_name) + 1, (void**) &child);
		assert(result == SUCCESS);	/* FIXME: is this correct? */

		/* child inherits from base */
		ZEND_DO_INHERITANCE(child, base);

		/* all children of child inherit from base */
		inherit(base, child);
	}
}

enum { START_SIZE = 1, GROW_FACTOR = 2 };

static char* dst   = 0;		/* destination (serialization) buffer */
static int dstpos  = 0;		/* position in destination buffer */
static int dstsize = 0;		/* physical size of destination buffer */

static char* src   = 0;		/* source (deserialization) buffer */
static int srcpos  = 0;		/* position in source buffer */
static int srcsize = 0;		/* physical size of source buffer */

/* expandbuf: resize buffer to be at least minsize bytes in length */
static void expandbuf(char** bufptr, int* cursize, int minsize)
{
	while (*cursize < minsize) {
		*cursize *= GROW_FACTOR;
	}
	*bufptr = (char*) realloc(*bufptr, *cursize);
}

/* apc_serializer_request_init: initialize this module per request */
void apc_serializer_request_init()
{
	deferred_inheritance = apc_nametable_create(97);
}

/* apc_serializer_request_shutdown: clean up this module per request */
void apc_serializer_request_shutdown()
{
	if (deferred_inheritance != 0) {
		apc_nametable_clear(deferred_inheritance, namearray_dtor);
		apc_nametable_destroy(deferred_inheritance);
		deferred_inheritance = 0;
	}
}

/* First step in serialization.  Creates the serialization buffer */
void apc_init_serializer()
{
	if (dst == 0) {
		dstsize = START_SIZE;
		dst = (char*) malloc(dstsize);
	}
	dstpos = 0;
}

/* First step in deserialization.  Sets the size/position of the 
 * serialized buffer */
void apc_init_deserializer(char* input, int size)
{
	src     = input;
	srcpos  = 0;
	srcsize = size;
}

/* zend_serialize_debug: prints information about the source and
 * destination serialization buffers */
void apc_serialize_debug(FILE* out)
{
	fprintf(out, "src=%p, srcpos=%d, srcsize=%d\n", src, srcpos, srcsize);
	fprintf(out, "dst=%p, dstpos=%d, dstsize=%d\n", dst, dstpos, dstsize);
}

/* Sets the pointer to and length of buffer to deserialize to */
void apc_get_serialized_data(char** bufptr, int* length)
{
	*bufptr = dst;
	*length = dstpos;
}

/* dumps serialization to a file.  This is for debug only and is not how
 * the mmap implementation works.  See mmap/apc_iface.c for those details */
void apc_save(const char* filename)
{
	FILE* out;
	char buf[sizeof(int)];

	apc_serialize_debug(stdout);

	if (!(out = fopen(filename, "w"))) {
		fprintf(stderr, "could not open %s for writing\n", filename);
		exit(2);
	}

	*((int*)buf) = dstpos;
	fwrite(buf, sizeof(int), 1, out);

	if (fwrite(dst, sizeof(char), dstpos, out) != dstpos*sizeof(char)) {
		fprintf(stderr, "error writing to %s\n", filename);
		exit(2);
	}

	fclose(out);
}

/* Loads in a serialized data file as created by apc_save().  Again this
 * is for debugging purposes and is not used by the mmap implemntation. 
 * See mmap/apc_iface.c for those details. */
int apc_load(const char* filename)
{
	FILE* in;
	char buf[sizeof(int)];

	if (!(in = fopen(filename, "r"))) {
		fprintf(stderr, "could not open %s for reading\n", filename);
		return 0;
	}

	fread(buf, sizeof(int), 1, in);
	srcpos = *((int*)buf);

	if (src != 0) {
		free(src);
	}
	srcsize = srcpos;
	src     = (char*) malloc(srcsize);
	srcpos  = 0;

	if (fread(src, sizeof(char), srcsize, in) != srcsize*sizeof(char)) {
		fprintf(stderr, "error reading from %s\n", filename);
		exit(2);
	}

	fclose(in);
	return 1;
}

/* By convention all apc_serialize_* functions serialize objects of the
 * specified type to the serialization buffer (dst). The apc_deserialize_*
 * functions deserialize objects of the specified type from the
 * deserialization buffer (src). The apc_create_* functions allocate
 * objects of the specified type, then call the appropriate deserialization
 * function. */

/* general */
void apc_serialize_string(char* string);
void apc_create_string(char** string);
void apc_serialize_arg_types(zend_uchar* arg_types);
void apc_create_arg_types(zend_uchar** arg_types);

/* routines for handling structures from zend_llist.h */
void apc_serialize_zend_llist(zend_llist* list);
void apc_deserialize_zend_llist(zend_llist* list);
void apc_create_zend_llist(zend_llist** list);

/* routines for handling structures from zend_hash.h */
void apc_serialize_hashtable(HashTable* ht, void* funcptr);
void apc_deserialize_hashtable(HashTable* ht, void* funcptr, int datasize);
void apc_create_hashtable(HashTable** ht, void* funcptr, int datasize);

/* routines for handling structures from zend.h */
void apc_serialize_zvalue_value(zvalue_value* zvv, int type);
void apc_deserialize_zvalue_value(zvalue_value* zvv, int type);
void apc_serialize_zval_ptr(zval** zv);
void apc_serialize_zval(zval* zv);
void apc_deserialize_zval(zval* zv);
void apc_create_zval(zval** zv);
void apc_serialize_zend_function_entry(zend_function_entry* zfe);
void apc_deserialize_zend_function_entry(zend_function_entry* zfe);
void apc_serialize_zend_property_reference(zend_property_reference* zpr);
void apc_deserialize_zend_property_reference(zend_property_reference* zpr);
void apc_serialize_zend_overloaded_element(zend_overloaded_element* zoe);
void apc_deserialize_zend_overloaded_element(zend_overloaded_element* zoe);
void apc_serialize_zend_class_entry(zend_class_entry* zce);
void apc_deserialize_zend_class_entry(zend_class_entry* zce);
void apc_create_zend_class_entry(zend_class_entry** zce);
void apc_serialize_zend_utility_functions(zend_utility_functions* zuf);
void apc_deserialize_zend_utility_functions(zend_utility_functions* zuf);
void apc_serialize_zend_utility_values(zend_utility_values* zuv);
void apc_deserialize_zend_utility_values(zend_utility_values* zuv);

/* routines for handling structures from zend_compile.h */
void apc_serialize_znode(znode* zn);
void apc_deserialize_znode(znode* zn);
void apc_serialize_zend_op(zend_op* zo);
void apc_deserialize_zend_op(zend_op* zo);
void apc_serialize_zend_op_array(zend_op_array* zoa);
void apc_deserialize_zend_op_array(zend_op_array* zoa);
void apc_create_zend_op_array(zend_op_array** zoa);
void apc_serialize_zend_internal_function(zend_internal_function* zif);
void apc_deserialize_zend_internal_function(zend_internal_function* zif);
void apc_serialize_zend_overloaded_function(zend_overloaded_function* zof);
void apc_deserialize_zend_overloaded_function(zend_overloaded_function* zof);
void apc_serialize_zend_function(zend_function* zf);
void apc_deserialize_zend_function(zend_function* zf);
void apc_create_zend_function(zend_function** zf);

/* special purpose */
void apc_serialize_zend_function_table(HashTable* gft, apc_nametable_t* acc, apc_nametable_t*);
void apc_deserialize_zend_function_table(HashTable* gft, apc_nametable_t* acc, apc_nametable_t*);
void apc_serialize_zend_class_table(HashTable* gct, apc_nametable_t* acc, apc_nametable_t*);
void apc_deserialize_zend_class_table(HashTable* gct, apc_nametable_t* acc, apc_nametable_t*);


/* type: Fundamental operations */

/* SERIALIZE_SCALAR: write a scalar value to dst */
#define SERIALIZE_SCALAR(x, type) {							\
	if (dstsize - dstpos < sizeof(type)) {					\
		expandbuf(&dst, &dstsize, sizeof(type) + dstpos);	\
	}														\
	*((type*)(dst + dstpos)) = x;							\
	dstpos += sizeof(type);									\
}

/* DESERIALIZE_SCALAR: read a scalar value from src */
#define DESERIALIZE_SCALAR(xp, type) {						\
	assert(srcsize - srcpos >= sizeof(type));				\
	*(xp) = *((type*)(src + srcpos));						\
	srcpos += sizeof(type);									\
}

/* PEEK_SCALAR: read a scalar value from src without advancing read pos */
#define PEEK_SCALAR(xp, type) {								\
	assert(srcsize - srcpos >= sizeof(type));				\
	*(xp) = *((type*)(src + srcpos));						\
}

/* STORE_BYTES: memcpy wrapper, writes to dst buffer */
#define STORE_BYTES(bytes, n) {								\
	if (dstsize - dstpos < n) {								\
		expandbuf(&dst, &dstsize, n + dstpos);				\
	}														\
	memcpy(dst + dstpos, (void*)bytes, n);					\
	dstpos += n;											\
}
	
/* LOAD_BYTES: memcpy wrapper, reads from src buffer */
#define LOAD_BYTES(bytes, n) {								\
	assert(srcsize - srcpos >= n);							\
	memcpy((void*)bytes, src + srcpos, n);					\
	srcpos += n;											\
}


/* type: string (null-terminated) */

void apc_serialize_string(char* string)
{
	int len;

	/* by convention, mark null strings with a length of -1 */
	if (string == NULL) {
		SERIALIZE_SCALAR(-1, int);
		return;
	}
	len = strlen(string);
	SERIALIZE_SCALAR(len, int);
	STORE_BYTES(string, len);
}

void apc_create_string(char** string)
{
	int len;

	DESERIALIZE_SCALAR(&len, int);
	if (len == -1) {
		*string = NULL;
		return;
	}
	*string = (char*) emalloc(len + 1);
	LOAD_BYTES(*string, len);
	(*string)[len] = '\0';
}


/* type: arg_types (a special case of zend_uchar[]) */

void apc_serialize_arg_types(zend_uchar* arg_types)
{
	if (arg_types == NULL) {
		SERIALIZE_SCALAR(0, char);
		return; /* arg_types is null */
	}
	SERIALIZE_SCALAR(1, char);
	SERIALIZE_SCALAR(arg_types[0], zend_uchar);
	STORE_BYTES(arg_types + 1, arg_types[0]*sizeof(zend_uchar));
}

void apc_create_arg_types(zend_uchar** arg_types)
{
	char exists;
	zend_uchar count;

	DESERIALIZE_SCALAR(&exists, char);
	if (!exists) {
		*arg_types = NULL;
		return; /* arg_types is null */
	}
	DESERIALIZE_SCALAR(&count, zend_uchar);
	*arg_types = emalloc(count + 1);
	(*arg_types)[0] = count;
	LOAD_BYTES((*arg_types) + 1, count*sizeof(zend_uchar));
}


/* type: zend_llist */

static void store_zend_llist_element(void* arg, void* data)
{
	int size = *((int*)arg);
	STORE_BYTES((char*)data, size);
}

void apc_serialize_zend_llist(zend_llist* list)
{
	char exists;

	exists = (list != NULL) ? 1 : 0;
	SERIALIZE_SCALAR(exists, char);
	if (!exists) {
		return;
	}
	SERIALIZE_SCALAR(list->size, size_t);
	SERIALIZE_SCALAR(list->dtor, void*);
	SERIALIZE_SCALAR(list->persistent, unsigned char);
	SERIALIZE_SCALAR(zend_llist_count(list), int);
	zend_llist_apply_with_argument(list, store_zend_llist_element,
		&list->size);
}

void apc_deserialize_zend_llist(zend_llist* list)
{
	char exists;
	size_t size;
	void (*dtor)(void*);
	unsigned char persistent;
	int count, i;
	char* data;

	DESERIALIZE_SCALAR(&exists, char);
	assert(exists != 0); 

	/* read the list parameters */
	DESERIALIZE_SCALAR(&size, size_t);
	DESERIALIZE_SCALAR(&dtor, void*);
	DESERIALIZE_SCALAR(&persistent, unsigned char);
	/* initialize the list */
	zend_llist_init(list, size, dtor, persistent);

	/* insert the list elements */
	DESERIALIZE_SCALAR(&count, int);
	data = (char*) malloc(list->size);
	for (i = 0; i < count; i++) {
		LOAD_BYTES(data, list->size);
		zend_llist_add_element(list, data);
	}
	free(data);
}

void apc_create_zend_llist(zend_llist** list)
{
	char exists;

	/* Sneak a look one byte ahead to see whether the list exists or not.
	 * If it does, then exists is part of the structure, otherwise it was
	 * a zero-value placeholder byte. */
	PEEK_SCALAR(&exists, char);
	if (exists) {
		*list = (zend_llist*) emalloc(sizeof(zend_llist));
		apc_deserialize_zend_llist(*list);
	}
	else {
		DESERIALIZE_SCALAR(&exists, char);
		*list = 0;
	}
}


/* type: HashTable */
	
void apc_serialize_hashtable(HashTable* ht, void* funcptr)
{
	char exists;	/* for identifying null lists */
	Bucket* p;
	void (*serialize_bucket)(void*);

	serialize_bucket = (void(*)(void*)) funcptr;

	exists = (ht != NULL) ? 1 : 0;
	SERIALIZE_SCALAR(exists, char);
	if (!exists) {
		return;
	}

	/* Serialize the hash meta-data. */
	SERIALIZE_SCALAR(ht->nTableSize, uint);
	SERIALIZE_SCALAR(ht->pHashFunction, void*);
	SERIALIZE_SCALAR(ht->pDestructor, void*);
	SERIALIZE_SCALAR(ht->nNumOfElements, uint);
	SERIALIZE_SCALAR(ht->persistent, int);

	/* Iterate through the buckets of the hash, serializing as we go. */
	p = ht->pListHead;
	while(p != NULL) {
		SERIALIZE_SCALAR(p->nKeyLength,uint);
		apc_serialize_string(p->arKey);
		serialize_bucket(p->pData); 
		p = p->pListNext;
	}
}

void apc_deserialize_hashtable(HashTable* ht, void* funcptr, int datasize)
{
	char exists;
	uint nSize;
	hash_func_t pHashFunction;
	dtor_func_t pDestructor;
	uint nNumOfElements;
	int persistent;
	int j;
	uint nKeyLength;
	char* arKey;
	void* pData;
	int status;
	void (*deserialize_bucket)(void*);

	deserialize_bucket = (void(*)(void*)) funcptr;

	DESERIALIZE_SCALAR(&exists, char);
	assert(exists != 0);

	DESERIALIZE_SCALAR(&nSize, uint);
	DESERIALIZE_SCALAR(&pHashFunction, void*);
	DESERIALIZE_SCALAR(&pDestructor, void*);
	DESERIALIZE_SCALAR(&nNumOfElements,uint);
	DESERIALIZE_SCALAR(&persistent, int);
	
	/* Although the hash is already allocated (we're a deserialize, not a 
	 * create), we still need to initialize it. If this fails, something 
	 * very very bad happened. */
	status = zend_hash_init(ht, nSize, pHashFunction, pDestructor, persistent);
	assert(status != FAILURE);
	
	/* Luckily, the number of elements in a hash is part of its struct, so
	 * we can just deserialize that many hashtable elements. */
	for (j = 0; j < nNumOfElements; j++) {
		DESERIALIZE_SCALAR(&nKeyLength, uint);
		apc_create_string(&arKey);
		deserialize_bucket(&pData);
		if(datasize == sizeof(zval *)) {
		zend_hash_add_or_update(ht, arKey, nKeyLength, &pData, datasize,
			NULL, HASH_ADD);
		}
		else {
			zend_hash_add_or_update(ht, arKey, nKeyLength, pData, datasize,
				NULL, HASH_ADD);
		}

	}
}

void apc_create_hashtable(HashTable** ht, void* funcptr, int datasize)
{
	char exists;	/* for identifying null hashtables */

	PEEK_SCALAR(&exists, char);
	if (exists) {
		*ht = (HashTable*) emalloc(sizeof(HashTable));
		apc_deserialize_hashtable(*ht, funcptr, datasize);
	}
	else {
		DESERIALIZE_SCALAR(&exists, char);
		*ht = 0;
	}
}

/* type: zvalue_value */

void apc_serialize_zvalue_value(zvalue_value* zv, int type)
{
	/* A zvalue_value is a union, and as such we first need to
	 * determine exactly what it's type is, then serialize the
	 * appropriate structure. */

	switch (type) {
	  case IS_RESOURCE:
	  case IS_BOOL:
	  case IS_LONG:
		SERIALIZE_SCALAR(zv->lval, long);
		break;	
	  case IS_DOUBLE:
		SERIALIZE_SCALAR(zv->dval, double);
		break;
	  case IS_NULL:
		/* null value, do nothing */
		break;
	  case IS_CONSTANT:
	  case IS_STRING:
	  case FLAG_IS_BC:
		apc_serialize_string(zv->str.val);
		SERIALIZE_SCALAR(zv->str.len, int);
		break;
	  case IS_ARRAY:
	  case IS_CONSTANT_ARRAY:
		apc_serialize_hashtable(zv->ht, apc_serialize_zval_ptr);
		break;
	  case IS_OBJECT:
		apc_serialize_zend_class_entry(zv->obj.ce);
		apc_serialize_hashtable(zv->obj.properties, apc_serialize_zval_ptr);
		break;
	  default:
		/* The above list enumerates all types.  If we get here,
		 * something very very bad has happened. */
		assert(0);
	}
}

void apc_deserialize_zvalue_value(zvalue_value* zv, int type)
{
	/* We peeked ahead in the calling routine to deserialize the
	 * type. Now we just deserialize. */

	switch(type) {
	  case IS_RESOURCE:
	  case IS_BOOL:
	  case IS_LONG:
		DESERIALIZE_SCALAR(&zv->lval, int);
		break;
	  case IS_NULL:
		/* null value, do nothing */
		break;
	  case IS_DOUBLE:
		DESERIALIZE_SCALAR(&zv->dval, double);
		break;
	  case IS_CONSTANT:
	  case IS_STRING:
	  case FLAG_IS_BC:
		apc_create_string(&zv->str.val);
		DESERIALIZE_SCALAR(&zv->str.len, int);
		break;
	  case IS_ARRAY:
	  case IS_CONSTANT_ARRAY:
		apc_create_hashtable(&zv->ht, apc_create_zval, sizeof(void*));
		break;
	  case IS_OBJECT:
		apc_create_zend_class_entry(&zv->obj.ce);
		apc_create_hashtable(&zv->obj.properties, apc_create_zval,
			sizeof(zval *));
		break;
	  default:
        /* The above list enumerates all types.  If we get here,
         * something very very bad has happened. */
		assert(0);
	}	
}


/* type: zval */

void apc_serialize_zval_ptr(zval** zv)
{
	apc_serialize_zval(*zv);
}

void apc_serialize_zval(zval* zv)
{
	/* type is the switch for serializing zvalue_value */
	SERIALIZE_SCALAR(zv->type, zend_uchar);
	apc_serialize_zvalue_value(&zv->value, zv->type);
	SERIALIZE_SCALAR(zv->is_ref, zend_uchar);
	SERIALIZE_SCALAR(zv->refcount, zend_ushort);
}

void apc_deserialize_zval(zval* zv)
{
	/* type is the switch for deserializing zvalue_value */
	DESERIALIZE_SCALAR(&zv->type, zend_uchar);
	apc_deserialize_zvalue_value(&zv->value, zv->type);
	DESERIALIZE_SCALAR(&zv->is_ref, zend_uchar);
	DESERIALIZE_SCALAR(&zv->refcount, zend_ushort);
}

void apc_create_zval(zval** zv)
{
	*zv = (zval*) emalloc(sizeof(zval));
	apc_deserialize_zval(*zv);
}


/* type: zend_function_entry */

void apc_serialize_zend_function_entry(zend_function_entry* zfe)
{
	apc_serialize_string(zfe->fname);
	SERIALIZE_SCALAR(zfe->handler, void*);
	apc_serialize_arg_types(zfe->func_arg_types);
}

void apc_deserialize_zend_function_entry(zend_function_entry* zfe)
{
	apc_create_string(&zfe->fname);
	DESERIALIZE_SCALAR(&zfe->handler, void*);
	apc_create_arg_types(&zfe->func_arg_types);
}


/* type: zend_property_reference */

void apc_serialize_zend_property_reference(zend_property_reference* zpr)
{
	SERIALIZE_SCALAR(zpr->type, int);
	apc_serialize_zval(zpr->object);
	apc_serialize_zend_llist(zpr->elements_list);
}

void apc_deserialize_zend_property_reference(zend_property_reference* zpr)
{
	SERIALIZE_SCALAR(zpr->type, int);
	apc_deserialize_zval(zpr->object);
	apc_create_zend_llist(&zpr->elements_list);
}


/* type: zend_overloaded_element */

void apc_serialize_zend_overloaded_element(zend_overloaded_element* zoe)
{
	SERIALIZE_SCALAR(zoe->type, zend_uchar);
	apc_serialize_zval(&zoe->element);
}

void apc_deserialize_zend_overloaded_element(zend_overloaded_element* zoe)
{
	DESERIALIZE_SCALAR(&zoe->type, zend_uchar);
	apc_deserialize_zval(&zoe->element);
}


/* type: zend_class_entry */


void apc_serialize_zend_class_entry(zend_class_entry* zce)
{
	zend_function_entry* zfe;
	int count, i, exists;

	SERIALIZE_SCALAR(zce->type, char);
	apc_serialize_string(zce->name);
	SERIALIZE_SCALAR(zce->name_length, uint);

	/* Serialize the name of this class's parent class (if it has one)
	 * so that we can perform inheritance during deserialization (see
	 * apc_deserialize_zend_class_entry). */
	
 	exists = (zce->parent != NULL) ? 1 : 0;
    SERIALIZE_SCALAR(exists, char);
	if (exists) {
		apc_serialize_string(zce->parent->name);
	}

	SERIALIZE_SCALAR(zce->refcount[0], int);
	SERIALIZE_SCALAR(zce->constants_updated, zend_bool);
	apc_serialize_hashtable(&zce->function_table, apc_serialize_zend_function);
	apc_serialize_hashtable(&zce->default_properties, apc_serialize_zval_ptr);

	/* zend_class_entry.builtin_functions: this array appears to be
	 * terminated by an element where zend_function_entry.fname is null 
	 * First we count the number of elements, then we serialize that count
	 * and all the functions. */

	count = 0;
	if (zce->builtin_functions) {
		for (zfe = zce->builtin_functions; zfe->fname != NULL; zfe++) {
			count++;
		}
	}
	SERIALIZE_SCALAR(count, int);
	for (i = 0; i < count; i++) {
		apc_serialize_zend_function_entry(&zce->builtin_functions[i]);
	}
	
	SERIALIZE_SCALAR(zce->handle_function_call, void*);
	SERIALIZE_SCALAR(zce->handle_property_get, void*);
	SERIALIZE_SCALAR(zce->handle_property_set, void*);
}

void apc_deserialize_zend_class_entry(zend_class_entry* zce)
{
	int count, i, exists;
	namearray_t* children;

	DESERIALIZE_SCALAR(&zce->type, char);
	apc_create_string(&zce->name);
	DESERIALIZE_SCALAR(&zce->name_length, uint);

	DESERIALIZE_SCALAR(&exists, char);
	if (exists) {
		/* This class knows the name of its parent, most likely because
		 * its parent is defined in the same source file. Therefore, we
		 * can simply restore the parent link, and not worry about
		 * manually inheriting methods from the parent (PHP will perform
		 * the inheritance). */

		char* parent_name;	/* name of parent class */

		apc_create_string(&parent_name);
		if (zend_hash_find(CG(class_table), parent_name,
			strlen(parent_name) + 1, (void**) &zce->parent) == FAILURE)
		{
			zend_error(E_ERROR, "parent '%s' of class '%s' undefined",
				parent_name, zce->name);
		}
		efree(parent_name);
	}

	/* refcount is a pointer to a single int.  Don't ask me why, I
	 * just work here. */
	zce->refcount = (int*) emalloc(sizeof(int));
	DESERIALIZE_SCALAR(zce->refcount, int);
	DESERIALIZE_SCALAR(&zce->constants_updated, zend_bool);
	apc_deserialize_hashtable(&zce->function_table, apc_create_zend_function,
		sizeof(zend_function));
	apc_deserialize_hashtable(&zce->default_properties, apc_create_zval,
		sizeof(zval *));

	/* see apc_serialize_zend_class_entry() for a description of the
	 * zend_class_entry.builtin_functions member */

	DESERIALIZE_SCALAR(&count, int);
	zce->builtin_functions = NULL;
	if (count > 0) {
		zce->builtin_functions = (zend_function_entry*)
			emalloc((count+1) * sizeof(zend_function_entry));
		zce->builtin_functions[count].fname = NULL;
		for (i = 0; i < count; i++) {
			apc_deserialize_zend_function_entry(&zce->builtin_functions[i]);
		}
	}
	
	DESERIALIZE_SCALAR(&zce->handle_function_call, void*);
	DESERIALIZE_SCALAR(&zce->handle_property_get, void*);
	DESERIALIZE_SCALAR(&zce->handle_property_set, void*);


	/* Resolve the inheritance relationships that have been delayed and
	 * are waiting for this class to be created, i.e., every child of
	 * this class that has already been compiled needs to be inherited
	 * from this class. Call inherit() with this class as the base class
	 * (first parameter) and as the current parent class (second parameter).
	 * inherit will recursively resolve every inheritance relationships
	 * in which this class is the base. */

	inherit(zce, zce);
}

void apc_create_zend_class_entry(zend_class_entry** zce)
{
	*zce = (zend_class_entry*) emalloc(sizeof(zend_class_entry));
	apc_deserialize_zend_class_entry(*zce);
}


/* type: zend_utility_functions */

void apc_serialize_zend_utility_functions(zend_utility_functions* zuf)
{
	SERIALIZE_SCALAR(zuf->error_function, void*);
	SERIALIZE_SCALAR(zuf->printf_function, void*);
	SERIALIZE_SCALAR(zuf->write_function, void*);
	SERIALIZE_SCALAR(zuf->fopen_function, void*);
	SERIALIZE_SCALAR(zuf->message_handler, void*);
	SERIALIZE_SCALAR(zuf->block_interruptions, void*);
	SERIALIZE_SCALAR(zuf->unblock_interruptions, void*);
	SERIALIZE_SCALAR(zuf->get_ini_entry, void*);
	SERIALIZE_SCALAR(zuf->ticks_function, void*);
}

void apc_deserialize_zend_utility_functions(zend_utility_functions* zuf)
{
	DESERIALIZE_SCALAR(&zuf->error_function, void*);
	DESERIALIZE_SCALAR(&zuf->printf_function, void*);
	DESERIALIZE_SCALAR(&zuf->write_function, void*);
	DESERIALIZE_SCALAR(&zuf->fopen_function, void*);
	DESERIALIZE_SCALAR(&zuf->message_handler, void*);
	DESERIALIZE_SCALAR(&zuf->block_interruptions, void*);
	DESERIALIZE_SCALAR(&zuf->unblock_interruptions, void*);
	DESERIALIZE_SCALAR(&zuf->get_ini_entry, void*);
	DESERIALIZE_SCALAR(&zuf->ticks_function, void*);
}


/* type: zend_utility_values */

void apc_serialize_zend_utility_values(zend_utility_values* zuv)
{
	apc_serialize_string(zuv->import_use_extension);
	SERIALIZE_SCALAR(zuv->import_use_extension_length, uint);
}

void apc_deserialize_zend_utility_values(zend_utility_values* zuv)
{
	apc_create_string(&zuv->import_use_extension);
	DESERIALIZE_SCALAR(&zuv->import_use_extension_length, uint);
}


/* type: znode */

void apc_serialize_znode(znode* zn)
{
	SERIALIZE_SCALAR(zn->op_type, int);

	/* If the znode's op_type is IS_CONST, we know precisely what it is.
	 * otherwise, it is too case-dependent (or inscrutable), so we do
	 * a bitwise copy. */
	
	switch(zn->op_type) {
	  case IS_CONST: 
		apc_serialize_zval(&zn->u.constant);
		break;
	  default:
		STORE_BYTES(&zn->u, sizeof(zn->u));
		break;
	}
}

void apc_deserialize_znode(znode* zn)
{
	DESERIALIZE_SCALAR(&zn->op_type, int);

	/* If the znode's op_type is IS_CONST, we know precisely what it is.
	 * otherwise, it is too case-dependent (or inscrutable), so we do
	 * a bitwise copy. */
	
	switch(zn->op_type) {
	  case IS_CONST:
		apc_deserialize_zval(&zn->u.constant);
		break;
	  default:
		LOAD_BYTES(&zn->u, sizeof(zn->u));
		break;
	}
}


/* type: zend_op */

void apc_serialize_zend_op(zend_op* zo)
{
	SERIALIZE_SCALAR(zo->opcode, zend_uchar);
	apc_serialize_znode(&zo->result);
	apc_serialize_znode(&zo->op1);
	apc_serialize_znode(&zo->op2);
	SERIALIZE_SCALAR(zo->extended_value, ulong);
	SERIALIZE_SCALAR(zo->lineno, uint);
}

void apc_deserialize_zend_op(zend_op* zo)
{
	DESERIALIZE_SCALAR(&zo->opcode, zend_uchar);
	apc_deserialize_znode(&zo->result);
	apc_deserialize_znode(&zo->op1);
	apc_deserialize_znode(&zo->op2);
	DESERIALIZE_SCALAR(&zo->extended_value, ulong);
	DESERIALIZE_SCALAR(&zo->lineno, uint);
}


/* type: zend_op_array */

void apc_serialize_zend_op_array(zend_op_array* zoa)
{
	char exists;
	int i;

	SERIALIZE_SCALAR(zoa->type, zend_uchar);
	apc_serialize_arg_types(zoa->arg_types);
	apc_serialize_string(zoa->function_name);
	SERIALIZE_SCALAR(zoa->refcount[0], zend_uint);
	SERIALIZE_SCALAR(zoa->last, zend_uint);
	SERIALIZE_SCALAR(zoa->size, zend_uint);

	/* If a file 'A' is included twice in a single request, the following 
	 * situation can occur: A is deserialized and its functions added to
	 * the global function table. On its next call, A is expired (either
	 * forcibly removed or removed due to an expired ttl). Now when A is
	 * compiled, its functions can't be added to the global function_table
	 * (they are already present) so they are serialized as an opcode
	 * ZEND_DECLARE_FUNCTION_OR_CLASS. This means that the functions will
	 * be declared at execution time. Since they are present in the global
	 * function_table, they will will also be serialized. This will cause
	 * a fatal 'failed to redclare....' error.  We avoid this by simulating
	 * the action of the parser and changing all
	 * ZEND_DECLARE_FUNCTION_OR_CLASS opcodes to ZEND_NOPs. */ 
	 
	for (i = 0; i < zoa->last; i++) {
		apc_serialize_zend_op(&zoa->opcodes[i]);
	}

	SERIALIZE_SCALAR(zoa->T, zend_uint);
	SERIALIZE_SCALAR(zoa->last_brk_cont, zend_uint);
	SERIALIZE_SCALAR(zoa->current_brk_cont, zend_uint);
	SERIALIZE_SCALAR(zoa->uses_globals, zend_bool);
	exists = (zoa->brk_cont_array != NULL) ? 1 : 0;
	SERIALIZE_SCALAR(exists, char);
	if (exists) {
		STORE_BYTES(zoa->brk_cont_array, zoa->last_brk_cont *
			sizeof(zend_brk_cont_element));
	}
	apc_serialize_hashtable(zoa->static_variables, apc_serialize_zval_ptr);
#if SUPPORT_INTERACTIVE
	/* we don't */
#endif
	SERIALIZE_SCALAR(zoa->return_reference, zend_bool);
	SERIALIZE_SCALAR(zoa->done_pass_two, zend_bool);
	apc_serialize_string(zoa->filename);
	/* zend_op_array.reserved is not used */
}

void apc_deserialize_zend_op_array(zend_op_array* zoa)
{
	char exists;
	int i;

	DESERIALIZE_SCALAR(&zoa->type, zend_uchar);
	apc_create_arg_types(&zoa->arg_types);
	apc_create_string(&zoa->function_name);
	zoa->refcount = (int*) emalloc(sizeof(zend_uint));
	DESERIALIZE_SCALAR(zoa->refcount, zend_uint);
	DESERIALIZE_SCALAR(&zoa->last, zend_uint);
	DESERIALIZE_SCALAR(&zoa->size, zend_uint);

	zoa->opcodes = NULL;

	if (zoa->last > 0) {
		zoa->opcodes = (zend_op*) emalloc(zoa->last * sizeof(zend_op));

		for (i = 0; i < zoa->last; i++) {
			apc_deserialize_zend_op(&zoa->opcodes[i]);

			if(zoa->opcodes[i].opcode == ZEND_DECLARE_FUNCTION_OR_CLASS) {
				HashTable* table;
				char* op2str;	/* op2str and op2len are for convenience */
				int op2len;
				
				op2str = zoa->opcodes[i].op2.u.constant.value.str.val;
				op2len = zoa->opcodes[i].op2.u.constant.value.str.len;

				switch(zoa->opcodes[i].extended_value) {
				  /* a run-time function declaration */
				  case ZEND_DECLARE_FUNCTION: {
					zend_function* function;

					table = CG(function_table);

					if (zend_hash_find(table, op2str, op2len + 1,
						(void**) &function) == SUCCESS) 
					{
						zval_dtor(&zoa->opcodes[i].op1.u.constant);
						zval_dtor(&zoa->opcodes[i].op2.u.constant);
						zoa->opcodes[i].opcode = ZEND_NOP;
						memset(&zoa->opcodes[i].op1, 0, sizeof(znode));
						memset(&zoa->opcodes[i].op2, 0, sizeof(znode));
						zoa->opcodes[i].op1.op_type = IS_UNUSED;
						zoa->opcodes[i].op2.op_type = IS_UNUSED;
					}
				  } break;
				  
				  /* a run-time class declaration */
				  case ZEND_DECLARE_CLASS: {
					zend_class_entry *ce;

					table = CG(class_table);

					if (zend_hash_find(table, op2str, op2len + 1,
						(void**) &ce) == SUCCESS)
					{
						zval_dtor(&zoa->opcodes[i].op1.u.constant);
						zval_dtor(&zoa->opcodes[i].op2.u.constant);
						zoa->opcodes[i].opcode = ZEND_NOP;
						memset(&zoa->opcodes[i].op1, 0, sizeof(znode));
						memset(&zoa->opcodes[i].op2, 0, sizeof(znode));
						zoa->opcodes[i].op1.op_type = IS_UNUSED;
						zoa->opcodes[i].op2.op_type = IS_UNUSED;
					}
				  } break;
				
				  /* a run-time derived class declaration */
				  case ZEND_DECLARE_INHERITED_CLASS: {
					zend_class_entry* ce;
					char* class_name;
					char* parent_name;
					int class_name_length;
					namearray_t* children;
					int minSize;

					table = CG(class_table);

					/* op2str is a class name of the form "base:derived",
					 * where derived is the class being declared and base
					 * is its parent in the class hierarchy. Extract the
					 * two names into parent_name and class_name. */
					
					parent_name = apc_estrdup(op2str);
					if ((class_name = strchr(parent_name, ':')) == 0) {
						zend_error(E_CORE_ERROR,"Invalid runtime class entry");
					}
					*(strchr(parent_name, ':')) = '\0';

					class_name++;	/* advance past ':' */

					parent_name = apc_estrdup(op2str);
					*(strchr(parent_name, ':')) = '\0';

					if (zend_hash_find(table, class_name, strlen(class_name)+1,
						(void**) &ce) == SUCCESS) 
					{
						zval_dtor(&zoa->opcodes[i].op1.u.constant);
						zval_dtor(&zoa->opcodes[i].op2.u.constant);
						zoa->opcodes[i].opcode = ZEND_NOP;
						memset(&zoa->opcodes[i].op1, 0, sizeof(znode));
						memset(&zoa->opcodes[i].op2, 0, sizeof(znode));
						zoa->opcodes[i].op1.op_type = IS_UNUSED;
						zoa->opcodes[i].op2.op_type = IS_UNUSED;
					}

					/* The parent class hasn't been compiled yet, most likely
					 * because it is defined in an included file. We must defer
					 * this inheritance until the parent class is created. Add
					 * a tuple for this class and its parent to the deferred
					 * inheritance table */
		
					class_name_length = strlen(class_name);

					children = apc_nametable_retrieve(deferred_inheritance,
					                                  parent_name);
		
					if (children == 0) {
						/* Create and initialize a new namearray_t for this
						 * class's parent and insert into the deferred
						 * inheritance table. */
		
						children = (namearray_t*) malloc(sizeof(namearray_t));
						children->length = 0;
						children->size = class_name_length + 1;
						children->strings = (char*) malloc(children->size);
		
						apc_nametable_insert(deferred_inheritance,
						                     parent_name, children);
					}
		
					/* Add this class's name to the namearray_t 'children' */

					minSize = children->length + class_name_length + 1;
		
					if (minSize > children->size) {
						/* The strings array (children->strings) is not big
						 * enough to store this class name. Expand the array
						 * using an exponential resize. */

						while (minSize > children->size) {
							children->size *= 2;
						}
						children->strings = apc_erealloc(children->strings,
						                                 children->size);
					}
					memcpy(children->strings + children->length,
						class_name, class_name_length + 1);
					children->length += class_name_length + 1;
				  } break;

				  default:
					break;
				}
			}
		}
	}

	DESERIALIZE_SCALAR(&zoa->T, zend_uint);
	DESERIALIZE_SCALAR(&zoa->last_brk_cont, zend_uint);
	DESERIALIZE_SCALAR(&zoa->current_brk_cont, zend_uint);
	DESERIALIZE_SCALAR(&zoa->uses_globals, zend_bool);
	DESERIALIZE_SCALAR(&exists, char);
	zoa->brk_cont_array = NULL;
	if (exists) {
		zoa->brk_cont_array = (zend_brk_cont_element*)
			emalloc(zoa->last_brk_cont * sizeof(zend_brk_cont_element));
		LOAD_BYTES(zoa->brk_cont_array, zoa->last_brk_cont *
			sizeof(zend_brk_cont_element));
	}
	apc_create_hashtable(&zoa->static_variables, apc_create_zval, sizeof(zval *));
#if SUPPORT_INTERACTIVE
	/* we accept patches */
#endif
	DESERIALIZE_SCALAR(&zoa->return_reference, zend_bool);
	DESERIALIZE_SCALAR(&zoa->done_pass_two, zend_bool);
	apc_create_string(&zoa->filename);
}

void apc_create_zend_op_array(zend_op_array** zoa)
{
	*zoa = (zend_op_array*) emalloc(sizeof(zend_op_array));
	apc_deserialize_zend_op_array(*zoa);
}


/* type: zend_internal_function */

void apc_serialize_zend_internal_function(zend_internal_function* zif)
{
	SERIALIZE_SCALAR(zif->type, zend_uchar);
	apc_serialize_arg_types(zif->arg_types);
	apc_serialize_string(zif->function_name);
	SERIALIZE_SCALAR(zif->handler, void*);	
}

void apc_deserialize_zend_internal_function(zend_internal_function* zif)
{
	DESERIALIZE_SCALAR(&zif->type, zend_uchar);
	apc_create_arg_types(&zif->arg_types);
	apc_create_string(&zif->function_name);
	DESERIALIZE_SCALAR(&zif->handler, void*);
}


/* type: zend_overloaded_function */

void apc_serialize_zend_overloaded_function(zend_overloaded_function* zof)
{
	SERIALIZE_SCALAR(zof->type, zend_uchar);
	apc_serialize_arg_types(zof->arg_types);
	apc_serialize_string(zof->function_name);
	SERIALIZE_SCALAR(zof->var, zend_uint);	
}

void apc_deserialize_zend_overloaded_function(zend_overloaded_function* zof)
{
	DESERIALIZE_SCALAR(&zof->type, zend_uchar);
	apc_create_arg_types(&zof->arg_types);
	apc_create_string(&zof->function_name);
	DESERIALIZE_SCALAR(&zof->var, zend_uint);
}


/* type: zend_function */

void apc_serialize_zend_function(zend_function* zf)
{
	/* zend_function come in 4 different types with different internal 
	 * structs. */
	switch(zf->type) {
	  case ZEND_INTERNAL_FUNCTION:
		apc_serialize_zend_internal_function(&zf->internal_function);
		break;
	  case ZEND_OVERLOADED_FUNCTION:
		apc_serialize_zend_overloaded_function(&zf->overloaded_function);
		break;
	  case ZEND_USER_FUNCTION:
	  case ZEND_EVAL_CODE:
		apc_serialize_zend_op_array(&zf->op_array);
		break;
	  default:
		/* the above are all valid zend_function types.  If we hit this
		 * case something has gone very very wrong. */
		assert(0);
	}
}

void apc_deserialize_zend_function(zend_function* zf)
{
	PEEK_SCALAR(&zf->type, zend_uchar);
	switch(zf->type) {
	  case ZEND_INTERNAL_FUNCTION:
		apc_deserialize_zend_internal_function(&zf->internal_function);
		break;
      case ZEND_OVERLOADED_FUNCTION:
		apc_deserialize_zend_overloaded_function(&zf->overloaded_function);
		break;
      case ZEND_USER_FUNCTION:
      case ZEND_EVAL_CODE:
		apc_deserialize_zend_op_array(&zf->op_array);
		break;
	  default:
		/* the above are all valid zend_function types.  If we hit this
         * case something has gone very very wrong. */
		assert(0);
	}
}

void apc_create_zend_function(zend_function** zf)
{
	*zf = (zend_function*) emalloc(sizeof(zend_function));
	apc_deserialize_zend_function(*zf);
}


/* special purpose serialization functions */

/* In serialize_function_table we serialize all the elements of the 
 * global function_table that were inserted during this compilation.
 * We track this using both a global accumulator table acc and a 
 * table priv which tracks changes made specifically in this file.
 * We need a priv table to handle another aspect of the situation,
 * when a file is included multiple times.  Whenever a file is
 * serialized after having already been seen once during a particular
 * request, all the functions it deserialized previously are deleted
 * from the accumulator table. If we don't do this, it won't appear
 * to have declared any new functions during the second call to this
 * routine, and no functions will be serialized. */

static int store_function_table(void *element, int num_args,
	va_list args, zend_hash_key *hash_key)
{
	zend_function* zf;
	apc_nametable_t* acc;
	apc_nametable_t* priv;

	zf = (zend_function*) element;
	acc = va_arg(args, apc_nametable_t*);
	priv = va_arg(args, apc_nametable_t*);

	/* do not serialize internal functions */
	if (zf->type == ZEND_INTERNAL_FUNCTION) {
		return 0;
	}

	/* serialize differences */
	if (apc_nametable_insert(acc, zf->common.function_name, 0) != 0) {
		SERIALIZE_SCALAR(1, char);
		apc_serialize_zend_function(zf);
		apc_nametable_insert(priv, zf->common.function_name, 0);
	}
	return 0;
}

void apc_serialize_zend_function_table(HashTable* gft,
	apc_nametable_t* acc, apc_nametable_t* priv)
{
	zend_hash_apply_with_arguments(gft, store_function_table, 2, acc, priv);
	SERIALIZE_SCALAR(0, char);
}

/* During deserialization we deserialize functions and add them to the
 * global function_table, the accumulator table for that requst, and the
 * private function_table for the file being compiled. See the note at
 * the top of apc_serialize_zend_function_table for the logic. */

void apc_deserialize_zend_function_table(HashTable* gft, apc_nametable_t* acc, apc_nametable_t* priv)
{
	zend_function* zf;
	char exists;

	DESERIALIZE_SCALAR(&exists, char);
	while (exists) {
		apc_create_zend_function(&zf);
		if (zend_hash_add(gft, zf->common.function_name,
			strlen(zf->common.function_name)+1, zf,
			sizeof(zend_function), NULL) == FAILURE)
		{
		// This can validly happen.
		}
		apc_nametable_insert(acc, zf->common.function_name, 0);
		apc_nametable_insert(priv, zf->common.function_name, 0);
		DESERIALIZE_SCALAR(&exists, char);
	}
}

/* The logic for serialize_class_table and deserialize_class_table
 * mirror that of the corresponding function_table functions (see
 * above), with classes in place of functions. */

static int store_class_table(void *element, int num_args,
	va_list args, zend_hash_key *hash_key)
{
	zend_class_entry* zc;
	apc_nametable_t* acc;
	apc_nametable_t* priv;

	zc = (zend_class_entry*) element;
	acc = va_arg(args, apc_nametable_t*);
	priv = va_arg(args, apc_nametable_t*);

	/* do not serialize internal classes */
	if (zc->type == ZEND_INTERNAL_CLASS) {
		return 0;
	}

	/* serialize differences */
	if (apc_nametable_insert(acc, zc->name, 0) != 0) {
		SERIALIZE_SCALAR(1, char);
		apc_serialize_zend_class_entry(zc);
		apc_nametable_insert(priv, zc->name, 0);
	}

	return 0;
}

void apc_serialize_zend_class_table(HashTable* gct,
	apc_nametable_t* acc, apc_nametable_t* priv)
{
	zend_class_entry* zc;

	zend_hash_apply_with_arguments(gct, store_class_table, 2, acc, priv);
	SERIALIZE_SCALAR(0, char);
}

void apc_deserialize_zend_class_table(HashTable* gct, apc_nametable_t* acc, apc_nametable_t* priv)
{
	char exists;
	zend_class_entry* zc;
	int status;

	DESERIALIZE_SCALAR(&exists, char);
	while (exists) {
		apc_create_zend_class_entry(&zc);
		if (zend_hash_add(gct, zc->name, zc->name_length + 1,
			zc, sizeof(zend_class_entry), NULL) == FAILURE)
		{
		// This can validly happen.
		}
		apc_nametable_insert(acc, zc->name, 0);
		apc_nametable_insert(priv, zc->name, 0);
		DESERIALIZE_SCALAR(&exists, char);
	}
}

