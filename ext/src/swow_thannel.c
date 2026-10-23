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

#include "swow_thannel.h"

#include "zend_smart_str.h"

#include "ext/standard/php_var.h"

#include <stdint.h>

#ifdef ZTS

SWOW_API zend_class_entry *swow_thannel_ce;
SWOW_API zend_class_entry *swow_thannel_exception_ce;
SWOW_API zend_object_handlers swow_thannel_handlers;

static cat_always_inline swow_thannel_object_t *swow_thannel_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_thannel_object_t, std);
}

static cat_bool_t swow_thannel_free(swow_shared_object_t *object)
{
    swow_thannel_t *thannel = (swow_thannel_t *) object;

    if (cat_thread_wait_is_parked(&thannel->thannel.readable.wait) ||
        cat_thread_wait_is_parked(&thannel->thannel.writable.wait)) {
        php_error_docref(NULL, E_WARNING, "Thannel is waited on and was not destroyed");
        return cat_false;
    }

    cat_thannel_cleanup(&thannel->thannel);

    return cat_true;
}

static zend_object *swow_thannel_create_object(zend_class_entry *ce)
{
    swow_thannel_object_t *s_thannel =
        swow_object_alloc(swow_thannel_object_t, ce, swow_thannel_handlers);

    s_thannel->thannel = NULL;

    return &s_thannel->std;
}

static void swow_thannel_free_object(zend_object *object)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(object);

    if (s_thannel->thannel != NULL) {
        swow_shared_object_release(&s_thannel->thannel->object);
        s_thannel->thannel = NULL;
    }

    zend_object_std_dtor(&s_thannel->std);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Thannel___construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, capacity, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Thannel_push, 0, 1, Swow\\Thannel, 0)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_pop, 0, 0, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_isClosed, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_getCapacity, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_getLength, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_isEmpty, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_isFull, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_willBlock, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel___serialize, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel___unserialize, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thannel, __construct)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long capacity = 0;
    swow_thannel_t *thannel;
    cat_thannel_size_t cat_capacity;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(capacity)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(capacity < -1 || (capacity > 0 && (zend_ulong) capacity > UINT32_MAX))) {
        zend_value_error("The capacity must be -1 or between 0 and %u", UINT32_MAX);
        RETURN_THROWS();
    }

    if (unlikely(s_thannel->thannel != NULL)) {
        zend_throw_exception(swow_thannel_exception_ce, "Thannel has already been constructed", 0);
        RETURN_THROWS();
    }

    thannel = (swow_thannel_t *) swow_shared_object_create(sizeof(*thannel), NULL);
    if (unlikely(thannel == NULL)) {
        swow_throw_exception_with_last(swow_thannel_exception_ce);
        RETURN_THROWS();
    }

    cat_capacity = capacity == -1 ? UINT32_MAX : (cat_thannel_size_t) capacity;

    if (unlikely(cat_thannel_create(&thannel->thannel, cat_capacity) == NULL)) {
        swow_shared_object_release(&thannel->object);
        swow_throw_exception_with_last(swow_thannel_exception_ce);
        RETURN_THROWS();
    }

    thannel->object.free = swow_thannel_free;
    s_thannel->thannel = thannel;
}

static PHP_METHOD(Swow_Thannel, push)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *z_data;
    zend_long timeout = -1;
    smart_str buf = { 0 };
    php_serialize_data_t var_hash;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ZVAL(z_data)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(s_thannel->thannel == NULL)) {
        zend_throw_exception(swow_thannel_exception_ce, "Thannel has not been constructed", 0);
        RETURN_THROWS();
    }

    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&buf, z_data, &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);

    if (EG(exception) != NULL || buf.s == NULL) {
        smart_str_free(&buf);
        if (EG(exception) == NULL) {
            zend_throw_exception(swow_thannel_exception_ce, "Failed to serialize the message", 0);
        }
        RETURN_THROWS();
    }

    if (!cat_thannel_push(&s_thannel->thannel->thannel,
                          ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), timeout)) {
        smart_str_free(&buf);
        swow_throw_exception_with_last(swow_thannel_exception_ce);
        RETURN_THROWS();
    }

    smart_str_free(&buf);

    RETURN_THIS();
}

static PHP_METHOD(Swow_Thannel, pop)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long timeout = -1;
    void *data = NULL;
    size_t size = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(s_thannel->thannel == NULL)) {
        zend_throw_exception(swow_thannel_exception_ce, "Thannel has not been constructed", 0);
        RETURN_THROWS();
    }

    if (!cat_thannel_pop(&s_thannel->thannel->thannel, &data, &size, timeout)) {
        swow_throw_exception_with_last(swow_thannel_exception_ce);
        RETURN_THROWS();
    }

    if (data == NULL || size == 0) {
        if (data != NULL) {
            cat_sys_free(data);
        }
        zend_throw_exception(swow_thannel_exception_ce, "Received an empty message", 0);
        RETURN_THROWS();
    }

    ZVAL_UNDEF(return_value);
    {
        php_unserialize_data_t var_hash;
        const unsigned char *p = (const unsigned char *) data;
        cat_bool_t ok;

        PHP_VAR_UNSERIALIZE_INIT(var_hash);
        ok = php_var_unserialize(return_value, &p, p + size, &var_hash);
        PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

        cat_sys_free(data);

        if (!ok) {
            if (Z_TYPE_P(return_value) != IS_UNDEF) {
                zval_ptr_dtor(return_value);
            }
            ZVAL_UNDEF(return_value);
            zend_throw_exception(swow_thannel_exception_ce, "Failed to unserialize the message", 0);
            RETURN_THROWS();
        }
    }
}

static PHP_METHOD(Swow_Thannel, close)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_thannel->thannel == NULL)) {
        zend_throw_exception(swow_thannel_exception_ce, "Thannel has not been constructed", 0);
        RETURN_THROWS();
    }

    if (unlikely(!cat_thannel_close(&s_thannel->thannel->thannel))) {
        swow_throw_exception_with_last(swow_thannel_exception_ce);
        RETURN_THROWS();
    }
}

static PHP_METHOD(Swow_Thannel, isClosed)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_thannel->thannel == NULL)) {
        RETURN_FALSE;
    }

    RETURN_BOOL(cat_thannel_is_closed(&s_thannel->thannel->thannel));
}

static PHP_METHOD(Swow_Thannel, getCapacity)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_thannel->thannel == NULL)) {
        RETURN_LONG(0);
    }

    RETURN_LONG((zend_long) cat_thannel_get_capacity(&s_thannel->thannel->thannel));
}

static PHP_METHOD(Swow_Thannel, getLength)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_thannel->thannel == NULL)) {
        RETURN_LONG(0);
    }

    RETURN_LONG((zend_long) cat_thannel_get_length(&s_thannel->thannel->thannel));
}

static PHP_METHOD(Swow_Thannel, isEmpty)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_thannel->thannel == NULL)) {
        RETURN_TRUE;
    }

    RETURN_BOOL(cat_thannel_is_empty(&s_thannel->thannel->thannel));
}

static PHP_METHOD(Swow_Thannel, isFull)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_thannel->thannel == NULL)) {
        RETURN_FALSE;
    }

    if (cat_thannel_get_capacity(&s_thannel->thannel->thannel) ==
        CAT_THANNEL_CAPACITY_UNBUFFERED) {
        RETURN_TRUE;
    }

    RETURN_BOOL(cat_thannel_is_full(&s_thannel->thannel->thannel));
}

static PHP_METHOD(Swow_Thannel, willBlock)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_thannel->thannel == NULL)) {
        RETURN_FALSE;
    }

    RETURN_BOOL(cat_thannel_is_empty(&s_thannel->thannel->thannel) ||
                cat_thannel_is_full(&s_thannel->thannel->thannel));
}

static PHP_METHOD(Swow_Thannel, __serialize)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_thannel->thannel == NULL)) {
        zend_throw_exception(swow_thannel_exception_ce, "Thannel has not been constructed", 0);
        RETURN_THROWS();
    }

    swow_shared_object_serialize(return_value, &s_thannel->thannel->object);
}

static PHP_METHOD(Swow_Thannel, __unserialize)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *z_data;
    swow_shared_object_t *object;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY(z_data)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(!swow_shared_object_unserialize(Z_ARRVAL_P(z_data), &object))) {
        swow_throw_exception_with_last(swow_thannel_exception_ce);
        RETURN_THROWS();
    }

    if (s_thannel->thannel != NULL) {
        swow_shared_object_release(&s_thannel->thannel->object);
    }
    s_thannel->thannel = (swow_thannel_t *) object;
}

static PHP_METHOD(Swow_Thannel, __debugInfo)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);

    if (s_thannel->thannel != NULL) {
        add_assoc_long(return_value, "capacity",
                       (zend_long) cat_thannel_get_capacity(&s_thannel->thannel->thannel));
        add_assoc_long(return_value, "length",
                       (zend_long) cat_thannel_get_length(&s_thannel->thannel->thannel));
        add_assoc_bool(return_value, "closed",
                       cat_thannel_is_closed(&s_thannel->thannel->thannel));
    }
}

static const zend_function_entry swow_thannel_methods[] = {
    PHP_ME(Swow_Thannel, __construct,   arginfo_class_Swow_Thannel___construct,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, push,          arginfo_class_Swow_Thannel_push,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, pop,           arginfo_class_Swow_Thannel_pop,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, close,         arginfo_class_Swow_Thannel_close,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, isClosed,      arginfo_class_Swow_Thannel_isClosed,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, getCapacity,   arginfo_class_Swow_Thannel_getCapacity,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, getLength,     arginfo_class_Swow_Thannel_getLength,     ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, isEmpty,       arginfo_class_Swow_Thannel_isEmpty,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, isFull,        arginfo_class_Swow_Thannel_isFull,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, willBlock,     arginfo_class_Swow_Thannel_willBlock,     ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, __serialize,   arginfo_class_Swow_Thannel___serialize,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, __unserialize, arginfo_class_Swow_Thannel___unserialize, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, __debugInfo,   arginfo_class_Swow_Thannel___debugInfo,   ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_thannel_module_init(INIT_FUNC_ARGS)
{
    swow_thannel_exception_ce = swow_register_internal_class(
        "Swow\\ThannelException", swow_exception_ce, NULL, NULL, NULL,
        cat_true, cat_true, NULL, NULL, 0
    );

    swow_thannel_ce = swow_register_internal_class(
        "Swow\\Thannel", NULL, swow_thannel_methods,
        &swow_thannel_handlers, NULL,
        cat_false, cat_true,
        swow_thannel_create_object, swow_thannel_free_object,
        offsetof(swow_thannel_object_t, std)
    );

    return SUCCESS;
}

zend_result swow_thannel_module_shutdown(SHUTDOWN_FUNC_ARGS)
{
    return SUCCESS;
}

#endif /* ZTS */
