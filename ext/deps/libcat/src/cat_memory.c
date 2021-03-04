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
cat_allocator_t cat_allocator;
const cat_allocator_t cat_sys_allocator = { malloc, calloc, realloc, free, strdup, strndup };

CAT_API cat_bool_t cat_register_allocator(const cat_allocator_t *allocator)
{
    if (
        allocator->malloc == NULL ||
        allocator->calloc == NULL ||
        allocator->realloc == NULL ||
        allocator->free == NULL ||
        allocator->strdup == NULL ||
        allocator->strndup == NULL
    ) {
        cat_update_last_error(CAT_EINVAL, "Allocator must be filled");
    }
    cat_allocator = *allocator;
    return cat_true;
}
#endif

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
    if (unlikely(ptr == NULL)) {
        return;
    }
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

CAT_API char *cat_strdup_function(const char *string)
{
    return cat_strdup(string);
}

CAT_API char *cat_strndup_function(const char *string, size_t length)
{
    return cat_strndup(string ,length);
}

CAT_API int cat_getpagesize(void)
{
    static int pagesize = 0;

    if (unlikely(pagesize == 0)) {
#ifdef CAT_OS_WIN
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        pagesize = system_info.dwPageSize;
#else
#if __APPLE__
        pagesize = sysconf(_SC_PAGE_SIZE);
#else
        pagesize = getpagesize();
#endif
#endif
        if (pagesize <= 0) {
            /* anyway, We have to return a normal result */
            pagesize = 4096;
        }
    }

    return (int) pagesize;
}

CAT_API void *cat_getpageof(const void *p)
{
    return (void *) (((uintptr_t) p) & ~(cat_getpagesize() - 1));
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
