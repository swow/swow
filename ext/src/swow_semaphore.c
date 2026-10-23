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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#include "swow_semaphore.h"

#include <limits.h>

#ifdef ZTS

SWOW_API zend_class_entry *swow_semaphore_ce;
SWOW_API zend_object_handlers swow_semaphore_handlers;

static cat_always_inline swow_semaphore_object_t *swow_semaphore_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_semaphore_object_t, std);
}

static cat_bool_t swow_semaphore_free(swow_shared_object_t *object)
{
    swow_semaphore_t *semaphore = (swow_semaphore_t *) object;

    if (cat_thread_wait_is_parked(&semaphore->sem.wait)) {
        php_error_docref(NULL, E_WARNING, "Semaphore is waited on and was not destroyed");
        return cat_false;
    }

    cat_sem_destroy(&semaphore->sem);

    return cat_true;
}

static zend_object *swow_semaphore_create_object(zend_class_entry *ce)
{
    swow_semaphore_object_t *s_semaphore =
        swow_object_alloc(swow_semaphore_object_t, ce, swow_semaphore_handlers);

    s_semaphore->semaphore = NULL;

    return &s_semaphore->std;
}

static void swow_semaphore_free_object(zend_object *object)
{
    swow_semaphore_object_t *s_semaphore = swow_semaphore_get_from_object(object);

    if (s_semaphore->semaphore != NULL) {
        swow_shared_object_release(&s_semaphore->semaphore->object);
        s_semaphore->semaphore = NULL;
    }

    zend_object_std_dtor(&s_semaphore->std);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Semaphore___construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, value, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Semaphore_acquire, 0, 0, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Semaphore_release, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, count, IS_LONG, 0, "1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Semaphore_tryAcquire, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Semaphore_getValue, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Semaphore_getWaiterCount, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Semaphore___serialize, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Semaphore___unserialize, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Semaphore, __construct)
{
    swow_semaphore_object_t *s_semaphore = swow_semaphore_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long value = 0;
    swow_semaphore_t *semaphore;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(value)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(value < 0 || (zend_ulong) value > UINT_MAX)) {
        zend_value_error("The initial value must be between 0 and %u", UINT_MAX);
        RETURN_THROWS();
    }

    if (unlikely(s_semaphore->semaphore != NULL)) {
        zend_throw_exception(swow_sync_exception_ce, "Semaphore has already been constructed", 0);
        RETURN_THROWS();
    }

    semaphore = (swow_semaphore_t *) swow_shared_object_create(sizeof(*semaphore), NULL);
    if (unlikely(semaphore == NULL)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    if (unlikely(!cat_sem_init(&semaphore->sem, (unsigned int) value))) {
        swow_shared_object_release(&semaphore->object);
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    semaphore->object.free = swow_semaphore_free;
    s_semaphore->semaphore = semaphore;
}

static PHP_METHOD(Swow_Semaphore, acquire)
{
    swow_semaphore_object_t *s_semaphore = swow_semaphore_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long timeout = -1;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(s_semaphore->semaphore == NULL)) {
        zend_throw_exception(swow_sync_exception_ce, "Semaphore has not been constructed", 0);
        RETURN_THROWS();
    }

    if (!cat_sem_acquire(&s_semaphore->semaphore->sem, timeout)) {
        cat_errno_t code = cat_get_last_error_code();

        if (code == CAT_ETIMEDOUT || code == CAT_EBUSY) {
            RETURN_FALSE;
        }

        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    RETURN_TRUE;
}

static PHP_METHOD(Swow_Semaphore, tryAcquire)
{
    swow_semaphore_object_t *s_semaphore = swow_semaphore_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_semaphore->semaphore == NULL)) {
        zend_throw_exception(swow_sync_exception_ce, "Semaphore has not been constructed", 0);
        RETURN_THROWS();
    }

    if (!cat_sem_try_acquire(&s_semaphore->semaphore->sem)) {
        cat_errno_t code = cat_get_last_error_code();

        if (code == CAT_EBUSY) {
            RETURN_FALSE;
        }

        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    RETURN_TRUE;
}

static PHP_METHOD(Swow_Semaphore, getValue)
{
    swow_semaphore_object_t *s_semaphore = swow_semaphore_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_semaphore->semaphore == NULL)) {
        RETURN_LONG(0);
    }

    RETURN_LONG((zend_long) cat_sem_get_value(&s_semaphore->semaphore->sem));
}

static PHP_METHOD(Swow_Semaphore, getWaiterCount)
{
    swow_semaphore_object_t *s_semaphore = swow_semaphore_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_semaphore->semaphore == NULL)) {
        RETURN_LONG(0);
    }

    RETURN_LONG((zend_long) cat_sem_get_waiter_count(&s_semaphore->semaphore->sem));
}

static PHP_METHOD(Swow_Semaphore, release)
{
    swow_semaphore_object_t *s_semaphore = swow_semaphore_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long count = 1;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(count)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(count < 0)) {
        zend_value_error("The release count must be greater than or equal to 0");
        RETURN_THROWS();
    }

    if (unlikely(s_semaphore->semaphore == NULL)) {
        zend_throw_exception(swow_sync_exception_ce, "Semaphore has not been constructed", 0);
        RETURN_THROWS();
    }

    cat_sem_release(&s_semaphore->semaphore->sem, (size_t) count);
}

static PHP_METHOD(Swow_Semaphore, __serialize)
{
    swow_semaphore_object_t *s_semaphore = swow_semaphore_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_semaphore->semaphore == NULL)) {
        zend_throw_exception(swow_sync_exception_ce, "Semaphore has not been constructed", 0);
        RETURN_THROWS();
    }

    swow_shared_object_serialize(return_value, &s_semaphore->semaphore->object);
}

static PHP_METHOD(Swow_Semaphore, __unserialize)
{
    swow_semaphore_object_t *s_semaphore = swow_semaphore_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *z_data;
    swow_shared_object_t *object;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY(z_data)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(!swow_shared_object_unserialize(Z_ARRVAL_P(z_data), &object))) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    if (s_semaphore->semaphore != NULL) {
        swow_shared_object_release(&s_semaphore->semaphore->object);
    }
    s_semaphore->semaphore = (swow_semaphore_t *) object;
}

static const zend_function_entry swow_semaphore_methods[] = {
    PHP_ME(Swow_Semaphore, __construct,   arginfo_class_Swow_Semaphore___construct,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Semaphore, acquire,       arginfo_class_Swow_Semaphore_acquire,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Semaphore, tryAcquire,    arginfo_class_Swow_Semaphore_tryAcquire,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Semaphore, getValue,      arginfo_class_Swow_Semaphore_getValue,     ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Semaphore, getWaiterCount, arginfo_class_Swow_Semaphore_getWaiterCount, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Semaphore, release,       arginfo_class_Swow_Semaphore_release,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Semaphore, __serialize,   arginfo_class_Swow_Semaphore___serialize,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Semaphore, __unserialize, arginfo_class_Swow_Semaphore___unserialize, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_semaphore_module_init(INIT_FUNC_ARGS)
{
    swow_semaphore_ce = swow_register_internal_class(
        "Swow\\Semaphore", NULL, swow_semaphore_methods,
        &swow_semaphore_handlers, NULL,
        cat_false, cat_true,
        swow_semaphore_create_object, swow_semaphore_free_object,
        offsetof(swow_semaphore_object_t, std)
    );

    return SUCCESS;
}

zend_result swow_semaphore_module_shutdown(SHUTDOWN_FUNC_ARGS)
{
    return SUCCESS;
}

#endif /* ZTS */
