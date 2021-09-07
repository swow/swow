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

#ifndef CAT_BUFFER_H
#define CAT_BUFFER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#define CAT_BUFFER_DEFAULT_SIZE 8192

typedef struct cat_buffer_s {
    /* public readonly */
    char *value;
    size_t size;
    size_t length;
} cat_buffer_t;

typedef char *(*cat_buffer_alloc_function_t)(size_t size);
typedef char *(*cat_buffer_realloc_function_t)(char *old_value, size_t old_length, size_t new_size);
typedef void (*cat_buffer_update_function_t)(char *value, size_t new_length);
typedef void (*cat_buffer_free_function_t)(char *value);

typedef struct cat_buffer_allocator_s {
    cat_buffer_alloc_function_t alloc_function;
    cat_buffer_realloc_function_t realloc_function;
    cat_buffer_update_function_t update_function;
    cat_buffer_free_function_t free_function;
} cat_buffer_allocator_t;

extern CAT_API cat_buffer_allocator_t cat_buffer_allocator;

CAT_API cat_bool_t cat_buffer_module_init(void);

CAT_API cat_bool_t cat_buffer_register_allocator(const cat_buffer_allocator_t *allocator);

CAT_API size_t cat_buffer_align_size(size_t size, size_t alignment);

CAT_API void cat_buffer_init(cat_buffer_t *buffer);
CAT_API cat_bool_t cat_buffer_alloc(cat_buffer_t *buffer, size_t size);
CAT_API cat_bool_t cat_buffer_create(cat_buffer_t *buffer, size_t size);
CAT_API cat_bool_t cat_buffer_realloc(cat_buffer_t *buffer, size_t new_size);
CAT_API cat_bool_t cat_buffer_extend(cat_buffer_t *buffer, size_t recommend_size);
CAT_API cat_bool_t cat_buffer_malloc_trim(cat_buffer_t *buffer);
CAT_API cat_bool_t cat_buffer_write(cat_buffer_t *buffer, size_t offset, const char *ptr, size_t length);
CAT_API cat_bool_t cat_buffer_append(cat_buffer_t *buffer, const char *ptr, size_t length);
CAT_API void cat_buffer_truncate(cat_buffer_t *buffer, size_t length);
CAT_API void cat_buffer_truncate_from(cat_buffer_t *buffer, size_t offset, size_t length);
CAT_API void cat_buffer_clear(cat_buffer_t *buffer);
CAT_API char *cat_buffer_fetch(cat_buffer_t *buffer);
CAT_API cat_bool_t cat_buffer_dup(cat_buffer_t *buffer, cat_buffer_t *new_buffer);
CAT_API void cat_buffer_close(cat_buffer_t *buffer);

CAT_API char *cat_buffer_get_value(const cat_buffer_t *buffer);
CAT_API size_t cat_buffer_get_size(const cat_buffer_t *buffer);
CAT_API size_t cat_buffer_get_length(const cat_buffer_t *buffer);

CAT_API cat_bool_t cat_buffer_make_pair(cat_buffer_t *rbuffer, size_t rsize, cat_buffer_t *wbuffer, size_t wsize);
CAT_API void cat_buffer_dump(cat_buffer_t *buffer);

#ifdef __cplusplus
}
#endif
#endif /* CAT_BUFFER_H */
