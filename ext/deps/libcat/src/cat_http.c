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

#include "cat_http.h"

CAT_API const char *cat_http_method_get_name(cat_http_method_t method)
{
    switch (method) {
#define CAT_HTTP_METHOD_NAME_GEN(_, name, string) case CAT_HTTP_METHOD_##name: return #string;
        CAT_HTTP_METHOD_MAP(CAT_HTTP_METHOD_NAME_GEN)
#undef CAT_HTTP_METHOD_NAME_GEN
    }
    return "UNKNOWN";
}

CAT_API const char *cat_http_status_get_reason(cat_http_status_code_t status)
{
    switch (status) {
#define CAT_HTTP_STATUS_GEN(_, code, reason) case code: return reason;
        CAT_HTTP_STATUS_MAP(CAT_HTTP_STATUS_GEN)
#undef CAT_HTTP_STATUS_GEN
    }
    return "UNKNOWN";
}

#define mt_dbg() do { \
    CAT_LOG_DEBUG_V3(HTTP, "content_type parser %s:%d state %d char %c", __FILE__, __LINE__, state, *p); \
} while (0)
#include "cat_http_content_type.c"

/* http parser definations */

typedef enum cat_http_parser_errno_e {
#define CAT_HTTP_PARSER_ERRNO_GEN(code, name, string) CAT_HTTP_PARSER_E_##name = code,
    CAT_HTTP_PARSER_ERRNO_MAP(CAT_HTTP_PARSER_ERRNO_GEN)
#undef CAT_HTTP_PARSER_ERRNO_GEN
} cat_http_parser_internal_errno_t;

CAT_STRCASECMP_FAST_FUNCTION(content_type, "content-type", "       \0    ");

#define cat_http_parser_throw_error(action, code, fmt, ...) do { \
    parser->internal_flags |= CAT_HTTP_PARSER_INTERNAL_FLAG_HAS_PREVIOUS_ERROR; \
    cat_http_parser_update_last_error(code, fmt, ##__VA_ARGS__); \
    action; \
} while (0)

#define cat_http_parser_throw_std_error(action, code, fmt, ...) do { \
    parser->internal_flags |= CAT_HTTP_PARSER_INTERNAL_FLAG_HAS_PREVIOUS_ERROR; \
    cat_update_last_error(code, fmt, ##__VA_ARGS__); \
    action; \
} while (0)

static void cat_http_parser_update_last_error(cat_http_parser_internal_errno_t error, const char *format, ...);

/* multipart parser things */

enum cat_multipart_state {
    CAT_MULTIPART_HEADER_FIELD_STATE_START = 0,
    CAT_MULTIPART_HEADER_FIELD_STATE_MAYBE_CONTENT_TYPE = sizeof("content-type") - 1,
    CAT_MULTIPART_HEADER_FIELD_STATE_NOT_CONTENT_TYPE = 252,
    CAT_MULTIPART_PARSING_VALUE = 253,
    CAT_MULTIPART_BOUNDARY_OK = 254,
    CAT_MULTIPART_IN_BODY = 255,
};

#define CAT_HTTP_MULTIPART_CB_FNAME(name) cat_http_multipart_parser_on_##name
#define CAT_HTTP_MULTIPART_ON_DATA(name, NAME) \
static long CAT_HTTP_MULTIPART_CB_FNAME(name)(multipart_parser *p, const char *at, size_t length) { \
    cat_http_parser_t* parser = cat_container_of(p, cat_http_parser_t, multipart); \
    parser->event = CAT_HTTP_PARSER_EVENT_##NAME; \
    parser->data = at; \
    parser->data_length = length; \
    CAT_LOG_DEBUG_VA_WITH_LEVEL(HTTP, 3, { \
        char *s; \
        CAT_LOG_DEBUG_D(HTTP, "HTTP multipart parser data on_" # name ": [%zu] %s", \
            length, cat_log_str_quote(at, length, &s)); \
        cat_free(s); \
    }); \
    if (((cat_http_parser_event_t) (parser->events & parser->event)) == parser->event) { \
        return MPPE_PAUSED; \
    } \
    return MPPE_OK; \
}

#define CAT_HTTP_MULTIPART_ON_EVENT(name, NAME) \
static long CAT_HTTP_MULTIPART_CB_FNAME(name)(multipart_parser *p) \
{ \
    cat_http_parser_t* parser = cat_container_of(p, cat_http_parser_t, multipart); \
    parser->event = CAT_HTTP_PARSER_EVENT_##NAME; \
    CAT_LOG_DEBUG_V2(HTTP, "HTTP multipart parser notify on_" # name ); \
    if (((cat_http_parser_event_t) (parser->events & parser->event)) == parser->event) { \
        return MPPE_PAUSED; \
    } \
    return MPPE_OK; \
}

CAT_HTTP_MULTIPART_ON_EVENT(part_data_begin, MULTIPART_DATA_BEGIN)
CAT_HTTP_MULTIPART_ON_DATA(header_field, MULTIPART_HEADER_FIELD)
CAT_HTTP_MULTIPART_ON_DATA(header_value, MULTIPART_HEADER_VALUE)
CAT_HTTP_MULTIPART_ON_EVENT(headers_complete, MULTIPART_HEADERS_COMPLETE)
CAT_HTTP_MULTIPART_ON_DATA(part_data, MULTIPART_BODY)
CAT_HTTP_MULTIPART_ON_EVENT(part_data_end, MULTIPART_DATA_END)

static long CAT_HTTP_MULTIPART_CB_FNAME(body_end)(multipart_parser *p)
{
    cat_http_parser_t* parser = cat_container_of(p, cat_http_parser_t, multipart);
    CAT_ASSERT(!(parser->events & CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) || parser->multipart_state == CAT_MULTIPART_IN_BODY);
    // escape mp parser
    parser->event = CAT_HTTP_PARSER_EVENT_MULTIPART_BODY_END;
    parser->multipart_state = CAT_MULTIPART_HEADER_FIELD_STATE_NOT_CONTENT_TYPE;
    CAT_LOG_DEBUG_V2(HTTP, "HTTP multipart parser data on_body_end");
    return MPPE_OK;
}

static const multipart_parser_settings cat_http_multipart_parser_settings = {
    /* .calloc = */ NULL,
    /* .free = */ NULL,
    CAT_HTTP_MULTIPART_CB_FNAME(header_field),
    CAT_HTTP_MULTIPART_CB_FNAME(header_value),
    CAT_HTTP_MULTIPART_CB_FNAME(part_data),
    CAT_HTTP_MULTIPART_CB_FNAME(part_data_begin),
    CAT_HTTP_MULTIPART_CB_FNAME(headers_complete),
    CAT_HTTP_MULTIPART_CB_FNAME(part_data_end),
    CAT_HTTP_MULTIPART_CB_FNAME(body_end),
};

/* http parser things */

static cat_always_inline cat_http_parser_t *cat_http_parser_get_from_handle(const llhttp_t *llhttp)
{
    return cat_container_of(llhttp, cat_http_parser_t, llhttp);
}

#define CAT_HTTP_PARSER_ON_EVENT_BEGIN(name, NAME) \
static int cat_http_parser_on_##name(llhttp_t *llhttp) \
{ \
    cat_http_parser_t *parser = cat_http_parser_get_from_handle(llhttp); \
    \
    parser->event = CAT_HTTP_PARSER_EVENT_##NAME; \
    \

#define _CAT_HTTP_PARSER_ON_EVENT_END() \
    if (((cat_http_parser_event_t) (parser->events & parser->event)) == parser->event) { \
        return CAT_HTTP_PARSER_E_PAUSED; \
    } else { \
        return CAT_HTTP_PARSER_E_OK; \
    }

#define CAT_HTTP_PARSER_ON_EVENT_END() \
    _CAT_HTTP_PARSER_ON_EVENT_END() \
}

#define CAT_HTTP_PARSER_ON_EVENT(name, NAME) \
CAT_HTTP_PARSER_ON_EVENT_BEGIN(name, NAME) \
CAT_HTTP_PARSER_ON_EVENT_END()

#define CAT_HTTP_PARSER_ON_DATA_BEGIN(name, NAME) \
static int cat_http_parser_on_##name(llhttp_t *llhttp, const char *at, size_t length) \
{ \
    cat_http_parser_t *parser = cat_http_parser_get_from_handle(llhttp); \
    \
    if (parser->previous_event == CAT_HTTP_PARSER_EVENT_##NAME && length == 0) { \
        /* Event will be re-triggered with 0 length when we pass 0 length data,
         * it means that the parser need more data from socket;
         * or previous execute stopped right at the end of an event data.
         * Do not pause parser at those cases, just continue by return OK. */ \
        return CAT_HTTP_PARSER_E_OK; \
    } \
    \
    parser->event = CAT_HTTP_PARSER_EVENT_##NAME; \
    parser->data = at; \
    parser->data_length = length;

#define CAT_HTTP_PARSER_ON_DATA_END() CAT_HTTP_PARSER_ON_EVENT_END()

#define CAT_HTTP_PARSER_ON_DATA(name, NAME) \
CAT_HTTP_PARSER_ON_DATA_BEGIN(name, NAME) \
CAT_HTTP_PARSER_ON_DATA_END()

CAT_HTTP_PARSER_ON_EVENT(message_begin, MESSAGE_BEGIN)
CAT_HTTP_PARSER_ON_DATA (url,           URL          )
CAT_HTTP_PARSER_ON_DATA (status,        STATUS       )

CAT_HTTP_PARSER_ON_DATA_BEGIN(header_field, HEADER_FIELD) {
    if (! (parser->events & CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART)) {
        _CAT_HTTP_PARSER_ON_EVENT_END();
    }

    // copy header into buffer
    if (length == 0) {
        return CAT_HTTP_PARSER_E_OK;
    }

    size_t index_new = (size_t) parser->multipart_state + length;
    if (index_new < 13) {
        memcpy(parser->multipart_header + parser->multipart_state, at, length);
        parser->multipart_state = (uint8_t) index_new;
    } else {
        parser->multipart_state = CAT_MULTIPART_HEADER_FIELD_STATE_NOT_CONTENT_TYPE;
    }
} CAT_HTTP_PARSER_ON_EVENT_END()

CAT_HTTP_PARSER_ON_DATA_BEGIN(header_value, HEADER_VALUE) {
    if (! (parser->events & CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART)) {
        _CAT_HTTP_PARSER_ON_EVENT_END();
    }
    switch (parser->multipart_state) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
            // not content-type
            CAT_FALLTHROUGH;
        case CAT_MULTIPART_HEADER_FIELD_STATE_NOT_CONTENT_TYPE:
            // not content-type
            parser->multipart_state = CAT_MULTIPART_HEADER_FIELD_STATE_START;
            break;
        case CAT_MULTIPART_BOUNDARY_OK:
            // content-type parsed
            parser->multipart_state = CAT_MULTIPART_HEADER_FIELD_STATE_NOT_CONTENT_TYPE;
            break;
        case CAT_MULTIPART_HEADER_FIELD_STATE_MAYBE_CONTENT_TYPE:
            // maybe content-type
            if (0 == cat_strcasecmp_fast_content_type(parser->multipart_header)) {
                parser->multipart_state = CAT_MULTIPART_HEADER_FIELD_STATE_NOT_CONTENT_TYPE;
                break;
            }
            cat_http_parser_multipart_parse_content_type_init(parser);
            parser->multipart_state = CAT_MULTIPART_PARSING_VALUE;
            CAT_FALLTHROUGH;
        case CAT_MULTIPART_PARSING_VALUE:
            cat_http_parser_multipart_parse_content_type(parser, at, at + length, NULL);
            break;
        default:
        printf("Invalid header field parser state %d\n", parser->multipart_state);
        CAT_NEVER_HERE("Invalid header field parser state");
    }
} CAT_HTTP_PARSER_ON_DATA_END()

CAT_HTTP_PARSER_ON_EVENT_BEGIN(headers_complete, HEADERS_COMPLETE) {
    parser->keep_alive = !!llhttp_should_keep_alive(&parser->llhttp);
    parser->content_length = parser->llhttp.content_length;
    if (! (parser->events & CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART)) {
        _CAT_HTTP_PARSER_ON_EVENT_END();
    }
    // finish parsing media type
    cat_bool_t ret = cat_http_parser_multipart_parse_content_type(parser, NULL, NULL, NULL);
    if (!ret) {
        // bad media-type
        parser->multipart.boundary_length = 0;
        _CAT_HTTP_PARSER_ON_EVENT_END();
    }
    if (parser->multipart.boundary_length < 2) {
        // no boundary, not multipart
        _CAT_HTTP_PARSER_ON_EVENT_END();
    }
    CAT_LOG_DEBUG_VA_WITH_LEVEL(HTTP, 3, {
        CAT_LOG_DEBUG_D(HTTP, "multipart boundary is [%d] %.*s",
            parser->multipart.boundary_length, (int)parser->multipart.boundary_length,
            parser->multipart.multipart_boundary);
    });
    if (0 != multipart_parser_init(&parser->multipart, NULL, 0, &cat_http_multipart_parser_settings)) {
        CAT_ASSERT(0 && "multipart_parser_init() failed");
        cat_http_parser_throw_error(return -1, CAT_HTTP_PARSER_E_MULTIPART, "multipart_parser_init() failed");
    }
} CAT_HTTP_PARSER_ON_EVENT_END()

CAT_HTTP_PARSER_ON_DATA_BEGIN(body, BODY) {
    if (parser->multipart.boundary_length > 2) {
        return CAT_HTTP_PARSER_E_PAUSED;
    }
} CAT_HTTP_PARSER_ON_EVENT_END()

CAT_HTTP_PARSER_ON_EVENT_BEGIN(chunk_header, CHUNK_HEADER) {
    parser->current_chunk_length = parser->llhttp.content_length;
} CAT_HTTP_PARSER_ON_EVENT_END()
CAT_HTTP_PARSER_ON_EVENT(chunk_complete,   CHUNK_COMPLETE  )
CAT_HTTP_PARSER_ON_EVENT(message_complete, MESSAGE_COMPLETE)

// TODO: dynamic change with event flags
static const llhttp_settings_t cat_http_parser_settings = {
    /* Possible return values 0, -1, `HPE_PAUSED` */
    cat_http_parser_on_message_begin,

    /* Possible return values 0, -1, HPE_USER */
    cat_http_parser_on_url,
    cat_http_parser_on_status,
    NULL, // cat_http_parser_on_method,
    NULL, // cat_http_parser_on_version,
    cat_http_parser_on_header_field,
    cat_http_parser_on_header_value,
    NULL, // cat_http_parser_on_chunk_extension_name,
    NULL, // cat_http_parser_on_chunk_extension_value,

    /* Possible return values:
     * 0  - Proceed normally
     * 1  - Assume that request/response has no body, and proceed to parsing the
     *      next message
     * 2  - Assume absence of body (as above) and make `llhttp_execute()` return
     *      `HPE_PAUSED_UPGRADE`
     * -1 - Error
     * `HPE_PAUSED`
     */
    cat_http_parser_on_headers_complete,

    /* Possible return values 0, -1, HPE_USER */
    cat_http_parser_on_body,

    /* Possible return values 0, -1, `HPE_PAUSED` */
    cat_http_parser_on_message_complete,
    NULL, // cat_http_parser_on_url_complete,
    NULL, // cat_http_parser_on_status_complete,
    NULL, // cat_http_parser_on_method_complete,
    NULL, // cat_http_parser_on_version_complete,
    NULL, // cat_http_parser_on_header_field_complete,
    NULL, // cat_http_parser_on_header_value_complete, /* it's fake, from old version */
    NULL, // cat_http_parser_on_chunk_extension_name_complete,
    NULL, // cat_http_parser_on_chunk_extension_value_complete,

    /* When on_chunk_header is called, the current chunk length is stored
     * in parser->content_length.
     * Possible return values 0, -1, `HPE_PAUSED`
     */
    cat_http_parser_on_chunk_header,
    cat_http_parser_on_chunk_complete,
    NULL, // cat_http_parser_on_reset,
};

static cat_always_inline void cat_http_parser__init(cat_http_parser_t *parser)
{
    parser->llhttp.method = CAT_HTTP_METHOD_UNKNOWN;
    parser->event = CAT_HTTP_PARSER_EVENT_NONE;
    parser->data = NULL;
    parser->data_length = 0;
    parser->parsed_length = 0;
    parser->content_length = 0;
    parser->current_chunk_length = 0;
    parser->keep_alive = cat_false;
    parser->multipart_state = CAT_MULTIPART_HEADER_FIELD_STATE_START;
    parser->multipart.boundary_length = 0;
    parser->internal_flags = CAT_HTTP_PARSER_INTERNAL_FLAG_NONE;
}

CAT_API void cat_http_parser_init(cat_http_parser_t *parser)
{
    llhttp_init(&parser->llhttp, HTTP_BOTH, &cat_http_parser_settings);
    cat_http_parser__init(parser);
    parser->events = CAT_HTTP_PARSER_EVENTS_NONE;
}

CAT_API void cat_http_parser_reset(cat_http_parser_t *parser)
{
    llhttp_reset(&parser->llhttp);
    cat_http_parser__init(parser);
}

CAT_API cat_http_parser_t *cat_http_parser_create(cat_http_parser_t *parser)
{
    if (parser == NULL) {
        parser = (cat_http_parser_t *) cat_malloc(sizeof(*parser));
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(parser == NULL)) {
            cat_update_last_error_of_syscall("Malloc for HTTP parser failed");
            return NULL;
        }
#endif
    }
    cat_http_parser_init(parser);

    return parser;
}

CAT_API cat_http_parser_type_t cat_http_parser_get_type(const cat_http_parser_t *parser)
{
    return (cat_http_parser_type_t) parser->llhttp.type;
}

CAT_API cat_bool_t cat_http_parser_set_type(cat_http_parser_t *parser, cat_http_parser_type_t type)
{
    if (unlikely(parser->llhttp.finish != HTTP_FINISH_SAFE)) {
        cat_update_last_error(CAT_EMISUSE, "Change HTTP-Parser type during execution is not allowed");
        return cat_false;
    }
    parser->llhttp.type = type;

    return cat_true;
}

CAT_API cat_http_parser_events_t cat_http_parser_get_events(const cat_http_parser_t *parser)
{
    return parser->events;
}

CAT_API void cat_http_parser_set_events(cat_http_parser_t *parser, cat_http_parser_events_t events)
{
    parser->events = (events & CAT_HTTP_PARSER_EVENTS_ALL);
}

static cat_http_parser_internal_errno_t cat_http_parser_llhttp_execute(cat_http_parser_t *parser, const char *data, size_t length)
{
    cat_http_parser_internal_errno_t error;

    parser->event = CAT_HTTP_PARSER_EVENT_NONE;
    error = (cat_http_parser_internal_errno_t) llhttp_execute(&parser->llhttp, data, length);
    if (error != CAT_HTTP_PARSER_E_OK) {
        parser->parsed_length = llhttp_get_error_pos(&parser->llhttp) - data;
        if (unlikely(error != CAT_HTTP_PARSER_E_PAUSED)) {
            if (unlikely(error != CAT_HTTP_PARSER_E_PAUSED_UPGRADE)) {
                return error;
            } else {
                llhttp_resume_after_upgrade(&parser->llhttp);
            }
        } else {
            llhttp_resume(&parser->llhttp);
        }
    } else {
        parser->parsed_length = length;
    }

    return CAT_HTTP_PARSER_E_OK;
}

// TODO: maybe export-able? for email protocols usage?
static cat_bool_t cat_http_parser_multipart_parser_execute(cat_http_parser_t *parser, const char *data, size_t length)
{
    size_t parsed_length = 0;

    parser->event = CAT_HTTP_PARSER_EVENT_NONE;
    parsed_length = multipart_parser_execute((multipart_parser*) &parser->multipart, data, length);
    if (MPPE_ERROR == parsed_length) {
        char error_buffer[4096];
        int error_length;
        error_length = multipart_parser_error_msg(&parser->multipart, error_buffer, sizeof(error_buffer));
        CAT_ASSERT((error_length != 0 && error_length <= 4095) && "multipart_parser_error_msg returns bad result");
        cat_http_parser_throw_error(return cat_false, CAT_HTTP_PARSER_E_MULTIPART, "Failed to parse multipart body: \"%.*s\"", error_length, error_buffer);
    }

    CAT_LOG_DEBUG_VA_WITH_LEVEL(HTTP, 3, {
        char *s;
        CAT_LOG_DEBUG_D(HTTP, "multipart_parser_execute() returns %zu, parsed %s",
            parsed_length, cat_log_str_quote(data, parsed_length, &s));
        cat_free(s);
    });
    CAT_ASSERT((!(parser->event & CAT_HTTP_PARSER_EVENT_FLAG_DATA)) || parser->data_length <= parsed_length);
    CAT_ASSERT(parser->event & CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART || parser->event == CAT_HTTP_PARSER_EVENT_NONE);

    // add ptr
    parser->multipart_ptr = data + parsed_length;
    parser->parsed_length = parsed_length;

    return cat_true;
}

static cat_bool_t cat_http_parser_solve_multipart_body(cat_http_parser_t *parser)
{
    const char *mp_body;
    size_t http_parsed_length, mp_length;
    cat_bool_t ret;

    // if body with multipart boundary is OK
    http_parsed_length = parser->parsed_length - parser->data_length;
    mp_body = parser->data;
    mp_length = parser->data_length;

    // update llhttp status
    // a little hacky here: llhttp wont check the body it got
    CAT_ASSERT(parser->content_length >= parser->data_length || (
        parser->content_length == 0 && cat_http_parser_get_type(parser) == CAT_HTTP_PARSER_TYPE_RESPONSE));

    if (parser->content_length == 0 && cat_http_parser_get_type(parser) == CAT_HTTP_PARSER_TYPE_RESPONSE) {
        // TODO: implement full support for multipart/byterange
        llhttp_finish(&parser->llhttp);
        cat_http_parser_throw_std_error(return cat_false, CAT_ENOTSUP, "Unsupported type: multipart/byterange");
    }
    ret = cat_http_parser_llhttp_execute(parser, NULL, parser->content_length - parser->data_length) == CAT_HTTP_PARSER_E_OK;
    CAT_ASSERT(ret && "Never fail");
    if (parser->event == CAT_HTTP_PARSER_EVENT_BODY) {
        ret = cat_http_parser_llhttp_execute(parser, NULL, 0) == CAT_HTTP_PARSER_E_OK;
        CAT_ASSERT(ret && "Never fail");
    }
    CAT_ASSERT(parser->event == CAT_HTTP_PARSER_EVENT_MESSAGE_COMPLETE);

    // now parsed length is all body, execute multipart_parser
    ret = cat_http_parser_multipart_parser_execute(parser, mp_body, mp_length);
    parser->parsed_length += http_parsed_length;
    parser->multipart_state = CAT_MULTIPART_IN_BODY;

    return ret;
}

CAT_API cat_bool_t cat_http_parser_execute(cat_http_parser_t *parser, const char *data, size_t length)
{
    cat_http_parser_internal_errno_t error = CAT_HTTP_PARSER_E_OK;
    cat_bool_t ret;

    parser->previous_event = parser->event;

    if (likely(parser->multipart_state != CAT_MULTIPART_IN_BODY)) {
        // if not in multipart body
        error = cat_http_parser_llhttp_execute(parser, data, length);
        ret = error == CAT_HTTP_PARSER_E_OK;
        if (ret && parser->event == CAT_HTTP_PARSER_EVENT_BODY && parser->multipart.boundary_length > 2) {
            ret = cat_http_parser_solve_multipart_body(parser);
        }
    } else {
        // if in multipart body, bypass llhttp
        ret = cat_http_parser_multipart_parser_execute(parser, data, length);
    }
    if (unlikely(!ret)) {
        if (!(parser->internal_flags & CAT_HTTP_PARSER_INTERNAL_FLAG_HAS_PREVIOUS_ERROR)) {
            cat_http_parser_update_last_error(error, "HTTP-Parser execute failed: %s", llhttp_get_error_reason(&parser->llhttp));
        } else {
            cat_update_last_error_with_previous("HTTP-Parser execute failed");
            parser->internal_flags ^= CAT_HTTP_PARSER_INTERNAL_FLAG_HAS_PREVIOUS_ERROR;
        }
        return cat_false;
    }

    if (parser->event == CAT_HTTP_PARSER_EVENT_MULTIPART_BODY_END) {
        // if MULTIPART_BODY_END, return llhttp event
        parser->event = CAT_HTTP_PARSER_EVENT_MESSAGE_COMPLETE;
    }
    if (parser->event == CAT_HTTP_PARSER_EVENT_MESSAGE_COMPLETE) {
        // reset multipart-parser
        parser->multipart_state = CAT_MULTIPART_HEADER_FIELD_STATE_START;
        parser->multipart.boundary_length = 0;
    }

    CAT_LOG_DEBUG_VA_WITH_LEVEL(HTTP, 2, {
        if (parser->event & CAT_HTTP_PARSER_EVENT_FLAG_DATA) {
            char *s;
            CAT_LOG_DEBUG_D(HTTP, "http_parser_execute(length=%-4zu) parsed %-4zu return %-12s [%2zu] %s",
                length, parser->parsed_length, cat_http_parser_get_event_name(parser),
                parser->data_length, cat_log_str_quote(parser->data, parser->data_length, &s));
            cat_free(s);
        } else  if ((parser->event & CAT_HTTP_PARSER_EVENT_CHUNK_HEADER) == CAT_HTTP_PARSER_EVENT_CHUNK_HEADER) {
            CAT_LOG_DEBUG_D(HTTP, "http_parser_execute(length=%-4zu) parsed %-4zu return %-12s with chunk_length=%" PRIu64,
                length, parser->parsed_length, cat_http_parser_get_event_name(parser), cat_http_parser_get_current_chunk_length(parser));
        } else {
            CAT_LOG_DEBUG_D(HTTP, "http_parser_execute(length=%-4zu) parsed %-4zu return %-12s",
                length, parser->parsed_length, cat_http_parser_get_event_name(parser));
        }
    });

    return cat_true;
}

CAT_API const char *cat_http_parser_event_get_name(cat_http_parser_event_t event)
{
    switch (event) {
#define CAT_HTTP_PARSER_EVENT_NAME_GEN(name, unused1) case CAT_HTTP_PARSER_EVENT_##name: return #name;
    CAT_HTTP_PARSER_EVENT_MAP(CAT_HTTP_PARSER_EVENT_NAME_GEN)
#undef CAT_HTTP_PARSER_EVENT_NAME_GEN
#define CAT_HTTP_PARSER_INTERNAL_EVENT_NAME_GEN(name, unused1) case CAT_HTTP_PARSER_EVENT_##name:
    CAT_HTTP_PARSER_INTERNAL_EVENT_MAP(CAT_HTTP_PARSER_INTERNAL_EVENT_NAME_GEN)
#undef CAT_HTTP_PARSER_EVENT_NAME_GEN
        default:
            break;
    }
    abort();
}

CAT_API cat_http_parser_event_t cat_http_parser_get_event(const cat_http_parser_t *parser)
{
    return parser->event;
}

CAT_API const char* cat_http_parser_get_event_name(const cat_http_parser_t *parser)
{
    return cat_http_parser_event_get_name(parser->event);
}

CAT_API cat_http_parser_event_t cat_http_parser_get_previous_event(const cat_http_parser_t *parser)
{
    return parser->previous_event;
}

CAT_API const char* cat_http_parser_get_previous_event_name(const cat_http_parser_t *parser)
{
    return cat_http_parser_event_get_name(parser->previous_event);
}

CAT_API const char* cat_http_parser_get_data(const cat_http_parser_t *parser)
{
    return parser->data;
}

CAT_API size_t cat_http_parser_get_data_length(const cat_http_parser_t *parser)
{
    return parser->data_length;
}

CAT_API cat_bool_t cat_http_parser_should_keep_alive(const cat_http_parser_t *parser)
{
    return parser->keep_alive;
}

CAT_API cat_bool_t cat_http_parser_finish(cat_http_parser_t *parser)
{
    cat_http_parser_internal_errno_t error;

    error = (cat_http_parser_internal_errno_t) llhttp_finish(&parser->llhttp);
    if (unlikely(
        error != CAT_HTTP_PARSER_E_OK &&
        error != CAT_HTTP_PARSER_E_PAUSED /* on_message_complete may return E_PAUSED */
    )) {
        cat_http_parser_update_last_error(error, "HTTP-Parser finish failed: %s", llhttp_get_error_reason(&parser->llhttp));
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_http_parser_is_completed(const cat_http_parser_t *parser)
{
    return parser->event == CAT_HTTP_PARSER_EVENT_MESSAGE_COMPLETE;
}

CAT_API const char *cat_http_parser_get_current_pos(const cat_http_parser_t *parser)
{
    // bypass llhttp_get_error_pos if in multipart body
    if (
        parser->event & CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART &&
        parser->multipart_state == CAT_MULTIPART_IN_BODY
    ) {
        return parser->multipart_ptr;
    }
    return llhttp_get_error_pos(&parser->llhttp);
}

CAT_API size_t cat_http_parser_get_current_offset(const cat_http_parser_t *parser, const char *data)
{
    const char *p = cat_http_parser_get_current_pos(parser);

    if (p == NULL) {
        return 0;
    }

    return p - data;
}

CAT_API size_t cat_http_parser_get_parsed_length(const cat_http_parser_t *parser)
{
    return parser->parsed_length;
}

CAT_API cat_http_method_t cat_http_parser_get_method(const cat_http_parser_t *parser)
{
    return parser->llhttp.method;
}

CAT_API const char  *cat_http_parser_get_method_name(const cat_http_parser_t *parser)
{
    return cat_http_method_get_name(parser->llhttp.method);
}

CAT_API uint8_t cat_http_parser_get_major_version(const cat_http_parser_t *parser)
{
    return parser->llhttp.http_major;
}

CAT_API uint8_t cat_http_parser_get_minor_version(const cat_http_parser_t *parser)
{
    return parser->llhttp.http_minor;
}

CAT_API const char *cat_http_parser_get_protocol_version(const cat_http_parser_t *parser)
{
    if (parser->llhttp.http_major == 1) {
        if (parser->llhttp.http_minor == 1) {
            return "1.1";
        }
        if (parser->llhttp.http_minor == 0) {
            return "1.0";
        }
    }
    if (parser->llhttp.http_major == 2) {
        if (parser->llhttp.http_minor == 0) {
            return "2";
        }
    }
    return "UNKNOWN";
}

CAT_API cat_http_status_code_t cat_http_parser_get_status_code(const cat_http_parser_t *parser)
{
    return parser->llhttp.status_code;
}

CAT_API const char *cat_http_parser_get_reason_phrase(const cat_http_parser_t *parser)
{
    return cat_http_status_get_reason(parser->llhttp.status_code);
}

CAT_API uint64_t cat_http_parser_get_content_length(const cat_http_parser_t *parser)
{
    return parser->content_length;
}

CAT_API uint64_t cat_http_parser_get_current_chunk_length(const cat_http_parser_t *parser)
{
    return parser->current_chunk_length;
}

CAT_API cat_bool_t cat_http_parser_is_chunked(const cat_http_parser_t *parser)
{
    return parser->llhttp.flags & F_CHUNKED;
}

CAT_API cat_bool_t cat_http_parser_is_upgrade(const cat_http_parser_t *parser)
{
    return !!parser->llhttp.upgrade;
}

CAT_API cat_bool_t cat_http_parser_is_multipart(const cat_http_parser_t *parser)
{
    return parser->multipart.boundary_length >= 2;
}

/* module */

static void cat_http_parser_update_last_error(cat_http_parser_internal_errno_t error, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    cat_update_last_error_va_list(CAT_HTTP_PARSER_STANDARD_ERRNO_START - error, format, args);
    va_end(args);
}

static const char *cat_http_parser_strerrno_function(cat_errno_t error)
{
#define CAT_HTTP_PARSER_STRERROR_GEN(code, name, string) case CAT_EHP_ ## name: return "EHP_" #name;
    switch (error) {
        CAT_HTTP_PARSER_ERRNO_MAP(CAT_HTTP_PARSER_STRERROR_GEN)
    }
#undef CAT_HTTP_PARSER_STRERROR_GEN
    return NULL;
}

CAT_API cat_bool_t cat_http_module_init(void)
{
    cat_strerrno_handler_register(cat_http_parser_strerrno_function);

    return cat_true;
}
