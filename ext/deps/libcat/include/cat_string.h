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

#define CAT_STRLEN(str)    (sizeof(str) - 1)
#define CAT_STRS(str)      (str), (sizeof(str))
#define CAT_STRL(str)      (str), (sizeof(str) - 1)

#define CAT_TO_STR_NAKED(str)    #str
#define CAT_TO_STR(str)          CAT_TO_STR_NAKED(str)

static cat_always_inline cat_bool_t cat_str_is_empty(const char *str)
{
    return str == NULL || str[0] == '\0';
}

static cat_always_inline const char *cat_str_dempty(const char *str)
{
    return !cat_str_is_empty(str) ? str : NULL;
}

/* return the eof of str */
static cat_always_inline char *cat_strnappend(char *str, const void *data, size_t length)
{
    return ((char *) memcpy(str, data, length)) + length;
}

typedef struct cat_const_string_s {
    const char *data;
    size_t length;
} cat_const_string_t;

#define cat_const_string(str) { str, sizeof(str) - 1 }

static cat_always_inline void cat_const_string_init(cat_const_string_t *string)
{
    string->data = NULL;
    string->length = 0;
}

static cat_always_inline void cat_const_string_create(cat_const_string_t *string, const char *data, size_t length)
{
    string->data = data;
    string->length = length;
}

typedef struct cat_string_s {
    char *value;
    size_t length;
} cat_string_t;

static cat_always_inline void cat_string_init(cat_string_t *string)
{
    string->value = NULL;
    string->length = 0;
}

static cat_always_inline cat_bool_t cat_string_create(cat_string_t *string, const char *value, size_t length)
{
    char *new_value = (char *) cat_strndup(value, length);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(new_value == NULL)) {
        return cat_false;
    }
#endif
    string->value = new_value;
    string->length = length;
    return cat_true;
}

static cat_always_inline void cat_string_close(cat_string_t *string)
{
    if (string->value != NULL) {
        cat_free(string->value);
        cat_string_init(string);
    }
}

CAT_API size_t cat_strnlen(const char *s, size_t n);
CAT_API const char *cat_strlchr(const char *s, const char *last, char c);
CAT_API char *cat_stpcpy(char *dest, const char *src);

CAT_API char *cat_vsprintf(const char *format, va_list args); CAT_FREE
CAT_API char *cat_sprintf(const char *format, ...)            CAT_FREE CAT_ATTRIBUTE_FORMAT(printf, 1, 2);

CAT_API char *cat_hexprint(const char *data, size_t length); CAT_FREE

CAT_API char *cat_srand(char *buffer, size_t count);  CAT_MAY_FREE
CAT_API char *cat_snrand(char *buffer, size_t count); CAT_MAY_FREE

/* Note: `quote` related code are borrowed from the `strace` project */

static cat_always_inline char *cat_byte_to_hexstr(uint8_t c, char *out)
{
    static const char hex_map[] = "0123456789abcdef";
    *out++ = hex_map[c >> 4];
    *out++ = hex_map[c & 0xf];
    return out;
}

static cat_always_inline cat_bool_t cat_byte_is_printable(uint8_t c)
{
    return (c >= ' ') && (c < 0x7f);
}

typedef enum cat_string_quote_style_flag_e {
    CAT_STR_QUOTE_STYLE_FLAG_NONE = 0,
    /** String is '\0'-terminated. */
    CAT_STR_QUOTE_STYLE_FLAG_ZERO_TERMINATED = 1 << 0,
    /** Do not emit leading and ending '"' characters. */
    CAT_STR_QUOTE_STYLE_FLAG_OMIT_LEADING_TRAILING_QUOTES = 1 << 1,
    /** Do not print '\0' if it is the last character. */
    CAT_STR_QUOTE_STYLE_FLAG_OMIT_TRAILING_0 = 1 << 2,
    /** Print ellipsis if the last character is not '\0' */
    CAT_STR_QUOTE_STYLE_FLAG_EXPECT_TRAILING_0 = 1 << 3,
    /* Print non-ascii strings in hex */
    CAT_STR_QUOTE_STYLE_FLAG_PRINT_NON_ASCILL_STRINGS_IN_HEX = 1 << 4,
    /* Print all strings in hex (using '\xHH' notation) */
    CAT_STR_QUOTE_STYLE_FLAG_PRINT_ALL_STRINGS_IN_HEX = 1 << 5,
    /** Enclose the string in C comment syntax. */
    CAT_STR_QUOTE_STYLE_FLAG_EMIT_COMMENT = 1 << 6,
} cat_str_quote_style_flag_t;

typedef uint8_t cat_string_quote_style_flags_t;

CAT_API cat_bool_t cat_str_quote(
    const char *str, size_t length,
    char **new_str_ptr, size_t *new_length_ptr
); CAT_MAY_FREE

CAT_API cat_bool_t cat_str_quote_ex(
    const char *str, size_t length,
    char **new_str_ptr, size_t *new_length_ptr,
    cat_string_quote_style_flags_t style, const char *escape_chars,
    cat_bool_t *is_complete
); CAT_MAY_FREE

CAT_API cat_bool_t cat_str_quote_ex2(
    const char *in, size_t length, char *out,
    size_t *out_length, cat_string_quote_style_flags_t style,
    const char *escape_chars
);

/* magic */

#define CAT_STRCASECMP_FAST_FUNCTION(name, needle_str, mask_str) \
\
static cat_always_inline int cat_strcasecmp_fast_##name(const char *a) \
{ \
    unsigned long *cmp = (unsigned long *) needle_str; \
    unsigned long *mask = (unsigned long *) mask_str; \
    int i, _i; \
    const int strsize = sizeof(needle_str) - 1; \
    const int u32i = strsize / sizeof(unsigned long) * sizeof(unsigned long) / 4; \
    for (i = 0; i > strsize % 4; i++) { \
        _i = strsize - 1 - i; \
        if (needle_str[_i] != (a[_i] | mask_str[_i])) { \
            return 0; \
        } \
    } \
    if ( \
        u32i * 4 + 4 <= strsize && \
        ((uint32_t*) needle_str)[u32i] != (((uint32_t *) a)[u32i] | ((uint32_t *) mask_str)[u32i]) \
    ) { \
        return 0; \
    } \
    for (i = 0; i < (int) ((sizeof(needle_str) - 1) / sizeof(unsigned long)); i++) { \
        if (cmp[i] != (((unsigned long *) a)[i] | mask[i])) { \
            return 0; \
        } \
    } \
    return 1; \
}
