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

CAT_API CAT_GLOBALS_DECLARE(cat_event)

CAT_GLOBALS_CTOR_DECLARE_SZ(cat_event)

static void cat_event_run(uv_loop_t *loop);
static cat_bool_t cat_event_do_defer_tasks(void);

CAT_API cat_bool_t cat_event_module_init(void)
{
    CAT_GLOBALS_REGISTER(cat_event, CAT_GLOBALS_CTOR(cat_event), NULL);
    return cat_true;
}

CAT_API cat_bool_t cat_event_runtime_init(void)
{
    int error;

    CAT_EVENT_G(running) = cat_false;
    CAT_EVENT_G(loop) = &CAT_EVENT_G(_loop);
    CAT_EVENT_G(dead_lock) = &CAT_EVENT_G(_dead_lock);

    do {
        uv_loop_t *loop = CAT_EVENT_G(loop);

        error = uv_loop_init(loop);

        if (unlikely(error != 0)) {
            cat_core_error_with_reason(EVENT, error, "Event loop init failed");
        }
    } while (0);

    cat_queue_init(&CAT_EVENT_G(defer_tasks));

    return cat_true;
}

CAT_API cat_bool_t cat_event_runtime_shutdown(void)
{
    int error;

    do {
        uv_loop_t *loop = CAT_EVENT_G(loop);

        if (unlikely(loop == NULL)) {
            break;
        }

        /* we must call run to close all handles and clear defer tasks */
        cat_event_run(loop);

        error = uv_loop_close(loop);
        if (unlikely(error != 0)) {
            cat_warn_with_reason(EVENT, error, "Event loop close failed");
        }
    } while (0);

    return cat_true;
}

CAT_API cat_bool_t cat_event_is_running(void)
{
    return CAT_EVENT_G(running);
}

static void cat_event_dead_lock_callback(cat_data_t *data)
{
    uv_timer_stop((uv_timer_t *) data);
    uv_close((uv_handle_t *) data, NULL);
}

static void cat_event_forever_timer_callback(uv_timer_t *forever_timer)
{
    uv_timer_again(forever_timer);
}

static cat_never_inline void cat_event_dead_lock(void)
{
    uv_timer_t *dead_lock = CAT_EVENT_G(dead_lock);
    int error;

    cat_warn_ex(EVENT, CAT_EDEADLK, "Dead lock: all coroutines are asleep");

    error = uv_timer_init(cat_event_loop, dead_lock);
    if (unlikely(error != 0)) {
        cat_core_error_with_reason(EVENT, error, "Dead-Lock init failed");
    }
    error = uv_timer_start(dead_lock, cat_event_forever_timer_callback, UINT64_MAX, UINT64_MAX);
    if (unlikely(error != 0)) {
        cat_core_error_with_reason(EVENT, error, "Dead-Lock start failed");
    }
    if (!cat_event_defer(cat_event_dead_lock_callback, dead_lock)) {
        cat_core_error_with_last(EVENT, "Dead-Lock defer failed");
    }
}

static void cat_event_run(uv_loop_t *loop)
{
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

CAT_API cat_data_t *cat_event_scheduler_function(cat_data_t *data)
{
    uv_loop_t *loop = CAT_EVENT_G(loop);
    cat_coroutine_t *coroutine = NULL;

    if (CAT_EVENT_G(running)) {
        cat_update_last_error(CAT_EMISUSE, "Event loop is running");
        return CAT_COROUTINE_DATA_ERROR;
    }
    if (loop == NULL) {
        cat_core_error(EVENT, "Event loop is not available");
    }

    CAT_EVENT_G(running) = cat_true;

    /* exchange */
    coroutine = cat_coroutine_exchange_with_previous();
    /* run event loop */
    do {
        cat_event_run(loop);
        if (unlikely(!cat_coroutine_is_locked(coroutine))) {
            cat_event_dead_lock();
        } else {
            if (unlikely(!cat_coroutine_unlock(coroutine))) {
                cat_core_error_with_last(EVENT, "Unlock previous coroutine failed");
            }
        }
    } while (CAT_COROUTINE_G(current)->previous == NULL); /* is_root */

    CAT_EVENT_G(running) = cat_false;

    return CAT_COROUTINE_DATA_NULL;
}

CAT_API cat_bool_t cat_event_scheduler_run(void)
{
   cat_coroutine_t *scheduler;

    do {
        cat_coroutine_id_t last_id = CAT_COROUTINE_G(last_id);
        CAT_COROUTINE_G(last_id) = 0;
        scheduler = cat_coroutine_create(NULL, cat_event_scheduler_function);
        CAT_COROUTINE_G(last_id) = last_id;
    } while (0);
    if (scheduler == NULL) {
        cat_update_last_error_with_previous("Create event scheduler failed");
        return cat_false;
    }
    if (!cat_coroutine_scheduler_run(scheduler)) {
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_event_scheduler_stop(void)
{
    return cat_coroutine_scheduler_stop() != NULL;
}

CAT_API void cat_event_wait(void)
{
    /* usually, it will be unlocked by event scheduler */
    cat_coroutine_lock();

    /* we expect everything is over,
     * but there are still coroutines that have not finished
     * so we try to trigger the dead lock
    */
    while (unlikely(CAT_COROUTINE_G(active_count) != 2 /* scheduler + main */)) {
        cat_coroutine_yield_ez();
    }
    /* dead lock was broken by some magic ways (we use while loop to fix it now, but we do not know if it is right) */
}

static cat_bool_t cat_event_do_defer_tasks(void)
{
    cat_queue_t *tasks = &CAT_EVENT_G(defer_tasks);
    cat_event_task_t *task;
    /* only tasks of the current round will be called */
    size_t count = CAT_EVENT_G(defer_tasks_count);

    while (count--) {
        task = cat_queue_front_data(tasks, cat_event_task_t, node);
        cat_queue_remove(&task->node);
        CAT_EVENT_G(defer_tasks_count)--;
        task->callback(task->data);
        cat_free(task);
    }

    return CAT_EVENT_G(defer_tasks_count) > 0;
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
    CAT_EVENT_G(defer_tasks_count)++;

    return cat_true;
}
