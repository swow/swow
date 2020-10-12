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

#include "cat_watch_dog.h"

CAT_API CAT_GLOBALS_DECLARE(cat_watch_dog)

CAT_GLOBALS_CTOR_DECLARE_SZ(cat_watch_dog)

static cat_nsec_t cat_watch_dog_align_quantum(cat_nsec_t quantum)
{
    if (quantum <= 0) {
        quantum = CAT_WATCH_DOG_DEFAULT_QUANTUM;
    }

    return quantum;
}

static cat_nsec_t cat_watch_dog_align_threshold(cat_nsec_t threshold)
{
    if (threshold == 0) {
        threshold = CAT_WATCH_DOG_DEFAULT_THRESHOLD;
    } else if (threshold < 0) {
        threshold = CAT_WATCH_DOG_THRESHOLD_DISABLED;
    }

    return threshold;
}

static void cat_watch_dog_loop(void* arg)
{
    cat_watch_dog_t *watch_dog = (cat_watch_dog_t *) arg;

    uv_sem_post(watch_dog->sem);

    while (1) {
        watch_dog->last_round = watch_dog->globals->round;
        uv_mutex_lock(&watch_dog->mutex);
        uv_cond_timedwait(&watch_dog->cond, &watch_dog->mutex, watch_dog->quantum);
        uv_mutex_unlock(&watch_dog->mutex);
        if (watch_dog->stop) {
            return;
        }
        /* Notice: globals info maybe changed during check,
         * but it is usually acceptable to us.
         * In other words, there is a certain probability of false alert. */
        if (watch_dog->globals->round == watch_dog->last_round &&
            watch_dog->globals->current != watch_dog->globals->scheduler &&
            watch_dog->globals->count > 1
        ) {
            watch_dog->alert_count++;
            watch_dog->alerter(watch_dog);
        } else {
            watch_dog->alert_count = 0;
        }
    }
}

CAT_API cat_bool_t cat_watch_dog_module_init(void)
{
    CAT_GLOBALS_REGISTER(cat_watch_dog, CAT_GLOBALS_CTOR(cat_watch_dog), NULL);
    return cat_true;
}

CAT_API cat_bool_t cat_watch_dog_runtime_init(void)
{
    CAT_WATCH_DOG_G(watch_dog) = NULL;

    return cat_true;
}

CAT_API cat_bool_t cat_watch_dog_runtime_shutdown(void)
{
    if (cat_watch_dog_is_running() && !cat_watch_dog_stop()) {
        cat_core_error(WATCH_DOG, "Watch-Dog close failed during rshutdown");
    }

    return cat_true;
}

CAT_API void cat_watch_dog_alert_standard(cat_watch_dog_t *watch_dog)
{
    fprintf(stderr, "Warning: <Watch-Dog> Syscall blocking or CPU starvation may occur in " CAT_WATCH_DOG_ROLE_NAME " %d, "
                    "it has been blocked for more than " CAT_NSEC_FMT  " ns" CAT_EOL,
                    watch_dog->pid, watch_dog->quantum * watch_dog->alert_count);
}

CAT_API cat_bool_t cat_watch_dog_run(cat_watch_dog_t *watch_dog, cat_nsec_t quantum, cat_nsec_t threshold, cat_watch_dog_alerter_t alerter)
{
    uv_thread_options_t options;
    uv_sem_t sem;
    int error;

    if (CAT_WATCH_DOG_G(watch_dog) != NULL) {
        cat_update_last_error(CAT_EMISUSE, "Only one watch-dog is allowed to run per " CAT_WATCH_DOG_ROLE_NAME);
        return cat_false;
    }

    if (watch_dog == NULL) {
        watch_dog = (cat_watch_dog_t *) cat_malloc(sizeof(*watch_dog));
        if (watch_dog == NULL) {
            cat_update_last_error_of_syscall("Malloc for watch-dog failed");
            return cat_false;
        }
        watch_dog->allocated = cat_true;
    } else {
        watch_dog->allocated = cat_false;
    }

    watch_dog->quantum = cat_watch_dog_align_quantum(quantum);
    watch_dog->threshold = cat_watch_dog_align_threshold(threshold);
    watch_dog->alerter = alerter != NULL ? alerter : cat_watch_dog_alert_standard;
    watch_dog->alert_count = 0;
    watch_dog->stop = cat_false;
    watch_dog->pid = uv_os_getpid();
    watch_dog->globals = CAT_GLOBALS_BULK(cat_coroutine);
    watch_dog->last_round = 0;

    error = uv_sem_init(&sem, 0);
    if (error != 0) {
        cat_update_last_error_with_reason(error, "Watch-Dog init sem failed");
        goto _sem_init_failed;
    }
    error = uv_cond_init(&watch_dog->cond);
    if (error != 0) {
        cat_update_last_error_with_reason(error, "Watch-Dog init cond failed");
        goto _cond_init_failed;
    }
    error = uv_mutex_init(&watch_dog->mutex);
    if (error != 0) {
        cat_update_last_error_with_reason(error, "Watch-Dog init mutex failed");
        goto _mutex_init_failed;
    }

    watch_dog->sem = &sem;
    options.flags = UV_THREAD_HAS_STACK_SIZE;
    options.stack_size = CAT_COROUTINE_RECOMMENDED_STACK_SIZE;

    error = uv_thread_create_ex(&watch_dog->thread, &options, cat_watch_dog_loop, watch_dog);

    if (error != 0) {
        cat_update_last_error_with_reason(error, "Watch-Dog create thread failed");
        goto _thread_create_failed;
    }

    CAT_WATCH_DOG_G(watch_dog) = watch_dog;

    uv_sem_wait(&sem);
    uv_sem_destroy(&sem);
    watch_dog->sem = NULL;

    return cat_true;

    _thread_create_failed:
    uv_mutex_destroy(&watch_dog->mutex);
    _mutex_init_failed:
    uv_cond_destroy(&watch_dog->cond);
    _cond_init_failed:
    uv_sem_destroy(&sem);
    _sem_init_failed:
    if (watch_dog->allocated) {
        cat_free(watch_dog);
    }
    return cat_false;
}

CAT_API cat_bool_t cat_watch_dog_stop(void)
{
    cat_watch_dog_t *watch_dog = CAT_WATCH_DOG_G(watch_dog);
    int error;

    if (watch_dog == NULL) {
        cat_update_last_error(CAT_EMISUSE, "Watch-Dog is not running");
        return cat_false;
    }

    watch_dog->stop = cat_true;
    uv_mutex_lock(&watch_dog->mutex);
    uv_cond_signal(&watch_dog->cond);
    uv_mutex_unlock(&watch_dog->mutex);

    error = uv_thread_join(&watch_dog->thread);

    if (error != 0) {
        cat_update_last_error_with_reason(error, "Watch-Dog close thread failed");
        return cat_false;
    }

    uv_mutex_destroy(&watch_dog->mutex);
    uv_cond_destroy(&watch_dog->cond);
    CAT_ASSERT(watch_dog->sem == NULL);

    if (watch_dog->allocated) {
        cat_free(watch_dog);
    }

    CAT_WATCH_DOG_G(watch_dog) = NULL;

    return cat_true;
}

CAT_API cat_bool_t cat_watch_dog_is_running(void)
{
    return CAT_WATCH_DOG_G(watch_dog) != NULL;
}

CAT_API cat_nsec_t cat_watch_dog_get_quantum(void)
{
    cat_watch_dog_t *watch_dog = CAT_WATCH_DOG_G(watch_dog);

    return watch_dog != NULL ?
            watch_dog->quantum :
            -1;
}

CAT_API cat_nsec_t cat_watch_dog_get_threshold(void)
{
    cat_watch_dog_t *watch_dog = CAT_WATCH_DOG_G(watch_dog);

    return watch_dog != NULL ?
            watch_dog->threshold :
            -1;
}
