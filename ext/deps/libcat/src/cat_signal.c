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

#include "cat_signal.h"
#include "cat_coroutine.h"
#include "cat_event.h"
#include "cat_time.h"

CAT_API cat_bool_t cat_kill(int pid, int signum)
{
    int error;

    error = uv_kill(pid, signum);

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Kill(%d, %d) failed", pid, signum);
        return cat_false;
    }

    return cat_true;
}

typedef union {
    cat_coroutine_t *coroutine;
    uv_handle_t handle;
    uv_signal_t signal;
} cat_signal_t;

void cat_signal_callback(uv_signal_t* handle, int signum)
{
    (void) signum;
    cat_signal_t *signal = (cat_signal_t *) handle;
    cat_coroutine_t *coroutine = signal->coroutine;

    signal->coroutine = NULL;

    cat_coroutine_schedule(coroutine, SIGNAL, "Signal");
}

CAT_API cat_bool_t cat_signal_wait(int signum, cat_timeout_t timeout)
{
    cat_signal_t *signal;
    cat_bool_t ret;
    int error;

    signal = (cat_signal_t *) cat_malloc(sizeof(*signal));
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(signal == NULL)) {
        cat_update_last_error_of_syscall("Malloc for signal failed");
        return cat_false;
    }
#endif
    error = uv_signal_init(&CAT_EVENT_G(loop), &signal->signal);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Signal(%d) init failed", signum);
        return cat_false;
    }
    error = uv_signal_start_oneshot(&signal->signal, cat_signal_callback, signum);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Signal(%d) start failed", signum);
        uv_close(&signal->handle, (uv_close_cb) cat_free_function);
        return cat_false;
    }
    signal->coroutine = CAT_COROUTINE_G(current);
    ret = cat_time_wait(timeout);
    uv_close(&signal->handle, (uv_close_cb) cat_free_function);
    if (unlikely(!ret)) {
        cat_update_last_error_with_previous("Signal(%d) wait failed", signum);
        return cat_false;
    }
    if (signal->coroutine != NULL) {
        cat_update_last_error(CAT_ECANCELED, "Signal(%d) has been canceled", signum);
        return cat_false;
    }

    return cat_true;
}
