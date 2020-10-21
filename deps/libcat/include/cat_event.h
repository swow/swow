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

typedef struct
{
    cat_queue_node_t node;
    cat_data_callback_t callback;
    cat_data_t *data;
} cat_event_task_t;

CAT_GLOBALS_STRUCT_BEGIN(cat_event)
    uv_loop_t *loop;
    uv_timer_t *dead_lock;
    cat_queue_t defer_tasks;
    size_t defer_task_count;
    /* --- */
    uv_loop_t _loop;
    uv_timer_t _dead_lock;
CAT_GLOBALS_STRUCT_END(cat_event)

extern CAT_API CAT_GLOBALS_DECLARE(cat_event)

#define CAT_EVENT_G(x) CAT_GLOBALS_GET(cat_event, x)

#define cat_event_loop CAT_EVENT_G(loop)

CAT_API cat_bool_t cat_event_module_init(void);
CAT_API cat_bool_t cat_event_runtime_init(void);
CAT_API cat_bool_t cat_event_runtime_shutdown(void);

CAT_API void cat_event_schedule(void)  CAT_INTERNAL;
CAT_API void cat_event_dead_lock(void) CAT_INTERNAL;

CAT_API cat_coroutine_t *cat_event_scheduler_run(cat_coroutine_t *coroutine);
CAT_API cat_coroutine_t *cat_event_scheduler_close(void);

CAT_API cat_bool_t cat_event_defer(cat_data_callback_t callback, cat_data_t *data);
CAT_API cat_bool_t cat_event_do_defer_tasks(void);

CAT_API cat_bool_t cat_event_wait(void);

#ifdef __cplusplus
}
#endif
#endif /* CAT_EVENT_H */
