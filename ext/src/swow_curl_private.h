/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Sterling Hughes <sterling@php.net>                           |
   |         Wez Furlong <wez@thebrainroom.com>                           |
   +----------------------------------------------------------------------+
*/

// from ext/curl/curl_private.h @ 89be689f778430f93bac04d75248b5b6fc593571

#ifndef _SWOW_CURL_PRIVATE_H
#define _SWOW_CURL_PRIVATE_H

#include "swow_curl.h"

#define PHP_CURL_DEBUG 0

#include "php_version.h"
#define PHP_CURL_VERSION PHP_VERSION

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

#ifndef HAVE_LIBCAT
ZEND_BEGIN_MODULE_GLOBALS(curl)
    HashTable persistent_curlsh;
ZEND_END_MODULE_GLOBALS(curl)

ZEND_EXTERN_MODULE_GLOBALS(curl)

#define CURL_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(curl, v)

PHP_MINIT_FUNCTION(curl);
PHP_MSHUTDOWN_FUNCTION(curl);
PHP_MINFO_FUNCTION(curl);
PHP_GINIT_FUNCTION(curl);
PHP_GSHUTDOWN_FUNCTION(curl);
#endif

typedef struct {
    swow_fcall_info_cache fcc;
    FILE                 *fp;
    smart_str             buf;
    int                   method;
    zval                    stream;
} php_curl_write;

typedef struct {
    swow_fcall_info_cache fcc;
    FILE                 *fp;
    zend_resource        *res;
    int                   method;
    zval                  stream;
} php_curl_read;

typedef struct {
    php_curl_write    *write;
    php_curl_write    *write_header;
    php_curl_read     *read;
    zval               std_err;
    swow_fcall_info_cache progress;
    swow_fcall_info_cache xferinfo;
    swow_fcall_info_cache fnmatch;
    swow_fcall_info_cache debug;
#if LIBCURL_VERSION_NUM >= 0x075000 /* Available since 7.80.0 */
    swow_fcall_info_cache prereq;
#endif
#if LIBCURL_VERSION_NUM >= 0x075400 /* Available since 7.84.0 */
    swow_fcall_info_cache sshhostkey;
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
    zend_llist post;
    zend_llist stream;
    HashTable slist;
};

typedef struct {
    CURL                         *cp;
    php_curl_handlers             handlers;
    struct _php_curl_free        *to_free;
    struct _php_curl_send_headers header;
    struct _php_curl_error        err;
    bool                     in_callback;
    uint32_t*                     clone;
    zval                          postfields;
    /* For CURLOPT_PRIVATE */
    zval private_data;
    /* CurlShareHandle object set using CURLOPT_SHARE. */
    struct _php_curlsh *share;
    zend_object                   std;
} php_curl;

#define CURLOPT_SAFE_UPLOAD -1

typedef struct {
    swow_fcall_info_cache server_push;
} php_curlm_handlers;

typedef struct {
    CURLM      *multi;
    zend_llist  easyh;
    php_curlm_handlers handlers;
    struct {
        int no;
    } err;
    zend_object std;
} php_curlm;

typedef struct _php_curlsh {
    CURLSH                   *share;
    struct {
        int no;
    } err;
    zend_object std;
} php_curlsh;

php_curl *swow_init_curl_handle_into_zval(zval *curl);
void swow_init_curl_handle(php_curl *ch);
void _swow_curl_cleanup_handle(php_curl *);
void _swow_curl_multi_cleanup_list(void *data);
void _swow_curl_verify_handlers(php_curl *ch, bool reporterror);
void _swow_setup_easy_copy_handlers(php_curl *ch, php_curl *source);

/* Consumes `zv` */
zend_long swow_curl_get_long(zval *zv);

static inline php_curl *swow_curl_from_obj(zend_object *obj) {
    return (php_curl *)((char *)(obj) - XtOffsetOf(php_curl, std));
}

#define Z_CURL_P(zv) swow_curl_from_obj(Z_OBJ_P(zv))

static inline php_curlsh *swow_curl_share_from_obj(zend_object *obj) {
    return (php_curlsh *)((char *)(obj) - XtOffsetOf(php_curlsh, std));
}

#define Z_CURL_SHARE_P(zv) swow_curl_share_from_obj(Z_OBJ_P(zv))

void swow_curl_multi_register_handlers(void);
void swow_curl_share_register_handlers(void);
void swow_curl_share_persistent_register_handlers(void);
void swow_curl_share_free_persistent_curlsh(zval *data);
void swow_curlfile_register_class(void);
#if PHP_VERSION_ID < 80200
int swow_curl_cast_object(zend_object *obj, zval *result, int type);
#else
zend_result swow_curl_cast_object(zend_object *obj, zval *result, int type);
#endif

#endif  /* _SWOW_CURL_PRIVATE_H */
