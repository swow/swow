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

#ifndef CAT_THREAD_SAFE /* the block of a module, one per thread or one per process */

#if defined(CAT_USE_THREAD_LOCAL) && defined(CAT_USE_THREAD_KEY)
# error "Unable to use THREAD_LOCAL and THREAD_KEY at the same time"
#endif

#if defined(CAT_USE_THREAD_LOCAL) || defined(CAT_USE_THREAD_KEY)
# define CAT_THREAD_SAFE 1
#endif

#if defined(CAT_USE_THREAD_LOCAL)
/*
 * use thread local storage to store the globals block
 * the globals is declared as a global thread local variable
 */

/* implement specific */
# if defined(CAT_OS_WIN) && defined(_MSC_VER)
#  define _CAT_THREAD_LOCAL __declspec(thread)
# else
#  define _CAT_THREAD_LOCAL __thread
# endif

CAT_API cat_bool_t cat_globals_is_initialized(void);
CAT_API void cat_globals_runtime_init(void);
CAT_API void cat_globals_runtime_shutdown(void);

/* globals lifecycle functions */
# define CAT_GLOBALS_AVAILABLE()        cat_globals_is_initialized()
# define CAT_GLOBALS_MODULE_INIT()
# define CAT_GLOBALS_MODULE_SHUTDOWN()
# define CAT_GLOBALS_RUNTIME_INIT()     cat_globals_runtime_init()
# define CAT_GLOBALS_RUNTIME_SHUTDOWN() cat_globals_runtime_shutdown()

# define CAT_GLOBALS_REGISTER(name)
# define CAT_GLOBALS_UNREGISTER(name)

/* declaration and getter */
# define CAT_GLOBALS_DECLARE(name)     _CAT_THREAD_LOCAL CAT_GLOBALS_TYPE(name) CAT_GLOBALS(name)
# define CAT_GLOBALS_BULK(name)        (&CAT_GLOBALS(name))
#elif defined(CAT_USE_THREAD_KEY)
/*
 * use thread key to store the globals block
 * the globals is allocated on the heap and index stored in a key associated with the thread
 */
/* implement specific */
# define _CAT_GLOBALS_OFFSET(name)      name##_offset

CAT_API void cat_globals_module_init(void);
CAT_API void cat_globals_module_shutdown(void);
CAT_API void cat_globals_runtime_init(void);
CAT_API void cat_globals_runtime_shutdown(void);

CAT_API void cat_globals_register(size_t size, size_t *offset);
CAT_API void cat_globals_unregister(size_t size, size_t *offset);

/* the block of this thread, which the key holds */
extern CAT_API uv_key_t cat_globals_key;

static cat_always_inline void *cat_globals_get_by_offset(size_t offset)
{
    return ((char *) uv_key_get(&cat_globals_key)) + offset;
}

/* a missing key means that this thread never took a runtime */
CAT_API cat_bool_t cat_globals_key_available(void);

/* globals lifecycle functions */

# define CAT_GLOBALS_AVAILABLE()        cat_globals_key_available()
# define CAT_GLOBALS_MODULE_INIT()      cat_globals_module_init()
# define CAT_GLOBALS_MODULE_SHUTDOWN()  cat_globals_module_shutdown()
# define CAT_GLOBALS_RUNTIME_INIT()     cat_globals_runtime_init()
# define CAT_GLOBALS_RUNTIME_SHUTDOWN() cat_globals_runtime_shutdown()

# define CAT_GLOBALS_REGISTER(name)     cat_globals_register(sizeof(CAT_GLOBALS_TYPE(name)), &_CAT_GLOBALS_OFFSET(name))
# define CAT_GLOBALS_UNREGISTER(name)   cat_globals_unregister(sizeof(CAT_GLOBALS_TYPE(name)), &_CAT_GLOBALS_OFFSET(name))

/* declaration and getter */
# define CAT_GLOBALS_DECLARE(name)      size_t _CAT_GLOBALS_OFFSET(name)
# define CAT_GLOBALS_BULK(name)         ((CAT_GLOBALS_TYPE(name) *) cat_globals_get_by_offset(_CAT_GLOBALS_OFFSET(name)))
#else
/*
 * no thread-safety, the globals block is a real global variable
 */

/* globals lifecycle functions */
# define CAT_GLOBALS_AVAILABLE()        (cat_true)
# define CAT_GLOBALS_MODULE_INIT()
# define CAT_GLOBALS_MODULE_SHUTDOWN()
# define CAT_GLOBALS_RUNTIME_INIT()
# define CAT_GLOBALS_RUNTIME_SHUTDOWN()
# define CAT_GLOBALS_REGISTER(name)
# define CAT_GLOBALS_UNREGISTER(name)

/* declaration and getter */
# define CAT_GLOBALS_DECLARE(name)      CAT_GLOBALS_TYPE(name) CAT_GLOBALS(name)
# define CAT_GLOBALS_BULK(name)         (&CAT_GLOBALS(name))
#endif

#endif /* ndef CAT_THREAD_SAFE */

/*
 * A host that defines CAT_THREAD_SAFE and keeps the globals itself must also
 * define CAT_GLOBALS_AVAILABLE(): cat_true when the current thread has a block.
 */
#if !defined(CAT_GLOBALS_AVAILABLE)
# error "Define CAT_GLOBALS_AVAILABLE() for this globals storage"
#endif

/*
 * Every read of a block goes through this, because a thread without a runtime
 * has nowhere to keep one and the accessor above would walk away from a null
 * base. The name stays the same in all storage modes.
 */
static cat_always_inline cat_bool_t cat_globals_available(void)
{
    return CAT_GLOBALS_AVAILABLE();
}
