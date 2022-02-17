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

/* http parser definations */

CAT_STRCASECMP_FAST_FUNCTION(content_type, "content-type", "       \0    ");
CAT_STRCASECMP_FAST_FUNCTION(multipart_dash, "multipart/", "         \0");
CAT_STRCASECMP_FAST_FUNCTION(boundary_eq, "boundary=", "        \0");

#define cat_http_parser_throw_error(action, code, fmt, ...) do { \
    parser->internal_flags |= CAT_HTTP_PARSER_INTERNAL_FLAG_HAS_PREVIOUS_ERROR; \
    cat_update_last_error(code, fmt, ##__VA_ARGS__); \
    action; \
} while (0)

/* multipart parser things */

typedef enum cat_multipart_state_e {
    CAT_HTTP_MULTIPART_STATE_UNINIT = 0,
    CAT_HTTP_MULTIPART_STATE_IN_CONTENT_TYPE,
    CAT_HTTP_MULTIPART_STATE_TYPE_IS_MULTIPART,
    CAT_HTTP_MULTIPART_STATE_ALMOST_BOUNDARY,
    CAT_HTTP_MULTIPART_STATE_BOUNDARY,
    CAT_HTTP_MULTIPART_STATE_BOUNDARY_START,
    CAT_HTTP_MULTIPART_STATE_BOUNDARY_COMMON,
    CAT_HTTP_MULTIPART_STATE_BOUNDARY_QUOTED,
    CAT_HTTP_MULTIPART_STATE_BOUNDARY_END,
    CAT_HTTP_MULTIPART_STATE_OUT_CONTENT_TYPE,
    CAT_HTTP_MULTIPART_STATE_NOT_MULTIPART,
    CAT_HTTP_MULTIPART_STATE_BOUNDARY_OK,
    CAT_HTTP_MULTIPART_STATE_IN_BODY,
    CAT_HTTP_MULTIPART_STATE_BODY_END,
} cat_multipart_state_t;

#define CAT_HTTP_MULTIPART_CB_FNAME(name) cat_http_multipart_parser_on_##name
#define CAT_HTTP_MULTIPART_ON_DATA(name, NAME) \
static long CAT_HTTP_MULTIPART_CB_FNAME(name)(multipart_parser *p, const char *at, size_t length) { \
    cat_http_parser_t* parser = cat_container_of(p, cat_http_parser_t, multipart); \
    parser->event = CAT_HTTP_PARSER_EVENT_##NAME; \
    parser->data = at; \
    parser->data_length = length; \
    CAT_LOG_DEBUG_SCOPE_START_WITH_LEVEL_EX(PROTOCOL, 3, char *tmp) { \
        CAT_LOG_DEBUG_D(PROTOCOL, "HTTP multipart parser data on_" # name ": [%zu] %s", \
            length, cat_log_buffer_quote_unlimited(at, length, &tmp)); \
    } CAT_LOG_DEBUG_SCOPE_END_WITH_LEVEL_EX(cat_free(tmp)); \
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
    CAT_LOG_DEBUG_V2(PROTOCOL, "HTTP multipart parser notify on_" # name ); \
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
    CAT_ASSERT(!(parser->events & CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) || parser->multipart_state == CAT_HTTP_MULTIPART_STATE_IN_BODY);
    // escape mp parser
    parser->event = CAT_HTTP_PARSER_EVENT_MULTIPART_BODY_END;
    parser->multipart_state = CAT_HTTP_MULTIPART_STATE_NOT_MULTIPART;
    CAT_LOG_DEBUG_V2(PROTOCOL, "HTTP multipart parser data on_body_end");
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

static cat_always_inline void cat_http_parser_shrink_boundary(cat_http_parser_t *parser)
{
    while (parser->multipart.boundary_length > 2 &&
            (parser->multipart.multipart_boundary[parser->multipart.boundary_length - 1] == '\t' ||
            parser->multipart.multipart_boundary[parser->multipart.boundary_length - 1] == ' ')) {
        parser->multipart.boundary_length--;
    }
}

/*
boundary syntax from RFC2046 section 5.1.1
boundary := 0*69<bchars> bcharsnospace
bchars := bcharsnospace / " "
bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" /
    "+" / "_" / "," / "-" / "." /
    "/" / ":" / "=" / "?"
*/
static cat_always_inline cat_bool_t cat_http_boundary_char_is_legal(char ch)
{
           /* '\'', '(' ')' */ \
    return ('\'' <= (ch) && (ch) <= ')') ||
           /* '+', ',', '-', '.', '/', NUM, ':' */
           ('+' <= (ch) && (ch) <= ':') ||
           (ch) == '=' ||
           (ch) == '?' ||
           ('A' <= (ch) && (ch) <= 'Z') ||
           (ch) == '_' ||
           ('a' <= (ch) && (ch) <= 'z') ||
           (ch) == ' ';
}

static int cat_http_parser_multipart_parse_content_type(cat_http_parser_t *parser, const char *at, size_t length)
{
    // global variables
    // not parsed inputs length
    size_t mp_length = length;
    // buf is [mp_at, mp_endp)
    const char *mp_endp = at + length;
#   define mp_at (mp_endp - mp_length)
#   define mp_state (parser->multipart_state)
    // local variables
    size_t mp_size;
    const char *mp_c;
    cat_bool_t mp_bool;
#define DEBUG_STATE(name) do { \
    CAT_LOG_DEBUG_V3(PROTOCOL, "multipart_parser state at " #name " [%zu] %.*s", mp_length, (int) mp_length, mp_at); \
} while(0)
// consume specified length
#define CONSUME_BUF(len) do { \
    CAT_ASSERT(parser->multipart_header_index < len); \
    mp_size = mp_length + (size_t) parser->multipart_header_index; \
    if (mp_size < len) { \
        /* not enough, break */ \
        memcpy(parser->multipart_header + parser->multipart_header_index, mp_at, mp_length); \
        parser->multipart_header_index = (uint8_t) mp_size; \
        return CAT_HTTP_PARSER_E_OK; \
    } \
    /* copy at to buffer */ \
    memcpy(parser->multipart_header + parser->multipart_header_index, mp_at, len - parser->multipart_header_index); \
    mp_length -= len - parser->multipart_header_index; \
    /* clean multipart_header_index */ \
    parser->multipart_header_index = 0; \
} while(0)
// consume until condition
#define CONSUME_UNTIL(cond) do { \
    for (mp_c = mp_at; mp_c < mp_endp; mp_c++) { \
        if (cond) { \
            break; \
        } \
    } \
    if (mp_c == mp_endp) { \
        return CAT_HTTP_PARSER_E_OK; \
    } \
    /* update mp_length */ \
    mp_length = mp_endp - mp_c; \
} while(0)

    switch (mp_state) {
        case CAT_HTTP_MULTIPART_STATE_IN_CONTENT_TYPE:
            // s_start "start"
            DEBUG_STATE(s_start);
            CONSUME_BUF(10);

            if (!cat_strcasecmp_fast_multipart_dash(parser->multipart_header)) {
                // not mp
                mp_state = CAT_HTTP_MULTIPART_STATE_NOT_MULTIPART;
                return CAT_HTTP_PARSER_E_OK;
            }
            // is mp
            mp_state = CAT_HTTP_MULTIPART_STATE_TYPE_IS_MULTIPART;
            /* fallthrough */
        case CAT_HTTP_MULTIPART_STATE_TYPE_IS_MULTIPART:
_s_mime_type_end:
            // s_mime_type_end "mime type end"
            DEBUG_STATE(s_mime_type_end);
            // skip to next semicolon
            CONSUME_UNTIL(*mp_c == ';');

            // consume ';'
            mp_length--;
            mp_state = CAT_HTTP_MULTIPART_STATE_ALMOST_BOUNDARY;
            /* fallthrough */
        case CAT_HTTP_MULTIPART_STATE_ALMOST_BOUNDARY:
_s_almost_boundary:
            // s_almost_boundary "almost 'boundary='"
            DEBUG_STATE(s_almost_boundary);
            // skip ows
            CONSUME_UNTIL(*mp_c != ' ' && *mp_c != '\t');

            mp_state = CAT_HTTP_MULTIPART_STATE_BOUNDARY;
            /* fallthrough */
        case CAT_HTTP_MULTIPART_STATE_BOUNDARY:
            // s_boundary "boundary="
            DEBUG_STATE(s_boundary);
            CONSUME_BUF(9);

            // if "multipart/dasd; foo; boundary=";
            //       header buf is ^ -----> ^
            // so skip to semicolon
            for (mp_c = parser->multipart_header + 9 - 1; mp_c >= parser->multipart_header; mp_c--) {
                if (*mp_c == ';') {
                    break;
                }
            }
            if (mp_c >= parser->multipart_header) {
                mp_c++; /* ';' */
                // sizeof new things
                mp_length += parser->multipart_header + 9 - mp_c;
                goto _s_almost_boundary;
            }

            if (!cat_strcasecmp_fast_boundary_eq(parser->multipart_header)) {
                // not boundary=
                // roll back to state "mime type end"
                mp_state = CAT_HTTP_MULTIPART_STATE_TYPE_IS_MULTIPART;
                goto _s_mime_type_end;
            }
            if (parser->multipart.boundary_length != 0) {
                cat_http_parser_throw_error(return -1, CAT_HTTP_PARSER_E_MULTIPART_HEADER, "Duplicate boundary=");
            }
            // is boundary=
            mp_state = CAT_HTTP_MULTIPART_STATE_BOUNDARY_START;
            /* fallthrough */
        case CAT_HTTP_MULTIPART_STATE_BOUNDARY_START:
            // s_boundary_data "boundary data" NOT_ACCEPTABLE
            DEBUG_STATE(s_boundary_data);
            if (mp_length == 0) {
                return CAT_HTTP_PARSER_E_OK;
            }
            // reset boundary buf
            parser->multipart.boundary_length = 2;
            parser->multipart.multipart_boundary[0] = '-';
            parser->multipart.multipart_boundary[1] = '-';

            // goto next state
            mp_state = CAT_HTTP_MULTIPART_STATE_BOUNDARY_COMMON;
            if (mp_at[0] == '"') {
                mp_length--;
                mp_state = CAT_HTTP_MULTIPART_STATE_BOUNDARY_QUOTED;
                goto _s_boundary_quoted_data;
            }
            /* fallthrough */
        case CAT_HTTP_MULTIPART_STATE_BOUNDARY_COMMON:
            // s_boundary_common_data "boundary common data"
            DEBUG_STATE(s_boundary_common_data);

            // find parsed size
            for (mp_c = mp_at; mp_c < mp_endp; mp_c++) {
                if (!cat_http_boundary_char_is_legal(*mp_c)) {
                    break;
                }
            }
            mp_size = mp_c - mp_at;

            // should we keep on the state
            mp_bool = cat_true;
            if (mp_size + parser->multipart.boundary_length > BOUNDARY_MAX_LEN + 2) {
                // out of range
                mp_size = BOUNDARY_MAX_LEN + 2 - parser->multipart.boundary_length;
            }
            if (mp_size != mp_length) {
                // break at middle
                mp_bool = cat_false;
            }
            memcpy(parser->multipart.multipart_boundary + parser->multipart.boundary_length, mp_at, mp_size);
            parser->multipart.boundary_length += (uint8_t) mp_size;
            mp_length -= mp_size;

            if (mp_bool) {
                return CAT_HTTP_PARSER_E_OK;
            }

            cat_http_parser_shrink_boundary(parser);
            if (parser->multipart.boundary_length == 0) {
                cat_http_parser_throw_error(return -1, CAT_HTTP_PARSER_E_MULTIPART_HEADER, "Empty boundary");
            }

            mp_state = CAT_HTTP_MULTIPART_STATE_BOUNDARY_END;
            /* fallthrough */
        case CAT_HTTP_MULTIPART_STATE_BOUNDARY_END:
s_boundary_end:
            // s_boundary_end "boundary end"
            DEBUG_STATE(s_boundary_end);

            // consume to semicolon
            CONSUME_UNTIL((*mp_c) != '\t' && (*mp_c) != ' ');

            if (*mp_c != ';') {
                cat_http_parser_throw_error(return -1, CAT_HTTP_PARSER_E_MULTIPART_HEADER, "Extra char at boundary end");
            }
            length--;
            mp_state = CAT_HTTP_MULTIPART_STATE_TYPE_IS_MULTIPART;
            goto _s_mime_type_end;
        case CAT_HTTP_MULTIPART_STATE_BOUNDARY_QUOTED:
_s_boundary_quoted_data:
            // s_boundary_quoted_data "boundary quoted data" NOT_ACCEPTABLE
            DEBUG_STATE(s_boundary_quoted_data);

            // find parsed size
            for (mp_c = mp_at; mp_c < mp_endp; mp_c++) {
                if (!cat_http_boundary_char_is_legal(*mp_c)) {
                    break;
                }
            }
            mp_size = mp_c - mp_at;

            if (mp_size + parser->multipart.boundary_length > BOUNDARY_MAX_LEN + 2) {
                cat_http_parser_throw_error(return -1, CAT_HTTP_PARSER_E_MULTIPART_HEADER, "Boundary is too long");
            }
            // copy to buf
            memcpy(parser->multipart.multipart_boundary + parser->multipart.boundary_length, mp_at, mp_size);
            parser->multipart.boundary_length += (uint8_t) mp_size;

            if (mp_length == mp_size) {
                // not end
                return CAT_HTTP_PARSER_E_OK;
            }

            // ending
            mp_length -= mp_size + 1;
            if (*mp_c != '"') {
                cat_http_parser_throw_error(return -1, CAT_HTTP_PARSER_E_MULTIPART_HEADER, "Illegal charactor '%c' in boundary", *mp_c);
            }
            if (parser->multipart.multipart_boundary[parser->multipart.boundary_length - 1] == ' ') {
                cat_http_parser_throw_error(return -1, CAT_HTTP_PARSER_E_MULTIPART_HEADER, "Boundary ends with space");
            }
            mp_state = CAT_HTTP_MULTIPART_STATE_BOUNDARY_END;
            goto s_boundary_end;
        default:
            // never here
            CAT_NEVER_HERE("Unknow state");
    }
#undef CONSUME_UNTIL
#undef CONSUME_BUF
#undef DEBUG_STATE
#undef mp_at
#undef mp_state
}

static cat_bool_t cat_http_parser_multipart_parser_init(cat_http_parser_t *parser)
{
    cat_http_parser_shrink_boundary(parser);
    if (parser->multipart.boundary_length <= 2) {
        cat_http_parser_throw_error(return cat_false, CAT_HTTP_PARSER_E_MULTIPART_HEADER, "Empty boundary on EOL");
    }
    if (0 != multipart_parser_init(&parser->multipart, NULL, 0, &cat_http_multipart_parser_settings)) {
        cat_http_parser_throw_error(return cat_false, CAT_HTTP_PARSER_E_MULTIPART_HEADER, "Multipart parser init failed");
    }
    CAT_LOG_DEBUG_V3(PROTOCOL, "multipart boundary is [%u] \"%.*s\"",
        parser->multipart.boundary_length, (int) parser->multipart.boundary_length, parser->multipart.multipart_boundary);
    parser->multipart_state = CAT_HTTP_MULTIPART_STATE_BOUNDARY_OK;

    return cat_true;
}

static cat_always_inline cat_bool_t cat_http_parser_multipart_state_solve(cat_http_parser_t *parser)
{
    switch (parser->multipart_state) {
        case CAT_HTTP_MULTIPART_STATE_UNINIT:
        case CAT_HTTP_MULTIPART_STATE_BOUNDARY_OK:
        case CAT_HTTP_MULTIPART_STATE_NOT_MULTIPART:
            return cat_true;
        case CAT_HTTP_MULTIPART_STATE_IN_CONTENT_TYPE:
        case CAT_HTTP_MULTIPART_STATE_TYPE_IS_MULTIPART:
        case CAT_HTTP_MULTIPART_STATE_ALMOST_BOUNDARY:
        case CAT_HTTP_MULTIPART_STATE_BOUNDARY:
        case CAT_HTTP_MULTIPART_STATE_BOUNDARY_COMMON:
        case CAT_HTTP_MULTIPART_STATE_BOUNDARY_END:
            return cat_http_parser_multipart_parser_init(parser);
        case CAT_HTTP_MULTIPART_STATE_BOUNDARY_START:
        case CAT_HTTP_MULTIPART_STATE_BOUNDARY_QUOTED:
            cat_http_parser_throw_error(return cat_false, CAT_HTTP_PARSER_E_MULTIPART_HEADER, "Unexpected EOF on parsing Content-Type header");
        case CAT_HTTP_MULTIPART_STATE_OUT_CONTENT_TYPE:
            CAT_NEVER_HERE("Never used CAT_HTTP_MULTIPART_STATE_OUT_CONTENT_TYPE");
            CAT_FALLTHROUGH;
        case CAT_HTTP_MULTIPART_STATE_IN_BODY:
        case CAT_HTTP_MULTIPART_STATE_BODY_END:
            CAT_NEVER_HERE("Should not be body here");
            CAT_FALLTHROUGH;
        default:
            CAT_NEVER_HERE("Unknown state, maybe memory corrupt");
    }
}

/* http parser things */

static cat_always_inline cat_http_parser_t *cat_http_parser_get_from_handle(const llhttp_t *llhttp)
{
    return cat_container_of(llhttp, cat_http_parser_t, llhttp);
}

typedef enum cat_http_parser_complete_states_e {
    CAT_HTTP_PARSER_COMPLETE_STATE_HEADER_FIELD_COMPLETE = 1,
    CAT_HTTP_PARSER_COMPLETE_STATE_HEADER_VALUE_COMPLETE = 2,
} cat_http_parser_complete_states_t;

static cat_always_inline cat_bool_t cat_http_parser_on_header_value_complete(llhttp_t *llhttp)
{
    cat_http_parser_t *parser = cat_http_parser_get_from_handle(llhttp);

    if (parser->complete_state == CAT_HTTP_PARSER_COMPLETE_STATE_HEADER_VALUE_COMPLETE) {
        // not first called
        return cat_true;
    }
    parser->complete_state = CAT_HTTP_PARSER_COMPLETE_STATE_HEADER_VALUE_COMPLETE;

    return cat_http_parser_multipart_state_solve(parser);
}

#define CAT_HTTP_PARSER_ON_EVENT_BEGIN(name, NAME) \
static int cat_http_parser_on_##name(llhttp_t *llhttp) \
{ \
    cat_http_parser_t *parser = cat_http_parser_get_from_handle(llhttp); \
    \
    parser->event = CAT_HTTP_PARSER_EVENT_##NAME; \
    \

#define CAT_HTTP_PARSER_ON_EVENT_END() \
    if (((cat_http_parser_event_t) (parser->events & parser->event)) == parser->event) { \
        return CAT_HTTP_PARSER_E_PAUSED; \
    } else { \
        return CAT_HTTP_PARSER_E_OK; \
    } \
}

#define CAT_HTTP_PARSER_ON_EVENT(name, NAME) \
CAT_HTTP_PARSER_ON_EVENT_BEGIN(name, NAME) \
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

CAT_HTTP_PARSER_ON_EVENT(message_begin, MESSAGE_BEGIN)
CAT_HTTP_PARSER_ON_DATA (url,           URL          )
CAT_HTTP_PARSER_ON_DATA (status,        STATUS       )

CAT_HTTP_PARSER_ON_DATA_BEGIN(header_field, HEADER_FIELD) {
    if (parser->events & CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) {
        if (!cat_http_parser_on_header_value_complete(llhttp)) {
            return -1;
        }
        // copy header into buffer
        if (length != 0) {
            size_t index_new = (size_t) parser->multipart_header_index + length;
            if (index_new < 13) {
                memcpy(parser->multipart_header + parser->multipart_header_index, at, length);
                parser->multipart_header_index = (uint8_t) index_new;
            } else {
                parser->multipart_header_index = UINT8_MAX;
            }
        }
    }
} CAT_HTTP_PARSER_ON_EVENT_END()

CAT_HTTP_PARSER_ON_DATA_BEGIN(header_value, HEADER_VALUE) {
    if (parser->events & CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) {
        int error;
        if (parser->complete_state == CAT_HTTP_PARSER_COMPLETE_STATE_HEADER_VALUE_COMPLETE) {
            // first called
            parser->complete_state = CAT_HTTP_PARSER_COMPLETE_STATE_HEADER_FIELD_COMPLETE;
            if (parser->multipart_header_index == 12 && cat_strcasecmp_fast_content_type(parser->multipart_header)) {
                if (parser->multipart_state != CAT_HTTP_MULTIPART_STATE_UNINIT) {
                    cat_http_parser_throw_error(return -1, CAT_HTTP_PARSER_E_DUPLICATE_CONTENT_TYPE, "Duplicate Content-Type header");
                }
                CAT_LOG_DEBUG_V3(PROTOCOL, "multipart_parser on_header_field_complete found Content-Type");
                parser->multipart_state = CAT_HTTP_MULTIPART_STATE_IN_CONTENT_TYPE;
            }
            parser->multipart_header_index = 0;
        }
        if (
            length > 0 &&
            parser->multipart_state >= CAT_HTTP_MULTIPART_STATE_IN_CONTENT_TYPE &&
            parser->multipart_state < CAT_HTTP_MULTIPART_STATE_OUT_CONTENT_TYPE
        ) {
            CAT_LOG_DEBUG_V3(PROTOCOL, "multipart_parser Content-Type state %d", parser->multipart_state);
            error = cat_http_parser_multipart_parse_content_type(parser, at, length);
            if (CAT_HTTP_PARSER_E_OK != error) {
                return -1;
            }
        }
    }
} CAT_HTTP_PARSER_ON_DATA_END()

CAT_HTTP_PARSER_ON_EVENT_BEGIN(headers_complete, HEADERS_COMPLETE) {
    parser->keep_alive = !!llhttp_should_keep_alive(&parser->llhttp);
    parser->content_length = parser->llhttp.content_length;
    if (parser->events & CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) {
        if (!cat_http_parser_on_header_value_complete(llhttp)) {
            return -1;
        }
    }
    parser->complete_state = 0;
} CAT_HTTP_PARSER_ON_EVENT_END()

CAT_HTTP_PARSER_ON_DATA_BEGIN(body, BODY) {
    if (parser->events & CAT_HTTP_PARSER_EVENT_FLAG_MULTIPART) {
        CAT_ASSERT(
            parser->multipart_state == CAT_HTTP_MULTIPART_STATE_UNINIT ||
            parser->multipart_state == CAT_HTTP_MULTIPART_STATE_NOT_MULTIPART ||
            parser->multipart_state == CAT_HTTP_MULTIPART_STATE_BOUNDARY_OK
        );
        if (parser->multipart_state == CAT_HTTP_MULTIPART_STATE_BOUNDARY_OK) {
            return CAT_HTTP_PARSER_E_PAUSED;
        }
    }
} CAT_HTTP_PARSER_ON_EVENT_END()

CAT_HTTP_PARSER_ON_EVENT_BEGIN(chunk_header, CHUNK_HEADER) {
    parser->current_chunk_length = parser->llhttp.content_length;
} CAT_HTTP_PARSER_ON_EVENT_END()
CAT_HTTP_PARSER_ON_EVENT(chunk_complete,   CHUNK_COMPLETE  )
CAT_HTTP_PARSER_ON_EVENT(message_complete, MESSAGE_COMPLETE)

static const llhttp_settings_t cat_http_parser_settings = {
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
    NULL, NULL, NULL, NULL,
};

static cat_always_inline void cat_http_parser__init(cat_http_parser_t *parser)
{
    parser->llhttp.method = CAT_HTTP_METHOD_UNKNOWN;
    parser->event = CAT_HTTP_PARSER_EVENT_NONE;
    parser->complete_state = 0;
    parser->data = NULL;
    parser->data_length = 0;
    parser->parsed_length = 0;
    parser->content_length = 0;
    parser->current_chunk_length = 0;
    parser->keep_alive = cat_false;
    parser->multipart_state = CAT_HTTP_MULTIPART_STATE_UNINIT;
    parser->multipart_header_index = 0;
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

static cat_http_parser_errno_t cat_http_parser_llhttp_execute(cat_http_parser_t *parser, const char *data, size_t length)
{
    cat_http_parser_errno_t error;

    parser->event = CAT_HTTP_PARSER_EVENT_NONE;
    error = (cat_http_parser_errno_t) llhttp_execute(&parser->llhttp, data, length);
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
        cat_http_parser_throw_error(return cat_false, CAT_HTTP_PARSER_E_MULTIPART_BODY, "Failed to parse multipart body: \"%.*s\"", error_length, error_buffer);
    }

    CAT_LOG_DEBUG_SCOPE_START_WITH_LEVEL_EX(PROTOCOL, 3, char *tmp) {
        CAT_LOG_DEBUG_D(PROTOCOL, "multipart_parser_execute() returns %zu, parsed \"%s\"",
            parsed_length, cat_log_buffer_quote_unlimited(data, parsed_length, &tmp));
    } CAT_LOG_DEBUG_SCOPE_END_WITH_LEVEL_EX(cat_free(tmp));
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
        cat_http_parser_throw_error(return cat_false, CAT_ENOTSUP, "Unsupported type: multipart/byterange");
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
    parser->multipart_state = CAT_HTTP_MULTIPART_STATE_IN_BODY;

    return ret;
}

CAT_API cat_bool_t cat_http_parser_execute(cat_http_parser_t *parser, const char *data, size_t length)
{
    cat_http_parser_errno_t error = CAT_HTTP_PARSER_E_OK;
    cat_bool_t ret;

    if (likely(parser->multipart_state != CAT_HTTP_MULTIPART_STATE_IN_BODY)) {
        // if not in multipart body
        error = cat_http_parser_llhttp_execute(parser, data, length);
        ret = error == CAT_HTTP_PARSER_E_OK;
        if (ret && parser->event == CAT_HTTP_PARSER_EVENT_BODY && parser->multipart_state == CAT_HTTP_MULTIPART_STATE_BOUNDARY_OK) {
            ret = cat_http_parser_solve_multipart_body(parser);
        }
    } else {
        // if in multipart body, bypass llhttp
        ret = cat_http_parser_multipart_parser_execute(parser, data, length);
    }
    if (unlikely(!ret)) {
        if (!(parser->internal_flags & CAT_HTTP_PARSER_INTERNAL_FLAG_HAS_PREVIOUS_ERROR)) {
            cat_update_last_error(error, "HTTP-Parser execute failed: %s", llhttp_get_error_reason(&parser->llhttp));
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
        parser->multipart_state = CAT_HTTP_MULTIPART_STATE_UNINIT;
        parser->multipart.boundary_length = 0;
    }

#ifdef CAT_DEBUG
    if (!(parser->event & CAT_HTTP_PARSER_EVENT_FLAG_DATA)) {
        CAT_LOG_DEBUG_V2(PROTOCOL, "http_parser_execute() parsed %-2zu return %-12s",
            parser->parsed_length, cat_http_parser_get_event_name(parser));
    } else {
        CAT_LOG_DEBUG_SCOPE_START_WITH_LEVEL_EX(PROTOCOL, 2, char *tmp) {
            CAT_LOG_DEBUG_D(PROTOCOL, "http_parser_execute() parsed %-2zu return %-12s [%zu] %s",
                parser->parsed_length, cat_http_parser_get_event_name(parser),
                parser->data_length, cat_log_buffer_quote_unlimited(parser->data, parser->data_length, &tmp));
        } CAT_LOG_DEBUG_SCOPE_END_WITH_LEVEL_EX(cat_free(tmp));
    }
#endif
    return cat_true;
}

CAT_API const char *cat_http_parser_event_name(cat_http_parser_event_t event)
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
    if (unlikely(error != HPE_OK)) {
        cat_update_last_error(error, "HTTP-Parser finish failed: %s", llhttp_get_error_reason(&parser->llhttp));
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
        parser->multipart_state == CAT_HTTP_MULTIPART_STATE_IN_BODY
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
    return parser->multipart_state >= CAT_HTTP_MULTIPART_STATE_BOUNDARY_OK;
}
