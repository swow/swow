/*
  +--------------------------------------------------------------------------+
  | Swow                                                                     |
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

#include "swow_time.h"

#include "swow_hook.h"

#ifdef PHP_WIN32
# include "win32/time.h"
#endif // PHP_WIN32

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_sleep, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, seconds, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* {{{ proto void sleep(int seconds): int
   Delay for a given number of seconds */
static PHP_FUNCTION(swow_sleep)
{
    zend_long seconds;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(seconds)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(seconds < 0)) {
        zend_argument_value_error(1, "must be greater than or equal to 0");
        RETURN_THROWS();
    }

    RETURN_LONG(cat_time_sleep(seconds));
}
/* }}} */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_msleep, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, milli_seconds, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* {{{ proto void msleep(int milliseconds): int
   Delay for a given number of micro seconds */
static PHP_FUNCTION(swow_msleep)
{
    zend_long milli_seconds;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(milli_seconds)
    ZEND_PARSE_PARAMETERS_END();

    if (milli_seconds < 0) {
        zend_argument_value_error(1, "must be greater than or equal to 0");
        RETURN_THROWS();
    }

    RETURN_LONG(cat_time_msleep((uint64_t) milli_seconds));
}
/* }}} */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_usleep, ZEND_RETURN_VALUE, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, micro_seconds, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* {{{ proto void usleep(int micro_seconds): void
   Delay for a given number of micro seconds */
static PHP_FUNCTION(swow_usleep)
{
    zend_long micro_seconds;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(micro_seconds)
    ZEND_PARSE_PARAMETERS_END();

    if (micro_seconds < 0) {
        zend_argument_value_error(1, "must be greater than or equal to 0");
        RETURN_THROWS();
    }

    (void) cat_time_usleep((unsigned int) micro_seconds);
}
/* }}} */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_swow_nanosleep, ZEND_RETURN_VALUE, 2, MAY_BE_ARRAY|MAY_BE_BOOL)
    ZEND_ARG_TYPE_INFO(0, seconds, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, nanoseconds, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* {{{ proto mixed time_nanosleep(int seconds, int nanoseconds): bool|array
   Delay for a number of seconds and nano seconds */
static PHP_FUNCTION(swow_nanosleep)
{
    zend_long tv_sec, tv_nsec;
    struct cat_timespec php_req, php_rem;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(tv_sec)
        Z_PARAM_LONG(tv_nsec)
    ZEND_PARSE_PARAMETERS_END();

    if (tv_sec < 0) {
        zend_argument_value_error(1, "must be greater than or equal to 0");
        RETURN_THROWS();
    }
    if (tv_nsec < 0) {
        zend_argument_value_error(2, "must be greater than or equal to 0");
        RETURN_THROWS();
    }

    php_req.tv_sec = (time_t) tv_sec;
    php_req.tv_nsec = (long) tv_nsec;
    if (cat_time_nanosleep((const struct cat_timespec *)&php_req, &php_rem) == 0) {
        RETURN_TRUE;
    } else if (cat_get_last_error_code() == CAT_ECANCELED)  {
        array_init(return_value);
        add_assoc_long_ex(return_value, ZEND_STRL("seconds"), php_rem.tv_sec);
        add_assoc_long_ex(return_value, ZEND_STRL("nanoseconds"), php_rem.tv_nsec);
        return;
    } else if (cat_get_last_error_code() == CAT_EINVAL) {
        zend_value_error("Nanoseconds was not in the range 0 to 999 999 999 or seconds was negative");
        RETURN_THROWS();
    }
}
/* }}} */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_sleep_until, ZEND_RETURN_VALUE, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, timestamp, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* {{{ proto bool time_sleep_until(float timestamp): bool
   Make the script sleep until the specified time */
static PHP_FUNCTION(swow_sleep_until)
{
    double target_secs;
    struct timeval tm;
    struct cat_timespec php_req, php_rem;
    uint64_t current_ns, target_ns, diff_ns;
    const uint64_t ns_per_sec = 1000000000;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_DOUBLE(target_secs)
    ZEND_PARSE_PARAMETERS_END();

    if (gettimeofday((struct timeval *) &tm, NULL) != 0) {
        RETURN_FALSE;
    }

    target_ns = (uint64_t) (target_secs * ns_per_sec);
    current_ns = ((uint64_t) tm.tv_sec) * ns_per_sec + ((uint64_t) tm.tv_usec) * 1000;
    if (target_ns < current_ns) {
        php_error_docref(NULL, E_WARNING, "Argument #1 ($timestamp) must be greater than or equal to the current time");
        RETURN_FALSE;
    }

    diff_ns = target_ns - current_ns;
    php_req.tv_sec = (time_t) (diff_ns / ns_per_sec);
    php_req.tv_nsec = (long) (diff_ns % ns_per_sec);

    RETURN_BOOL(cat_time_nanosleep((const struct cat_timespec *)&php_req, &php_rem) == 0);
}
/* }}} */

static const zend_function_entry swow_time_functions[] = {
    PHP_FENTRY(sleep,            PHP_FN(swow_sleep),       arginfo_swow_sleep,       0)
    PHP_FENTRY(msleep,           PHP_FN(swow_msleep),      arginfo_swow_msleep,      0)
    PHP_FENTRY(usleep,           PHP_FN(swow_usleep),      arginfo_swow_usleep,      0)
    PHP_FENTRY(time_nanosleep,   PHP_FN(swow_nanosleep),   arginfo_swow_nanosleep,   0)
    PHP_FENTRY(time_sleep_until, PHP_FN(swow_sleep_until), arginfo_swow_sleep_until, 0)
    PHP_FE_END
};

zend_result swow_time_module_init(INIT_FUNC_ARGS)
{
    SWOW_MODULES_CHECK_PRE_START() {
        "core"
    } SWOW_MODULES_CHECK_PRE_END();

    if (!swow_hook_internal_functions(swow_time_functions)) {
        return FAILURE;
    }

    return SUCCESS;
}
