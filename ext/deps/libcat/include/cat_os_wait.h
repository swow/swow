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

#ifndef CAT_OS_WAIT_H
#define CAT_OS_WAIT_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#ifdef CAT_OS_UNIX_LIKE
# include <sys/resource.h>

/* we believe it's always available on nowadays machines,
 * we can consider checking it only when someone met this problem :) */
# define CAT_OS_WAIT_HAVE_RUSAGE 1
#endif

CAT_API cat_bool_t cat_os_wait_module_init(void);
CAT_API cat_bool_t cat_os_wait_module_shutdown(void);
CAT_API cat_bool_t cat_os_wait_runtime_init(void);
CAT_API cat_bool_t cat_os_wait_runtime_shutdown(void);

#ifdef CAT_OS_UNIX_LIKE
/*
 * Fork the process, without blocking the coroutine.
 *
 * The child calls cat_event_fork() and gets a fresh event loop. The parent gets
 * the pid of the child, the child gets 0, and a failure gets -1.
 */
CAT_API cat_pid_t cat_os_fork(void);
#endif

/*
 * Wait for a process, without blocking the thread that waits.
 *
 * Return the pid of the process that changed state, 0 when WNOHANG is set and
 * the process is not ready, and -1 on error. A wait that runs out of time
 * returns -1 with CAT_ETIMEDOUT, so a timeout is not a process state.
 *
 * A Unix-like platform reports a stopped child even when WUNTRACED is not set,
 * because the bookkeeping keeps every status it takes from the kernel. Windows
 * has no child process relation and no wait status: only a positive pid is
 * supported, the status is the exit code, and a process that the system stopped
 * is reported as an exit.
 *
 * On Windows, pid -1 and pid 0 fail with CAT_ENOTSUP. On a Unix-like platform,
 * pid -1 waits for any child and pid 0 fails with CAT_ENOTSUP.
 *
 * Do not wait for the same child with this API and with cat_process_* on a
 * Unix-like platform: the SIGCHLD handler here can take the exit status of a
 * process that uv_spawn() started, and the cat_process wait then never completes.
 * Windows waits on the process handle, so both waits can observe the same
 * process.
 *
 * Read the status with cat_os_wait_status_*(), which decodes the raw wait status
 * on a Unix-like platform and the exit code on Windows.
 */
CAT_API cat_pid_t cat_os_wait(int *status);
CAT_API cat_pid_t cat_os_wait_ex(int *status, cat_msec_t timeout);
CAT_API cat_pid_t cat_os_waitpid(cat_pid_t pid, int *status, int options);
CAT_API cat_pid_t cat_os_waitpid_ex(cat_pid_t pid, int *status, int options, cat_msec_t timeout);

CAT_API cat_bool_t cat_os_wait_status_exited(int status);
CAT_API int cat_os_wait_status_exit_code(int status);
CAT_API cat_bool_t cat_os_wait_status_signaled(int status);
CAT_API int cat_os_wait_status_term_signal(int status);

#ifdef CAT_OS_WIN
/* The value that the POSIX platforms use, so one call compiles on both. */
# ifndef WNOHANG
#  define WNOHANG 0x0001
# endif
#endif

#ifdef CAT_OS_WAIT_HAVE_RUSAGE
CAT_API cat_pid_t cat_os_wait3(int *status, int options, struct rusage *rusage);
CAT_API cat_pid_t cat_os_wait3_ex(int *status, int options, struct rusage *rusage, cat_msec_t timeout);
CAT_API cat_pid_t cat_os_wait4(cat_pid_t pid, int *status, int options, struct rusage *rusage);
CAT_API cat_pid_t cat_os_wait4_ex(cat_pid_t pid, int *status, int options, struct rusage *rusage, cat_msec_t timeout);
#endif /* CAT_OS_WAIT_HAVE_RUSAGE */

#ifdef __cplusplus
}
#endif
#endif /* CAT_OS_WAIT_H */
