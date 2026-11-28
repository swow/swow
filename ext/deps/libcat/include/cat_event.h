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

#ifndef CAT_EVENT_H
#define CAT_EVENT_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#include "cat_coroutine.h"

typedef uint64_t cat_event_round_t;
#define CAT_EVENT_ROUND_FMT "%" PRIu64
#define CAT_EVENT_ROUND_FMT_SPEC PRIu64

typedef cat_data_callback_t cat_event_shutdown_callback_t;
typedef struct cat_event_shutdown_task_s cat_event_shutdown_task_t;

typedef struct cat_event_loop_defer_task_s cat_event_loop_defer_task_t;
typedef void (*cat_event_loop_defer_callback_t)(cat_event_loop_defer_task_t *task, cat_data_t *data);

typedef struct cat_event_io_defer_task_s cat_event_io_defer_task_t;
typedef void (*cat_event_io_defer_callback_t)(cat_event_io_defer_task_t *task, cat_data_t *data);

/* Node.js monitorEventLoopDelay: how late a repeating timer fires */
#define CAT_EVENT_DELAY_BUCKET_COUNT 64

typedef struct cat_event_delay_stats_s {
    uint64_t resolution_ns;
    uint64_t count;
    uint64_t min_ns;
    uint64_t max_ns;
    uint64_t sum_ns;
    uint64_t last_ns;
    /* bucket i counts delays in [2^i, 2^(i+1)) microseconds, bucket 0 also takes < 1us */
    uint64_t buckets[CAT_EVENT_DELAY_BUCKET_COUNT];
} cat_event_delay_stats_t;

typedef struct cat_event_delay_monitor_s cat_event_delay_monitor_t;

typedef struct cat_event_metrics_s {
    uint64_t round;
    /* uv_metrics_info() */
    uint64_t loop_count;
    uint64_t events;
    uint64_t events_waiting;
    /* time spent waiting in poll, cumulative */
    uint64_t idle_time_ns;
    uint32_t active_handles;
    uint32_t active_requests;
} cat_event_metrics_t;

CAT_GLOBALS_STRUCT_BEGIN(cat_event) {
    uv_loop_t loop;
    uv_timer_t deadlock;
#ifdef CAT_THREAD_SAFE
    uv_async_t wakeup;
#endif
    cat_queue_t runtime_shutdown_tasks;
    cat_queue_t io_defer_tasks;
    uv_check_t io_defer_check;
    cat_event_delay_monitor_t *delay_monitor;
} CAT_GLOBALS_STRUCT_END(cat_event);

extern CAT_API CAT_GLOBALS_DECLARE(cat_event);

#define CAT_EVENT_G(x) CAT_GLOBALS_GET(cat_event, x)

CAT_API cat_bool_t cat_event_module_init(void);
CAT_API cat_bool_t cat_event_module_shutdown(void);
CAT_API cat_bool_t cat_event_runtime_init(void);
CAT_API cat_bool_t cat_event_runtime_shutdown(void);
CAT_API cat_bool_t cat_event_runtime_close(void);

CAT_API void cat_event_schedule(void)  CAT_INTERNAL;

CAT_API cat_event_round_t cat_event_get_round(void);

/* observability */
CAT_API void cat_event_get_metrics(cat_event_metrics_t *metrics);
/* counts[type] for every uv_handle_type, walks the loop, call it on request only */
CAT_API void cat_event_count_handles(uint32_t counts[UV_HANDLE_TYPE_MAX]);
CAT_API cat_bool_t cat_event_delay_monitor_start(cat_timeout_t resolution);
CAT_API cat_bool_t cat_event_delay_monitor_stop(void);
CAT_API cat_bool_t cat_event_delay_monitor_is_running(void);
/* copies the statistics, and starts them over when reset is true */
CAT_API cat_bool_t cat_event_delay_monitor_get_stats(cat_event_delay_stats_t *stats, cat_bool_t reset);

CAT_API cat_coroutine_t *cat_event_scheduler_run(cat_coroutine_t *coroutine);
CAT_API cat_coroutine_t *cat_event_scheduler_close(void);

/* Note: we should not call any event related APIs in shutdown task */
CAT_API cat_event_shutdown_task_t *cat_event_register_runtime_shutdown_task(cat_event_shutdown_callback_t callback, cat_data_t *data);
/* Note: it can only be called before shutdown. */
CAT_API void cat_event_unregister_runtime_shutdown_task(cat_event_shutdown_task_t *task);

/* defer task callbacks will be called in the current_round + 1 event loop,
 * it's useful to free memory later safely.  */
CAT_API cat_event_loop_defer_task_t *cat_event_loop_defer_task_create(
    cat_event_loop_defer_callback_t callback,
    cat_data_t *data
);
/** tasks that have not yet run will be canceled,
 * if task callback has been called, it will return true, otherwise false */
CAT_API cat_bool_t cat_event_loop_defer_task_close(cat_event_loop_defer_task_t *task);

/* io defer task callbacks will be called after all io events in the current_round,
 * it's useful to merge multiple io events.  */
CAT_API cat_event_io_defer_task_t *cat_event_io_defer_task_create(
    cat_event_io_defer_callback_t callback,
    cat_data_t *data
);
/** tasks that have not yet run will be canceled,
 * if task callback has been called, it will return true, otherwise false. */
CAT_API cat_bool_t cat_event_io_defer_task_close(cat_event_io_defer_task_t *task);

CAT_API void cat_event_fork(void);

CAT_API void cat_event_print_all_handles(cat_os_fd_t output);
CAT_API void cat_event_print_active_handles(cat_os_fd_t output);

#ifdef __cplusplus
}
#endif
#endif /* CAT_EVENT_H */
