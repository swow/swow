/*
  +--------------------------------------------------------------------------+
  | libcat                                                                   |
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
  |         dixyes <dixyes@gmail.com>                                        |
  +--------------------------------------------------------------------------+
 */

#ifndef CAT_HTTP_H
#define CAT_HTTP_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#include "llhttp.h"

#include "multipart_parser.h"

#define CAT_HTTP_METHOD_MAP HTTP_METHOD_MAP

enum cat_http_method_e
{
#define CAT_HTTP_METHOD_GEN(id, name, string) CAT_HTTP_METHOD_##name = id,
    CAT_HTTP_METHOD_MAP(CAT_HTTP_METHOD_GEN)
#undef CAT_HTTP_METHOD_GEN
    CAT_HTTP_METHOD_UNKNOWN
};

typedef uint8_t cat_http_method_t;

CAT_API const char *cat_http_method_get_name(cat_http_method_t method);

#define CAT_HTTP_STATUS_MAP(XX) \
    XX(CONTINUE, 100, "Continue") \
    XX(SWITCHING_PROTOCOLS, 101, "Switching Protocols") \
    XX(PROCESSING, 102, "Processing") /* WebDAV */ \
    XX(EARLY_HINTS, 103, "Early Hints") \
    XX(OK, 200, "OK") \
    XX(CREATED, 201, "Created") \
    XX(ACCEPTED, 202, "Accepted") \
    XX(NONE_AUTHORITATIVE_INFORMATION, 203, "Non-Authoritative Information") \
    XX(NO_CONTENT, 204, "No Content") \
    XX(RESET_CONTENT, 205, "Reset Content") \
    XX(PARTIAL_CONTENT, 206, "Partial Content") \
    XX(MULTI_STATUS, 207, "Multi-Status") /* WebDAV */ \
    XX(ALREADY_REPORTED, 208, "Already Reported") /* WebDAV */ \
    XX(IM_USED, 226, "IM Used") /* HTTP Delta encoding */ \
    XX(SPECIAL_RESPONSE, 300, "Multiple Choices") \
    XX(MOVED_PERMANENTLY, 301, "Moved Permanently") \
    XX(MOVED_TEMPORARILY, 302, "Found") \
    XX(SEE_OTHER, 303, "See Other") \
    XX(NOT_MODIFIED, 304, "Not Modified") \
    XX(TEMPORARY_REDIRECT, 307, "Temporary Redirect") \
    XX(PERMANENT_REDIRECT, 308, "Permanent Redirect") \
    XX(BAD_REQUEST, 400, "Bad Request") \
    XX(UNAUTHORIZED, 401, "Unauthorized") \
    XX(PAYMENT_REQUIRED, 402, "Payment Required") /* unused now */ \
    XX(FORBIDDEN, 403, "Forbidden") \
    XX(NOT_FOUND, 404, "Not Found") \
    XX(NOT_ALLOWED, 405, "Method Not Allowed") \
    XX(NOT_ACCEPTABLE, 406, "Not Acceptable") \
    XX(PROXY_AUTHENTICATION_REQUIRED, 407, "Proxy Authentication Required") \
    XX(REQUEST_TIME_OUT, 408, "Request Time-out") \
    XX(CONFLICT, 409, "Conflict") \
    XX(GONE, 410, "Gone") \
    XX(LENGTH_REQUIRED, 411, "Length Required") \
    XX(PRECONDITION_FAILED, 412, "Precondition Failed") \
    XX(REQUEST_ENTITY_TOO_LARGE, 413, "Request Entity Too Large") \
    XX(REQUEST_URI_TOO_LARGE, 414, "Request-URI Too Large") \
    XX(UNSUPPORTED_MEDIA_TYPE, 415, "Unsupported Media Type") \
    XX(REQUEST_RANGE_NOT_SATISFIABLE, 416, "Requested range not satisfiable") \
    XX(EXPECTATION_FAILED, 417, "Expectation Failed") \
    XX(I_AM_A_TEAPOT, 418, "I'm a teapot") /* lol */ \
    XX(MISDIRECTED_REQUEST, 421, "Misdirected Request") \
    XX(UNPROCESSABLE_ENTITY, 422, "Unprocessable Entity") /* WebDAV */ \
    XX(LOCKED, 423, "Locked") /* WebDAV */ \
    XX(FAILED_DEPENDENCY, 424, "Failed Dependency") /* WebDAV */ \
    XX(TOO_EARLY, 425, "Too Early") \
    XX(UPGRADE_REQUIRED, 426, "Upgrade Required") \
    XX(TOO_MANY_REQUESTS, 429, "Too Many Requests") \
    XX(REQUEST_HEADER_FIELDS_TOO_LARGE, 431, "Request Header Fields Too Large") \
    XX(UNAVAILABLE_FOR_LEGAL_REASONS, 451, "Unavailable For Legal Reasons") \
    XX(INTERNAL_SERVER_ERROR, 500, "Internal Server Error") \
    XX(NOT_IMPLEMENTED, 501, "Not Implemented") \
    XX(BAD_GATEWAY, 502, "Bad Gateway") \
    XX(SERVICE_UNAVAILABLE, 503, "Service Unavailable") \
    XX(GATEWAY_TIME_OUT, 504, "Gateway Time-out") \
    XX(VERSION_NOT_SUPPORTED, 505, "HTTP Version not supported") \
    XX(VARIANT_ALSO_NEGOTIATES, 506, "Variant Also Negotiates") \
    XX(INSUFFICIENT_STORAGE, 507, "Insufficient Storage") \
    XX(LOOP_DETECTED, 508, "Loop Detected") /* WebDAV */ \
    XX(NOT_EXTENDED, 510, "Not Extended") \
    XX(NETWORK_AUTHENTICATION_REQUIRED, 511, "Network Authentication Required") \

enum cat_http_status_e
{
#define CAT_HTTP_STATUS_GEN(name, value, unused) \
    CAT_HTTP_STATUS_##name = value,
    CAT_HTTP_STATUS_MAP(CAT_HTTP_STATUS_GEN)
#undef CAT_HTTP_STATUS_GEN
};

typedef uint16_t cat_http_status_code_t;

CAT_API const char *cat_http_status_get_reason(cat_http_status_code_t status);

/* parser */

#define CAT_HTTP_PARSER_ERRNO_MAP(XX) \
    HTTP_ERRNO_MAP(XX) \
    XX(HPE_USER + 1, NOMEM, NOMEM) \
    XX(HPE_USER + 2, MULTIPART_HEADER, MULTIPART_HEADER) \
    XX(HPE_USER + 3, DUPLICATE_CONTENT_TYPE, DUPLICATE_CONTENT_TYPE) \
    XX(HPE_USER + 4, MULTIPART_BODY, MULTIPART_BODY) \

typedef enum cat_http_parser_errno_e {
#define CAT_HTTP_PARSER_ERRNO_GEN(code, name, description) CAT_HTTP_PARSER_E_##name = code,
    CAT_HTTP_PARSER_ERRNO_MAP(CAT_HTTP_PARSER_ERRNO_GEN)
#undef CAT_HTTP_PARSER_ERRNO_GEN
} cat_http_parser_errno_t;

typedef enum cat_http_parser_type_e {
    CAT_HTTP_PARSER_TYPE_BOTH     = HTTP_BOTH,
    CAT_HTTP_PARSER_TYPE_REQUEST  = HTTP_REQUEST,
    CAT_HTTP_PARSER_TYPE_RESPONSE = HTTP_RESPONSE,
} cat_http_parser_type_t;

typedef enum cat_http_parser_event_flag_e {
    CAT_HTTP_PARSER_EVENT_FLAG_NONE = 0,
    CAT_HTTP_PARSER_EVENT_FLAG_LINE = 1 << 1, /* first line */
    CAT_HTTP_PARSER_EVENT_FLAG_DATA = 1 << 2, /* have data */
    CAT_HTTP_PARSER_EVENT_FLAG_COMPLETE = 1 << 3, /* something completed */
    CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART = 1 << 4 /* multipart */
} cat_http_parser_event_flag_t; /* 1 ~ 8 */

#define CAT_HTTP_PARSER_EVENT_MAP(XX) \
    XX(NONE,                  (0)) \
    XX(MESSAGE_BEGIN,         (1 << 9)) \
    XX(URL,                   (1 << 10) | CAT_HTTP_PARSER_EVENT_FLAG_LINE | CAT_HTTP_PARSER_EVENT_FLAG_DATA) \
    XX(URL_COMPLETE,          (1 << 11) | CAT_HTTP_PARSER_EVENT_FLAG_COMPLETE) \
    XX(STATUS,                (1 << 12) | CAT_HTTP_PARSER_EVENT_FLAG_LINE | CAT_HTTP_PARSER_EVENT_FLAG_DATA) \
    XX(STATUS_COMPLETE,       (1 << 13) | CAT_HTTP_PARSER_EVENT_FLAG_COMPLETE) \
    XX(HEADER_FIELD,          (1 << 14) | CAT_HTTP_PARSER_EVENT_FLAG_DATA) \
    XX(HEADER_VALUE,          (1 << 15) | CAT_HTTP_PARSER_EVENT_FLAG_DATA) \
    XX(HEADER_FIELD_COMPLETE, (1 << 16) | CAT_HTTP_PARSER_EVENT_FLAG_COMPLETE) \
    XX(HEADER_VALUE_COMPLETE, (1 << 17) | CAT_HTTP_PARSER_EVENT_FLAG_COMPLETE) \
    XX(HEADERS_COMPLETE,      (1 << 18) | CAT_HTTP_PARSER_EVENT_FLAG_COMPLETE) \
    XX(BODY,                  (1 << 19) | CAT_HTTP_PARSER_EVENT_FLAG_DATA) \
    XX(MESSAGE_COMPLETE,      (1 << 20) | CAT_HTTP_PARSER_EVENT_FLAG_COMPLETE) \
    XX(CHUNK_HEADER,          (1 << 21)) \
    XX(CHUNK_COMPLETE,        (1 << 22) | CAT_HTTP_PARSER_EVENT_FLAG_COMPLETE) \
    /* multipart */ \
    XX(MULTIPART_DATA_BEGIN,       (1 << 23) | CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) \
    XX(MULTIPART_HEADER_FIELD,     (1 << 24) | CAT_HTTP_PARSER_EVENT_FLAG_DATA | CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) \
    XX(MULTIPART_HEADER_VALUE,     (1 << 25) | CAT_HTTP_PARSER_EVENT_FLAG_DATA | CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) \
    XX(MULTIPART_HEADERS_COMPLETE, (1 << 26) | CAT_HTTP_PARSER_EVENT_FLAG_COMPLETE | CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) \
    XX(MULTIPART_BODY,             (1 << 27) | CAT_HTTP_PARSER_EVENT_FLAG_DATA | CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) \
    XX(MULTIPART_DATA_END,         (1 << 28) | CAT_HTTP_PARSER_EVENT_FLAG_COMPLETE | CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) \

#define CAT_HTTP_PARSER_INTERNAL_EVENT_MAP(XX) \
    XX(MULTIPART_BODY_END,         CAT_HTTP_PARSER_EVENT_FLAG_COMPLETE | CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) \

typedef enum cat_http_parser_event_e {
#define CAT_HTTP_PARSER_EVENT_GEN(name, value) CAT_HTTP_PARSER_EVENT_##name = value,
    CAT_HTTP_PARSER_EVENT_MAP(CAT_HTTP_PARSER_EVENT_GEN)
    CAT_HTTP_PARSER_INTERNAL_EVENT_MAP(CAT_HTTP_PARSER_EVENT_GEN)
#undef CAT_HTTP_PARSER_EVENT_GEN
} cat_http_parser_event_t; /* 16 ~ 31 */

#define CAT_HTTP_PARSER_EVENTS_ALL_GEN(name, unused1) | CAT_HTTP_PARSER_EVENT_##name
#define CAT_HTTP_PARSER_EVENTS_ALL_VALUE (0 CAT_HTTP_PARSER_EVENT_MAP(CAT_HTTP_PARSER_EVENTS_ALL_GEN))

typedef enum cat_http_parser_union_events_e {
    CAT_HTTP_PARSER_EVENTS_NONE = 0,
    CAT_HTTP_PARSER_EVENTS_ALL  = CAT_HTTP_PARSER_EVENTS_ALL_VALUE
} cat_http_parser_union_events_t;

typedef uint32_t cat_http_parser_events_t;

typedef enum cat_http_parser_internal_flag_e {
    CAT_HTTP_PARSER_INTERNAL_FLAG_NONE = 0,
    CAT_HTTP_PARSER_INTERNAL_FLAG_HAS_PREVIOUS_ERROR = 1 << 0,
} cat_http_parser_internal_flag_t;

typedef uint8_t cat_http_parser_internal_flags_t;

typedef struct cat_http_parser_s {
    /* public readonly: which events will return from execute */
    cat_http_parser_events_t events;
    /* public readonly: current event */
    cat_http_parser_event_t event;
    /* public readonly: current data pos */
    const char *data;
    /* public readonly: current data length */
    size_t data_length;
    /* public readonly: current parsed length */
    size_t parsed_length;
    /* Note: llhttp will clear content_length before parse body,
     * so we store it ourselves to ensure it is always available.
     * public readonly: it is not always reliable (consider chunk case) */
    uint64_t content_length;
    /* public readonly: to distinguish it from content-length */
    uint64_t current_chunk_length;
    /* public readonly: keep alive (update on headers complete) */
    cat_bool_t keep_alive;
    /* private: internal flags */
    cat_http_parser_internal_flags_t internal_flags;
    /* private: handle */
    llhttp_t llhttp;
    /* private: multipart parser */
    multipart_parser multipart;
    /* private: multipart parser header parser index */
    uint8_t multipart_header_index;
    /* private: multipart parser state*/
    uint8_t multipart_state;
    /* private: callback complete state */
    uint8_t complete_state;
    /* private: multipart parser header parser buffer */
    char multipart_header[12];
    /* private: multipart parser pointer */
    const char *multipart_ptr;
} cat_http_parser_t;

/*
* prepare parser for new parsing and clear subscripted events
*/
CAT_API void cat_http_parser_init(cat_http_parser_t *parser);
/*
* prepare parser for new parsing
*/
CAT_API void cat_http_parser_reset(cat_http_parser_t *parser);
/*
* create parser for new parsing, if parser is NULL, allocate memory for parser using cat_malloc
* @return returns newly allocated parser if parser arg is NULL, else return prepared parser
*/
CAT_API cat_http_parser_t *cat_http_parser_create(cat_http_parser_t *parser);
/*
* get parser type, type may be CAT_HTTP_PARSER_TYPE_BOTH or CAT_HTTP_PARSER_TYPE_REQUEST or CAT_HTTP_PARSER_TYPE_RESPONSE
*/
CAT_API cat_http_parser_type_t cat_http_parser_get_type(const cat_http_parser_t *parser);
/*
* set parser type, type may be CAT_HTTP_PARSER_TYPE_BOTH or CAT_HTTP_PARSER_TYPE_REQUEST or CAT_HTTP_PARSER_TYPE_RESPONSE
*/
CAT_API cat_bool_t cat_http_parser_set_type(cat_http_parser_t *parser, cat_http_parser_type_t type);
/*
* get parser subscripted events
*/
CAT_API cat_http_parser_events_t cat_http_parser_get_events(const cat_http_parser_t *parser);
/*
* set parser subscripted events
* parser will pause at subscripted event, parser->event will be set at any event even it is not been subscripted
*/
CAT_API void cat_http_parser_set_events(cat_http_parser_t *parser, cat_http_parser_events_t events);
/*
* execute parser with data
* parser will pause at subscripted event, parser->event will be set at any event even it is not been subscripted;
* parser will pause at specified data end, parsed data length can be got via cat_http_parser_get_parsed_length.
* @param data pointer to the input buffer
* @param length input buffer size in bytes
* @return cat_true when parser paused or stopped without error otherwise return cat_false
*/
CAT_API cat_bool_t cat_http_parser_execute(cat_http_parser_t *parser, const char *data, size_t length);
/*
* get parser event name string for event code
*/
CAT_API const char *cat_http_parser_event_name(cat_http_parser_event_t event);
/*
* get event code of parser
*/
CAT_API cat_http_parser_event_t cat_http_parser_get_event(const cat_http_parser_t *parser);
/*
* get event name string for event code of parser
*/
CAT_API const char* cat_http_parser_get_event_name(const cat_http_parser_t *parser);
/*
* get data for data event
* when a data event emitted, parser.data is pointed to data start
* data returned should be in range of inputed buffer in last execution
*/
CAT_API const char* cat_http_parser_get_data(const cat_http_parser_t *parser);
/*
* get data length for data event
* when a data event emitted, parser.data_length is setted to data length
*/
CAT_API size_t cat_http_parser_get_data_length(const cat_http_parser_t *parser);
/*
* tell should we keep alive: "if there might be any other messages following the last that was successfully parsed"
*/
CAT_API cat_bool_t cat_http_parser_should_keep_alive(const cat_http_parser_t *parser);
/*
* finish parser: "This method should be called when the other side has no further bytes to send (e.g. shutdown of readable side of the TCP connection.)"
*/
CAT_API cat_bool_t cat_http_parser_finish(cat_http_parser_t *parser);
/*
* tell if parser is completed
*/
CAT_API cat_bool_t cat_http_parser_is_completed(const cat_http_parser_t *parser);
/*
* get current position
*/
CAT_API const char *cat_http_parser_get_current_pos(const cat_http_parser_t *parser);
/*
* get current offset
*/
CAT_API size_t cat_http_parser_get_current_offset(const cat_http_parser_t *parser, const char *data);
/*
* get last execution parsed length
*/
CAT_API size_t cat_http_parser_get_parsed_length(const cat_http_parser_t *parser);
/*
* get http method
*/
CAT_API cat_http_method_t cat_http_parser_get_method(const cat_http_parser_t *parser);
/*
* get http method name
*/
CAT_API const char  *cat_http_parser_get_method_name(const cat_http_parser_t *parser);
/*
* get http major version
*/
CAT_API uint8_t cat_http_parser_get_major_version(const cat_http_parser_t *parser);
/*
* get http minor version
*/
CAT_API uint8_t cat_http_parser_get_minor_version(const cat_http_parser_t *parser);
/*
* get http protocol version in string
*/
CAT_API const char *cat_http_parser_get_protocol_version(const cat_http_parser_t *parser);
/*
* get http status code
*/
CAT_API cat_http_status_code_t cat_http_parser_get_status_code(const cat_http_parser_t *parser);
/*
* get http status reason phrase
*/
CAT_API const char *cat_http_parser_get_reason_phrase(const cat_http_parser_t *parser);
/*
* get http content-length header value
*/
CAT_API uint64_t cat_http_parser_get_content_length(const cat_http_parser_t *parser);
/*
* get current http chunk length
*/
CAT_API uint64_t cat_http_parser_get_current_chunk_length(const cat_http_parser_t *parser);
/*
* tell if its' transfer-encoding is chunked
*/
CAT_API cat_bool_t cat_http_parser_is_chunked(const cat_http_parser_t *parser);
/*
* tell if needs protocol upgrade
*/
CAT_API cat_bool_t cat_http_parser_is_upgrade(const cat_http_parser_t *parser);
/*
* tell if its' content-type is multipart form data
* Notice: it should be called after headers complete event triggered
*/
CAT_API cat_bool_t cat_http_parser_is_multipart(const cat_http_parser_t *parser);

#ifdef __cplusplus
}
#endif
#endif /* CAT_HTTP_H */
