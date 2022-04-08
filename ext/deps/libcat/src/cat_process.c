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
  | Author: Twosee <twose@qq.com>                                            |
  +--------------------------------------------------------------------------+
 */

#include "cat_process.h"

#include "cat_event.h"

#define CAT_PROCESS_CHECK_SILENT(_process, _failure) \
    if (unlikely(_process->exited)) { \
        _failure; \
    }

#define CAT_PROCESS_CHECK(_process, _failure) \
        CAT_PROCESS_CHECK_SILENT(_process, { \
            cat_update_last_error(CAT_ESRCH, "Process has already exited"); \
            _failure; \
        })

static void cat_process_close_callback(uv_handle_t *handle)
{
    cat_process_t *process = cat_container_of(handle, cat_process_t, u.handle);
    cat_free(process);
}

static void cat_process_exit_callback(uv_process_t* uprocess, int64_t exit_status, int term_signal)
{
    cat_process_t *process = cat_container_of(uprocess, cat_process_t, u.process);

    process->exited = cat_true;
    process->exit_status = exit_status;
    process->term_signal = term_signal;

    /* notify all waiters */
    cat_coroutine_t *waiter;
    while ((waiter = cat_queue_front_data(&process->waiters, cat_coroutine_t, waiter.node))) {
        cat_coroutine_schedule(waiter, PROCESS, "Process");
    }
}

static int cat_process_check_stdio(uv_stdio_container_t *stdio)
{
    if (stdio->flags & (CAT_PROCESS_STDIO_FLAG_INHERIT_STREAM | CAT_PROCESS_STDIO_FLAG_CREATE_PIPE)) {
        cat_socket_t *stream = (cat_socket_t *) stdio->data.stream;
        cat_socket_internal_t *istream = stream->internal;
        if (unlikely(istream == NULL || !(istream->type & CAT_SOCKET_TYPE_FLAG_STREAM))) {
            return CAT_EINVAL;
        }
        stdio->data.stream = &stream->internal->u.stream;
    }

    return 0;
}

static int cat_process_update_stdio(uv_stdio_container_t *stdio)
{
    if (stdio->flags & CAT_PROCESS_STDIO_FLAG_CREATE_PIPE) {
        cat_socket_internal_t *istream = cat_container_of(stdio->data.stream, cat_socket_internal_t, u.stream);
        istream->flags |= CAT_SOCKET_INTERNAL_FLAG_ESTABLISHED;
        // TODO: on_open() ?
    }

    return 0;
}

CAT_API cat_process_t *cat_process_run(const cat_process_options_t *options)
{
    cat_process_t *process = NULL;
    uv_process_options_t uoptions = *((uv_process_options_t *) options);
    uv_stdio_container_t *ustdio = NULL;
    int n, error = 0;

    /* set callback */
    uoptions.exit_cb = cat_process_exit_callback;

    /* check stdio */
    if (uoptions.stdio_count > 0) {
        ustdio = (uv_stdio_container_t *) cat_malloc(sizeof(*ustdio) * uoptions.stdio_count);
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(ustdio == NULL)) {
            cat_update_last_error_of_syscall("Malloc for process stdio failed");
            goto _out;
        }
#endif
    }
    memcpy(ustdio, uoptions.stdio, sizeof(*ustdio) * uoptions.stdio_count);
    for (n = 0; n < uoptions.stdio_count; n++) {
        error = cat_process_check_stdio(&ustdio[n]);
        if (unlikely(error != 0)) {
            goto _out;
        }
    }
    uoptions.stdio = ustdio;

    /* init process structure */
    process = (cat_process_t *) cat_malloc(sizeof(*process));
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(process == NULL)) {
        cat_update_last_error_of_syscall("Malloc for process failed");
        goto _out;
    }
#endif
    process->exited = cat_false;
    process->exit_status = 0;
    process->term_signal = 0;
    cat_queue_init(&process->waiters);

    /* call spawn() to start process */
    error = uv_spawn(&CAT_EVENT_G(loop), &process->u.process, &uoptions);

    if (unlikely(error != 0)) {
        goto _out;
    }

    /* update stdio */
    for (n = 0; n < uoptions.stdio_count; n++) {
        cat_process_update_stdio(&uoptions.stdio[n]);
    }

    _out:
    if (ustdio != NULL) {
        cat_free(ustdio);
    }
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Process run failed");
        if (process != NULL) {
            cat_process_close(process);
        }
        return NULL;
    }

    return process;
}

CAT_API cat_bool_t cat_process_wait(cat_process_t *process)
{
    return cat_process_wait_ex(process, CAT_TIMEOUT_FOREVER);
}

CAT_API cat_bool_t cat_process_wait_ex(cat_process_t *process, cat_timeout_t timeout)
{
    CAT_PROCESS_CHECK_SILENT(process, return cat_true);
    cat_queue_t *waiter = &CAT_COROUTINE_G(current)->waiter.node;
    cat_bool_t ret;

    cat_queue_push_back(&process->waiters, waiter);
    ret = cat_time_wait(timeout);
    if (unlikely(!ret)) {
        cat_update_last_error_with_previous("Process wait failed");
    } else if (!process->exited) {
        cat_update_last_error(CAT_ECANCELED, "Process wait has been canceled");
        ret = cat_false;
    }
    cat_queue_remove(waiter);

    return ret;
}

CAT_API void cat_process_close(cat_process_t *process)
{
    uv_close(&process->u.handle, cat_process_close_callback);
}

CAT_API cat_pid_t cat_process_get_pid(const cat_process_t *process)
{
    return process->u.process.pid;
}

CAT_API cat_bool_t cat_process_has_exited(const cat_process_t *process)
{
    return process->exited;
}

CAT_API int64_t cat_process_get_exit_status(const cat_process_t *process)
{
    return process->exit_status;
}

CAT_API int cat_process_get_term_signal(const cat_process_t *process)
{
    return process->term_signal;
}

CAT_API cat_bool_t cat_process_kill(cat_process_t *process, int signum)
{
    CAT_PROCESS_CHECK(process, return cat_false);
    int error;

    error = uv_process_kill(&process->u.process, signum);

    if (error != 0) {
        cat_update_last_error_with_reason(error, "Process kill(%d) failed", signum);
        return cat_false;
    }

    return cat_true;
}
