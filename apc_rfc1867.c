/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$*/

#include "apc.h"
#include "apc_globals.h"
#include "rfc1867.h"

#ifdef PHP_WIN32
#include "win32/time.h"
#endif

#ifdef MULTIPART_EVENT_FORMDATA
extern int _apc_store(char *strkey, int strkey_len, const zval *val, const unsigned int ttl, const int exclusive TSRMLS_DC);
extern int _apc_update(char *strkey, int strkey_len, apc_cache_updater_t updater, void* data TSRMLS_DC);

static int update_bytes_processed(apc_cache_t* cache, apc_cache_entry_t* entry, void* data) 
{
    int *bytes_ptr = (int*)data;
    zval* val = entry->data.user.val;

    if(Z_TYPE_P(val) == IS_ARRAY) {
        HashTable *ht = val->value.ht;
        Bucket* curr = NULL;
        Bucket* prev = NULL;
        for (curr = ht->pListHead; curr != NULL; curr = curr->pListNext) {
            if(curr->nKeyLength == 8 && 
                (!memcmp(curr->arKey, "current", curr->nKeyLength))) {
                zval* current =  ((zval**)curr->pData)[0];
                current->value.lval = *bytes_ptr;
                return 1;
            }
        }
    }

    return 0;
}

static double my_time() {
    struct timeval a;
    double t;
    gettimeofday(&a, NULL);
    t = a.tv_sec + (a.tv_usec/1000000.00);
    return t;
}

void apc_rfc1867_progress(unsigned int event, void *event_data, void **extra TSRMLS_DC) {
    static char tracking_key[64];
    static int  key_length = 0;
    static size_t content_length = 0;
    static char filename[128];
    static char name[64];
    static char *temp_filename=NULL;
    static int cancel_upload = 0;
    static double start_time;
    static size_t bytes_processed = 0;
    static size_t prev_bytes_processed = 0;
    static int update_freq = 0;
    static double rate;
    zval *track = NULL;

    switch (event) {
        case MULTIPART_EVENT_START:
            {
                multipart_event_start *data = (multipart_event_start *) event_data;
                content_length = data->content_length;
                *tracking_key = '\0';
                *name = '\0';
                cancel_upload = 0;
                temp_filename = NULL;
                *filename= '\0';
                key_length = 0;
                start_time = my_time();
                bytes_processed = 0;
                rate = 0;
                update_freq = APCG(rfc1867_freq);
                if(update_freq < 0) {  // frequency is a percentage, not bytes
                    update_freq = content_length * APCG(rfc1867_freq) / 100; 
                }
            }
            break;

        case MULTIPART_EVENT_FORMDATA:
            {
                int prefix_len = strlen(APCG(rfc1867_prefix));
                multipart_event_formdata *data = (multipart_event_formdata *) event_data;
                if(data->name && !strncasecmp(data->name, APCG(rfc1867_name), strlen(APCG(rfc1867_name))) && data->value && data->length && data->length < sizeof(tracking_key) - prefix_len) {
                    strlcat(tracking_key, APCG(rfc1867_prefix), 63);
                    strlcat(tracking_key, *data->value, 63);
                    key_length = data->length + prefix_len;
                    bytes_processed = data->post_bytes_processed;
                }
            }
            break;

        case MULTIPART_EVENT_FILE_START:
            if(*tracking_key) {
                multipart_event_file_start *data = (multipart_event_file_start *) event_data;

                bytes_processed = data->post_bytes_processed;
                strncpy(filename,*data->filename,127);
                temp_filename = NULL;
                strncpy(name,data->name,63);
                ALLOC_INIT_ZVAL(track);
                array_init(track);
                add_assoc_long(track, "total", content_length);
                add_assoc_long(track, "current", bytes_processed);
                add_assoc_string(track, "filename", filename, 1);
                add_assoc_string(track, "name", name, 1);
                add_assoc_long(track, "done", 0);
                add_assoc_double(track, "start_time", start_time);
                _apc_store(tracking_key, key_length, track, 3600, 0 TSRMLS_CC);
                zval_ptr_dtor(&track);
            }
            break;

        case MULTIPART_EVENT_FILE_DATA:
            if(*tracking_key) {
                multipart_event_file_data *data = (multipart_event_file_data *) event_data;
                bytes_processed = data->post_bytes_processed;
                if(bytes_processed - prev_bytes_processed > update_freq) {
                    if(!_apc_update(tracking_key, key_length, update_bytes_processed, &bytes_processed)) {
                        ALLOC_INIT_ZVAL(track);
                        array_init(track);
                        add_assoc_long(track, "total", content_length);
                        add_assoc_long(track, "current", bytes_processed);
                        add_assoc_string(track, "filename", filename, 1);
                        add_assoc_string(track, "name", name, 1);
                        add_assoc_long(track, "done", 0);
                        add_assoc_double(track, "start_time", start_time);
                        _apc_store(tracking_key, key_length, track, 3600, 0 TSRMLS_CC);
                        zval_ptr_dtor(&track);
                    }
                    prev_bytes_processed = bytes_processed;
                }
            }
            break;

        case MULTIPART_EVENT_FILE_END:
            if(*tracking_key) {
                multipart_event_file_end *data = (multipart_event_file_end *) event_data;
                bytes_processed = data->post_bytes_processed;
                cancel_upload = data->cancel_upload;
                temp_filename = data->temp_filename;
                ALLOC_INIT_ZVAL(track);
                array_init(track);
                add_assoc_long(track, "total", content_length);
                add_assoc_long(track, "current", bytes_processed);
                add_assoc_string(track, "filename", filename, 1);
                add_assoc_string(track, "name", name, 1);
                add_assoc_string(track, "temp_filename", temp_filename, 1);
                add_assoc_long(track, "cancel_upload", cancel_upload);
                add_assoc_long(track, "done", 0);
                add_assoc_double(track, "start_time", start_time);
                _apc_store(tracking_key, key_length, track, 3600, 0 TSRMLS_CC);
                zval_ptr_dtor(&track);
            }
            break;

        case MULTIPART_EVENT_END:
            if(*tracking_key) {
                double now = my_time(); 
                multipart_event_end *data = (multipart_event_end *) event_data;
                bytes_processed = data->post_bytes_processed;
                if(now>start_time) rate = 8.0*bytes_processed/(now-start_time);
                else rate = 8.0*bytes_processed;  /* Too quick */
                ALLOC_INIT_ZVAL(track);
                array_init(track);
                add_assoc_long(track, "total", content_length);
                add_assoc_long(track, "current", bytes_processed);
                add_assoc_double(track, "rate", rate);
                add_assoc_string(track, "filename", filename, 1);
                add_assoc_string(track, "name", name, 1);
                if(temp_filename) {
                    add_assoc_string(track, "temp_filename", temp_filename, 1);
                }
                add_assoc_long(track, "cancel_upload", cancel_upload);
                add_assoc_long(track, "done", 1);
                add_assoc_double(track, "start_time", start_time);
                _apc_store(tracking_key, key_length, track, 3600, 0 TSRMLS_CC);
                zval_ptr_dtor(&track);
            }
            break;
    }
}

#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
