#include "apc_cache_mm.h"

void apc_mmap_dump(apc_outputfn_t outputfn, HashTable* cache)
{
	Bucket *p;
	outputfn("<html>\n<head>\n\t<title>APC-MMAP Cache Info\n</title>\n</head>\n");
	outputfn("<table BORDER=0 CELLSPACING=0 CELLPADDING=4 WIDTH=\"100%\" >\n");
	outputfn("<tr><td>\n");
	outputfn("<table BORDER=0 CELLSPACING=0 CELLPADDING=0 WIDTH=\"98%\" 
						BGCOLOR=\"#006666\" >");
	outputfn("<tr BGCOLOR=\"#0000FF\">\n");
	outputfn("<td>Filename</td><td>APC CacheFile</td><td>Inode</td><td>
						Cached-Since</td>\n");
	outputfn("</tr>\n");
	p = cache->pListHead;
	while(p !=NULL) {
		struct mm_fl_element *in_elem;
		in_elem = (struct mm_fl_element *) p->pData;
		outputfn("<tr>\n");
		outputfn("<td>%s</td><td>%s</td><td>%d</td><td>%ld</td>\n", p->arKey, 
							in_elem->cache_filename, in_elem->inode, in_elem->mtime);
		outputfn("</tr>\n");
		p = p->pListNext;
	}
	outputfn("</table>\n");
	outputfn("</td> </tr>\n");
	outputfn("</table>\n");
	outputfn("</html>\n");
}

	
	
