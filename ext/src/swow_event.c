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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Event_wait, ZEND_RETURN_VALUE, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Event, wait)
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

    /* TODO: remove from coroutine map */
    /* internal space */
    cat_event_wait();

#if defined(EG_FLAGS_IN_SHUTDOWN) && !defined(EG_FLAGS_OBJECT_STORE_NO_REUSE)
    if (in_shutdown) {
        EG(flags) |= EG_FLAGS_IN_SHUTDOWN;
    }
#endif
}

static const zend_function_entry swow_event_methods[] = {
    PHP_ME(Swow_Event, wait, arginfo_class_Swow_Event_wait, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

static cat_bool_t swow_event_scheduler_run(void)
{
    swow_coroutine_t *scoroutine;
    cat_coroutine_t *coroutine;

    /* Notice: if object id is 0, its destructor will never be called */
    do {
        uint32_t top = EG(objects_store).top;
        EG(objects_store).top = 0;
        scoroutine = swow_coroutine_get_from_object(
            swow_object_create(swow_coroutine_ce)
        );
        EG(objects_store).top = top;
        EG(objects_store).object_buckets[0] = NULL;
        scoroutine->std.handle = UINT32_MAX;
    } while (0);

    coroutine = cat_event_scheduler_run(&scoroutine->coroutine);

    return coroutine != NULL;
}

static cat_bool_t swow_event_scheduler_close(void)
{
    swow_coroutine_t *scoroutine;
    cat_coroutine_t *coroutine;

    coroutine = cat_event_scheduler_close();

    if (coroutine == NULL) {
        cat_warn_with_last(EVENT, "Event sheduler close failed");
        return cat_false;
    }

    do {
        scoroutine = swow_coroutine_get_from_handle(coroutine);
        scoroutine->std.handle = 0;
        swow_coroutine_close(scoroutine);
        EG(objects_store).object_buckets[0] = NULL;
    } while (0);

    return cat_true;
}

static zif_handler original_pcntl_fork = (zif_handler) -1;

static PHP_FUNCTION(swow_pcntl_fork)
{
    original_pcntl_fork(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    if (Z_TYPE_P(return_value) == IS_LONG || Z_LVAL_P(return_value) == 0) {
        /* Fork event loop in child process
         * TODO: kill all coroutines?  */
        cat_event_fork();
    }
}

int swow_event_module_init(INIT_FUNC_ARGS)
{
    if (!cat_event_module_init()) {
        return FAILURE;
    }

    swow_event_ce = swow_register_internal_class(
        "Swow\\Event", NULL, swow_event_methods,
        &swow_event_handlers, NULL,
        cat_false, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );

    return SUCCESS;
}

int swow_event_module_shutdown(INIT_FUNC_ARGS)
{
    if (!cat_event_module_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}

int swow_event_runtime_init(INIT_FUNC_ARGS)
{
    if (!cat_event_runtime_init()) {
        return FAILURE;
    }

    if (!swow_event_scheduler_run()) {
        return FAILURE;
    }
    do {
        php_shutdown_function_entry shutdown_function_entry;
        zval *zfunction_name;
#if PHP_VERSION_ID >= 80100
        zval zcallable;
        zfunction_name = &zcallable;
#else
        int arg_count;
#if PHP_VERSION_ID >= 80000
        arg_count = 1;
#else
        arg_count = 2;
#endif
        shutdown_function_entry.arg_count = arg_count;
        shutdown_function_entry.arguments = (zval *) safe_emalloc(sizeof(zval), arg_count, 0);
#if PHP_VERSION_ID >= 80000
        zfunction_name = &shutdown_function_entry.function_name;
#else
        zfunction_name = &shutdown_function_entry.arguments[0];
#endif
#endif
        ZVAL_STRING(zfunction_name, "Swow\\Event::wait");
#if PHP_VERSION_ID >= 80100
        zval zkey;
        ZVAL_PTR(&zkey, &swow_internal_callable_key);
        zend_fcall_info_init(&zcallable, 0, &shutdown_function_entry.fci,
            &shutdown_function_entry.fci_cache, NULL, NULL);
        zend_fcall_info_argp(&shutdown_function_entry.fci, 1, &zkey);
#else
        ZVAL_PTR(&shutdown_function_entry.arguments[arg_count - 1], &swow_internal_callable_key);
#endif
        register_user_shutdown_function(Z_STRVAL_P(zfunction_name), Z_STRLEN_P(zfunction_name), &shutdown_function_entry);
    } while (0);

    if (original_pcntl_fork == (zif_handler) -1) {
        zend_function *pcntl_fork_function = (zend_function *) zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("pcntl_fork"));
        if (pcntl_fork_function != NULL && pcntl_fork_function->type == ZEND_INTERNAL_FUNCTION) {
            original_pcntl_fork = pcntl_fork_function->internal_function.handler;
            pcntl_fork_function->internal_function.handler = zif_swow_pcntl_fork;
        }
    }

    return SUCCESS;
}

int swow_event_runtime_shutdown(SHUTDOWN_FUNC_ARGS)
{
    /* after register_shutdown and call all destructors */

    /* stop event scheduler (event_wait() is included) */
    if (!swow_event_scheduler_close()) {
        return FAILURE;
    }

    if (!cat_event_runtime_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}
