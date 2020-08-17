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
  | Author: Twosee <twose@qq.com>                                            |
  +--------------------------------------------------------------------------+
 */

#include "swow_channel.h"

SWOW_API zend_class_entry *swow_channel_ce;
SWOW_API zend_object_handlers swow_channel_handlers;

SWOW_API zend_class_entry *swow_channel_exception_ce;

#define SWOW_CHANNEL_GETTER_INTERNAL(object, schannel, channel) \
    swow_channel_t *schannel = swow_channel_get_from_object(object); \
    cat_channel_t *channel = &schannel->channel

#define CHANNEL_HAS_CONSTRUCTED(channel) ((channel)->dtor == (cat_data_dtor_t) i_zval_ptr_dtor)

static zend_object *swow_channel_create_object(zend_class_entry *ce)
{
    swow_channel_t *schannel = swow_object_alloc(swow_channel_t, ce, swow_channel_handlers);

    schannel->channel.dtor = NULL;
    ZEND_GET_GC_BUFFER_INIT(schannel, zqueue);

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

    ZEND_GET_GC_BUFFER_FREE(schannel, zqueue);

    zend_object_std_dtor(&schannel->std);
}

#define SWOW_CHANNEL_GETTER(schannel, channel) \
        SWOW_CHANNEL_GETTER_INTERNAL(Z_OBJ_P(ZEND_THIS), schannel, channel); \

#define SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel) \
        SWOW_CHANNEL_GETTER(schannel, channel); \
        do { \
            if (UNEXPECTED(!CHANNEL_HAS_CONSTRUCTED(channel))) { \
                zend_throw_error(NULL, "%s must construct first", ZEND_THIS_NAME); \
                RETURN_THROWS(); \
            } \
        } while (0)

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

    channel = cat_channel_create(channel, capacity, sizeof(zval), (cat_data_dtor_t) i_zval_ptr_dtor);

    if (UNEXPECTED(channel == NULL)) {
        swow_throw_exception_with_last(swow_channel_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_channel_push, ZEND_RETURN_VALUE, 1, Swow\\Channel, 0)
    ZEND_ARG_INFO(0, data)
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
    ret = cat_channel_push_ex(channel, zdata, timeout);
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

    ret = cat_channel_pop_ex(channel, return_value, timeout);

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

    cat_channel_close(&schannel->channel);
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_channel_getBool, ZEND_RETURN_VALUE, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_channel_isEmpty arginfo_swow_channel_getBool

static PHP_METHOD(swow_channel, isEmpty)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(channel->length == 0);
}

#define arginfo_swow_channel_isFull arginfo_swow_channel_getBool

static PHP_METHOD(swow_channel, isFull)
{
    SWOW_CHANNEL_GETTER_CONSTRUCTED(schannel, channel);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(channel->length == channel->capacity);
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_channel___debugInfo, ZEND_RETURN_VALUE, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_channel, __debugInfo)
{
    SWOW_CHANNEL_GETTER(schannel, channel);
    zval zdebug_info;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(&zdebug_info);
    add_assoc_long(&zdebug_info, "capacity", channel->capacity);
    add_assoc_long(&zdebug_info, "length", channel->length);

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
    PHP_ME(swow_channel, isEmpty,      arginfo_swow_channel_isEmpty,      ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, isFull,       arginfo_swow_channel_isFull,       ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, hasProducers, arginfo_swow_channel_hasProducers, ZEND_ACC_PUBLIC)
    PHP_ME(swow_channel, hasConsumers, arginfo_swow_channel_hasConsumers, ZEND_ACC_PUBLIC)
    /* magic */
    PHP_ME(swow_channel, __debugInfo, arginfo_swow_channel___debugInfo, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static HashTable *swow_channel_get_gc(zend7_object *object, zval **gc_data, int *gc_count)
{
    SWOW_CHANNEL_GETTER_INTERNAL(Z7_OBJ_P(object), schannel, channel);
    cat_queue_t *storage = &channel->storage;

    ZEND_GET_GC_BUFFER_CREATE(schannel, zqueue, channel->length);

    CAT_QUEUE_FOREACH_DATA_START(storage, cat_channel_bucket_t, node, bucket) {
        ZEND_GET_GC_BUFFER_ADD(zqueue, (zval *) bucket->data);
    } CAT_QUEUE_FOREACH_DATA_END();

    ZEND_GET_GC_BUFFER_DONE(zqueue, gc_data, gc_count);
}

int swow_channel_module_init(INIT_FUNC_ARGS)
{
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

    swow_channel_exception_ce = swow_register_internal_class(
        "Swow\\Channel\\Exception", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}
