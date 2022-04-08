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
    uv__once_init();
    return uv__hrtime(1000);
#endif
}

CAT_API cat_msec_t cat_time_msec_cached(void)
{
    return CAT_EVENT_G(loop).time;
}

CAT_API cat_nsec_t cat_time_nsec2(void)
{
    uv_timeval64_t tv;
    (void) uv_gettimeofday(&tv);
    return ((cat_nsec_t) (tv.tv_sec * 1000000)) + ((cat_nsec_t) (tv.tv_usec));
}

CAT_API cat_msec_t cat_time_msec2(void)
{
    uv_timeval64_t tv;
    (void) uv_gettimeofday(&tv);
    return ((cat_msec_t) (tv.tv_sec * 1000)) + ((cat_msec_t) (tv.tv_usec / 1000.00));
}

CAT_API double cat_microtime(void)
{
    uv_timeval64_t tv;
    (void) uv_gettimeofday(&tv);
    return (double) (tv.tv_sec + tv.tv_usec / 1000000.00);
}

CAT_API char *cat_time_format_msec(cat_msec_t msec)
{
#define DAY    (24ULL * HOUR)
#define HOUR   (60ULL * MINUTE)
#define MINUTE (60ULL * SECOND)
#define SECOND 1000ULL
    cat_msec_t rmsec = msec;
    uint64_t day, hour, minute, second;

    day = (uint64_t) (msec / DAY);
    rmsec -= (day * DAY);
    hour = (uint64_t) (rmsec / HOUR);
    rmsec -= (hour * HOUR);
    minute = (uint64_t) (rmsec / MINUTE);
    rmsec -= (minute * MINUTE);
    second = (uint64_t) (rmsec / SECOND);
    rmsec -= (second * SECOND);

    if (day > 0) {
        return cat_sprintf("%" PRIu64 "d%" PRIu64 "h%" PRIu64 "m%" PRIu64 "s%" PRIu64 "ms", day, hour, minute, second, (uint64_t) rmsec);
    } else if (hour > 0) {
        return cat_sprintf("%" PRIu64 "h%" PRIu64 "m%" PRIu64 "s%" PRIu64 "ms", hour, minute, second, (uint64_t) rmsec);
    } else if (minute > 0) {
        return cat_sprintf("%" PRIu64 "m%" PRIu64 "s%" PRIu64 "ms", minute, second, (uint64_t) rmsec);
    } else if (second > 0) {
        return cat_sprintf("%" PRIu64 "s%" PRIu64 "ms", second, (uint64_t) rmsec);
    } else {
        return cat_sprintf("%" PRIu64 "ms", (uint64_t) rmsec);
    }
#undef DAY
#undef HOUR
#undef MINUTE
#undef SECOND
}

typedef union {
    cat_coroutine_t *coroutine;
    uv_handle_t handle;
    uv_timer_t timer;
} cat_timer_t;

static void cat_sleep_timer_callback(uv_timer_t* handle)
{
    cat_timer_t *timer = (cat_timer_t *) handle;
    cat_coroutine_t *coroutine = timer->coroutine;

    timer->coroutine = NULL;
    cat_coroutine_schedule(coroutine, TIME, "Timer");
}

static cat_timer_t *cat_timer_wait(cat_msec_t msec)
{
    cat_timer_t *timer;
    cat_bool_t ret;

    timer = (cat_timer_t *) cat_malloc(sizeof(*timer));;
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(timer == NULL)) {
        cat_update_last_error_of_syscall("Malloc for timer failed");
        return NULL;
    }
#endif

    (void) uv_timer_init(&CAT_EVENT_G(loop), &timer->timer);
    (void) uv_timer_start(&timer->timer, cat_sleep_timer_callback, msec, 0);

    CAT_LOG_DEBUG_SCOPE_START_EX(TIME, char *tmp) {
        CAT_LOG_DEBUG_D(TIME, "Sleep %s", tmp = cat_time_format_msec(msec));
    } CAT_LOG_DEBUG_SCOPE_END_EX(cat_free(tmp));

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

CAT_API cat_ret_t cat_time_delay(cat_timeout_t timeout)
{
    if (timeout < 0) {
        cat_coroutine_yield(NULL, NULL);
    } else {
        cat_timer_t *timer;

        timer = cat_timer_wait(timeout);

        if (unlikely(timer == NULL)) {
            return CAT_RET_ERROR;
        }
        if (timer->coroutine == NULL) {
            return CAT_RET_OK;
        }
    }

    return CAT_RET_NONE;
}

CAT_API unsigned int cat_time_sleep(unsigned int seconds)
{
    return (unsigned int) cat_time_msleep((cat_msec_t) (seconds * 1000)) / 1000;
}

CAT_API cat_msec_t cat_time_msleep(cat_msec_t msec)
{
    cat_timer_t *timer;

    timer = cat_timer_wait(msec);

    if (unlikely(timer == NULL)) {
        return msec;
    }

    if (unlikely(timer->coroutine != NULL)) {
        cat_update_last_error(CAT_ECANCELED, "Time waiter has been canceled");
        if (unlikely(timer->timer.timeout <= CAT_EVENT_G(loop).time)) {
            /* blocking IO lead it to be negative or 0
             * we can not know the real reserve time */
            return msec;
        }
        return timer->timer.timeout - CAT_EVENT_G(loop).time;
    }

    return 0;
}

CAT_API int cat_time_usleep(cat_usec_t microseconds)
{
    return cat_time_msleep((cat_msec_t) (((double) microseconds) / 1000)) == 0 ? 0 : -1;
}

CAT_API int cat_time_nanosleep(const struct cat_timespec *req, struct cat_timespec *rem)
{
    if (unlikely(req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec > 999999999)) {
        cat_update_last_error(CAT_EINVAL, "The value in the tv_nsec field was not in the range 0 to 999999999 or tv_sec was negative");
        return -1;
    } else {
        cat_msec_t msec = (uint64_t) (req->tv_sec * 1000 + (((double) req->tv_nsec) / (1000 * 1000)));
        cat_msec_t rmsec = cat_time_msleep(msec);
        if (rmsec > 0 && rem) {
            rem->tv_sec = (rmsec - (rmsec % 1000)) / 1000;
            rem->tv_nsec = (rmsec % 1000) * 1000 * 1000;
        }
        return rmsec == 0 ? 0 : -1;
    }
}

CAT_API cat_timeout_t cat_time_tv2to(const struct timeval *tv)
{
    cat_timeout_t timeout;

    if (tv == NULL) {
        timeout = CAT_TIMEOUT_FOREVER;
    } else {
        timeout = (tv->tv_sec * 1000) + (tv->tv_usec / 1000);
        if (unlikely(timeout < 0)) {
            timeout = CAT_TIMEOUT_FOREVER;
        }
    }

    return timeout;
}
