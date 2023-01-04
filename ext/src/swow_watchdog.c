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

#include "swow_watchdog.h"

#include "cat_time.h" /* for time_wait() */

SWOW_API zend_class_entry *swow_watchdog_ce;

SWOW_API zend_class_entry *swow_watchdog_exception_ce;

typedef void (*swow_interrupt_function_t)(zend_execute_data *execute_data);

static swow_interrupt_function_t original_zend_interrupt_function = (swow_interrupt_function_t) -1;

SWOW_API void swow_watchdog_alert_standard(cat_watchdog_t *watchdog)
{
    swow_watchdog_t *s_watchdog = swow_watchdog_get_from_handle(watchdog);
    cat_bool_t vm_interrupted;

    // CPU starvation (and we should try to schedule the coroutine)

    vm_interrupted = cat_atomic_bool_exchange(&s_watchdog->vm_interrupted, cat_false);
    zend_atomic_bool_store(s_watchdog->vm_interrupt_ptr, 1);

    if (
        watchdog->alert_count > 1 &&
        vm_interrupted == 0 /* interrupt maybe failed */
    ) {
        if (
            watchdog->threshold > 0 && /* blocking time is greater than syscall threshold */
            ((cat_timeout_t) (watchdog->quantum * watchdog->alert_count)) > watchdog->threshold
        ) {
            /* Syscall blocking
             * CPU starvation is also possible,
             * the machine performance is too bad (such as mine),
             * VM has not interrupted yet */
            cat_watchdog_alert_standard(watchdog);
        }
    }
}

static void swow_watchdog_interrupt_function(zend_execute_data *execute_data)
{
    if (cat_watchdog_is_running()) {
        swow_watchdog_t *s_watchdog = swow_watchdog_get_current();
        cat_watchdog_t *watchdog = &s_watchdog->watchdog;

        cat_atomic_bool_store(&s_watchdog->vm_interrupted, cat_true);
        /* re-check if current switches still equal to last_switches  */
        if (CAT_COROUTINE_G(switches) == watchdog->last_switches) {
            if (s_watchdog->alerter.function_handler == NULL) {
                if (
                    !cat_time_wait(s_watchdog->delay) &&
                    cat_get_last_error_code() != CAT_ETIMEDOUT
                ) {
                    CAT_CORE_ERROR_WITH_LAST(WATCH_DOG, "Watchdog interrupt schedule failed");
                }
            } else {
                zend_fcall_info fci;
                zval retval;
                fci.size = sizeof(fci);
                ZVAL_UNDEF(&fci.function_name);
                fci.object = NULL;
                fci.param_count = 0;
                fci.named_params = NULL;
                fci.retval = &retval;
                (void) zend_call_function(&fci, &s_watchdog->alerter);
                zval_ptr_dtor(&retval);
            }
        }
    }

    if (original_zend_interrupt_function != NULL) {
        original_zend_interrupt_function(execute_data);
    }
}

SWOW_API cat_bool_t swow_watchdog_run(cat_timeout_t quantum, cat_timeout_t threshold, zval *z_alerter)
{
    swow_watchdog_t *s_watchdog;
    zend_fcall_info_cache fcc = empty_fcall_info_cache;
    cat_timeout_t delay = 0;
    cat_bool_t ret;

    if (z_alerter != NULL) {
        switch (Z_TYPE_P(z_alerter)) {
            case IS_NULL:
            case IS_LONG:
            case IS_DOUBLE:
                delay = zval_get_long(z_alerter);
                z_alerter = NULL;
                break;
            default: {
                char *error;
                if (!zend_is_callable_ex(z_alerter, NULL, 0, NULL, &fcc, &error)) {
                    cat_update_last_error(CAT_EMISUSE, "Watchdog alerter must be numeric or callable, %s", error);
                    efree(error);
                    return cat_false;
                }
                efree(error);
            }
        }
    }

    s_watchdog = (swow_watchdog_t *) emalloc(sizeof(*s_watchdog));
    cat_atomic_bool_init(&s_watchdog->vm_interrupted, cat_false);
    s_watchdog->vm_interrupt_ptr = &EG(vm_interrupt);
    s_watchdog->delay = delay;
    s_watchdog->alerter = fcc;
    if (z_alerter != NULL) {
        ZVAL_COPY(&s_watchdog->z_alerter, z_alerter);
    } else {
        ZVAL_NULL(&s_watchdog->z_alerter);
    }

    ret = cat_watchdog_run(&s_watchdog->watchdog, quantum, threshold, swow_watchdog_alert_standard);

    if (!ret) {
        Z_TRY_DELREF(s_watchdog->z_alerter);
        efree(s_watchdog);
        return cat_false;
    }

    return cat_true;
}

SWOW_API cat_bool_t swow_watchdog_stop(void)
{
    swow_watchdog_t *s_watchdog = swow_watchdog_get_current();
    cat_bool_t ret;

    ret = cat_watchdog_stop();

    if (!ret) {
        return cat_false;
    }

    zval_ptr_dtor(&s_watchdog->z_alerter);
    efree(s_watchdog);

    return cat_true;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Watchdog_run, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, quantum, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, threshold, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_MASK(0, alerter, MAY_BE_CALLABLE | MAY_BE_LONG | MAY_BE_DOUBLE | MAY_BE_NULL, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Watchdog, run)
{
    zend_long quantum = 0;
    zend_long threshold = 0;
    zval *z_alerter = NULL;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 3)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(quantum)
        Z_PARAM_LONG(threshold)
        Z_PARAM_ZVAL(z_alerter)
    ZEND_PARSE_PARAMETERS_END();

    if (original_zend_interrupt_function == (swow_interrupt_function_t) -1) {
        original_zend_interrupt_function = zend_interrupt_function;
        zend_interrupt_function = swow_watchdog_interrupt_function;
    }

    ret = swow_watchdog_run(quantum, threshold, z_alerter);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_watchdog_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Watchdog_stop, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Watchdog, stop)
{
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = swow_watchdog_stop();

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_watchdog_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Watchdog_isRunning, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Watchdog, isRunning)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_watchdog_is_running());
}

static const zend_function_entry swow_watchdog_methods[] = {
    PHP_ME(Swow_Watchdog, run,       arginfo_class_Swow_Watchdog_run,       ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Watchdog, stop,      arginfo_class_Swow_Watchdog_stop,      ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Watchdog, isRunning, arginfo_class_Swow_Watchdog_isRunning, ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_watchdog_module_init(INIT_FUNC_ARGS)
{
    if (!cat_watchdog_module_init()) {
        return FAILURE;
    }

    swow_watchdog_ce = swow_register_internal_class(
        "Swow\\Watchdog", NULL, swow_watchdog_methods,
        NULL, NULL, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );

    swow_watchdog_exception_ce = swow_register_internal_class(
        "Swow\\WatchdogException", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}

zend_result swow_watchdog_module_shutdown(INIT_FUNC_ARGS)
{
    if (!cat_watchdog_module_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result swow_watchdog_runtime_init(INIT_FUNC_ARGS)
{
    if (!cat_watchdog_runtime_init()) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result swow_watchdog_runtime_shutdown(INIT_FUNC_ARGS)
{
    if (cat_watchdog_is_running() && !swow_watchdog_stop()) {
        CAT_CORE_ERROR_WITH_LAST(WATCH_DOG, "Watchdog stop failed");
    }

    if (!cat_watchdog_runtime_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}
