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

#include "swow_sync.h"

#include "cat_time.h"

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Sync_waitAll, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_FUNCTION(Swow_Sync_waitAll)
{
    zend_long timeout = -1;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    ret = cat_coroutine_wait_all_ex(timeout);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }
}

static const zend_function_entry swow_sync_functions[] = {
    PHP_FENTRY(Swow\\Sync\\waitAll, PHP_FN(Swow_Sync_waitAll), arginfo_Swow_Sync_waitAll, 0)
    PHP_FE_END
};

SWOW_API zend_class_entry *swow_sync_wait_reference_ce;
SWOW_API zend_object_handlers swow_sync_wait_reference_handlers;

SWOW_API zend_class_entry *swow_sync_wait_group_ce;
SWOW_API zend_object_handlers swow_sync_wait_group_handlers;

SWOW_API zend_class_entry *swow_sync_exception_ce;

static zend_object *swow_sync_wait_reference_create_object(zend_class_entry *ce)
{
    swow_sync_wait_reference_t *swr = swow_object_alloc(swow_sync_wait_reference_t, ce, swow_sync_wait_reference_handlers);

    swr->scoroutine = NULL;
    swr->done = cat_false;

    return &swr->std;
}

static zend_object *swow_sync_wait_group_create_object(zend_class_entry *ce)
{
    swow_sync_wait_group_t *swg = swow_object_alloc(swow_sync_wait_group_t, ce, swow_sync_wait_group_handlers);

    (void) cat_sync_wait_group_create(&swg->wg);

    return &swg->std;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Sync_WaitReference_wait, 0, 1, IS_VOID, 0)
    ZEND_ARG_OBJ_INFO(1, waitReference, Swow\\Sync\\WaitReference, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Sync_WaitReference, wait)
{
    swow_sync_wait_reference_t *swr;
    zval *zwf;
    zend_object *wr;
    zend_long timeout = -1;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_OBJECT_OF_CLASS_EX(zwf, swow_sync_wait_reference_ce, 0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    wr = Z_OBJ_P(zwf);
    swr = swow_sync_wait_reference_get_from_object(wr);
    if (UNEXPECTED(swr->scoroutine != NULL)) {
        zend_throw_error(NULL, "WaitReference can not be reused before previous wait has returned");
        RETURN_THROWS();
    }

    /* check the refcount (if we should do wait) */
    ret = GC_REFCOUNT(wr) != 1;

    /* eq to unset() */
    ZVAL_NULL(zwf);
    zend_object_release(wr);

    /* if object has been released, just return */
    if (UNEXPECTED(!ret)) {
        return;
    }

    swr->scoroutine = swow_coroutine_get_current();
    ret = cat_time_wait(timeout);
    swr->scoroutine = NULL;

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last_as_reason(swow_sync_exception_ce, "WaitReference waiting for completion failed");
        RETURN_THROWS();
    }
    if (UNEXPECTED(!swr->done)) {
        swow_throw_exception(swow_sync_exception_ce, CAT_ECANCELED, "WaitReference waiting has been canceled");
        RETURN_THROWS();
    }
}

#define getThisWR() (swow_sync_wait_reference_get_from_object(Z_OBJ_P(ZEND_THIS)))

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Sync_WaitReference___destruct, 0, 0, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Sync_WaitReference, __destruct)
{
    swow_sync_wait_reference_t *swr = getThisWR();

    if (swr->scoroutine != NULL) {
        swr->done = cat_true;
        if (UNEXPECTED(!swow_coroutine_resume(swr->scoroutine, NULL, NULL))) {
            CAT_CORE_ERROR(SYNC, "Resume waiting coroutine failed, reason: %s", cat_get_last_error_message());
        }
    }
}

static const zend_function_entry swow_sync_wait_reference_methods[] = {
    PHP_ME(Swow_Sync_WaitReference, wait,       arginfo_class_Swow_Sync_WaitReference_wait,       ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Sync_WaitReference, __destruct, arginfo_class_Swow_Sync_WaitReference___destruct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

#define getThisWG() (swow_sync_wait_group_get_from_object(Z_OBJ_P(ZEND_THIS)))

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Sync_WaitGroup_add, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, delta, IS_LONG, 0, "1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Sync_WaitGroup, add)
{
    swow_sync_wait_group_t *swg = getThisWG();
    zend_long delta = 1;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(delta)
    ZEND_PARSE_PARAMETERS_END();

    ret = cat_sync_wait_group_add(&swg->wg, delta);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }
}

#define arginfo_class_Swow_Sync_WaitGroup_wait arginfo_Swow_Sync_waitAll

static PHP_METHOD(Swow_Sync_WaitGroup, wait)
{
    swow_sync_wait_group_t *swg = getThisWG();
    zend_long timeout = -1;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    ret = cat_sync_wait_group_wait(&swg->wg, timeout);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Sync_WaitGroup_done, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Sync_WaitGroup, done)
{
    swow_sync_wait_group_t *swg = getThisWG();
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = cat_sync_wait_group_done(&swg->wg);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }
}

static const zend_function_entry swow_sync_wait_group_methods[] = {
    PHP_ME(Swow_Sync_WaitGroup, add,  arginfo_class_Swow_Sync_WaitGroup_add,  ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Sync_WaitGroup, wait, arginfo_class_Swow_Sync_WaitGroup_wait, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Sync_WaitGroup, done, arginfo_class_Swow_Sync_WaitGroup_done, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_sync_module_init(INIT_FUNC_ARGS)
{
    if (zend_register_functions(NULL, swow_sync_functions, NULL, type) != SUCCESS) {
        return FAILURE;
    }

    swow_sync_wait_reference_ce = swow_register_internal_class(
        "Swow\\Sync\\WaitReference", NULL, swow_sync_wait_reference_methods,
        &swow_sync_wait_reference_handlers, NULL,
        cat_false, cat_false,
        swow_sync_wait_reference_create_object, NULL,
        XtOffsetOf(swow_sync_wait_reference_t, std)
    );
    swow_sync_wait_group_ce = swow_register_internal_class(
        "Swow\\Sync\\WaitGroup", NULL, swow_sync_wait_group_methods,
        &swow_sync_wait_group_handlers, NULL,
        cat_false, cat_false,
        swow_sync_wait_group_create_object, NULL,
        XtOffsetOf(swow_sync_wait_group_t, std)
    );

    swow_sync_exception_ce = swow_register_internal_class(
        "Swow\\SyncException", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}
