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

#include "cat_thread.h"

#ifdef CAT_THREAD

#include "cat_atomic.h"
#include "cat_event.h"

static uv_key_t cat_thread_key;

static cat_thread_t *cat_thread_main;
static cat_thread_t _cat_thread_main;

static cat_atomic_uint32_t cat_thread_count;
static cat_atomic_uint64_t cat_thread_last_id;

static void cat_thread_async_callback(uv_async_t *handle)
{
    cat_thread_t *thread = cat_container_of(handle, cat_thread_t, notifier);

    cat_thread_do_tasks(thread);
}

static void cat_thread_notify_parent_waiters(cat_thread_t *sub_thread)
{
    uint32_t count = sub_thread->parent_waiter_count;

    while (count--) {
        cat_coroutine_t *waiter = cat_queue_front_data(&sub_thread->parent_waiters, cat_coroutine_t, waiter.node);
        cat_coroutine_schedule(waiter, THREAD, "Thread notify waiters");
    }
}

static void cat_thread_ready_notify_task(cat_data_t *data)
{
    cat_thread_t *sub_thread = (cat_thread_t *) data;
    sub_thread->started = cat_true;
    sub_thread->parent->child_count++;
    cat_thread_notify_parent_waiters(sub_thread);
}

static void cat_thread_exit_notify_task(cat_data_t *data)
{
    cat_thread_t *sub_thread = (cat_thread_t *) data;
    sub_thread->exited = cat_true;
    sub_thread->parent->child_count--;
    cat_thread_notify_parent_waiters(sub_thread);
}

static cat_always_inline void cat_thread_current_notifier_addref(cat_thread_t *thread)
{
    if (thread->notifier_refcount++ == 0) {
        uv_ref((uv_handle_t *) &thread->notifier);
    }
}

static cat_always_inline void cat_thread_current_notifier_delref(cat_thread_t *thread)
{
    if (--thread->notifier_refcount == 0) {
        uv_unref((uv_handle_t *) &thread->notifier);
    }
}

#define CAT_THREAD_CURRENT_NOTIFIER_LISTEN_START() do { \
    cat_thread_t *_current_thread = cat_thread_get_current(); \
    cat_thread_current_notifier_addref(_current_thread); \

#define CAT_THREAD_CURRENT_NOTIFIER_LISTEN_END() \
    cat_thread_current_notifier_delref(_current_thread); \
} while (0)

static cat_bool_t cat_thread_wait_for_child_ex(cat_thread_t *thread, cat_timeout_t timeout)
{
    cat_queue_t *waiter = &CAT_COROUTINE_G(current)->waiter.node;
    cat_bool_t ret;

    cat_queue_push_back(&thread->parent_waiters, waiter);
    thread->parent_waiter_count++;
    CAT_THREAD_CURRENT_NOTIFIER_LISTEN_START() {
        ret = cat_time_wait(timeout);
    } CAT_THREAD_CURRENT_NOTIFIER_LISTEN_END();
    thread->parent_waiter_count--;
    cat_queue_remove(waiter);

    if (unlikely(!ret)) {
        cat_update_last_error_with_previous("Thread wait failed");
        return cat_false;
    } else if (thread->exited) {
        return cat_true; /* Think of it as if it has been started and exited */
    } else if (!thread->started) {
        cat_update_last_error(CAT_ECANCELED, "Thread wait has been canceled");
        return cat_false;
    }

    return cat_true;
}

static cat_bool_t cat_thread_wait_for_child(cat_thread_t *thread)
{
    return cat_thread_wait_for_child_ex(thread, CAT_TIMEOUT_FOREVER);
}

static cat_bool_t cat_thread_init(cat_thread_t *thread, cat_thread_routine_t routine, cat_data_t *arg, cat_thread_t *parent)
{
    int error;

    thread->started = cat_false;
    thread->exited = cat_false;
    thread->joined = cat_false;
    thread->routine = routine;
    thread->transfer_data = arg;
    thread->child_count = 0;
    thread->parent = parent;
    thread->parent_waiter_count = 0;
    cat_queue_init(&thread->parent_waiters);
    thread->task_count = 0;
    cat_queue_init(&thread->tasks);
    error = uv_mutex_init(&thread->task_mutex);
    if (error != 0) {
        cat_update_last_error(error, "Thread task_mutex init failed");
        return cat_false;
    }

    return cat_true;
}

static void cat_thread_release(cat_thread_t *thread)
{
    uv_mutex_destroy(&thread->task_mutex);
}

static void cat_thread_routine(cat_data_t *arg)
{
    cat_thread_t *thread = (cat_thread_t *) arg;

    uv_key_set(&cat_thread_key, thread);

    thread->id = cat_atomic_uint64_fetch_add(&cat_thread_last_id, 1);
    cat_atomic_uint32_fetch_add(&cat_thread_count, 1);

    thread->routine(thread->transfer_data);

    cat_atomic_uint32_fetch_sub(&cat_thread_count, 1);

    cat_thread_dispatch_task(thread->parent, cat_thread_exit_notify_task, thread);
}

CAT_API uint32_t cat_thread_get_count(void)
{
    return cat_atomic_uint32_load(&cat_thread_count);
}

CAT_API cat_thread_t *cat_thread_create(cat_thread_t *thread, cat_thread_routine_t routine, cat_data_t *arg)
{
    cat_thread_options_t options;

    options.flags = CAT_THREAD_OPTION_FLAG_NONE;

    return cat_thread_create_ex(thread, routine, arg, &options);
}

CAT_API cat_thread_t *cat_thread_create_ex(cat_thread_t *thread, cat_thread_routine_t routine, cat_data_t *arg, const cat_thread_options_t *options)
{
    int error;

    if (thread == NULL) {
        thread = (cat_thread_t *) cat_sys_malloc(sizeof(*thread));
#if CAT_ALLOC_HANDLE_ERRORS
        if (thread == NULL) {
            cat_update_last_error_of_syscall("Malloc for thread failed");
            return NULL;
        }
#endif
        thread->allocated = cat_true;
    } else {
        thread->allocated = cat_false;
    }

    if (!cat_thread_init(thread, routine, arg, cat_thread_get_current())) {
        goto _thread_init_error;
    }

    error = uv_thread_create_ex(&thread->tid, (const uv_thread_options_t *) options, cat_thread_routine, thread);
    if (error != 0) {
        cat_update_last_error_with_reason(error, "Thread create failed");
        goto _thread_create_error;
    }

    (void) cat_thread_wait_for_child(thread);

    return thread;

    _thread_create_error:
    cat_thread_release(thread);
    _thread_init_error:
    if (thread->allocated) {
        cat_sys_free(thread);
    }
    return NULL;
}

CAT_API cat_bool_t cat_thread_close(cat_thread_t *thread)
{
    cat_thread_join(thread);

    if (thread->allocated) {
        cat_sys_free(thread);
    }

    return cat_true;
}

CAT_API cat_bool_t cat_thread_equal(const cat_thread_t *thread1, const cat_thread_t *thread2)
{
    return !!uv_thread_equal(&thread1->tid, &thread2->tid);
}

CAT_API cat_thread_t *cat_thread_get_current(void)
{
    cat_thread_t *thread = (cat_thread_t *) uv_key_get(&cat_thread_key);

    if (unlikely(thread == NULL)) {
        CAT_ERROR(THREAD, "Thread is created by unknown");
    }

#ifdef CAT_DEBUG
    uv_thread_t tid = uv_thread_self();
    CAT_ASSERT(uv_thread_equal(&thread->tid, &tid));
#endif

    return thread;
}


CAT_API cat_bool_t cat_thread_join(cat_thread_t *thread)
{
    if (thread->parent == NULL) {
        cat_update_last_error(CAT_EMISUSE, "Can not join main thread");
        return cat_false;
    }

    cat_thread_t *current_thread = cat_thread_get_current();
    if (current_thread != thread->parent) {
        cat_update_last_error(CAT_EMISUSE, "Can not join thread from another thread which is not parent of this thread");
        return cat_false;
    }

    if (!thread->started) {
        if (!cat_thread_wait_for_child(thread)) {
            cat_update_last_error_with_previous("Thread join failed when waiting for it to be ready");
            return cat_false;
        }
        CAT_ASSERT(thread->started);
    }
    if (!thread->exited) {
        if (!cat_thread_wait_for_child(thread)) {
            cat_update_last_error_with_previous("Thread join failed when waiting for it to be exited");
            return cat_false;
        }
        CAT_ASSERT(thread->exited);
    }
    /* TODO: join it later to prevent blocking? (sub-thread may be in cleaning) */
    if (!thread->joined) {
        /* FIXME: handle error? */
        int error = uv_thread_join(&thread->tid);

        /* Whether successful or not, it only means that a join has been attempted */
        thread->joined = cat_true;

        if (error != 0) {
            cat_update_last_error_with_reason(error, "Thread join failed");
            return cat_false;
        }
    }

    return cat_true;
}

CAT_API cat_bool_t cat_thread_dispatch_task(cat_thread_t *thread, cat_data_callback_t callback, cat_data_t *data)
{
    cat_thread_task_t *task = (cat_thread_task_t *) cat_sys_malloc(sizeof(*task));
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(task == NULL)) {
        cat_update_last_error_of_syscall("Malloc for thread task failed");
        return cat_false;
    }
#endif
    task->callback = callback;
    task->data = data;

    uv_mutex_lock(&thread->task_mutex);
    cat_queue_push_back(&thread->tasks, &task->node);
    uv_mutex_unlock(&thread->task_mutex);

    /* never failed */
    (void) uv_async_send(&thread->notifier);

    return cat_true;
}

CAT_API void cat_thread_do_tasks(cat_thread_t *thread)
{
    cat_queue_t *tasks = &thread->tasks;
    cat_thread_task_t *task;
    while (1) {
        uv_mutex_lock(&thread->task_mutex);
        task = cat_queue_front_data(tasks, cat_thread_task_t, node);
        if (task == NULL) {
            uv_mutex_unlock(&thread->task_mutex);
            break;
        }
        cat_queue_remove(&task->node);
        thread->task_count--;
        uv_mutex_unlock(&thread->task_mutex);
        task->callback(task->data);
        cat_sys_free(task);
    }
    CAT_ASSERT(cat_queue_empty(tasks));
}

CAT_API cat_thread_t *cat_thread_register_main(cat_thread_t *thread)
{
    CAT_ASSERT(cat_thread_get_current()->id == CAT_THREAD_MAIN_ID &&
        "can only be called by main thread");

    cat_thread_t *original_main = cat_thread_main;

    if (original_main != thread) {
        if (original_main != NULL) {
            memcpy(thread, original_main, sizeof(*thread));
        }
        cat_thread_main = thread;
    }

    return original_main;
}

CAT_API cat_bool_t cat_thread_module_init(void)
{
    int error = uv_key_create(&cat_thread_key);
    if (error != 0) {
        CAT_WARN_WITH_REASON(THREAD, error, "Thread key create failed");
        return cat_false;
    }
    cat_thread_main = &_cat_thread_main;
    uv_key_set(&cat_thread_key, cat_thread_main);

    cat_thread_main->id = CAT_THREAD_MAIN_ID;
    cat_thread_main->tid = uv_thread_self();
    cat_thread_init(cat_thread_main, NULL, NULL, NULL);

    return cat_true;
}

CAT_API cat_bool_t cat_thread_runtime_init(void)
{
    cat_thread_t *thread = cat_thread_get_current();

    if (thread->id == CAT_THREAD_MAIN_ID) {
        cat_atomic_uint32_init(&cat_thread_count, 1);
        cat_atomic_uint64_init(&cat_thread_last_id, CAT_THREAD_MAIN_ID + 1);
    }
    do {
        int error = uv_async_init(&CAT_EVENT_G(loop), &thread->notifier, cat_thread_async_callback);
        if (error != 0) {
            CAT_WARN_WITH_REASON(THREAD, error, "Thread async init failed");
            return cat_false;
        }
    } while (0);
    thread->notifier_refcount = 0;
    /* otherwise, it always make event loop hang */
    uv_unref((uv_handle_t *) &thread->notifier);

    if (thread->parent != NULL) {
        cat_bool_t ret = cat_thread_dispatch_task(thread->parent, cat_thread_ready_notify_task, thread);
        if (!ret) {
            CAT_WARN_WITH_LAST(THREAD, "Thread ready notify task dispatch failed");
            return cat_false;
        }
    }

    return cat_true;
}

CAT_API cat_bool_t cat_thread_runtime_shutdown(void)
{
    cat_thread_t *thread = cat_thread_get_current();

    CAT_ASSERT(cat_queue_empty(&thread->tasks));

    CAT_ASSERT(thread->notifier_refcount == 0);
    uv_ref((uv_handle_t *) &thread->notifier);
    uv_close((uv_handle_t *) &thread->notifier, NULL);
    if (thread->id == CAT_THREAD_MAIN_ID) {
        CAT_ASSERT(cat_atomic_uint32_load(&cat_thread_count) == 1);
        cat_atomic_uint64_destroy(&cat_thread_last_id);
        cat_atomic_uint32_destroy(&cat_thread_count);
    }

    return cat_true;
}

#endif /* CAT_THREAD */
