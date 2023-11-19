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

#define CAT_MEMORY_DEFAULT_ALIGNED_SIZE                sizeof(void *)
#define CAT_MEMORY_ALIGNED_SIZE(size)                  CAT_MEMORY_ALIGNED_SIZE_EX(size, CAT_MEMORY_DEFAULT_ALIGNED_SIZE)
#define CAT_MEMORY_ALIGNED_SIZE_EX(size, alignment)    (((size) + ((alignment) - 1LL)) & ~((alignment) - 1LL))

#define CAT_ARRAY_SIZE(array) (sizeof(array) / sizeof(*array))

/* member offset + member size */
#define cat_offsize_of(type, member) (offsetof(type, member) + sizeof(((type *) 0)->member))
/* get object ptr by member ptr */
#define cat_container_of(ptr, type, member)  ((type *) ((char *) (ptr) - offsetof(type, member)))

/* allocator */

#ifndef cat_sys_malloc_recoverable
# define cat_sys_malloc_recoverable  malloc
# define cat_sys_calloc_recoverable  calloc
# define cat_sys_realloc_recoverable realloc
#endif

#ifndef cat_sys_malloc
# ifndef CAT_SYS_ALLOC_ABORT_ON_OOM
#  define CAT_SYS_ALLOC_ABORT_ON_OOM 1
# endif
# if CAT_SYS_ALLOC_ABORT_ON_OOM
#  define CAT_SYS_ALLOC_HANDLE_ERRORS 0
# else
#  define CAT_SYS_ALLOC_HANDLE_ERRORS 1
# endif
# if CAT_SYS_ALLOC_ABORT_ON_OOM
#  define cat_sys_malloc  cat_sys_malloc_unrecoverable
#  define cat_sys_calloc  cat_sys_calloc_unrecoverable
#  define cat_sys_realloc cat_sys_realloc_unrecoverable
# else
#  define cat_sys_malloc  cat_sys_malloc_recoverable
#  define cat_sys_calloc  cat_sys_calloc_recoverable
#  define cat_sys_realloc cat_sys_realloc_recoverable
# endif
#  define cat_sys_free    free
#endif

#ifndef cat_malloc
# ifndef CAT_ALLOC_ABORT_ON_OOM
#  define CAT_ALLOC_ABORT_ON_OOM 1
# endif
# ifndef CAT_ALLOC_HANDLE_ERRORS
#  if CAT_ALLOC_ABORT_ON_OOM
#   define CAT_ALLOC_HANDLE_ERRORS 0
#  else
#   define CAT_ALLOC_HANDLE_ERRORS 1
#  endif
# endif
# ifndef CAT_USE_DYNAMIC_ALLOCATOR
#  if CAT_ALLOC_ABORT_ON_OOM
#   define cat_malloc  cat_sys_malloc_unrecoverable
#   define cat_calloc  cat_sys_calloc_unrecoverable
#   define cat_realloc cat_sys_realloc_unrecoverable
#   define cat_free    cat_sys_free
#  else
#   define cat_malloc  cat_sys_malloc_recoverable
#   define cat_calloc  cat_sys_calloc_recoverable
#   define cat_realloc cat_sys_realloc_recoverable
#   define cat_free    cat_sys_free
#  endif
# else /* CAT_USE_DYNAMIC_ALLOCATOR */
#  if CAT_ALLOC_ABORT_ON_OOM
#   define cat_malloc  cat_allocator.malloc
#   define cat_calloc  cat_allocator.calloc
#   define cat_realloc cat_allocator.realloc
#  else
#   define cat_malloc  cat_allocator_malloc_unrecoverable
#   define cat_calloc  cat_allocator_calloc_unrecoverable
#   define cat_realloc cat_allocator_realloc_unrecoverable
#  endif
#   define cat_free    cat_allocator.free
# endif
#endif

#ifndef CAT_SYS_ALLOC_HANDLE_ERRORS
# error "CAT_SYS_ALLOC_HANDLE_ERRORS should be defined to indicates how the program should handle OOM"
#endif
#ifndef CAT_ALLOC_HANDLE_ERRORS
# error "CAT_ALLOC_HANDLE_ERRORS should be defined to indicates how the program should handle OOM"
#endif

#ifndef cat_out_of_memory
CAT_API CAT_COLD CAT_NORETURN void cat_out_of_memory(void);
#endif

static cat_always_inline void *cat_sys_malloc_unrecoverable(size_t size)
{
    void *ptr = cat_sys_malloc_recoverable(size);
    if (unlikely(ptr == NULL && size != 0)) {
        cat_out_of_memory();
    }
    return ptr;
}

static cat_always_inline void *cat_sys_calloc_unrecoverable(size_t count, size_t size)
{
    void *ptr = cat_sys_calloc_recoverable(count, size);
    if (unlikely(ptr == NULL && count != 0 && size != 0)) {
        cat_out_of_memory();
    }
    return ptr;
}

static cat_always_inline void *cat_sys_realloc_unrecoverable(void *ptr, size_t size)
{
    ptr = cat_sys_realloc_recoverable(ptr, size);
    if (unlikely(ptr == NULL && size != 0)) {
        cat_out_of_memory();
    }
    return ptr;
}

static cat_always_inline void *cat_malloc_unrecoverable(size_t size)
{
    void *ptr = cat_malloc(size);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(ptr == NULL && size != 0)) {
        cat_out_of_memory();
    }
#endif
    return ptr;
}

static cat_always_inline void *cat_calloc_unrecoverable(size_t count, size_t size)
{
    void *ptr = cat_calloc(count, size);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(ptr == NULL && count != 0 && size != 0)) {
        cat_out_of_memory();
    }
#endif
    return ptr;
}

#ifdef CAT_USE_DYNAMIC_ALLOCATOR
typedef void *(*cat_malloc_function_t)(size_t size);
typedef void *(*cat_calloc_function_t)(size_t count, size_t size);
typedef void *(*cat_realloc_function_t)(void *ptr, size_t size);
typedef void (*cat_free_function_t)(void *ptr);

typedef struct cat_allocator_s {
    cat_malloc_function_t malloc;
    cat_calloc_function_t calloc;
    cat_realloc_function_t realloc;
    cat_free_function_t free;
} cat_allocator_t;

extern CAT_API cat_allocator_t cat_allocator;

static cat_always_inline void *cat_allocator_malloc_unrecoverable(size_t size)
{
    void *ptr = cat_allocator.malloc(size);
    if (unlikely(ptr == NULL && size != 0)) {
        cat_out_of_memory();
    }
    return ptr;
}

static cat_always_inline void *cat_allocator_calloc_unrecoverable(size_t count, size_t size)
{
    void *ptr = cat_allocator.calloc(count, size);
    if (unlikely(ptr == NULL && count != 0 && size != 0)) {
        cat_out_of_memory();
    }
    return ptr;
}

static cat_always_inline void *cat_allocator_realloc_unrecoverable(void *ptr, size_t size)
{
    ptr = cat_allocator.realloc(ptr, size);
    if (unlikely(ptr == NULL && size != 0)) {
        cat_out_of_memory();
    }
    return ptr;
}

CAT_API cat_bool_t cat_register_allocator(const cat_allocator_t *allocator);
#endif /* CAT_USE_DYNAMIC_ALLOCATOR */

static cat_always_inline char *cat_memdup(const void *p, size_t size)
{
    void *np = cat_malloc(size);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(np == NULL)) {
        return NULL;
    }
#endif
    return (char *) memcpy(np, p, size);
}

CAT_API char *cat_sys_strdup(const char *str);
CAT_API char *cat_sys_strndup(const char *str, size_t length);

CAT_API char *cat_strdup(const char *str);
CAT_API char *cat_strndup(const char *str, size_t length);

CAT_API void *cat_malloc_function(size_t size);
CAT_API void *cat_calloc_function(size_t count, size_t size);
CAT_API void *cat_realloc_function(void *ptr, size_t size);
CAT_API void cat_free_function(void *ptr);
CAT_API void cat_freep_function(void *ptr); /* free(ptr->ptr) */

extern CAT_API size_t cat_pagesize;

CAT_API size_t cat_getpagesize_slow(void);

static cat_always_inline size_t cat_getpagesize(void)
{
    if (unlikely(cat_pagesize == 0)) {
        cat_pagesize = cat_getpagesize_slow();
    }
    return cat_pagesize;
}

CAT_API void *cat_getpageof(const void *ptr);
CAT_API void *cat_getpageafter(const void *ptr);

CAT_API unsigned int cat_bit_count(uintmax_t num);
CAT_API int cat_bit_pos(uintmax_t num);

CAT_API uint64_t cat_hton64(uint64_t host);
CAT_API uint64_t cat_ntoh64(uint64_t network);
