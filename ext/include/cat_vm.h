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

#if !defined(__cplusplus) && !defined(_MSC_VER)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
#include "zend_API.h"
#if !defined(__cplusplus) && !defined(_MSC_VER)
#pragma GCC diagnostic pop
#endif

/* memory */

#define CAT_ALLOC_NEVER_RETURNS_NULL

#define cat_malloc     emalloc
#define cat_calloc     ecalloc
#define cat_realloc    erealloc
#define cat_free(ptr)  do { \
    void *__ptr = ptr; \
    if (__ptr != NULL) { \
        efree(__ptr); \
    } \
} while (0)

/* thread safe */

#ifdef ZTS
#define CAT_THREAD_SAFE 1

#ifdef TSRMG_FAST
/* we can not use fast allocate for now,
 * see the way of php_reserve_tsrm_memory() */
// #define CAT_TSRMG_FAST 1
#endif

typedef struct cat_globals_info_s {
    ts_rsrc_id id;
#ifdef TSRMG_FAST
    size_t offset;
#endif
} cat_globals_info_t;

#define CAT_GLOBALS_INFO(name)                    name##_globals_info
#define CAT_GLOBALS_DECLARE(name)                 cat_globals_info_t CAT_GLOBALS_INFO(name);

#ifdef CAT_TSRMG_FAST
#if ZEND_ENABLE_STATIC_TSRMLS_CACHE
#define CAT_GLOBALS_BULK(name)                    TSRMG_FAST_BULK_STATIC(CAT_GLOBALS_INFO(name).offset, CAT_GLOBALS_TYPE(name) *)
#else
#define CAT_GLOBALS_BULK(name)                    TSRMG_FAST_BULK(CAT_GLOBALS_INFO(name).offset, CAT_GLOBALS_TYPE(name) *)
#endif
#define CAT_GLOBALS_GET(name, value)              (CAT_GLOBALS_BULK(name)->value)
#define CAT_GLOBALS_REGISTER(name, ctor, dtor)    ts_allocate_fast_id(&CAT_GLOBALS_INFO(name).id, &CAT_GLOBALS_INFO(name).offset, sizeof(CAT_GLOBALS_TYPE(name)), (ts_allocate_ctor) ctor, (ts_allocate_dtor) dtor);

#else

#if ZEND_ENABLE_STATIC_TSRMLS_CACHE
#define CAT_GLOBALS_BULK(name)                    TSRMG_BULK_STATIC(CAT_GLOBALS_INFO(name).id, CAT_GLOBALS_TYPE(name) *)
#else
#define CAT_GLOBALS_BULK(name)                    TSRMG_BULK(CAT_GLOBALS_INFO(name).id, CAT_GLOBALS_TYPE(name) *)
#endif
#define CAT_GLOBALS_GET(name, value)              (CAT_GLOBALS_BULK(name)->value)
#define CAT_GLOBALS_REGISTER(name, ctor, dtor)    ts_allocate_id(&CAT_GLOBALS_INFO(name).id, sizeof(CAT_GLOBALS_TYPE(name)), (ts_allocate_ctor) ctor, (ts_allocate_dtor) dtor);
#endif /* CAT_TSRMG_FAST */

#endif /* CAT_THREAD_SAFE */
