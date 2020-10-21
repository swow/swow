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

#define CAT_MEMORY_DEFAULT_ALIGNED_SIZE                sizeof(long)
#define CAT_MEMORY_ALIGNED_SIZE(size)                  CAT_MEMORY_ALIGNED_SIZE_EX(size, CAT_MEMORY_DEFAULT_ALIGNED_SIZE)
#define CAT_MEMORY_ALIGNED_SIZE_EX(size, alignment)    (((size) + ((alignment) - 1LL)) & ~((alignment) - 1LL))

#define CAT_ARRAY_SIZE(array) (sizeof(array) / sizeof(*array))

/* member offset + member size */
#define cat_offsize_of(type, member) (offsetof(type, member) + sizeof(((type *) 0)->member))
/* get object ptr by member ptr */
#define cat_container_of(ptr, type, member)  ((type *) ((char *) (ptr) - offsetof(type, member)))

/* return the eof point */
static cat_always_inline char* cat_memcpy(char *p, const char *data, size_t length)
{
    return (((char *) memcpy(p, data, length)) + length);
}

/* allocator */

#define cat_sys_malloc                 malloc
#define cat_sys_calloc                 calloc
#define cat_sys_realloc                realloc
#define cat_sys_free                   free
#define cat_sys_strdup                 strdup
#define cat_sys_strndup                strndup

#ifndef cat_malloc
#ifndef CAT_USE_DYNAMIC_ALLOCATOR
#define cat_malloc                     cat_sys_malloc
#define cat_calloc                     cat_sys_calloc
#define cat_realloc                    cat_sys_realloc
#define cat_free                       cat_sys_free
#define cat_strdup                     cat_sys_strdup
#define cat_strndup                    cat_sys_strndup
#else
#define cat_malloc(size)               cat_allocator.malloc(size)
#define cat_calloc(count, size)        cat_allocator.calloc(count, size)
#define cat_realloc(ptr, size)         cat_allocator.realloc(ptr, size)
#define cat_free(ptr)                  cat_allocator.free(ptr)
#define cat_strdup(string)             cat_allocator.strdup(string)
#define cat_strndup(string, length)    cat_allocator.strndup(string, length)
#endif
#endif

#ifdef CAT_USE_DYNAMIC_ALLOCATOR
typedef void *(*cat_malloc_function_t)(size_t size);
typedef void *(*cat_calloc_function_t)(size_t count, size_t size);
typedef void *(*cat_realloc_function_t)(void *ptr, size_t size);
typedef void (*cat_free_function_t)(void *ptr);
typedef char *(*cat_strdup_function_t)(const char *string);
typedef char *(*cat_strndup_function_t)(const char *string, size_t length);

typedef struct
{
    cat_malloc_function_t malloc;
    cat_calloc_function_t calloc;
    cat_realloc_function_t realloc;
    cat_free_function_t free;
    cat_strdup_function_t strdup;
    cat_strndup_function_t strndup;
} cat_allocator_t;

extern cat_allocator_t cat_allocator;
extern const cat_allocator_t cat_sys_allocator;

CAT_API cat_bool_t cat_register_allocator(const cat_allocator_t *allocator);
#endif

CAT_API void *cat_malloc_function(size_t size);
CAT_API void *cat_calloc_function(size_t count, size_t size);
CAT_API void *cat_realloc_function(void *ptr, size_t size);
CAT_API void cat_free_function(void *ptr);
CAT_API void cat_freep_function(void *ptr); /* free(ptr->ptr) */
CAT_API char *cat_strdup_function(const char *string);
CAT_API char *cat_strndup_function(const char *string, size_t length);

CAT_API size_t cat_getpagesize(void);
CAT_API void *cat_getpageof(const void *p);

CAT_API unsigned int cat_bit_count(uintmax_t num);
CAT_API int cat_bit_pos(uintmax_t num);

CAT_API uint64_t cat_hton64(uint64_t host);
CAT_API uint64_t cat_ntoh64(uint64_t network);
