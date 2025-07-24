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
   | Author: Stanislav Malyshev <stas@php.net>                            |
   +----------------------------------------------------------------------+
 */

// from ext/curl/curl_file.c @ 263b22f3744c5e8e878e4d5841dc14e49671655c

// @see: https://github.com/php/php-src/pull/13347
#if !defined(__cplusplus) && !defined(_MSC_VER) && defined(HAVE_WTYPEDEF_REDEFINITION)
#pragma GCC diagnostic ignored "-Wtypedef-redefinition"
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "swow.h"
#include "Zend/zend_exceptions.h"
#include "swow_curl_private.h"
#include "swow_curl_file_arginfo.h"

SWOW_API zend_class_entry *swow_curl_CURLFile_class;
SWOW_API zend_class_entry *swow_curl_CURLStringFile_class;

static void swow_curlfile_ctor(INTERNAL_FUNCTION_PARAMETERS)
{
    zend_string *fname, *mime = NULL, *postname = NULL;
    zval *cf = return_value;

    ZEND_PARSE_PARAMETERS_START(1,3)
        Z_PARAM_PATH_STR(fname)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR_OR_NULL(mime)
        Z_PARAM_STR_OR_NULL(postname)
    ZEND_PARSE_PARAMETERS_END();

    zend_update_property_str(swow_curl_CURLFile_class, Z_OBJ_P(cf), "name", sizeof("name")-1, fname);

    if (mime) {
        zend_update_property_str(swow_curl_CURLFile_class, Z_OBJ_P(cf), "mime", sizeof("mime")-1, mime);
    }

    if (postname) {
        zend_update_property_str(swow_curl_CURLFile_class, Z_OBJ_P(cf), "postname", sizeof("postname")-1, postname);
    }
}

/* {{{ Create the Swow_CURLFile object */
ZEND_METHOD(Swow_CURLFile, __construct)
{
    return_value = ZEND_THIS;
    swow_curlfile_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ Create the Swow_CURLFile object */
PHP_FUNCTION(swow_curl_file_create)
{
    object_init_ex( return_value, swow_curl_CURLFile_class );
    swow_curlfile_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

static void swow_curlfile_get_property(const char *name, size_t name_len, INTERNAL_FUNCTION_PARAMETERS)
{
    zval *res, rv;

    ZEND_PARSE_PARAMETERS_NONE();
    res = zend_read_property(swow_curl_CURLFile_class, Z_OBJ_P(ZEND_THIS), name, name_len, 1, &rv);
    RETURN_COPY_DEREF(res);
}

static void swow_curlfile_set_property(const char *name, size_t name_len, INTERNAL_FUNCTION_PARAMETERS)
{
    zend_string *arg;

    ZEND_PARSE_PARAMETERS_START(1,1)
        Z_PARAM_STR(arg)
    ZEND_PARSE_PARAMETERS_END();

    zend_update_property_str(swow_curl_CURLFile_class, Z_OBJ_P(ZEND_THIS), name, name_len, arg);
}

/* {{{ Get file name */
ZEND_METHOD(Swow_CURLFile, getFilename)
{
    swow_curlfile_get_property("name", sizeof("name")-1, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ Get MIME type */
ZEND_METHOD(Swow_CURLFile, getMimeType)
{
    swow_curlfile_get_property("mime", sizeof("mime")-1, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ Get file name for POST */
ZEND_METHOD(Swow_CURLFile, getPostFilename)
{
    swow_curlfile_get_property("postname", sizeof("postname")-1, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ Set MIME type */
ZEND_METHOD(Swow_CURLFile, setMimeType)
{
    swow_curlfile_set_property("mime", sizeof("mime")-1, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ Set file name for POST */
ZEND_METHOD(Swow_CURLFile, setPostFilename)
{
    swow_curlfile_set_property("postname", sizeof("postname")-1, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

ZEND_METHOD(Swow_CURLStringFile, __construct)
{
    zend_string *data, *postname, *mime = NULL;
    zval *object;

    object = ZEND_THIS;

    ZEND_PARSE_PARAMETERS_START(2,3)
        Z_PARAM_STR(data)
        Z_PARAM_STR(postname)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(mime)
    ZEND_PARSE_PARAMETERS_END();

    zend_update_property_str(swow_curl_CURLStringFile_class, Z_OBJ_P(object), "data", sizeof("data") - 1, data);
    zend_update_property_str(swow_curl_CURLStringFile_class, Z_OBJ_P(object), "postname", sizeof("postname")-1, postname);
    if (mime) {
        zend_update_property_str(swow_curl_CURLStringFile_class, Z_OBJ_P(object), "mime", sizeof("mime")-1, mime);
    } else {
        zend_update_property_string(swow_curl_CURLStringFile_class, Z_OBJ_P(object), "mime", sizeof("mime")-1, "application/octet-stream");
    }
}

void swow_curlfile_register_class(void)
{
    swow_curl_CURLFile_class = register_class_CURLFile();

    swow_curl_CURLStringFile_class = register_class_CURLStringFile();
}
