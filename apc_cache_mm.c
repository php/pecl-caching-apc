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


#include "apc_cache_mm.h"
#include "php_apc.h"

extern zend_apc_globals apc_globals;

void apc_mmap_dump(apc_outputfn_t outputfn, HashTable* cache)
{
	Bucket *p;
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
	outputfn("<tr>\n");
	outputfn("<td bgcolor=#eeeeee>Regex Exclude Text</td>\n");
    	outputfn("<td bgcolor=#eeeeee>%s</td>\n", APCG(regex_text) ? APCG(regex_text) : "(none)");
	outputfn("</table>\n");
	outputfn("<br>\n");
	outputfn("<br>\n");

 /* display bucket info */
	outputfn("<table border=1 cellpadding=2 cellspacing=1>\n");
	outputfn("<tr>\n");
	outputfn("<td colspan=5 bgcolor=#dde4ff>Child Cache Data</td>\n");
	outputfn("<tr>\n");
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
		outputfn("<td bgcolor=#eeeeee>%s</td>\n", p->arKey);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", in_elem->inputlen);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", in_elem->mtime);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", in_elem->hitcounter);
		outputfn("<td bgcolor=#eeeeee>%d</td>\n", in_elem->inode);
		outputfn("</tr>\n");
		p = p->pListNext;
	}
	outputfn("</table>\n");
	outputfn("<br>\n");
	outputfn("<br>\n");
}	


	
