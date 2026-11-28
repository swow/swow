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
#include "cat_coroutine.h"

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

#ifdef CAT_THREAD_SAFE
static void cat_event_wakeup_callback(uv_async_t *handle)
{
    (void) handle;

    cat_coroutine_run_wakeups();
}
#endif

CAT_API cat_bool_t cat_event_runtime_init(void)
{
    int error;

    error = uv_loop_init(&CAT_EVENT_G(loop));

    if (error != 0) {
        CAT_WARN_WITH_REASON(EVENT, error, "Event loop init failed");
        return cat_false;
    }

    /* Event loop utilization: two clock reads around each poll, as Node.js does
     * by default. It has to be set before the loop first runs. */
    (void) uv_loop_configure(&CAT_EVENT_G(loop), UV_METRICS_IDLE_TIME);

    cat_queue_init(&CAT_EVENT_G(runtime_shutdown_tasks));
    cat_queue_init(&CAT_EVENT_G(io_defer_tasks));
    CAT_EVENT_G(delay_monitor) = NULL;

#ifdef CAT_THREAD_SAFE
    /* One wakeup handle for each thread. Another thread uses it to bring this
     * loop back from its poll, which is the only libuv call that is safe from
     * another thread. */
    do {
        uv_async_t *wakeup = &CAT_EVENT_G(wakeup);
        (void) uv_async_init(&CAT_EVENT_G(loop), wakeup, cat_event_wakeup_callback);
        /* The handle wakes this loop. It must not keep the loop alive, or the
         * scheduler never finishes. */
        uv_unref((uv_handle_t *) wakeup);
    } while (0);

    /* A waker on another thread needs this handle. Keep it here, because the
     * waker cannot compute the address from its own thread. */
    if (!cat_coroutine_wakeup_register(&CAT_EVENT_G(wakeup))) {
        return cat_false;
    }
#endif

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

    if (CAT_EVENT_G(delay_monitor) != NULL) {
        (void) cat_event_delay_monitor_stop();
    }

    /* we must call run to close all handles and clear defer tasks */
    cat_event_schedule();

    uv_close((uv_handle_t *) &CAT_EVENT_G(io_defer_check), NULL);
#ifdef CAT_THREAD_SAFE
    uv_close((uv_handle_t *) &CAT_EVENT_G(wakeup), NULL);
#endif

    /* A close only finishes when the loop runs again, and an unfinished close
     * makes uv_loop_close fail with EBUSY. */
    cat_event_schedule();

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
        if (error == CAT_EBUSY) {
            uv_print_all_handles(&CAT_EVENT_G(loop), CAT_LOG_G(error_output));
        }
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

/* observability */

CAT_API void cat_event_get_metrics(cat_event_metrics_t *metrics)
{
    uv_loop_t *loop = &CAT_EVENT_G(loop);
    uv_metrics_t uv_metrics;

    memset(metrics, 0, sizeof(*metrics));
    metrics->round = loop->round;
    if (uv_metrics_info(loop, &uv_metrics) == 0) {
        metrics->loop_count = uv_metrics.loop_count;
        metrics->events = uv_metrics.events;
        metrics->events_waiting = uv_metrics.events_waiting;
    }
    metrics->idle_time_ns = uv_metrics_idle_time(loop);
    metrics->active_handles = loop->active_handles;
    metrics->active_requests = loop->active_reqs.count;
}

static void cat_event_count_handles_callback(uv_handle_t *handle, void *arg)
{
    uint32_t *counts = (uint32_t *) arg;

    if (handle->type > UV_UNKNOWN_HANDLE && handle->type < UV_HANDLE_TYPE_MAX && !uv_is_closing(handle)) {
        counts[handle->type]++;
    }
}

CAT_API void cat_event_count_handles(uint32_t counts[UV_HANDLE_TYPE_MAX])
{
    memset(counts, 0, sizeof(uint32_t) * UV_HANDLE_TYPE_MAX);
    uv_walk(&CAT_EVENT_G(loop), cat_event_count_handles_callback, counts);
}

struct cat_event_delay_monitor_s {
    uv_timer_t timer;
    uint64_t previous;
    cat_event_delay_stats_t stats;
};

static void cat_event_delay_monitor_reset(cat_event_delay_monitor_t *monitor)
{
    uint64_t resolution_ns = monitor->stats.resolution_ns;

    memset(&monitor->stats, 0, sizeof(monitor->stats));
    monitor->stats.resolution_ns = resolution_ns;
    monitor->stats.min_ns = UINT64_MAX;
}

static void cat_event_delay_monitor_callback(uv_timer_t *timer)
{
    cat_event_delay_monitor_t *monitor = cat_container_of(timer, cat_event_delay_monitor_t, timer);
    cat_event_delay_stats_t *stats = &monitor->stats;
    uint64_t now = uv_hrtime();
    uint64_t elapsed = now - monitor->previous;
    /* the part of the interval beyond the resolution is the delay */
    uint64_t delay = elapsed > stats->resolution_ns ? elapsed - stats->resolution_ns : 0;
    uint64_t us = delay / 1000;
    unsigned int bucket = 0;

    monitor->previous = now;
    while (us > 1 && bucket < CAT_EVENT_DELAY_BUCKET_COUNT - 1) {
        us >>= 1;
        bucket++;
    }
    stats->buckets[bucket]++;
    stats->count++;
    stats->sum_ns += delay;
    stats->last_ns = delay;
    if (delay < stats->min_ns) {
        stats->min_ns = delay;
    }
    if (delay > stats->max_ns) {
        stats->max_ns = delay;
    }
}

static void cat_event_delay_monitor_close_callback(uv_handle_t *handle)
{
    cat_event_delay_monitor_t *monitor = cat_container_of(handle, cat_event_delay_monitor_t, timer);
    cat_free(monitor);
}

CAT_API cat_bool_t cat_event_delay_monitor_start(cat_timeout_t resolution)
{
    cat_event_delay_monitor_t *monitor;

    if (unlikely(resolution <= 0)) {
        cat_update_last_error(CAT_EINVAL, "Resolution must be greater than 0");
        return cat_false;
    }
    if (unlikely(CAT_EVENT_G(delay_monitor) != NULL)) {
        cat_update_last_error(CAT_EMISUSE, "Event loop delay monitor is already running");
        return cat_false;
    }
    monitor = (cat_event_delay_monitor_t *) cat_malloc(sizeof(*monitor));
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(monitor == NULL)) {
        cat_update_last_error_of_syscall("Malloc for event loop delay monitor failed");
        return cat_false;
    }
#endif
    monitor->stats.resolution_ns = (uint64_t) resolution * 1000 * 1000;
    cat_event_delay_monitor_reset(monitor);
    (void) uv_timer_init(&CAT_EVENT_G(loop), &monitor->timer);
    uv_update_time(&CAT_EVENT_G(loop));
    monitor->previous = uv_hrtime();
    (void) uv_timer_start(&monitor->timer, cat_event_delay_monitor_callback, (uint64_t) resolution, (uint64_t) resolution);
    /* observing the loop must neither keep it alive nor hide a deadlock */
    uv_unref((uv_handle_t *) &monitor->timer);
    monitor->timer.flags |= UV_HANDLE_INTERNAL;
    CAT_EVENT_G(delay_monitor) = monitor;

    return cat_true;
}

CAT_API cat_bool_t cat_event_delay_monitor_stop(void)
{
    cat_event_delay_monitor_t *monitor = CAT_EVENT_G(delay_monitor);

    if (unlikely(monitor == NULL)) {
        cat_update_last_error(CAT_EMISUSE, "Event loop delay monitor is not running");
        return cat_false;
    }
    CAT_EVENT_G(delay_monitor) = NULL;
    uv_close((uv_handle_t *) &monitor->timer, cat_event_delay_monitor_close_callback);

    return cat_true;
}

CAT_API cat_bool_t cat_event_delay_monitor_is_running(void)
{
    return CAT_EVENT_G(delay_monitor) != NULL;
}

CAT_API cat_bool_t cat_event_delay_monitor_get_stats(cat_event_delay_stats_t *stats, cat_bool_t reset)
{
    cat_event_delay_monitor_t *monitor = CAT_EVENT_G(delay_monitor);

    if (unlikely(monitor == NULL)) {
        cat_update_last_error(CAT_EMISUSE, "Event loop delay monitor is not running");
        return cat_false;
    }
    *stats = monitor->stats;
    if (stats->count == 0) {
        stats->min_ns = 0;
    }
    if (reset) {
        cat_event_delay_monitor_reset(monitor);
    }

    return cat_true;
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
