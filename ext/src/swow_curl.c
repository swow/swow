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
  | Author: Twosee <twosee@php.net>                                          |
  +--------------------------------------------------------------------------+
 */

#include "swow_curl.h"
#include "swow_hook.h"

#include "cat_curl.h"
#ifdef CAT_CURL

#include <curl/curl.h>
#include <curl/multi.h>

#define CURLOPT_RETURNTRANSFER 19913
#define CURLOPT_BINARYTRANSFER 19914 /* For Backward compatibility */
#define PHP_CURL_STDOUT 0
#define PHP_CURL_FILE   1
#define PHP_CURL_USER   2
#define PHP_CURL_DIRECT 3
#define PHP_CURL_RETURN 4
#define PHP_CURL_IGNORE 7

#define SAVE_CURL_ERROR(__handle, __err) \
    do { (__handle)->err.no = (int) __err; } while (0)

#define SAVE_CURLM_ERROR SAVE_CURL_ERROR

typedef struct {
    zval                  func_name;
    zend_fcall_info_cache fci_cache;
    FILE                 *fp;
    smart_str             buf;
    int                   method;
    zval                  stream;
} php_curl_write;

typedef struct {
    zval                  func_name;
    zend_fcall_info_cache fci_cache;
    FILE                 *fp;
    zend_resource        *res;
    int                   method;
    zval                  stream;
} php_curl_read;

typedef struct {
    zval                  func_name;
    zend_fcall_info_cache fci_cache;
#if PHP_VERSION_ID < 80100
    int                    method;
#endif
} php_curl_callback;

typedef struct {
    php_curl_write    *write;
    php_curl_write    *write_header;
    php_curl_read     *read;
    zval               std_err;
    php_curl_callback *progress;
#if LIBCURL_VERSION_NUM >= 0x071500 /* Available since 7.21.0 */
    php_curl_callback *fnmatch;
#endif
} php_curl_handlers;

struct _php_curl_error  {
    char str[CURL_ERROR_SIZE + 1];
    int  no;
};

struct _php_curl_send_headers {
    zend_string *str;
};

struct _php_curl_free {
#if PHP_VERSION_ID < 80100
    zend_llist str;
#endif
    zend_llist post;
    zend_llist stream;
#if PHP_VERSION_ID >= 80100
#if LIBCURL_VERSION_NUM < 0x073800 /* 7.56.0 */
    zend_llist buffers;
#endif
#endif
    HashTable *slist;
};

typedef struct {
    CURL                         *cp;
#if PHP_VERSION_ID < 80100
    php_curl_handlers            *handlers;
#else
    php_curl_handlers             handlers;
#endif
    struct _php_curl_free        *to_free;
    struct _php_curl_send_headers header;
    struct _php_curl_error        err;
    bool                          in_callback;
    uint32_t*                     clone;
    zval                          postfields;
#if PHP_VERSION_ID >= 80100
    /* For CURLOPT_PRIVATE */
    zval private_data;
#endif
    /* CurlShareHandle object set using CURLOPT_SHARE. */
    struct _php_curlsh           *share;
    zend_object                   std;
} php_curl;

typedef struct {
    php_curl_callback    *server_push;
} php_curlm_handlers;

typedef struct {
#if PHP_VERSION_ID < 80100
    int         still_running;
#endif
    CURLM      *multi;
    zend_llist  easyh;
#if PHP_VERSION_ID < 80100
    php_curlm_handlers *handlers;
#else
    php_curlm_handlers handlers;
#endif
    struct {
        int no;
    } err;
    zend_object std;
} php_curlm;

static zend_class_entry *curl_ce = NULL;

static inline php_curl *curl_from_obj(zend_object *obj) {
    return (php_curl *)((char *)(obj) - XtOffsetOf(php_curl, std));
}

#define Z_CURL_P(zv) curl_from_obj(Z_OBJ_P(zv))

static zend_class_entry *curl_multi_ce = NULL;

static inline php_curlm *curl_multi_from_obj(zend_object *obj) {
    return (php_curlm *)((char *)(obj) - XtOffsetOf(php_curlm, std));
}

#define Z_CURL_MULTI_P(zv) curl_multi_from_obj(Z_OBJ_P(zv))


#if PHP_VERSION_ID < 80100
#define CURL_HANDLERS_GET(ch, name) ch->handlers->name
#else
#define CURL_HANDLERS_GET(ch, name) ch->handlers.name
#endif
#define CURLM_HANDLERS_GET(mh, name) CURL_HANDLERS_GET(mh, name)

static void _swow_php_curl_verify_handlers(php_curl *ch, int reporterror) /* {{{ */
{
    php_stream *stream;

    ZEND_ASSERT(ch);

    if (!Z_ISUNDEF(CURL_HANDLERS_GET(ch, std_err))) {
        stream = (php_stream *)zend_fetch_resource2_ex(&CURL_HANDLERS_GET(ch, std_err), NULL, php_file_le_stream(), php_file_le_pstream());
        if (stream == NULL) {
            if (reporterror) {
                php_error_docref(NULL, E_WARNING, "CURLOPT_STDERR resource has gone away, resetting to stderr");
            }
            zval_ptr_dtor(&CURL_HANDLERS_GET(ch, std_err));
            ZVAL_UNDEF(&CURL_HANDLERS_GET(ch, std_err));

            curl_easy_setopt(ch->cp, CURLOPT_STDERR, stderr);
        }
    }
    if (CURL_HANDLERS_GET(ch, read) && !Z_ISUNDEF(CURL_HANDLERS_GET(ch, read)->stream)) {
        stream = (php_stream *)zend_fetch_resource2_ex(&CURL_HANDLERS_GET(ch, read)->stream, NULL, php_file_le_stream(), php_file_le_pstream());
        if (stream == NULL) {
            if (reporterror) {
                php_error_docref(NULL, E_WARNING, "CURLOPT_INFILE resource has gone away, resetting to default");
            }
            zval_ptr_dtor(&CURL_HANDLERS_GET(ch, read)->stream);
            ZVAL_UNDEF(&CURL_HANDLERS_GET(ch, read)->stream);
            CURL_HANDLERS_GET(ch, read)->res = NULL;
            CURL_HANDLERS_GET(ch, read)->fp = 0;

            curl_easy_setopt(ch->cp, CURLOPT_INFILE, (void *) ch);
        }
    }
    if (CURL_HANDLERS_GET(ch, write_header) && !Z_ISUNDEF(CURL_HANDLERS_GET(ch, write_header)->stream)) {
        stream = (php_stream *)zend_fetch_resource2_ex(&CURL_HANDLERS_GET(ch, write_header)->stream, NULL, php_file_le_stream(), php_file_le_pstream());
        if (stream == NULL) {
            if (reporterror) {
                php_error_docref(NULL, E_WARNING, "CURLOPT_WRITEHEADER resource has gone away, resetting to default");
            }
            zval_ptr_dtor(&CURL_HANDLERS_GET(ch, write_header)->stream);
            ZVAL_UNDEF(&CURL_HANDLERS_GET(ch, write_header)->stream);
            CURL_HANDLERS_GET(ch, write_header)->fp = 0;

            CURL_HANDLERS_GET(ch, write_header)->method = PHP_CURL_IGNORE;
            curl_easy_setopt(ch->cp, CURLOPT_WRITEHEADER, (void *) ch);
        }
    }
    if (CURL_HANDLERS_GET(ch, write) && !Z_ISUNDEF(CURL_HANDLERS_GET(ch, write)->stream)) {
        stream = (php_stream *)zend_fetch_resource2_ex(&CURL_HANDLERS_GET(ch, write)->stream, NULL, php_file_le_stream(), php_file_le_pstream());
        if (stream == NULL) {
            if (reporterror) {
                php_error_docref(NULL, E_WARNING, "CURLOPT_FILE resource has gone away, resetting to default");
            }
            zval_ptr_dtor(&CURL_HANDLERS_GET(ch, write)->stream);
            ZVAL_UNDEF(&CURL_HANDLERS_GET(ch, write)->stream);
            CURL_HANDLERS_GET(ch, write)->fp = 0;

            CURL_HANDLERS_GET(ch, write)->method = PHP_CURL_STDOUT;
            curl_easy_setopt(ch->cp, CURLOPT_FILE, (void *) ch);
        }
    }
    return;
}
/* }}} */

/* {{{ _php_curl_cleanup_handle(ch)
   Cleanup an execution phase */
static void _swow_php_curl_cleanup_handle(php_curl *ch)
{
    smart_str_free(&CURL_HANDLERS_GET(ch, write)->buf);
    if (ch->header.str) {
        zend_string_release_ex(ch->header.str, 0);
        ch->header.str = NULL;
    }

    memset(ch->err.str, 0, CURL_ERROR_SIZE + 1);
    ch->err.no = 0;
}
/* }}} */

static void _swow_php_curl_multi_cleanup_list(void *data) /* {{{ */
{
    zval *z_ch = (zval *)data;
    zval_ptr_dtor(z_ch);
}
/* }}} */

static void swow_curl_multi_free_obj(zend_object *object)
{
    php_curlm *mh = curl_multi_from_obj(object);

    zend_llist_position pos;
    php_curl *ch;
    zval    *pz_ch;

    if (!mh->multi) {
        /* Can happen if constructor throws. */
        zend_object_std_dtor(&mh->std);
        return;
    }

    for (pz_ch = (zval *)zend_llist_get_first_ex(&mh->easyh, &pos); pz_ch;
        pz_ch = (zval *)zend_llist_get_next_ex(&mh->easyh, &pos)) {
        if (!(OBJ_FLAGS(Z_OBJ_P(pz_ch)) & IS_OBJ_FREE_CALLED)) {
            ch = Z_CURL_P(pz_ch);
            _swow_php_curl_verify_handlers(ch, 0);
        }
    }

    cat_curl_multi_cleanup(mh->multi);
    zend_llist_clean(&mh->easyh);
    if (CURLM_HANDLERS_GET(mh, server_push)) {
        zval_ptr_dtor(&CURLM_HANDLERS_GET(mh, server_push->func_name));
        efree(CURLM_HANDLERS_GET(mh, server_push));
    }
#if PHP_VERSION_ID < 80100
    if (mh->handlers) {
        efree(mh->handlers);
    }
#endif

    zend_object_std_dtor(&mh->std);
}

// TODO: hook curl_read()/curl_write()

/* {{{ Perform a cURL session */
static PHP_FUNCTION(swow_curl_exec)
{
    CURLcode    error;
    zval        *zid;
    php_curl    *ch;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
    ZEND_PARSE_PARAMETERS_END();

    ch = Z_CURL_P(zid);

    _swow_php_curl_verify_handlers(ch, 1);

    _swow_php_curl_cleanup_handle(ch);

    error = cat_curl_easy_perform(ch->cp);
    SAVE_CURL_ERROR(ch, error);

    if (error != CURLE_OK) {
        smart_str_free(&CURL_HANDLERS_GET(ch, write)->buf);
        RETURN_FALSE;
    }

    if (!Z_ISUNDEF(CURL_HANDLERS_GET(ch, std_err))) {
        php_stream  *stream;
        stream = (php_stream*)zend_fetch_resource2_ex(&CURL_HANDLERS_GET(ch, std_err), NULL, php_file_le_stream(), php_file_le_pstream());
        if (stream) {
            php_stream_flush(stream);
        }
    }

    if (CURL_HANDLERS_GET(ch, write)->method == PHP_CURL_RETURN && CURL_HANDLERS_GET(ch, write)->buf.s) {
        smart_str_0(&CURL_HANDLERS_GET(ch, write)->buf);
        RETURN_STR_COPY(CURL_HANDLERS_GET(ch, write)->buf.s);
    }

    /* flush the file handle, so any remaining data is synched to disk */
    if (CURL_HANDLERS_GET(ch, write)->method == PHP_CURL_FILE && CURL_HANDLERS_GET(ch, write)->fp) {
        fflush(CURL_HANDLERS_GET(ch, write)->fp);
    }
    if (CURL_HANDLERS_GET(ch, write_header)->method == PHP_CURL_FILE && CURL_HANDLERS_GET(ch, write_header)->fp) {
        fflush(CURL_HANDLERS_GET(ch, write_header)->fp);
    }

    if (CURL_HANDLERS_GET(ch, write)->method == PHP_CURL_RETURN) {
        RETURN_EMPTY_STRING();
    } else {
        RETURN_TRUE;
    }
}
/* }}} */

/* {{{ proto resource curl_multi_init(void)
   Returns a new cURL multi handle */
static PHP_FUNCTION(swow_curl_multi_init)
{
    php_curlm *mh;

    ZEND_PARSE_PARAMETERS_NONE();

    object_init_ex(return_value, curl_multi_ce);
    mh = Z_CURL_MULTI_P(return_value);
    mh->multi = cat_curl_multi_init();
# if PHP_VERSION_ID < 80100
    mh->handlers = ecalloc(1, sizeof(php_curlm_handlers));
# endif
    ((zend_object_handlers *) mh->std.handlers)->free_obj = swow_curl_multi_free_obj;

    zend_llist_init(&mh->easyh, sizeof(zval), _swow_php_curl_multi_cleanup_list, 0);
}
/* }}} */

/* {{{ Run the sub-connections of the current cURL handle */
static PHP_FUNCTION(swow_curl_multi_exec)
{
    zval      *z_mh;
    zval      *z_still_running;
    php_curlm *mh;
    int        still_running;
    CURLMcode error = CURLM_OK;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT_OF_CLASS(z_mh, curl_multi_ce)
        Z_PARAM_ZVAL(z_still_running)
    ZEND_PARSE_PARAMETERS_END();

    mh = Z_CURL_MULTI_P(z_mh);

    {
        zend_llist_position pos;
        php_curl *ch;
        zval    *pz_ch;

        for (pz_ch = (zval *)zend_llist_get_first_ex(&mh->easyh, &pos); pz_ch;
            pz_ch = (zval *)zend_llist_get_next_ex(&mh->easyh, &pos)) {
            ch = Z_CURL_P(pz_ch);
            _swow_php_curl_verify_handlers(ch, 1);
        }
    }

    still_running = zval_get_long(z_still_running);
    error = cat_curl_multi_perform(mh->multi, &still_running);
#ifdef ZEND_TRY_ASSIGN_REF_LONG
    ZEND_TRY_ASSIGN_REF_LONG(z_still_running, still_running);
#else
    zval_ptr_dtor(z_still_running);
    ZVAL_LONG(z_still_running, still_running);
#endif

    SAVE_CURLM_ERROR(mh, error);
    RETURN_LONG((zend_long) error);
}
/* }}} */

/* {{{ Get all the sockets associated with the cURL extension, which can then be "selected" */
static PHP_FUNCTION(swow_curl_multi_select)
{
    zval           *z_mh;
    php_curlm      *mh;
    double          timeout = 1.0;
    int             numfds = 0;
    CURLMcode error = CURLM_OK;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_OBJECT_OF_CLASS(z_mh, curl_multi_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END();

    mh = Z_CURL_MULTI_P(z_mh);

    error = cat_curl_multi_wait(mh->multi, NULL, 0, (unsigned long) (timeout * 1000.0), &numfds);
    if (CURLM_OK != error) {
        SAVE_CURLM_ERROR(mh, error);
        RETURN_LONG(-1);
    }

    RETURN_LONG(numfds);
}
/* }}} */

zend_result swow_curl_module_init(INIT_FUNC_ARGS)
{
    SWOW_MODULES_CHECK_PRE_START() {
        "curl"
    } SWOW_MODULES_CHECK_PRE_END();

    curl_ce = (zend_class_entry *) zend_hash_str_find_ptr(CG(class_table), ZEND_STRL("curlhandle"));
    if (curl_ce == NULL) {
        return SUCCESS;
    }
    curl_multi_ce = (zend_class_entry *) zend_hash_str_find_ptr(CG(class_table), ZEND_STRL("curlmultihandle"));
    ZEND_ASSERT(curl_multi_ce != NULL);

    if (!cat_curl_module_init()) {
        return FAILURE;
    }

    if (!swow_hook_internal_function_handler(ZEND_STRL("curl_exec"), PHP_FN(swow_curl_exec))) {
        return FAILURE;
    }
    if (!swow_hook_internal_function_handler(ZEND_STRL("curl_multi_init"), PHP_FN(swow_curl_multi_init))) {
        return FAILURE;
    }
    if (!swow_hook_internal_function_handler(ZEND_STRL("curl_multi_exec"), PHP_FN(swow_curl_multi_exec))) {
        return FAILURE;
    }
    if (!swow_hook_internal_function_handler(ZEND_STRL("curl_multi_select"), PHP_FN(swow_curl_multi_select))) {
        return FAILURE;
    }

    return SUCCESS;
}

#define SWOW_CURL_CHECK_MODULE() do { \
    if (curl_ce == NULL) { \
        return SUCCESS; \
    } \
} while (0)

zend_result swow_curl_module_shutdown(INIT_FUNC_ARGS)
{
    SWOW_CURL_CHECK_MODULE();

    if (!cat_curl_module_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result swow_curl_runtime_init(INIT_FUNC_ARGS)
{
    SWOW_CURL_CHECK_MODULE();

    if (!cat_curl_runtime_init()) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result swow_curl_runtime_shutdown(INIT_FUNC_ARGS)
{
    SWOW_CURL_CHECK_MODULE();

    if (!cat_curl_runtime_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}

#endif /* CAT_CURL */
