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

#include "cat_time.h"
#include "cat_coroutine.h"
#include "cat_event.h"

#ifndef CAT_OS_WIN
#include "../deps/libuv/src/unix/internal.h"
#include "unistd.h"
#else
#include "../deps/libuv/src/win/internal.h"
#include "windows.h"
#endif

CAT_API cat_nsec_t cat_time_nsec(void)
{
    return uv_hrtime();
}

CAT_API cat_msec_t cat_time_msec(void)
{
#ifndef CAT_OS_WIN
    return uv__hrtime(UV_CLOCK_FAST) / (1000 * 1000);
#else
    return uv__hrtime(1000);
#endif
}

CAT_API char *cat_time_format_msec(cat_msec_t msec)
{
#define DAY    (24 * HOUR)
#define HOUR   (60 * MINUTE)
#define MINUTE (60 * SECOND)
#define SECOND 1000
    cat_msec_t rmsec = msec;
    int day, hour, minute, second;

    day = msec / DAY;
    rmsec -= (day * DAY);
    hour = rmsec / HOUR;
    rmsec -= (hour * HOUR);
    minute = rmsec / MINUTE;
    rmsec -= (minute * MINUTE);
    second = rmsec / SECOND;
    rmsec -= (second * SECOND);

    if (day > 0) {
        return cat_sprintf("%dd%dh%dm%ds%dms", day, hour, minute, second, (int) rmsec);
    } else if (hour > 0) {
        return cat_sprintf("%dh%dm%ds%dms", hour, minute, second, (int) rmsec);
    } else if (minute > 0) {
        return cat_sprintf("%dm%ds%dms", minute, second, (int) rmsec);
    } else if (second > 0) {
        return cat_sprintf("%ds%dms", second, (int) rmsec);
    } else {
        return cat_sprintf("%dms", (int) rmsec);
    }
#undef DAY
#undef HOUR
#undef MINUTE
#undef SECOND
}

typedef union
{
    cat_coroutine_t *coroutine;
    uv_handle_t handle;
    uv_timer_t timer;
} cat_timer_t;

static void cat_sleep_timer_callback(uv_timer_t* handle)
{
    cat_timer_t *timer = (cat_timer_t *) handle;
    cat_coroutine_t *coroutine = timer->coroutine;

    timer->coroutine = NULL;

    if (unlikely(!cat_coroutine_resume(coroutine, NULL, NULL))) {
        cat_core_error_with_last(TIME, "Timer schedule failed");
    }
}

static cat_timer_t *cat_timer_wait(cat_msec_t msec)
{
    cat_timer_t *timer;
    cat_bool_t ret;

    timer = (cat_timer_t *) cat_malloc(sizeof(*timer));;

    if (unlikely(timer == NULL)) {
        cat_update_last_error_of_syscall("Malloc for timer failed");
        return NULL;
    }

    (void) uv_timer_init(cat_event_loop, &timer->timer);
    (void) uv_timer_start(&timer->timer, cat_sleep_timer_callback, msec, 0);
#ifdef CAT_DEBUG
    do {
        char *tmp = NULL;
        cat_debug(TIME, "Sleep %s", tmp = cat_time_format_msec(msec));
        if (tmp != NULL) { cat_free(tmp); }
    } while (0);
#endif

    timer->coroutine = CAT_COROUTINE_G(current);

    ret = cat_coroutine_yield(NULL, NULL);

    uv_close(&timer->handle, (uv_close_cb) cat_free_function);

    if (unlikely(!ret)) {
        cat_update_last_error_with_previous("Time sleep failed");
        return NULL;
    }

    return timer;
}

CAT_API cat_bool_t cat_time_wait(cat_timeout_t timeout)
{
    if (timeout < 0) {
        return cat_coroutine_yield(NULL, NULL);
    } else {
        cat_timer_t *timer;

        timer = cat_timer_wait(timeout);

        if (unlikely(timer == NULL)) {
            return cat_false;
        }
        if (unlikely(timer->coroutine == NULL)) {
            cat_update_last_error(CAT_ETIMEDOUT, "Timed out for " CAT_TIMEOUT_FMT " ms", timeout);
            return cat_false;
        }
    }

    return cat_true;
}

CAT_API unsigned int cat_time_sleep(unsigned int seconds)
{
    cat_msec_t ret = cat_time_msleep((cat_msec_t) (seconds * 1000)) / 1000;
    if (unlikely(ret < 0)) {
        return seconds;
    }
    return (unsigned int) ret;
}

CAT_API cat_msec_t cat_time_msleep(cat_msec_t msec)
{
    cat_timer_t *timer;

    timer = cat_timer_wait(msec);

    if (unlikely(timer == NULL)) {
        return -1;
    }

    if (unlikely(timer->coroutine != NULL)) {
        cat_msec_t reserve;
        cat_update_last_error(CAT_ECANCELED, "Time waiter has been canceled");
        reserve = timer->timer.timeout - cat_event_loop->time;
        if (unlikely(reserve <= 0)) {
            /* blocking IO lead it to be negative or 0
             * we can not know the real reserve time */
            return msec;
        }
        return reserve;
    }

    return 0;
}

CAT_API int cat_time_usleep(uint64_t microseconds)
{
    cat_msec_t msec = ((double) microseconds) / 1000;
    return likely(cat_time_msleep(msec) == 0) ? 0 : -1;
}

CAT_API int cat_time_nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
    if (unlikely(rqtp->tv_sec < 0 || rqtp->tv_nsec < 0 || rqtp->tv_nsec > 999999999)) {
        cat_update_last_error(CAT_EINVAL, "The value in the tv_nsec field was not in the range 0 to 999999999 or tv_sec was negative");
        return -1;
    } else {
        uint64_t msec = rqtp->tv_sec * 1000 + (((double) rqtp->tv_nsec) / (1000 * 1000));
        if (unlikely(msec > INT64_MAX)) {
            msec = INT64_MAX;
        }
        cat_msec_t rmsec = cat_time_msleep(msec);
        if (rmsec > 0 && rmtp) {
            rmtp->tv_sec = rmsec - (rmsec % 1000);
            rmtp->tv_nsec = (rmsec % 1000) * 1000 * 1000;
        }
        return likely(rmsec == 0) ? 0 : -1;
    }
}

CAT_API cat_timeout_t cat_time_tv2to(const struct timeval *tv)
{
    if (tv && tv->tv_sec > 0) {
        return (tv->tv_sec * 1000) + (tv->tv_usec / 1000);
    }
    return -1;
}
