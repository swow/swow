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

#include "swow_http.h"

#include "swow_buffer.h"

SWOW_API zend_class_entry *swow_http_status_ce;

SWOW_API zend_class_entry *swow_http_parser_ce;
SWOW_API zend_object_handlers swow_http_parser_handlers;
SWOW_API zend_class_entry *swow_http_parser_exception_ce;

/* Status */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_http_status_getReasonPhrase, ZEND_RETURN_VALUE, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, statusCode, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_http_status, getReasonPhrase)
{
    zend_long status_code;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(status_code)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING(cat_http_status_get_reason(status_code));
}

static const zend_function_entry swow_http_status_methods[] = {
    PHP_ME(swow_http_status, getReasonPhrase, arginfo_swow_http_status_getReasonPhrase, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

/* Parser */

static zend_object *swow_http_parser_create_object(zend_class_entry *ce)
{
    swow_http_parser_t *sparser = swow_object_alloc(swow_http_parser_t, ce, swow_http_parser_handlers);

    cat_http_parser_init(&sparser->parser);
    sparser->data_offset = 0;

    return &sparser->std;
}

#define getThisParser() (swow_http_parser_get_from_object(Z_OBJ_P(ZEND_THIS)))

#define SWOW_HTTP_PARSER_GETTER(_sparser, _parser) \
    swow_http_parser_t *_sparser = getThisParser(); \
    cat_http_parser_t *_parser = &_sparser->parser

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_http_parser_getLong, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_http_parser_getType arginfo_swow_http_parser_getLong

static PHP_METHOD(swow_http_parser, getType)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_type(parser));
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_http_parser_setType, ZEND_RETURN_VALUE, 1, Swow\\Http\\Parser, 0)
    ZEND_ARG_TYPE_INFO(0, type, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_http_parser, setType)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);
    zend_long ptype;
    cat_http_parser_type_t type;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(ptype)
    ZEND_PARSE_PARAMETERS_END();

    switch(ptype)
    {
        case CAT_HTTP_PARSER_TYPE_BOTH:
            type = CAT_HTTP_PARSER_TYPE_BOTH;
            break;
        case CAT_HTTP_PARSER_TYPE_REQUEST:
            type = CAT_HTTP_PARSER_TYPE_REQUEST;
            break;
        case CAT_HTTP_PARSER_TYPE_RESPONSE:
            type = CAT_HTTP_PARSER_TYPE_RESPONSE;
            break;
        default:
            zend_throw_error(NULL, "Unknown HTTP-Parser type");
            RETURN_THROWS();
    }
    cat_http_parser_set_type(parser, type);

    RETURN_THIS();
}

#define arginfo_swow_http_parser_getEvents arginfo_swow_http_parser_getLong

static PHP_METHOD(swow_http_parser, getEvents)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_events(parser));
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_http_parser_setEvents, ZEND_RETURN_VALUE, 1, Swow\\Http\\Parser, 0)
    ZEND_ARG_TYPE_INFO(0, events, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_http_parser, setEvents)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);
    zend_long events;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(events)
    ZEND_PARSE_PARAMETERS_END();

    cat_http_parser_set_events(parser, events);

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_http_parser_execute, ZEND_RETURN_VALUE, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, data, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_http_parser, execute)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);
    zval *zbuffer;
    swow_buffer_t *sbuffer;
    const char *buffer;
    size_t length;
    zval *zdata = NULL;
    ssize_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_OBJECT_OF_CLASS(zbuffer, swow_buffer_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL_EX(zdata, 0, 1)
    ZEND_PARSE_PARAMETERS_END();

    sbuffer = swow_buffer_get_from_object(Z_OBJ_P(zbuffer));
    length = swow_buffer_get_readable_space(sbuffer, &buffer);

    ret = cat_http_parser_execute(parser, buffer, length);

    /* anyway, update the parsed length */
    sparser->parsed_length = cat_http_parser_get_parsed_length(parser, buffer);
    swow_buffer_virtual_read(sbuffer, sparser->parsed_length);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_http_parser_exception_ce);
        RETURN_THROWS();
    }

    sparser->data_offset = parser->data - sbuffer->buffer.value;

    if (zdata != NULL && (parser->event & CAT_HTTP_PARSER_EVENT_FLAG_DATA)) {
        zval_ptr_dtor(zdata);
        ZVAL_STRINGL(zdata, parser->data, parser->data_length);
    }

    RETURN_LONG(parser->event);
}

#if 0
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_http_parser_executeString, ZEND_RETURN_VALUE, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, string, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, string_offset, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, string_length, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_http_parser, executeString)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);
    zend_string *string;
    zend_long offset = 0;
    const char *data;
    zend_long length = 0;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_STR(string)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(offset)
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    SWOW_BUFFER_CHECK_STRING_SCOPE(string, offset, length);
    data = ZSTR_VAL(string) + offset;

    ret = cat_http_parser_execute(parser, data, length);

    /* anyway, update the parsed length */
    sparser->parsed_length = cat_http_parser_get_parsed_length(parser, data);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_http_parser_exception_ce);
        RETURN_THROWS();
    }

    sparser->data_offset = parser->data - ZSTR_VAL(string);

    RETURN_LONG(parser->event);
}
#endif

#define arginfo_swow_http_parser_getEvent arginfo_swow_http_parser_getLong

static PHP_METHOD(swow_http_parser, getEvent)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(parser->event);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_http_parser_getString, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_http_parser_getEventName arginfo_swow_http_parser_getString

static PHP_METHOD(swow_http_parser, getEventName)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_http_parser_get_event_name(parser));
}

#define arginfo_swow_http_parser_getDataOffset arginfo_swow_http_parser_getLong

static PHP_METHOD(swow_http_parser, getDataOffset)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(getThisParser()->data_offset);
}

#define arginfo_swow_http_parser_getDataLength arginfo_swow_http_parser_getLong

static PHP_METHOD(swow_http_parser, getDataLength)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(parser->data_length);
}

#define arginfo_swow_http_parser_getParsedLength arginfo_swow_http_parser_getLong

static PHP_METHOD(swow_http_parser, getParsedLength)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(getThisParser()->parsed_length);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_http_parser_getBool, ZEND_RETURN_VALUE, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_http_parser_isCompleted arginfo_swow_http_parser_getBool

static PHP_METHOD(swow_http_parser, isCompleted)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_http_parser_is_completed(parser));
}

#define arginfo_swow_http_parser_shouldKeepAlive arginfo_swow_http_parser_getBool

static PHP_METHOD(swow_http_parser, shouldKeepAlive)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(parser->keep_alive);
}

#define arginfo_swow_http_parser_getMethod arginfo_swow_http_parser_getString

static PHP_METHOD(swow_http_parser, getMethod)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_http_parser_get_method_name(parser));
}

#define arginfo_swow_http_parser_getMajorVersion arginfo_swow_http_parser_getLong

static PHP_METHOD(swow_http_parser, getMajorVersion)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_major_version(parser));
}

#define arginfo_swow_http_parser_getMinorVersion arginfo_swow_http_parser_getLong

static PHP_METHOD(swow_http_parser, getMinorVersion)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_minor_version(parser));
}

#define arginfo_swow_http_parser_getProtocolVersion arginfo_swow_http_parser_getString

static PHP_METHOD(swow_http_parser, getProtocolVersion)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_http_parser_get_protocol_version(parser));
}

#define arginfo_swow_http_parser_getStatusCode arginfo_swow_http_parser_getLong

static PHP_METHOD(swow_http_parser, getStatusCode)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_status_code(parser));
}

#define arginfo_swow_http_parser_getReasonPhrase arginfo_swow_http_parser_getString

static PHP_METHOD(swow_http_parser, getReasonPhrase)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_http_parser_get_reason_phrase(parser));
}

#define arginfo_swow_http_parser_getContentLength arginfo_swow_http_parser_getLong

static PHP_METHOD(swow_http_parser, getContentLength)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_content_length(parser));
}

#define arginfo_swow_http_parser_isUpgrade arginfo_swow_http_parser_getBool

static PHP_METHOD(swow_http_parser, isUpgrade)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_http_parser_is_upgrade(parser));
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_http_parser_finish, ZEND_RETURN_VALUE, 0, Swow\\Http\\Parser, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_http_parser, finish)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = cat_http_parser_finish(parser);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_http_parser_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_http_parser_reset, ZEND_RETURN_VALUE, 0, Swow\\Http\\Parser, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_http_parser, reset)
{
    SWOW_HTTP_PARSER_GETTER(sparser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    cat_http_parser_reset(parser);
    sparser->parsed_length = 0;
    sparser->data_offset = 0;

    RETURN_THIS();
}

static const zend_function_entry swow_http_parser_methods[] = {
    PHP_ME(swow_http_parser, getType,            arginfo_swow_http_parser_getType,            ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, setType,            arginfo_swow_http_parser_setType,            ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getEvents,          arginfo_swow_http_parser_getEvents,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, setEvents,          arginfo_swow_http_parser_setEvents,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, execute,            arginfo_swow_http_parser_execute,            ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getEvent,           arginfo_swow_http_parser_getEvent,           ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getEventName,       arginfo_swow_http_parser_getEventName,       ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getDataOffset,      arginfo_swow_http_parser_getDataOffset,      ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getDataLength,      arginfo_swow_http_parser_getDataLength,      ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getParsedLength,    arginfo_swow_http_parser_getParsedLength,    ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, isCompleted,        arginfo_swow_http_parser_isCompleted,        ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, shouldKeepAlive,    arginfo_swow_http_parser_shouldKeepAlive,    ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getMethod,          arginfo_swow_http_parser_getMethod,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getMajorVersion,    arginfo_swow_http_parser_getMajorVersion,    ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getMinorVersion,    arginfo_swow_http_parser_getMinorVersion,    ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getProtocolVersion, arginfo_swow_http_parser_getProtocolVersion, ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getStatusCode,      arginfo_swow_http_parser_getStatusCode,      ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getReasonPhrase,    arginfo_swow_http_parser_getReasonPhrase,    ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, getContentLength,   arginfo_swow_http_parser_getContentLength,   ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, isUpgrade,          arginfo_swow_http_parser_isUpgrade,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, finish,             arginfo_swow_http_parser_finish,             ZEND_ACC_PUBLIC)
    PHP_ME(swow_http_parser, reset,              arginfo_swow_http_parser_reset,              ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static cat_always_inline size_t swow_http_get_header_length(zend_string *header_name, zval *zheader_value)
{
    zend_string *header_value, *tmp_header_value;
    size_t size;

    header_value = zval_get_tmp_string(zheader_value, &tmp_header_value);
    size = ZSTR_LEN(header_name) + CAT_STRLEN(": ") + ZSTR_LEN(header_value) + CAT_STRLEN("\r\n");
    zend_tmp_string_release(tmp_header_value);

    return size;
}

static cat_always_inline size_t swow_http_get_message_length(HashTable *headers, zend_string *body)
{
    zend_string *header_name;
    zval *zheader_value;
    size_t size = 0;

    ZEND_HASH_FOREACH_STR_KEY_VAL(headers, header_name, zheader_value) {
        if (UNEXPECTED(header_name == NULL)) {
            continue;
        }
        if (Z_TYPE_P(zheader_value) != IS_ARRAY) {
            size += swow_http_get_header_length(header_name, zheader_value);
        } else {
            ZEND_HASH_FOREACH_VAL(Z_ARR_P(zheader_value), zheader_value) {
                size += swow_http_get_header_length(header_name, zheader_value);
            } ZEND_HASH_FOREACH_END();
        }
    } ZEND_HASH_FOREACH_END();

    size += CAT_STRLEN("\r\n");

    size += ZSTR_LEN(body);

    return size;
}

static cat_always_inline char *swow_http_pack_header(char *p, zend_string *header_name, zval *zheader_value)
{
    zend_string *header_value, *tmp_header_value;

    header_value = zval_get_tmp_string(zheader_value, &tmp_header_value);
    p = cat_memcpy(p, ZSTR_VAL(header_name), ZSTR_LEN(header_name));
    p = cat_memcpy(p, CAT_STRL(": "));
    p = cat_memcpy(p, ZSTR_VAL(header_value), ZSTR_LEN(header_value));
    p = cat_memcpy(p, CAT_STRL("\r\n"));
    zend_tmp_string_release(tmp_header_value);

    return p;
}

static cat_always_inline char *swow_http_pack_headers(char *p, HashTable *headers)
{
    zend_string *header_name;
    zval *zheader_value;

    ZEND_HASH_FOREACH_STR_KEY_VAL(headers, header_name, zheader_value) {
        if (UNEXPECTED(header_name == NULL)) {
            continue;
        }
        if (Z_TYPE_P(zheader_value) != IS_ARRAY) {
            p = swow_http_pack_header(p, header_name, zheader_value);
        } else {
            ZEND_HASH_FOREACH_VAL(Z_ARR_P(zheader_value), zheader_value) {
                p = swow_http_pack_header(p, header_name, zheader_value);
            } ZEND_HASH_FOREACH_END();
        }
    } ZEND_HASH_FOREACH_END();

    return p;
}

static cat_always_inline char* swow_http_pack_message(char *p, HashTable *headers, zend_string *body)
{
    p = swow_http_pack_headers(p, headers);

    p = cat_memcpy(p, CAT_STRL("\r\n"));

    if (ZSTR_LEN(body) > 0) {
        p = cat_memcpy(p, ZSTR_VAL(body), ZSTR_LEN(body));
    }

    *p = '\0';

    return p;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_http_packMessage, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, headers, IS_ARRAY, 0, "[]")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, body, IS_STRING, 0, "\"\"")
ZEND_END_ARG_INFO()

static PHP_FUNCTION(swow_http_packMessage)
{
    zend_string *message = zend_empty_string;
    /* arguments */
    zval *zheaders = NULL;
    HashTable *headers = (HashTable *) &zend_empty_array;
    zend_string *body = zend_empty_string;
    /* pack */
    char *p;
    size_t size;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(zheaders)
        Z_PARAM_STR(body)
    ZEND_PARSE_PARAMETERS_END();

    if (zheaders != NULL) {
        headers = Z_ARR_P(zheaders);
    }

    size = swow_http_get_message_length(headers, body);

    message = zend_string_alloc(size, 0);

    p = ZSTR_VAL(message);
    p = swow_http_pack_message(p, headers, body);
    *p = '\0';

    RETURN_STR(message);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_http_packRequest, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, method, IS_STRING, 0, "\"\"")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, url, IS_STRING, 0, "\"\"")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, headers, IS_ARRAY, 0, "[]")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, body, IS_STRING, 0, "\"\"")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, protocolVersion, IS_STRING, 0, "\"\"")
ZEND_END_ARG_INFO()

static PHP_FUNCTION(swow_http_packRequest)
{
    zend_string *request;
    /* arguments */
    zend_string *method = zend_empty_string;
    zend_string *url = zend_empty_string;
    char *protocol_version = "1.1";
    size_t protocol_version_length = CAT_STRLEN("1.1");
    HashTable *headers = (HashTable *) &zend_empty_array;
    zend_string *body = zend_empty_string;
    /* pack */
    char *p;
    size_t size;

    ZEND_PARSE_PARAMETERS_START(0, 5)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(method)
        Z_PARAM_STR(url)
        Z_PARAM_ARRAY_HT(headers)
        Z_PARAM_STR(body)
        Z_PARAM_STRING(protocol_version, protocol_version_length)
    ZEND_PARSE_PARAMETERS_END();

    size = ZSTR_LEN(method) + CAT_STRLEN(" ") +
           ZSTR_LEN(url) + CAT_STRLEN(" ") +
           CAT_STRLEN("HTTP/") + protocol_version_length + CAT_STRLEN("\r\n");

    size += swow_http_get_message_length(headers, body);

    request = zend_string_alloc(size, 0);

    p = ZSTR_VAL(request);
    p = cat_memcpy(p, ZSTR_VAL(method), ZSTR_LEN(method));
    p = cat_memcpy(p, CAT_STRL(" "));
    p = cat_memcpy(p, ZSTR_VAL(url), ZSTR_LEN(url));
    p = cat_memcpy(p, CAT_STRL(" HTTP/"));
    p = cat_memcpy(p, protocol_version, protocol_version_length);
    p = cat_memcpy(p, CAT_STRL("\r\n"));

    (void) swow_http_pack_message(p, headers, body);

    RETURN_STR(request);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_http_packResponse, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, statusCode, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, headers, IS_ARRAY, 0, "[]")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, body, IS_STRING, 0, "\"\"")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, reasonPhrase, IS_STRING, 0, "\"\"")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, protocolVersion, IS_STRING, 0, "\"\"")
ZEND_END_ARG_INFO()

static PHP_FUNCTION(swow_http_packResponse)
{
    zend_string *response;
    /* arguments */
    char *protocol_version = "1.1";
    size_t protocol_version_length = CAT_STRLEN("1.1");
    zend_long status_code = CAT_HTTP_STATUS_OK;
    char status_code_buffer[MAX_LENGTH_OF_LONG + 1];
    char *status_code_string, *status_code_string_eof;
    size_t status_code_length;
    char *reason_phrase = NULL;
    size_t reason_phrase_length = 0;
    HashTable *headers = (HashTable *) &zend_empty_array;
    zend_string *body = zend_empty_string;
    /* pack */
    char *p;
    size_t size;

    ZEND_PARSE_PARAMETERS_START(0, 5)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(status_code)
        Z_PARAM_ARRAY_HT(headers)
        Z_PARAM_STR(body)
        Z_PARAM_STRING(reason_phrase, reason_phrase_length)
        Z_PARAM_STRING(protocol_version, protocol_version_length)
    ZEND_PARSE_PARAMETERS_END();

    status_code_string_eof = status_code_buffer + sizeof(status_code_buffer) - 1;
    status_code_string = zend_print_long_to_buf(status_code_string_eof, status_code);
    status_code_length = status_code_string_eof - status_code_string;
    if (reason_phrase_length == 0) {
        reason_phrase = (char *) cat_http_status_get_reason(status_code);
        reason_phrase_length = strlen(reason_phrase);
    }

    size = CAT_STRLEN("HTTP/") + protocol_version_length + CAT_STRLEN(" ") +
           status_code_length + CAT_STRLEN(" ") +
           reason_phrase_length + CAT_STRLEN("\r\n");

    size += swow_http_get_message_length(headers, body);

    response = zend_string_alloc(size, 0);

    p = ZSTR_VAL(response);
    p = cat_memcpy(p, CAT_STRL("HTTP/"));
    p = cat_memcpy(p, protocol_version, protocol_version_length);
    p = cat_memcpy(p, CAT_STRL(" "));
    p = cat_memcpy(p, status_code_string, status_code_length);
    p = cat_memcpy(p, CAT_STRL(" "));
    p = cat_memcpy(p, reason_phrase, reason_phrase_length);
    p = cat_memcpy(p, CAT_STRL("\r\n"));

    (void) swow_http_pack_message(p, headers, body);

    RETURN_STR(response);
}

static const zend_function_entry swow_http_functions[] = {
    PHP_FENTRY(Swow\\Http\\packMessage,  PHP_FN(swow_http_packMessage),  arginfo_swow_http_packMessage,  0)
    PHP_FENTRY(Swow\\Http\\packRequest,  PHP_FN(swow_http_packRequest),  arginfo_swow_http_packRequest,  0)
    PHP_FENTRY(Swow\\Http\\packResponse, PHP_FN(swow_http_packResponse), arginfo_swow_http_packResponse, 0)
    PHP_FE_END
};

int swow_http_module_init(INIT_FUNC_ARGS)
{
    /* Status */
    swow_http_status_ce = swow_register_internal_class(
        "Swow\\Http\\Status", NULL, swow_http_status_methods,
        NULL, NULL, cat_false, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );
#define SWOW_HTTP_STATUS_GEN(name, value, unused)  zend_declare_class_constant_long(swow_http_status_ce, ZEND_STRL(#name), value);
    CAT_HTTP_STATUS_MAP(SWOW_HTTP_STATUS_GEN)
#undef SWOW_HTTP_STATUS_GEN

    /* Parser */
    swow_http_parser_ce = swow_register_internal_class(
        "Swow\\Http\\Parser", NULL, swow_http_parser_methods,
        &swow_http_parser_handlers, NULL,
        cat_false, cat_false, cat_false,
        swow_http_parser_create_object, NULL,
        XtOffsetOf(swow_http_parser_t, std)
    );
    zend_declare_class_constant_long(swow_http_parser_ce, ZEND_STRL("TYPE_BOTH"), CAT_HTTP_PARSER_TYPE_BOTH);
    zend_declare_class_constant_long(swow_http_parser_ce, ZEND_STRL("TYPE_REQUEST"), CAT_HTTP_PARSER_TYPE_REQUEST);
    zend_declare_class_constant_long(swow_http_parser_ce, ZEND_STRL("TYPE_RESPONSE"), CAT_HTTP_PARSER_TYPE_RESPONSE);
    zend_declare_class_constant_long(swow_http_parser_ce, ZEND_STRL("EVENT_FLAG_NONE"), CAT_HTTP_PARSER_EVENT_FLAG_NONE);
    zend_declare_class_constant_long(swow_http_parser_ce, ZEND_STRL("EVENT_FLAG_LINE"), CAT_HTTP_PARSER_EVENT_FLAG_LINE);
    zend_declare_class_constant_long(swow_http_parser_ce, ZEND_STRL("EVENT_FLAG_DATA"), CAT_HTTP_PARSER_EVENT_FLAG_DATA);
#define SWOW_HTTP_PARSER_EVENT_GEN(name, value) zend_declare_class_constant_long(swow_http_parser_ce, ZEND_STRL("EVENT_" #name), value);
    CAT_HTTP_PARSER_EVENT_MAP(SWOW_HTTP_PARSER_EVENT_GEN)
#undef SWOW_HTTP_PARSER_EVENT_GEN
    zend_declare_class_constant_long(swow_http_parser_ce, ZEND_STRL("EVENTS_NONE"), CAT_HTTP_PARSER_EVENTS_NONE);
    zend_declare_class_constant_long(swow_http_parser_ce, ZEND_STRL("EVENTS_ALL"), CAT_HTTP_PARSER_EVENTS_ALL);
    /* Parser\\Exception */
    swow_http_parser_exception_ce = swow_register_internal_class(
        "Swow\\Http\\Parser\\Exception", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );

    if (zend_register_functions(NULL, swow_http_functions, NULL, MODULE_PERSISTENT) != SUCCESS) {
        return FAILURE;
    }

    return SUCCESS;
}
