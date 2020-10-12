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

#define CAT_STRLEN(str)    (sizeof(str) - 1)
#define CAT_STRS(str)      (str), (sizeof(str))
#define CAT_STRL(str)      (str), (sizeof(str) - 1)

#define CAT_TO_STR_NAKED(str)    #str
#define CAT_TO_STR(str)          CAT_TO_STR_NAKED(str)

CAT_API char *cat_vsprintf(const char *format, va_list args); CAT_FREE
CAT_API char *cat_sprintf(const char *format, ...)            CAT_FREE CAT_ATTRIBUTE_FORMAT(printf, 1, 2);

CAT_API char *cat_hexprint(const char *data, size_t length); CAT_FREE

CAT_API char *cat_srand(char *buffer, size_t count);  CAT_MAY_FREE
CAT_API char *cat_snrand(char *buffer, size_t count); CAT_MAY_FREE

typedef struct {
    size_t length;
    const char *data;
} cat_const_string_t;

#define cat_const_string(str) { sizeof(str) - 1, str }
