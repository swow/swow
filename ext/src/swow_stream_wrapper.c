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
* in php phar stream wrapper
*/

#include "swow.h"
#include "swow_stream.h"

static php_stream_wrapper modified_wrapper = { 0 };
static php_stream_wrapper_ops modified_wops = { 0 };
static php_stream_wrapper *orig_wrapper = NULL;

#define SWOW_UNHOOK(bad_ret) \
if(!orig_wrapper){ return bad_ret; }\
cat_bool_t hooking_plain_wrapper = SWOW_STREAM_G(hooking_plain_wrapper);\
cat_bool_t hooking_stdio_ops = SWOW_STREAM_G(hooking_stdio_ops);\
do {\
    if(hooking_plain_wrapper) SWOW_STREAM_G(hooking_plain_wrapper) = cat_false;\
    if(hooking_stdio_ops) SWOW_STREAM_G(hooking_stdio_ops) = cat_false;\
}while(0)
#define SWOW_REHOOK() do {\
    SWOW_STREAM_G(hooking_plain_wrapper) = hooking_plain_wrapper;\
    SWOW_STREAM_G(hooking_stdio_ops) = hooking_stdio_ops;\
}while(0)

// stream ops proxies
static php_stream* swow_proxy_stream_opener(
    php_stream_wrapper *wrapper, const char *filename, const char *mode,
	int options, zend_string **opened_path, php_stream_context *context STREAMS_DC) {
    //printf("wrapper is %p, orig is %p\n", wrapper, wrapper->abstract);
    //printf("opener real wrapper is %p\n", orig_wrapper);
    SWOW_UNHOOK(NULL);
    php_stream* ret = orig_wrapper->wops->stream_opener(wrapper,
        filename, mode, options, opened_path, context STREAMS_REL_CC);
    SWOW_REHOOK();
    //printf("end opener\n");
    return ret;
}

static int swow_proxy_stream_closer(php_stream_wrapper *wrapper, php_stream *stream){
    //printf("close wrapper is %p\n", orig_wrapper);
    /* restore original stdio ops */
    SWOW_UNHOOK(-1);
    int ret = orig_wrapper->wops->stream_closer(wrapper, stream);
    SWOW_REHOOK();
    return ret;
}

static int swow_proxy_stream_stat(php_stream_wrapper *wrapper, php_stream *stream,
    php_stream_statbuf *ssb){
    //printf("fstat wrapper is %p\n", orig_wrapper);
    /* restore original stdio ops */
    SWOW_UNHOOK(-1);
    int ret = orig_wrapper->wops->stream_stat(wrapper, stream, ssb);
    SWOW_REHOOK();
    return ret;
}

static int swow_proxy_url_stat(php_stream_wrapper *wrapper,
    const char *url, int flags, php_stream_statbuf *ssb,php_stream_context *context){
    //printf("stat wrapper is %p\n", orig_wrapper);
    /* restore original stdio ops */
    SWOW_UNHOOK(-1);
    int ret = orig_wrapper->wops->url_stat(wrapper, url, flags, ssb, context);
    SWOW_REHOOK();
    return ret;
}

static php_stream *swow_proxy_dir_opener(
    php_stream_wrapper *wrapper, const char *filename, const char *mode,
    int options, zend_string **opened_path, php_stream_context *context STREAMS_DC){
    //printf("opendir wrapper is %p\n", orig_wrapper);
    /* restore original stdio ops */
    SWOW_UNHOOK(NULL);
    //printf("orig wrapper %p\n", orig_wrapper);
    php_stream *ret = orig_wrapper->wops->dir_opener(
        wrapper, filename, mode,options, opened_path, context STREAMS_REL_CC);

    //printf("returnnig wrp %p\n", ret->wrapper);
    SWOW_REHOOK();
    return ret;
}

static int swow_proxy_unlink(php_stream_wrapper *wrapper, const char *url, int options,
    php_stream_context *context){
    //printf("unlink wrapper is %p\n", orig_wrapper);
    /* restore original stdio ops */
    SWOW_UNHOOK(-1);
    int ret = orig_wrapper->wops->unlink(
        wrapper, url, options, context);
    SWOW_REHOOK();
    return ret;
}

static int swow_proxy_rename(php_stream_wrapper *wrapper, const char *url_from,
    const char *url_to, int options, php_stream_context *context){
    //printf("rename real wrapper is %p\n", orig_wrapper);
    /* restore original stdio ops */
    SWOW_UNHOOK(-1);
    int ret = orig_wrapper->wops->rename(
        wrapper, url_from, url_to, options, context);
    SWOW_REHOOK();
    return ret;
}

static int swow_proxy_stream_mkdir(php_stream_wrapper *wrapper, const char *url,
    int mode, int options, php_stream_context *context){
    //printf("mkdir real wrapper is %p\n", orig_wrapper);
    /* restore original stdio ops */
    SWOW_UNHOOK(-1);
    int ret = orig_wrapper->wops->stream_mkdir(
        wrapper, url, mode, options, context);
    SWOW_REHOOK();
    return ret;
}
static int swow_proxy_stream_rmdir(php_stream_wrapper *wrapper, const char *url,
    int options, php_stream_context *context){
    //printf("rmdir real wrapper is %p\n", orig_wrapper);
    /* restore original stdio ops */
    SWOW_UNHOOK(-1);
    int ret = orig_wrapper->wops->stream_rmdir(
        wrapper, url, options, context);
    SWOW_REHOOK();
    return ret;
}
static int swow_proxy_stream_metadata(php_stream_wrapper *wrapper, const char *url,
    int options, void *value, php_stream_context *context){
    //printf("md real wrapper is %p\n", orig_wrapper);
    /* restore original stdio ops */
    SWOW_UNHOOK(-1);
    int ret = orig_wrapper->wops->stream_metadata(
        wrapper, url, options, value, context);
    SWOW_REHOOK();
    return ret;
}

/*
* swow_unhook_stream_wrapper: unhook specified wrapper
* name should be wrapper name like "phar"
* name_len is strlen(name)
*/
int swow_unhook_stream_wrapper(void){
    HashTable * ht = php_stream_get_url_stream_wrappers_hash_global();
    orig_wrapper = zend_hash_find_ptr(ht, zend_string_init_interned("phar", 4, 1));
    if(NULL == orig_wrapper){
        // no wrapper found
        return 0;
    }
    if(SUCCESS != php_unregister_url_stream_wrapper("phar")){
        //printf("failed unregister wrapper %s\n", name);
        return -1;
    }

    modified_wrapper.wops = &modified_wops;
    modified_wrapper.abstract = orig_wrapper->abstract;
    modified_wrapper.is_url = orig_wrapper->is_url;

    // copy oroginal wops as modified.
    memcpy(&modified_wops, orig_wrapper->wops, sizeof(modified_wops));
    // modify wrapper operators
#define modify(x) modified_wops.x = modified_wops.x ? swow_proxy_##x : NULL
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
    if(SUCCESS != php_register_url_stream_wrapper("phar", &modified_wrapper)){
        //printf("failed reregister wrapper %s\n", name);
        return -1;
    };
    return 0;
}

/*
* swow_rehook_stream_wrapper: rehook all wrappers
*/
int swow_rehook_stream_wrappers(void){
    int ret = 0;
    //printf("free modified %s\n", name);
    if(SUCCESS != (ret = php_unregister_url_stream_wrapper("phar"))){
        //printf("failed unregister %s\n", name);
        return -1;
    }
    if(SUCCESS != (ret = php_register_url_stream_wrapper("phar", orig_wrapper))){
        //printf("failed restore %s\n", name);
        return -1;
    }
    //printf("done free modified %s\n", name);
    return 0;
}
