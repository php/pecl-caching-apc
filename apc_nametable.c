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

#include "apc_nametable.h"
#include "apc_lib.h"
#include <string.h>

typedef struct link_t link_t;
struct link_t {
	char* key;			/* the hashed key */
	void* value;		/* associated value */
	link_t* next;		/* the next bucket in the chain */
};

struct apc_nametable_t {
	int nbuckets;		/* the number of buckets in the table */
	link_t** buckets;	/* array of bucket links */
};

/* newlink: create and initialize a new link_t instance */
static link_t* newlink(const char* key, void* value, link_t* next)
{
	link_t* link = (link_t*) malloc(sizeof(link_t));
	link->key = apc_estrdup(key);
	link->value = value;
	link->next = next;
	return link;
}

/* deletelink: destroy a link_t instance returned by newlink() */
static void deletelink(link_t* link)
{
	free(link->key);
	free(link);
}

/* hash: compute hash value of a string */
static unsigned int hash(const char* v)
{
	unsigned int h = 0;
	for (; *v != 0; v++) {
		h = 127*h + *v;
	}
	return h;
}

/* apc_nametable_create: create a new name table of the specified size */
apc_nametable_t* apc_nametable_create(int nbuckets)
{
	apc_nametable_t* table =
		(apc_nametable_t*) malloc(sizeof(apc_nametable_t));
	table->nbuckets = nbuckets;
	table->buckets = (link_t**) malloc(nbuckets * sizeof(link_t*));
	memset(table->buckets, 0, nbuckets * sizeof(link_t*));
	return table;
}

/* apc_nametable_destroy: free all memory associated with a name table */
void apc_nametable_destroy(apc_nametable_t* table)
{
	apc_nametable_clear(table, 0);
	free(table->buckets);
	free(table);
}

/* apc_nametable_insert: add a new key to the table if it doesn't exist */
int apc_nametable_insert(apc_nametable_t* table, const char* key, void* value)
{
	link_t** slot = &table->buckets[hash(key) % table->nbuckets];

	while (*slot != 0 && strcmp((*slot)->key, key) != 0) {
		slot = &((*slot)->next);
	}
	if (*slot != 0) {
		return 0;
	}
	*slot = newlink(key, value, 0);
	return 1;
}

/* apc_nametable_search: return true is a key exists in table */
int apc_nametable_search(apc_nametable_t* table, const char* key)
{
	link_t* slot = table->buckets[hash(key) % table->nbuckets];

	while (slot != 0 && strcmp(slot->key, key) != 0) {
		slot = slot->next;
	}
	return (slot != 0); /* if (slot != 0), strcmp returned 0 */
}

/* apc_nametable_search: return true is a key exists in table */
void* apc_nametable_retrieve(apc_nametable_t* table, const char* key)
{
	link_t* slot = table->buckets[hash(key) % table->nbuckets];

	while (slot != 0 && strcmp(slot->key, key) != 0) {
		slot = slot->next;
	}
	return (slot != 0) ? slot->value : 0;
}

/* apc_nametable_remove: remove a key from the table */
int apc_nametable_remove(apc_nametable_t* table, const char* key)
{
	link_t* q;
	link_t** slot = &table->buckets[hash(key) % table->nbuckets];

	while (*slot != 0 && strcmp((*slot)->key, key) != 0) {
		slot = &((*slot)->next);
	}
	if (*slot == 0) {
		return 0;
	}
	q = *slot;
	*slot = (*slot)->next;
	deletelink(q);
	return 1;
}

/* apc_nametable_clear: remove all keys from the table */
void apc_nametable_clear(apc_nametable_t* table,
	apc_nametable_destructor_t destructor)
{
	int i;

	for (i = 0; i < table->nbuckets; i++) {
		link_t* p = table->buckets[i];
		while (p != 0) {
			link_t* q = p;
			p = p->next;
			if (destructor != 0) {
				destructor(q->value);
			}
			deletelink(q);
		}
	}
	memset(table->buckets, 0, table->nbuckets * sizeof(link_t*));
}

/* apc_nametable_union: add elements in a to b */
void apc_nametable_union(apc_nametable_t* a, apc_nametable_t* b)
{
	int i;

	for (i = 0; i < b->nbuckets; i++) {
		link_t* p = b->buckets[i];
		while (p != 0) {
			apc_nametable_insert(a, p->key, p->value);
			p = p->next;
		}
	}
}

/* apc_nametable_difference: remove elements in b from a */
void apc_nametable_difference(apc_nametable_t* a, apc_nametable_t* b)
{
	int i;

	for (i = 0; i < b->nbuckets; i++) {
		link_t* p = b->buckets[i];
		while (p != 0) {
			apc_nametable_remove(a, p->key);
			p = p->next;
		}
	}
}

/* apc_nametable_size: return number of elements of table */
int apc_nametable_size(apc_nametable_t* table)
{
	int i;
	int size;	/* logical size */

	size = 0;
	for (i = 0; i < table->nbuckets; i++) {
		link_t* p = table->buckets[i];
		while (p != 0) {
			size++;
			p = p->next;
		}
	}
	return size;
}

/* apc_nametable_dump: debug display of nametable */
void apc_nametable_dump(apc_nametable_t* table, apc_outputfn_t outputfn)
{
	int i;

	for (i = 0; i < table->nbuckets; i++) {
		link_t* p = table->buckets[i];
		while (p != 0) {
			link_t* q = p;
			p = p->next;
			outputfn("nametable contains %s", q->key);
		}
	}
}
