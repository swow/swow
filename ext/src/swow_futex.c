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

#include "swow_futex.h"

#include "swow_sync.h"

#include "cat_atomic.h"

#include <stdint.h>
#include <string.h>

#ifdef ZTS

#define SWOW_FUTEX_MAGIC 0x46555458U /* "FUTX" */
#define SWOW_FUTEX_VERSION 1

SWOW_API zend_class_entry *swow_futex_ce;
SWOW_API zend_object_handlers swow_futex_handlers;

typedef struct swow_futex_shared_s {
    uint32_t magic;
    uint32_t version;
    cat_atomic_int32_t value;
} swow_futex_shared_t;

typedef struct swow_futex_s {
    swow_shared_object_t object;
    uint32_t magic;
    cat_shm_t *shm;
    char *name;
    swow_futex_shared_t *shared;
} swow_futex_t;

typedef struct swow_futex_object_s {
    swow_futex_t *futex;
    zend_object std;
} swow_futex_object_t;

static swow_futex_object_t *swow_futex_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_futex_object_t, std);
}

static cat_bool_t swow_futex_free(swow_shared_object_t *object)
{
    swow_futex_t *futex = (swow_futex_t *) object;

    if (futex->shm != NULL) {
        cat_shm_detach(futex->shm);
        futex->shm = NULL;
    }
    if (futex->name != NULL) {
        cat_sys_free(futex->name);
        futex->name = NULL;
    }

    return cat_true;
}

static zend_object *swow_futex_create_object(zend_class_entry *ce)
{
    swow_futex_object_t *futex = swow_object_alloc(swow_futex_object_t, ce, swow_futex_handlers);

    futex->futex = NULL;

    return &futex->std;
}

static void swow_futex_free_object(zend_object *object)
{
    swow_futex_object_t *futex = swow_futex_get_from_object(object);

    if (futex->futex != NULL) {
        swow_shared_object_release(&futex->futex->object);
        futex->futex = NULL;
    }

    zend_object_std_dtor(&futex->std);
}

static swow_futex_t *swow_futex_create(const char *name, zend_long value)
{
    swow_futex_t *futex;
    cat_shm_t *shm;

    if (name == NULL) {
        shm = cat_shm_create_anonymous(sizeof(swow_futex_shared_t));
    } else {
        shm = cat_shm_create_named(name, sizeof(swow_futex_shared_t));
    }
    if (unlikely(shm == NULL)) {
        return NULL;
    }

    futex = (swow_futex_t *) swow_shared_object_create(sizeof(*futex), swow_futex_free);
    if (unlikely(futex == NULL)) {
        cat_shm_detach(shm);
        return NULL;
    }

    futex->magic = SWOW_FUTEX_MAGIC;
    futex->shm = shm;
    futex->shared = (swow_futex_shared_t *) cat_shm_get_address(shm);
    futex->shared->magic = SWOW_FUTEX_MAGIC;
    futex->shared->version = SWOW_FUTEX_VERSION;
    cat_atomic_int32_init(&futex->shared->value, (int32_t) value);

    if (name != NULL) {
        size_t length = strlen(name) + 1;

        futex->name = (char *) cat_sys_malloc(length);
        if (unlikely(futex->name == NULL)) {
            swow_shared_object_release(&futex->object);
            cat_update_last_error(CAT_ENOMEM, "Failed to allocate futex name");
            return NULL;
        }
        memcpy(futex->name, name, length);
    }

    return futex;
}

static swow_futex_t *swow_futex_attach(const char *name)
{
    swow_futex_t *futex;
    cat_shm_t *shm;
    swow_futex_shared_t *shared;

    shm = cat_shm_attach_named(name);
    if (unlikely(shm == NULL)) {
        return NULL;
    }
    if (unlikely(cat_shm_get_size(shm) < sizeof(swow_futex_shared_t))) {
        cat_shm_detach(shm);
        cat_update_last_error(CAT_EINVAL, "The futex mapping is too small");
        return NULL;
    }

    shared = (swow_futex_shared_t *) cat_shm_get_address(shm);
    if (unlikely(shared->magic != SWOW_FUTEX_MAGIC ||
                 shared->version != SWOW_FUTEX_VERSION)) {
        cat_shm_detach(shm);
        cat_update_last_error(CAT_EINVAL, "The mapping is not a futex");
        return NULL;
    }

    futex = (swow_futex_t *) swow_shared_object_create(sizeof(*futex), swow_futex_free);
    if (unlikely(futex == NULL)) {
        cat_shm_detach(shm);
        return NULL;
    }

    futex->magic = SWOW_FUTEX_MAGIC;
    futex->shm = shm;
    futex->shared = shared;

    {
        size_t length = strlen(name) + 1;

        futex->name = (char *) cat_sys_malloc(length);
        if (unlikely(futex->name == NULL)) {
            swow_shared_object_release(&futex->object);
            cat_update_last_error(CAT_ENOMEM, "Failed to allocate futex name");
            return NULL;
        }
        memcpy(futex->name, name, length);
    }

    return futex;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Futex___construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, value, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Futex_createNamed, 0, 1, Swow\\Futex, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, value, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Futex_attachNamed, 0, 1, Swow\\Futex, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Futex_unlink, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Futex_getValue, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Futex_setValue, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Futex_wait, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, expected, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Futex_wake, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, count, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Futex_getSharedId, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Futex___serialize, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Futex___unserialize, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Futex___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Futex, __construct)
{
    swow_futex_object_t *object = swow_futex_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long value = 0;
    swow_futex_t *futex;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(value)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(value < INT32_MIN || value > INT32_MAX)) {
        zend_value_error("The value must fit a 32-bit signed integer");
        RETURN_THROWS();
    }
    if (unlikely(object->futex != NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "Futex has already been constructed", 0);
        RETURN_THROWS();
    }

    futex = swow_futex_create(NULL, value);
    if (unlikely(futex == NULL)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    object->futex = futex;
}

static PHP_METHOD(Swow_Futex, createNamed)
{
    zend_string *name;
    zend_long value = 0;
    swow_futex_t *futex;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(name)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(value)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(ZSTR_LEN(name) == 0 || value < INT32_MIN || value > INT32_MAX)) {
        zend_value_error("The name must not be empty and the value must fit a 32-bit signed integer");
        RETURN_THROWS();
    }

    futex = swow_futex_create(ZSTR_VAL(name), value);
    if (unlikely(futex == NULL)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    object_init_ex(return_value, swow_futex_ce);
    swow_futex_get_from_object(Z_OBJ_P(return_value))->futex = futex;
}

static PHP_METHOD(Swow_Futex, attachNamed)
{
    zend_string *name;
    swow_futex_t *futex;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(ZSTR_LEN(name) == 0)) {
        zend_value_error("The name must not be empty");
        RETURN_THROWS();
    }

    futex = swow_futex_attach(ZSTR_VAL(name));
    if (unlikely(futex == NULL)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    object_init_ex(return_value, swow_futex_ce);
    swow_futex_get_from_object(Z_OBJ_P(return_value))->futex = futex;
}

static PHP_METHOD(Swow_Futex, unlink)
{
    zend_string *name;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(ZSTR_LEN(name) == 0)) {
        zend_value_error("The name must not be empty");
        RETURN_THROWS();
    }
    if (unlikely(!cat_shm_unlink(ZSTR_VAL(name)))) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }
}

static PHP_METHOD(Swow_Futex, getValue)
{
    swow_futex_object_t *object = swow_futex_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(object->futex == NULL)) {
        RETURN_LONG(0);
    }

    RETURN_LONG(cat_atomic_int32_load_explicit(
        &object->futex->shared->value, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE));
}

static PHP_METHOD(Swow_Futex, setValue)
{
    swow_futex_object_t *object = swow_futex_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long value;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(value)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(value < INT32_MIN || value > INT32_MAX)) {
        zend_value_error("The value must fit a 32-bit signed integer");
        RETURN_THROWS();
    }
    if (unlikely(object->futex == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "Futex has not been constructed", 0);
        RETURN_THROWS();
    }

    cat_atomic_int32_store_explicit(
        &object->futex->shared->value, (int32_t) value, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
}

static PHP_METHOD(Swow_Futex, wait)
{
    swow_futex_object_t *object = swow_futex_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long expected;
    zend_long timeout = -1;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(expected)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(expected < INT32_MIN || expected > INT32_MAX)) {
        zend_value_error("The expected value must fit a 32-bit signed integer");
        RETURN_THROWS();
    }
    if (unlikely(object->futex == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "Futex has not been constructed", 0);
        RETURN_THROWS();
    }

    if (!cat_futex_wait(
            (cat_futex_t *) &object->futex->shared->value, (int32_t) expected, timeout)) {
        cat_errno_t code = cat_get_last_error_code();

        if (code == CAT_EAGAIN || code == CAT_ETIMEDOUT) {
            RETURN_FALSE;
        }
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    RETURN_TRUE;
}

static PHP_METHOD(Swow_Futex, wake)
{
    swow_futex_object_t *object = swow_futex_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long count;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(count)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(count < INT32_MIN || count > INT32_MAX)) {
        zend_value_error("The wake count must fit a 32-bit signed integer");
        RETURN_THROWS();
    }
    if (unlikely(object->futex == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "Futex has not been constructed", 0);
        RETURN_THROWS();
    }

    if (unlikely(!cat_futex_wake(
                     (cat_futex_t *) &object->futex->shared->value, (int32_t) count))) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    RETURN_TRUE;
}

static PHP_METHOD(Swow_Futex, getSharedId)
{
    swow_futex_object_t *object = swow_futex_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(object->futex != NULL ? (zend_long) object->futex->object.id : 0);
}

static PHP_METHOD(Swow_Futex, __serialize)
{
    swow_futex_object_t *object = swow_futex_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(object->futex == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "Futex has not been constructed", 0);
        RETURN_THROWS();
    }

    if (object->futex->name != NULL) {
        array_init(return_value);
        add_assoc_string(return_value, "name", object->futex->name);
        return;
    }

    swow_shared_object_serialize(return_value, &object->futex->object);
}

static PHP_METHOD(Swow_Futex, __unserialize)
{
    swow_futex_object_t *object = swow_futex_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *data;
    zval *z_name;
    swow_futex_t *futex;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY(data)
    ZEND_PARSE_PARAMETERS_END();

    z_name = zend_hash_str_find(Z_ARRVAL_P(data), ZEND_STRL("name"));
    if (z_name != NULL) {
        if (unlikely(Z_TYPE_P(z_name) != IS_STRING)) {
            zend_throw_exception(swow_sync_exception_ce,
                                 "Invalid serialized Futex", 0);
            RETURN_THROWS();
        }
        futex = swow_futex_attach(Z_STRVAL_P(z_name));
        if (unlikely(futex == NULL)) {
            swow_throw_exception_with_last(swow_sync_exception_ce);
            RETURN_THROWS();
        }
        if (object->futex != NULL) {
            swow_shared_object_release(&object->futex->object);
        }
        object->futex = futex;
        return;
    }

    {
        swow_shared_object_t *shared;

        if (unlikely(!swow_shared_object_unserialize(Z_ARRVAL_P(data), &shared))) {
            swow_throw_exception_with_last(swow_sync_exception_ce);
            RETURN_THROWS();
        }
        futex = (swow_futex_t *) shared;
        if (unlikely(futex->magic != SWOW_FUTEX_MAGIC)) {
            swow_shared_object_release(shared);
            zend_throw_exception(swow_sync_exception_ce,
                                 "Invalid serialized Futex", 0);
            RETURN_THROWS();
        }
        if (object->futex != NULL) {
            swow_shared_object_release(&object->futex->object);
        }
        object->futex = futex;
    }
}

static PHP_METHOD(Swow_Futex, __debugInfo)
{
    swow_futex_object_t *object = swow_futex_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);
    if (object->futex != NULL) {
        if (object->futex->name != NULL) {
            add_assoc_string(return_value, "name", object->futex->name);
        }
        add_assoc_long(return_value, "value", cat_atomic_int32_load_explicit(
            &object->futex->shared->value, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE));
    }
}

static const zend_function_entry swow_futex_methods[] = {
    PHP_ME(Swow_Futex, __construct,   arginfo_class_Swow_Futex___construct,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Futex, createNamed,   arginfo_class_Swow_Futex_createNamed,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Futex, attachNamed,   arginfo_class_Swow_Futex_attachNamed,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Futex, unlink,        arginfo_class_Swow_Futex_unlink,        ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Futex, getValue,      arginfo_class_Swow_Futex_getValue,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Futex, setValue,      arginfo_class_Swow_Futex_setValue,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Futex, wait,          arginfo_class_Swow_Futex_wait,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Futex, wake,          arginfo_class_Swow_Futex_wake,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Futex, getSharedId,   arginfo_class_Swow_Futex_getSharedId,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Futex, __serialize,   arginfo_class_Swow_Futex___serialize,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Futex, __unserialize, arginfo_class_Swow_Futex___unserialize, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Futex, __debugInfo,   arginfo_class_Swow_Futex___debugInfo,   ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_futex_module_init(INIT_FUNC_ARGS)
{
    swow_futex_ce = swow_register_internal_class(
        "Swow\\Futex", NULL, swow_futex_methods,
        &swow_futex_handlers, NULL,
        cat_false, cat_true,
        swow_futex_create_object, swow_futex_free_object,
        offsetof(swow_futex_object_t, std)
    );

    return SUCCESS;
}

zend_result swow_futex_module_shutdown(SHUTDOWN_FUNC_ARGS)
{
    return SUCCESS;
}

#endif /* ZTS */
