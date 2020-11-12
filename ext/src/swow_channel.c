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

#include "swow_channel.h"

SWOW_API zend_class_entry *swow_channel_ce;
SWOW_API zend_object_handlers swow_channel_handlers;

SWOW_API zend_class_entry *swow_channel_selector_ce;
SWOW_API zend_object_handlers swow_channel_selector_handlers;

SWOW_API zend_class_entry *swow_channel_exception_ce;
SWOW_API zend_class_entry *swow_channel_selector_exception_ce;

#define SWOW_CHANNEL_GETTER_INTERNAL(object, schannel, channel) \
    swow_channel_t *schannel = swow_channel_get_from_object(object); \
    cat_channel_t *channel = &schannel->channel

#define CHANNEL_HAS_CONSTRUCTED(channel) ((channel)->dtor == (cat_channel_data_dtor_t) i_zval_ptr_dtor)

static zend_object *swow_channel_create_object(zend_class_entry *ce)
{
    swow_channel_t *schannel = swow_object_alloc(swow_channel_t, ce, swow_channel_handlers);

    schannel->channel.dtor = NULL;
    ZEND_GET_GC_BUFFER_INIT(schannel, zgc_buffer);

    return &schannel->std;
}

static void swow_channel_dtor_object(zend_object *object)
{
    /* try to call __destruct first */
    zend_objects_destroy_object(object);

    /* force close the channel (as far as possible before free_obj) */
    SWOW_CHANNEL_GETTER_INTERNAL(object, schannel, channel);

    if (UNEXPECTED(!CHANNEL_HAS_CONSTRUCTED(channel))) {
        return;
    }

    cat_channel_close(channel);
}

static void swow_channel_free_object(zend_object *object)
{
    SWOW_CHANNEL_GETTER_INTERNAL(object, schannel, channel);

    if (EXPECTED(CHANNEL_HAS_CONSTRUCTED(channel))) {
        cat_channel_close(channel);
    }

    ZEND_GET_GC_BUFFER_FREE(schannel, zgc_buffer);

    zend_object_std_dtor(&schannel->std);
}

#define SWOW_CHANNEL_CHECK(channel) do { \
    if (UNEXPECTED(!CHANNEL_HAS_CONSTRUCTED(channel))) { \
        zend_throw_error(NULL, "%s must construct first", ZEND_THIS_NAME); \
        RETURN_THROWS(); \
    } \
} while (0)

#define SWOW_CHANNEL_GETTER(schannel, channel) \
        SWOW_CHANNEL_GETTER_INTERNAL(Z_OBJ_P(ZEND_THIS), schannel, channel); \

#define SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel) \
        SWOW_CHANNEL_GETTER(schannel, channel); \
        SWOW_CHANNEL_CHECK(channel)

ZEND_BEGIN_ARG_INFO_EX(arginfo_swow_channel___construct, 0, ZEND_RETURN_VALUE, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, capacity, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_channel, __construct)
{
    SWOW_CHANNEL_GETTER(schannel, channel);
    zend_long capacity = 0;

    if (UNEXPECTED(CHANNEL_HAS_CONSTRUCTED(channel))) {
        zend_throw_error(NULL, "%s can only construct once", ZEND_THIS_NAME);
        RETURN_THROWS();
    }

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(capacity)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(capacity < 0)) {
        zend_argument_value_error(1, "can not be negative");
        RETURN_THROWS();
    }

    channel = cat_channel_create(channel, capacity, sizeof(zval), (cat_channel_data_dtor_t) i_zval_ptr_dtor);

    if (UNEXPECTED(channel == NULL)) {
        swow_throw_exception_with_last(swow_channel_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_channel_push, 1)
    ZEND_ARG_TYPE_INFO(0, data, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_channel, push)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);
    zval *zdata;
    zend_long timeout = -1;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ZVAL(zdata)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    Z_TRY_ADDREF_P(zdata);
    ret = cat_channel_push(channel, zdata, timeout);
    if (UNEXPECTED(!ret)) {
        Z_TRY_DELREF_P(zdata);
        swow_throw_exception_with_last(swow_channel_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_swow_channel_pop, 0, ZEND_RETURN_VALUE, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_channel, pop)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);
    zend_long timeout = -1;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    ret = cat_channel_pop(channel, return_value, timeout);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_channel_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_swow_channel_close, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_channel, close)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    cat_channel_close(channel);
}

/* status */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_channel_getLong, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_channel_getCapacity arginfo_swow_channel_getLong

static PHP_METHOD(swow_channel, getCapacity)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(channel->capacity);
}

#define arginfo_swow_channel_getLength arginfo_swow_channel_getLong

static PHP_METHOD(swow_channel, getLength)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(channel->length);
}

#define arginfo_swow_channel_isAvailable arginfo_swow_channel_getBool

static PHP_METHOD(swow_channel, isAvailable)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_channel_is_available(channel));
}

#define arginfo_swow_channel_hasProducers arginfo_swow_channel_getBool

static PHP_METHOD(swow_channel, hasProducers)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_channel_has_producers(channel));
}

#define arginfo_swow_channel_hasConsumers arginfo_swow_channel_getBool

static PHP_METHOD(swow_channel, hasConsumers)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_channel_has_consumers(channel));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_channel_getBool, ZEND_RETURN_VALUE, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_channel_isEmpty arginfo_swow_channel_getBool

static PHP_METHOD(swow_channel, isEmpty)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_channel_is_empty(channel));
}

#define arginfo_swow_channel_isFull arginfo_swow_channel_getBool

static PHP_METHOD(swow_channel, isFull)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_channel_is_full(channel));
}

#define arginfo_swow_channel_isReadable arginfo_swow_channel_getBool

static PHP_METHOD(swow_channel, isReadable)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_channel_is_readable(channel));
}

#define arginfo_swow_channel_isWritable arginfo_swow_channel_getBool

static PHP_METHOD(swow_channel, isWritable)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_channel_is_writable(channel));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_channel___debugInfo, ZEND_RETURN_VALUE, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_channel, __debugInfo)
{
    SWOW_CHANNEL_GETTER(schannel, channel);
    zval zdebug_info;

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(!CHANNEL_HAS_CONSTRUCTED(channel))) {
        return;
    }

    array_init(&zdebug_info);
    add_assoc_long(&zdebug_info, "capacity", channel->capacity);
    add_assoc_long(&zdebug_info, "length", channel->length);
    add_assoc_bool(&zdebug_info, "readable", cat_channel_is_readable(channel));
    add_assoc_bool(&zdebug_info, "writable", cat_channel_is_writable(channel));

    RETURN_DEBUG_INFO_WITH_PROPERTIES(&zdebug_info);
}

static const zend_function_entry swow_channel_methods[] = {
    PHP_ME(swow_channel, __construct,  arginfo_swow_channel___construct,  ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, push,         arginfo_swow_channel_push,         ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, pop,          arginfo_swow_channel_pop,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, close,        arginfo_swow_channel_close,        ZEND_ACC_PUBLIC)
    /* status */
    PHP_ME(swow_channel, getCapacity,  arginfo_swow_channel_getCapacity,  ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, getLength,    arginfo_swow_channel_getLength,    ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, isAvailable,  arginfo_swow_channel_isAvailable,  ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, hasProducers, arginfo_swow_channel_hasProducers, ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, hasConsumers, arginfo_swow_channel_hasConsumers, ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, isEmpty,      arginfo_swow_channel_isEmpty,      ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, isFull,       arginfo_swow_channel_isFull,       ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, isReadable,   arginfo_swow_channel_isReadable,   ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, isWritable,   arginfo_swow_channel_isWritable,   ZEND_ACC_PUBLIC)
    /* magic */
    PHP_ME(swow_channel, __debugInfo,  arginfo_swow_channel___debugInfo,  ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static HashTable *swow_channel_get_gc(zend7_object *object, zval **gc_data, int *gc_count)
{
    SWOW_CHANNEL_GETTER_INTERNAL(Z7_OBJ_P(object), schannel, channel);

    if (channel->length == 0) {
        *gc_data = NULL;
        *gc_count = 0;
        return zend_std_get_properties(object);
    }

    ZEND_GET_GC_BUFFER_CREATE(schannel, zgc_buffer, channel->length);

    CAT_QUEUE_FOREACH_DATA_START(&channel->u.buffered.storage, cat_channel_bucket_t, node, bucket) {
        ZEND_GET_GC_BUFFER_ADD(zgc_buffer, (zval *) bucket->data);
    } CAT_QUEUE_FOREACH_DATA_END();

    ZEND_GET_GC_BUFFER_DONE(zgc_buffer, gc_data, gc_count);
}

/* select */

static void swow_channel_selector_release_requests(swow_channel_selector_t *selector)
{
    cat_channel_select_request_t *request;
    uint32_t n;

    for (n = 0, request = selector->requests; n < selector->count; n++, request++) {
        zend_object_release(&(swow_channel_get_from_handle(request->channel)->std));
        if (request->opcode == CAT_CHANNEL_OPCODE_PUSH) {
            zval_ptr_dtor((zval *) request->data.common);
        }
    }

    selector->count = 0;
}

static void swow_channel_selector_release_response(swow_channel_selector_t *selector)
{
    zval_ptr_dtor(&selector->zdata);
}

static zend_object *swow_channel_selector_create_object(zend_class_entry *ce)
{
    swow_channel_selector_t *selector = swow_object_alloc(swow_channel_selector_t, ce, swow_channel_selector_handlers);

    selector->count = 0;
    selector->requests = selector->_requests;
    selector->size = CAT_ARRAY_SIZE(selector->_requests);
    selector->last_opcode = CAT_CHANNEL_OPCODE_PUSH;
    ZVAL_NULL(&selector->zdata);
    ZEND_GET_GC_BUFFER_INIT(selector, zgc_buffer);

    return &selector->std;
}

static void swow_channel_selector_free_object(zend_object *object)
{
    swow_channel_selector_t *selector = swow_channel_selector_get_from_object(object);

    swow_channel_selector_release_requests(selector);
    if (selector->requests != selector->_requests) {
        efree(selector->requests);
    }
    swow_channel_selector_release_response(selector);

    ZEND_GET_GC_BUFFER_FREE(selector, zgc_buffer);

    zend_object_std_dtor(&selector->std);
}

#define getThisSelector() swow_channel_selector_get_from_object(Z_OBJ_P(ZEND_THIS))

static PHP_METHOD_EX(swow_channel_selector, add, zval *zchannel, zval *zdata)
{
    swow_channel_selector_t *selector = getThisSelector();
    swow_channel_t *schannel = swow_channel_get_from_object(Z_OBJ_P(zchannel));
    cat_channel_t *channel = &schannel->channel;
    cat_channel_select_request_t *requests, *request;
    zval *zstorage, *zbucket;

    SWOW_CHANNEL_CHECK(channel);

    requests = selector->requests;
    if (requests == selector->_requests) {
        zstorage = selector->zstorage;
    } else {
        zstorage = (zval *) (selector->requests + selector->size);
    }

    if (UNEXPECTED(selector->count == selector->size)) {
        size_t n;
        /* extend size to 2x */
        selector->size += selector->size;
        selector->requests = emalloc((sizeof(*request) + sizeof(*zbucket)) * selector->size);
        memcpy(selector->requests,                  requests, sizeof(*request) * selector->count);
        memcpy(selector->requests + selector->size, zstorage, sizeof(*zbucket) * selector->count);
        if (requests != selector->_requests) {
            efree(requests);
        }
        requests = selector->requests;
        zstorage = (zval *) (requests + selector->size);
        for (n = 0, request = requests, zbucket = zstorage; n < selector->count; n++, request++, zbucket++) {
            request->data.common = zbucket;
        }
    }

    request = &requests[selector->count];
    zbucket = &zstorage[selector->count];
    /* copy channel */
    GC_ADDREF(&schannel->std);
    request->channel = channel;
    /* solve data */
    if (zdata != NULL) {
        request->opcode = CAT_CHANNEL_OPCODE_PUSH;
        ZVAL_COPY(zbucket, zdata);
        request->data.in = zbucket;
    } else {
        request->opcode = CAT_CHANNEL_OPCODE_POP;
        ZVAL_UNDEF(zbucket);
        request->data.out = zbucket;
    }
    /* count++ */
    selector->count++;

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_channel_selector_push, 2)
    ZEND_ARG_OBJ_INFO(0, channel, Swow\\Channel, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_MIXED, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_channel_selector, push)
{
    zval *zchannel, *zdata;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT_OF_CLASS(zchannel, swow_channel_ce)
        Z_PARAM_ZVAL(zdata)
    ZEND_PARSE_PARAMETERS_END();

    PHP_METHOD_CALL(swow_channel_selector, add, zchannel, zdata);
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_channel_selector_pop, 1)
    ZEND_ARG_OBJ_INFO(0, channel, Swow\\Channel, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_channel_selector, pop)
{
    zval *zchannel;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(zchannel, swow_channel_ce)
    ZEND_PARSE_PARAMETERS_END();

    PHP_METHOD_CALL(swow_channel_selector, add, zchannel, NULL);
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_channel_selector_do, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_channel_selector, do)
{
    swow_channel_selector_t *selector = getThisSelector();
    zend_long timeout = -1;
    cat_channel_select_response_t *response;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    response = cat_channel_select(selector->requests, selector->count, timeout);

    /* release the last response */
    swow_channel_selector_release_response(selector);

    /* update the response info */
    if (EXPECTED(response != NULL)) {
        zend_object *channel = &(swow_channel_get_from_handle(response->channel)->std);
        zval *zdata = (zval *) response->data.common;
        GC_ADDREF(channel);
        RETVAL_OBJ(channel);
        selector->last_opcode = response->opcode;
        if (response->opcode == CAT_CHANNEL_OPCODE_PUSH) {
            ZVAL_UNDEF(&selector->zdata);
        } else {
            ZVAL_COPY_VALUE(&selector->zdata, zdata);
        }
        /* data has been comsumed or copied */
        ZVAL_UNDEF(zdata);
    } else {
        ZVAL_UNDEF(&selector->zdata);
    }

    /* reset */
    swow_channel_selector_release_requests(selector);

    /* handle error */
    if (UNEXPECTED(response == NULL || response->error)) {
        swow_throw_call_exception_with_last(swow_channel_selector_exception_ce);
        RETURN_THROWS_ASSERTION();
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_swow_channel_selector_fetch, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_channel_selector, fetch)
{
    swow_channel_selector_t *selector = getThisSelector();
    zval *zdata = &selector->zdata;

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(Z_TYPE_P(zdata) == IS_UNDEF)) {
        zend_throw_error(NULL, "No data");
        RETURN_THROWS();
    }

    RETVAL_ZVAL(zdata, 0, 0);

    ZVAL_UNDEF(zdata);
}

#define arginfo_swow_channel_selector_getLastOpcode arginfo_swow_channel_getLong

static PHP_METHOD(swow_channel_selector, getLastOpcode)
{
    swow_channel_selector_t *selector = getThisSelector();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(selector->last_opcode);
}

static const zend_function_entry swow_channel_selector_methods[] = {
    PHP_ME(swow_channel_selector, push,          arginfo_swow_channel_selector_push,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel_selector, pop,           arginfo_swow_channel_selector_pop,           ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel_selector, do,            arginfo_swow_channel_selector_do,            ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel_selector, fetch,         arginfo_swow_channel_selector_fetch,         ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel_selector, getLastOpcode, arginfo_swow_channel_selector_getLastOpcode, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static HashTable *swow_channel_selector_get_gc(zend7_object *object, zval **gc_data, int *gc_count)
{
    swow_channel_selector_t *selector = swow_channel_selector_get_from_object(Z7_OBJ_P(object));
    cat_channel_select_request_t *request;
    uint32_t n;

    if (selector->count == 0 && !Z_REFCOUNTED_P(&selector->zdata)) {
        *gc_data = NULL;
        *gc_count = 0;
        return zend_std_get_properties(object);
    }

    ZEND_GET_GC_BUFFER_CREATE(selector, zgc_buffer, selector->count + 1);

    /* request */
    for (n = 0, request = selector->requests; n < selector->count; n++, request++) {
        if (request->opcode == CAT_CHANNEL_OPCODE_PUSH) {
            ZEND_GET_GC_BUFFER_ADD(zgc_buffer, (zval *) request->data.common);
        }
    }
    /* response */
    ZEND_GET_GC_BUFFER_ADD(zgc_buffer, &selector->zdata);

    ZEND_GET_GC_BUFFER_DONE(zgc_buffer, gc_data, gc_count);
}

int swow_channel_module_init(INIT_FUNC_ARGS)
{
    /* channel */

    swow_channel_ce = swow_register_internal_class(
        "Swow\\Channel", NULL, swow_channel_methods,
        &swow_channel_handlers, NULL,
        cat_false, cat_false, cat_false,
        swow_channel_create_object,
        swow_channel_free_object,
        XtOffsetOf(swow_channel_t, std)
    );
    swow_channel_handlers.get_gc = swow_channel_get_gc;
    swow_channel_handlers.dtor_obj = swow_channel_dtor_object;

    zend_declare_class_constant_long(swow_channel_ce, ZEND_STRL("OPCODE_PUSH"), CAT_CHANNEL_OPCODE_PUSH);
    zend_declare_class_constant_long(swow_channel_ce, ZEND_STRL("OPCODE_POP"), CAT_CHANNEL_OPCODE_POP);

    swow_channel_exception_ce = swow_register_internal_class(
        "Swow\\Channel\\Exception", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );

    /* selector */

    swow_channel_selector_ce = swow_register_internal_class(
        "Swow\\Channel\\Selector", NULL, swow_channel_selector_methods,
        &swow_channel_selector_handlers, NULL,
        cat_false, cat_false, cat_false,
        swow_channel_selector_create_object,
        swow_channel_selector_free_object,
        XtOffsetOf(swow_channel_selector_t, std)
    );
    swow_channel_selector_handlers.get_gc = swow_channel_selector_get_gc;

    swow_channel_selector_exception_ce = swow_register_internal_class(
        "Swow\\Channel\\Selector\\Exception", swow_call_exception_ce, NULL, NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}
