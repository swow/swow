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

#include "cat_event.h"

#ifdef CAT_IDE_HELPER
#include "uv-common.h"
#else
#include "../deps/libuv/src/uv-common.h"
#endif

struct cat_event_shutdown_task_s {
    cat_queue_node_t node;
    cat_data_callback_t callback;
    cat_data_t *data;
};

struct cat_event_loop_defer_task_s {
    union {
        cat_data_t *data;
        uv_timer_t timer;
        uv_handle_t handle;
    } u;
    cat_event_loop_defer_callback_t callback;
};

struct cat_event_io_defer_task_s {
    cat_queue_node_t node;
    cat_event_io_defer_callback_t callback;
    cat_data_t *data;
};

CAT_API CAT_GLOBALS_DECLARE(cat_event);

static void cat_event_do_io_defer_tasks(uv_check_t *check);

CAT_API cat_bool_t cat_event_module_init(void)
{
    CAT_GLOBALS_REGISTER(cat_event);

    return cat_true;
}

CAT_API cat_bool_t cat_event_module_shutdown(void)
{
    CAT_GLOBALS_UNREGISTER(cat_event);

    return cat_true;
}

CAT_API cat_bool_t cat_event_runtime_init(void)
{
    int error;

    error = uv_loop_init(&CAT_EVENT_G(loop));

    if (error != 0) {
        CAT_WARN_WITH_REASON(EVENT, error, "Event loop init failed");
        return cat_false;
    }

    cat_queue_init(&CAT_EVENT_G(runtime_shutdown_tasks));
    cat_queue_init(&CAT_EVENT_G(io_defer_tasks));
    do {
        uv_check_t *check = &CAT_EVENT_G(io_defer_check);
        (void) uv_check_init(&CAT_EVENT_G(loop), check);
        (void) uv_check_start(check, cat_event_do_io_defer_tasks);
        uv_unref((uv_handle_t *) check);
        check->flags |= UV_HANDLE_INTERNAL;
    } while (0);

    return cat_true;
}

CAT_API cat_bool_t cat_event_runtime_shutdown(void)
{
    do {
        cat_queue_t *shutdown_tasks = &CAT_EVENT_G(runtime_shutdown_tasks);
        cat_event_shutdown_task_t *shutdown_task;
        /* call shutdown tasks */
        while ((shutdown_task = cat_queue_front_data(shutdown_tasks, cat_event_shutdown_task_t, node))) {
            cat_queue_remove(&shutdown_task->node);
            shutdown_task->callback(shutdown_task->data);
            cat_free(shutdown_task);
        }
    } while (0);

    /* we must call run to close all handles and clear defer tasks */
    cat_event_schedule();

    uv_close((uv_handle_t *) &CAT_EVENT_G(io_defer_check), NULL);

    CAT_ASSERT(cat_queue_empty(&CAT_EVENT_G(runtime_shutdown_tasks)));
    CAT_ASSERT(cat_queue_empty(&CAT_EVENT_G(io_defer_tasks)));

    return cat_true;
}

CAT_API cat_bool_t cat_event_runtime_close(void)
{
    int error;

    error = uv_loop_close(&CAT_EVENT_G(loop));

    if (unlikely(error != 0)) {
        CAT_WARN_WITH_REASON(EVENT, error, "Event loop close failed");
#ifdef CAT_DEBUG
        if (error == CAT_EBUSY) {
            uv_print_all_handles(&CAT_EVENT_G(loop), CAT_LOG_G(error_output));
        }
#endif
        return cat_false;
    }

    return cat_true;
}

CAT_API void cat_event_schedule(void)
{
    (void) uv_crun(&CAT_EVENT_G(loop));
}

CAT_API cat_event_round_t cat_event_get_round(void)
{
    return CAT_EVENT_G(loop).round;
}

CAT_API cat_coroutine_t *cat_event_scheduler_run(cat_coroutine_t *coroutine)
{
    const cat_coroutine_scheduler_t scheduler = {
        cat_event_schedule,
        NULL
    };

    return cat_coroutine_scheduler_run(coroutine, &scheduler);
}

CAT_API cat_coroutine_t *cat_event_scheduler_close(void)
{
    return cat_coroutine_scheduler_close();
}

CAT_API cat_event_shutdown_task_t *cat_event_register_runtime_shutdown_task(cat_event_shutdown_callback_t callback, cat_data_t *data)
{
    cat_event_shutdown_task_t *task = (cat_event_shutdown_task_t *) cat_malloc(sizeof(*task));

#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(task == NULL)) {
        cat_update_last_error_of_syscall("Malloc for runtime shutdown task failed");
        return NULL;
    }
#endif
    task->callback = callback;
    task->data = data;
    cat_queue_push_back(&CAT_EVENT_G(runtime_shutdown_tasks), &task->node);

    return task;
}

CAT_API void cat_event_unregister_runtime_shutdown_task(cat_event_shutdown_task_t *task)
{
    cat_queue_remove(&task->node);
    cat_free(task);
}

static void cat_event_loop_defer_task_callback(uv_timer_t *timer)
{
    cat_event_loop_defer_task_t *task = cat_container_of(timer, cat_event_loop_defer_task_t, u.timer);
    cat_data_t *data = task->u.data;
    task->u.data = NULL; // make others know it's running
    task->callback(task, data);
}

static void cat_event_loop_defer_task_free_callback(uv_handle_t *handle)
{
    cat_event_loop_defer_task_t *task = cat_container_of(handle, cat_event_loop_defer_task_t, u.handle);
    cat_free(task);
}

CAT_API cat_event_loop_defer_task_t *cat_event_loop_defer_task_create(
    cat_event_loop_defer_callback_t callback,
    cat_data_t *data
) {
    cat_event_loop_defer_task_t *task;
    task = (cat_event_loop_defer_task_t *) cat_malloc_unrecoverable(sizeof(*task));
    task->callback = callback;
    task->u.data = data;
    (void) uv_timer_init(&CAT_EVENT_G(loop), &task->u.timer);
    (void) uv_timer_start(&task->u.timer, cat_event_loop_defer_task_callback, 0, 0);
    return task;
}

CAT_API cat_bool_t cat_event_loop_defer_task_close(cat_event_loop_defer_task_t *task)
{
    cat_bool_t called = task->u.data == NULL;
    uv_close(&task->u.handle, cat_event_loop_defer_task_free_callback);
    return called;
}

static void cat_event_do_io_defer_tasks(uv_check_t *check)
{
    cat_queue_t *tasks = &CAT_EVENT_G(io_defer_tasks);
    cat_event_io_defer_task_t *task;

    (void) check;
    /* execute tasks of current round */
    while ((task = cat_queue_front_data(tasks, cat_event_io_defer_task_t, node)) != NULL) {
        cat_queue_remove(&task->node);
        task->callback(task, task->data);
        /* note: do not access the task anymore,
         * it may be free'd in callback. */
    }
}

CAT_API cat_event_io_defer_task_t *cat_event_io_defer_task_create(
    cat_event_io_defer_callback_t callback,
    cat_data_t *data
) {
    cat_event_io_defer_task_t *task = (cat_event_io_defer_task_t *) cat_malloc_unrecoverable(sizeof(*task));
    task->callback = callback;
    task->data = data;
    cat_queue_push_back(&CAT_EVENT_G(io_defer_tasks), &task->node);

    return task;
}

CAT_API cat_bool_t cat_event_io_defer_task_close(cat_event_io_defer_task_t *task)
{
    cat_bool_t called = cat_queue_empty(&task->node);
    if (!called) {
        cat_queue_remove(&task->node);
    }
    cat_free(task);
    return called;
}

CAT_API void cat_event_fork(void)
{
#ifndef CAT_COROUTINE_USE_THREAD_CONTEXT
    int error = uv_loop_fork(&CAT_EVENT_G(loop));

    if (error != 0) {
        CAT_CORE_ERROR_WITH_REASON(EVENT, error, "Event loop fork failed");
    }
#else
    CAT_ERROR(EVENT, "Function fork() is disabled for internal reasons when using thread-context");
#endif
}

static FILE *cat_event_get_fp_from_fd(cat_os_fd_t fd)
{
    FILE* fp;
    if (fd == CAT_STDOUT_FILENO) {
        fp = stdout;
    } else if (fd == CAT_STDERR_FILENO) {
        fp = stderr;
    } else {
        fp = stdout;
    }
    return fp;
}

CAT_API void cat_event_print_all_handles(cat_os_fd_t output)
{
    uv_print_all_handles(&CAT_EVENT_G(loop), cat_event_get_fp_from_fd(output));
}

CAT_API void cat_event_print_active_handles(cat_os_fd_t output)
{
    uv_print_active_handles(&CAT_EVENT_G(loop), cat_event_get_fp_from_fd(output));
}
