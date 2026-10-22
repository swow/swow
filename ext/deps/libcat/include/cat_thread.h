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

#ifndef CAT_THREAD_H
#define CAT_THREAD_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#include "cat_atomic.h"
#include "cat_thread_sync.h"

/*
 * The thread model needs a per-thread place to keep the object of the running
 * thread, so a build without a thread local store has none of these symbols.
 */
#ifdef CAT_THREAD_SAFE

typedef uv_thread_t cat_thread_id_t;

#define CAT_THREAD_NAME_MAX 15

typedef struct cat_thread_s cat_thread_t;

/*
 * The registry alone needs the identity, the parent link, the state and the
 * name. Creation adds the routine and its data, and join adds the rest.
 *
 * Fields are grouped by size, so the compiler adds little padding.
 */
/*
 * What a thread runs. It has no return value. A thread that reports a result uses
 * the cross-thread facilities, like any other caller.
 */
typedef void (*cat_thread_routine_t)(cat_data_t *data);

typedef uint32_t cat_thread_create_flags_t;

/*
 * The routine takes the runtime of its thread.
 *
 * It must start the runtime before the first call into this library, and stop it
 * before it returns. A host that stops the modules one by one must call
 * cat_thread_runtime_shutdown() as part of that, so the registration of the
 * thread goes away with the runtime.
 *
 * It must call cat_thread_bootstrap() after the runtime is up, and
 * cat_thread_teardown() as its last call on the object. A thread of this kind has
 * no event loop and no coroutine until the routine starts them.
 */
#define CAT_THREAD_CREATE_MANUAL_RUNTIME (1u << 0)

typedef struct cat_thread_options_s {
    /* the name given to the thread, or NULL to leave it alone */
    const char *name;
    /* the stack size of the coroutines of this thread, 0 for the default */
    size_t stack_size;
    /* zero, or CAT_THREAD_CREATE_MANUAL_RUNTIME */
    cat_thread_create_flags_t flags;
} cat_thread_options_t;

struct cat_thread_s {
    /* 8 byte */
    cat_thread_id_t id;
    cat_thread_t *parent;
    cat_thread_routine_t routine;
    cat_data_t *data;
    /* a thread that exits wakes whoever joins it */
    cat_thread_wait_t join;
    /* 4 byte */
    cat_atomic_uint32_t state;
    cat_thread_create_flags_t create_flags;
    /* at most CAT_THREAD_NAME_MAX bytes plus the terminator, kept last */
    char name[CAT_THREAD_NAME_MAX + 1];
};

#define CAT_THREAD_STATE_JOINABLE    (1 << 0)
#define CAT_THREAD_STATE_JOINED      (1 << 1)
#define CAT_THREAD_STATE_EXITED      (1 << 2)
/* the object of a thread that libcat did not create */
#define CAT_THREAD_STATE_EXTERNAL    (1 << 3)
/* the thread finishes on its own and can not be joined */
#define CAT_THREAD_STATE_DETACHED    (1 << 4)
/* the teardown is complete, so the thread no longer touches the object */
#define CAT_THREAD_STATE_FINISHED    (1 << 5)

CAT_API cat_bool_t cat_thread_module_init(void);
CAT_API cat_bool_t cat_thread_module_shutdown(void);
CAT_API cat_bool_t cat_thread_runtime_init(void);
CAT_API cat_bool_t cat_thread_runtime_shutdown(void);

/*
 * Register the object of the calling thread, after the thread took a runtime.
 *
 * libcat registers a managed thread itself, so only a routine of a manual runtime
 * thread calls this. After the call, cat_thread_get_current() returns the object
 * and the thread counts as alive.
 *
 * The call does nothing after the first one. It fails with CAT_EMISUSE on a
 * thread that has no runtime.
 */
CAT_API cat_bool_t cat_thread_bootstrap(cat_thread_t *thread);

/*
 * Mark the calling thread as exited and wake whoever joins it.
 *
 * It is the last call a manual runtime routine makes on its own object, and the
 * entry of that thread touches nothing after the routine returns. The routine may
 * release the storage right after this call.
 *
 * It also drops the registration of the thread when the runtime is still up, so
 * the count and the current object stay clean when the host does not call
 * cat_thread_runtime_shutdown().
 *
 * The call does nothing after the first one.
 */
CAT_API void cat_thread_teardown(cat_thread_t *thread);

/* Optional. A thread that runs cat_run becomes the root of its own runtime when
 * no earlier call registered it. */
CAT_API cat_bool_t cat_thread_register_main(cat_thread_t *thread);
CAT_API cat_bool_t cat_thread_ensure_root(void); CAT_INTERNAL
CAT_API cat_bool_t cat_thread_register_external(cat_thread_t *thread);
CAT_API cat_bool_t cat_thread_register(cat_thread_t *thread); CAT_INTERNAL

CAT_API cat_thread_t   *cat_thread_get_current(void);
CAT_API cat_bool_t      cat_thread_equal(const cat_thread_t *a, const cat_thread_t *b);
CAT_API cat_bool_t      cat_thread_is_main(const cat_thread_t *thread);
/* False once the thread finished its teardown, so a caller that sees false may
 * reuse the object. */
CAT_API cat_bool_t      cat_thread_is_running(const cat_thread_t *thread);
CAT_API cat_thread_t   *cat_thread_get_parent(const cat_thread_t *thread);
CAT_API cat_thread_id_t cat_thread_get_id(const cat_thread_t *thread);
CAT_API cat_thread_id_t cat_thread_get_current_id(void);
CAT_API uint32_t        cat_thread_get_count(void);
CAT_API uint32_t        cat_thread_get_external_count(void);
CAT_API cat_bool_t      cat_thread_set_name(const char *name);

/*
 * Start a thread that runs routine.
 *
 * The object belongs to the caller and must stay alive until the thread is
 * joined or detached. The call returns before the routine runs, so a caller
 * that needs to know it started synchronizes with it.
 *
 * The thread takes a runtime of its own, so routine may use the facilities of
 * this library. With CAT_THREAD_CREATE_MANUAL_RUNTIME the routine takes the
 * runtime instead, because a host with its own per-thread setup needs to run it
 * at its own point. See cat_thread_create_ex().
 */
CAT_API cat_bool_t cat_thread_create(cat_thread_t *thread, cat_thread_routine_t routine, cat_data_t *data);
CAT_API cat_bool_t cat_thread_create_ex(cat_thread_t *thread, cat_thread_routine_t routine, cat_data_t *data,
                                        const cat_thread_options_t *options);

/*
 * Wait for a thread to exit without blocking the thread that waits: the caller
 * gives its thread back to the event loop, and every other coroutine of it keeps
 * running.
 *
 * cat_false on a timeout (CAT_ETIMEDOUT), on a thread that is not joinable
 * (CAT_EMISUSE), and when a thread tries to join itself (CAT_EMISUSE).
 */
CAT_API cat_bool_t cat_thread_join(cat_thread_t *thread, cat_timeout_t timeout);

/*
 * Let a thread finish on its own. The object must outlive the thread, and the
 * thread can not be joined afterwards (CAT_EMISUSE).
 */
CAT_API cat_bool_t cat_thread_detach(cat_thread_t *thread);

/*
 * Only the object of the running thread is thread local. The root and the two
 * counters belong to the process, because every thread reads them.
 */
CAT_GLOBALS_STRUCT_BEGIN(cat_thread) {
    cat_thread_t *current;
    /* the root of this thread's runtime, which is the caller's object when it
     * supplied one and the storage below when it did not */
    cat_thread_t *root;
    cat_thread_t root_storage;
} CAT_GLOBALS_STRUCT_END(cat_thread);

extern CAT_API CAT_GLOBALS_DECLARE(cat_thread);

#define CAT_THREAD_G(x) CAT_GLOBALS_GET(cat_thread, x)

/*
 * Every read and write of the block goes through these, because a thread without
 * a runtime has no block and G(x) would walk away from a null base.
 */
static cat_always_inline cat_thread_t *cat_thread_get_current_safe(void)
{
    return cat_globals_available() ? CAT_THREAD_G(current) : NULL;
}

static cat_always_inline cat_bool_t cat_thread_set_current(cat_thread_t *thread)
{
    if (!cat_globals_available()) {
        return cat_false;
    }
    CAT_THREAD_G(current) = thread;

    return cat_true;
}

#endif /* CAT_THREAD_SAFE */

#ifdef __cplusplus
}
#endif
#endif /* CAT_THREAD_H */
