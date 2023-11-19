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
# include "cat_cp.h"
#endif

#ifdef CAT_OS_WIN
# include <windows.h>
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

/* select */

#ifdef CAT_OS_WIN
/* Win32 select() will only work with sockets, so we roll our own implementation here.
 * - If you supply only sockets, this simply passes through to winsock select().
 * - If you supply file handles, there is no way to distinguish between
 *   ready for read/write or OOB, so any set in which the handle is found will
 *   be marked as ready.
 * - If you supply a mixture of handles and sockets, the system will interleave
 *   calls between select() and WaitForMultipleObjects(). The time slicing may
 *   cause this function call to take up to 100 ms longer than you specified.
 * - Calling this with NULL sets as a portable way to sleep with sub-second
 *   accuracy is not supported.
 * */
CAT_API int cat_sys_select(cat_os_socket_t max_fd, fd_set *rfds, fd_set *wfds, fd_set *efds, struct timeval *tv)
{
#define SAFE_FD_ISSET(fd, set) \
        (set != NULL && FD_ISSET(fd, set))

    ULONGLONG ms_total, limit;
    HANDLE handles[MAXIMUM_WAIT_OBJECTS];
    int handle_slot_to_fd[MAXIMUM_WAIT_OBJECTS];
    int n_handles = 0, i;
    fd_set sock_read, sock_write, sock_except;
    fd_set aread, awrite, aexcept;
    int sock_max_fd = -1;
    struct timeval tvslice;
    int retcode;

    /* As max_fd is unsigned, non socket might overflow. */
    if (max_fd > (cat_os_socket_t) INT_MAX) {
        cat_set_sys_errno(WSAEINVAL);
        return -1;
    }

    if (max_fd == 0) {
        fd_set _rfds;
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCKET) {
            return -1;
        }
        FD_ZERO(&_rfds);
        FD_SET(s, &_rfds);
        select(-1, &_rfds, NULL, NULL, tv);
        closesocket(s);
        return 0;
    }

    /* calculate how long we need to wait in milliseconds */
    if (tv == NULL) {
        ms_total = INFINITE;
    } else {
        ms_total = tv->tv_sec * 1000;
        ms_total += tv->tv_usec / 1000;
    }

    FD_ZERO(&sock_read);
    FD_ZERO(&sock_write);
    FD_ZERO(&sock_except);

    /* build an array of handles for non-sockets */
    for (i = 0; (uint32_t)i < max_fd; i++) {
        if (SAFE_FD_ISSET(i, rfds) || SAFE_FD_ISSET(i, wfds) || SAFE_FD_ISSET(i, efds)) {
            handles[n_handles] = uv_get_osfhandle(i);
            if (handles[n_handles] == INVALID_HANDLE_VALUE) {
                /* socket */
                if (SAFE_FD_ISSET(i, rfds)) {
                    FD_SET((uint32_t)i, &sock_read);
                }
                if (SAFE_FD_ISSET(i, wfds)) {
                    FD_SET((uint32_t)i, &sock_write);
                }
                if (SAFE_FD_ISSET(i, efds)) {
                    FD_SET((uint32_t)i, &sock_except);
                }
                if (i > sock_max_fd) {
                    sock_max_fd = i;
                }
            } else {
                handle_slot_to_fd[n_handles] = i;
                n_handles++;
            }
        }
    }

    if (n_handles == 0) {
        /* plain sockets only - let winsock handle the whole thing */
        return select(-1, rfds, wfds, efds, tv);
    }

    /* mixture of handles and sockets; lets multiplex between
     * winsock and waiting on the handles */

    FD_ZERO(&aread);
    FD_ZERO(&awrite);
    FD_ZERO(&aexcept);

    limit = GetTickCount64() + ms_total;
    do {
        retcode = 0;

        if (sock_max_fd >= 0) {
            /* overwrite the zero'd sets here; the select call
             * will clear those that are not active */
            aread = sock_read;
            awrite = sock_write;
            aexcept = sock_except;

            tvslice.tv_sec = 0;
            tvslice.tv_usec = 100000;

            retcode = select(-1, &aread, &awrite, &aexcept, &tvslice);
        }
        if (n_handles > 0) {
            /* check handles */
            DWORD wret;

            wret = WaitForMultipleObjects(n_handles, handles, FALSE, retcode > 0 ? 0 : 100);

            if (wret == WAIT_TIMEOUT) {
                /* set retcode to 0; this is the default.
                 * select() may have set it to something else,
                 * in which case we leave it alone, so this branch
                 * does nothing */
                ;
            } else if (wret == WAIT_FAILED) {
                if (retcode == 0) {
                    retcode = -1;
                }
            } else {
                if (retcode < 0) {
                    retcode = 0;
                }
                for (i = 0; i < n_handles; i++) {
                    if (WAIT_OBJECT_0 == WaitForSingleObject(handles[i], 0)) {
                        if (SAFE_FD_ISSET(handle_slot_to_fd[i], rfds)) {
                            FD_SET((uint32_t)handle_slot_to_fd[i], &aread);
                        }
                        if (SAFE_FD_ISSET(handle_slot_to_fd[i], wfds)) {
                            FD_SET((uint32_t)handle_slot_to_fd[i], &awrite);
                        }
                        if (SAFE_FD_ISSET(handle_slot_to_fd[i], efds)) {
                            FD_SET((uint32_t)handle_slot_to_fd[i], &aexcept);
                        }
                        retcode++;
                    }
                }
            }
        }
    } while (retcode == 0 && (ms_total == INFINITE || GetTickCount64() < limit));

    if (rfds) {
        *rfds = aread;
    }
    if (wfds) {
        *wfds = awrite;
    }
    if (efds) {
        *efds = aexcept;
    }

    return retcode;

#undef SAFE_FD_ISSET
}
#endif
