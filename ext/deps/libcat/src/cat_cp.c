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

#include "cat.h"

/* sleep */

#ifdef CAT_OS_WIN
CAT_API unsigned int cat_sys_sleep(unsigned int seconds)
{
    return SleepEx(seconds * 1000, TRUE);
}

CAT_API int cat_sys_usleep(unsigned int useconds)
{
    HANDLE timer;
    LARGE_INTEGER due;

    due.QuadPart = -(10 * (__int64 ) useconds);

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &due, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);

    return 0;
}

CAT_API int cat_sys_nanosleep(const struct cat_timespec *req, struct cat_timespec *rem)
{
    (void) rem;
    if (req->tv_nsec > 999999999) {
        /* The time interval specified 1,000,000 or more microseconds. */
        cat_set_sys_errno(WSAEINVAL);
        return -1;
    }
    return cat_sys_usleep((unsigned int) (req->tv_sec * 1000000 + req->tv_nsec / 1000));
}
#endif

/* process */

CAT_API cat_pid_t cat_getpid(void)
{
    cat_pid_t pid;

    pid = uv_os_getpid();

    if (unlikely(pid < 0)) {
        cat_update_last_error_of_syscall("Process get id failed");
    }

    return pid;
}

CAT_API cat_pid_t cat_getppid(void)
{
    cat_pid_t ppid;

    ppid = uv_os_getppid();

    if (unlikely(ppid < 0)) {
        cat_update_last_error_of_syscall("Process get parent id failed");
    }

    return ppid;
}

/* vector */

CAT_API size_t cat_io_vector_length(const cat_io_vector_t *vector, unsigned int vector_count)
{
    size_t nbytes = 0;

    while (vector_count--) {
        nbytes += (vector++)->length;
    }

    return nbytes;
}
