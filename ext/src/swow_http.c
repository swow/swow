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

SWOW_API zend_class_entry *swow_http_http_ce;

SWOW_API zend_class_entry *swow_http_status_ce;

SWOW_API zend_class_entry *swow_http_parser_ce;
SWOW_API zend_object_handlers swow_http_parser_handlers;
SWOW_API zend_class_entry *swow_http_parser_exception_ce;

/* Status */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Http_Status_getReasonPhraseOf, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, statusCode, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Http_Status, getReasonPhraseOf)
{
    zend_long status_code;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(status_code)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING(cat_http_status_get_reason(status_code));
}

static const zend_function_entry swow_http_status_methods[] = {
    PHP_ME(Swow_Http_Status, getReasonPhraseOf, arginfo_class_Swow_Http_Status_getReasonPhraseOf, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

/* Parser */

static zend_object *swow_http_parser_create_object(zend_class_entry *ce)
{
    swow_http_parser_t *s_parser = swow_object_alloc(swow_http_parser_t, ce, swow_http_parser_handlers);

    cat_http_parser_init(&s_parser->parser);
    s_parser->data_offset = 0;

    return &s_parser->std;
}

#define getThisParser() (swow_http_parser_get_from_object(Z_OBJ_P(ZEND_THIS)))

#define SWOW_HTTP_PARSER_GETTER(_sparser, _parser) \
    swow_http_parser_t *_sparser = getThisParser(); \
    cat_http_parser_t *_parser = &_sparser->parser

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Http_Parser_getType, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Http_Parser, getType)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_type(parser));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Http_Parser_setType, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, type, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Http_Parser, setType)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);
    zend_long ptype;
    cat_http_parser_type_t type;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(ptype)
    ZEND_PARSE_PARAMETERS_END();

    switch (ptype) {
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
            zend_value_error("Unknown HTTP-Parser type " ZEND_LONG_FMT, ptype);
            RETURN_THROWS();
    }
    cat_http_parser_set_type(parser, type);

    RETURN_THIS();
}

#define arginfo_class_Swow_Http_Parser_getEvents arginfo_class_Swow_Http_Parser_getType

static PHP_METHOD(Swow_Http_Parser, getEvents)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_events(parser));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Http_Parser_setEvents, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, events, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Http_Parser, setEvents)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);
    zend_long events;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(events)
    ZEND_PARSE_PARAMETERS_END();

    cat_http_parser_set_events(parser, events);

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Http_Parser_execute, 0, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_TYPE_MASK(0, data, Stringable, MAY_BE_STRING, NULL)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, start, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Http_Parser, execute)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);
    zend_string *string;
    zend_long start = 0;
    zend_long length = -1;
    const char *ptr;
    ssize_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        SWOW_PARAM_STRINGABLE_EXPECT_BUFFER_FOR_READING(string)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(start)
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    /* check args and initialize */
    ptr = swow_string_get_readable_space(string, start, &length, 1);

    if (UNEXPECTED(ptr == NULL)) {
        RETURN_THROWS();
    }

    ret = cat_http_parser_execute(parser, ptr, length);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_http_parser_exception_ce);
        RETURN_THROWS();
    }

    s_parser->data_offset = parser->data - ZSTR_VAL(string);

    RETURN_LONG(parser->parsed_length);
}

#define arginfo_class_Swow_Http_Parser_getEvent arginfo_class_Swow_Http_Parser_getType

static PHP_METHOD(Swow_Http_Parser, getEvent)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(parser->event);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Http_Parser_getEventName, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Http_Parser, getEventName)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    // TODO: interned strings?
    RETURN_STRING(cat_http_parser_get_event_name(parser));
}

#define arginfo_class_Swow_Http_Parser_getPreviousEvent arginfo_class_Swow_Http_Parser_getType

static PHP_METHOD(Swow_Http_Parser, getPreviousEvent)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(parser->previous_event);
}

#define arginfo_class_Swow_Http_Parser_getPreviousEventName arginfo_class_Swow_Http_Parser_getEventName

static PHP_METHOD(Swow_Http_Parser, getPreviousEventName)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    // TODO: interned strings?
    RETURN_STRING(cat_http_parser_get_previous_event_name(parser));
}

#define arginfo_class_Swow_Http_Parser_getDataOffset arginfo_class_Swow_Http_Parser_getType

static PHP_METHOD(Swow_Http_Parser, getDataOffset)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(getThisParser()->data_offset);
}

#define arginfo_class_Swow_Http_Parser_getDataLength arginfo_class_Swow_Http_Parser_getType

static PHP_METHOD(Swow_Http_Parser, getDataLength)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(parser->data_length);
}

#define arginfo_class_Swow_Http_Parser_getParsedLength arginfo_class_Swow_Http_Parser_getType

static PHP_METHOD(Swow_Http_Parser, getParsedLength)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_parsed_length(parser));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Http_Parser_isCompleted, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Http_Parser, isCompleted)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_http_parser_is_completed(parser));
}

#define arginfo_class_Swow_Http_Parser_shouldKeepAlive arginfo_class_Swow_Http_Parser_isCompleted

static PHP_METHOD(Swow_Http_Parser, shouldKeepAlive)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(parser->keep_alive);
}

#define arginfo_class_Swow_Http_Parser_getMethod arginfo_class_Swow_Http_Parser_getEventName

static PHP_METHOD(Swow_Http_Parser, getMethod)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_http_parser_get_method_name(parser));
}

#define arginfo_class_Swow_Http_Parser_getMajorVersion arginfo_class_Swow_Http_Parser_getType

static PHP_METHOD(Swow_Http_Parser, getMajorVersion)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_major_version(parser));
}

#define arginfo_class_Swow_Http_Parser_getMinorVersion arginfo_class_Swow_Http_Parser_getType

static PHP_METHOD(Swow_Http_Parser, getMinorVersion)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_minor_version(parser));
}

#define arginfo_class_Swow_Http_Parser_getProtocolVersion arginfo_class_Swow_Http_Parser_getEventName

static PHP_METHOD(Swow_Http_Parser, getProtocolVersion)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_http_parser_get_protocol_version(parser));
}

#define arginfo_class_Swow_Http_Parser_getStatusCode arginfo_class_Swow_Http_Parser_getType

static PHP_METHOD(Swow_Http_Parser, getStatusCode)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_status_code(parser));
}

#define arginfo_class_Swow_Http_Parser_getReasonPhrase arginfo_class_Swow_Http_Parser_getEventName

static PHP_METHOD(Swow_Http_Parser, getReasonPhrase)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_http_parser_get_reason_phrase(parser));
}

#define arginfo_class_Swow_Http_Parser_getContentLength arginfo_class_Swow_Http_Parser_getType

static PHP_METHOD(Swow_Http_Parser, getContentLength)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_content_length(parser));
}

#define arginfo_class_Swow_Http_Parser_getCurrentChunkLength arginfo_class_Swow_Http_Parser_getType

static PHP_METHOD(Swow_Http_Parser, getCurrentChunkLength)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_http_parser_get_current_chunk_length(parser));
}

#define arginfo_class_Swow_Http_Parser_isChunked arginfo_class_Swow_Http_Parser_isCompleted

static PHP_METHOD(Swow_Http_Parser, isChunked)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_http_parser_is_chunked(parser));
}

#define arginfo_class_Swow_Http_Parser_isMultipart arginfo_class_Swow_Http_Parser_isCompleted

static PHP_METHOD(Swow_Http_Parser, isMultipart)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_http_parser_is_multipart(parser));
}

#define arginfo_class_Swow_Http_Parser_isUpgrade arginfo_class_Swow_Http_Parser_isCompleted

static PHP_METHOD(Swow_Http_Parser, isUpgrade)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_http_parser_is_upgrade(parser));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Http_Parser_finish, 0, 0, IS_STATIC, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Http_Parser, finish)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = cat_http_parser_finish(parser);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_http_parser_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

#define arginfo_class_Swow_Http_Parser_reset arginfo_class_Swow_Http_Parser_finish

static PHP_METHOD(Swow_Http_Parser, reset)
{
    SWOW_HTTP_PARSER_GETTER(s_parser, parser);

    ZEND_PARSE_PARAMETERS_NONE();

    cat_http_parser_reset(parser);
    s_parser->data_offset = 0;

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Http_Parser_getEventNameFor, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, event, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Http_Parser, getEventNameFor)
{
    zend_long event;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(event)
    ZEND_PARSE_PARAMETERS_END();

    // TODO: interned strings?
    RETURN_STRING(cat_http_parser_event_get_name(event));
}

static const zend_function_entry swow_http_parser_methods[] = {
    PHP_ME(Swow_Http_Parser, getType,               arginfo_class_Swow_Http_Parser_getType,               ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, setType,               arginfo_class_Swow_Http_Parser_setType,               ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getEvents,             arginfo_class_Swow_Http_Parser_getEvents,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, setEvents,             arginfo_class_Swow_Http_Parser_setEvents,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, execute,               arginfo_class_Swow_Http_Parser_execute,               ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getEvent,              arginfo_class_Swow_Http_Parser_getEvent,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getEventName,          arginfo_class_Swow_Http_Parser_getEventName,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getPreviousEvent,      arginfo_class_Swow_Http_Parser_getPreviousEvent,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getPreviousEventName,  arginfo_class_Swow_Http_Parser_getPreviousEventName,  ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getDataOffset,         arginfo_class_Swow_Http_Parser_getDataOffset,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getDataLength,         arginfo_class_Swow_Http_Parser_getDataLength,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getParsedLength,       arginfo_class_Swow_Http_Parser_getParsedLength,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, isCompleted,           arginfo_class_Swow_Http_Parser_isCompleted,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, shouldKeepAlive,       arginfo_class_Swow_Http_Parser_shouldKeepAlive,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getMethod,             arginfo_class_Swow_Http_Parser_getMethod,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getMajorVersion,       arginfo_class_Swow_Http_Parser_getMajorVersion,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getMinorVersion,       arginfo_class_Swow_Http_Parser_getMinorVersion,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getProtocolVersion,    arginfo_class_Swow_Http_Parser_getProtocolVersion,    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getStatusCode,         arginfo_class_Swow_Http_Parser_getStatusCode,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getReasonPhrase,       arginfo_class_Swow_Http_Parser_getReasonPhrase,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getContentLength,      arginfo_class_Swow_Http_Parser_getContentLength,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, getCurrentChunkLength, arginfo_class_Swow_Http_Parser_getCurrentChunkLength, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, isChunked,             arginfo_class_Swow_Http_Parser_isChunked,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, isMultipart,           arginfo_class_Swow_Http_Parser_isMultipart,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, isUpgrade,             arginfo_class_Swow_Http_Parser_isUpgrade,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, finish,                arginfo_class_Swow_Http_Parser_finish,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Http_Parser, reset,                 arginfo_class_Swow_Http_Parser_reset,                 ZEND_ACC_PUBLIC)
    /* static */
    PHP_ME(Swow_Http_Parser, getEventNameFor,       arginfo_class_Swow_Http_Parser_getEventNameFor,       ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

static zend_always_inline size_t swow_http_get_header_length(zend_string *header_name, zval *z_header_value)
{
    zend_string *header_value, *tmp_header_value;
    size_t size;

    header_value = zval_get_tmp_string(z_header_value, &tmp_header_value);
    size = ZSTR_LEN(header_name) + CAT_STRLEN(": ") + ZSTR_LEN(header_value) + CAT_STRLEN("\r\n");
    zend_tmp_string_release(tmp_header_value);

    return size;
}

static zend_always_inline size_t swow_http_get_message_length(HashTable *headers, zend_string *body)
{
    zend_string *header_name;
    zval *z_header_value;
    size_t size = 0;

    ZEND_HASH_FOREACH_STR_KEY_VAL(headers, header_name, z_header_value) {
        if (UNEXPECTED(header_name == NULL)) {
            continue;
        }
        if (Z_TYPE_P(z_header_value) != IS_ARRAY) {
            size += swow_http_get_header_length(header_name, z_header_value);
        } else {
            ZEND_HASH_FOREACH_VAL(Z_ARR_P(z_header_value), z_header_value) {
                size += swow_http_get_header_length(header_name, z_header_value);
            } ZEND_HASH_FOREACH_END();
        }
    } ZEND_HASH_FOREACH_END();

    size += CAT_STRLEN("\r\n");

    size += ZSTR_LEN(body);

    return size;
}

static zend_always_inline char *swow_http_pack_header(char *p, zend_string *header_name, zval *z_header_value)
{
    zend_string *header_value, *tmp_header_value;

    header_value = zval_get_tmp_string(z_header_value, &tmp_header_value);
    p = cat_strnappend(p, ZSTR_VAL(header_name), ZSTR_LEN(header_name));
    p = cat_strnappend(p, CAT_STRL(": "));
    p = cat_strnappend(p, ZSTR_VAL(header_value), ZSTR_LEN(header_value));
    p = cat_strnappend(p, CAT_STRL("\r\n"));
    zend_tmp_string_release(tmp_header_value);

    return p;
}

static zend_always_inline char *swow_http_pack_headers(char *p, HashTable *headers)
{
    zend_string *header_name;
    zval *z_header_value;

    ZEND_HASH_FOREACH_STR_KEY_VAL(headers, header_name, z_header_value) {
        if (UNEXPECTED(header_name == NULL)) {
            continue;
        }
        if (Z_TYPE_P(z_header_value) != IS_ARRAY) {
            p = swow_http_pack_header(p, header_name, z_header_value);
        } else {
            ZEND_HASH_FOREACH_VAL(Z_ARR_P(z_header_value), z_header_value) {
                p = swow_http_pack_header(p, header_name, z_header_value);
            } ZEND_HASH_FOREACH_END();
        }
    } ZEND_HASH_FOREACH_END();

    return p;
}

static zend_always_inline char *swow_http_pack_message(char *p, HashTable *headers, zend_string *body)
{
    p = swow_http_pack_headers(p, headers);

    p = cat_strnappend(p, CAT_STRL("\r\n"));

    if (ZSTR_LEN(body) > 0) {
        p = cat_strnappend(p, ZSTR_VAL(body), ZSTR_LEN(body));
    }

    *p = '\0';

    return p;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Http_Http_packRequest, 0, 2, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, method, IS_STRING, 0)
    ZEND_ARG_OBJ_TYPE_MASK(0, uri, Stringable, MAY_BE_STRING, NULL)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, headers, IS_ARRAY, 0, "[]")
    ZEND_ARG_OBJ_TYPE_MASK(0, body, Stringable, MAY_BE_STRING, "\'\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, protocolVersion, IS_STRING, 0, "Swow\\Http\\Http::DEFAULT_PROTOCOL_VERSION")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Http_Http, packRequest)
{
    zend_string *request;
    /* arguments */
    zend_string *method;
    zend_string *uri;
    HashTable *headers = (HashTable *) &zend_empty_array;
    zend_string *body = zend_empty_string;
    // TODO: use zend_string
    char *protocol_version = (char *) "1.1";
    size_t protocol_version_length = CAT_STRLEN("1.1");
    /* pack */
    char *p;
    size_t size;

    ZEND_PARSE_PARAMETERS_START(2, 5)
        Z_PARAM_STR(method)
        SWOW_PARAM_STRINGABLE_EXPECT_BUFFER_FOR_READING(uri)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(headers)
        SWOW_PARAM_STRINGABLE_EXPECT_BUFFER_FOR_READING(body)
        Z_PARAM_STRING(protocol_version, protocol_version_length)
    ZEND_PARSE_PARAMETERS_END();

    size = ZSTR_LEN(method) + CAT_STRLEN(" ") +
           ZSTR_LEN(uri) + CAT_STRLEN(" ") +
           CAT_STRLEN("HTTP/") + protocol_version_length + CAT_STRLEN("\r\n");

    size += swow_http_get_message_length(headers, body);

    request = zend_string_alloc(size, 0);

    p = ZSTR_VAL(request);
    p = cat_strnappend(p, ZSTR_VAL(method), ZSTR_LEN(method));
    p = cat_strnappend(p, CAT_STRL(" "));
    p = cat_strnappend(p, ZSTR_VAL(uri), ZSTR_LEN(uri));
    p = cat_strnappend(p, CAT_STRL(" HTTP/"));
    p = cat_strnappend(p, protocol_version, protocol_version_length);
    p = cat_strnappend(p, CAT_STRL("\r\n"));

    (void) swow_http_pack_message(p, headers, body);

    RETURN_STR(request);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Http_Http_packResponse, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, statusCode, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, reasonPhrase, IS_STRING, 0, "\'\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, headers, IS_ARRAY, 0, "[]")
    ZEND_ARG_OBJ_TYPE_MASK(0, body, Stringable, MAY_BE_STRING, "\'\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, protocolVersion, IS_STRING, 0, "Swow\\Http\\Http::DEFAULT_PROTOCOL_VERSION")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Http_Http, packResponse)
{
    zend_string *response;
    /* arguments */
    zend_long status_code;
    char status_code_buffer[MAX_LENGTH_OF_LONG + 1];
    char *status_code_string, *status_code_string_eof;
    size_t status_code_length;
    char *reason_phrase = NULL;
    size_t reason_phrase_length = 0;
    HashTable *headers = (HashTable *) &zend_empty_array;
    zend_string *body = zend_empty_string;
    // TODO: use zend_string
    char *protocol_version = (char *) "1.1";
    size_t protocol_version_length = CAT_STRLEN("1.1");
    /* pack */
    char *p;
    size_t size;

    ZEND_PARSE_PARAMETERS_START(1, 5)
        Z_PARAM_LONG(status_code)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(reason_phrase, reason_phrase_length)
        Z_PARAM_ARRAY_HT(headers)
        SWOW_PARAM_STRINGABLE_EXPECT_BUFFER_FOR_READING(body)
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
    p = cat_strnappend(p, CAT_STRL("HTTP/"));
    p = cat_strnappend(p, protocol_version, protocol_version_length);
    p = cat_strnappend(p, CAT_STRL(" "));
    p = cat_strnappend(p, status_code_string, status_code_length);
    p = cat_strnappend(p, CAT_STRL(" "));
    p = cat_strnappend(p, reason_phrase, reason_phrase_length);
    p = cat_strnappend(p, CAT_STRL("\r\n"));

    (void) swow_http_pack_message(p, headers, body);

    RETURN_STR(response);
}

static const zend_function_entry swow_http_http_methods[] = {
    PHP_ME(Swow_Http_Http, packRequest,  arginfo_class_Swow_Http_Http_packRequest,  ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Http_Http, packResponse, arginfo_class_Swow_Http_Http_packResponse, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

zend_result swow_http_module_init(INIT_FUNC_ARGS)
{
    /* Http */
    swow_http_http_ce = swow_register_internal_class(
        "Swow\\Http\\Http", NULL, swow_http_http_methods,
        NULL, NULL, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );
    zend_declare_class_constant_stringl(swow_http_http_ce, ZEND_STRL("DEFAULT_PROTOCOL_VERSION"), ZEND_STRL("1.1"));

    /* Status */
    swow_http_status_ce = swow_register_internal_class(
        "Swow\\Http\\Status", NULL, swow_http_status_methods,
        NULL, NULL, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );
#define SWOW_HTTP_STATUS_GEN(name, value, unused)  zend_declare_class_constant_long(swow_http_status_ce, ZEND_STRL(#name), value);
    CAT_HTTP_STATUS_MAP(SWOW_HTTP_STATUS_GEN)
#undef SWOW_HTTP_STATUS_GEN

    /* Parser */
    swow_http_parser_ce = swow_register_internal_class(
        "Swow\\Http\\Parser", NULL, swow_http_parser_methods,
        &swow_http_parser_handlers, NULL,
        cat_false, cat_false,
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
        "Swow\\Http\\ParserException", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}
