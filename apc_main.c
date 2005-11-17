/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2005 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
  |          Rasmus Lerdorf <rasmus@php.net>                             |
  |          Arun C. Murthy <arunc@yahoo-inc.com>                        |
  |          Gopal Vijayaraghavan <gopalv@yahoo-inc.com>                 |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$ */

#include "apc_php.h"
#include "apc_main.h"
#include "apc.h"
#include "apc_cache.h"
#include "apc_compile.h"
#include "apc_globals.h"
#include "apc_sma.h"
#include "apc_stack.h"
#include "apc_zend.h"
#include "SAPI.h"
#if PHP_API_VERSION <= 20020918
#if HAVE_APACHE
#ifdef APC_PHP4_STAT
#undef XtOffsetOf
#include "httpd.h"
#endif
#endif
#endif

/* {{{ module variables */

/* pointer to the original Zend engine compile_file function */
typedef zend_op_array* (zend_compile_t)(zend_file_handle*, int TSRMLS_DC);
static zend_compile_t *old_compile_file;
static int recursive_inheritance(zend_class_entry *class_entry);

/* }}} */

/* {{{ get/set old_compile_file (to interact with other extensions that need the compile hook) */
static zend_compile_t* set_compile_hook(zend_compile_t *ptr)
{
    zend_compile_t *retval = old_compile_file;

    if (ptr != NULL) old_compile_file = ptr;
    return retval;
}
/* }}} */

/* {{{ install_function */
static int install_function(apc_function_t fn TSRMLS_DC)
{
    int status =
        zend_hash_add(EG(function_table),
                      fn.name,
                      fn.name_len+1,
                      apc_copy_function_for_execution(fn.function),
                      sizeof(fn.function[0]),
                      NULL);

    if (status == FAILURE) {
        zend_error(E_ERROR, "Cannot redeclare %s()", fn.name);
    }

    return status;
}
/* }}} */

typedef struct apc_hash_link_t apc_hash_link_t;
struct apc_hash_link_t {
    zend_class_entry *ce;
    apc_hash_link_t *next;
};

/* {{{ install_class
 *
 * This install_class function contains mucho magico related to inheritance.
 * Think of the inheritance chain of A->B->C as in A is the root class extended
 * by B which is in turn extended by C.  The simplest case is when everything
 * happens in order.  That is, we get called on to install A, B and C in that
 * order.  For A it doesn't have a parent so we just fall through to doing a
 * zend_hash_add(A).  For B, it has parent A and the class lookup will find
 * A because we added it on the previous call.  Then zend_do_inheritance() is
 * called to inherit from A into B.  And the same thing happens when C comes
 * along.
 *
 * Now, flip things around.  Say we get called on to install class C first.
 * It has a parent, so we try to lookup the parent.  This will fail because
 * it hasn't been added yet.  We then add it to the delayed inheritance hash
 * with the key being B.  Next, B comes along.  Its parent is A which isn't
 * around so we fall into the same logic as with C.  We couldn't find the 
 * parent, so we are not allowed to do any inheritance yet.  We can only
 * add it to the delayed inheritance hash.  We therefore add the A->B
 * entry to the delayed inheritance hash.  Note that there can be multiple
 * children for a parent node, so the hash actually contains a linked list
 * of children at each parent node.  
 * 
 * -Rasmus (in a hotel room in Paris, Nov.13 2005)
 */
static int install_class(apc_class_t cl TSRMLS_DC)
{
    zend_class_entry *class_entry;
    zend_class_entry *parent;
    apc_hash_link_t  *he;
    int status;

    class_entry = apc_copy_class_entry_for_execution(cl.class_entry, cl.is_derived);

    if(cl.parent_name != NULL) {
        int parent_len = strlen(cl.parent_name);
#ifdef ZEND_ENGINE_2    
        zend_class_entry** parent_ptr = NULL;
        status = zend_lookup_class(cl.parent_name, parent_len, &parent_ptr TSRMLS_CC);
#else
        status = zend_hash_find(EG(class_table), cl.parent_name, parent_len+1, (void**) &parent);
#endif
        if(status == SUCCESS) {
#ifdef ZEND_ENGINE_2            
            parent = *parent_ptr;
#endif 
            zend_do_inheritance(class_entry, parent TSRMLS_CC);
            zend_error(E_WARNING, "Inheriting into %s from parent %s", class_entry->name, parent->name);
            status = recursive_inheritance(class_entry);
        } else {
            if(APCG(dynamic_error)) {
                zend_error(E_WARNING, "Dynamic inheritance detected on %s extending %s", class_entry->name, cl.parent_name);
            }
            apc_hash_link_t *hl = apc_php_malloc(sizeof(apc_hash_link_t *));
            int added = 0;
            char *lower_parent = estrndup(cl.parent_name, parent_len);
            zend_str_tolower(lower_parent, parent_len);
            hl->ce = class_entry;
            hl->next = NULL;
            if(APCG(delayed_inheritance_hash).nNumOfElements) {
                apc_hash_link_t **ehl;
                status = zend_hash_find(&APCG(delayed_inheritance_hash), lower_parent, parent_len+1, (void **)&ehl);
                if(status == SUCCESS) {
                    /* It's already in the hash, so add the class_entry to the linked list */
                    while((*ehl)->next) *ehl=(*ehl)->next;
                    (*ehl)->next = hl;        
                    added = 1;
                }
            }
            if(!added) {
                zend_hash_add(&APCG(delayed_inheritance_hash), lower_parent, parent_len+1, (void *)&hl, sizeof(apc_hash_link_t *), NULL);
            }
            class_entry->parent = NULL;
            efree(lower_parent);
            status = SUCCESS;
        }
    } else {
        status = SUCCESS;
        status = recursive_inheritance(class_entry);
    }
   
    return status;
}
/* }}} */

/* {{{ recursive_inheritance 
 * 
 * We can't have mucho magico without a dose of recursion to really confuse
 * things.  This function completes a node in the inheritance tree.
 * By complete I mean a node who is either a true root parent node or a sub-node
 * who has already been inherited into from a complete parent.  We check to
 * see if we have any children waiting for us on the delayed inheritance list and
 * if we do we inherit into them making each of those nodes complete.  Once we
 * have inherited into a node, we call ourselves again with this new node as the
 * complete parent node.  Once there are no more children for a node, we delete 
 * the hash entry from the delayed inheritance list and finally we add the class
 * to the class table.
 */
static int recursive_inheritance(zend_class_entry *class_entry) {
    apc_hash_link_t  **ehl;
    zend_class_entry **allocated_ce;
    int status;
    char *lower_cen = estrndup(class_entry->name, class_entry->name_length);
    zend_str_tolower(lower_cen, class_entry->name_length);
    status = zend_hash_find(&APCG(delayed_inheritance_hash), lower_cen, class_entry->name_length+1, (void **)&ehl);
    zend_error(E_WARNING, "Recursive inheritance called for %s", class_entry->name);
    if(status == SUCCESS) {
        do {
            zend_do_inheritance((*ehl)->ce, class_entry TSRMLS_CC);
            zend_error(E_WARNING, "Inheriting into %s from parent %s", (*ehl)->ce->name, class_entry->name);
            zend_error(E_WARNING, "Recursing for %s", (*ehl)->ce->name);
            status = recursive_inheritance((*ehl)->ce);
            if(status == FAILURE) {
                efree(lower_cen);
                return status;
            }
        } while(*ehl = (*ehl)->next);
        zend_hash_del(&APCG(delayed_inheritance_hash), lower_cen, class_entry->name_length+1);
    }

#ifdef ZEND_ENGINE_2    
    /* XXX: We need to free this somewhere... */
    allocated_ce = apc_php_malloc(sizeof(zend_class_entry*));    
    if(!allocated_ce) {
        efree(lower_cen);
        return FAILURE;
    }
    *allocated_ce = class_entry; 
    status = zend_hash_add(EG(class_table), lower_cen, class_entry->name_length+1, allocated_ce, sizeof(zend_class_entry*), NULL);
#else
    status = zend_hash_add(EG(class_table), lower_cen, class_entry->name_length+1, class_entry, sizeof(zend_class_entry), NULL);
#endif        
    if(status == FAILURE) {
        zend_error(E_WARNING, "Failed adding class %s", lower_cen);
    } else {
        zend_error(E_WARNING, "Added class %s", lower_cen);
    }

    efree(lower_cen);
    return status;
}
/* }}} */

/* {{{ old_install_class */
static int old_install_class(apc_class_t cl TSRMLS_DC)
{
    zend_class_entry* class_entry = cl.class_entry;
    zend_class_entry* parent;
    int status;

#ifdef ZEND_ENGINE_2    
    /*
     * XXX: We need to free this somewhere...
     */
    zend_class_entry** allocated_ce = apc_php_malloc(sizeof(zend_class_entry*));    

    if(!allocated_ce) {
        return FAILURE;
    }

    *allocated_ce = 
#endif        
    class_entry = apc_copy_class_entry_for_execution(cl.class_entry, cl.is_derived);

    /* check to see if this class is on the delayed inheritance list and perform inheritance if so */
    if(APCG(delayed_inheritance_hash).nNumOfElements) {
        zend_class_entry **pparent;
        status = zend_hash_find(&APCG(delayed_inheritance_hash), cl.name, cl.name_len+1, (void **)&pparent);
        if(status == SUCCESS) {
            /* 
               At this point we have found a class that a child has previously said it extended, so
               we go back and fix up that child class by adding this class as its parent and doing the
               inheritance magic on it
            */
            zend_do_inheritance(*pparent, class_entry TSRMLS_CC);
            zend_error(E_WARNING, "Inheriting into %s from parent %s", (*pparent)->name, class_entry->name);
        }
    } 

    /* restore parent class pointer for compile-time inheritance */
    if (cl.parent_name != NULL) {
        int parent_len = strlen(cl.parent_name);
#ifdef ZEND_ENGINE_2    
        zend_class_entry** parent_ptr = NULL;
        /*
         * zend_lookup_class has to be due to presence of __autoload, 
         * just looking up the EG(class_table) is not enough in php5!
         * Even more dangerously, thanks to __autoload and people using
         * class names as filepaths for inclusion, this has to be case
         * sensitive. zend_lookup_class automatically does a case_fold
         * internally, but passes the case preserved version to __autoload.
         * Aside: Do NOT pass *strlen(cl.parent_name)+1* because 
         * zend_lookup_class does it internally anyway!
         */
        status = zend_lookup_class(cl.parent_name, parent_len, &parent_ptr TSRMLS_CC);
#else
        status = zend_hash_find(EG(class_table), cl.parent_name, parent_len+1, (void**) &parent);
#endif
        if (status == FAILURE) {
            /*
              Here a class has said it is a child of some other class, but we can't
              find that other class in the class table.  This is either an error, or
              this parent class is going to come along later (dynamic languages suck!)
              so we keep a table of future_parent->child relationships and each time
              we see a new class we check to see if a child wants it to be its parent.
              Of course, you shouldn't write code that does this since it is going to
              be much slower doing the inheritance dynamically like this.  Stick to 
              well-ordered inheritance trees that allows early binding to do its thing.
            */
            char *lower_parent = estrndup(cl.parent_name, parent_len);
            zend_str_tolower(lower_parent, parent_len);
#ifdef ZEND_ENGINE_2                           
            (*allocated_ce)->refcount++; 
            zend_hash_add(&APCG(delayed_inheritance_hash), lower_parent, parent_len+1, (void *)allocated_ce, sizeof(zend_class_entry *), NULL);
#else                           
            zend_hash_add(&APCG(delayed_inheritance_hash), lower_parent, parent_len+1, (void *)&class_entry, sizeof(zend_class_entry *), NULL);
#endif
            efree(lower_parent);
            class_entry->parent = NULL;
        }
        else {
#ifdef ZEND_ENGINE_2            
            parent = *parent_ptr;
#endif 
            zend_do_inheritance(class_entry, parent TSRMLS_CC);
            zend_error(E_WARNING, "Inheriting into %s from parent %s", class_entry->name, parent->name);
        }
    }
    
    zend_error(E_WARNING, "Adding %s", (*allocated_ce)->name);
#ifdef ZEND_ENGINE_2                           
    status = zend_hash_add(EG(class_table),
                           cl.name,
                           cl.name_len+1,
                           allocated_ce,
                           sizeof(zend_class_entry*),
                           NULL);
#else                           
    status = zend_hash_add(EG(class_table),
                           cl.name,
                           cl.name_len+1,
                           class_entry,
                           sizeof(zend_class_entry),
                           NULL);
#endif                           
    if (status == FAILURE) {
        zend_error(E_ERROR, "Cannot redeclare class %s", cl.name);
    } 
    return status;
}
/* }}} */

/* {{{ cached_compile */
static zend_op_array* cached_compile(TSRMLS_D)
{
    apc_cache_entry_t* cache_entry;
    int i;

    cache_entry = (apc_cache_entry_t*) apc_stack_top(APCG(cache_stack));
    assert(cache_entry != NULL);

    if (cache_entry->data.file.functions) {
        for (i = 0; cache_entry->data.file.functions[i].function != NULL; i++) {
            install_function(cache_entry->data.file.functions[i] TSRMLS_CC);
        }
    }

    if (cache_entry->data.file.classes) {
        for (i = 0; cache_entry->data.file.classes[i].class_entry != NULL; i++) {
            install_class(cache_entry->data.file.classes[i] TSRMLS_CC);
        }
    }

    return apc_copy_op_array_for_execution(cache_entry->data.file.op_array);
}
/* }}} */

/* {{{ my_compile_file
   Overrides zend_compile_file */
static zend_op_array* my_compile_file(zend_file_handle* h,
                                               int type TSRMLS_DC)
{
    apc_cache_key_t key;
    apc_cache_entry_t* cache_entry;
    zend_op_array* op_array;
    int num_functions, num_classes, ret;
    zend_op_array* alloc_op_array;
    apc_function_t* alloc_functions;
    apc_class_t* alloc_classes;
    time_t t;
    char *path;
    size_t mem_size;

    if (!APCG(enabled)) {
		return old_compile_file(h, type TSRMLS_CC);
	}

    /* check our regular expression filters */
    if (apc_compiled_filters) {
        int ret = apc_regex_match_array(apc_compiled_filters, h->filename);
        if(ret == APC_NEGATIVE_MATCH || (ret != APC_POSITIVE_MATCH && !APCG(cache_by_default))) {
            return old_compile_file(h, type TSRMLS_CC);
        }
    } else if(!APCG(cache_by_default)) {
        return old_compile_file(h, type TSRMLS_CC);
    }

#if PHP_API_VERSION <= 20041225
#if HAVE_APACHE && defined(APC_PHP4_STAT)
    t = ((request_rec *)SG(server_context))->request_time;
#else 
    t = time(0);
#endif
#else 
    t = sapi_get_request_time(TSRMLS_C);
#endif

    /* try to create a cache key; if we fail, give up on caching */
    if (!apc_cache_make_file_key(&key, h->filename, PG(include_path), t TSRMLS_CC)) {
        return old_compile_file(h, type TSRMLS_CC);
    }
    
    /* search for the file in the cache */
    cache_entry = apc_cache_find(apc_cache, key, t);
    if (cache_entry != NULL) {
        int dummy = 1;
        if (h->opened_path == NULL) {
            h->opened_path = estrdup(cache_entry->data.file.filename);
        }
        zend_hash_add(&EG(included_files), h->opened_path, strlen(h->opened_path)+1, (void *)&dummy, sizeof(int), NULL);
        zend_llist_add_element(&CG(open_files), h); /* XXX kludge */
        apc_stack_push(APCG(cache_stack), cache_entry);
        return cached_compile(TSRMLS_C);
    }

    /* remember how many functions and classes existed before compilation */
    num_functions = zend_hash_num_elements(CG(function_table));
    num_classes   = zend_hash_num_elements(CG(class_table));
    
    /* compile the file using the default compile function */
    op_array = old_compile_file(h, type TSRMLS_CC);
    if (op_array == NULL) {
        return NULL;
    }
    /*
     * Basically this will cause a file only to be cached on a percentage 
     * of the attempts.  This is to avoid cache slams when starting up a
     * very busy server or when modifying files on a very busy live server.
     * There is no point having many processes all trying to cache the same
     * file at the same time.  By introducing a chance of being cached
     * we theoretically cut the cache slam problem by the given percentage.
     * For example if apc.slam_defense is set to 66 then 2/3 of the attempts
     * to cache an uncached file will be ignored.
     */
    if(APCG(slam_defense) && (int)(100.0*rand()/(RAND_MAX+1.0)) < APCG(slam_defense))
      return op_array;

    HANDLE_BLOCK_INTERRUPTIONS();
    mem_size = 0;
    APCG(mem_size_ptr) = &mem_size;
    if(!(alloc_op_array = apc_copy_op_array(NULL, op_array, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_cache_expunge(apc_cache,t);
        apc_cache_expunge(apc_user_cache,t);
        APCG(mem_size_ptr) = NULL;
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }
    
    if(!(alloc_functions = apc_copy_new_functions(num_functions, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_cache_expunge(apc_cache,t);
        apc_cache_expunge(apc_user_cache,t);
        APCG(mem_size_ptr) = NULL;
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }
    if(!(alloc_classes = apc_copy_new_classes(op_array, num_classes, apc_sma_malloc, apc_sma_free TSRMLS_CC))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_free_functions(alloc_functions, apc_sma_free);
        apc_cache_expunge(apc_cache,t);
        apc_cache_expunge(apc_user_cache,t);
        APCG(mem_size_ptr) = NULL;
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }

    path = h->opened_path;
    if(!path) path=h->filename;

    if(!(cache_entry = apc_cache_make_file_entry(path, alloc_op_array, alloc_functions, alloc_classes))) {
        apc_free_op_array(alloc_op_array, apc_sma_free);
        apc_free_functions(alloc_functions, apc_sma_free);
        apc_free_classes(alloc_classes, apc_sma_free);
        apc_cache_expunge(apc_cache,t);
        apc_cache_expunge(apc_user_cache,t);
        APCG(mem_size_ptr) = NULL;
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return op_array;
    }
    APCG(mem_size_ptr) = NULL;
    cache_entry->mem_size = mem_size;
    HANDLE_UNBLOCK_INTERRUPTIONS();

    if ((ret = apc_cache_insert(apc_cache, key, cache_entry, t)) != 1) {
        apc_cache_free_entry(cache_entry);
        if(ret==-1) {
            apc_cache_expunge(apc_cache,t);
            apc_cache_expunge(apc_user_cache,t);
        }
    }

    return op_array;
}
/* }}} */

/* {{{ module init and shutdown */

int apc_module_init(int module_number TSRMLS_DC)
{
    /* apc initialization */
#if APC_MMAP
    apc_sma_init(APCG(shm_segments), APCG(shm_size)*1024*1024, APCG(mmap_file_mask));
#else
    apc_sma_init(APCG(shm_segments), APCG(shm_size)*1024*1024, NULL);
#endif
    apc_cache = apc_cache_create(APCG(num_files_hint), APCG(gc_ttl), APCG(ttl));
    apc_user_cache = apc_cache_create(APCG(user_entries_hint), APCG(gc_ttl), APCG(user_ttl));
    apc_compiled_filters = apc_regex_compile_array(APCG(filters));

    /* override compilation */
    old_compile_file = zend_compile_file;
    zend_compile_file = my_compile_file;
    REGISTER_LONG_CONSTANT("\000apc_magic", (long)&set_compile_hook, CONST_PERSISTENT | CONST_CS);

    APCG(initialized) = 1;
    return 0;
}

int apc_module_shutdown(TSRMLS_D)
{
    if (!APCG(initialized))
        return 0;

    /* restore compilation */
    zend_compile_file = old_compile_file;

    apc_cache_destroy(apc_cache);
    apc_cache_destroy(apc_user_cache);
    apc_sma_cleanup();

    APCG(initialized) = 0;
    return 0;
}

/* }}} */

/* {{{ request init and shutdown */

int apc_request_init(TSRMLS_D)
{
    apc_stack_clear(APCG(cache_stack));
    return 0;
}

int apc_request_shutdown(TSRMLS_D)
{
    apc_deactivate(TSRMLS_C);
    return 0;
}

/* }}} */

/* {{{ apc_deactivate */
void apc_deactivate(TSRMLS_D)
{
    /* The execution stack was unwound, which prevented us from decrementing
     * the reference counts on active cache entries in `my_execute`.
     */
    while (apc_stack_size(APCG(cache_stack)) > 0) {
        apc_cache_entry_t* cache_entry =
            (apc_cache_entry_t*) apc_stack_pop(APCG(cache_stack));
        apc_cache_release(apc_cache, cache_entry);
    }
    if(APCG(delayed_inheritance_hash).nNumOfElements) {
        zend_hash_clean(&APCG(delayed_inheritance_hash));
    }
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
