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

#include "cat_watchdog.h"

CAT_API CAT_GLOBALS_DECLARE(cat_watchdog)

CAT_GLOBALS_CTOR_DECLARE_SZ(cat_watchdog)

static cat_timeout_t cat_watchdog_align_quantum(cat_timeout_t quantum)
{
    if (quantum <= 0) {
        quantum = CAT_WATCH_DOG_DEFAULT_QUANTUM;
    }

    return quantum;
}

static cat_timeout_t cat_watchdog_align_threshold(cat_timeout_t threshold)
{
    if (threshold == 0) {
        threshold = CAT_WATCH_DOG_DEFAULT_THRESHOLD;
    } else if (threshold < 0) {
        threshold = CAT_WATCH_DOG_THRESHOLD_DISABLED;
    }

    return threshold;
}

#ifdef CAT_OS_WIN
// in default, Windows timer slice is about 15.6ms
// this function will try reduce it
static void cat_improve_timer_resolution(void)
{
    static NTSTATUS (NTAPI *NtSetTimerResolution)(
        IN ULONG                DesiredResolution,
        IN BOOLEAN              SetResolution,
        OUT PULONG              CurrentResolution
    ) = NULL;
    // prove NtSetTimerResolution
    if (NULL == NtSetTimerResolution) {
        if (NULL == (NtSetTimerResolution =
            (NTSTATUS (NTAPI *)(ULONG, BOOLEAN, PULONG)) GetProcAddress(
                GetModuleHandleW(L"ntdll.dll"), "NtSetTimerResolution"))) {
            return;
        }
    }
    // TODO: get current at first called, record it, then recover it
    ULONG dummy;
    (void) NtSetTimerResolution(1 /* 1us, will be upscaled to minimum */, TRUE, &dummy);
}
#endif

static void cat_watchdog_loop(void* arg)
{
    cat_watchdog_t *watchdog = (cat_watchdog_t *) arg;

#ifdef CAT_OS_WIN
    cat_improve_timer_resolution();
#endif

    uv_sem_post(watchdog->sem);

    while (1) {
        watchdog->last_round = watchdog->globals->round;
        uv_mutex_lock(&watchdog->mutex);
        uv_cond_timedwait(&watchdog->cond, &watchdog->mutex, watchdog->quantum);
        uv_mutex_unlock(&watchdog->mutex);
        if (watchdog->stop) {
            return;
        }
        /* Notice: globals info maybe changed during check,
         * but it is usually acceptable to us.
         * In other words, there is a certain probability of false alert. */
        if (watchdog->globals->round == watchdog->last_round &&
            watchdog->globals->current != watchdog->globals->scheduler &&
            watchdog->globals->count > 1
        ) {
            watchdog->alert_count++;
            watchdog->alerter(watchdog);
        } else {
            watchdog->alert_count = 0;
        }
    }
}

CAT_API cat_bool_t cat_watchdog_module_init(void)
{
    CAT_GLOBALS_REGISTER(cat_watchdog, CAT_GLOBALS_CTOR(cat_watchdog), NULL);
    return cat_true;
}

CAT_API cat_bool_t cat_watchdog_runtime_init(void)
{
    CAT_WATCH_DOG_G(watchdog) = NULL;

    return cat_true;
}

CAT_API cat_bool_t cat_watchdog_runtime_shutdown(void)
{
    if (cat_watchdog_is_running() && !cat_watchdog_stop()) {
        CAT_CORE_ERROR(WATCH_DOG, "WatchDog close failed during rshutdown");
    }

    return cat_true;
}

CAT_API void cat_watchdog_alert_standard(cat_watchdog_t *watchdog)
{
    fprintf(stderr, "Warning: <WatchDog> Syscall blocking or CPU starvation may occur in " CAT_WATCH_DOG_ROLE_NAME " %d, "
                    "it has been blocked for more than " CAT_TIMEOUT_FMT  " ns" CAT_EOL,
                    watchdog->pid, watchdog->quantum * watchdog->alert_count);
}

CAT_API cat_bool_t cat_watchdog_run(cat_watchdog_t *watchdog, cat_timeout_t quantum, cat_timeout_t threshold, cat_watchdog_alerter_t alerter)
{
    uv_thread_options_t options;
    uv_sem_t sem;
    int error;

    if (CAT_WATCH_DOG_G(watchdog) != NULL) {
        cat_update_last_error(CAT_EMISUSE, "Only one watchdog is allowed to run per " CAT_WATCH_DOG_ROLE_NAME);
        return cat_false;
    }

    if (watchdog == NULL) {
        watchdog = (cat_watchdog_t *) cat_malloc(sizeof(*watchdog));
#if CAT_ALLOC_HANDLE_ERRORS
        if (watchdog == NULL) {
            cat_update_last_error_of_syscall("Malloc for watchdog failed");
            return cat_false;
        }
#endif
        watchdog->allocated = cat_true;
    } else {
        watchdog->allocated = cat_false;
    }

    watchdog->quantum = cat_watchdog_align_quantum(quantum);
    watchdog->threshold = cat_watchdog_align_threshold(threshold);
    watchdog->alerter = alerter != NULL ? alerter : cat_watchdog_alert_standard;
    watchdog->alert_count = 0;
    watchdog->stop = cat_false;
    watchdog->pid = uv_os_getpid();
    watchdog->globals = CAT_GLOBALS_BULK(cat_coroutine);
    watchdog->last_round = 0;

    error = uv_sem_init(&sem, 0);
    if (error != 0) {
        cat_update_last_error_with_reason(error, "WatchDog init sem failed");
        goto _sem_init_failed;
    }
    error = uv_cond_init(&watchdog->cond);
    if (error != 0) {
        cat_update_last_error_with_reason(error, "WatchDog init cond failed");
        goto _cond_init_failed;
    }
    error = uv_mutex_init(&watchdog->mutex);
    if (error != 0) {
        cat_update_last_error_with_reason(error, "WatchDog init mutex failed");
        goto _mutex_init_failed;
    }

    watchdog->sem = &sem;
    options.flags = UV_THREAD_HAS_STACK_SIZE;
    options.stack_size = CAT_COROUTINE_RECOMMENDED_STACK_SIZE;

    error = uv_thread_create_ex(&watchdog->thread, &options, cat_watchdog_loop, watchdog);

    if (error != 0) {
        cat_update_last_error_with_reason(error, "WatchDog create thread failed");
        goto _thread_create_failed;
    }

    CAT_WATCH_DOG_G(watchdog) = watchdog;

    uv_sem_wait(&sem);
    uv_sem_destroy(&sem);
    watchdog->sem = NULL;

    return cat_true;

    _thread_create_failed:
    uv_mutex_destroy(&watchdog->mutex);
    _mutex_init_failed:
    uv_cond_destroy(&watchdog->cond);
    _cond_init_failed:
    uv_sem_destroy(&sem);
    _sem_init_failed:
    if (watchdog->allocated) {
        cat_free(watchdog);
    }
    return cat_false;
}

CAT_API cat_bool_t cat_watchdog_stop(void)
{
    cat_watchdog_t *watchdog = CAT_WATCH_DOG_G(watchdog);
    int error;

    if (watchdog == NULL) {
        cat_update_last_error(CAT_EMISUSE, "WatchDog is not running");
        return cat_false;
    }

    watchdog->stop = cat_true;
    uv_mutex_lock(&watchdog->mutex);
    uv_cond_signal(&watchdog->cond);
    uv_mutex_unlock(&watchdog->mutex);

    error = uv_thread_join(&watchdog->thread);

    if (error != 0) {
        cat_update_last_error_with_reason(error, "WatchDog close thread failed");
        return cat_false;
    }

    uv_mutex_destroy(&watchdog->mutex);
    uv_cond_destroy(&watchdog->cond);
    CAT_ASSERT(watchdog->sem == NULL);

    if (watchdog->allocated) {
        cat_free(watchdog);
    }

    CAT_WATCH_DOG_G(watchdog) = NULL;

    return cat_true;
}

CAT_API cat_bool_t cat_watchdog_is_running(void)
{
    return CAT_WATCH_DOG_G(watchdog) != NULL;
}

CAT_API cat_timeout_t cat_watchdog_get_quantum(void)
{
    cat_watchdog_t *watchdog = CAT_WATCH_DOG_G(watchdog);

    return watchdog != NULL ?
            watchdog->quantum :
            -1;
}

CAT_API cat_timeout_t cat_watchdog_get_threshold(void)
{
    cat_watchdog_t *watchdog = CAT_WATCH_DOG_G(watchdog);

    return watchdog != NULL ?
            watchdog->threshold :
            -1;
}
