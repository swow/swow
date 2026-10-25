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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#include "cat.h"
#include "cat_futex.h"
#include "cat_time.h"

#if defined(CAT_OS_LINUX)

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#elif defined(CAT_OS_DARWIN) && defined(__clang__)
/*
 * The wait on an address arrived in macOS 14.4. An SDK that has no header for it
 * builds the fallback below, and the library then reports CAT_ENOTSUP.
 */
# if defined(__has_include) && __has_include(<os/os_sync_wait_on_address.h>)
#  define CAT_HAVE_OS_SYNC_WAIT_ON_ADDRESS 1
#  include <os/os_sync_wait_on_address.h>
# endif
#endif

#ifdef CAT_OS_WIN

static cat_bool_t cat_futex_wait_at(cat_futex_t *futex, int32_t expected, cat_timeout_t timeout)
{
    cat_msec_t deadline = timeout > 0 ? cat_time_msec() + (cat_msec_t) timeout : 0;

    for (;;) {
        DWORD wait = INFINITE;

        if (timeout > 0) {
            cat_msec_t now = cat_time_msec();
            cat_msec_t remaining = deadline > now ? deadline - now : 0;

            if (remaining == 0) {
                cat_update_last_error(CAT_ETIMEDOUT, "Timed out waiting for the value");
                return cat_false;
            }
            /* the wait takes a 32-bit count of msec, so a longer rest repeats */
            wait = remaining > 0x7FFFFFFF ? 0x7FFFFFFF : (DWORD) remaining;
        }

        if (WaitOnAddress((volatile void *) &futex->value, &expected, sizeof(expected), wait)) {
            return cat_true;
        }

        {
            DWORD error = GetLastError();

            if (error == ERROR_TIMEOUT) {
                continue;
            }
            cat_update_last_error_with_reason(cat_translate_sys_error((int) error),
                "WaitOnAddress failed");
        }

        return cat_false;
    }
}

static cat_bool_t cat_futex_wake_at(cat_futex_t *futex, int32_t count)
{
    int32_t index;

    if (count < 0) {
        WakeByAddressAll((PVOID) &futex->value);
        return cat_true;
    }

    for (index = 0; index < count; index++) {
        WakeByAddressSingle((PVOID) &futex->value);
    }

    return cat_true;
}

#elif defined(CAT_OS_LINUX)

static int cat_futex_wait_syscall(cat_futex_t *futex, int32_t expected, const struct timespec *timeout)
{
    return (int) syscall(SYS_futex, &futex->value, FUTEX_WAIT, (int) expected, timeout, NULL, 0);
}

static cat_bool_t cat_futex_wait_at(cat_futex_t *futex, int32_t expected, cat_timeout_t timeout)
{
    cat_msec_t deadline = timeout > 0 ? cat_time_msec() + (cat_msec_t) timeout : 0;

    for (;;) {
        struct timespec wait;
        const struct timespec *timeout_arg = NULL;

        if (timeout > 0) {
            cat_msec_t now = cat_time_msec();
            cat_msec_t remaining = deadline > now ? deadline - now : 0;

            if (remaining == 0) {
                cat_update_last_error(CAT_ETIMEDOUT, "Timed out waiting for the value");
                return cat_false;
            }
            wait.tv_sec = (time_t) (remaining / 1000);
            wait.tv_nsec = (long) (remaining % 1000) * 1000000L;
            timeout_arg = &wait;
        }

        if (cat_futex_wait_syscall(futex, expected, timeout_arg) == 0) {
            return cat_true;
        }

        switch (errno) {
        case EAGAIN:
            cat_update_last_error(CAT_EAGAIN, "The value is not the expected one");
            return cat_false;
        case ETIMEDOUT:
            cat_update_last_error(CAT_ETIMEDOUT, "Timed out waiting for the value");
            return cat_false;
        case EINTR:
            /* a signal ended the wait, and the value may have changed meanwhile */
            continue;
        default:
            cat_update_last_error_with_reason(cat_translate_sys_error(errno), "Futex wait failed");
            return cat_false;
        }
    }
}

static cat_bool_t cat_futex_wake_at(cat_futex_t *futex, int32_t count)
{
    int32_t waiters = count < 0 ? INT32_MAX : count;

    if ((int) syscall(SYS_futex, &futex->value, FUTEX_WAKE, (int) waiters, NULL, NULL, 0) < 0) {
        cat_update_last_error_with_reason(cat_translate_sys_error(errno), "Futex wake failed");
        return cat_false;
    }

    return cat_true;
}

#elif defined(CAT_HAVE_OS_SYNC_WAIT_ON_ADDRESS)

static cat_bool_t cat_futex_wait_at(cat_futex_t *futex, int32_t expected, cat_timeout_t timeout)
{
    cat_msec_t deadline = timeout > 0 ? cat_time_msec() + (cat_msec_t) timeout : 0;
    uint64_t value = (uint64_t) (uint32_t) expected;

    if (__builtin_available(macOS 14.4, *)) {
        for (;;) {
            int result;

            if (timeout > 0) {
                cat_msec_t now = cat_time_msec();
                cat_msec_t remaining = deadline > now ? deadline - now : 0;

                if (remaining == 0) {
                    cat_update_last_error(CAT_ETIMEDOUT, "Timed out waiting for the value");
                    return cat_false;
                }
                result = os_sync_wait_on_address_with_timeout(&futex->value, value, sizeof(int32_t),
                    OS_SYNC_WAIT_ON_ADDRESS_SHARED, OS_CLOCK_MACH_ABSOLUTE_TIME, remaining * 1000000ULL);
            } else {
                result = os_sync_wait_on_address(&futex->value, value, sizeof(int32_t),
                    OS_SYNC_WAIT_ON_ADDRESS_SHARED);
            }

            if (result >= 0) {
                return cat_true;
            }

            if (errno == ETIMEDOUT) {
                cat_update_last_error(CAT_ETIMEDOUT, "Timed out waiting for the value");
                return cat_false;
            }
            if (errno == EINTR || errno == EFAULT) {
                continue;
            }

            cat_update_last_error_with_reason(cat_translate_sys_error(errno),
                "os_sync_wait_on_address failed");
            return cat_false;
        }
    }

    cat_update_last_error(CAT_ENOTSUP, "This macOS version has no wait on an address");
    return cat_false;
}

static cat_bool_t cat_futex_wake_at(cat_futex_t *futex, int32_t count)
{
    int32_t index;

    if (__builtin_available(macOS 14.4, *)) {
        if (count < 0) {
            if (os_sync_wake_by_address_all(&futex->value, sizeof(int32_t),
                OS_SYNC_WAKE_BY_ADDRESS_SHARED) != 0 && errno != ENOENT) {
                cat_update_last_error_with_reason(cat_translate_sys_error(errno),
                    "os_sync_wake_by_address_all failed");
                return cat_false;
            }
            return cat_true;
        }

        for (index = 0; index < count; index++) {
            if (os_sync_wake_by_address_any(&futex->value, sizeof(int32_t),
                OS_SYNC_WAKE_BY_ADDRESS_SHARED) != 0) {
                /* ENOENT: no waiter is left to wake */
                if (errno == ENOENT) {
                    break;
                }
                cat_update_last_error_with_reason(cat_translate_sys_error(errno),
                    "os_sync_wake_by_address_any failed");
                return cat_false;
            }
        }

        return cat_true;
    }

    cat_update_last_error(CAT_ENOTSUP, "This macOS version has no wake on an address");
    return cat_false;
}

#else /* no wait on an address */

static cat_bool_t cat_futex_wait_at(cat_futex_t *futex, int32_t expected, cat_timeout_t timeout)
{
    (void) futex;
    (void) expected;
    (void) timeout;

    cat_update_last_error(CAT_ENOTSUP, "This platform has no wait on an address");
    return cat_false;
}

static cat_bool_t cat_futex_wake_at(cat_futex_t *futex, int32_t count)
{
    (void) futex;
    (void) count;

    cat_update_last_error(CAT_ENOTSUP, "This platform has no wake on an address");
    return cat_false;
}

#endif

CAT_API cat_bool_t cat_futex_wait(cat_futex_t *futex, int32_t expected, cat_timeout_t timeout)
{
    /*
     * A Windows or a macOS wait reports success for a value that already differs.
     * Check the value here, so that every platform reports the same error.
     */
    if (cat_atomic_int32_load(futex) != expected) {
        cat_update_last_error(CAT_EAGAIN, "The value is not the expected one");
        return cat_false;
    }

    if (timeout == 0) {
        cat_update_last_error(CAT_ETIMEDOUT, "The timeout ran out before the wait");
        return cat_false;
    }

    return cat_futex_wait_at(futex, expected, timeout);
}

CAT_API cat_bool_t cat_futex_wake(cat_futex_t *futex, int32_t count)
{
    /* Windows and macOS wake one waiter for any count above zero, so zero leaves
     * here before it reaches them */
    if (count == 0) {
        return cat_true;
    }

    return cat_futex_wake_at(futex, count);
}

CAT_API int32_t cat_futex_add(cat_futex_t *futex, int32_t delta)
{
    return cat_atomic_int32_fetch_add(futex, delta);
}

CAT_API int32_t cat_futex_exchange(cat_futex_t *futex, int32_t new_value)
{
    return cat_atomic_int32_exchange(futex, new_value);
}

CAT_API cat_bool_t cat_futex_compare_exchange(cat_futex_t *futex, int32_t expected, int32_t new_value)
{
    return cat_atomic_int32_compare_exchange_strong(futex, &expected, new_value);
}
