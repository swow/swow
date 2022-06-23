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

#define CAT_GLOBALS_STRUCT(name)       name##_globals_s
#define CAT_GLOBALS_TYPE(name)         name##_globals_t
#define CAT_GLOBALS(name)              name##_globals

#define CAT_GLOBALS_STRUCT_BEGIN(name) typedef struct CAT_GLOBALS_STRUCT(name)
#define CAT_GLOBALS_STRUCT_END(name)   CAT_GLOBALS_TYPE(name)

#define CAT_GLOBALS_BZERO(name)        memset(CAT_GLOBALS_BULK(name), 0, sizeof(CAT_GLOBALS_TYPE(name)))

#define CAT_GLOBALS_GET(name, value) (CAT_GLOBALS_BULK(name)->value)

#ifndef CAT_THREAD_SAFE /* use our own thread-safe implementation */

#if defined(CAT_USE_THREAD_LOCAL) && defined(CAT_USE_THREAD_KEY)
# error "Unable to use THREAD_LOCAL and THREAD_KEY at the same time"
#endif

#if defined(CAT_USE_THREAD_LOCAL) || defined(CAT_USE_THREAD_KEY)
# define CAT_THREAD_SAFE 1
#endif

#if !defined(CAT_THREAD_SAFE) || defined(CAT_USE_THREAD_LOCAL)
#define CAT_USE_COMMON_GLOBAL_ACCESSOR 1
#endif

#ifdef CAT_USE_THREAD_LOCAL
# if defined(CAT_OS_WIN) && defined(_MSC_VER)
#  define CAT_THREAD_LOCAL __declspec(thread)
# else
#  define CAT_THREAD_LOCAL __thread
# endif
#endif /* CAT_USE_THREAD_LOCAL */

#ifdef CAT_USE_COMMON_GLOBAL_ACCESSOR
# ifndef CAT_THREAD_LOCAL
#  define CAT_GLOBALS_DECLARE(name) CAT_GLOBALS_TYPE(name) CAT_GLOBALS(name)
# else
#  define CAT_GLOBALS_DECLARE(name) CAT_THREAD_LOCAL CAT_GLOBALS_TYPE(name) CAT_GLOBALS(name)
# endif /* CAT_THREAD_LOCAL */
# define CAT_GLOBALS_BULK(name)     (&CAT_GLOBALS(name))

# define CAT_GLOBALS_MODULE_INIT()
# define CAT_GLOBALS_MODULE_SHUTDOWN()
# define CAT_GLOBALS_RUNTIME_INIT()
# define CAT_GLOBALS_RUNTIME_CLOSE()

# define CAT_GLOBALS_REGISTER(name)
# define CAT_GLOBALS_UNREGISTER(name)
#endif /* CAT_USE_COMMON_GLOBAL_ACCESSOR */

#ifdef CAT_USE_THREAD_KEY
# define CAT_GLOBALS_OFFSET(name)  name##_offset
# define CAT_GLOBALS_DECLARE(name) size_t CAT_GLOBALS_OFFSET(name)
# define CAT_GLOBALS_BULK(name)    ((CAT_GLOBALS_TYPE(name) *) cat_globals_get_by_offset(CAT_GLOBALS_OFFSET(name)))

extern CAT_API uv_key_t cat_globals_key;
extern CAT_API size_t cat_globals_size;

static cat_always_inline void *cat_globals_get_by_offset(size_t offset)
{
    return ((char *) uv_key_get(&cat_globals_key)) + offset;
}

CAT_API void cat_globals_module_init(void);
CAT_API void cat_globals_module_shutdown(void);
CAT_API void cat_globals_runtime_init(void);
CAT_API void cat_globals_runtime_close(void);

CAT_API void cat_globals_register(size_t size, size_t *offset);
CAT_API void cat_globals_unregister(size_t size, size_t *offset);

# define CAT_GLOBALS_MODULE_INIT()     cat_globals_module_init()
# define CAT_GLOBALS_MODULE_SHUTDOWN() cat_globals_module_shutdown()
# define CAT_GLOBALS_RUNTIME_INIT()    cat_globals_runtime_init()
# define CAT_GLOBALS_RUNTIME_CLOSE()   cat_globals_runtime_close()

# define CAT_GLOBALS_REGISTER(name)   cat_globals_register(sizeof(CAT_GLOBALS_TYPE(name)), &CAT_GLOBALS_OFFSET(name))
# define CAT_GLOBALS_UNREGISTER(name) cat_globals_unregister(sizeof(CAT_GLOBALS_TYPE(name)), &CAT_GLOBALS_OFFSET(name))
#endif /* CAT_USE_THREAD_KEY */

#endif /* CAT_THREAD_SAFE */
