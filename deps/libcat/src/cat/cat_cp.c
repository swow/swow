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

#ifdef CAT_OS_WIN
CAT_API char *strndup(const char *string, size_t length)
{
    char *buffer;

    buffer = (char *) cat_malloc(length + 1);
    if (likely(buffer != NULL)) {
        size_t n = 0;
        for (; ((n < length) && (string[n] != '\0')); n++) {
            buffer[n] = string[n];
        }
        buffer[n] = '\0';
    }

    return buffer;
}

CAT_API int setenv(const char *name, const char *value, int overwrite)
{
    (void) overwrite;
    return uv_os_setenv(name, value);
}

CAT_API int gettimeofday(struct timeval *time_info, struct timezone *timezone_info)
{
    int error = 0;

    /* Get the time, if they want it */
    if (time_info != NULL) {
        error = uv_gettimeofday(&time_info);
    }
    /* Get the timezone, if they want it */
    if (timezone_info != NULL) {
        _tzset();
        timezone_info->tz_minuteswest = _timezone;
        timezone_info->tz_dsttime = _daylight;
    }

    return error;
}

CAT_API unsigned int sleep(unsigned int seconds)
{
    return SleepEx(seconds * 1000, TRUE);
}

CAT_API int usleep(unsigned int useconds)
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

CAT_API int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
    if (rqtp->tv_nsec > 999999999) {
        /* The time interval specified 1,000,000 or more microseconds. */
        cat_set_sys_errno(WSAEINVAL);
        return -1;
    }
    return usleep(rqtp->tv_sec * 1000000 + rqtp->tv_nsec / 1000);
}
#endif
