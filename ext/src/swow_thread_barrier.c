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

#include "swow_thread_barrier.h"

#include "swow_sync.h"

#ifdef ZTS

#define SWOW_THREAD_BARRIER_MAGIC 0x54424152U /* "TBAR" */

SWOW_API zend_class_entry *swow_thread_barrier_ce;
SWOW_API zend_object_handlers swow_thread_barrier_handlers;

/* the tag of a shared barrier, so a serialized handle of another class is
 * rejected */
static const char swow_thread_barrier_type[] = "Swow\\Sync\\ThreadBarrier";

static swow_thread_barrier_object_t *swow_thread_barrier_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_thread_barrier_object_t, std);
}

static cat_bool_t swow_thread_barrier_free(swow_shared_object_t *object)
{
    swow_thread_barrier_t *barrier = (swow_thread_barrier_t *) object;

    if (cat_thread_wait_is_parked(&barrier->cond.wait)) {
        php_error_docref(NULL, E_WARNING, "ThreadBarrier is waited on and was not destroyed");
        return cat_false;
    }

    cat_cond_destroy(&barrier->cond);
    if (!cat_mutex_destroy(&barrier->mutex)) {
        php_error_docref(NULL, E_WARNING, "ThreadBarrier is still locked and was not destroyed");
        return cat_false;
    }

    return cat_true;
}

static zend_object *swow_thread_barrier_create_object(zend_class_entry *ce)
{
    swow_thread_barrier_object_t *barrier =
        swow_object_alloc(swow_thread_barrier_object_t, ce, swow_thread_barrier_handlers);

    barrier->barrier = NULL;

    return &barrier->std;
}

static void swow_thread_barrier_free_object(zend_object *object)
{
    swow_thread_barrier_object_t *barrier = swow_thread_barrier_get_from_object(object);

    if (barrier->barrier != NULL) {
        swow_shared_object_release(&barrier->barrier->object);
        barrier->barrier = NULL;
    }

    zend_object_std_dtor(&barrier->std);
}

/* Only CAT_TIMEOUT_FOREVER waits without a deadline. A negative timeout would
 * arm no timer, so it becomes an expired one */
static cat_always_inline cat_timeout_t swow_thread_barrier_normalize(cat_timeout_t timeout)
{
    return timeout < 0 && timeout != CAT_TIMEOUT_FOREVER ? 0 : timeout;
}

/* The deadline is fixed once for the whole wait, so a wakeup that finds no trip
 * does not restart the timeout */
static cat_always_inline cat_timeout_t swow_thread_barrier_deadline(cat_timeout_t timeout)
{
    timeout = swow_thread_barrier_normalize(timeout);

    return timeout == CAT_TIMEOUT_FOREVER ? CAT_TIMEOUT_FOREVER
                                          : (cat_timeout_t) cat_time_msec() + timeout;
}

static cat_timeout_t swow_thread_barrier_remaining(cat_timeout_t deadline)
{
    cat_msec_t now;

    if (deadline == CAT_TIMEOUT_FOREVER) {
        return CAT_TIMEOUT_FOREVER;
    }

    now = cat_time_msec();
    if (now >= (cat_msec_t) deadline) {
        return 0;
    }

    return (cat_timeout_t) ((cat_msec_t) deadline - now);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Sync_ThreadBarrier___construct, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, count, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Sync_ThreadBarrier_wait, 0, 0, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Sync_ThreadBarrier_getCount, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Sync_ThreadBarrier_getWaiting, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Sync_ThreadBarrier___serialize, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Sync_ThreadBarrier___unserialize, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Sync_ThreadBarrier___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Sync_ThreadBarrier, __construct)
{
    swow_thread_barrier_object_t *barrier = swow_thread_barrier_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long count;
    swow_thread_barrier_t *shared;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(count)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(count <= 0 || (zend_ulong) count > UINT32_MAX)) {
        zend_value_error("The barrier count must be between 1 and %u", UINT32_MAX);
        RETURN_THROWS();
    }
    if (unlikely(barrier->barrier != NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "ThreadBarrier has already been constructed", 0);
        RETURN_THROWS();
    }

    shared = (swow_thread_barrier_t *) swow_shared_object_create(
        sizeof(*shared), swow_thread_barrier_type, NULL);
    if (unlikely(shared == NULL)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    if (unlikely(!cat_cond_init(&shared->cond))) {
        swow_shared_object_release(&shared->object);
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }
    if (unlikely(!cat_mutex_init_ex(&shared->mutex, CAT_SYNC_WAIT_BLOCK))) {
        cat_cond_destroy(&shared->cond);
        swow_shared_object_release(&shared->object);
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    shared->object.free = swow_thread_barrier_free;
    shared->magic = SWOW_THREAD_BARRIER_MAGIC;
    shared->count = (uint32_t) count;
    shared->waiting = 0;
    shared->generation = 0;
    barrier->barrier = shared;
}

static PHP_METHOD(Swow_Sync_ThreadBarrier, wait)
{
    swow_thread_barrier_object_t *barrier = swow_thread_barrier_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long timeout = -1;
    swow_thread_barrier_t *shared;
    uint64_t generation;
    cat_timeout_t deadline;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(barrier->barrier == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "ThreadBarrier has not been constructed", 0);
        RETURN_THROWS();
    }

    shared = barrier->barrier;
    deadline = swow_thread_barrier_deadline(timeout);

    if (unlikely(!cat_mutex_lock(&shared->mutex, timeout))) {
        cat_errno_t code = cat_get_last_error_code();

        if (code == CAT_ETIMEDOUT || code == CAT_EBUSY) {
            RETURN_FALSE;
        }
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    shared->waiting++;
    if (shared->waiting == shared->count) {
        shared->waiting = 0;
        shared->generation++;
        (void) cat_cond_broadcast(&shared->cond);
        if (unlikely(!cat_mutex_unlock(&shared->mutex))) {
            swow_throw_exception_with_last(swow_sync_exception_ce);
            RETURN_THROWS();
        }
        RETURN_TRUE;
    }

    generation = shared->generation;
    while (shared->generation == generation) {
        if (!cat_cond_wait(&shared->cond, &shared->mutex,
                           swow_thread_barrier_remaining(deadline))) {
            cat_errno_t code = cat_get_last_error_code();

            /*
             * The trip of another participant may have cleared the count while
             * this wait was timing out. Only the round that this waiter joined
             * can be left, or the count wraps and the barrier never trips again
             */
            if (shared->generation == generation) {
                shared->waiting--;
            }

            if (unlikely(!cat_mutex_unlock(&shared->mutex))) {
                swow_throw_exception_with_last(swow_sync_exception_ce);
                RETURN_THROWS();
            }
            if (code == CAT_ETIMEDOUT || code == CAT_EBUSY) {
                RETURN_FALSE;
            }
            swow_throw_exception_with_last(swow_sync_exception_ce);
            RETURN_THROWS();
        }
    }

    if (unlikely(!cat_mutex_unlock(&shared->mutex))) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    RETURN_TRUE;
}

static PHP_METHOD(Swow_Sync_ThreadBarrier, getCount)
{
    swow_thread_barrier_object_t *barrier = swow_thread_barrier_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(barrier->barrier != NULL ? barrier->barrier->count : 0);
}

static PHP_METHOD(Swow_Sync_ThreadBarrier, getWaiting)
{
    swow_thread_barrier_object_t *barrier = swow_thread_barrier_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long waiting = 0;

    ZEND_PARSE_PARAMETERS_NONE();

    if (barrier->barrier == NULL) {
        RETURN_LONG(0);
    }

    if (cat_mutex_lock(&barrier->barrier->mutex, 0)) {
        waiting = barrier->barrier->waiting;
        (void) cat_mutex_unlock(&barrier->barrier->mutex);
    }

    RETURN_LONG(waiting);
}

static PHP_METHOD(Swow_Sync_ThreadBarrier, __serialize)
{
    swow_thread_barrier_object_t *barrier = swow_thread_barrier_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(barrier->barrier == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "ThreadBarrier has not been constructed", 0);
        RETURN_THROWS();
    }

    swow_shared_object_serialize(return_value, &barrier->barrier->object);
}

static PHP_METHOD(Swow_Sync_ThreadBarrier, __unserialize)
{
    swow_thread_barrier_object_t *barrier = swow_thread_barrier_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *data;
    swow_shared_object_t *object;
    swow_thread_barrier_t *shared;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY(data)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(!swow_shared_object_unserialize(Z_ARRVAL_P(data), swow_thread_barrier_type,
                                                 &object))) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    shared = (swow_thread_barrier_t *) object;
    if (unlikely(shared->magic != SWOW_THREAD_BARRIER_MAGIC)) {
        swow_shared_object_release(object);
        zend_throw_exception(swow_sync_exception_ce,
                             "Invalid serialized ThreadBarrier", 0);
        RETURN_THROWS();
    }

    if (barrier->barrier != NULL) {
        swow_shared_object_release(&barrier->barrier->object);
    }
    barrier->barrier = shared;
}

static PHP_METHOD(Swow_Sync_ThreadBarrier, __debugInfo)
{
    swow_thread_barrier_object_t *barrier = swow_thread_barrier_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);
    if (barrier->barrier != NULL) {
        add_assoc_long(return_value, "count", barrier->barrier->count);
        add_assoc_long(return_value, "waiting", barrier->barrier->waiting);
        add_assoc_long(return_value, "generation", (zend_long) barrier->barrier->generation);
    }
}

static const zend_function_entry swow_thread_barrier_methods[] = {
    PHP_ME(Swow_Sync_ThreadBarrier, __construct,   arginfo_class_Swow_Sync_ThreadBarrier___construct,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Sync_ThreadBarrier, wait,          arginfo_class_Swow_Sync_ThreadBarrier_wait,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Sync_ThreadBarrier, getCount,      arginfo_class_Swow_Sync_ThreadBarrier_getCount,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Sync_ThreadBarrier, getWaiting,    arginfo_class_Swow_Sync_ThreadBarrier_getWaiting,    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Sync_ThreadBarrier, __serialize,   arginfo_class_Swow_Sync_ThreadBarrier___serialize,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Sync_ThreadBarrier, __unserialize, arginfo_class_Swow_Sync_ThreadBarrier___unserialize, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Sync_ThreadBarrier, __debugInfo,   arginfo_class_Swow_Sync_ThreadBarrier___debugInfo,   ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_thread_barrier_module_init(INIT_FUNC_ARGS)
{
    swow_thread_barrier_ce = swow_register_internal_class(
        "Swow\\Sync\\ThreadBarrier", NULL, swow_thread_barrier_methods,
        &swow_thread_barrier_handlers, NULL,
        cat_false, cat_true,
        swow_thread_barrier_create_object, swow_thread_barrier_free_object,
        offsetof(swow_thread_barrier_object_t, std)
    );

    return SUCCESS;
}

zend_result swow_thread_barrier_module_shutdown(SHUTDOWN_FUNC_ARGS)
{
    return SUCCESS;
}

#endif /* ZTS */
