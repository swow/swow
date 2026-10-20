/*
  +--------------------------------------------------------------------------+
  | Swow                                                                     |
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

#include "swow_debug.h"

typedef struct swow_debug_sampler_s {
    uv_thread_t thread;
    uv_mutex_t mutex;
    uv_cond_t cond;
    swow_atomic_bool *vm_interrupt;
    uint64_t interval;
    uint64_t generation;
    bool armed;
    bool stopping;
} swow_debug_sampler_t;

static void swow_debug_sampler_loop(void *data)
{
    swow_debug_sampler_t *sampler = data;

    uv_mutex_lock(&sampler->mutex);
    while (!sampler->stopping) {
        while (!sampler->armed && !sampler->stopping) {
            uv_cond_wait(&sampler->cond, &sampler->mutex);
        }
        if (sampler->stopping) {
            break;
        }
        uint64_t generation = sampler->generation;
        uint64_t started = uv_hrtime();
        uint64_t elapsed = 0;
        while (sampler->armed && !sampler->stopping && generation == sampler->generation && elapsed < sampler->interval) {
            (void) uv_cond_timedwait(&sampler->cond, &sampler->mutex, sampler->interval - elapsed);
            elapsed = uv_hrtime() - started;
        }
        if (sampler->armed && !sampler->stopping && generation == sampler->generation) {
            /* Only the atomic flag crosses threads. PHP frames and handlers are
             * accessed by the executor after it acknowledges this request. */
            swow_atomic_bool_store(sampler->vm_interrupt, true);
            sampler->armed = false;
        }
    }
    uv_mutex_unlock(&sampler->mutex);
}

bool swow_debug_sampler_start(void)
{
    if (SWOW_DEBUG_G(sampler) != NULL) {
        return true;
    }
    zend_long interval = SWOW_G(ini.debug_force_interval);
    if (interval <= 0 || (uint64_t) interval > UINT64_MAX / 1000000) {
        zend_throw_error(NULL, "swow.debug_force_interval must be a positive number of milliseconds within the timer range");
        return false;
    }
    swow_debug_sampler_t *sampler = ecalloc(1, sizeof(*sampler));
    sampler->vm_interrupt = &EG(vm_interrupt);
    sampler->interval = (uint64_t) interval * 1000000;
    sampler->armed = true;

    int error = uv_mutex_init(&sampler->mutex);
    if (error != 0) {
        goto mutex_init_failed;
    }
    error = uv_cond_init(&sampler->cond);
    if (error != 0) {
        goto cond_init_failed;
    }
    error = uv_thread_create(&sampler->thread, swow_debug_sampler_loop, sampler);
    if (error != 0) {
        goto thread_create_failed;
    }
    SWOW_DEBUG_G(sampler) = sampler;
    return true;

    thread_create_failed:
    uv_cond_destroy(&sampler->cond);
    cond_init_failed:
    uv_mutex_destroy(&sampler->mutex);
    mutex_init_failed:
    efree(sampler);
    zend_throw_error(NULL, "Unable to start forced debug observation: %s", uv_strerror(error));
    return false;
}

void swow_debug_sampler_set_armed(bool armed)
{
    swow_debug_sampler_t *sampler = SWOW_DEBUG_G(sampler);
    if (sampler == NULL) {
        return;
    }
    uv_mutex_lock(&sampler->mutex);
    sampler->armed = armed;
    sampler->generation++;
    uv_cond_signal(&sampler->cond);
    uv_mutex_unlock(&sampler->mutex);
}

bool swow_debug_sampler_stop(void)
{
    swow_debug_sampler_t *sampler = SWOW_DEBUG_G(sampler);
    if (sampler == NULL) {
        return false;
    }
    uv_mutex_lock(&sampler->mutex);
    sampler->stopping = true;
    uv_cond_signal(&sampler->cond);
    uv_mutex_unlock(&sampler->mutex);
    int error = uv_thread_join(&sampler->thread);
    if (error != 0) {
        zend_error_noreturn(E_ERROR, "Unable to join forced debug observation thread: %s", uv_strerror(error));
    }
    uv_cond_destroy(&sampler->cond);
    uv_mutex_destroy(&sampler->mutex);
    SWOW_DEBUG_G(sampler) = NULL;
    efree(sampler);
    return true;
}
