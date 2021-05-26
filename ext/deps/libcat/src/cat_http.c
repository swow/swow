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

/* parser */

static cat_always_inline cat_http_parser_t *cat_http_parser_get_from_handle(llhttp_t *llhttp)
{
    return cat_container_of(llhttp, cat_http_parser_t, llhttp);
}

static cat_bool_t cat_llhttp_should_keep_alive(llhttp_t *llhttp)
{
    if (llhttp->http_major == 1) {
        if (llhttp->http_minor == 1) {
            if (!(llhttp->flags & F_CONNECTION_CLOSE)) {
                return cat_true;
            }
        } else if (llhttp->http_minor == 0) {
            if (llhttp->flags & F_CONNECTION_KEEP_ALIVE) {
                return cat_true;
            }
        }
    }

    return cat_false;
}

#define CAT_HTTP_PARSER_ON_EVENT_BEGIN(name, NAME) \
static int cat_http_parser_on_##name(llhttp_t *llhttp) \
{ \
    cat_http_parser_t *parser = cat_http_parser_get_from_handle(llhttp); \
    \
    parser->event = CAT_HTTP_PARSER_EVENT_##NAME; \
    \

#define CAT_HTTP_PARSER_ON_EVENT_END() \
    if ((cat_http_parser_event_t) (parser->events & parser->event) == parser->event) { \
        return HPE_PAUSED; \
    } else { \
        return HPE_OK; \
    } \
}

#define CAT_HTTP_PARSER_ON_EVENT(name, NAME) \
CAT_HTTP_PARSER_ON_EVENT_BEGIN(name, NAME) \
CAT_HTTP_PARSER_ON_EVENT_END()

#define CAT_HTTP_PARSER_ON_HDONE(name, NAME) \
CAT_HTTP_PARSER_ON_EVENT_BEGIN(name, NAME) \
    parser->keep_alive = cat_llhttp_should_keep_alive(llhttp); \
CAT_HTTP_PARSER_ON_EVENT_END()

#define CAT_HTTP_PARSER_ON_DATA_BEGIN(name, NAME) \
static int cat_http_parser_on_##name(llhttp_t *llhttp, const char *at, size_t length) \
{ \
    cat_http_parser_t *parser = cat_http_parser_get_from_handle(llhttp); \
    \
    parser->event = CAT_HTTP_PARSER_EVENT_##NAME; \
    parser->data = at; \
    parser->data_length = length;

#define CAT_HTTP_PARSER_ON_DATA_END() CAT_HTTP_PARSER_ON_EVENT_END()

#define CAT_HTTP_PARSER_ON_DATA(name, NAME) \
CAT_HTTP_PARSER_ON_DATA_BEGIN(name, NAME) \
CAT_HTTP_PARSER_ON_DATA_END()

#define CAT_HTTP_PARSER_ON_DONE(name, NAME) \
static int cat_http_parser_on_##name(llhttp_t *llhttp) \
{ \
    cat_http_parser_t *parser = cat_http_parser_get_from_handle(llhttp); \
    \
    if (parser->event != CAT_HTTP_PARSER_EVENT_##NAME) { \
        /* first execute, may paused */ \
        parser->event = CAT_HTTP_PARSER_EVENT_##NAME; \
        if ((cat_http_parser_event_t) (parser->events & parser->event) == parser->event) { \
            return HPE_PAUSED; \
        } \
    } \
    \
    return HPE_OK; \
}

CAT_HTTP_PARSER_ON_EVENT(message_begin,         MESSAGE_BEGIN        )
CAT_HTTP_PARSER_ON_DATA (url,                   URL                  )
CAT_HTTP_PARSER_ON_EVENT(url_complete,          URL_COMPLETE         )
CAT_HTTP_PARSER_ON_DATA (status,                STATUS               )
CAT_HTTP_PARSER_ON_EVENT(status_complete,       STATUS_COMPLETE      )
CAT_HTTP_PARSER_ON_DATA (header_field,          HEADER_FIELD         )
CAT_HTTP_PARSER_ON_DATA (header_value,          HEADER_VALUE         )
CAT_HTTP_PARSER_ON_EVENT(header_field_complete, HEADER_FIELD_COMPLETE)
CAT_HTTP_PARSER_ON_EVENT(header_value_complete, HEADER_VALUE_COMPLETE)
CAT_HTTP_PARSER_ON_HDONE(headers_complete,      HEADERS_COMPLETE     )
CAT_HTTP_PARSER_ON_DATA (body,                  BODY                 )
CAT_HTTP_PARSER_ON_EVENT(chunk_header,          CHUNK_HEADER         )
CAT_HTTP_PARSER_ON_EVENT(chunk_complete,        CHUNK_COMPLETE       )
CAT_HTTP_PARSER_ON_DONE (message_complete,      MESSAGE_COMPLETE     )

const llhttp_settings_t cat_http_parser_settings = {
    cat_http_parser_on_message_begin,
    cat_http_parser_on_url,
    cat_http_parser_on_status,
    cat_http_parser_on_header_field,
    cat_http_parser_on_header_value,
    cat_http_parser_on_headers_complete,
    cat_http_parser_on_body,
    cat_http_parser_on_message_complete,
    cat_http_parser_on_chunk_header,
    cat_http_parser_on_chunk_complete,
    cat_http_parser_on_url_complete,
    cat_http_parser_on_status_complete,
    cat_http_parser_on_header_field_complete,
    cat_http_parser_on_header_value_complete,
};

static cat_always_inline void cat_http_parser__init(cat_http_parser_t *parser)
{
    parser->llhttp.method = CAT_HTTP_METHOD_UNKNOWN;
    parser->data = NULL;
    parser->data_length = 0;
    parser->keep_alive = cat_false;
}

CAT_API void cat_http_parser_init(cat_http_parser_t *parser)
{
    llhttp_init(&parser->llhttp, HTTP_BOTH, &cat_http_parser_settings);
    cat_http_parser__init(parser);
    parser->events = CAT_HTTP_PARSER_EVENTS_NONE;
    parser->event = CAT_HTTP_PARSER_EVENT_NONE;
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
#ifndef CAT_ALLOC_NEVER_RETURNS_NULL
        if (unlikely(parser == NULL)) {
            cat_update_last_error_of_syscall("Malloc for HTTP parser failed");
            return NULL;
        }
#endif
    }
    cat_http_parser_init(parser);

    return parser;
}

CAT_API cat_http_parser_type_t cat_http_parser_get_type(cat_http_parser_t *parser)
{
    return parser->llhttp.type;
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

CAT_API cat_bool_t cat_http_parser_execute(cat_http_parser_t *parser, const char *data, size_t length)
{
    llhttp_errno_t error;

    parser->event = CAT_HTTP_PARSER_EVENT_NONE;
    llhttp_resume(&parser->llhttp);
    error = llhttp_execute(&parser->llhttp, data, length);
    if (unlikely(error != HPE_OK)) {
        if (unlikely(error != HPE_PAUSED)) {
            if (unlikely(error != HPE_PAUSED_UPGRADE)) {
                goto _error;
            }
        } else if (unlikely(parser->event == CAT_HTTP_PARSER_EVENT_MESSAGE_COMPLETE)) {
            llhttp_resume(&parser->llhttp);
            error = llhttp_execute(&parser->llhttp, NULL, 0);
            if (unlikely(error != HPE_OK && error != HPE_PAUSED_UPGRADE)) {
                goto _error;
            }
        }
    }

    return cat_true;

    _error:
    cat_update_last_error(error, "HTTP-Parser execute failed: %s", llhttp_errno_name(error));
    return cat_false;
}

CAT_API const char *cat_http_parser_event_name(cat_http_parser_event_t event)
{
    switch (event) {
#define CAT_HTTP_PARSER_EVENT_NAME_GEN(name, unused1) case CAT_HTTP_PARSER_EVENT_##name: return #name;
    CAT_HTTP_PARSER_EVENT_MAP(CAT_HTTP_PARSER_EVENT_NAME_GEN)
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
    return cat_http_parser_event_name(parser->event);
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
    llhttp_errno_t error;

    error = llhttp_finish(&parser->llhttp);
    if (unlikely(error != 0)) {
        cat_update_last_error(error, "HTTP-Parser finish failed: %s", llhttp_errno_name(error));
        return cat_false;
    }

    return cat_true;
}

CAT_API llhttp_errno_t cat_http_parser_get_error_code(const cat_http_parser_t *parser)
{
    return llhttp_get_errno(&parser->llhttp);
}

CAT_API const char *cat_http_parser_get_error_message(const cat_http_parser_t *parser)
{
    return llhttp_get_error_reason(&parser->llhttp);
}

CAT_API cat_bool_t cat_http_parser_is_completed(const cat_http_parser_t *parser)
{
    return parser->event == CAT_HTTP_PARSER_EVENT_MESSAGE_COMPLETE;
}

CAT_API const char *cat_http_parser_get_current_pos(const cat_http_parser_t *parser)
{
    return llhttp_get_error_pos(&parser->llhttp);
}

CAT_API size_t cat_http_parser_get_parsed_length(cat_http_parser_t *parser, const char *data)
{
    const char *p = cat_http_parser_get_current_pos(parser);

    if (p == NULL) {
        return 0;
    }

    return p - data;
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
    return parser->llhttp.content_length;
}

CAT_API cat_bool_t cat_http_parser_is_upgrade(const cat_http_parser_t *parser)
{
    return !!parser->llhttp.upgrade;
}
