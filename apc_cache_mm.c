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


#include "apc_cache_mm.h"
#include "apc_nametable.h"
#include "apc_fcntl.h"
#include "php_apc.h"
#include "apc_serialize.h"
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>

extern zend_apc_globals apc_globals;

/* apc_generate_cache_filename returns the path of the compiled 
 * data file associated with a php file. If no cachedir is specified, 
 * cache files will be created in the same dir as their parent cache file 
 */

char *apc_generate_cache_filename(const char *filename)
{
  static char cache_filename[1024];

  if(APCG(cachedir) == NULL)
  {
    snprintf(cache_filename, sizeof(cache_filename), "%s_apc", filename);
    return cache_filename;
  }
  else
  {
        snprintf(cache_filename, sizeof(cache_filename), "%s/%s_apc",
        APCG(cachedir),filename);
        return cache_filename;
  }
}

/* apc_mmap_dump recurses through the called child's cache table,
 * displaying the objects and their hit counts */

void apc_mmap_dump(HashTable* cache, const char *linkurl, apc_outputfn_t outputfn)
{
	Bucket *p;
	int j;
 /* display HEAD */
	outputfn("<head>\n");
	outputfn("<title>APC-MMAP Cache Info</title>\n");
	outputfn("</head>\n");

 /* display global cache information */
	outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
	outputfn("<tr>\n");
	outputfn("<td colspan=2 bgcolor=#dde4ff>Global Cache Configuration</td>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#ffffff>Name</td>\n");
	outputfn("<td bgcolor=#ffffff>Value</td>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>Global TTL</td>\n");
	outputfn("<td bgcolor=#eeeeee>%d</td>\n", APCG(ttl) ? APCG(ttl) : 0 );

	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>Root Cache Dir</td>\n");
	outputfn("<td bgcolor=#eeeeee>%s</td>\n", APCG(cachedir) ? APCG(cachedir) : "(none)");
	for(j = 0; j < APCG(nmatches); j++) {
	outputfn("<tr>\n");
		outputfn("<td bgcolor=#eeeeee>Regex Exclude Text (%d)</td>\n", j);
    	outputfn("<td bgcolor=#eeeeee>%s</td>\n", 
			APCG(regex_text)[j]?APCG(regex_text)[j] : "(none)");
	}
	outputfn("</table>\n");
	outputfn("<br>\n");
	outputfn("<br>\n");

 /* display bucket info */
	outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
	outputfn("<tr><form method=post action=%s>\n", linkurl);
	outputfn("<tr>\n");
	outputfn("<td colspan=6 bgcolor=#dde4ff>Child Cache Data</td>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#ffffff>Delete</td>\n");
	outputfn("<td bgcolor=#ffffff>Key</td>\n");
	outputfn("<td bgcolor=#ffffff>Length</td>\n");
	outputfn("<td bgcolor=#ffffff>Last ModTime(B)</td>\n");
	outputfn("<td bgcolor=#ffffff>Hits</td>\n");
	outputfn("<td bgcolor=#ffffff>Inode</td>\n");
	p = cache->pListHead;
  	while(p !=NULL) {
		struct mm_fl_element *in_elem;
		in_elem = (struct mm_fl_element *) p->pData;
		outputfn("<tr>\n");

	 	if (linkurl != 0) {
            outputfn("<td bgcolor=#eeeeee><input type=checkbox "
			         "name=apc_rm[] value=%s>&nbsp</td>\n",
					 p->arKey);
            outputfn("<td bgcolor=#eeeeee><a href=%s?apc_info=%s>"
			         "%s</a></td>\n", linkurl, p->arKey, p->arKey);
        }
        else {
            outputfn("<td bgcolor=#eeeeee>&nbsp</td>\n");
            outputfn("<td bgcolor=#eeeeee>%s</td>\n", p->arKey);
        }
		
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", in_elem->inputlen);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", in_elem->mtime);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", in_elem->hitcounter);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", in_elem->inode);
		outputfn("</tr>\n");
		p = p->pListNext;
	}
	if(linkurl != 0 ) {
        outputfn("<tr><td><input type=submit name=submit "
		         "value=\"Delete Marked Objects\"></tr></td>\n");
    }
    outputfn("</form>\n");
	outputfn("</table>\n");
	outputfn("<br>\n");
	outputfn("<br>\n");
}	
int apc_mmap_dump_entry(const char* filename, apc_outputfn_t outputfn)
{

    static const char NBSP[] = "&nbsp;";
    int i, n, fd, numclasses;
		char* input;
		char* cache_filename;

    HashTable function_table;
    HashTable class_table;
    apc_nametable_t* dummy;
    zend_op_array* op_array;
	struct stat statbuf;
    Bucket* p;
    Bucket* q;
	
	cache_filename = apc_generate_cache_filename(filename);
	op_array = (zend_op_array*) malloc(sizeof(zend_op_array));
	zend_hash_init(&function_table, 13, NULL, NULL, 1);
	zend_hash_init(&class_table, 13, NULL, NULL, 1);
	dummy = apc_nametable_create(97);

    if( (n = stat(cache_filename, &statbuf)) < 0) {
        outputfn("error: '%s' is not in the cache\n", filename);
        return 1;
    }
    if( (fd = open(cache_filename, O_RDONLY)) < 0) {
        outputfn("error: '%s' is not in the cache\n", filename);
        return 1;
    }
    readw_lock(fd, 0, SEEK_SET, 0);
    input = (char *) mmap(0, statbuf.st_size,  PROT_READ,
        MAP_SHARED, fd, 0);
    close(fd);
    un_lock(fd, 0, SEEK_SET, 0);

    apc_init_deserializer(input, statbuf.st_size);
	apc_deserialize_magic();
    apc_deserialize_zend_function_table(&function_table, dummy, dummy);
    numclasses = apc_deserialize_zend_class_table(&class_table, dummy, dummy);
    apc_deserialize_zend_op_array(op_array, numclasses);
    munmap(input, statbuf.st_size);

    /* begin outer table */
    outputfn("<table border=0 cellpadding=2 cellspacing=1>\n");

  /* begin first row of outer table */
    outputfn("<tr>\n");

    /* display bucket info */
    outputfn("<td colspan=3 valign=top>");
    outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
    outputfn("<tr>\n");
    outputfn("<td colspan=3 bgcolor=#dde4ff>Bucket Data</td>\n");
    outputfn("<tr>\n");
    outputfn("<td bgcolor=#ffffff>Scriptname</td>\n");
    outputfn("<td bgcolor=#ffffff>Length (B)</td>\n");
    outputfn("<td bgcolor=#ffffff>Create Time</td>\n");
    outputfn("<tr>\n");
    outputfn("<td bgcolor=#eeeeee>%s</td>\n", filename);
    outputfn("<td bgcolor=#eeeeee>%d</td>\n", statbuf.st_size);
    outputfn("<td bgcolor=#eeeeee>%d</td>\n", statbuf.st_mtime);
    outputfn("</table>\n");
    outputfn("</td>\n");

    /* end first row of outer table */
    outputfn("</tr>\n");


    /* deserialize bucket and see what's inside */

    /* begin second row of outer table */
    outputfn("<tr>\n");

    /* display opcodes in the entry */
    outputfn("<td valign=top>");
    outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
    outputfn("<tr>\n");
    outputfn("<td colspan=3 bgcolor=#dde4ff>OpCodes</td>\n");
    outputfn("<tr>\n");
    outputfn("<td bgcolor=#ffffff>Value</td>\n");
    outputfn("<td bgcolor=#ffffff>Extended</td>\n");
	outputfn("<td bgcolor=#ffffff>Line</td>\n");
    for (i = 0; i < op_array->last; i++) {
        const char* name;

        outputfn("<tr>\n");

        /* print the regular opcode, or '&nbsp;' if empty */
        name = apc_get_zend_opname(op_array->opcodes[i].opcode);
        outputfn("<td bgcolor=#eeeeee>%s</td>\n", name);

        /* print the extended opcode, or '&nbsp;' if empty */
        if (op_array->opcodes[i].opcode != ZEND_NOP &&
            op_array->opcodes[i].opcode != ZEND_DECLARE_FUNCTION_OR_CLASS)
        {
            /* this opcode does not have an extended value */
            name = NBSP;
        }
        else {
            name = apc_get_zend_extopname(op_array->opcodes[i].extended_value);
        }
        outputfn("<td bgcolor=#eeeeee>%s</td>\n", name);

        /* print the line number of the opcode */
        outputfn("<td bgcolor=#eeeeee>%u</td>\n", op_array->opcodes[i].lineno);
    }
    outputfn("</table>\n");
    outputfn("</td>\n");

    /* display functions in the entry */
    outputfn("<td valign=top>");
    outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
    outputfn("<tr>\n");
    outputfn("<td bgcolor=#dde4ff>Functions</td>\n");
    p = function_table.pListHead;
    while (p) {
        zend_function* zf = (zend_function*) p->pData;
        outputfn("<tr>\n");
        outputfn("<td bgcolor=#eeeeee>%s</td>\n",
            zf->common.function_name);
        p = p->pListNext;
    }
    outputfn("</table>\n");
    outputfn("</td>\n");

    /* display classes in the entry */
    outputfn("<td valign=top>");
    outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
    outputfn("<tr>\n");
    outputfn("<td colspan=2 bgcolor=#dde4ff>Classes</td>\n");
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#ffffff>Class</td>\n");
	outputfn("<td bgcolor=#ffffff>Function</td>\n");
    p = class_table.pListHead;
    while (p) {
        zend_class_entry* zc = (zend_class_entry*) p->pData;
        outputfn("<tr>\n");
        outputfn("<td bgcolor=#eeeeee>%s</td><td bgcolor=#eeeeee>&nbsp</td>\n", 
			zc->name);
		q = zc->function_table.pListHead;
		while(q) {
			zend_function* zf = (zend_function*) q->pData;
			outputfn("<tr>\n");
			outputfn("<td bgcolor=#eeeeee>&nbsp</td>\n");
			outputfn("<td bgcolor=#eeeeee>%s</td>\n",
            	zf->common.function_name);
        	q = q->pListNext;
    	}
        p = p->pListNext;
    }
    outputfn("</table>\n");
    outputfn("</td>\n");

    /* end second row of outer table */
    outputfn("</tr>\n");

    /* close outer table */
    outputfn("</table>\n");

    /* clean up */
    zend_hash_clean(&class_table);
    zend_hash_clean(&function_table);
    destroy_op_array(op_array);
    free(op_array);

    return 0;
}

int apc_cache_index_mmap(HashTable* cache, zval **hash) {
	Bucket *p;
    p = cache->pListHead;
    while(p !=NULL) {
        zval *array = NULL;
        struct mm_fl_element *in_elem;

        ALLOC_ZVAL(array);
		INIT_PZVAL(array);
        if(array_init(array) == FAILURE) {
            return 1;
        }
        in_elem = (struct mm_fl_element *) p->pData;
        add_next_index_long(array, in_elem->inputlen);
        add_next_index_long(array, in_elem->mtime);
        add_next_index_long(array, in_elem->hitcounter);
        add_next_index_long(array, in_elem->inode);
        zend_hash_update((*hash)->value.ht, p->arKey, p->nKeyLength,
            (void *) &array, sizeof(zval *), NULL);
        p = p->pListNext;
    }
    return 0;
}

int apc_cache_info_mmap(zval **hash) {
	int j;
	char buf[20];

	array_init(*hash);
	add_assoc_string(*hash, "mode", "MMAP", 1);

	add_assoc_long(*hash, "time-to-live", APCG(ttl));
	for(j = 0; j < APCG(nmatches); j++) {
		snprintf(buf, sizeof(buf)-1, "cache filter (%d)", j);
		add_assoc_string(*hash, buf,
			APCG(regex_text)[j]?APCG(regex_text)[j]:"(none)", 1);
	}
	add_assoc_string(*hash, "cache directory", APCG(cachedir)?APCG(cachedir):"/tmp", 1 );
	add_assoc_long(*hash, "check file modification times", APCG(check_mtime));
	add_assoc_long(*hash, "support relative includes", APCG(relative_includes));
	add_assoc_long(*hash, "check for compiled source", APCG(check_compiled_source));
	return 0;
}
