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

CAT_GLOBALS_STRUCT_BEGIN(cat_event) {
    uv_loop_t loop;
    uv_timer_t deadlock;
    cat_queue_t runtime_shutdown_tasks;
    cat_queue_t io_defer_tasks;
    uv_check_t io_defer_check;
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
