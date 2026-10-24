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

#include "swow_mutex.h"

#include "cat_thread.h"

#ifdef ZTS

SWOW_API zend_class_entry *swow_mutex_ce;
SWOW_API zend_object_handlers swow_mutex_handlers;

/* the tag of a shared mutex, so a serialized handle of another class is
 * rejected */
static const char swow_mutex_type[] = "Swow\\Mutex";

static cat_always_inline swow_mutex_object_t *swow_mutex_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_mutex_object_t, std);
}

static cat_bool_t swow_mutex_free(swow_shared_object_t *object)
{
    swow_mutex_t *mutex = (swow_mutex_t *) object;

    if (!cat_mutex_destroy(&mutex->mutex)) {
        php_error_docref(NULL, E_WARNING, "Mutex is still locked and was not destroyed");
        return cat_false;
    }

    return cat_true;
}

static zend_object *swow_mutex_create_object(zend_class_entry *ce)
{
    swow_mutex_object_t *s_mutex = swow_object_alloc(swow_mutex_object_t, ce, swow_mutex_handlers);

    s_mutex->mutex = NULL;

    return &s_mutex->std;
}

static void swow_mutex_free_object(zend_object *object)
{
    swow_mutex_object_t *s_mutex = swow_mutex_get_from_object(object);

    if (s_mutex->mutex != NULL) {
        swow_shared_object_release(&s_mutex->mutex->object);
        s_mutex->mutex = NULL;
    }

    zend_object_std_dtor(&s_mutex->std);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Mutex___construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Mutex_lock, 0, 0, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Mutex_unlock, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Mutex___serialize, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Mutex___unserialize, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Mutex___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Mutex, __construct)
{
    swow_mutex_object_t *s_mutex = swow_mutex_get_from_object(Z_OBJ_P(ZEND_THIS));
    swow_mutex_t *mutex;

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_mutex->mutex != NULL)) {
        zend_throw_exception(swow_sync_exception_ce, "Mutex has already been constructed", 0);
        RETURN_THROWS();
    }

    mutex = (swow_mutex_t *) swow_shared_object_create(sizeof(*mutex), swow_mutex_type, NULL);
    if (unlikely(mutex == NULL)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    if (unlikely(!cat_mutex_init(&mutex->mutex))) {
        swow_shared_object_release(&mutex->object);
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    cat_atomic_uint64_init(&mutex->owner, 0);

    mutex->object.free = swow_mutex_free;
    s_mutex->mutex = mutex;
}

static PHP_METHOD(Swow_Mutex, lock)
{
    swow_mutex_object_t *s_mutex = swow_mutex_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long timeout = -1;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(s_mutex->mutex == NULL)) {
        zend_throw_exception(swow_sync_exception_ce, "Mutex has not been constructed", 0);
        RETURN_THROWS();
    }

    if (!cat_mutex_lock(&s_mutex->mutex->mutex, timeout)) {
        cat_errno_t code = cat_get_last_error_code();

        if (code == CAT_ETIMEDOUT || code == CAT_EBUSY) {
            RETURN_FALSE;
        }

        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    cat_atomic_uint64_store(&s_mutex->mutex->owner,
                            (uint64_t) cat_thread_get_current_id());

    RETURN_TRUE;
}

static PHP_METHOD(Swow_Mutex, unlock)
{
    swow_mutex_object_t *s_mutex = swow_mutex_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_mutex->mutex == NULL)) {
        zend_throw_exception(swow_sync_exception_ce, "Mutex has not been constructed", 0);
        RETURN_THROWS();
    }

    if (cat_atomic_uint64_load(&s_mutex->mutex->owner) !=
        (uint64_t) cat_thread_get_current_id()) {
        zend_throw_exception(swow_sync_exception_ce, "The caller does not hold the mutex", 0);
        RETURN_THROWS();
    }

    /* Drop the owner before the release. The release wakes a waiter that takes
     * the mutex and stores its own owner immediately, so clearing it afterwards
     * would erase the new holder */
    {
        uint64_t current = (uint64_t) cat_thread_get_current_id();
        uint64_t owner = current;

        if (unlikely(!cat_atomic_uint64_compare_exchange_strong(
                &s_mutex->mutex->owner, &owner, 0))) {
            zend_throw_exception(swow_sync_exception_ce,
                                 "The caller does not hold the mutex", 0);
            RETURN_THROWS();
        }

        if (unlikely(!cat_mutex_unlock(&s_mutex->mutex->mutex))) {
            cat_atomic_uint64_store(&s_mutex->mutex->owner, current);
            swow_throw_exception_with_last(swow_sync_exception_ce);
            RETURN_THROWS();
        }
    }
}

static PHP_METHOD(Swow_Mutex, __serialize)
{
    swow_mutex_object_t *s_mutex = swow_mutex_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_mutex->mutex == NULL)) {
        zend_throw_exception(swow_sync_exception_ce, "Mutex has not been constructed", 0);
        RETURN_THROWS();
    }

    swow_shared_object_serialize(return_value, &s_mutex->mutex->object);
}

static PHP_METHOD(Swow_Mutex, __unserialize)
{
    swow_mutex_object_t *s_mutex = swow_mutex_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *z_data;
    swow_shared_object_t *object;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY(z_data)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(!swow_shared_object_unserialize(Z_ARRVAL_P(z_data), swow_mutex_type,
                                                 &object))) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    if (s_mutex->mutex != NULL) {
        swow_shared_object_release(&s_mutex->mutex->object);
    }
    s_mutex->mutex = (swow_mutex_t *) object;
}

static PHP_METHOD(Swow_Mutex, __debugInfo)
{
    swow_mutex_object_t *s_mutex = swow_mutex_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);
    if (s_mutex->mutex != NULL) {
        add_assoc_bool(return_value, "locked", cat_atomic_bool_load(&s_mutex->mutex->mutex.held));
    }
}

static const zend_function_entry swow_mutex_methods[] = {
    PHP_ME(Swow_Mutex, __construct,   arginfo_class_Swow_Mutex___construct,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Mutex, lock,          arginfo_class_Swow_Mutex_lock,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Mutex, unlock,        arginfo_class_Swow_Mutex_unlock,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Mutex, __serialize,   arginfo_class_Swow_Mutex___serialize,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Mutex, __unserialize, arginfo_class_Swow_Mutex___unserialize, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Mutex, __debugInfo,   arginfo_class_Swow_Mutex___debugInfo,   ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_mutex_module_init(INIT_FUNC_ARGS)
{
    swow_mutex_ce = swow_register_internal_class(
        "Swow\\Mutex", NULL, swow_mutex_methods,
        &swow_mutex_handlers, NULL,
        cat_false, cat_true,
        swow_mutex_create_object, swow_mutex_free_object,
        offsetof(swow_mutex_object_t, std)
    );

    return SUCCESS;
}

zend_result swow_mutex_module_shutdown(SHUTDOWN_FUNC_ARGS)
{
    return SUCCESS;
}

#endif /* ZTS */
