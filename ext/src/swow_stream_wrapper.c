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
 * in php phar stream wrapper to support ie
 */

#include "swow.h"
#include "swow_stream.h"

static php_stream_ops modified_ops = { 0 };
static php_stream_ops modified_dir_ops = { 0 };
// holder for phar_ops
static const php_stream_ops *orig_ops = NULL;
static const php_stream_ops *orig_dir_ops = NULL;
static php_stream_wrapper modified_wrapper = { 0 };
static php_stream_wrapper_ops modified_wops = { 0 };
static php_stream_wrapper *orig_wrapper = NULL;

#define SWOW_UNHOOK(check) \
    {check}; \
    cat_bool_t hooking_plain_wrapper = SWOW_STREAM_G(hooking_plain_wrapper); \
    cat_bool_t hooking_stdio_ops = SWOW_STREAM_G(hooking_stdio_ops); \
    do { \
        if (hooking_plain_wrapper) \
            SWOW_STREAM_G(hooking_plain_wrapper) = cat_false; \
        if (hooking_stdio_ops) \
            SWOW_STREAM_G(hooking_stdio_ops) = cat_false; \
    } while (0)
#define SWOW_REHOOK() \
    do { \
        SWOW_STREAM_G(hooking_plain_wrapper) = hooking_plain_wrapper; \
        SWOW_STREAM_G(hooking_stdio_ops) = hooking_stdio_ops; \
    } while (0)

// stream ops proxies
ssize_t swow_proxy_write(php_stream *stream, const char *buf, size_t count)
{
    SWOW_UNHOOK(ZEND_ASSERT(orig_ops););
    ssize_t ret = orig_ops->write(stream, buf, count);
    SWOW_REHOOK();
    return ret;
}

ssize_t swow_proxy_read(php_stream *stream, char *buf, size_t count)
{
    SWOW_UNHOOK(ZEND_ASSERT(orig_ops););
    ssize_t ret = orig_ops->read(stream, buf, count);
    SWOW_REHOOK();
    return ret;
}

int swow_proxy_close(php_stream *stream, int close_handle)
{
    SWOW_UNHOOK(ZEND_ASSERT(orig_ops););
    int ret = orig_ops->close(stream, close_handle);
    SWOW_REHOOK();
    return ret;
}

int swow_proxy_flush(php_stream *stream)
{
    SWOW_UNHOOK(ZEND_ASSERT(orig_ops););
    int ret = orig_ops->flush(stream);
    SWOW_REHOOK();
    return ret;
}

int swow_proxy_seek(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset)
{
    SWOW_UNHOOK(ZEND_ASSERT(orig_ops););
    int ret = orig_ops->seek(stream, offset, whence, newoffset);
    SWOW_REHOOK();
    return ret;
}

/*
// phar not use this
int swow_proxy_cast(php_stream *stream, int castas, void **ret){
    SWOW_UNHOOK(ZEND_ASSERT(orig_ops););
    int ret = orig_ops->cast(stream, castas, ret);
    SWOW_REHOOK();
    return ret;
}
*/

int swow_proxy_stat(php_stream *stream, php_stream_statbuf *ssb)
{
    SWOW_UNHOOK(ZEND_ASSERT(orig_ops););
    int ret = orig_ops->stat(stream, ssb);
    SWOW_REHOOK();
    return ret;
}

/*
// phar not use this
int swow_proxy_set_option(php_stream *stream, int option, int value, void *ptrparam){
    SWOW_UNHOOK(ZEND_ASSERT(orig_ops););
    int ret = orig_ops->set_option(stream, option, value, ptrparam);
    SWOW_REHOOK();
    return ret;
}
*/

// stream dir ops proxies
ssize_t swow_proxy_dir_write(php_stream *stream, const char *buf, size_t count)
{
    SWOW_UNHOOK(ZEND_ASSERT(orig_dir_ops););
    ssize_t ret = orig_dir_ops->write(stream, buf, count);
    SWOW_REHOOK();
    return ret;
}

ssize_t swow_proxy_dir_read(php_stream *stream, char *buf, size_t count)
{
    SWOW_UNHOOK(ZEND_ASSERT(orig_dir_ops););
    ssize_t ret = orig_dir_ops->read(stream, buf, count);
    SWOW_REHOOK();
    return ret;
}

int swow_proxy_dir_close(php_stream *stream, int close_handle)
{
    SWOW_UNHOOK(ZEND_ASSERT(orig_dir_ops););
    int ret = orig_dir_ops->close(stream, close_handle);
    SWOW_REHOOK();
    return ret;
}

int swow_proxy_dir_flush(php_stream *stream)
{
    SWOW_UNHOOK(ZEND_ASSERT(orig_dir_ops););
    int ret = orig_dir_ops->flush(stream);
    SWOW_REHOOK();
    return ret;
}

int swow_proxy_dir_seek(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset)
{
    SWOW_UNHOOK(ZEND_ASSERT(orig_dir_ops););
    int ret = orig_dir_ops->seek(stream, offset, whence, newoffset);
    SWOW_REHOOK();
    return ret;
}

static php_stream* swow_proxy_stream_opener(
    php_stream_wrapper *wrapper, const char *filename, const char *mode,
    int options, zend_string **opened_path, php_stream_context *context STREAMS_DC
)
{
    //printf("wrapper is %p, orig is %p\n", wrapper, wrapper->abstract);
    //printf("opener real wrapper is %p\n", orig_wrapper);
    SWOW_UNHOOK(ZEND_ASSERT(orig_wrapper););
    php_stream *ret =
        orig_wrapper->wops->stream_opener(wrapper, filename, mode, options, opened_path, context STREAMS_REL_CC);
    SWOW_REHOOK();
    if (!ret || !ret->ops || !ret->ops->label) {
        // if operation failed, exit
        return ret;
    }
    if (NULL == orig_ops && 0 == strncmp(ret->ops->label, "phar stream", sizeof("phar stream"))) {
        // lazy setting
        orig_ops = ret->ops;
        memcpy(&modified_ops, ret->ops, sizeof(modified_ops));
        // modify stream operators
#define modify(x) modified_ops.x = modified_ops.x ? swow_proxy_##x : NULL
        modify(write);
        modify(read);
        modify(close);
        modify(flush);
        modify(seek);
        ZEND_ASSERT(NULL == modified_ops.cast);
        //modify(cast);
        modify(stat);
        ZEND_ASSERT(NULL == modified_ops.set_option);
        //modify(set_option);
#undef modify
    }
    // at first use, orig_ops is not set, ret->ops may not be phar things
    // this will be (NULL != ret->ops) -> true, that's ok
    if (orig_ops != ret->ops) {
        // we're not modify this because it's not phar stream
        return ret;
    }
    ret->ops = &modified_ops;
    //printf("end opener\n");
    return ret;
}

/*
// phar not used this
static int swow_proxy_stream_closer(php_stream_wrapper *wrapper, php_stream *stream){
    //printf("close wrapper is %p\n", orig_wrapper);
    SWOW_UNHOOK(ZEND_ASSERT(orig_wrapper););
    int ret = orig_wrapper->wops->stream_closer(wrapper, stream);
    SWOW_REHOOK();
    return ret;
}

static int swow_proxy_stream_stat(php_stream_wrapper *wrapper, php_stream *stream,
    php_stream_statbuf *ssb){
    //printf("fstat wrapper is %p\n", orig_wrapper);
    SWOW_UNHOOK(ZEND_ASSERT(orig_wrapper););
    int ret = orig_wrapper->wops->stream_stat(wrapper, stream, ssb);
    SWOW_REHOOK();
    return ret;
}
*/

static int swow_proxy_url_stat(
    php_stream_wrapper *wrapper, const char *url, int flags, php_stream_statbuf *ssb, php_stream_context *context
)
{
    // printf("stat wrapper is %p\n", orig_wrapper);
    SWOW_UNHOOK(ZEND_ASSERT(orig_wrapper););
    int ret = orig_wrapper->wops->url_stat(wrapper, url, flags, ssb, context);
    SWOW_REHOOK();
    return ret;
}

static php_stream *swow_proxy_dir_opener(php_stream_wrapper *wrapper,
    const char *filename,
    const char *mode,
    int options,
    zend_string **opened_path,
    php_stream_context *context STREAMS_DC
)
{
    // printf("opendir wrapper is %p\n", orig_wrapper);
    SWOW_UNHOOK(ZEND_ASSERT(orig_wrapper););
    // printf("orig wrapper %p\n", orig_wrapper);
    php_stream *ret =
        orig_wrapper->wops->dir_opener(wrapper, filename, mode, options, opened_path, context STREAMS_REL_CC);
    // printf("returnnig wrp %p\n", ret->wrapper);
    SWOW_REHOOK();
    if (!ret || !ret->ops || !ret->ops->label) {
        // if operation failed, exit
        return ret;
    }
    if (NULL == orig_dir_ops && 0 == strncmp(ret->ops->label, "phar dir", sizeof("phar dir"))) {
        // lazy setting
        orig_dir_ops = ret->ops;
        memcpy(&modified_dir_ops, ret->ops, sizeof(modified_dir_ops));
        // modify stream operators
#define modify(x) modified_dir_ops.x = modified_dir_ops.x ? swow_proxy_dir_##x : NULL
        modify(write);
        modify(read);
        modify(close);
        modify(flush);
        modify(seek);
        ZEND_ASSERT(NULL == modified_dir_ops.cast);
        ZEND_ASSERT(NULL == modified_dir_ops.stat);
        ZEND_ASSERT(NULL == modified_dir_ops.set_option);
#undef modify
    }
    // at first use, orig_dir_ops is not set, ret->ops may not be phar things
    // this will be (NULL != ret->ops) -> true, that's ok
    if (orig_dir_ops != ret->ops) {
        // we're not modify this because it's not phar dir stream
        return ret;
    }
    ret->ops = &modified_dir_ops;
    return ret;
}

static int swow_proxy_unlink(php_stream_wrapper *wrapper, const char *url, int options, php_stream_context *context)
{
    // printf("unlink wrapper is %p\n", orig_wrapper);
    SWOW_UNHOOK(ZEND_ASSERT(orig_wrapper););
    int ret = orig_wrapper->wops->unlink(wrapper, url, options, context);
    SWOW_REHOOK();
    return ret;
}

static int swow_proxy_rename(
    php_stream_wrapper *wrapper, const char *url_from, const char *url_to, int options, php_stream_context *context)
{
    // printf("rename real wrapper is %p\n", orig_wrapper);
    SWOW_UNHOOK(ZEND_ASSERT(orig_wrapper););
    int ret = orig_wrapper->wops->rename(wrapper, url_from, url_to, options, context);
    SWOW_REHOOK();
    return ret;
}

static int swow_proxy_stream_mkdir(
    php_stream_wrapper *wrapper, const char *url, int mode, int options, php_stream_context *context
)
{
    // printf("mkdir real wrapper is %p\n", orig_wrapper);
    SWOW_UNHOOK(ZEND_ASSERT(orig_wrapper););
    int ret = orig_wrapper->wops->stream_mkdir(wrapper, url, mode, options, context);
    SWOW_REHOOK();
    return ret;
}
static int swow_proxy_stream_rmdir(
    php_stream_wrapper *wrapper, const char *url, int options, php_stream_context *context
)
{
    // printf("rmdir real wrapper is %p\n", orig_wrapper);
    SWOW_UNHOOK(ZEND_ASSERT(orig_wrapper););
    int ret = orig_wrapper->wops->stream_rmdir(wrapper, url, options, context);
    SWOW_REHOOK();
    return ret;
}
/*
// phar not used this
static int swow_proxy_stream_metadata(php_stream_wrapper *wrapper, const char *url,
    int options, void *value, php_stream_context *context){
    //printf("md real wrapper is %p\n", orig_wrapper);
    SWOW_UNHOOK(ZEND_ASSERT(orig_wrapper););
    int ret = orig_wrapper->wops->stream_metadata(
        wrapper, url, options, value, context);
    SWOW_REHOOK();
    return ret;
}
*/

/*
 * swow_unhook_stream_wrapper: unhook specified wrapper
 * name should be wrapper name like "phar"
 * name_len is strlen(name)
 */
int swow_unhook_stream_wrapper(void)
{
    HashTable *ht = php_stream_get_url_stream_wrappers_hash_global();
    orig_wrapper = zend_hash_find_ptr(ht, zend_string_init_interned("phar", 4, 1));
    if (NULL == orig_wrapper) {
        // no wrapper found
        return 0;
    }
    if (SUCCESS != php_unregister_url_stream_wrapper("phar")) {
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
    ZEND_ASSERT(NULL == modified_wops.stream_closer);
    //modify(stream_closer);
    ZEND_ASSERT(NULL == modified_wops.stream_stat);
    //modify(stream_stat);
    modify(url_stat);
    modify(dir_opener);
    modify(unlink);
    modify(rename);
    modify(stream_mkdir);
    modify(stream_rmdir);
    ZEND_ASSERT(NULL == modified_wops.stream_metadata);
    //modify(stream_metadata);
#undef modify

    // re-register it
    if (SUCCESS != php_register_url_stream_wrapper("phar", &modified_wrapper)) {
        //printf("failed reregister wrapper %s\n", name);
        return -1;
    };
    return 0;
}

/*
 * swow_rehook_stream_wrapper: rehook all wrappers
 */
int swow_rehook_stream_wrappers(void)
{
    //printf("free modified %s\n", name);
    if (SUCCESS != php_unregister_url_stream_wrapper("phar")) {
        //printf("failed unregister %s\n", name);
        return 0;
    }
    if (SUCCESS != php_register_url_stream_wrapper("phar", orig_wrapper)) {
        //printf("failed restore %s\n", name);
        return -1;
    }
    //printf("done free modified %s\n", name);
    return 0;
}
