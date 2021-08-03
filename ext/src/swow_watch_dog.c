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

#include "swow_watch_dog.h"

#include "cat_time.h" /* for time_wait() */

SWOW_API zend_class_entry *swow_watch_dog_ce;
SWOW_API zend_object_handlers swow_watch_dog_handlers;

SWOW_API zend_class_entry *swow_watch_dog_exception_ce;

typedef void (*swow_interrupt_function_t)(zend_execute_data *execute_data);

static swow_interrupt_function_t original_zend_interrupt_function = (swow_interrupt_function_t) -1;

SWOW_API void swow_watch_dog_alert_standard(cat_watch_dog_t *watch_dog)
{
    swow_watch_dog_t *swatch_dog = swow_watch_dog_get_from_handle(watch_dog);
    zend_bool vm_interrupted = swatch_dog->vm_interrupted;

    // CPU starvation (and we should try to schedule the coroutine)
    swatch_dog->vm_interrupted = 0;
    *swatch_dog->vm_interrupt_ptr = 1;

    if (watch_dog->alert_count > 1 &&
        vm_interrupted == 0 /* interrupt maybe failed */
    ) {
        if (watch_dog->threshold > 0 && /* blocking time is greater than syscall threshold */
            ((cat_timeout_t) (watch_dog->quantum * watch_dog->alert_count)) > watch_dog->threshold
        ) {
            /* Syscall blocking
             * CPU starvation is also possbile,
             * the machine performance is too bad (such as mine),
             * VM has not interrupted yet */
            cat_watch_dog_alert_standard(watch_dog);
        }
    }
}

static void swow_watch_dog_interrupt_function(zend_execute_data *execute_data)
{
    if (cat_watch_dog_is_running()) {
        swow_watch_dog_t *swatch_dog = swow_watch_dog_get_current();
        cat_watch_dog_t *watch_dog = &swatch_dog->watch_dog;
        swatch_dog->vm_interrupted = 1;
        /* re-check if current round still equal to last_round  */
        if (CAT_COROUTINE_G(round) == watch_dog->last_round) {
            if (swatch_dog->alerter.function_handler == NULL) {
                if (!cat_time_wait(swatch_dog->delay) &&
                    cat_get_last_error_code() != CAT_ETIMEDOUT
                ) {
                    cat_core_error_with_last(WATCH_DOG, "WatchDog interrupt schedule failed");
                }
            } else {
                zend_fcall_info fci;
                zval retval;
                fci.size = sizeof(fci);
                ZVAL_UNDEF(&fci.function_name);
                fci.object = NULL;
                fci.param_count = 0;
#if PHP_VERSION_ID >= 80000
                fci.named_params = NULL;
#else
                fci.no_separation = 0;
#endif
                fci.retval = &retval;
                (void) zend_call_function(&fci, &swatch_dog->alerter);
                zval_ptr_dtor(&retval);
            }
        }
    }

    if (original_zend_interrupt_function != NULL) {
        original_zend_interrupt_function(execute_data);
    }
}

SWOW_API cat_bool_t swow_watch_dog_run(cat_timeout_t quantum, cat_timeout_t threshold, zval *zalerter)
{
    swow_watch_dog_t *swatch_dog;
    zend_fcall_info_cache fcc = empty_fcall_info_cache;
    cat_timeout_t delay = 0;
    cat_bool_t ret;

    if (zalerter != NULL) {
        switch (Z_TYPE_P(zalerter)) {
            case IS_NULL:
            case IS_LONG:
            case IS_DOUBLE:
                delay = zval_get_long(zalerter);
                zalerter = NULL;
                break;
            default: {
                char *error;
                if (!zend_is_callable_ex(zalerter, NULL, 0, NULL, &fcc, &error)) {
                    cat_update_last_error(CAT_EMISUSE, "WatchDog alerter must be numeric or callable, %s", error);
                    efree(error);
                    return cat_false;
                }
                efree(error);
            }
        }
    }

    swatch_dog = (swow_watch_dog_t *) emalloc(sizeof(*swatch_dog));
    swatch_dog->vm_interrupt_ptr = &EG(vm_interrupt);
    swatch_dog->delay = delay;
    swatch_dog->alerter = fcc;
    if (zalerter != NULL) {
        ZVAL_COPY(&swatch_dog->zalerter, zalerter);
    } else {
        ZVAL_NULL(&swatch_dog->zalerter);
    }

    ret = cat_watch_dog_run(&swatch_dog->watch_dog, quantum, threshold, swow_watch_dog_alert_standard);

    if (!ret) {
        Z_TRY_DELREF(swatch_dog->zalerter);
        efree(swatch_dog);
        return cat_false;
    }

    return cat_true;
}

SWOW_API cat_bool_t swow_watch_dog_stop(void)
{
    swow_watch_dog_t *swatch_dog = swow_watch_dog_get_current();
    cat_bool_t ret;

    ret = cat_watch_dog_stop();

    if (!ret) {
        return cat_false;
    }

    zval_ptr_dtor(&swatch_dog->zalerter);
    efree(swatch_dog);

    return cat_true;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WatchDog_run, ZEND_RETURN_VALUE, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, quantum, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, threshold, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_MASK(0, alerter, MAY_BE_NULL | MAY_BE_LONG | MAY_BE_DOUBLE | MAY_BE_CALLABLE, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WatchDog, run)
{
    zend_long quantum = 0;
    zend_long threshold = 0;
    zval *zalerter = NULL;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 3)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(quantum)
        Z_PARAM_LONG(threshold)
        Z_PARAM_ZVAL(zalerter)
    ZEND_PARSE_PARAMETERS_END();

    if (original_zend_interrupt_function == (swow_interrupt_function_t) -1) {
        original_zend_interrupt_function = zend_interrupt_function;
        zend_interrupt_function = swow_watch_dog_interrupt_function;
    }

    ret = swow_watch_dog_run(quantum, threshold, zalerter);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_watch_dog_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WatchDog_stop, ZEND_RETURN_VALUE, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WatchDog, stop)
{
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = cat_watch_dog_stop();

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_watch_dog_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WatchDog_isRunning, ZEND_RETURN_VALUE, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WatchDog, isRunning)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_watch_dog_is_running());
}

static const zend_function_entry swow_watch_dog_methods[] = {
    PHP_ME(Swow_WatchDog, run,       arginfo_class_Swow_WatchDog_run,       ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WatchDog, stop,      arginfo_class_Swow_WatchDog_stop,      ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WatchDog, isRunning, arginfo_class_Swow_WatchDog_isRunning, ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_FE_END
};

int swow_watch_dog_module_init(INIT_FUNC_ARGS)
{
    if (!cat_watch_dog_module_init()) {
        return FAILURE;
    }

    swow_watch_dog_ce = swow_register_internal_class(
        "Swow\\WatchDog", NULL, swow_watch_dog_methods,
        &swow_watch_dog_handlers, NULL,
        cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );

    swow_watch_dog_exception_ce = swow_register_internal_class(
        "Swow\\WatchDog\\Exception", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}

int swow_watch_dog_runtime_init(INIT_FUNC_ARGS)
{
    if (!cat_watch_dog_runtime_init()) {
        return FAILURE;
    }

    return SUCCESS;
}

int swow_watch_dog_runtime_shudtown(INIT_FUNC_ARGS)
{
    if (cat_watch_dog_is_running() && !swow_watch_dog_stop()) {
        cat_core_error_with_last(WATCH_DOG, "WatchDog stop failed");
    }

    if (!cat_watch_dog_runtime_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}
