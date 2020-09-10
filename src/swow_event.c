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

#include "swow_event.h"

#include "swow_defer.h"
#include "swow_coroutine.h"

SWOW_API zend_class_entry *swow_event_ce;
SWOW_API zend_object_handlers swow_event_handlers;

SWOW_API zend_class_entry *swow_event_scheduler_ce;
SWOW_API zend_object_handlers swow_event_scheduler_handlers;

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_event_wait, ZEND_RETURN_VALUE, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_event, wait)
{
    if (!swow_function_is_internal_accessor(INTERNAL_FUNCTION_PARAM_PASSTHRU)) {
        /* called by user */
        cat_event_wait();
        return;
    }

    // Don't disable object slot reuse while running shutdown functions:
    // https://github.com/php/php-src/commit/bd6eabd6591ae5a7c9ad75dfbe7cc575fa907eac
#if defined(EG_FLAGS_IN_SHUTDOWN) && !defined(EG_FLAGS_OBJECT_STORE_NO_REUSE)
    zend_bool in_shutdown = EG(flags) & EG_FLAGS_IN_SHUTDOWN;
    EG(flags) &= ~EG_FLAGS_IN_SHUTDOWN;
#endif

    /* user space (we simulate main exit here) */
    swow_defer_do_main_tasks();
#if SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS
    swow_output_globals_fast_end();
#endif

    /* internal space */
    cat_event_wait();

#if defined(EG_FLAGS_IN_SHUTDOWN) && !defined(EG_FLAGS_OBJECT_STORE_NO_REUSE)
    if (in_shutdown) {
        EG(flags) |= EG_FLAGS_IN_SHUTDOWN;
    }
#endif
}

static const zend_function_entry swow_event_methods[] = {
    PHP_ME(swow_event, wait, arginfo_swow_event_wait, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_event_scheduler_run, ZEND_RETURN_VALUE, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_event_scheduler, run)
{
    cat_data_t *data;

    swow_coroutine_set_executor_switcher(cat_false);
    data = cat_event_scheduler_function(CAT_COROUTINE_DATA_NULL);
    swow_coroutine_set_executor_switcher(cat_true);

    if (data != CAT_COROUTINE_DATA_NULL) {
        zend_throw_error(NULL, "%s", cat_get_last_error_message());
        RETURN_THROWS();
    }
}

static const zend_function_entry swow_event_scheduler_methods[] = {
    PHP_ME(swow_event_scheduler, run,  arginfo_swow_event_scheduler_run,  ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

SWOW_API CAT_GLOBALS_DECLARE(swow_event)

CAT_GLOBALS_CTOR_DECLARE_SZ(swow_event)

int swow_event_module_init(INIT_FUNC_ARGS)
{
    if (!cat_event_module_init()) {
        return FAILURE;
    }

    CAT_GLOBALS_REGISTER(swow_event, CAT_GLOBALS_CTOR(swow_event), NULL);

    swow_event_ce = swow_register_internal_class(
        "Swow\\Event", NULL, swow_event_methods,
        &swow_event_scheduler_handlers, NULL,
        cat_false, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );

    swow_event_scheduler_ce = swow_register_internal_class(
        "Swow\\Event\\Scheduler", NULL, swow_event_scheduler_methods,
        &swow_event_scheduler_handlers, NULL,
        cat_false, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );

    return SUCCESS;
}

int swow_event_runtime_init(INIT_FUNC_ARGS)
{
    if (!cat_event_runtime_init()) {
        return FAILURE;
    }

    /* create and run event sheduler */
    do {
        swow_coroutine_t *scheduler;
        do {
            cat_coroutine_id_t last_id = CAT_COROUTINE_G(last_id);
            uint32_t top = EG(objects_store).top;
            zval zcallable;

            ZVAL_STRING(&zcallable, "Swow\\Event\\Scheduler::run");
            CAT_COROUTINE_G(last_id) = 0;
            EG(objects_store).top = 0;

            scheduler = swow_coroutine_create(&zcallable);

            EG(objects_store).top = top;
            CAT_COROUTINE_G(last_id) = last_id;
            zend_string_release(Z_STR_P(&zcallable));
        } while (0);
        if (scheduler == NULL) {
            cat_core_error(EVENT, "Event sheduler failed to create, reason: %s", cat_get_last_error_message());
        }
        /* run event scheduler in the new coroutine */
        if (!swow_coroutine_scheduler_run(scheduler)) {
            cat_core_error_with_last(EVENT, "Event scheduler failed to run");
        }
        /* check (never too much) */
        if (!cat_event_is_running()) {
            cat_core_error(EVENT, "Event scheduler failed to run by unknwon reason");
        }
    } while (0);

    do {
        php_shutdown_function_entry shutdown_function_entry;
        shutdown_function_entry.arg_count = 2;
        shutdown_function_entry.arguments = (zval *) safe_emalloc(sizeof(zval), 2, 0);
        ZVAL_STRING(&shutdown_function_entry.arguments[0], "Swow\\Event::wait");
        ZVAL_PTR(&shutdown_function_entry.arguments[1], &swow_internal_callable_key);
        register_user_shutdown_function((char *) ZEND_STRL("Swow\\Event::wait"), &shutdown_function_entry);
    } while (0);

    return SUCCESS;
}

int swow_event_runtime_shutdown(SHUTDOWN_FUNC_ARGS)
{
    /* after register_shutdown and call all destructors */
    cat_event_wait();

    /* stop event scheduler */
    CAT_ASSERT(swow_coroutine_get_scheduler() != NULL);
    do {
        swow_coroutine_t *scheduler = swow_coroutine_scheduler_stop();
        if (scheduler == NULL) {
            cat_core_error_with_last(EVENT, "Stop event scheduler failed");
        } else {
            zend_object_release(&scheduler->std);
        }
    } while (0);

    if (cat_event_is_running()) {
        cat_core_error(EVENT, "Unexpected status of event scheduler (still running)");
    }

    /* Notice: some PHP objects still has not been released
     * (e.g. static vars or set global vars on __destruct)
     * so we must close the loop on sapi_runtime_shutdown */

    return SUCCESS;
}

int swow_event_delay_runtime_shutdown(void)
{
    cat_event_runtime_shutdown();

    return SUCCESS;
}
