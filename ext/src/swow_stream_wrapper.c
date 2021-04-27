/*
  +--------------------------------------------------------------------------+
  | Swow                                                                     |
  +--------------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License");          |
  | you may not use this file except in compliance with the License.         |
  | You may obtain a copy of the License at                                  |
  | http://www.apache.org/licenses/LICENSE-2.0                               |
  | Unless required by applicable law or agreed to in writing, software      |
  | distributed under the License is distributed on an "AS IS" BASIS,        |
  | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. |
  | See the License for the specific language governing permissions and      |
  | limitations under the License. See accompanying LICENSE file.            |
  +--------------------------------------------------------------------------+
  | Author: dixyes <dixyes@gmail.com>                                        |
  +--------------------------------------------------------------------------+
 */

/*
* swow_stream_wrapper.c: stream_wrapper unhooker
* this file implements a way to take use of synchronous fs io operations
* in some php stream wrapper like phar or zip
*/

#include "swow.h"

void _swow_hook_stdio_ops(void);
void _swow_unhook_stdio_ops(void);

// stream ops proxies
static php_stream* swow_proxy_stream_opener(
    php_stream_wrapper *wrapper, const char *filename, const char *mode,
	int options, zend_string **opened_path, php_stream_context *context STREAMS_DC) {
    //printf("wrapper is %p, orig is %p\n", wrapper, wrapper->abstract);
    php_stream_wrapper *real_wrapper = wrapper->abstract;
    /* restore original stdio ops */
    _swow_unhook_stdio_ops();
    php_stream* ret = real_wrapper->wops->stream_opener(real_wrapper,
        filename, mode, options, opened_path, context, STREAMS_REL_C);
    _swow_hook_stdio_ops();
    //printf("end opener\n");
    return ret;
}

static int swow_proxy_stream_closer(php_stream_wrapper *wrapper, php_stream *stream){
    php_stream_wrapper *real_wrapper = wrapper->abstract;
    /* restore original stdio ops */
    _swow_unhook_stdio_ops();
    int ret = real_wrapper->wops->stream_closer(real_wrapper, stream);
    _swow_hook_stdio_ops();
    return ret;
}

static int swow_proxy_stream_stat(php_stream_wrapper *wrapper, php_stream *stream,
    php_stream_statbuf *ssb){
    php_stream_wrapper *real_wrapper = wrapper->abstract;
    /* restore original stdio ops */
    _swow_unhook_stdio_ops();
    int ret = real_wrapper->wops->stream_stat(real_wrapper, stream, ssb);
    _swow_hook_stdio_ops();
    return ret;
}

static int swow_proxy_url_stat(php_stream_wrapper *wrapper,
    const char *url, int flags, php_stream_statbuf *ssb,php_stream_context *context){
    php_stream_wrapper *real_wrapper = wrapper->abstract;
    /* restore original stdio ops */
    _swow_unhook_stdio_ops();
    int ret = real_wrapper->wops->url_stat(real_wrapper, url, flags, ssb, context);
    _swow_hook_stdio_ops();
    return ret;
}

static php_stream *swow_proxy_dir_opener(
    php_stream_wrapper *wrapper, const char *filename, const char *mode,
    int options, zend_string **opened_path, php_stream_context *context STREAMS_DC){
    php_stream_wrapper *real_wrapper = wrapper->abstract;
    /* restore original stdio ops */
    _swow_unhook_stdio_ops();
    php_stream *ret = real_wrapper->wops->dir_opener(
        real_wrapper, filename, mode,options, opened_path, context, STREAMS_REL_C);
    _swow_hook_stdio_ops();
    return ret;
}

static int swow_proxy_unlink(php_stream_wrapper *wrapper, const char *url, int options,
    php_stream_context *context){
    php_stream_wrapper *real_wrapper = wrapper->abstract;
    /* restore original stdio ops */
    _swow_unhook_stdio_ops();
    int ret = real_wrapper->wops->unlink(
        real_wrapper, url, options, context);
    _swow_hook_stdio_ops();
    return ret;
}

static int swow_proxy_rename(php_stream_wrapper *wrapper, const char *url_from,
    const char *url_to, int options, php_stream_context *context){
    php_stream_wrapper *real_wrapper = wrapper->abstract;
    /* restore original stdio ops */
    _swow_unhook_stdio_ops();
    int ret = real_wrapper->wops->rename(
        real_wrapper, url_from, url_to, options, context);
    _swow_hook_stdio_ops();
    return ret;
}

static int swow_proxy_stream_mkdir(php_stream_wrapper *wrapper, const char *url,
    int mode, int options, php_stream_context *context){
    php_stream_wrapper *real_wrapper = wrapper->abstract;
    /* restore original stdio ops */
    _swow_unhook_stdio_ops();
    int ret = real_wrapper->wops->stream_mkdir(
        real_wrapper, url, mode, options, context);
    _swow_hook_stdio_ops();
    return ret;
}
static int swow_proxy_stream_rmdir(php_stream_wrapper *wrapper, const char *url,
    int options, php_stream_context *context){
    php_stream_wrapper *real_wrapper = wrapper->abstract;
    /* restore original stdio ops */
    _swow_unhook_stdio_ops();
    int ret = real_wrapper->wops->stream_rmdir(
        real_wrapper, url, options, context);
    _swow_hook_stdio_ops();
    return ret;
}
static int swow_proxy_stream_metadata(php_stream_wrapper *wrapper, const char *url,
    int options, void *value, php_stream_context *context){
    php_stream_wrapper *real_wrapper = wrapper->abstract;
    /* restore original stdio ops */
    _swow_unhook_stdio_ops();
    int ret = real_wrapper->wops->stream_metadata(
        real_wrapper, url, options, value, context);
    _swow_hook_stdio_ops();
    return ret;
}

typedef struct modified_wrapper_info_s {
    void* modified_wrapper;
    void* modified_wops;
    void* orig_wrapper;
    char name[1];
} modified_wrapper_info;

// TODO: zts?
static int modified_wrappers_inited = 0;
static HashTable modified_wrappers = {0};

/*
* swow_unhook_stream_wrapper: unhook specified wrapper
* name should be wrapper name like "phar"
* name_len is strlen(name)
* this function uses emalloc so it should be called after module startup
*/
int swow_unhook_stream_wrapper(const char * name, size_t name_len){
    if(0 == modified_wrappers_inited){
        zend_hash_init(&modified_wrappers, 0, NULL, ZVAL_PTR_DTOR, 1);
        modified_wrappers_inited = 1;
    }
    HashTable * ht = php_stream_get_url_stream_wrappers_hash_global();
    php_stream_wrapper * orig_wrapper = zend_hash_find_ptr(ht, zend_string_init_interned(name, name_len, 1));
    if(NULL == orig_wrapper){
        // no wrapper found
        return 0;
    }
    if(SUCCESS != php_unregister_url_stream_wrapper(name)){
        //printf("failed unregister wrapper %s\n", name);
        return -1;
    }

    php_stream_wrapper_ops *modified_wops = emalloc(sizeof(*modified_wops));
    php_stream_wrapper *modified_wrapper = emalloc(sizeof(*modified_wrapper));
    modified_wrapper->wops = modified_wops;
    modified_wrapper->abstract = orig_wrapper;
    modified_wrapper->is_url = orig_wrapper->is_url;

    // copy oroginal wops as modified.
    memcpy(modified_wops, orig_wrapper->wops, sizeof(*modified_wops));
    // modify wrapper operators
#define modify(x) modified_wops->x = modified_wops->x == NULL ? NULL : swow_proxy_##x
    modify(stream_opener);
    modify(stream_closer);
    modify(stream_stat);
    modify(url_stat);
    modify(dir_opener);
    modify(unlink);
    modify(rename);
    modify(stream_mkdir);
    modify(stream_rmdir);
    modify(stream_metadata);
#undef modify

    // re-register it
    if(SUCCESS != php_register_url_stream_wrapper(name, modified_wrapper)){
        //printf("failed reregister wrapper %s\n", name);
        efree(modified_wops);
        efree(modified_wrapper);
        return -1;
    };
    //printf("orig wrapper is %p\n", orig_wrapper);
    //printf("new wrapper is %p\n", modified_wrapper);
    // prepare info for restore/cleanup
    modified_wrapper_info* ptr = emalloc(sizeof(*ptr) + name_len + 1);
    ptr->modified_wops = modified_wops;
    ptr->modified_wrapper = modified_wrapper;
    ptr->orig_wrapper = orig_wrapper;
    memcpy(ptr->name, name, name_len + 1);
    // add it into our dustbin "modified_wrappers"
    // using anything as key (here it's modified_wrapper)
    //zend_hash_str_add_ptr(&modified_wrappers, (void*)modified_wrapper, sizeof(*modified_wrapper), ptr);
    zend_hash_next_index_insert_ptr(&modified_wrappers, ptr);
    //zend_hash_str_add_ptr(&modified_wrappers, (void*)modified_wrapper, sizeof(*modified_wrapper), ptr);
    return 0;
}

/*
* swow_rehook_stream_wrapper: rehook all wrappers
* this function uses efree so it should be called before module shutdown
*/
int swow_rehook_stream_wrappers(void){
    if(0 == modified_wrappers_inited){
        return 0;
    }
    int finalret = 0;
    ZEND_HASH_REVERSE_FOREACH_PTR(&modified_wrappers, modified_wrapper_info* info)
        int ret = 0;
        const char * name = info->name;
        //printf("free modified %s\n", name);
        if(SUCCESS != (ret = php_unregister_url_stream_wrapper(name))){
            //printf("failed unregister %s\n", name);
            finalret = ret;
            continue;
        }
        if(SUCCESS != (ret = php_register_url_stream_wrapper(name, info->orig_wrapper))){
            //printf("failed restore %s\n", name);
            finalret = ret;
            continue;
        }
        efree(info->modified_wops);
        efree(info->modified_wrapper);
        efree(info);
    ZEND_HASH_FOREACH_END();
    zend_hash_destroy(&modified_wrappers);
    modified_wrappers_inited = 0;
    return finalret;
}

