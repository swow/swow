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

#include "swow_debug.h"    /* for trace functions */
#include "cat_time.h"      /* for time_wait() */

#include "zend_generators.h" /* for zend_generator_check_placeholder_frame() */

SWOW_API zend_class_entry *swow_watchdog_ce;

SWOW_API zend_class_entry *swow_watchdog_exception_ce;

static swow_interrupt_function_t original_zend_interrupt_function = (swow_interrupt_function_t) -1;

/* Try to read current execution location from executor_globals.
 * Note: Called from watchdog thread with potential race conditions.
 * The returned information may be inaccurate but should not crash. */
static void swow_watchdog_try_read_execute_location(
    zend_executor_globals *eg,
    const char **filename,
    uint32_t *lineno,
    const char **function_name,
    const char **class_name
)
{
    *filename = NULL;
    *lineno = 0;
    *function_name = NULL;
    *class_name = NULL;

    if (eg == NULL) {
        return;
    }

    /* Try to read current_execute_data (race condition possible,
     * but reading pointer value should be safe) */
    zend_execute_data *execute_data = eg->current_execute_data;

    if (execute_data == NULL) {
        return;
    }

    /* Try to read function first (check before accessing opline) */
    zend_function *func = execute_data->func;
    if (func == NULL) {
        return;
    }

    /* Read function name (safe to read pointer to string) */
    if (func->common.function_name != NULL) {
        *function_name = ZSTR_VAL(func->common.function_name);
        /* Try to read class name if it's a method */
        if (func->common.scope != NULL && func->common.scope->name != NULL) {
            *class_name = ZSTR_VAL(func->common.scope->name);
        }
    }

    /* Check if it's a user function */
    if (ZEND_USER_CODE(func->common.type)) {
        /* Try to read opline */
        const zend_op *opline = execute_data->opline;
        if (opline != NULL) {
            /* Read filename and line number from user function */
            if (func->op_array.filename != NULL) {
                *filename = ZSTR_VAL(func->op_array.filename);
            }
            *lineno = opline->lineno;
            return;
        }
    }

    /* Current function is internal or has no location info,
     * try to find caller's location from previous frame */
    zend_execute_data *prev = execute_data->prev_execute_data;
    while (prev != NULL) {
        zend_function *prev_func = prev->func;
        if (prev_func != NULL && ZEND_USER_CODE(prev_func->common.type)) {
            const zend_op *prev_opline = prev->opline;
            if (prev_opline != NULL && prev_func->op_array.filename != NULL) {
                *filename = ZSTR_VAL(prev_func->op_array.filename);
                *lineno = prev_opline->lineno;
                return;
            }
        }
        prev = prev->prev_execute_data;
    }
}

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

            /* Try to read current execution location (may be inaccurate, but provides debugging clues) */
            const char *filename = NULL;
            uint32_t lineno = 0;
            const char *function_name = NULL;
            const char *class_name = NULL;
            cat_coroutine_t *current_coroutine = watchdog->globals->current;

            swow_watchdog_try_read_execute_location(
                s_watchdog->executor_globals,
                &filename,
                &lineno,
                &function_name,
                &class_name
            );

            /* Output standard warning first */
            cat_watchdog_alert_standard(watchdog);

            /* Output detailed location info for debugging */
            if (current_coroutine != NULL) {
                fprintf(stderr,
                    "         Coroutine: #" CAT_COROUTINE_ID_FMT " (total: " CAT_COROUTINE_COUNT_FMT ")\n",
                    current_coroutine->id, watchdog->globals->count);
            }

            if (function_name != NULL) {
                if (class_name != NULL) {
                    fprintf(stderr, "         Function: %s::%s()\n", class_name, function_name);
                } else {
                    fprintf(stderr, "         Function: %s()\n", function_name);
                }
            }

            if (filename != NULL) {
                fprintf(stderr, "         Location: %s:%u\n", filename, lineno);
                fprintf(stderr, "         Note: Location info may be inaccurate due to cross-thread reading\n");
            } else if (function_name == NULL) {
                fprintf(stderr, "         Unable to read execute location (coroutine may be idle or in C extension)\n");
            }

            /* Set flag to trigger user callback when syscall returns and VM resumes */
            cat_atomic_bool_store(&s_watchdog->syscall_blocked, cat_true);
        }
    }
}

/* Check if execution context is safe for functions that access call stack (like debug_backtrace)
 * Returns true if safe, false if execution context is incomplete (e.g., just returned from FFI call) */
static cat_bool_t swow_watchdog_is_backtrace_safe(void)
{
    zend_execute_data *call = EG(current_execute_data);

    if (!call) {
        return cat_false;
    }

    /* Check a few frames to ensure func pointers are valid
     * This prevents assertion failure in zend_fetch_debug_backtrace when
     * returning from FFI calls where execute_data->func may be NULL */
    int check_depth = 0;
    while (call && check_depth < 3) {
        if (!call->func) {
            /* Check if this is a generator placeholder frame */
            zend_execute_data *checked = zend_generator_check_placeholder_frame(call);
            if (!checked->func) {
                /* Not a generator frame and func is still NULL - unsafe for backtrace */
                return cat_false;
            }
        }

        call = call->prev_execute_data;
        check_depth++;
    }

    return cat_true;
}

/* Call user alerter with blocking type parameter */
static void swow_watchdog_call_alerter(swow_watchdog_t *s_watchdog, const char *blocking_type, cat_bool_t is_delayed)
{
    if (s_watchdog->alerter.function_handler == NULL) {
        return;
    }

    /* Print hint for delayed syscall alerter callback */
    if (is_delayed) {
        fprintf(stderr, "Notice: <Watchdog> Alerter callback invoked at nearest safe execution point (not at exact blocking location)\n");
    }

    zend_fcall_info fci;
    zval retval, z_type;

    fci.size = sizeof(fci);
    ZVAL_UNDEF(&fci.function_name);
    fci.object = NULL;
    fci.param_count = 1;
    fci.params = &z_type;
    fci.named_params = NULL;
    fci.retval = &retval;

    ZVAL_STRING(&z_type, blocking_type);

    /* Save and clear vm_interrupt to prevent being triggered again in PHP alerter function */
    bool original_vm_interrupt = zend_atomic_bool_exchange(s_watchdog->vm_interrupt_ptr, 0);

    (void) zend_call_function(&fci, &s_watchdog->alerter);

    /* Restore original vm_interrupt value in case it was set by other reasons */
    zend_atomic_bool_store(s_watchdog->vm_interrupt_ptr, original_vm_interrupt);

    zval_ptr_dtor(&retval);
    zval_ptr_dtor(&z_type);
}

static void swow_watchdog_interrupt_function(zend_execute_data *execute_data)
{
    if (cat_watchdog_is_running()) {
        swow_watchdog_t *s_watchdog = swow_watchdog_get_current();
        cat_watchdog_t *watchdog = &s_watchdog->watchdog;

        /* Check if syscall blocking was detected and VM just resumed */
        if (cat_atomic_bool_load(&s_watchdog->syscall_blocked)) {
            /* Syscall blocking detected, check if execution context is safe for alerter callback */
            if (swow_watchdog_is_backtrace_safe()) {
                /* Execution context is now safe, clear flag and call user alerter */
                cat_atomic_bool_store(&s_watchdog->syscall_blocked, cat_false);
                swow_watchdog_call_alerter(s_watchdog, SWOW_WATCHDOG_BLOCKING_TYPE_SYSCALL, cat_true);
                goto _end;
            } else {
                /* Execution context still incomplete (e.g., just returned from FFI),
                 * keep flag set and trigger another interrupt to retry later */
                zend_atomic_bool_store(s_watchdog->vm_interrupt_ptr, 1);
                goto _end;
            }
        }

        cat_atomic_bool_store(&s_watchdog->vm_interrupted, cat_true);
        /* re-check if current switches still equal to last_switches  */
        if (CAT_COROUTINE_G(switches) == watchdog->last_switches) {
            /* CPU blocking detected */
            if (s_watchdog->alerter.function_handler == NULL) {
                /* No user alerter, use default delay scheduling */
                if (
                    !cat_time_wait(s_watchdog->delay) &&
                    cat_get_last_error_code() != CAT_ETIMEDOUT
                ) {
                    CAT_CORE_ERROR_WITH_LAST(WATCH_DOG, "Watchdog interrupt schedule failed");
                }
            } else {
                /* Call user alerter with CPU blocking type */
                swow_watchdog_call_alerter(s_watchdog, SWOW_WATCHDOG_BLOCKING_TYPE_CPU, cat_false);
            }
        }
    }

_end:
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
    s_watchdog->executor_globals = ZEND_GLOBALS_FAST_PTR(executor_globals);
    cat_atomic_bool_init(&s_watchdog->syscall_blocked, cat_false);
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
