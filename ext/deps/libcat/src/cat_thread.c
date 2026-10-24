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

#include "cat_api.h"
#include "cat_thread.h"

#ifdef CAT_THREAD_SAFE

CAT_GLOBALS_DECLARE(cat_thread);

/*
 * The first root of the process is what cat_thread_is_main answers about, so it
 * lives here for every thread to read. The root of one thread's own runtime is
 * thread local and lives in the globals block above.
 *
 * The pointer below takes one write and reads afterwards, so it needs no lock.
 * The two counters take writes from the registering threads, so they are atomic.
 */
static cat_thread_t *cat_thread_main;
static cat_atomic_uint32_t cat_thread_count;
static cat_atomic_uint32_t cat_thread_external_count;

CAT_API cat_bool_t cat_thread_module_init(void)
{
    CAT_GLOBALS_REGISTER(cat_thread);

    return cat_true;
}

CAT_API cat_bool_t cat_thread_module_shutdown(void)
{
    /* The object may belong to the caller, so drop the pointer here. */
    cat_thread_main = NULL;

    CAT_GLOBALS_UNREGISTER(cat_thread);

    return cat_true;
}

CAT_API cat_bool_t cat_thread_runtime_init(void)
{
    /* A thread has no object until it registers one, so a read of the current
     * thread returns null before that. */
    return cat_true;
}

CAT_API cat_bool_t cat_thread_runtime_shutdown(void)
{
    cat_thread_t *thread = cat_thread_get_current_safe();

    if (thread != NULL) {
        /* an external thread stands in both counts, because the root is one of
         * them and it takes the same path */
        if (cat_atomic_uint32_load(&thread->state) & CAT_THREAD_STATE_EXTERNAL) {
            cat_atomic_uint32_fetch_sub(&cat_thread_external_count, 1);
            cat_atomic_uint32_fetch_sub(&cat_thread_count, 1);
        } else {
            cat_atomic_uint32_fetch_sub(&cat_thread_count, 1);
        }
        (void) cat_thread_set_current(NULL);
    }

    /* The next init of this thread must start from nothing. */
    if (cat_globals_available()) {
        CAT_THREAD_G(root) = NULL;
    }

    return cat_true;
}

/* The checks both public entries share. A rejected call leaves the object of the
 * caller untouched. */
static cat_bool_t cat_thread_register_check(const cat_thread_t *thread)
{
    if (thread == NULL) {
        cat_update_last_error(CAT_EINVAL, "Thread object is null");
        return cat_false;
    }

    if (!cat_globals_available()) {
        /* this thread never took a runtime, so it has nowhere to keep the
         * object of the running thread */
        cat_update_last_error(CAT_EMISUSE, "This thread has no runtime");
        return cat_false;
    }

    if (CAT_THREAD_G(current) != NULL && CAT_THREAD_G(current) != thread) {
        /* one thread holds one object, and a second one would leave the first
         * one counted and marked as running */
        cat_update_last_error(CAT_EMISUSE, "This thread has already registered an object");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_thread_register(cat_thread_t *thread)
{
    if (!cat_thread_register_check(thread)) {
        return cat_false;
    }

    if (CAT_THREAD_G(current) == thread) {
        return cat_true;
    }

    thread->id = uv_thread_self();
    cat_atomic_uint32_store(&thread->state,
        cat_atomic_uint32_load(&thread->state) | CAT_THREAD_STATE_JOINABLE);

    return cat_thread_set_current(thread);
}

static cat_bool_t cat_thread_register_root(cat_thread_t *thread)
{
    if (thread == NULL) {
        cat_update_last_error(CAT_EINVAL, "Thread object is null");
        return cat_false;
    }

    cat_thread_id_t current_id = uv_thread_self();

    if (CAT_THREAD_G(root) != NULL) {
        /* A second call with the same object repeats the registration. Any other
         * call breaks it. */
        if (CAT_THREAD_G(root) != thread || !uv_thread_equal(&thread->id, &current_id)) {
            cat_update_last_error(CAT_EMISUSE, "This thread already has a root");
            return cat_false;
        }
        return cat_true;
    }

    if (cat_thread_get_current_safe() != NULL && cat_thread_get_current_safe() != thread) {
        cat_update_last_error(CAT_EMISUSE, "This thread has already registered an object");
        return cat_false;
    }

    if (cat_atomic_uint32_load(&thread->state) != 0 && !uv_thread_equal(&thread->id, &current_id)) {
        /* Adopting it would overwrite the id and the state of the other
         * thread. */
        cat_update_last_error(CAT_EMISUSE, "The object belongs to another thread");
        return cat_false;
    }

    if (!cat_thread_register_check(thread)) {
        return cat_false;
    }

    memset(thread, 0, sizeof(*thread));
    cat_atomic_uint32_store(&thread->state, CAT_THREAD_STATE_EXTERNAL | CAT_THREAD_STATE_JOINABLE);
    thread->parent = NULL;
    thread->id = uv_thread_self();
    if (!cat_thread_set_current(thread)) {
        return cat_false;
    }
    CAT_THREAD_G(root) = thread;
    if (cat_thread_main == NULL) {
        /* The first root of the process is the main thread. */
        cat_thread_main = thread;
    }
    cat_atomic_uint32_fetch_add(&cat_thread_count, 1);
    cat_atomic_uint32_fetch_add(&cat_thread_external_count, 1);

    return cat_true;
}

/*
 * Apply a name to the running thread without a write to its object. The creator
 * filled the object already, and a write here would race with a reader.
 */
static void cat_thread_apply_name(const char *name)
{
    (void) uv_thread_setname(name);
}

CAT_API cat_bool_t cat_thread_bootstrap(cat_thread_t *thread)
{
    if (unlikely(thread == NULL)) {
        cat_update_last_error(CAT_EINVAL, "Thread object is null");
        return cat_false;
    }

    if (unlikely(!cat_globals_available())) {
        cat_update_last_error(CAT_EMISUSE, "The thread has no runtime");
        return cat_false;
    }

    if (CAT_THREAD_G(current) == thread) {
        return cat_true;
    }

    if (unlikely(!cat_thread_set_current(thread))) {
        return cat_false;
    }

    cat_atomic_uint32_fetch_add(&cat_thread_count, 1);

    if (thread->name[0] != '\0') {
        cat_thread_apply_name(thread->name);
    }

    return cat_true;
}

CAT_API void cat_thread_teardown(cat_thread_t *thread)
{
    if (cat_atomic_uint32_load(&thread->state) & CAT_THREAD_STATE_EXITED) {
        return;
    }

    /* the registration needs a runtime, the wake of the joiners does not */
    if (cat_globals_available() && CAT_THREAD_G(current) == thread) {
        cat_atomic_uint32_fetch_sub(&cat_thread_count, 1);
        (void) cat_thread_set_current(NULL);
    }

    /* Record the exit before the wake, so a joiner that sees it sees a finished
     * thread. The store is atomic because the joiner reads it from another
     * thread. */
    cat_atomic_uint32_store(&thread->state,
        cat_atomic_uint32_load(&thread->state) | CAT_THREAD_STATE_EXITED);
    (void) cat_thread_wait_wake_all(&thread->join);

    /* The wake above still touches the object. Mark the end after it, so a
     * caller that sees a stopped thread may reuse the object. */
    cat_atomic_uint32_store(&thread->state,
        cat_atomic_uint32_load(&thread->state) | CAT_THREAD_STATE_FINISHED);
}

CAT_API cat_bool_t cat_thread_register_main(cat_thread_t *thread)
{
    return cat_thread_register_root(thread);
}

/*
 * Give this thread a root object when it has none. A thread can then run without
 * the caller that declares itself first.
 */
CAT_API cat_bool_t cat_thread_ensure_root(void)
{
    if (!cat_globals_available()) {
        cat_update_last_error(CAT_EMISUSE, "This thread has no runtime");
        return cat_false;
    }

    /* A thread that libcat started already has its object. */
    if (CAT_THREAD_G(current) != NULL) {
        return cat_true;
    }

    return cat_thread_register_root(&CAT_THREAD_G(root_storage));
}


CAT_API cat_bool_t cat_thread_register_external(cat_thread_t *thread)
{
    if (thread == NULL) {
        cat_update_last_error(CAT_EINVAL, "Thread object is null");
        return cat_false;
    }

    if (cat_thread_main == NULL) {
        /* without the root there is nothing to attach an outside thread to */
        cat_update_last_error(CAT_EMISUSE, "The main thread is not registered yet");
        return cat_false;
    }

    if (cat_thread_get_current_safe() == thread) {
        return cat_true;
    }

    if (!cat_thread_register_check(thread)) {
        return cat_false;
    }

    memset(thread, 0, sizeof(*thread));
    cat_atomic_uint32_store(&thread->state, CAT_THREAD_STATE_EXTERNAL | CAT_THREAD_STATE_JOINABLE);
    thread->parent = cat_thread_get_current_safe() != NULL ? cat_thread_get_current_safe()
                                                          : cat_thread_main;
    thread->id = uv_thread_self();
    if (!cat_thread_set_current(thread)) {
        return cat_false;
    }
    /* An external thread counts as an external object and as a thread, so
     * subtract both. */
    cat_atomic_uint32_fetch_add(&cat_thread_count, 1);
    cat_atomic_uint32_fetch_add(&cat_thread_external_count, 1);

    return cat_true;
}

CAT_API cat_thread_t *cat_thread_get_current(void)
{
    cat_thread_t *thread = cat_thread_get_current_safe();

    if (unlikely(thread == NULL)) {
        cat_update_last_error(CAT_EMISUSE, "Thread is not registered");
    }

    return thread;
}

CAT_API cat_bool_t cat_thread_equal(const cat_thread_t *a, const cat_thread_t *b)
{
    if (a == NULL || b == NULL) {
        return cat_false;
    }

    return !!uv_thread_equal(&a->id, &b->id);
}

CAT_API cat_bool_t cat_thread_is_main(const cat_thread_t *thread)
{
    return thread != NULL && thread == cat_thread_main;
}

CAT_API cat_bool_t cat_thread_is_running(const cat_thread_t *thread)
{
    /* A thread that is still in its teardown counts as running, so a caller
     * that sees a stopped thread may reuse the object. */
    return thread != NULL &&
        !(cat_atomic_uint32_load(&((cat_thread_t *) thread)->state) & CAT_THREAD_STATE_FINISHED);
}

CAT_API cat_thread_t *cat_thread_get_parent(const cat_thread_t *thread)
{
    return thread != NULL ? thread->parent : NULL;
}

CAT_API cat_thread_id_t cat_thread_get_id(const cat_thread_t *thread)
{
    if (thread == NULL) {
        cat_update_last_error(CAT_EINVAL, "Thread object is null");
        return (cat_thread_id_t) 0;
    }

    return thread->id;
}

CAT_API cat_thread_id_t cat_thread_get_current_id(void)
{
    return uv_thread_self();
}

CAT_API uint32_t cat_thread_get_count(void)
{
    return cat_atomic_uint32_load(&cat_thread_count);
}

CAT_API uint32_t cat_thread_get_external_count(void)
{
    return cat_atomic_uint32_load(&cat_thread_external_count);
}

CAT_API cat_bool_t cat_thread_set_name(const char *name)
{
    cat_thread_t *thread = cat_thread_get_current();
    if (thread == NULL) {
        return cat_false;
    }

    /* The platform sets the name of the running thread only. The limit comes
     * from the platform. */
    const char *wanted = name != NULL ? name : "";
    size_t length = strlen(wanted);
    if (length > CAT_THREAD_NAME_MAX) {
        cat_update_last_error(CAT_EINVAL, "Thread name is longer than %d bytes", CAT_THREAD_NAME_MAX);
        return cat_false;
    }

    /* An empty name drops the name of this thread. The platform may refuse that,
     * and the object drops the name either way. */
    if (length == 0) {
        thread->name[0] = '\0';
        (void) uv_thread_setname("");

        return cat_true;
    }

    /* Ask the platform first. A name it refuses never reaches the object, so the
     * two stay in step. */
    int error = uv_thread_setname(wanted);
    if (error != 0) {
        cat_update_last_error(CAT_EINVAL, "Thread name is not accepted by the platform, reason: %s",
            uv_strerror(error));
        return cat_false;
    }

    memcpy(thread->name, wanted, length + 1);

    return cat_true;
}

/*
 * The entry of a thread that libcat started. The thread takes a runtime of its
 * own, so the routine can use this library, and can suspend a coroutine.
 */
static void cat_thread_entry(void *data)
{
    cat_thread_t *thread = (cat_thread_t *) data;
    cat_bool_t runtime_was_up = cat_false;

    if (thread->create_flags & CAT_THREAD_CREATE_MANUAL_RUNTIME) {
        /* The routine takes the runtime and calls cat_thread_teardown(), so this
         * entry touches nothing after the routine returns. */
        thread->routine(thread->data);
        return;
    }

    if (unlikely(!cat_runtime_init_all())) {
        goto _done;
    }
    runtime_was_up = cat_true;

    /* The object was filled by create, so this thread only takes it as its own.
     * Do not register it again, because that would clear the fields. */
    if (unlikely(!cat_thread_bootstrap(thread))) {
        goto _done;
    }

    /* After this the main coroutine of this thread can wait. */
    if (unlikely(!cat_run(CAT_RUN_EASY))) {
        goto _done;
    }

    thread->routine(thread->data);

    /* Wait for the coroutines that this thread still has. */
    (void) cat_stop();

_done:
    if (runtime_was_up) {
        (void) cat_runtime_shutdown_all();
        (void) cat_runtime_close_all();
    }

    cat_thread_teardown(thread);
}

CAT_API cat_bool_t cat_thread_create_ex(cat_thread_t *thread, cat_thread_routine_t routine, cat_data_t *data,
                                        const cat_thread_options_t *options)
{
    int error;

    if (unlikely(thread == NULL || routine == NULL)) {
        cat_update_last_error(CAT_EINVAL, "Thread object and routine are required");
        return cat_false;
    }

    if (unlikely(options != NULL && (options->flags & ~CAT_THREAD_CREATE_MANUAL_RUNTIME) != 0)) {
        cat_update_last_error(CAT_EINVAL, "Unknown thread create flags");
        return cat_false;
    }

    memset(thread, 0, sizeof(*thread));
    thread->parent = cat_thread_get_current();
    thread->routine = routine;
    thread->data = data;
    thread->create_flags = options != NULL ? options->flags : 0;
    cat_atomic_uint32_store(&thread->state, CAT_THREAD_STATE_JOINABLE);

    if (options != NULL && options->name != NULL) {
        size_t length = strlen(options->name);

        if (length > CAT_THREAD_NAME_MAX) {
            length = CAT_THREAD_NAME_MAX;
        }
        memcpy(thread->name, options->name, length);
        thread->name[length] = '\0';
    }

    if (unlikely(!cat_thread_wait_init(&thread->join))) {
        return cat_false;
    }

    if (options != NULL && options->stack_size != 0) {
        uv_thread_options_t uv_options = {
            .flags = UV_THREAD_HAS_STACK_SIZE,
            .stack_size = options->stack_size,
        };

        error = uv_thread_create_ex(&thread->id, &uv_options, cat_thread_entry, thread);
    } else {
        error = uv_thread_create(&thread->id, cat_thread_entry, thread);
    }

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Thread create failed");
        cat_thread_wait_destroy(&thread->join);
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_thread_create(cat_thread_t *thread, cat_thread_routine_t routine, cat_data_t *data)
{
    return cat_thread_create_ex(thread, routine, data, NULL);
}

static cat_bool_t cat_thread_exited(const void *data)
{
    const cat_thread_t *thread = (const cat_thread_t *) data;

    return (cat_atomic_uint32_load(&thread->state) & CAT_THREAD_STATE_EXITED) != 0;
}

CAT_API cat_bool_t cat_thread_join(cat_thread_t *thread, cat_timeout_t timeout)
{
    uint32_t state;

    if (unlikely(thread == NULL)) {
        cat_update_last_error(CAT_EINVAL, "Thread object is null");
        return cat_false;
    }

    if (unlikely(thread == cat_thread_get_current_safe())) {
        /* a thread that waits for itself waits for ever */
        cat_update_last_error(CAT_EMISUSE, "A thread can not join itself");
        return cat_false;
    }

    state = cat_atomic_uint32_load(&thread->state);

    if (unlikely(!(state & CAT_THREAD_STATE_JOINABLE) || (state & CAT_THREAD_STATE_JOINED) ||
                 (state & CAT_THREAD_STATE_DETACHED))) {
        /* a detached thread finishes on its own, so there is no exit to take */
        cat_update_last_error(CAT_EMISUSE, "The thread is not joinable");
        return cat_false;
    }

    /* the predicate is read under the lock of the queue, so an exit that lands
     * between the check and the park still wakes this caller */
    if (unlikely(!cat_thread_wait_park_until(&thread->join, timeout,
                                             cat_thread_exited, thread))) {
        return cat_false;
    }

    /* Only one caller takes the exit, because the thread can be joined once. A
     * loaded state that already holds JOINED has to fail first, because the
     * compare and exchange below would succeed without changing anything and
     * hand the exit to two callers. */
    state = cat_atomic_uint32_load(&thread->state);
    for (;;) {
        if (unlikely(state & CAT_THREAD_STATE_JOINED)) {
            cat_update_last_error(CAT_EMISUSE, "The thread is already joined");
            return cat_false;
        }
        if (cat_atomic_uint32_compare_exchange_strong(&thread->state, &state,
                                                      state | CAT_THREAD_STATE_JOINED)) {
            break;
        }
    }

    (void) uv_thread_join(&thread->id);

    /* The thread is gone and no waiter is left, so the queue is free to destroy */
    cat_thread_wait_destroy(&thread->join);

    return cat_true;
}

CAT_API cat_bool_t cat_thread_detach(cat_thread_t *thread)
{
    uint32_t state;

    if (unlikely(thread == NULL)) {
        cat_update_last_error(CAT_EINVAL, "Thread object is null");
        return cat_false;
    }

    state = cat_atomic_uint32_load(&thread->state);

    if (unlikely(!(state & CAT_THREAD_STATE_JOINABLE) || (state & CAT_THREAD_STATE_JOINED))) {
        cat_update_last_error(CAT_EMISUSE, "The thread is not joinable");
        return cat_false;
    }

    if (unlikely(state & CAT_THREAD_STATE_DETACHED)) {
        cat_update_last_error(CAT_EMISUSE, "The thread is already detached");
        return cat_false;
    }

    cat_atomic_uint32_store(&thread->state, state | CAT_THREAD_STATE_DETACHED);
    (void) uv_thread_detach(&thread->id);

    return cat_true;
}

#endif /* CAT_THREAD_SAFE */
