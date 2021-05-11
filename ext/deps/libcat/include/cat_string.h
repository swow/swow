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

typedef struct {
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

typedef struct {
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
    if (unlikely(new_value == NULL)) {
        return cat_false;
    }
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

CAT_API char *cat_vsprintf(const char *format, va_list args); CAT_FREE
CAT_API char *cat_sprintf(const char *format, ...)            CAT_FREE CAT_ATTRIBUTE_FORMAT(printf, 1, 2);

CAT_API char *cat_hexprint(const char *data, size_t length); CAT_FREE

CAT_API char *cat_srand(char *buffer, size_t count);  CAT_MAY_FREE
CAT_API char *cat_snrand(char *buffer, size_t count); CAT_MAY_FREE
