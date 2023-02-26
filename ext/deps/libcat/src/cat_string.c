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

#include "cat.h"

#ifdef CAT_IDE_HELPER
#include "cat_string.h"
#endif

CAT_API size_t cat_strnlen(const char *s, size_t n)
{
    const char *p = memchr(s, 0, n);
    return p ? (size_t) (p - s) : n;
}

/* Searches a string based on the pointer to the beginning and end of the string for a given character.
 * Returns a pointer to the first match of that character. */
CAT_API const char *cat_strlchr(const char *s, const char *last, char c)
{
    while ((unsigned char *) s < (unsigned char *) last) {
        if (*s == c) {
            return s;
        }
        s++;
    }

    return NULL;
}

/* Copy a string returning a pointer to its end */
CAT_API char *cat_stpcpy(char *dest, const char *src)
{
    for (; (*dest=*src); src++, dest++);
    return dest;
}

CAT_API char *cat_vslprintf(const char *format, size_t *length, va_list args)
{
    va_list _args;
    size_t size;
    char *string;

    va_copy(_args, args);
    size = vsnprintf(NULL, 0, format, _args) + 1;
    va_end(_args);
    string = (char *) cat_malloc(size);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(string == NULL)) {
        /* no need to update error message, we even have no mem to sprintf,
           so we may also have no mem to update error message */
        cat_update_last_error(cat_translate_sys_error(cat_sys_errno), NULL);
        if (length != NULL) {
            *length = 0;
        }
        return NULL;
    }
#endif
    if (unlikely(vsnprintf(string, size, format, args) < 0)) {
        cat_free(string);
        return NULL;
    }
    string[size - 1] = '\0';
    if (length != NULL) {
        *length = size - 1;
    }

    return string;
}

CAT_API char *cat_vsprintf(const char *format, va_list args)
{
    return cat_vslprintf(format, NULL, args);
}

CAT_API char *cat_slprintf(const char *format, size_t *length, ...)
{
    va_list args;
    char *string;

    va_start(args, length);
    string = cat_vslprintf(format, length, args);
    va_end(args);

    return string;
}

CAT_API char *cat_sprintf(const char *format, ...)
{
    va_list args;
    char *string;

    va_start(args, format);
    string = cat_vslprintf(format, NULL, args);
    va_end(args);

    return string;
}

CAT_API cat_bool_t cat_str_is_print(const char *string, size_t length)
{
    size_t i;
    for (i = 0; i < length; i++) {
        if (!isprint(string[i])) {
            return cat_false;
        }
    }
    return cat_true;
}

CAT_API char *cat_hex_dump(const char *data, size_t length)
{
    const size_t width = 5;
    char *escaped;
    size_t size = width * length;
    size_t index, escaped_index;
    int n;

    escaped = (char *) cat_malloc(size + 1);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(escaped == NULL)) {
        cat_update_last_error_of_syscall("Malloc for escaped string failed");
        return NULL;
    }
#endif
    for (index = 0, escaped_index = 0; index < length; index++, escaped_index += width) {
        n = snprintf(escaped + escaped_index, size - escaped_index, "0x%02X ", data[index] & 0xff);
        if (unlikely(n < 0 || (size_t) n != width)) {
            cat_update_last_error_of_syscall("Snprintf() for escaped string failed");
            cat_free(escaped);
            return NULL;
        }
    }
    escaped[size] = '\0';

    return escaped;
}

CAT_API char *cat_srand(char *buffer, size_t count)
{
    buffer = cat_snrand(buffer, count);

    if (likely(buffer != NULL)) {
        buffer[count - 1] = '\0';
    }

    return buffer;
}

CAT_API char *cat_snrand(char *buffer, size_t count)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t n = 0;

    if (buffer == NULL) {
        buffer = (char *) cat_malloc(count);
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(buffer == NULL)) {
            cat_update_last_error_of_syscall("Malloc for random string failed");
            return NULL;
        }
#endif
    }

    for (; n < count; n++) {
        int key = rand() % ((int) (sizeof(charset) - 1));
        buffer[n] = charset[key];
    }

    return buffer;
}

CAT_API cat_bool_t cat_str_list_contains_ci(const char *haystack, const char *needle, size_t needle_length)
{
    const char *s = NULL, *e = haystack;

    while (1) {
        if (*e == ' ' || *e == ',' || *e == '\0') {
            if (s) {
                if ((size_t) (e - s) == needle_length && cat_strncasecmp(s, needle, needle_length) == 0) {
                    return cat_true;
                }
                s = NULL;
            }
        } else {
            if (!s) {
                s = e;
            }
        }
        if (*e == '\0') {
            break;
        }
        e++;
    }

    return cat_false;
}

CAT_API size_t cat_str_quote_size(size_t length, cat_str_quote_style_flags_t style)
{
    return
        (4 * length) +
        (style & CAT_STR_QUOTE_STYLE_FLAG_OMIT_LEADING_TRAILING_QUOTES ? 1 : 3) +
        (style & CAT_STR_QUOTE_STYLE_FLAG_ELLIPSIS ? 3 : 0);
}

/*
 * Quote string `in' of `length'
 * Write up to (3 + `length' * 4) bytes to `out' buffer.
 *
 * `escape_chars' specifies characters (in addition to characters with
 * codes 0..31, 127..255, single and double quotes) that should be escaped.
 *
 * If QUOTE_0_TERMINATED `style' flag is set,
 * treat `in' as a NUL-terminated string,
 * checking up to (`length' + 1) bytes of `in'.
 *
 * If QUOTE_OMIT_LEADING_TRAILING_QUOTES `style' flag is set,
 * do not add leading and trailing quoting symbols.
 *
 * Return true if we printed entire ASCIZ string (didn't truncate it)
 */
CAT_API cat_bool_t cat_str_quote_ex2(
    const char *in, size_t length, char *out, size_t *out_length,
    cat_str_quote_style_flags_t style, const char *escape_chars
)
{
    const unsigned char *p = (const unsigned char *) in;
    char *s = out;
    size_t i;
    unsigned char c;
    cat_bool_t use_hex;
    cat_bool_t is_zero_terminated = style & CAT_STR_QUOTE_STYLE_FLAG_ZERO_TERMINATED;
    cat_bool_t ret = cat_true;

    use_hex = 0;
    if (style & CAT_STR_QUOTE_STYLE_FLAG_PRINT_ALL_STRINGS_IN_HEX) {
        use_hex = 1;
    } else if (style & CAT_STR_QUOTE_STYLE_FLAG_PRINT_NON_ASCII_STRINGS_IN_HEX) {
        /* Check for presence of symbol which require
           to hex-quote the whole string. */
        for (i = 0; i < length; ++i) {
            c = p[i];
            /* Check for NUL-terminated string. */
            if (is_zero_terminated && c == '\0') {
                break;
            }

            /* Force hex unless c is printable or whitespace */
            if (c > 0x7e) {
                use_hex = 1;
                break;
            }
            /* In ASCII isspace is only these chars: "\t\n\v\f\r".
             * They happen to have ASCII codes 9,10,11,12,13.
             */
            if (c < ' ' && (unsigned) (c - 9) >= 5) {
                use_hex = 1;
                break;
            }
        }
    }

    if (style & CAT_STR_QUOTE_STYLE_FLAG_EMIT_COMMENT) {
        s = cat_stpcpy(s, " /* ");
    }
    if (!(style & CAT_STR_QUOTE_STYLE_FLAG_OMIT_LEADING_TRAILING_QUOTES)) {
        *s++ = '\"';
    }

    if (use_hex) {
        /* Hex-quote the whole string. */
        for (i = 0; i < length; ++i) {
            c = p[i];
            /* Check for NUL-terminated string. */
            if (is_zero_terminated && c == '\0')
                goto _asciz_ended;
            *s++ = '\\';
            *s++ = 'x';
            s = cat_byte_to_hexstr((uint8_t) c, s);
        }
        goto _string_ended;
    }

    for (i = 0; i < length; ++i) {
        c = p[i];
        /* Check for NUL-terminated string. */
        if (is_zero_terminated && c == '\0') {
            goto _asciz_ended;
        }
        if ((i == (length - 1)) &&
            (style & CAT_STR_QUOTE_STYLE_FLAG_OMIT_TRAILING_0) && (c == '\0')) {
            goto _asciz_ended;
        }
        switch (c) {
            case '\"':
            case '\\':
                *s++ = '\\';
                *s++ = c;
                break;
            case '\f':
                *s++ = '\\';
                *s++ = 'f';
                break;
            case '\n':
                *s++ = '\\';
                *s++ = 'n';
                break;
            case '\r':
                *s++ = '\\';
                *s++ = 'r';
                break;
            case '\t':
                *s++ = '\\';
                *s++ = 't';
                break;
            case '\v':
                *s++ = '\\';
                *s++ = 'v';
                break;
            default: {
                cat_bool_t printable = cat_byte_is_printable(c);

                if (printable && escape_chars != NULL) {
                    printable = !strchr(escape_chars, c);
                }

                if (printable) {
                    *s++ = c;
                }
                else {
                    /* Print \octal */
                    *s++ = '\\';
                    if (i + 1 < length && p[i + 1] >= '0' && p[i + 1] <= '7') {
                        /* Print \ooo */
                        *s++ = '0' + (c >> 6);
                        *s++ = '0' + ((c >> 3) & 0x7);
                    }
                    else {
                        /* Print \[[o]o]o */
                        if ((c >> 3) != 0) {
                            if ((c >> 6) != 0) {
                                *s++ = '0' + (c >> 6);
                            }
                            *s++ = '0' + ((c >> 3) & 0x7);
                        }
                    }
                    *s++ = '0' + (c & 0x7);
                }
            }
        }
    }

_string_ended:
    if (!(style & CAT_STR_QUOTE_STYLE_FLAG_OMIT_LEADING_TRAILING_QUOTES)) {
        if (style & CAT_STR_QUOTE_STYLE_FLAG_ELLIPSIS) {
            s = cat_stpcpy(s, "...");
        }
        *s++ = '\"';
    }
    if (style & CAT_STR_QUOTE_STYLE_FLAG_EMIT_COMMENT) {
        s = cat_stpcpy(s, " */");
    }

    if (!(style & CAT_STR_QUOTE_STYLE_FLAG_ZERO_TERMINATED) || p[i] != '\0') {
        ret = cat_false;
    }
    /* else we didn't see NUL yet (otherwise we'd jump to 'asciz_ended') but next char is NUL.
     * Return true if we printed entire ASCIZ string (didn't truncate it) */

    goto _done;

_asciz_ended:
    if (!(style & CAT_STR_QUOTE_STYLE_FLAG_OMIT_LEADING_TRAILING_QUOTES)) {
        if (style & CAT_STR_QUOTE_STYLE_FLAG_ELLIPSIS) {
            s = cat_stpcpy(s, "...");
        }
        *s++ = '\"';
    }
    if (style & CAT_STR_QUOTE_STYLE_FLAG_EMIT_COMMENT) {
        s = cat_stpcpy(s, " */");
    }
    /* Return true: we printed entire ASCIZ string (didn't truncate it)
     * ... */

_done:
    *s = '\0';
    if (out_length != NULL) {
        *out_length = s - out;
    }
    return ret;
}

CAT_API cat_bool_t cat_str_quote_ex(
    const char *str, size_t length,
    char **new_str_ptr, size_t *new_length_ptr,
    cat_str_quote_style_flags_t style, const char *escape_chars,
    cat_bool_t *is_complete
)
{
    size_t new_size = cat_str_quote_size(length, style);
    char *new_str = cat_malloc(new_size);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(new_str == NULL)) {
        cat_update_last_error_of_syscall("Malloc for quote str failed");
        *new_str_ptr = NULL;
        return cat_false;
    }
#endif
    cat_bool_t ret = cat_str_quote_ex2(str, length, new_str, new_length_ptr, style, escape_chars);
    *new_str_ptr = new_str;
    if (is_complete != NULL) {
        *is_complete = ret;
    }
    return cat_true;
}

CAT_API cat_bool_t cat_str_quote(const char *str, size_t length, char **new_str_ptr, size_t *new_length_ptr)
{
    return cat_str_quote_ex(str, length, new_str_ptr, new_length_ptr, CAT_STR_QUOTE_STYLE_FLAG_NONE, NULL, NULL);
}
