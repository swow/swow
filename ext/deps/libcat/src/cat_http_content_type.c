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
#include "cat_http.h"
#endif

CAT_STRCASECMP_FAST_FUNCTION(multipart_slash, "multipart/", "         \0");

enum media_type_state {
    mt_error = 0,
    mt_start = 1,
    /* 1 - 10 in_multipart */
    mt_maybe_subtype = mt_start+10,
    mt_subtype,
    mt_parameter_ows1_acceptable,
    mt_parameter_ows1,
    mt_parameter_ows2,
    mt_parameter_name,
    mt_parameter_value,
    mt_parameter_literal_value_acceptable,
    mt_parameter_quoted_value,
    mt_quoted_pair,
    mt_boundary_eq_o,
    mt_boundary_eq_u,
    mt_boundary_eq_n,
    mt_boundary_eq_d,
    mt_boundary_eq_a,
    mt_boundary_eq_r,
    mt_boundary_eq_y,
    mt_boundary_eq_eq,
    mt_boundary_value,
    mt_boundary_literal_value_acceptable,
    mt_boundary_quoted_value,
    mt_boundary_quoted_value_unacceptable,
    mt_boundary_quoted_pair,
};

static cat_always_inline void cat_http_parser_multipart_parse_content_type_init(cat_http_parser_t *parser) {
    parser->header_value_parser_state = mt_start;
    parser->multipart.boundary_length = 0;
    parser->multipart.multipart_boundary[0] = 0;
}

static cat_always_inline cat_bool_t cat_http_parser_multipart_parse_content_type(
    cat_http_parser_t *parser,
    const char *p,
    const char *pe,
    const char *eof
) {
    if (p == eof) {
        goto next;
    }

#define state parser->header_value_parser_state
#define tchar_cases \
    case '!': CAT_FALLTHROUGH; \
    case '#': CAT_FALLTHROUGH; \
    case '$': CAT_FALLTHROUGH; \
    case '%': CAT_FALLTHROUGH; \
    case '&': CAT_FALLTHROUGH; \
    case '\'': CAT_FALLTHROUGH; \
    case '*': CAT_FALLTHROUGH; \
    case '+': CAT_FALLTHROUGH; \
    case '-': CAT_FALLTHROUGH; \
    case '.': CAT_FALLTHROUGH; \
    case '^': CAT_FALLTHROUGH; \
    case '_': CAT_FALLTHROUGH; \
    case '`': CAT_FALLTHROUGH; \
    case '|': CAT_FALLTHROUGH; \
    case '~':
#define digit_cases \
    case '0': CAT_FALLTHROUGH; \
    case '1': CAT_FALLTHROUGH; \
    case '2': CAT_FALLTHROUGH; \
    case '3': CAT_FALLTHROUGH; \
    case '4': CAT_FALLTHROUGH; \
    case '5': CAT_FALLTHROUGH; \
    case '6': CAT_FALLTHROUGH; \
    case '7': CAT_FALLTHROUGH; \
    case '8': CAT_FALLTHROUGH; \
    case '9':
#define alpha_cases \
    case 'a': CAT_FALLTHROUGH; \
    case 'b': CAT_FALLTHROUGH; \
    case 'c': CAT_FALLTHROUGH; \
    case 'd': CAT_FALLTHROUGH; \
    case 'e': CAT_FALLTHROUGH; \
    case 'f': CAT_FALLTHROUGH; \
    case 'g': CAT_FALLTHROUGH; \
    case 'h': CAT_FALLTHROUGH; \
    case 'i': CAT_FALLTHROUGH; \
    case 'j': CAT_FALLTHROUGH; \
    case 'k': CAT_FALLTHROUGH; \
    case 'l': CAT_FALLTHROUGH; \
    case 'm': CAT_FALLTHROUGH; \
    case 'n': CAT_FALLTHROUGH; \
    case 'o': CAT_FALLTHROUGH; \
    case 'p': CAT_FALLTHROUGH; \
    case 'q': CAT_FALLTHROUGH; \
    case 'r': CAT_FALLTHROUGH; \
    case 's': CAT_FALLTHROUGH; \
    case 't': CAT_FALLTHROUGH; \
    case 'u': CAT_FALLTHROUGH; \
    case 'v': CAT_FALLTHROUGH; \
    case 'w': CAT_FALLTHROUGH; \
    case 'x': CAT_FALLTHROUGH; \
    case 'y': CAT_FALLTHROUGH; \
    case 'z': CAT_FALLTHROUGH; \
    case 'A': CAT_FALLTHROUGH; \
    case 'B': CAT_FALLTHROUGH; \
    case 'C': CAT_FALLTHROUGH; \
    case 'D': CAT_FALLTHROUGH; \
    case 'E': CAT_FALLTHROUGH; \
    case 'F': CAT_FALLTHROUGH; \
    case 'G': CAT_FALLTHROUGH; \
    case 'H': CAT_FALLTHROUGH; \
    case 'I': CAT_FALLTHROUGH; \
    case 'J': CAT_FALLTHROUGH; \
    case 'K': CAT_FALLTHROUGH; \
    case 'L': CAT_FALLTHROUGH; \
    case 'M': CAT_FALLTHROUGH; \
    case 'N': CAT_FALLTHROUGH; \
    case 'O': CAT_FALLTHROUGH; \
    case 'P': CAT_FALLTHROUGH; \
    case 'Q': CAT_FALLTHROUGH; \
    case 'R': CAT_FALLTHROUGH; \
    case 'S': CAT_FALLTHROUGH; \
    case 'T': CAT_FALLTHROUGH; \
    case 'U': CAT_FALLTHROUGH; \
    case 'V': CAT_FALLTHROUGH; \
    case 'W': CAT_FALLTHROUGH; \
    case 'X': CAT_FALLTHROUGH; \
    case 'Y': CAT_FALLTHROUGH; \
    case 'Z':
#define token_cases \
    tchar_cases CAT_FALLTHROUGH; \
    digit_cases CAT_FALLTHROUGH; \
    alpha_cases
#ifndef mt_dbg
# define mt_dbg() do { \
    printf("state %d *p='%c'\n", state, *p); \
} while (0)
#endif

    
    // cat_bool_t acceptable = cat_false;
    switch (state)
    {
        case mt_start: CAT_FALLTHROUGH;
        case mt_start + 1: CAT_FALLTHROUGH;
        case mt_start + 2: CAT_FALLTHROUGH;
        case mt_start + 3: CAT_FALLTHROUGH;
        case mt_start + 4: CAT_FALLTHROUGH;
        case mt_start + 5: CAT_FALLTHROUGH;
        case mt_start + 6: CAT_FALLTHROUGH;
        case mt_start + 7: CAT_FALLTHROUGH;
        case mt_start + 8: CAT_FALLTHROUGH;
        case mt_start + 9:
        mt_dbg();
            /*
            'multipart/' => continue to check
            */
            size_t read_len = pe - p > (10 - state + mt_start) ? (10 - state + mt_start) : pe - p;
            // printf("readlen: %d\n", read_len);
            memcpy(&parser->multipart.multipart_boundary[state], p, read_len);
            state += (int) read_len;
            p += read_len;

            if (pe - p == 0) {
                goto next;
            }
            if (unlikely(pe < p)) {
                CAT_ASSERT(0 && "logical bug");
                goto error;
            }
            CAT_FALLTHROUGH;
        case mt_maybe_subtype:
        mt_dbg();
            if (!cat_strcasecmp_fast_multipart_slash(&parser->multipart.multipart_boundary[1])) {
                /* not multipart */
                goto error;
            }
            /*
            tchar   = "!" | "#" | "$" | "%" | "&" | "'" | "*" | "+" | "-" | "." | "^" | "_" | "`" | "|" | "~" | [0-9a-zA-Z];
            token   = tchar +;
            subtype = token;

            subtype => mt_subtype
            */
            switch (*p) {
                token_cases
                    p++;
                    state = mt_subtype;
                    break;
                default:
                    goto error;
            }
            CAT_FALLTHROUGH;
        case mt_subtype:
        mt_dbg();
            /*
            ows = [ \t]*;

            subtype => self
            ows => mt_parameter_ows1
            ';' => mt_parameter_ows2
            */
            for (; p < pe; p++) {
                switch (*p) {
                    case ';':
                        p++;
                        state = mt_parameter_ows2;
                        goto mt_parameter_ows2;
                    case ' ': CAT_FALLTHROUGH;
                    case '\t':
                        state = mt_parameter_ows1_acceptable;
                        goto mt_parameter_ows1_acceptable;
                    token_cases
                        /* do nothing */
                        break;
                    default:
                        goto error;
                }
            }
            goto next;
        case mt_parameter_ows1_acceptable:
mt_parameter_ows1_acceptable:
        mt_dbg();
            p++;
            // printf("%p %p\n", p, pe);
            if (p == pe) {
                goto next;
            }
            if (unlikely(p > pe)) {
                CAT_ASSERT(0 && "logical bug");
                goto error;
            }
            state = mt_parameter_ows1;
            CAT_FALLTHROUGH;
        case mt_parameter_ows1:
mt_parameter_ows1:
        mt_dbg();
            /*
            ows => self
            ';' => mt_parameter_ows2
            */
            for (; p < pe; p++) {
                switch (*p) {
                    case ' ': CAT_FALLTHROUGH;
                    case '\t':
                        /* do nothing */
                        break;
                    case ';':
                        p++;
                        state = mt_parameter_ows2;
                        goto mt_parameter_ows2;
                    default:
                        goto error;
                }
            }
            goto next;
        case mt_parameter_ows2:
mt_parameter_ows2:
        mt_dbg();
            /*
            parameter_name  = token;

            ows => self
            parameter_name => mt_parameter_name
            [bB] => mt_boundary_eq_o
            */
            for (; p < pe; p++) {
                switch (*p) {
                    case ' ': CAT_FALLTHROUGH;
                    case '\t':
                        /* do nothing */
                        break;
                    token_cases
                        if (likely(*p == 'b' || *p == 'B')) {
                            p++;
                            state = mt_boundary_eq_o;
                            goto mt_boundary_eq_o;
                        }
                        state = mt_parameter_name;
                        goto mt_parameter_name;
                    default:
                        goto error;
                }
            }
            goto next;
        case mt_parameter_name:
mt_parameter_name:
        mt_dbg();
            /*
            parameter_name => self
            '=' => mt_parameter_value
            */
            for (; p < pe; p++) {
                switch (*p) {
                    token_cases
                        /* do nothing */
                        break;
                    case '=':
                        p++;
                        state = mt_parameter_value;
                        goto mt_parameter_value;
                    default:
                        goto error;
                }
            }
            goto next;
        case mt_parameter_value:
mt_parameter_value:
        mt_dbg();
            /*
            obs_text = [\x80-\xff];
            vchar    = [\x1f-\x7e];

            qdtext         = [\t] | [ ] | [\x21] | [\x23-\x5b] | [\x5d-\x7e] | obs_text;
            quoted_pair    = "\\" ( [\t] | [ ] | vchar | obs_text );
            quoted_string  = ["] ( qdtext | quoted_pair )* ["];

            parameter_value = (token | quoted_string)*;

            parameter_value => mt_parameter_literal_value
            ["] => mt_parameter_quoted_value
            ows => mt_parameter_ows1
            [;] => mt_parameter_ows2
            */
            switch (*p) {
                token_cases
                    state = mt_parameter_literal_value_acceptable;
                    goto mt_parameter_literal_value_acceptable;
                case '"':
                    p++;
                    state = mt_parameter_quoted_value;
                    goto mt_parameter_quoted_value;
                case ' ': CAT_FALLTHROUGH;
                case '\t':
                    state = mt_parameter_ows1;
                    goto mt_parameter_ows1;
                case ';':
                    p++;
                    state = mt_parameter_ows2;
                    goto mt_parameter_ows2;
                default:
                    goto error;
            }
            goto next;
        case mt_parameter_quoted_value:
mt_parameter_quoted_value:
        mt_dbg();
            /*
            qdtext => self
            "\\" => mt_quoted_pair
            ["] => mt_parameter_ows1
            */
            for (; p < pe; p++) {
                if (
                    *p == '\t' || *p == ' ' || *p == '\x21' ||
                    ('\x23' <= *p && *p <= '\x5b') ||
                    ('\x5d' <= *p && *p <= '\x7e') ||
                    (/* '\x80' <= *p && */ *p <= '\xff')
                ) {
                    /* qdtext */
                    /* do nothing */
                } else if (*p == '\\') {
                    p++;
                    state = mt_quoted_pair;
                    goto mt_quoted_pair;
                } else if (*p == '"') {
                    state = mt_parameter_ows1_acceptable;
                    goto mt_parameter_ows1_acceptable;
                } else {
                    goto error;
                }
            }
            goto next;
        case mt_quoted_pair:
mt_quoted_pair:
        mt_dbg();
            /*
            [ \t\x1f-\x7e\x80-\xff] => mt_parameter_quoted_value
            */
            if (
                *p == ' ' || *p == '\t' ||
                ('\x1f' <= *p && *p <= '\x7e') ||
                (/* '\x80' <= *p && */ *p <= '\xff')
            ) {
                p++;
                state = mt_parameter_quoted_value;
                goto mt_parameter_quoted_value;
            }
            goto error;
        case mt_parameter_literal_value_acceptable:
mt_parameter_literal_value_acceptable:
        mt_dbg();
            /*
            parameter_value => self
            ows => mt_parameter_ows1
            [;] => mt_parameter_ows2
            */
            for (; p < pe; p++) {
                switch (*p) {
                    token_cases
                        /* do nothing */
                        break;
                    case ' ': CAT_FALLTHROUGH;
                    case '\t':
                        state = mt_parameter_ows1;
                        goto mt_parameter_ows1;
                    case ';':
                        p++;
                        state = mt_parameter_ows2;
                        goto mt_parameter_ows2;
                    default:
                        goto error;
                }
            }
            goto next;
        case mt_boundary_eq_o: CAT_FALLTHROUGH;
        case mt_boundary_eq_u: CAT_FALLTHROUGH;
        case mt_boundary_eq_n: CAT_FALLTHROUGH;
        case mt_boundary_eq_d: CAT_FALLTHROUGH;
        case mt_boundary_eq_a: CAT_FALLTHROUGH;
        case mt_boundary_eq_r: CAT_FALLTHROUGH;
        case mt_boundary_eq_y:
mt_boundary_eq_o:
        mt_dbg();
            for (; p < pe; p++) {
                switch (*p) {
                    digit_cases CAT_FALLTHROUGH;
                    tchar_cases
                        state = mt_parameter_name;
                        goto mt_parameter_name;
                    alpha_cases
                        // printf("%c vs %c\n", (0x20 | (*p)), "oundary"[state - mt_boundary_eq_o]);
                        if (likely(
                            (0x20 | (*p)) /* tolower */
                            == "oundary"[state - mt_boundary_eq_o]
                        )) {
                            state++;
                            break;
                        }
                        state = mt_parameter_name;
                        goto mt_parameter_name;
                    case '=':
                        if (state != mt_boundary_eq_eq) {
                            p++;
                            state = mt_parameter_value;
                            goto mt_parameter_value;
                        }
                        goto mt_boundary_eq_eq;
                    default:
                        goto error;
                }
            }
            goto next;
        case mt_boundary_eq_eq:
mt_boundary_eq_eq:
        mt_dbg();
            if (*p == '=') {
                // printf("parser->multipart.boundary_length = %d\n", parser->multipart.boundary_length);
                if (parser->multipart.boundary_length != 0) {
                    goto error;
                }
                parser->multipart.boundary_length = 2;
                parser->multipart.multipart_boundary[0] = '-';
                parser->multipart.multipart_boundary[1] = '-';

                p++;
                state = mt_boundary_value;
                goto mt_boundary_value;
            } else {
                state = mt_parameter_name;
                goto mt_parameter_name;
            }
        case mt_boundary_value:
mt_boundary_value:
        mt_dbg();
            /*
            bcharsnospace = digit | alpha | "'" | "(" | ")" |
                        "+" | "_" | "," | "-" | "." |
                        "/" | ":" | "=" | "?";
            bchars = bcharsnospace | " ";
            boundary = bchars{0,69} bcharsnospace;

            ["] => mt_boundary_quoted_value
            bchars & token => mt_boundary_literal_value
            */
            switch (*p) {
                case '"':
                    p++;
                    state = mt_boundary_quoted_value;
                    goto mt_boundary_quoted_value;
                digit_cases CAT_FALLTHROUGH;
                alpha_cases CAT_FALLTHROUGH;
                case '\'': CAT_FALLTHROUGH;
                case '+': CAT_FALLTHROUGH;
                case '_': CAT_FALLTHROUGH;
                case '-': CAT_FALLTHROUGH;
                case '.':
                    state = mt_boundary_literal_value_acceptable;
                    goto mt_boundary_literal_value_acceptable;
                default:
                    goto error;
            }
            goto next;
        case mt_boundary_quoted_value:
mt_boundary_quoted_value:
        mt_dbg();
            /*
            RFC 2046 5.1.1

            Thus, a typical "multipart" Content-Type header
            field might look like this:

                Content-Type: multipart/mixed; boundary=gc0p4Jq0M2Yt08j34c0p

            But the following is not valid:

                Content-Type: multipart/mixed; boundary=gc0pJq0M:08jU534c0p

            (because of the colon) and must instead be represented as

                Content-Type: multipart/mixed; boundary="gc0pJq0M:08jU534c0p"

            [^ \t] => mt_boundary_quoted_value_unacceptable
            (bchars & qdtext (actually bchars)) & [^ \t] => self
            "\\" => mt_boundary_quoted_pair
            ["] => mt_parameter_ows1
            */
            for (; p < pe; p++) {
                switch (*p) {
                    case ' ': CAT_FALLTHROUGH;
                    case '\t':
                        state = mt_boundary_quoted_value_unacceptable;
                        goto mt_boundary_quoted_value_unacceptable;
                    digit_cases CAT_FALLTHROUGH;
                    alpha_cases CAT_FALLTHROUGH;
                    case '\'': CAT_FALLTHROUGH;
                    case '(': CAT_FALLTHROUGH;
                    case ')': CAT_FALLTHROUGH;
                    case '+': CAT_FALLTHROUGH;
                    case '_': CAT_FALLTHROUGH;
                    case ',': CAT_FALLTHROUGH;
                    case '-': CAT_FALLTHROUGH;
                    case '.': CAT_FALLTHROUGH;
                    case '/': CAT_FALLTHROUGH;
                    case ':': CAT_FALLTHROUGH;
                    case '=': CAT_FALLTHROUGH;
                    case '?':
                        /* bchars */
                        /* record the boundary char */
                        if (parser->multipart.boundary_length >= 72 /* strlen('--' boundary) */) {
                            goto error;
                        }
                        parser->multipart.multipart_boundary[parser->multipart.boundary_length++] = *p;
                        break;
                    case '\\':
                        p++;
                        state = mt_boundary_quoted_pair;
                        goto mt_boundary_quoted_pair;
                    case '"':
                        state = mt_parameter_ows1_acceptable;
                        goto mt_parameter_ows1_acceptable;
                }
            }
            goto next;
        case mt_boundary_quoted_value_unacceptable:
mt_boundary_quoted_value_unacceptable:
            for (; p < pe; p++) {
                switch (*p) {
                    digit_cases CAT_FALLTHROUGH;
                    alpha_cases CAT_FALLTHROUGH;
                    case '\'': CAT_FALLTHROUGH;
                    case '(': CAT_FALLTHROUGH;
                    case ')': CAT_FALLTHROUGH;
                    case '+': CAT_FALLTHROUGH;
                    case '_': CAT_FALLTHROUGH;
                    case ',': CAT_FALLTHROUGH;
                    case '-': CAT_FALLTHROUGH;
                    case '.': CAT_FALLTHROUGH;
                    case '/': CAT_FALLTHROUGH;
                    case ':': CAT_FALLTHROUGH;
                    case '=': CAT_FALLTHROUGH;
                    case '?':
                        state = mt_boundary_quoted_value;
                        goto mt_boundary_quoted_value;
                    case ' ': CAT_FALLTHROUGH;
                    case '\t':
                        /* bchars */
                        /* record the boundary char */
                        if (parser->multipart.boundary_length >= 72 /* strlen('--' boundary) */) {
                            goto error;
                        }
                        parser->multipart.multipart_boundary[parser->multipart.boundary_length++] = *p;
                        break;
                    case '\\':
                        p++;
                        state = mt_boundary_quoted_pair;
                        goto mt_boundary_quoted_pair;
                    case '"':
                        state = mt_parameter_ows1;
                        goto mt_parameter_ows1;
                }
            }
            goto next;

        case mt_boundary_quoted_pair:
mt_boundary_quoted_pair:
        mt_dbg();
            /*
            RFC 9110 5.6.4

            that process the value of a quoted-string MUST handle a quoted-pair
            as if it were replaced by the octet following the backslash.

            ([ \t] | vchar | obs_text) & bchars => mt_boundary_quoted_value
            */
            switch (*p) {
                case ' ': CAT_FALLTHROUGH;
                case '\t': CAT_FALLTHROUGH;
                digit_cases CAT_FALLTHROUGH;
                alpha_cases CAT_FALLTHROUGH;
                case '\'': CAT_FALLTHROUGH;
                case '(': CAT_FALLTHROUGH;
                case ')': CAT_FALLTHROUGH;
                case '+': CAT_FALLTHROUGH;
                case '_': CAT_FALLTHROUGH;
                case ',': CAT_FALLTHROUGH;
                case '-': CAT_FALLTHROUGH;
                case '.': CAT_FALLTHROUGH;
                case '/': CAT_FALLTHROUGH;
                case ':': CAT_FALLTHROUGH;
                case '=': CAT_FALLTHROUGH;
                case '?':
                    state = mt_boundary_quoted_value;
                    goto mt_boundary_quoted_value;
                default:
                    goto error;
            }
            // never here
        case mt_boundary_literal_value_acceptable:
mt_boundary_literal_value_acceptable:
        mt_dbg();
            /*
            bchars & token => self
            ows => mt_parameter_ows1
            [;] => mt_parameter_ows2
            */
            for (; p < pe; p++) {
                switch (*p) {
                    digit_cases CAT_FALLTHROUGH;
                    alpha_cases CAT_FALLTHROUGH;
                    case '\'': CAT_FALLTHROUGH;
                    case '+': CAT_FALLTHROUGH;
                    case '_': CAT_FALLTHROUGH;
                    case '-': CAT_FALLTHROUGH;
                    case '.':
                        /* bchars */
                        /* record the boundary char */
                        if (parser->multipart.boundary_length >= 72 /* strlen('--' boundary) */) {
                            goto error;
                        }
                        parser->multipart.multipart_boundary[parser->multipart.boundary_length++] = *p;
                        break;
                    case ' ': CAT_FALLTHROUGH;
                    case '\t':
                        state = mt_parameter_ows1;
                        goto mt_parameter_ows1;
                    case ';':
                        p++;
                        state = mt_parameter_ows2;
                        goto mt_parameter_ows2;
                    default:
                        goto error;
                }
            }
            goto next;
        case mt_error:
error:
        //printf("error state %d *p='%c'\n", state, *p);
            parser->header_value_parser_state = mt_error;
            return cat_false;
            break;
        default:
            CAT_NEVER_HERE("impossible state");
            break;
    }
next:

    switch(state) {
        case mt_parameter_ows1_acceptable: CAT_FALLTHROUGH;
        case mt_parameter_literal_value_acceptable: CAT_FALLTHROUGH;
        case mt_boundary_literal_value_acceptable:
            // acceptable = cat_true;
            return cat_true;
    }
    return cat_false;

#undef state
#undef tchar_cases
#undef digit_cases
#undef alpha_cases
#undef token_cases
#undef mt_dbg
}
