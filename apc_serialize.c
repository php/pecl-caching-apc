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
#include "apc_lib.h"
#include "apc_phpdeps.h"
#include "apc_sma.h"
#include "apc_version.h"
#include "apc_list.h"
#include <stdlib.h>
#include <assert.h>


#include "zend_variables.h"	/*  for zval_dtor() */

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

static const char* opcodes[] = {
  "ZEND_NOP", /*  0 */
  "ZEND_ADD", /*  1 */
  "ZEND_SUB", /*  2 */
  "ZEND_MUL", /*  3 */
  "ZEND_DIV", /*  4 */
  "ZEND_MOD", /*  5 */
  "ZEND_SL", /*  6 */
  "ZEND_SR", /*  7 */
  "ZEND_CONCAT", /*  8 */
  "ZEND_BW_OR", /*  9 */
  "ZEND_BW_AND", /*  10 */
  "ZEND_BW_XOR", /*  11 */
  "ZEND_BW_NOT", /*  12 */
  "ZEND_BOOL_NOT", /*  13 */
  "ZEND_BOOL_XOR", /*  14 */
  "ZEND_IS_IDENTICAL", /*  15 */
  "ZEND_IS_NOT_IDENTICAL", /*  16 */
  "ZEND_IS_EQUAL", /*  17 */
  "ZEND_IS_NOT_EQUAL", /*  18 */
  "ZEND_IS_SMALLER", /*  19 */
  "ZEND_IS_SMALLER_OR_EQUAL", /*  20 */
  "ZEND_CAST", /*  21 */
  "ZEND_QM_ASSIGN", /*  22 */
  "ZEND_ASSIGN_ADD", /*  23 */
  "ZEND_ASSIGN_SUB", /*  24 */
  "ZEND_ASSIGN_MUL", /*  25 */
  "ZEND_ASSIGN_DIV", /*  26 */
  "ZEND_ASSIGN_MOD", /*  27 */
  "ZEND_ASSIGN_SL", /*  28 */
  "ZEND_ASSIGN_SR", /*  29 */
  "ZEND_ASSIGN_CONCAT", /*  30 */
  "ZEND_ASSIGN_BW_OR", /*  31 */
  "ZEND_ASSIGN_BW_AND", /*  32 */
  "ZEND_ASSIGN_BW_XOR", /*  33 */
  "ZEND_PRE_INC", /*  34 */
  "ZEND_PRE_DEC", /*  35 */
  "ZEND_POST_INC", /*  36 */
  "ZEND_POST_DEC", /*  37 */
  "ZEND_ASSIGN", /*  38 */
  "ZEND_ASSIGN_REF", /*  39 */
  "ZEND_ECHO", /*  40 */
  "ZEND_PRINT", /*  41 */
  "ZEND_JMP", /*  42 */
  "ZEND_JMPZ", /*  43 */
  "ZEND_JMPNZ", /*  44 */
  "ZEND_JMPZNZ", /*  45 */
  "ZEND_JMPZ_EX", /*  46 */
  "ZEND_JMPNZ_EX", /*  47 */
  "ZEND_CASE", /*  48 */
  "ZEND_SWITCH_FREE", /*  49 */
  "ZEND_BRK", /*  50 */
  "ZEND_CONT", /*  51 */
  "ZEND_BOOL", /*  52 */
  "ZEND_INIT_STRING", /*  53 */
  "ZEND_ADD_CHAR", /*  54 */
  "ZEND_ADD_STRING", /*  55 */
  "ZEND_ADD_VAR", /*  56 */
  "ZEND_BEGIN_SILENCE", /*  57 */
  "ZEND_END_SILENCE", /*  58 */
  "ZEND_INIT_FCALL_BY_NAME", /*  59 */
  "ZEND_DO_FCALL", /*  60 */
  "ZEND_DO_FCALL_BY_NAME", /*  61 */
  "ZEND_DO_FCALL_BY_NAME", /*  61 */
  "ZEND_RETURN", /*  62 */
  "ZEND_RECV", /*  63 */
  "ZEND_RECV_INIT", /*  64 */
  "ZEND_SEND_VAL", /*  65 */
  "ZEND_SEND_VAR", /*  66 */
  "ZEND_SEND_REF", /*  67 */
  "ZEND_NEW", /*  68 */
  "ZEND_JMP_NO_CTOR", /*  69 */
  "ZEND_FREE", /*  70 */
  "ZEND_INIT_ARRAY", /*  71 */
  "ZEND_ADD_ARRAY_ELEMENT", /*  72 */
  "ZEND_INCLUDE_OR_EVAL", /*  73 */
  "ZEND_UNSET_VAR", /*  74 */
  "ZEND_UNSET_DIM_OBJ", /*  75 */
  "ZEND_ISSET_ISEMPTY", /*  76 */
  "ZEND_FE_RESET", /*  77 */
  "ZEND_FE_FETCH", /*  78 */
  "ZEND_EXIT", /*  79 */
  "ZEND_FETCH_R", /*  80 */
  "ZEND_FETCH_DIM_R", /*  81 */
  "ZEND_FETCH_OBJ_R", /*  82 */
  "ZEND_FETCH_W", /*  83 */
  "ZEND_FETCH_DIM_W", /*  84 */
  "ZEND_FETCH_OBJ_W", /*  85 */
  "ZEND_FETCH_RW", /*  86 */
  "ZEND_FETCH_DIM_RW", /*  87 */
  "ZEND_FETCH_OBJ_RW", /*  88 */
  "ZEND_FETCH_IS", /*  89 */
  "ZEND_FETCH_DIM_IS", /*  90 */
  "ZEND_FETCH_OBJ_IS", /*  91 */
  "ZEND_FETCH_FUNC_ARG", /*  92 */
  "ZEND_FETCH_DIM_FUNC_ARG", /*  93 */
  "ZEND_FETCH_OBJ_FUNC_ARG", /*  94 */
  "ZEND_FETCH_UNSET", /*  95 */
  "ZEND_FETCH_DIM_UNSET", /*  96 */
  "ZEND_FETCH_OBJ_UNSET", /*  97 */
  "ZEND_FETCH_DIM_TMP_VAR", /*  98 */
  "ZEND_FETCH_CONSTANT", /*  99 */
  "ZEND_DECLARE_FUNCTION_OR_CLASS", /*  100 */
  "ZEND_EXT_STMT", /*  101 */
  "ZEND_EXT_FCALL_BEGIN", /*  102 */
  "ZEND_EXT_FCALL_END", /*  103 */
  "ZEND_EXT_NOP", /*  104 */
  "ZEND_TICKS", /*  105 */
  "ZEND_SEND_VAR_NO_REF", /*  106 */
};
 
static const int NUM_OPCODES = sizeof(opcodes) / sizeof(opcodes[0]);

const char* getOpcodeName(int op)
{
  if (op < 0 || op >= NUM_OPCODES) {
    return "(unknown)";
  }
  return opcodes[op];
}

/* deferred_inheritance is a mapping from parent class names to
 * namearray_t structures that contain the names of all classes
 * derived from the parent class that were compiled before the
 * parent class definition was encountered... */ 
static apc_nametable_t* deferred_inheritance = 0;
static apc_nametable_t* parental_inheritors = 0;
static apc_nametable_t* non_inheritors = 0;

/* aux_list is for tracking refcounts and stuff*/
static apc_list* aux_list = 0;

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
		ZEND_DO_INHERITANCE(child, parent);

		/* all children of child inherit from base */
		inherit(parent, child);
	}
}

/* call_inherit */

static void call_inherit(char *key, void *data)
{
	zend_class_entry *ce;

    if(zend_hash_find(CG(class_table), key, strlen(key) + 1, (void **) &ce)
	        == SUCCESS) 
    {
        inherit(ce, ce);
    }
}

static char* shm_strdup(const char* s)
{
	int		n;
	char*	t;

	if (s == NULL) {
		return NULL;
	}
	n = strlen(s) + 1;
	fprintf(stderr, "shm_strdup\n");
	t = (char*) apc_sma_malloc(n);
	memcpy(t, s, n);
	return t;
}

static void* shm_memcpy(void* p, int n)
{
	void* q;
	
	if (p == NULL) {
		return NULL;
	}
	fprintf(stderr, "shm_memcpy\n");
	q = apc_sma_malloc(n);
	memcpy(q, p, n);
	return q;
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
	parental_inheritors = apc_nametable_create(97);
	non_inheritors = apc_nametable_create(97);
}

/* apc_serializer_request_shutdown: clean up this module per request */
void apc_serializer_request_shutdown()
{
	if (deferred_inheritance != 0) {
		apc_nametable_clear(deferred_inheritance, namearray_dtor);
		apc_nametable_destroy(deferred_inheritance);
		deferred_inheritance = 0;
	}
	if (parental_inheritors != 0) {
		apc_nametable_clear(parental_inheritors, NULL);
		apc_nametable_destroy(parental_inheritors);
		parental_inheritors = 0;
	}
	if (non_inheritors != 0) {
		apc_nametable_clear(non_inheritors, NULL);
		apc_nametable_destroy(non_inheritors);
		non_inheritors = 0;
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
void apc_serialize_zstring(char* string, int len);
void apc_create_zstring(char** string);
void apc_serialize_arg_types(zend_uchar* arg_types);
void apc_create_arg_types(zend_uchar** arg_types);

/* pre-compiler routines */
void apc_serialize_magic(void);
int apc_deserialize_magic(void);

/* routines for handling structures from zend_llist.h */
zend_llist* apc_copy_zend_llist(zend_llist* nlist, zend_llist* list, apc_malloc_t);
void apc_serialize_zend_llist(zend_llist* list);
void apc_deserialize_zend_llist(zend_llist* list);
void apc_create_zend_llist(zend_llist** list);

/* routines for handling structures from zend_hash.h */
HashTable* apc_copy_hashtable(HashTable* nt, HashTable* ht, void* funcptr, int datasize, apc_malloc_t);
void apc_serialize_hashtable(HashTable* ht, void* funcptr);
void apc_deserialize_hashtable(HashTable* ht, void* funcptr, int datasize);
void apc_create_hashtable(HashTable** ht, void* funcptr, int datasize);

/* routines for handling structures from zend.h */
zvalue_value* apc_copy_zvalue_value(zvalue_value* nv, zvalue_value* zv, int type, apc_malloc_t);
void apc_serialize_zvalue_value(zvalue_value* zvv, int type);
void apc_deserialize_zvalue_value(zvalue_value* zvv, int type);
zval** apc_copy_zval_ptr(zval** nzvp, zval** zvp, apc_malloc_t);
zval* apc_copy_zval(zval* nv, zval* zv, apc_malloc_t);
void apc_serialize_zval_ptr(zval** zv);
void apc_serialize_zval(zval* zv);
void apc_deserialize_zval(zval* zv);
void apc_create_zval(zval** zv);
zend_function_entry* apc_copy_zend_function_entry(zend_function_entry* nfe, zend_function_entry* zfe, apc_malloc_t);
void apc_serialize_zend_function_entry(zend_function_entry* zfe);
void apc_deserialize_zend_function_entry(zend_function_entry* zfe);
zend_property_reference* apc_copy_zend_property_reference(zend_property_reference* npr, zend_property_reference* zpr, apc_malloc_t);
void apc_serialize_zend_property_reference(zend_property_reference* zpr);
void apc_deserialize_zend_property_reference(zend_property_reference* zpr);
zend_overloaded_element* apc_copy_zend_overloaded_element(zend_overloaded_element* noe, zend_overloaded_element* zoe, apc_malloc_t);
void apc_serialize_zend_overloaded_element(zend_overloaded_element* zoe);
void apc_deserialize_zend_overloaded_element(zend_overloaded_element* zoe);
zend_class_entry* apc_copy_zend_class_entry(zend_class_entry* nce, zend_class_entry* zce, apc_malloc_t);
void apc_serialize_zend_class_entry(zend_class_entry* zce);
void apc_deserialize_zend_class_entry(zend_class_entry* zce);
void apc_create_zend_class_entry(zend_class_entry** zce);
zend_utility_functions* apc_copy_zend_utility_functions( zend_utility_functions* nuf, zend_utility_functions* zuf, apc_malloc_t);
void apc_serialize_zend_utility_functions(zend_utility_functions* zuf);
void apc_deserialize_zend_utility_functions(zend_utility_functions* zuf);
void apc_serialize_zend_utility_values(zend_utility_values* zuv);
void apc_deserialize_zend_utility_values(zend_utility_values* zuv);

/* routines for handling structures from zend_compile.h */
znode* apc_copy_znode(znode *nn, znode *zn, apc_malloc_t);
void apc_serialize_znode(znode* zn);
void apc_deserialize_znode(znode* zn);
zend_op* apc_copy_zend_op(zend_op *no, zend_op* zo, apc_malloc_t);
void apc_serialize_zend_op(zend_op* zo);
void apc_deserialize_zend_op(zend_op* zo);
zend_op_array* apc_copy_op_array(zend_op_array* noa, zend_op_array* zoa, apc_malloc_t);
void apc_serialize_zend_op_array(zend_op_array* zoa);
void apc_deserialize_zend_op_array(zend_op_array* zoa, int master);
void apc_create_zend_op_array(zend_op_array** zoa);
zend_internal_function* apc_copy_zend_internal_function(zend_internal_function* nif, zend_internal_function* zif, apc_malloc_t ctor);
void apc_serialize_zend_internal_function(zend_internal_function* zif);
void apc_deserialize_zend_internal_function(zend_internal_function* zif);
zend_overloaded_function* apc_copy_zend_overloaded_function(zend_overloaded_function* nof, zend_overloaded_function* zof, apc_malloc_t);
void apc_serialize_zend_overloaded_function(zend_overloaded_function* zof);
void apc_deserialize_zend_overloaded_function(zend_overloaded_function* zof);
zend_function *apc_copy_zend_function(zend_function* nf, zend_function* zf, apc_malloc_t);
void apc_serialize_zend_function(zend_function* zf);
void apc_deserialize_zend_function(zend_function* zf);
void apc_create_zend_function(zend_function** zf);

/* special purpose */
void apc_serialize_zend_function_table(HashTable* gft, apc_nametable_t* acc, apc_nametable_t*);
void apc_deserialize_zend_function_table(HashTable* gft, apc_nametable_t* acc, apc_nametable_t*);
void apc_serialize_zend_class_table(HashTable* gct, apc_nametable_t* acc, apc_nametable_t*);
int apc_deserialize_zend_class_table(HashTable* gct, apc_nametable_t* acc, apc_nametable_t*);


/* type: Fundamental operations */

/* SERIALIZE_SCALAR: write a scalar value to dst */
#define SERIALIZE_SCALAR(x, type) {							\
	if (dstsize - dstpos < alignword_int(sizeof(type))) {					\
		expandbuf(&dst, &dstsize, alignword_int(sizeof(type)) + dstpos);	\
	}														\
	*((type*)(dst + dstpos)) = x;							\
	dstpos += alignword_int(sizeof(type));									\
}

/* DESERIALIZE_SCALAR: read a scalar value from src */
#define DESERIALIZE_SCALAR(xp, type) {						\
	assert(srcsize - srcpos >= sizeof(type));				\
	*(xp) = *((type*)(src + srcpos));						\
	srcpos += alignword_int(sizeof(type));									\
}

/* PEEK_SCALAR: read a scalar value from src without advancing read pos */
#define PEEK_SCALAR(xp, type) {								\
	assert(srcsize - srcpos >= sizeof(type));				\
	*(xp) = *((type*)(src + srcpos));						\
}

/* STORE_BYTES: memcpy wrapper, writes to dst buffer */
#define STORE_BYTES(bytes, n) {								\
	if (dstsize - dstpos < alignword_int(n)) {								\
		expandbuf(&dst, &dstsize, alignword_int(n) + dstpos);				\
	}														\
	memcpy((char*) dst + dstpos, (char*)bytes, n);					\
	dstpos += alignword_int(n);											\
}
	
/* LOAD_BYTES: memcpy wrapper, reads from src buffer */
#define LOAD_BYTES(bytes, n) {								\
	assert(srcsize - srcpos >= n);							\
	memcpy((char*)bytes, src + srcpos, n);					\
	srcpos += alignword_int(n);											\
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

void apc_serialize_zstring(char* string, int len)
{
  /* by convention, mark null strings with a length of -1 */
  if (len == 0) {
    SERIALIZE_SCALAR(-1, int);
    return;
  }
  len = strlen(string);
  SERIALIZE_SCALAR(len, int);
  STORE_BYTES(string, len);
}

void apc_create_string(char** string)
{
  int len = 0;

  DESERIALIZE_SCALAR(&len, int);
  if (len == -1) {
    *string = NULL;
    return;
  }
  *string = (char*) emalloc(len + 1);
  LOAD_BYTES(*string, len);
  (*string)[len] = '\0';
}

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

/* precompiler routines */
void apc_serialize_magic(void)
{
	apc_serialize_string(APC_MAGIC_HEADER);
}

int apc_deserialize_magic(void)
{
	char *tmp;
	int retval;

	apc_create_string(&tmp);
	retval = strcmp(tmp,APC_MAGIC_HEADER);
	efree(tmp);
	return retval;
}
/* type: zend_llist */

static void store_zend_llist_element(void* arg, void* data)
{
	int size = *((int*)arg);
	STORE_BYTES((char*)data, size);
}

zend_llist* apc_copy_zend_llist(zend_llist* nlist, zend_llist* list, apc_malloc_t ctor)
{
	zend_llist_element *llelement;
	int firstelement = 0;

	if (list == 0) {
		nlist = 0;
		return nlist;
	}

	if (nlist == NULL) {
		fprintf(stderr, "apc_copy_zend_llist\n");
		nlist = (zend_llist*) ctor(sizeof(zend_llist));
	}

	memcpy(nlist, list, sizeof(zend_llist));
	llelement = list->head;
	while( llelement != NULL) {
		zend_llist_element *new_element;
		fprintf(stderr, "apc_copy_zend_llist - 2\n");
		new_element = (zend_llist_element*) ctor(sizeof(zend_llist_element) + nlist->size);
		if(firstelement == 0) {
			nlist->head = new_element;
			nlist->tail = new_element;
			new_element->next = NULL;
			new_element->prev = NULL;
			firstelement = 1;
		}
		else {
			new_element->prev = nlist->tail;
			nlist->tail->next = new_element;
			nlist->tail = new_element;
			new_element->next = NULL;
		}
		memcpy(new_element->data, llelement->data, nlist->size);
		llelement = llelement->next;
	}
	nlist->traverse_ptr = NULL;
	return nlist;
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

typedef void* (*copy_bucket_t)(void*, void*, void*);

HashTable* apc_copy_hashtable(HashTable* nt, HashTable* ht, void* funcptr, int datasize, apc_malloc_t ctor)
{
	Bucket *p, *np, *prev_p;
	copy_bucket_t copy_bucket;
	int firstbucket = 0;

	if (nt == NULL) {
		fprintf(stderr, "apc_copy_hashtable\n");
		nt = (HashTable*) ctor(sizeof(HashTable));
	}

	copy_bucket = (copy_bucket_t) funcptr;	
	memcpy(nt, ht, sizeof(HashTable));
	fprintf(stderr, "apc_copy_hashtable - 2\n");
	nt->arBuckets = (Bucket**) ctor(ht->nTableSize*sizeof(Bucket*));
	memset(nt->arBuckets, 0, ht->nTableSize*sizeof(Bucket*));
	nt->pInternalPointer = NULL;

	p = ht->pListHead;
	prev_p = NULL;
	np = NULL;
	while(p != NULL) {
		int nIndex;

		/* Don't ask about the arithmatic, look at the zend bucket definition */
		fprintf(stderr, "apc_copy_hashtable - 3\n");
		np = (Bucket *) ctor(sizeof(Bucket) + p->nKeyLength);

		nIndex = p->h % ht->nTableSize;
		if(nt->arBuckets[nIndex]) {
			np->pNext = nt->arBuckets[nIndex];
			np->pLast = NULL;
			np->pNext->pLast = np;
		}
		else {
			np->pNext = NULL;
			np->pLast = NULL;
		}
		nt->arBuckets[nIndex] = np;
		
		np->h = p->h;
		np->nKeyLength = p->nKeyLength;
		if( datasize == sizeof(void *)) {
			np->pDataPtr = copy_bucket(NULL, p->pData, ctor);
			np->pData = &np->pDataPtr;
		}
		else {
			np->pData = copy_bucket(NULL, p->pData, ctor);
			np->pDataPtr = NULL;
		}
		np->pListLast = prev_p;
		np->pListNext = NULL;
		memcpy(np->arKey, p->arKey, p->nKeyLength);
		if( firstbucket == 0) {
			nt->pListHead = np;
			firstbucket = 1;
		}
		prev_p = np;	
		p = p->pListNext;
	}
	nt->pListTail = np;
	return nt;
}		
		
	
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
	SERIALIZE_SCALAR(ht->pDestructor, void*);
	SERIALIZE_SCALAR(ht->nNumOfElements, uint);
	SERIALIZE_SCALAR(ht->persistent, int);

	/* Iterate through the buckets of the hash, serializing as we go. */
	p = ht->pListHead;
	while(p != NULL) {
		SERIALIZE_SCALAR(p->h, ulong);
		SERIALIZE_SCALAR(p->nKeyLength,uint);
		apc_serialize_zstring(p->arKey, p->nKeyLength);
		serialize_bucket(p->pData); 
		p = p->pListNext;
	}
}

void apc_deserialize_hashtable(HashTable* ht, void* funcptr, int datasize)
{
	char exists;
	uint nSize;
	dtor_func_t pDestructor;
	uint nNumOfElements;
	int persistent;
	int j;
	ulong h;
	uint nKeyLength;
	char* arKey;
	void* pData;
	int status;
	void (*deserialize_bucket)(void*);

	deserialize_bucket = (void(*)(void*)) funcptr;

	DESERIALIZE_SCALAR(&exists, char);
	assert(exists != 0);

	DESERIALIZE_SCALAR(&nSize, uint);
	DESERIALIZE_SCALAR(&pDestructor, void*);
	DESERIALIZE_SCALAR(&nNumOfElements,uint);
	DESERIALIZE_SCALAR(&persistent, int);
	
	/* Although the hash is already allocated (we're a deserialize, not a 
	 * create), we still need to initialize it. If this fails, something 
	 * very very bad happened. */
	status = zend_hash_init(ht, nSize, NULL, pDestructor, persistent);
	assert(status != FAILURE);
	
	/* Luckily, the number of elements in a hash is part of its struct, so
	 * we can just deserialize that many hashtable elements. */

	for (j = 0; j < nNumOfElements; j++) {
		DESERIALIZE_SCALAR(&h, ulong);
		DESERIALIZE_SCALAR(&nKeyLength, uint);
		apc_create_string(&arKey);
		deserialize_bucket(&pData);

		/* If nKeyLength is non-zero, this element is a hashed key/value
		 * pair. Otherwise, it is an array element with a numeric index */

		if (nKeyLength != 0) {
			if(datasize == sizeof(void*)) {
				zend_hash_add_or_update(ht, arKey, nKeyLength, &pData,
				                        datasize, NULL, HASH_ADD);
			}
			else {
				zend_hash_add_or_update(ht, arKey, nKeyLength, pData,
				                        datasize, NULL, HASH_ADD);
			}
		}
		else {	/* numeric index, not key string */
			if(datasize == sizeof(void*)) {
				zend_hash_index_update(ht, h, &pData, datasize, NULL);
			}
			else {
				zend_hash_index_update(ht, h, pData, datasize, NULL);
			}
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

zvalue_value* apc_copy_zvalue_value(zvalue_value* nv, zvalue_value* zv, int type, apc_malloc_t ctor)
{
	if (nv == NULL) {
		fprintf(stderr, "apc_copy_zvalue_value\n");
		nv = (zvalue_value *) ctor(sizeof(zvalue_value));
	}
  switch (type) {
    case IS_RESOURCE:
    case IS_BOOL:
    case IS_LONG:
    nv->lval = zv->lval;
    break;
    case IS_DOUBLE:
    nv->dval = zv->dval;
    break;
    case IS_NULL:
    /* null value, do nothing */
    break;
    case IS_CONSTANT:
    case IS_STRING:
    case FLAG_IS_BC:
		nv->str.val = apc_vmemcpy(zv->str.val, zv->str.len + 1, ctor);
		nv->str.len = zv->str.len;
    break;
    case IS_ARRAY:
    case IS_CONSTANT_ARRAY:
		nv->ht = apc_copy_hashtable(NULL, zv->ht, apc_copy_zval_ptr, sizeof(void*), ctor); // FIXME
    break;
    case IS_OBJECT:
		nv->obj.ce = apc_copy_zend_class_entry(NULL, zv->obj.ce, ctor);
		nv->obj.properties = apc_copy_hashtable(NULL, zv->obj.properties, apc_copy_zval_ptr, sizeof(void*), ctor); // FIXME
    break;
    default:
    /* The above list enumerates all types.  If we get here,
     * something very very bad has happened. */
    assert(0);
  }
	return nv;
}

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
		apc_serialize_zstring(zv->str.val, zv->str.len);
		SERIALIZE_SCALAR(zv->str.len, int);
		break;
	  case IS_ARRAY:
		apc_serialize_hashtable(zv->ht, apc_serialize_zval_ptr);
		break;
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
		apc_create_hashtable(&zv->ht, apc_create_zval, sizeof(void*));
		break;
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

zval* apc_copy_zval(zval* nv, zval* zv, apc_malloc_t ctor)
{
	if(nv == NULL) {
		fprintf(stderr, "apc_copy_zval\n");
		nv = (zval*) ctor(sizeof(zval));
	}
	memcpy(nv, zv, sizeof(zval));
	apc_copy_zvalue_value(&nv->value, &zv->value, zv->type, ctor);
	return nv;
}

zval** apc_copy_zval_ptr(zval** nzvp, zval** zvp, apc_malloc_t ctor)
{
	if (nzvp == NULL) {
		*nzvp = apc_copy_zval(NULL, *zvp, ctor);
		return nzvp;
	}
	else {
		*nzvp = apc_copy_zval(*nzvp, *zvp, ctor);
		return nzvp;
	}
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

zend_function_entry* apc_copy_zend_function_entry(zend_function_entry* nfe, zend_function_entry* zfe, apc_malloc_t ctor)
{
	if (nfe == NULL) {
		fprintf(stderr, "apc_copy_zend_function_entry\n");
		nfe = (zend_function_entry*) ctor(sizeof(zend_function_entry));
	}
	nfe->fname = apc_vstrdup(zfe->fname, ctor);
	nfe->handler = zfe->handler;
	
	if (zfe->func_arg_types) {
		nfe->func_arg_types = apc_vmemcpy(zfe->func_arg_types, zfe->func_arg_types[0] + 1, ctor);
	}
	else {
		nfe->func_arg_types = 0;
	}

	return nfe;
}

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

zend_property_reference* apc_copy_zend_property_reference(zend_property_reference* npr, zend_property_reference* zpr, apc_malloc_t ctor)
{
	if (npr == NULL) {
		fprintf(stderr, "apc_copy_zend_property_reference\n");
		npr = (zend_property_reference*) ctor(sizeof(zend_property_reference));
	}
	npr->type = zpr->type;
	npr->object = apc_copy_zval(NULL, zpr->object, ctor);
	npr->elements_list = apc_copy_zend_llist(NULL, zpr->elements_list, ctor);
	return npr;
}

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

zend_overloaded_element* apc_copy_zend_overloaded_element(zend_overloaded_element* noe, zend_overloaded_element* zoe, apc_malloc_t ctor)
{
	if ( noe == NULL) {
		fprintf(stderr, "apc_copy_zend_overloaded_element\n");
		noe = (zend_overloaded_element*) ctor(sizeof(zend_overloaded_element));
	}
	noe->type = zoe->type;
	apc_copy_zval(&noe->element, &zoe->element, ctor);
	return noe;
}

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

zend_class_entry* apc_copy_zend_class_entry(zend_class_entry* nce, zend_class_entry* zce, apc_malloc_t ctor)
{
	zend_function_entry* zfe;
	int i, count;
	
	if( nce == NULL) {
		fprintf(stderr, "apc_copy_zend_class_entry\n");
		nce = (zend_class_entry*) ctor(sizeof(zend_class_entry));
	}
	memcpy(nce, zce, sizeof(zend_class_entry));
	nce->name = apc_vstrdup(zce->name, ctor);
	if(zce->parent) {
//	FIXME this is for tracking parents for our inherit function
//		nce->parent_name = shm_strdup(zce->parent_name);
	}
	nce->refcount = apc_vmemcpy(zce->refcount, sizeof(zce->refcount[0]), ctor);
	apc_copy_hashtable(&nce->function_table, &zce->function_table, apc_copy_zend_function, sizeof(zend_function), ctor); /* FIXME */
	apc_copy_hashtable(&nce->default_properties, &zce->default_properties, apc_copy_zval_ptr, sizeof(void*), ctor); /* FIXME */
  count = 0;
  if (zce->builtin_functions) {
    for (zfe = zce->builtin_functions; zfe->fname != NULL; zfe++) {
      count++;
    }
  }
	nce->builtin_functions = (zend_function_entry*)
	fprintf(stderr, "apc_copy_zend_class_entry - 2\n");
      ctor((count+1) * sizeof(zend_function_entry));
	for (i = 0; i < count; i++) {
		apc_copy_zend_function_entry(&nce->builtin_functions[i], 
			&zce->builtin_functions[i], ctor);
	}
	return nce;
}
	
void apc_serialize_zend_class_entry(zend_class_entry* zce)
{
	zend_function_entry* zfe;
	int count, i, exists;

	SERIALIZE_SCALAR(zce->type, char);
	apc_serialize_zstring(zce->name, zce->name_length);
	SERIALIZE_SCALAR(zce->name_length, uint);

	/* Serialize the name of this class's parent class (if it has one)
	 * so that we can perform inheritance during deserialization (see
	 * apc_deserialize_zend_class_entry). */
	
 	exists = (zce->parent != NULL) ? 1 : 0;
    SERIALIZE_SCALAR(exists, char);
	if (exists) {
		apc_serialize_zstring(zce->parent->name, zce->parent->name_length);
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
		namearray_t *children;
		int minSize;

		if((children = apc_nametable_retrieve(deferred_inheritance,
			parent_name)) == 0) {
			children = (namearray_t*) malloc(sizeof(namearray_t));
			children->length = 0;
			children->size = zce->name_length + 1;
			children->strings = (char *) malloc(children->size);
			apc_nametable_insert(deferred_inheritance, parent_name, children);
		}
		minSize = children->length + zce->name_length + 1;
		if ( minSize > children->size) {
			while(minSize > children->size) {
				children->size *= 2;
			}
			children->strings = apc_erealloc(children->strings, children->size);
		}
		memcpy(children->strings + children->length, zce->name, zce->name_length
		+ 1);
		children->length += zce->name_length + 1;
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

	/* inherit(zce, zce); */
}

void apc_create_zend_class_entry(zend_class_entry** zce)
{
	*zce = (zend_class_entry*) emalloc(sizeof(zend_class_entry));
	apc_deserialize_zend_class_entry(*zce);
}


/* type: zend_utility_functions */

zend_utility_functions* apc_copy_zend_utility_functions( zend_utility_functions* nuf, zend_utility_functions* zuf, apc_malloc_t ctor)
{
	if( nuf == NULL) {
		fprintf(stderr, "apc_copy_zend_utility_functions\n");
		nuf = (zend_utility_functions*) ctor(sizeof(zend_utility_functions));
	}
	memcpy(nuf, zuf, sizeof(zend_utility_functions));
	return nuf;
}

void apc_serialize_zend_utility_functions(zend_utility_functions* zuf)
{
	SERIALIZE_SCALAR(zuf->error_function, void*);
	SERIALIZE_SCALAR(zuf->printf_function, void*);
	SERIALIZE_SCALAR(zuf->write_function, void*);
	SERIALIZE_SCALAR(zuf->fopen_function, void*);
	SERIALIZE_SCALAR(zuf->message_handler, void*);
	SERIALIZE_SCALAR(zuf->block_interruptions, void*);
	SERIALIZE_SCALAR(zuf->unblock_interruptions, void*);
	SERIALIZE_SCALAR(APC_ZEND_GET_INI_ENTRIES, void*);
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
	DESERIALIZE_SCALAR(&APC_ZEND_GET_INI_ENTRIES, void*);
	DESERIALIZE_SCALAR(&zuf->ticks_function, void*);
}


/* type: zend_utility_values */
/* FIXME  not used? */

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

znode* apc_copy_znode(znode *nn, znode *zn, apc_malloc_t ctor)
{
	if(nn == NULL) {
		fprintf(stderr, "apc_copy_znode\n");
		nn = (znode *) ctor(sizeof(znode));
	}
	switch(zn->op_type) {
    case IS_CONST:
		// FIXME
		apc_copy_zval(&nn->u.constant, &zn->u.constant, ctor);
    break;
    default:
    memcpy(&nn->u, &zn->u, sizeof(zn->u));
    break;
  }
	return nn;
}

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

zend_op* apc_copy_zend_op(zend_op *no, zend_op* zo, apc_malloc_t ctor)
{	
	if ( no == NULL) {
		fprintf(stderr, "apc_copy_zend_op\n");
		no = (zend_op *) ctor(sizeof(zend_op));
	}
	/* Do a copy first then overwrite any pointers */
	memcpy(no, zo, sizeof(zend_op));
	apc_copy_znode(&no->result, &zo->result, ctor);
	apc_copy_znode(&no->op1, &zo->op1, ctor);
	apc_copy_znode(&no->op2, &zo->op2, ctor);
	return no;
}	

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


zend_op_array* apc_copy_op_array(zend_op_array* noa, zend_op_array* zoa, apc_malloc_t ctor)
{
	int i;

	if ( noa == NULL) {
		fprintf(stderr, "apc_copy_op_array\n");
		noa = (zend_op_array*) ctor(sizeof(zend_op_array));
	}
	memcpy(noa, zoa, sizeof(zend_op_array));

	if (zoa->arg_types) {
		noa->arg_types = apc_vmemcpy(zoa->arg_types, zoa->arg_types[0] + 1, ctor);
	}
	else {
		noa->arg_types = 0;
	}

	noa->function_name = apc_vstrdup(zoa->function_name, ctor);
	noa->refcount = apc_vmemcpy(zoa->refcount, sizeof(zoa->refcount[0]), ctor);
	fprintf(stderr, "apc_copy_op_array - 2\n");
	noa->opcodes = (zend_op *) ctor(sizeof(zend_op)* zoa->last);
	for(i = 0; i < zoa->last; i++) {
		apc_copy_zend_op(&noa->opcodes[i], &zoa->opcodes[i], ctor);
	}
	noa->filename = apc_vstrdup(zoa->filename, ctor);
	noa->reserved[0] = (void *) 1;
	return noa;
}

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
#ifdef APC_MUST_DEFINE_START_OP /*  Introduced in php-4.0.7 */
	assert(zoa->start_op == NULL);
#endif
	SERIALIZE_SCALAR(zoa->return_reference, zend_bool);
	SERIALIZE_SCALAR(zoa->done_pass_two, zend_bool);
	apc_serialize_string(zoa->filename);
	/* zend_op_array.reserved is not used */
}

void apc_deserialize_zend_op_array(zend_op_array* zoa, int master)
{
	char *fname;
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

				exists = 1;
				
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
					zend_class_entry *ce;
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
					*class_name++ = '\0';	/* advance past ':' */

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

					/* a deferred class can't be a top-level parent */
					apc_nametable_insert(non_inheritors, class_name, NULL);


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
#ifdef APC_MUST_DEFINE_START_OP /*  Introduced in php 4.0.7 */
	zoa->start_op = NULL;
#endif
	DESERIALIZE_SCALAR(&zoa->return_reference, zend_bool);
	DESERIALIZE_SCALAR(&zoa->done_pass_two, zend_bool);
	apc_create_string(&fname);
	zoa->filename = zend_set_compiled_filename(fname);
	efree(fname);
	if(master) {
		apc_nametable_difference(parental_inheritors, non_inheritors);
		apc_nametable_apply(parental_inheritors, call_inherit);
	}
}

void apc_create_zend_op_array(zend_op_array** zoa)
{
	*zoa = (zend_op_array*) emalloc(sizeof(zend_op_array));
	apc_deserialize_zend_op_array(*zoa, 0);
}


/* type: zend_internal_function */

zend_internal_function* apc_copy_zend_internal_function(zend_internal_function* nif, zend_internal_function* zif, apc_malloc_t ctor)
{
	if ( nif == NULL ) {
		fprintf(stderr, "apc_copy_zend_internal_function\n");
		nif = (zend_internal_function*) ctor(sizeof(zend_internal_function));
	}
	nif->type = zif->type;
	if( zif->arg_types ) {
		nif->arg_types = apc_vmemcpy(zif->arg_types, zif->arg_types[0] + 1, ctor);
	}
	else {
		nif->arg_types = 0;
	}
	nif->function_name = apc_vstrdup(zif->function_name, ctor);
	nif->handler = zif->handler;
	return nif;
}

	
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

zend_overloaded_function* apc_copy_zend_overloaded_function(zend_overloaded_function* nof, zend_overloaded_function* zof, apc_malloc_t ctor)
{
	if(nof == NULL) {
		fprintf(stderr, "apc_copy_zend_overloaded_function\n");
		nof = (zend_overloaded_function*) ctor(sizeof(zend_overloaded_function));
	}
	nof->type = zof->type;

	if (zof->arg_types) {
		nof->arg_types = apc_vmemcpy(zof->arg_types, zof->arg_types[0] + 1, ctor);
	}
	else {
		nof->arg_types = 0;
	}

	nof->function_name = apc_vstrdup(zof->function_name, ctor);
	nof->var = zof->var;
	return nof;
}

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

zend_function *apc_copy_zend_function(zend_function* nf, zend_function* zf, apc_malloc_t ctor)
{
	if(nf == NULL) {
		fprintf(stderr, "apc_copy_zend_function\n");
		nf = (zend_function*) ctor(sizeof(zend_function));
	}
  switch(zf->type) {
    case ZEND_INTERNAL_FUNCTION:
    apc_copy_zend_internal_function(&nf->internal_function, &zf->internal_function, ctor);
    break;
    case ZEND_OVERLOADED_FUNCTION:
    apc_copy_zend_overloaded_function(&nf->overloaded_function, &zf->overloaded_function, ctor);
    break;
    case ZEND_USER_FUNCTION:
    case ZEND_EVAL_CODE:
    apc_copy_op_array(&nf->op_array, &zf->op_array, ctor);
    break;
    default:
    /* the above are all valid zend_function types.  If we hit this
     * case something has gone very very wrong. */
    assert(0);
  }
	return nf;
}

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
		apc_deserialize_zend_op_array(&zf->op_array, 0);
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
	/* do not serialize anonymous functions */
	if (hash_key->nKeyLength == 0 || !strncmp(hash_key->arKey, "__lambda", 8) || (hash_key->arKey[0] == '\0' && !(strncmp(hash_key->arKey + 1, "lambda", 6)))) {
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
		/*  This can validly happen. */
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
		if(zc->parent != NULL) {
			apc_nametable_insert(priv, zc->name, 0);
		}
	}

	return 0;
}

void apc_serialize_zend_class_table(HashTable* gct,
	apc_nametable_t* acc, apc_nametable_t* priv)
{
	zend_hash_apply_with_arguments(gct, store_class_table, 2, acc, priv);
	SERIALIZE_SCALAR(0, char);
}

int apc_deserialize_zend_class_table(HashTable* gct, apc_nametable_t* acc, apc_nametable_t* priv)
{
	char exists;
	int i;
	zend_class_entry* zc;

	i = 0;

	DESERIALIZE_SCALAR(&exists, char);
	while (exists) {
		apc_create_zend_class_entry(&zc);
		if (zend_hash_add(gct, zc->name, zc->name_length + 1,
			zc, sizeof(zend_class_entry), NULL) == FAILURE)
		{
		/*  This can validly happen. */
		}
		/* We may only want to do this insert if zc->parent is NULL */
		apc_nametable_insert(parental_inheritors, zc->name, NULL);
		apc_nametable_insert(acc, zc->name, 0);
		apc_nametable_insert(priv, zc->name, 0);
		DESERIALIZE_SCALAR(&exists, char);
		i++;
	}
	return i;
}

