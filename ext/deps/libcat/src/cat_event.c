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

struct cat_event_shutdown_task_s {
    cat_queue_node_t node;
    cat_data_callback_t callback;
    cat_data_t *data;
};

struct cat_event_loop_defer_task_s {
    cat_queue_node_t node;
    cat_event_loop_defer_callback_t callback;
    cat_data_t *data;
    cat_bool_t allocated;
    /* this can be optimized by cat_queue_empty(),
     * but it's not necessarily. */
    cat_bool_t cancelable;
    cat_event_round_t round;
};

struct cat_event_io_defer_task_s {
    cat_queue_node_t node;
    cat_event_io_defer_callback_t callback;
    cat_data_t *data;
    cat_bool_t allocated;
    /** @see: same with event_defer_task_t. */
    cat_bool_t cancelable;
};

CAT_API CAT_GLOBALS_DECLARE(cat_event);

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
    cat_queue_init(&CAT_EVENT_G(defer_tasks));
    cat_queue_init(&CAT_EVENT_G(io_defer_tasks));

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
    CAT_ASSERT(cat_queue_empty(&CAT_EVENT_G(runtime_shutdown_tasks)));
    CAT_ASSERT(cat_queue_empty(&CAT_EVENT_G(defer_tasks)));
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

static int cat_event_alive_callback(uv_loop_t *loop)
{
    (void) loop;
    return !cat_queue_empty(&CAT_EVENT_G(defer_tasks)) ||
           !cat_queue_empty(&CAT_EVENT_G(io_defer_tasks));
}

static void cat_event_do_defer_tasks(void)
{
    cat_event_round_t current_round = CAT_EVENT_G(loop).round;
    cat_queue_t *tasks = &CAT_EVENT_G(defer_tasks);
    cat_event_loop_defer_task_t *task;

    /* execute tasks of current round */
    while ((task = cat_queue_front_data(tasks, cat_event_loop_defer_task_t, node)) != NULL) {
        if (task->round == current_round) {
            /* must be triggered in the next round */
            break;
        }
        cat_queue_remove(&task->node);
        task->cancelable = cat_false;
        task->callback(task, task->data);
        /* note: do not access the task anymore,
         * it may be free'd in callback. */
    }
}

static void cat_event_do_io_defer_tasks(void)
{
    cat_queue_t *tasks = &CAT_EVENT_G(io_defer_tasks);
    cat_event_io_defer_task_t *task;

    /* execute tasks of current round */
    while ((task = cat_queue_front_data(tasks, cat_event_io_defer_task_t, node)) != NULL) {
        cat_queue_remove(&task->node);
        task->cancelable = cat_false;
        task->callback(task, task->data);
        /* note: do not access the task anymore,
         * it may be free'd in callback. */
    }
}

static void cat_event_loop_defer_callback(uv_loop_t *loop)
{
    (void) loop;
    cat_event_do_defer_tasks();
}

static void cat_event_io_defer_callback(uv_loop_t *loop)
{
    (void) loop;
    cat_event_do_io_defer_tasks();
}

CAT_API void cat_event_schedule(void)
{
    const uv_run_options_t options = {
        cat_event_alive_callback,
        cat_event_loop_defer_callback,
        cat_event_io_defer_callback,
    };
    (void) uv_crun(&CAT_EVENT_G(loop), &options);
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

CAT_API cat_event_loop_defer_task_t *cat_event_loop_defer_task_create(
    cat_event_loop_defer_task_t *task,
    cat_event_loop_defer_callback_t callback,
    cat_data_t *data
) {
    if (task == NULL) {
        task = (cat_event_loop_defer_task_t *) cat_malloc_unrecoverable(sizeof(*task));
        task->allocated = cat_true;
    } else {
        task->allocated = cat_false;
    }
    task->callback = callback;
    task->data = data;
    task->cancelable = cat_true;
    task->round = CAT_EVENT_G(loop).round;
    cat_queue_push_back(&CAT_EVENT_G(defer_tasks), &task->node);
    return task;
}

CAT_API void cat_event_loop_defer_task_close(cat_event_loop_defer_task_t *task)
{
    if (task->cancelable) {
        cat_queue_remove(&task->node);
    }
    if (task->allocated) {
        cat_free(task);
    }
}

CAT_API cat_event_io_defer_task_t *cat_event_io_defer_task_create(
    cat_event_io_defer_task_t *task,
    cat_event_io_defer_callback_t callback,
    cat_data_t *data
) {
    if (task == NULL) {
        task = (cat_event_io_defer_task_t *) cat_malloc_unrecoverable(sizeof(*task));
        task->allocated = cat_true;
    } else {
        task->allocated = cat_false;
    }
    task->callback = callback;
    task->data = data;
    task->cancelable = cat_true;
    cat_queue_push_back(&CAT_EVENT_G(io_defer_tasks), &task->node);
    return task;
}

CAT_API void cat_event_io_defer_task_close(cat_event_io_defer_task_t *task)
{
    if (task->cancelable) {
        cat_queue_remove(&task->node);
    }
    if (task->allocated) {
        cat_free(task);
    }
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
