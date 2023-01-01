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

#ifdef CAT_IDE_HELPER
#include "cat_cp.h"
#endif

#ifdef CAT_OS_WIN
#include <windows.h>
#endif

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

/* clock_gettime()
 * based on: https://stackoverflow.com/a/51974214/13695422 */

#ifndef CAT_OS_WIN
CAT_API int cat_clock_gettime_monotonic(struct timespec *tp)
{
    return clock_gettime(CLOCK_MONOTONIC, tp);
}

CAT_API int cat_clock_gettime_realtime(struct timespec *tp)
{
    return clock_gettime(CLOCK_REALTIME, tp);
}
#else
#define MS_PER_SEC      1000ULL     // MS = milliseconds
#define US_PER_MS       1000ULL     // US = microseconds
#define HNS_PER_US      10ULL       // HNS = hundred-nanoseconds (e.g., 1 hns = 100 ns)
#define NS_PER_US       1000ULL

#define HNS_PER_SEC     (MS_PER_SEC * US_PER_MS * HNS_PER_US)
#define NS_PER_HNS      (100ULL)    // NS = nanoseconds
#define NS_PER_SEC      (MS_PER_SEC * US_PER_MS * NS_PER_US)

CAT_API int cat_clock_gettime_realtime(struct timespec *tp)
{
    FILETIME ft;
    ULARGE_INTEGER hnsTime;

    GetSystemTimeAsFileTime(&ft);

    hnsTime.LowPart = ft.dwLowDateTime;
    hnsTime.HighPart = ft.dwHighDateTime;

    // To get POSIX Epoch as baseline, subtract the number of hns intervals from Jan 1, 1601 to Jan 1, 1970.
    hnsTime.QuadPart -= (11644473600ULL * HNS_PER_SEC);

    // modulus by hns intervals per second first, then convert to ns, as not to lose resolution
    tp->tv_nsec = (long) ((hnsTime.QuadPart % HNS_PER_SEC) * NS_PER_HNS);
    tp->tv_sec = (long) (hnsTime.QuadPart / HNS_PER_SEC);

    return 0;
}

CAT_API int cat_clock_gettime_monotonic(struct timespec *tp)
{
    static LARGE_INTEGER ticksPerSec;
    LARGE_INTEGER ticks;

    if (!ticksPerSec.QuadPart) {
        QueryPerformanceFrequency(&ticksPerSec);
        if (!ticksPerSec.QuadPart) {
            errno = ENOTSUP;
            return -1;
        }
    }

    QueryPerformanceCounter(&ticks);

    tp->tv_sec = (long)(ticks.QuadPart / ticksPerSec.QuadPart);
    tp->tv_nsec = (long)(((ticks.QuadPart % ticksPerSec.QuadPart) * NS_PER_SEC) / ticksPerSec.QuadPart);

    return 0;
}
#endif
