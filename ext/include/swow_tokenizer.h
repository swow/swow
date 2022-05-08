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

#ifndef SWOW_TOKENIZER_H
#define SWOW_TOKENIZER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_queue.h"

typedef struct php_token_s {
    cat_queue_t node;
    int type;
    cat_const_string_t text;
    uint32_t line;
    uint32_t offset;
} php_token_t;

typedef struct php_token_list_s {
    cat_queue_t queue;
    size_t count;
    zend_string *source;
} php_token_list_t;

SWOW_API php_token_list_t *php_tokenize(zend_string *source);

SWOW_API php_token_list_t *php_token_list_alloc(void);
SWOW_API void php_token_list_add(php_token_list_t *token_list, int token_type, const char *text, size_t text_length, int line);
SWOW_API void php_token_list_clear(php_token_list_t *token_list);
SWOW_API void php_token_list_free(php_token_list_t *token_list);

SWOW_API const char *php_token_get_name(const php_token_t *token);
SWOW_API const char *php_token_get_name_from_type(int type);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_TOKENIZER_H */
