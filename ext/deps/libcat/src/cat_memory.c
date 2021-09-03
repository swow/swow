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

#ifdef CAT_USE_DYNAMIC_ALLOCATOR
CAT_API cat_allocator_t cat_allocator;
static const cat_allocator_t cat_sys_allocator = { cat_sys_malloc, cat_sys_calloc, cat_sys_realloc, cat_sys_free };

CAT_API cat_bool_t cat_register_allocator(const cat_allocator_t *allocator)
{
    if (allocator != NULL) {
        if (
            allocator->malloc == NULL ||
            allocator->calloc == NULL ||
            allocator->realloc == NULL ||
            allocator->free == NULL
        ) {
            cat_update_last_error(CAT_EINVAL, "Allocator must be filled");
        }
        cat_allocator = *allocator;
    } else {
        cat_allocator = cat_sys_allocator;
    }
    return cat_true;
}
#endif

CAT_API char *cat_sys_strdup(const char *string)
{
    size_t size = strlen(string) + 1;
    char *ptr = (char *) malloc(size);
#ifndef CAT_SYS_ALLOC_NEVER_RETURNS_NULL
    if (unlikely(ptr == NULL)) {
        return NULL;
    }
#endif
    return memcpy(ptr, string, size);
}

CAT_API char *cat_sys_strndup(const char *string, size_t length)
{
    char *ptr;
    length = cat_strnlen(string, length);
    ptr = (char *) malloc(length + 1);
#ifndef CAT_SYS_ALLOC_NEVER_RETURNS_NULL
    if (unlikely(ptr == NULL)) {
        return NULL;
    }
#endif
    memcpy(ptr, string, length);
    ptr[length] = '\0';
    return ptr;
}

CAT_API char *cat_strdup(const char *string)
{
    size_t size = strlen(string) + 1;
    char *ptr = (char *) cat_malloc(size);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(ptr == NULL)) {
        return NULL;
    }
#endif
    return memcpy(ptr, string, size);
}

CAT_API char *cat_strndup(const char *string, size_t length)
{
    char *ptr;
    length = cat_strnlen(string, length);
    ptr = (char *) cat_malloc(length + 1);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(ptr == NULL)) {
        return NULL;
    }
#endif
    memcpy(ptr, string, length);
    ptr[length] = '\0';
    return ptr;
}

CAT_API void *cat_malloc_function(size_t size)
{
    return cat_malloc(size);
}

CAT_API void *cat_calloc_function(size_t count, size_t size)
{
    return cat_calloc(count, size);
}

CAT_API void *cat_realloc_function(void *ptr, size_t size)
{
    return cat_realloc(ptr, size);
}

CAT_API void cat_free_function(void *ptr)
{
    cat_free(ptr);
}

CAT_API void cat_freep_function(void *ptr)
{
    if (unlikely(ptr == NULL)) {
        return;
    }
    ptr = *((void **) ptr);
    cat_free_function(ptr);
}

CAT_API size_t cat_pagesize = 0;

CAT_API size_t cat_getpagesize_slow(void)
{
    int pagesize;

#ifdef CAT_OS_WIN
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    pagesize = system_info.dwPageSize;
#elif __APPLE__
    pagesize = sysconf(_SC_PAGE_SIZE);
#else
    pagesize = getpagesize();
#endif
    if (pagesize <= 0) {
        /* anyway, We have to return a normal result */
        pagesize = 4096;
    }

    return pagesize;
}

CAT_API void *cat_getpageof(const void *ptr)
{
    return (void *) (((uintptr_t) ptr) & ~(cat_getpagesize() - 1));
}

CAT_API void *cat_getpageafter(const void *ptr)
{
    void *page = cat_getpageof(ptr);
    /* page cannot exceed the virtual memory range */
    if (((uintptr_t) page) < ((uintptr_t) ptr)) {
        page = ((char *) page) + cat_getpagesize();
    }
    return page;
}

CAT_API unsigned int cat_bit_count(uintmax_t num)
{
    uint8_t count = 0;

    while (num != 0) {
        num = num & (num - 1);
        count++;
    }

    return count;
}

CAT_API int cat_bit_pos(uintmax_t num)
{
    int pos = -1;

    while (num != 0) {
        num = num >> 1;
        pos++;
    }

    return pos;
}

CAT_API uint64_t cat_hton64(uint64_t host)
{
    uint64_t ret = 0;
    uint32_t high, low;

    low = host & 0xFFFFFFFF;
    high = (host >> 32) & 0xFFFFFFFF;
    low = htonl(low);
    high = htonl(high);

    ret = low;
    ret <<= 32;
    ret |= high;

    return ret;
}

CAT_API uint64_t cat_ntoh64(uint64_t network)
{
    uint64_t ret = 0;
    uint32_t high, low;

    low = network & 0xFFFFFFFF;
    high = network >> 32;
    low = ntohl(low);
    high = ntohl(high);

    ret = low;
    ret <<= 32;
    ret |= high;

    return ret;
}
