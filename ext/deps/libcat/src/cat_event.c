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

CAT_API CAT_GLOBALS_DECLARE(cat_event)

CAT_GLOBALS_CTOR_DECLARE_SZ(cat_event)

CAT_API cat_bool_t cat_event_module_init(void)
{
    int error;

    CAT_GLOBALS_REGISTER(cat_event, CAT_GLOBALS_CTOR(cat_event), NULL);

    CAT_EVENT_G(loop) = &CAT_EVENT_G(_loop);
    CAT_EVENT_G(dead_lock) = &CAT_EVENT_G(_dead_lock);

    error = uv_loop_init(CAT_EVENT_G(loop));

    if (error != 0) {
        cat_warn_with_reason(EVENT, error, "Event loop init failed");
        return cat_false;
    }

    cat_queue_init(&CAT_EVENT_G(defer_tasks));
    CAT_EVENT_G(defer_task_count) = 0;

    return cat_true;
}

CAT_API cat_bool_t cat_event_module_shutdown(void)
{
    int error;

    error = uv_loop_close(CAT_EVENT_G(loop));

    if (unlikely(error != 0)) {
        cat_warn_with_reason(EVENT, error, "Event loop close failed");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_event_runtime_init(void)
{
    return cat_true;
}

CAT_API cat_bool_t cat_event_runtime_shutdown(void)
{
    /* we must call run to close all handles and clear defer tasks */
    cat_event_schedule();

    CAT_ASSERT(cat_queue_empty(&CAT_EVENT_G(defer_tasks)));
    CAT_ASSERT(CAT_EVENT_G(defer_task_count) == 0);

    return cat_true;
}

static void cat_event_dead_lock_unlock(uv_timer_t *dead_lock)
{
    uv_timer_stop(dead_lock);
    uv_close((uv_handle_t *) dead_lock, NULL);
}

static void cat_event_dead_lock_unlock_callback(cat_data_t *data)
{
    cat_event_dead_lock_unlock((uv_timer_t *) data);
}

static void cat_event_dead_lock_callback(uv_timer_t *dead_lock)
{
    uv_timer_again(dead_lock);
}

CAT_API void cat_event_dead_lock(void)
{
    uv_timer_t *dead_lock = CAT_EVENT_G(dead_lock);
    int error;

    error = uv_timer_init(cat_event_loop, dead_lock);

    if (unlikely(error != 0)) {
        cat_core_error_with_reason(EVENT, error, "Dead-Lock init failed");
    }

    error = uv_timer_start(dead_lock, cat_event_dead_lock_callback, UINT64_MAX, UINT64_MAX);

    if (unlikely(error != 0)) {
        cat_core_error_with_reason(EVENT, error, "Dead-Lock start failed");
    }

    if (!cat_event_defer(cat_event_dead_lock_unlock_callback, dead_lock)) {
        cat_core_error_with_last(EVENT, "Dead-Lock defer failed");
    }
}

CAT_API void cat_event_schedule(void)
{
    uv_loop_t *loop = CAT_EVENT_G(loop);

    while (1) {
        cat_bool_t alive;

        alive = uv_crun(loop);

        /* if we have unfinished tasks, continue to loop  */
        alive = cat_event_do_defer_tasks() || alive;

        if (!alive) {
            break;
        }
    }
}

CAT_API cat_coroutine_t *cat_event_scheduler_run(cat_coroutine_t *coroutine)
{
    const cat_coroutine_scheduler_t scheduler = {
        cat_event_schedule,
        cat_event_dead_lock
    };

    return cat_coroutine_scheduler_run(coroutine, &scheduler);
}

CAT_API cat_coroutine_t *cat_event_scheduler_close(void)
{
    return cat_coroutine_scheduler_close();
}

CAT_API cat_bool_t cat_event_defer(cat_data_callback_t callback, cat_data_t *data)
{
    cat_event_task_t *task = (cat_event_task_t *) cat_malloc(sizeof(*task));

    if (unlikely(task == NULL)) {
        cat_update_last_error_of_syscall("Malloc for defer task failed");
        return cat_false;
    }
    task->callback = callback;
    task->data = data;
    cat_queue_push_back(&CAT_EVENT_G(defer_tasks), &task->node);
    CAT_EVENT_G(defer_task_count)++;

    return cat_true;
}

CAT_API cat_bool_t cat_event_do_defer_tasks(void)
{
    cat_queue_t *tasks = &CAT_EVENT_G(defer_tasks);
    cat_event_task_t *task;
    /* only tasks of the current round will be called */
    size_t count = CAT_EVENT_G(defer_task_count);

    while (count--) {
        task = cat_queue_front_data(tasks, cat_event_task_t, node);
        cat_queue_remove(&task->node);
        CAT_EVENT_G(defer_task_count)--;
        task->callback(task->data);
        cat_free(task);
    }

    return CAT_EVENT_G(defer_task_count) > 0;
}

CAT_API cat_bool_t cat_event_wait(void)
{
    return cat_coroutine_wait();
}
