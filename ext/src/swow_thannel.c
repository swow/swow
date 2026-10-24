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

/* the tag of a shared queue, so a serialized handle of another class is
 * rejected */
static const char swow_thannel_type[] = "Swow\\Thannel";

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

static cat_bool_t swow_thannel_serialize_value(zval *value, smart_str *buffer)
{
    php_serialize_data_t var_hash;

    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(buffer, value, &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);

    if (unlikely(EG(exception) != NULL || buffer->s == NULL)) {
        smart_str_free(buffer);
        return cat_false;
    }

    return cat_true;
}

static cat_bool_t swow_thannel_unserialize_value(
    zval *return_value, const void *data, size_t size)
{
    php_unserialize_data_t var_hash;
    const unsigned char *p = (const unsigned char *) data;
    cat_bool_t ok;

    ZVAL_UNDEF(return_value);
    PHP_VAR_UNSERIALIZE_INIT(var_hash);
    ok = php_var_unserialize(return_value, &p, p + size, &var_hash);
    PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

    if (unlikely(!ok || EG(exception) != NULL)) {
        if (Z_TYPE_P(return_value) != IS_UNDEF) {
            zval_ptr_dtor(return_value);
        }
        ZVAL_UNDEF(return_value);
        return cat_false;
    }

    return cat_true;
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_pushMany, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, values, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_popMany, 0, 1, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, max, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_drain, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_getWakeCount, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thannel_getParkCount, 0, 0, IS_LONG, 0)
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

    thannel = (swow_thannel_t *) swow_shared_object_create(sizeof(*thannel), swow_thannel_type, NULL);
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

static PHP_METHOD(Swow_Thannel, pushMany)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *values;
    zend_long timeout = -1;
    smart_str *buffers;
    cat_thannel_item_t *items;
    uint32_t count;
    uint32_t i = 0;
    zval *value;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ARRAY(values)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(s_thannel->thannel == NULL)) {
        zend_throw_exception(swow_thannel_exception_ce, "Thannel has not been constructed", 0);
        RETURN_THROWS();
    }

    count = zend_hash_num_elements(Z_ARRVAL_P(values));
    if (count == 0) {
        RETURN_LONG(0);
    }

    buffers = (smart_str *) ecalloc(count, sizeof(*buffers));
    items = (cat_thannel_item_t *) ecalloc(count, sizeof(*items));

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(values), value) {
        if (unlikely(!swow_thannel_serialize_value(value, &buffers[i]))) {
            if (EG(exception) == NULL) {
                zend_throw_exception(swow_thannel_exception_ce,
                                     "Failed to serialize the message", 0);
            }
            goto _serialize_error;
        }
        items[i].data = ZSTR_VAL(buffers[i].s);
        items[i].size = ZSTR_LEN(buffers[i].s);
        i++;
    } ZEND_HASH_FOREACH_END();

    if (unlikely(!cat_thannel_pushv(&s_thannel->thannel->thannel, items, count, timeout))) {
        swow_throw_exception_with_last(swow_thannel_exception_ce);
        goto _serialize_error;
    }

    for (i = 0; i < count; i++) {
        smart_str_free(&buffers[i]);
    }
    efree(items);
    efree(buffers);
    RETURN_LONG((zend_long) count);

    _serialize_error:
    for (i = 0; i < count; i++) {
        smart_str_free(&buffers[i]);
    }
    efree(items);
    efree(buffers);
    RETURN_THROWS();
}

static PHP_METHOD(Swow_Thannel, popMany)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long max;
    zend_long timeout = -1;
    cat_thannel_item_t *items;
    size_t count = 0;
    size_t i;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(max)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(s_thannel->thannel == NULL)) {
        zend_throw_exception(swow_thannel_exception_ce, "Thannel has not been constructed", 0);
        RETURN_THROWS();
    }
    if (unlikely(max <= 0 || (zend_ulong) max > SIZE_MAX / sizeof(*items))) {
        zend_value_error("The max must be positive and fit the platform");
        RETURN_THROWS();
    }

    items = (cat_thannel_item_t *) ecalloc((size_t) max, sizeof(*items));
    if (unlikely(!cat_thannel_popv(
                     &s_thannel->thannel->thannel, items,
                     (size_t) max, &count, timeout))) {
        efree(items);
        swow_throw_exception_with_last(swow_thannel_exception_ce);
        RETURN_THROWS();
    }

    array_init(return_value);
    for (i = 0; i < count; i++) {
        zval value;

        if (unlikely(!swow_thannel_unserialize_value(
                         &value, items[i].data, items[i].size))) {
            size_t j;

            cat_sys_free(items[i].data);
            for (j = i + 1; j < count; j++) {
                cat_sys_free(items[j].data);
            }
            efree(items);
            zval_ptr_dtor(return_value);
            if (EG(exception) == NULL) {
                zend_throw_exception(swow_thannel_exception_ce,
                                     "Failed to unserialize the message", 0);
            }
            RETURN_THROWS();
        }
        add_next_index_zval(return_value, &value);
        cat_sys_free(items[i].data);
    }

    efree(items);
}

static PHP_METHOD(Swow_Thannel, drain)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));
    cat_thannel_item_t *items;
    size_t length;
    size_t count;
    size_t i;

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_thannel->thannel == NULL)) {
        zend_throw_exception(swow_thannel_exception_ce, "Thannel has not been constructed", 0);
        RETURN_THROWS();
    }

    length = cat_thannel_get_length(&s_thannel->thannel->thannel);
    if (length == 0) {
        RETURN_EMPTY_ARRAY();
    }

    items = (cat_thannel_item_t *) ecalloc(length, sizeof(*items));
    count = cat_thannel_drain(&s_thannel->thannel->thannel, items, length);

    array_init(return_value);
    for (i = 0; i < count; i++) {
        zval value;

        if (unlikely(!swow_thannel_unserialize_value(
                         &value, items[i].data, items[i].size))) {
            size_t j;

            cat_sys_free(items[i].data);
            for (j = i + 1; j < count; j++) {
                cat_sys_free(items[j].data);
            }
            efree(items);
            zval_ptr_dtor(return_value);
            if (EG(exception) == NULL) {
                zend_throw_exception(swow_thannel_exception_ce,
                                     "Failed to unserialize the message", 0);
            }
            RETURN_THROWS();
        }
        add_next_index_zval(return_value, &value);
        cat_sys_free(items[i].data);
    }

    efree(items);
}

static PHP_METHOD(Swow_Thannel, getWakeCount)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(s_thannel->thannel != NULL ?
                (zend_long) cat_thannel_get_wake_count(&s_thannel->thannel->thannel) : 0);
}

static PHP_METHOD(Swow_Thannel, getParkCount)
{
    swow_thannel_object_t *s_thannel = swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(s_thannel->thannel != NULL ?
                (zend_long) cat_thannel_get_park_count(&s_thannel->thannel->thannel) : 0);
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

    if (unlikely(!swow_shared_object_unserialize(Z_ARRVAL_P(z_data), swow_thannel_type,
                                                 &object))) {
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
    PHP_ME(Swow_Thannel, pushMany,      arginfo_class_Swow_Thannel_pushMany,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, popMany,       arginfo_class_Swow_Thannel_popMany,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, drain,         arginfo_class_Swow_Thannel_drain,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, getWakeCount,  arginfo_class_Swow_Thannel_getWakeCount,  ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thannel, getParkCount,  arginfo_class_Swow_Thannel_getParkCount,  ZEND_ACC_PUBLIC)
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
