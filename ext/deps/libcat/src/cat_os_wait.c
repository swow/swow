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


#include "cat_os_wait.h"

/* The POSIX backend. A Windows backend joins this file later, so that the wait
 * API is the same on every platform. */
#ifdef CAT_OS_UNIX_LIKE

#include "cat_signal.h"
#include "cat_event.h"
#include "cat_time.h"

#include <sys/wait.h>

/*
 * The wait mirrors wait4(2): it only ever asks the kernel about the pid (or the
 * process group) that the caller passed, with the options that the caller passed.
 *
 * | Step                   | What happens                                          |
 * | the call               | wait4(pid, options | WNOHANG) once, return if ready   |
 * | nothing ready          | queue a waiter, suspend the coroutine                 |
 * | SIGCHLD arrives        | retry wait4() for every queued waiter, wake the ready |
 *
 * Children that nobody waits for stay zombies until somebody waits for them,
 * exactly as with the raw system call. Earlier versions reaped every child with
 * wait4(-1) and kept the statuses, which stole children from code that waits by
 * itself (pclose(), PHP's pcntl_waitpid(), libuv's uv_spawn()) while any
 * coroutine was waiting here. libuv waits for its own processes the same way
 * (uv__wait_children() calls waitpid() per process).
 */

typedef struct cat_os_wait_waiter_s {
    cat_queue_t node;
    cat_bool_t linked;
    cat_coroutine_t *coroutine;
    cat_pid_t pid;
    int options;
    /* 0 while pending, the pid of the child, or -1 with error set */
    cat_pid_t result;
    int error;
    int status;
#ifdef CAT_OS_WAIT_HAVE_RUSAGE
    struct rusage rusage;
#endif
} cat_os_wait_waiter_t;

CAT_GLOBALS_STRUCT_BEGIN(cat_os_wait) {
    size_t sigchld_watcher_count;
    uv_signal_t sigchld_watcher;
    /* coroutines that hang on wait(), waitpid(), wait3() or wait4() */
    cat_queue_t waiters;
} CAT_GLOBALS_STRUCT_END(cat_os_wait);

CAT_GLOBALS_DECLARE(cat_os_wait);

#define CAT_OS_WAIT_G(x) CAT_GLOBALS_GET(cat_os_wait, x)

/* One non-blocking wait4() (or waitpid()) on behalf of a caller, retried on EINTR. */
static cat_pid_t cat_os_wait_syscall(cat_pid_t pid, int *status, int options, void *rusage)
{
    cat_pid_t ret;
#ifndef CAT_OS_WAIT_HAVE_RUSAGE
    CAT_ASSERT(rusage == NULL);
#endif
    do {
#ifdef CAT_OS_WAIT_HAVE_RUSAGE
        ret = wait4(pid, status, options | WNOHANG, (struct rusage *) rusage);
#else
        ret = waitpid(pid, status, options | WNOHANG);
#endif
    } while (unlikely(ret < 0 && errno == EINTR));

    return ret;
}

static cat_always_inline void cat_os_wait_waiter_unlink(cat_os_wait_waiter_t *waiter)
{
    if (waiter->linked) {
        cat_queue_remove(&waiter->node);
        waiter->linked = cat_false;
    }
}

static void cat_os_wait_waiter_resolve(cat_os_wait_waiter_t *waiter, cat_queue_t *ready, cat_pid_t result, int error, const cat_os_wait_waiter_t *source)
{
    waiter->result = result;
    waiter->error = error;
    if (source != NULL) {
        waiter->status = source->status;
#ifdef CAT_OS_WAIT_HAVE_RUSAGE
        waiter->rusage = source->rusage;
#endif
    }
    cat_queue_remove(&waiter->node);
    cat_queue_push_back(ready, &waiter->node);
}

/* Ask the kernel for one waiter; a ready status is shared with the other waiters
 * of the same pid, otherwise they would see ECHILD for a child they all wait for. */
static void cat_os_wait_poll_waiter(cat_os_wait_waiter_t *waiter, cat_queue_t *ready)
{
    cat_pid_t pid;
#ifdef CAT_OS_WAIT_HAVE_RUSAGE
    pid = cat_os_wait_syscall(waiter->pid, &waiter->status, waiter->options, &waiter->rusage);
#else
    pid = cat_os_wait_syscall(waiter->pid, &waiter->status, waiter->options, NULL);
#endif
    if (pid == 0) {
        return;
    }
    if (pid < 0) {
        cat_os_wait_waiter_resolve(waiter, ready, -1, errno, NULL);
        return;
    }
    CAT_LOG_DEBUG(OS_WAIT,
        "dispatcher: wait4(%d) = %d, exited=%u, exit_status=%d, if_signaled=%u, termsig=%d, if_stopped=%u, stopsig=%d",
        waiter->pid, pid, WIFEXITED(waiter->status), WEXITSTATUS(waiter->status), WIFSIGNALED(waiter->status),
        WTERMSIG(waiter->status), WIFSTOPPED(waiter->status), WSTOPSIG(waiter->status)
    );
    cat_os_wait_waiter_resolve(waiter, ready, pid, 0, NULL);
    cat_queue_t *node = cat_queue_next(&CAT_OS_WAIT_G(waiters));
    while (node != &CAT_OS_WAIT_G(waiters)) {
        cat_os_wait_waiter_t *other = cat_queue_data(node, cat_os_wait_waiter_t, node);
        node = cat_queue_next(node);
        if (other->pid == pid) {
            cat_os_wait_waiter_resolve(other, ready, pid, 0, waiter);
        }
    }
}

static void cat_os_wait_sigchld_handler(uv_signal_t* handle, int signum)
{
    cat_queue_t *waiters = &CAT_OS_WAIT_G(waiters);
    cat_queue_t ready;
    cat_queue_t *node;
    (void) handle;

    CAT_ASSERT(signum == CAT_SIGCHLD);
    CAT_LOG_DEBUG(OS_WAIT, "SIGCHLD received");

    /* Collect first and wake later: waking runs the coroutine at once, and it may
     * queue or cancel waiters while we walk the list.
     * Waiters for a positive pid go first, so that a child somebody waits for by
     * pid is not handed to a wait for any child that happens to be polled earlier. */
    cat_queue_init(&ready);
    for (int pass = 0; pass < 2; pass++) {
        node = cat_queue_next(waiters);
        while (node != waiters) {
            cat_os_wait_waiter_t *waiter = cat_queue_data(node, cat_os_wait_waiter_t, node);
            node = cat_queue_next(node);
            if ((pass == 0) == (waiter->pid > 0)) {
                cat_os_wait_poll_waiter(waiter, &ready);
                /* the resolve may have moved the next node to the ready list too */
                if (node != waiters && cat_queue_data(node, cat_os_wait_waiter_t, node)->result != 0) {
                    node = cat_queue_next(waiters);
                }
            }
        }
    }
    while (!cat_queue_empty(&ready)) {
        cat_os_wait_waiter_t *waiter = cat_queue_front_data(&ready, cat_os_wait_waiter_t, node);
        cat_queue_remove(&waiter->node);
        waiter->linked = cat_false;
        cat_coroutine_schedule(waiter->coroutine, OS, "Wait");
    }
}

static cat_always_inline void cat_os_wait_sigchld_watcher_start(void)
{
    (void) uv_signal_start(&CAT_OS_WAIT_G(sigchld_watcher), cat_os_wait_sigchld_handler, CAT_SIGCHLD);
    CAT_OS_WAIT_G(sigchld_watcher_count)++;
}

static cat_always_inline void cat_os_wait_sigchld_watcher_end(void)
{
    if (--CAT_OS_WAIT_G(sigchld_watcher_count) == 0) {
        (void) uv_signal_stop(&CAT_OS_WAIT_G(sigchld_watcher));
    }
}

typedef enum cat_os_wait_type_e {
    CAT_OS_WAIT_TYPE_WAIT,
    CAT_OS_WAIT_TYPE_WAITPID,
#ifdef CAT_OS_WAIT_HAVE_RUSAGE
    CAT_OS_WAIT_TYPE_WAIT3,
    CAT_OS_WAIT_TYPE_WAIT4,
#endif
} cat_os_wait_type_t;

static const char *cat_never_inline cat_os_wait_type_name(cat_os_wait_type_t type)
{
    switch (type) {
        case CAT_OS_WAIT_TYPE_WAIT:
            return "wait";
        case CAT_OS_WAIT_TYPE_WAITPID:
            return "waitpid";
#ifdef CAT_OS_WAIT_HAVE_RUSAGE
        case CAT_OS_WAIT_TYPE_WAIT3:
            return "wait3";
        case CAT_OS_WAIT_TYPE_WAIT4:
            return "wait4";
#endif
        default:
            CAT_NEVER_HERE("Unknown type");
    }
}

static cat_pid_t cat_os__wait(cat_pid_t pid, int *status, int options, void *rusage, cat_msec_t timeout, cat_os_wait_type_t type)
{
    cat_os_wait_waiter_t waiter;
    int raw_status = 0;
    cat_pid_t ret;

    /* The SIGCHLD watcher is already running (see the wrapper), so a child that
     * exits right after this check still wakes the waiter queued below. */
    for (;;) {
        ret = cat_os_wait_syscall(pid, &raw_status, options, rusage);
        if (ret < 0) {
            cat_errno_t code = errno == ECHILD ? CAT_ECHILD : cat_translate_sys_error(errno);
            cat_update_last_error_with_reason(code, "OS %s(pid=%d) failed", cat_os_wait_type_name(type), pid);
            return -1;
        }
        if (ret > 0) {
            if (status != NULL) {
                *status = raw_status;
            }
            return ret;
        }
        /* Nothing is ready, and WNOHANG asks for an answer without waiting. */
        if (options & WNOHANG) {
            return 0;
        }

        memset(&waiter, 0, sizeof(waiter));
        waiter.coroutine = CAT_COROUTINE_G(current);
        waiter.pid = pid;
        waiter.options = options;
        cat_queue_push_back(&CAT_OS_WAIT_G(waiters), &waiter.node);
        waiter.linked = cat_true;

        cat_bool_t waited = cat_time_wait(timeout);
        cat_os_wait_waiter_unlink(&waiter);

        if (waiter.result > 0) {
            if (status != NULL) {
                *status = waiter.status;
            }
#ifdef CAT_OS_WAIT_HAVE_RUSAGE
            if (rusage != NULL) {
                *((struct rusage *) rusage) = waiter.rusage;
            }
#endif
            return waiter.result;
        }
        if (waiter.result < 0) {
            cat_errno_t code = waiter.error == ECHILD ? CAT_ECHILD : cat_translate_sys_error(waiter.error);
            cat_update_last_error_with_reason(code, "OS %s(pid=%d) failed", cat_os_wait_type_name(type), pid);
            return -1;
        }
        if (unlikely(!waited)) {
            cat_update_last_error_with_previous("OS %s() wait failed", cat_os_wait_type_name(type));
            return -1;
        }
        /* resumed by someone else without a result: ask the kernel again */
    }
}

static cat_pid_t cat_os__wait_wrapper(cat_pid_t pid, int *status, int options, void *rusage, cat_msec_t timeout, cat_os_wait_type_t type)
{
    cat_os_wait_sigchld_watcher_start();

#define CAT_OS_WAIT_LOG_WAIT_FMT "%s(timeout=" CAT_TIMEOUT_FMT ")"
#define CAT_OS_WAIT_LOG_WAIT_ARGS cat_os_wait_type_name(type), (timeout)

#define CAT_OS_WAIT_LOG_WAIT3_FMT "%s(options=%d, timeout=" CAT_TIMEOUT_FMT ")"
#define CAT_OS_WAIT_LOG_WAIT3_ARGS cat_os_wait_type_name(type), options, timeout

#define CAT_OS_WAIT_LOG_WAIT4_OR_WAITPID_FMT "%s(pid=%d, options=%d, timeout=" CAT_TIMEOUT_FMT ")"
#define CAT_OS_WAIT_LOG_WAIT4_OR_WAITPID_ARGS cat_os_wait_type_name(type), pid, options, timeout

    CAT_LOG_DEBUG_VA(OS_WAIT, {
        if (type == CAT_OS_WAIT_TYPE_WAIT) {
            CAT_LOG_DEBUG_D(OS_WAIT, CAT_OS_WAIT_LOG_WAIT_FMT, CAT_OS_WAIT_LOG_WAIT_ARGS);
        } else if (type == CAT_OS_WAIT_TYPE_WAITPID || type == CAT_OS_WAIT_TYPE_WAIT4) {
            CAT_LOG_DEBUG_D(OS_WAIT, CAT_OS_WAIT_LOG_WAIT4_OR_WAITPID_FMT, CAT_OS_WAIT_LOG_WAIT4_OR_WAITPID_ARGS);
        } else if (type == CAT_OS_WAIT_TYPE_WAIT3) {
            CAT_LOG_DEBUG_D(OS_WAIT, CAT_OS_WAIT_LOG_WAIT3_FMT, CAT_OS_WAIT_LOG_WAIT3_ARGS);
        } else CAT_NEVER_HERE("Unknown type");
    });

    pid = cat_os__wait(pid, status, options, rusage, timeout, type);


#define CAT_OS_WAIT_LOG_RETURN_VALUE_FMT " = %d"
#define CAT_OS_WAIT_LOG_RETURN_VALUE_ARGS pid

    CAT_LOG_DEBUG_VA(OS_WAIT, {
        if (type == CAT_OS_WAIT_TYPE_WAIT) {
            CAT_LOG_DEBUG_D(OS_WAIT, CAT_OS_WAIT_LOG_WAIT_FMT CAT_OS_WAIT_LOG_RETURN_VALUE_FMT, CAT_OS_WAIT_LOG_WAIT_ARGS, CAT_OS_WAIT_LOG_RETURN_VALUE_ARGS);
        } else if (type == CAT_OS_WAIT_TYPE_WAITPID || type == CAT_OS_WAIT_TYPE_WAIT4) {
            CAT_LOG_DEBUG_D(OS_WAIT, CAT_OS_WAIT_LOG_WAIT4_OR_WAITPID_FMT CAT_OS_WAIT_LOG_RETURN_VALUE_FMT, CAT_OS_WAIT_LOG_WAIT4_OR_WAITPID_ARGS, CAT_OS_WAIT_LOG_RETURN_VALUE_ARGS);
        } else if (type == CAT_OS_WAIT_TYPE_WAIT3) {
            CAT_LOG_DEBUG_D(OS_WAIT, CAT_OS_WAIT_LOG_WAIT3_FMT CAT_OS_WAIT_LOG_RETURN_VALUE_FMT, CAT_OS_WAIT_LOG_WAIT3_ARGS, CAT_OS_WAIT_LOG_RETURN_VALUE_ARGS);
        } else CAT_NEVER_HERE("Unknown type");
    });

    cat_os_wait_sigchld_watcher_end();

    return pid;
}

CAT_API cat_pid_t cat_os_wait(int *status)
{
    return cat_os_wait_ex(status, CAT_TIMEOUT_FOREVER);
}

CAT_API cat_pid_t cat_os_wait_ex(int *status, cat_msec_t timeout)
{
    return cat_os__wait_wrapper(-1, status, 0, NULL, timeout, CAT_OS_WAIT_TYPE_WAIT);
}

CAT_API cat_pid_t cat_os_waitpid(cat_pid_t pid, int *status, int options)
{
    return cat_os_waitpid_ex(pid, status, options, CAT_TIMEOUT_FOREVER);
}

CAT_API cat_pid_t cat_os_waitpid_ex(cat_pid_t pid, int *status, int options, cat_msec_t timeout)
{
    return cat_os__wait_wrapper(pid, status, options, NULL, timeout, CAT_OS_WAIT_TYPE_WAITPID);
}

CAT_API cat_bool_t cat_os_wait_status_exited(int status)
{
    return WIFEXITED(status) ? cat_true : cat_false;
}

CAT_API int cat_os_wait_status_exit_code(int status)
{
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

CAT_API cat_bool_t cat_os_wait_status_signaled(int status)
{
    return WIFSIGNALED(status) ? cat_true : cat_false;
}

CAT_API int cat_os_wait_status_term_signal(int status)
{
    return WIFSIGNALED(status) ? WTERMSIG(status) : -1;
}

#ifdef CAT_OS_WAIT_HAVE_RUSAGE

CAT_API cat_pid_t cat_os_wait3(int *status, int options, struct rusage *rusage)
{
    return cat_os_wait3_ex(status, options, rusage, CAT_TIMEOUT_FOREVER);
}

CAT_API cat_pid_t cat_os_wait3_ex(int *status, int options, struct rusage *rusage, cat_msec_t timeout)
{
    return cat_os__wait_wrapper(-1, status, options, rusage, timeout, CAT_OS_WAIT_TYPE_WAIT3);
}

CAT_API cat_pid_t cat_os_wait4(cat_pid_t pid, int *status, int options, struct rusage *rusage)
{
    return cat_os_wait4_ex(pid, status, options, rusage, CAT_TIMEOUT_FOREVER);
}

CAT_API cat_pid_t cat_os_wait4_ex(cat_pid_t pid, int *status, int options, struct rusage *rusage, cat_msec_t timeout)
{
    return cat_os__wait_wrapper(pid, status, options, rusage, timeout, CAT_OS_WAIT_TYPE_WAIT4);
}

#endif /* CAT_OS_WAIT_HAVE_RUSAGE */

CAT_API cat_pid_t cat_os_fork(void)
{
    cat_pid_t pid = fork();
    if (pid == 0) {
        /* in the child: rebuild the event loop, so that the libuv handles work */
        cat_event_fork();
    } else if (pid < 0) {
        cat_update_last_error_of_syscall("Fork failed");
    }
    return pid;
}

CAT_API cat_bool_t cat_os_wait_module_init(void)
{
    CAT_GLOBALS_REGISTER(cat_os_wait);

    return cat_true;
}

CAT_API cat_bool_t cat_os_wait_module_shutdown(void)
{
    CAT_GLOBALS_UNREGISTER(cat_os_wait);

    return cat_true;
}

CAT_API cat_bool_t cat_os_wait_runtime_init(void)
{
    uv_signal_init(&CAT_EVENT_G(loop), &CAT_OS_WAIT_G(sigchld_watcher));
    cat_queue_init(&CAT_OS_WAIT_G(waiters));

    return cat_true;
}

CAT_API cat_bool_t cat_os_wait_runtime_shutdown(void)
{
    uv_close((uv_handle_t *) &CAT_OS_WAIT_G(sigchld_watcher), NULL);

    CAT_ASSERT(cat_queue_empty(&CAT_OS_WAIT_G(waiters)));

    return cat_true;
}

#else /* !CAT_OS_UNIX_LIKE */

#include "cat_coroutine.h"
#include "cat_time.h"

/*
 * Windows has no child process relation and no wait status. A caller waits for a
 * known pid: the backend opens the process, waits for its handle, and reports the
 * exit code as the status.
 */

#ifdef CAT_THREAD_SAFE

/*
 * The callback runs on a thread pool thread and wakes the parked coroutine. The
 * wait stays registered until the waiter unregisters it, so the task stays valid.
 */
typedef struct cat_os_wait_win_task_s {
    HANDLE process;
    cat_coroutine_t *coroutine;
    DWORD exit_code;
    cat_bool_t timed_out;
    cat_bool_t done;
} cat_os_wait_win_task_t;

static VOID CALLBACK cat_os_wait_win_callback(PVOID data, BOOLEAN timed_out)
{
    cat_os_wait_win_task_t *task = (cat_os_wait_win_task_t *) data;

    if (timed_out) {
        task->timed_out = cat_true;
    } else {
        DWORD exit_code = 0;
        (void) GetExitCodeProcess(task->process, &exit_code);
        task->exit_code = exit_code;
    }
    task->done = cat_true;

    /* The waiter is parked: it parks once before the callback can wake it. */
    cat_coroutine_wake(task->coroutine);
}

#endif /* CAT_THREAD_SAFE */

CAT_API cat_pid_t cat_os_waitpid_ex(cat_pid_t pid, int *status, int options, cat_msec_t timeout)
{
    HANDLE process;
    DWORD exit_code = 0;

    if (pid <= 0) {
        cat_update_last_error(CAT_ENOTSUP,
            "Windows can not wait for any child, pass the pid of a process");
        return -1;
    }

    process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD) pid);
    if (unlikely(process == NULL)) {
        DWORD error = GetLastError();
        cat_update_last_error_with_reason(
            error == ERROR_INVALID_PARAMETER ? CAT_ESRCH : cat_translate_sys_error((int) error),
            "OpenProcess(%d) failed with %lu", (int) pid, (unsigned long) error);
        return -1;
    }

    if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
        (void) GetExitCodeProcess(process, &exit_code);
        CloseHandle(process);
        if (status != NULL) {
            *status = (int) exit_code;
        }
        return pid;
    }

    if (options & WNOHANG) {
        CloseHandle(process);
        return 0;
    }

#ifdef CAT_THREAD_SAFE
    {
        HANDLE wait_handle = NULL;
        cat_os_wait_win_task_t task = {
            .process = process,
            .coroutine = CAT_COROUTINE_G(current),
        };
        /* CAT_TIMEOUT_FOREVER arrives here as the largest value of cat_msec_t.
         * A timeout above the range of the API waits without a deadline. */
        ULONG wait_msec = timeout > (cat_msec_t) (INFINITE - 1) ? INFINITE : (ULONG) timeout;

        if (unlikely(!RegisterWaitForSingleObject(
            &wait_handle, process, cat_os_wait_win_callback, &task, wait_msec,
            WT_EXECUTEONLYONCE | WT_EXECUTEINWAITTHREAD))) {
            DWORD error = GetLastError();
            CloseHandle(process);
            cat_update_last_error_with_reason(cat_translate_sys_error((int) error),
                "RegisterWaitForSingleObject(%d) failed with %lu",
                (int) pid, (unsigned long) error);
            return -1;
        }

        /* Park once before the callback can wake this coroutine, so a wake never
         * lands on a running coroutine. The callback is the only waker. */
        cat_bool_t ret = cat_time_wait(CAT_TIMEOUT_FOREVER);
        while (ret && !task.done) {
            ret = cat_time_wait(CAT_TIMEOUT_FOREVER);
        }
        (void) UnregisterWaitEx(wait_handle, INVALID_HANDLE_VALUE);

        if (unlikely(!ret)) {
            CloseHandle(process);
            cat_update_last_error_with_previous("OS waitpid() wait failed");
            return -1;
        }

        if (task.timed_out) {
            CloseHandle(process);
            cat_update_last_error(CAT_ETIMEDOUT, "Wait for process %d timed out", (int) pid);
            return -1;
        }

        exit_code = task.exit_code;
    }
#else
    {
        /* A single threaded build has no thread safe coroutine wake, so the
         * handle is polled from the loop. */
        cat_msec_t start = cat_time_msec();

        for (;;) {
            if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
                (void) GetExitCodeProcess(process, &exit_code);
                break;
            }
            if (timeout != (cat_msec_t) CAT_TIMEOUT_FOREVER &&
                cat_time_msec() - start >= timeout) {
                CloseHandle(process);
                cat_update_last_error(CAT_ETIMEDOUT, "Wait for process %d timed out", (int) pid);
                return -1;
            }
            (void) cat_time_delay(1);
        }
    }
#endif

    CloseHandle(process);
    if (status != NULL) {
        *status = (int) exit_code;
    }

    return pid;
}

CAT_API cat_pid_t cat_os_waitpid(cat_pid_t pid, int *status, int options)
{
    return cat_os_waitpid_ex(pid, status, options, CAT_TIMEOUT_FOREVER);
}

CAT_API cat_pid_t cat_os_wait(int *status)
{
    return cat_os_waitpid_ex(-1, status, 0, CAT_TIMEOUT_FOREVER);
}

CAT_API cat_pid_t cat_os_wait_ex(int *status, cat_msec_t timeout)
{
    return cat_os_waitpid_ex(-1, status, 0, timeout);
}

CAT_API cat_bool_t cat_os_wait_status_exited(int status)
{
    (void) status;

    return cat_true;
}

CAT_API int cat_os_wait_status_exit_code(int status)
{
    return status;
}

CAT_API cat_bool_t cat_os_wait_status_signaled(int status)
{
    (void) status;

    return cat_false;
}

CAT_API int cat_os_wait_status_term_signal(int status)
{
    (void) status;

    return -1;
}

CAT_API cat_bool_t cat_os_wait_module_init(void)
{
    return cat_true;
}

CAT_API cat_bool_t cat_os_wait_module_shutdown(void)
{
    return cat_true;
}

CAT_API cat_bool_t cat_os_wait_runtime_init(void)
{
    return cat_true;
}

CAT_API cat_bool_t cat_os_wait_runtime_shutdown(void)
{
    return cat_true;
}

#endif /* CAT_OS_UNIX_LIKE */
