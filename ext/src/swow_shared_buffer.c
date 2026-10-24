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

#include "swow_shared_buffer.h"

#include "swow_sync.h"

#include <stdint.h>
#include <string.h>

#ifdef ZTS

#define SWOW_SHARED_BUFFER_MAGIC 0x53484246U /* "SHBF" */

enum {
    SWOW_SHARED_BUFFER_ENDIAN_NATIVE = 0,
    SWOW_SHARED_BUFFER_ENDIAN_LITTLE = 1,
    SWOW_SHARED_BUFFER_ENDIAN_BIG = 2,
};

SWOW_API zend_class_entry *swow_shared_buffer_ce;
SWOW_API zend_object_handlers swow_shared_buffer_handlers;

/* the tag of a shared buffer, so a serialized handle of another class is
 * rejected */
static const char swow_shared_buffer_type[] = "Swow\\SharedBuffer";

typedef struct swow_shared_buffer_s {
    swow_shared_object_t object;
    uint32_t magic;
    cat_shm_t *shm;
    char *name;
    void *address;
    size_t size;
} swow_shared_buffer_t;

typedef struct swow_shared_buffer_object_s {
    swow_shared_buffer_t *buffer;
    zend_object std;
} swow_shared_buffer_object_t;

static swow_shared_buffer_object_t *swow_shared_buffer_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_shared_buffer_object_t, std);
}

static cat_bool_t swow_shared_buffer_free(swow_shared_object_t *object)
{
    swow_shared_buffer_t *buffer = (swow_shared_buffer_t *) object;

    if (buffer->shm != NULL) {
        cat_shm_detach(buffer->shm);
        buffer->shm = NULL;
    }
    if (buffer->name != NULL) {
        cat_sys_free(buffer->name);
        buffer->name = NULL;
    }

    return cat_true;
}

static zend_object *swow_shared_buffer_create_object(zend_class_entry *ce)
{
    swow_shared_buffer_object_t *buffer =
        swow_object_alloc(swow_shared_buffer_object_t, ce, swow_shared_buffer_handlers);

    buffer->buffer = NULL;

    return &buffer->std;
}

static void swow_shared_buffer_free_object(zend_object *object)
{
    swow_shared_buffer_object_t *buffer = swow_shared_buffer_get_from_object(object);

    if (buffer->buffer != NULL) {
        swow_shared_object_release(&buffer->buffer->object);
        buffer->buffer = NULL;
    }

    zend_object_std_dtor(&buffer->std);
}

static swow_shared_buffer_t *swow_shared_buffer_create(const char *name, size_t size)
{
    swow_shared_buffer_t *buffer;
    cat_shm_t *shm;

    if (unlikely(size == 0)) {
        cat_update_last_error(CAT_EINVAL, "The shared buffer size must be positive");
        return NULL;
    }

    if (name == NULL) {
        shm = cat_shm_create_anonymous(size);
    } else {
        shm = cat_shm_create_named(name, size);
    }
    if (unlikely(shm == NULL)) {
        return NULL;
    }

    buffer = (swow_shared_buffer_t *) swow_shared_object_create(
        sizeof(*buffer), swow_shared_buffer_type, swow_shared_buffer_free);
    if (unlikely(buffer == NULL)) {
        cat_shm_detach(shm);
        return NULL;
    }

    buffer->magic = SWOW_SHARED_BUFFER_MAGIC;
    buffer->shm = shm;
    buffer->address = cat_shm_get_address(shm);
    buffer->size = cat_shm_get_size(shm);

    if (name != NULL) {
        size_t length = strlen(name) + 1;

        buffer->name = (char *) cat_sys_malloc(length);
        if (unlikely(buffer->name == NULL)) {
            swow_shared_object_release(&buffer->object);
            cat_update_last_error(CAT_ENOMEM, "Failed to allocate shared buffer name");
            return NULL;
        }
        memcpy(buffer->name, name, length);
    }

    return buffer;
}

static swow_shared_buffer_t *swow_shared_buffer_attach(const char *name)
{
    swow_shared_buffer_t *buffer;
    cat_shm_t *shm;

    shm = cat_shm_attach_named(name);
    if (unlikely(shm == NULL)) {
        return NULL;
    }

    buffer = (swow_shared_buffer_t *) swow_shared_object_create(
        sizeof(*buffer), swow_shared_buffer_type, swow_shared_buffer_free);
    if (unlikely(buffer == NULL)) {
        cat_shm_detach(shm);
        return NULL;
    }

    buffer->magic = SWOW_SHARED_BUFFER_MAGIC;
    buffer->shm = shm;
    buffer->address = cat_shm_get_address(shm);
    buffer->size = cat_shm_get_size(shm);

    {
        size_t length = strlen(name) + 1;

        buffer->name = (char *) cat_sys_malloc(length);
        if (unlikely(buffer->name == NULL)) {
            swow_shared_object_release(&buffer->object);
            cat_update_last_error(CAT_ENOMEM, "Failed to allocate shared buffer name");
            return NULL;
        }
        memcpy(buffer->name, name, length);
    }

    return buffer;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_SharedBuffer___construct, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, size, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_SharedBuffer_createNamed, 0, 2, Swow\\SharedBuffer, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, size, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_SharedBuffer_attachNamed, 0, 1, Swow\\SharedBuffer, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_unlink, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_getSize, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_getName, 0, 0, IS_STRING, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_getSharedId, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_read, 0, 2, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, offsetBytes, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, lengthBytes, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_write, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, offsetBytes, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_writeF32, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, offsetBytes, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, values, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, endian, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_viewF32, 0, 2, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, offsetBytes, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, count, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, endian, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_writeF64, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, offsetBytes, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, values, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, endian, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_viewF64, 0, 2, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, offsetBytes, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, count, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, endian, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

#define SWOW_SHARED_BUFFER_INT_ARGINFO(name) \
    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_write##name, 0, 2, IS_VOID, 0) \
        ZEND_ARG_TYPE_INFO(0, offsetBytes, IS_LONG, 0) \
        ZEND_ARG_TYPE_INFO(0, values, IS_ARRAY, 0) \
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, endian, IS_LONG, 0, "0") \
    ZEND_END_ARG_INFO() \
    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_view##name, 0, 2, IS_ARRAY, 0) \
        ZEND_ARG_TYPE_INFO(0, offsetBytes, IS_LONG, 0) \
        ZEND_ARG_TYPE_INFO(0, count, IS_LONG, 0) \
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, endian, IS_LONG, 0, "0") \
    ZEND_END_ARG_INFO()

SWOW_SHARED_BUFFER_INT_ARGINFO(I8)
SWOW_SHARED_BUFFER_INT_ARGINFO(U8)
SWOW_SHARED_BUFFER_INT_ARGINFO(I16)
SWOW_SHARED_BUFFER_INT_ARGINFO(U16)
SWOW_SHARED_BUFFER_INT_ARGINFO(I32)
SWOW_SHARED_BUFFER_INT_ARGINFO(U32)
SWOW_SHARED_BUFFER_INT_ARGINFO(I64)
SWOW_SHARED_BUFFER_INT_ARGINFO(U64)
#undef SWOW_SHARED_BUFFER_INT_ARGINFO

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_fill, 0, 3, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, offsetBytes, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, lengthBytes, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, byte, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_clear, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer_copyFrom, 0, 4, IS_VOID, 0)
    ZEND_ARG_OBJ_INFO(0, source, Swow\\SharedBuffer, 0)
    ZEND_ARG_TYPE_INFO(0, sourceOffsetBytes, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, destinationOffsetBytes, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, lengthBytes, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer___serialize, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer___unserialize, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SharedBuffer___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static cat_bool_t swow_shared_buffer_check_range(
    swow_shared_buffer_t *buffer, zend_long offset, zend_long length)
{
    if (unlikely(offset < 0 || length < 0 ||
                 (size_t) offset > buffer->size ||
                 (size_t) length > buffer->size - (size_t) offset)) {
        zend_value_error("The offset and length are outside the shared buffer");
        return cat_false;
    }

    return cat_true;
}

static cat_bool_t swow_shared_buffer_check_count(
    swow_shared_buffer_t *buffer, zend_long offset, zend_long count, size_t width)
{
    if (unlikely(offset < 0 || (size_t) offset > buffer->size ||
                 count < 0 || (size_t) count > SIZE_MAX / width ||
                 (size_t) count > (buffer->size - (size_t) offset) / width)) {
        zend_value_error("The offset and length are outside the shared buffer");
        return cat_false;
    }

    return cat_true;
}

static cat_always_inline void swow_shared_buffer_store_le(
    void *address, uint64_t value, size_t width)
{
#ifdef WORDS_BIGENDIAN
    uint8_t *bytes = (uint8_t *) address;
    size_t i;

    for (i = 0; i < width; i++) {
        bytes[i] = (uint8_t) (value >> (i * 8));
    }
#else
    switch (width) {
        case 1:
            *(uint8_t *) address = (uint8_t) value;
            break;
        case 2: {
            uint16_t v = (uint16_t) value;
            memcpy(address, &v, sizeof(v));
            break;
        }
        case 4: {
            uint32_t v = (uint32_t) value;
            memcpy(address, &v, sizeof(v));
            break;
        }
        case 8:
            memcpy(address, &value, sizeof(value));
            break;
        default:
            ZEND_UNREACHABLE();
    }
#endif
}

static cat_always_inline uint64_t swow_shared_buffer_load_le(
    const void *address, size_t width)
{
#ifdef WORDS_BIGENDIAN
    const uint8_t *bytes = (const uint8_t *) address;
    uint64_t value = 0;
    size_t i;

    for (i = 0; i < width; i++) {
        value |= (uint64_t) bytes[i] << (i * 8);
    }

    return value;
#else
    uint64_t value;

    switch (width) {
        case 1:
            value = *(const uint8_t *) address;
            break;
        case 2: {
            uint16_t v;
            memcpy(&v, address, sizeof(v));
            value = v;
            break;
        }
        case 4: {
            uint32_t v;
            memcpy(&v, address, sizeof(v));
            value = v;
            break;
        }
        case 8:
            memcpy(&value, address, sizeof(value));
            break;
        default:
            ZEND_UNREACHABLE();
    }

    return value;
#endif
}

static cat_bool_t swow_shared_buffer_parse_endian(zend_long endian, cat_bool_t *big_endian)
{
    switch (endian) {
        case SWOW_SHARED_BUFFER_ENDIAN_NATIVE:
#ifdef WORDS_BIGENDIAN
            *big_endian = cat_true;
#else
            *big_endian = cat_false;
#endif
            return cat_true;
        case SWOW_SHARED_BUFFER_ENDIAN_LITTLE:
            *big_endian = cat_false;
            return cat_true;
        case SWOW_SHARED_BUFFER_ENDIAN_BIG:
            *big_endian = cat_true;
            return cat_true;
        default:
            zend_value_error("The endian must be NATIVE_ENDIAN, LITTLE_ENDIAN or BIG_ENDIAN");
            return cat_false;
    }
}

static cat_always_inline void swow_shared_buffer_store_be(
    void *address, uint64_t value, size_t width)
{
    uint8_t *bytes = (uint8_t *) address;
    size_t i;

    for (i = 0; i < width; i++) {
        bytes[width - 1 - i] = (uint8_t) (value >> (i * 8));
    }
}

static cat_always_inline uint64_t swow_shared_buffer_load_be(
    const void *address, size_t width)
{
    const uint8_t *bytes = (const uint8_t *) address;
    uint64_t value = 0;
    size_t i;

    for (i = 0; i < width; i++) {
        value |= (uint64_t) bytes[width - 1 - i] << (i * 8);
    }

    return value;
}

static cat_always_inline void swow_shared_buffer_store_value(
    void *address, uint64_t value, size_t width, cat_bool_t big_endian)
{
    if (big_endian) {
        swow_shared_buffer_store_be(address, value, width);
    } else {
        swow_shared_buffer_store_le(address, value, width);
    }
}

static cat_always_inline uint64_t swow_shared_buffer_load_value(
    const void *address, size_t width, cat_bool_t big_endian)
{
    return big_endian
        ? swow_shared_buffer_load_be(address, width)
        : swow_shared_buffer_load_le(address, width);
}

static cat_bool_t swow_shared_buffer_write_ints(
    swow_shared_buffer_t *buffer, zend_long offset, zval *values, size_t width,
    cat_bool_t is_signed, cat_bool_t big_endian)
{
    zend_long count = (zend_long) zend_hash_num_elements(Z_ARRVAL_P(values));
    int64_t min = 0;
    uint64_t max = 0;
    char *address;
    zval *value;

    if (unlikely(!swow_shared_buffer_check_count(buffer, offset, count, width))) {
        return cat_false;
    }

    if (is_signed) {
        if (width < 8) {
            min = -((int64_t) 1 << (width * 8 - 1));
            max = (uint64_t) (((int64_t) 1 << (width * 8 - 1)) - 1);
        }
    } else {
        max = width == 8 ? (uint64_t) ZEND_LONG_MAX : ((uint64_t) 1 << (width * 8)) - 1;
    }

    address = (char *) buffer->address + offset;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(values), value) {
        zend_long number = zval_get_long(value);

        if (is_signed) {
            if (width < 8 && (number < min || number > (zend_long) max)) {
                zend_value_error("An integer does not fit the requested width");
                return cat_false;
            }
        } else if (number < 0 || (uint64_t) number > max) {
            zend_value_error("An integer does not fit the requested width");
            return cat_false;
        }

        swow_shared_buffer_store_value(address, (uint64_t) number, width, big_endian);
        address += width;
    } ZEND_HASH_FOREACH_END();

    return cat_true;
}

static void swow_shared_buffer_view_ints(
    zval *return_value, swow_shared_buffer_t *buffer, zend_long offset,
    zend_long length, size_t width, cat_bool_t is_signed, cat_bool_t big_endian)
{
    const char *address = (const char *) buffer->address + offset;
    zend_long i;

    array_init_size(return_value, (uint32_t) length);
    zend_hash_real_init_packed(Z_ARRVAL_P(return_value));
    ZEND_HASH_FILL_PACKED(Z_ARRVAL_P(return_value)) {
        for (i = 0; i < length; i++) {
            uint64_t bits = swow_shared_buffer_load_value(address, width, big_endian);
            if (is_signed && width < 8 && (bits & ((uint64_t) 1 << (width * 8 - 1)))) {
                bits |= ~(((uint64_t) 1 << (width * 8)) - 1);
            }
            ZEND_HASH_FILL_SET_LONG((zend_long) bits);
            ZEND_HASH_FILL_NEXT();
            address += width;
        }
    } ZEND_HASH_FILL_END();
}

static void swow_shared_buffer_store_f64(
    void *address, double value, cat_bool_t big_endian)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    swow_shared_buffer_store_value(address, bits, sizeof(bits), big_endian);
}

static double swow_shared_buffer_load_f64(
    const void *address, cat_bool_t big_endian)
{
    uint64_t bits = swow_shared_buffer_load_value(address, sizeof(bits), big_endian);
    double value;

    memcpy(&value, &bits, sizeof(value));

    return value;
}

static PHP_METHOD(Swow_SharedBuffer, __construct)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long size;
    swow_shared_buffer_t *buffer;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(size)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(size <= 0)) {
        zend_value_error("The size must be positive");
        RETURN_THROWS();
    }
    if (unlikely(object->buffer != NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "SharedBuffer has already been constructed", 0);
        RETURN_THROWS();
    }

    buffer = swow_shared_buffer_create(NULL, (size_t) size);
    if (unlikely(buffer == NULL)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    object->buffer = buffer;
}

static PHP_METHOD(Swow_SharedBuffer, createNamed)
{
    zend_string *name;
    zend_long size;
    swow_shared_buffer_t *buffer;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(name)
        Z_PARAM_LONG(size)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(size <= 0)) {
        zend_value_error("The size must be positive");
        RETURN_THROWS();
    }
    if (unlikely(ZSTR_LEN(name) == 0)) {
        zend_value_error("The name must not be empty");
        RETURN_THROWS();
    }

    buffer = swow_shared_buffer_create(ZSTR_VAL(name), (size_t) size);
    if (unlikely(buffer == NULL)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    object_init_ex(return_value, swow_shared_buffer_ce);
    swow_shared_buffer_get_from_object(Z_OBJ_P(return_value))->buffer = buffer;
}

static PHP_METHOD(Swow_SharedBuffer, attachNamed)
{
    zend_string *name;
    swow_shared_buffer_t *buffer;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(ZSTR_LEN(name) == 0)) {
        zend_value_error("The name must not be empty");
        RETURN_THROWS();
    }

    buffer = swow_shared_buffer_attach(ZSTR_VAL(name));
    if (unlikely(buffer == NULL)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }

    object_init_ex(return_value, swow_shared_buffer_ce);
    swow_shared_buffer_get_from_object(Z_OBJ_P(return_value))->buffer = buffer;
}

static PHP_METHOD(Swow_SharedBuffer, unlink)
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

static PHP_METHOD(Swow_SharedBuffer, getSize)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(object->buffer != NULL ? (zend_long) object->buffer->size : 0);
}

static PHP_METHOD(Swow_SharedBuffer, getName)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (object->buffer == NULL || object->buffer->name == NULL) {
        RETURN_NULL();
    }

    RETURN_STRING(object->buffer->name);
}

static PHP_METHOD(Swow_SharedBuffer, getSharedId)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(object->buffer != NULL ? (zend_long) object->buffer->object.id : 0);
}

static PHP_METHOD(Swow_SharedBuffer, read)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long offset;
    zend_long length;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(offset)
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(object->buffer == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "SharedBuffer has not been constructed", 0);
        RETURN_THROWS();
    }
    if (unlikely(!swow_shared_buffer_check_range(object->buffer, offset, length))) {
        RETURN_THROWS();
    }

    RETURN_STRINGL((char *) object->buffer->address + offset, (size_t) length);
}

static PHP_METHOD(Swow_SharedBuffer, write)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long offset;
    zend_string *data;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(offset)
        Z_PARAM_STR(data)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(object->buffer == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "SharedBuffer has not been constructed", 0);
        RETURN_THROWS();
    }
    if (unlikely(!swow_shared_buffer_check_range(
                     object->buffer, offset, (zend_long) ZSTR_LEN(data)))) {
        RETURN_THROWS();
    }

    if (ZSTR_LEN(data) > 0) {
        memcpy((char *) object->buffer->address + offset, ZSTR_VAL(data), ZSTR_LEN(data));
    }
}

static void swow_shared_buffer_store_f32(
    void *address, double value, cat_bool_t big_endian)
{
    float f = (float) value;
    uint32_t bits;

    memcpy(&bits, &f, sizeof(bits));
    swow_shared_buffer_store_value(address, bits, sizeof(bits), big_endian);
}

static double swow_shared_buffer_load_f32(
    const void *address, cat_bool_t big_endian)
{
    uint32_t bits = (uint32_t) swow_shared_buffer_load_value(
        address, sizeof(bits), big_endian);
    float f;

    memcpy(&f, &bits, sizeof(f));

    return f;
}

static PHP_METHOD(Swow_SharedBuffer, writeF32)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long offset;
    zval *values;
    zend_long length;
    zend_long index = 0;
    zend_long endian = SWOW_SHARED_BUFFER_ENDIAN_NATIVE;
    cat_bool_t big_endian;
    zval *value;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_LONG(offset)
        Z_PARAM_ARRAY(values)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(endian)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(object->buffer == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "SharedBuffer has not been constructed", 0);
        RETURN_THROWS();
    }
    if (unlikely(!swow_shared_buffer_parse_endian(endian, &big_endian))) {
        RETURN_THROWS();
    }

    length = (zend_long) zend_hash_num_elements(Z_ARRVAL_P(values));
    if (unlikely(length > ZEND_LONG_MAX / 4 ||
                 !swow_shared_buffer_check_range(object->buffer, offset, length * 4))) {
        RETURN_THROWS();
    }

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(values), value) {
        swow_shared_buffer_store_f32(
            (char *) object->buffer->address + offset + index * 4,
            zval_get_double(value), big_endian);
        index++;
    } ZEND_HASH_FOREACH_END();
}

static PHP_METHOD(Swow_SharedBuffer, viewF32)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long offset;
    zend_long length;
    zend_long i;
    zend_long endian = SWOW_SHARED_BUFFER_ENDIAN_NATIVE;
    cat_bool_t big_endian;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_LONG(offset)
        Z_PARAM_LONG(length)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(endian)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(object->buffer == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "SharedBuffer has not been constructed", 0);
        RETURN_THROWS();
    }
    if (unlikely(!swow_shared_buffer_parse_endian(endian, &big_endian))) {
        RETURN_THROWS();
    }
    if (unlikely(length > ZEND_LONG_MAX / 4 ||
                 !swow_shared_buffer_check_range(object->buffer, offset, length * 4))) {
        RETURN_THROWS();
    }

    array_init_size(return_value, (uint32_t) length);
    zend_hash_real_init_packed(Z_ARRVAL_P(return_value));
    ZEND_HASH_FILL_PACKED(Z_ARRVAL_P(return_value)) {
        for (i = 0; i < length; i++) {
            ZEND_HASH_FILL_SET_DOUBLE(swow_shared_buffer_load_f32(
                (char *) object->buffer->address + offset + i * 4, big_endian));
            ZEND_HASH_FILL_NEXT();
        }
    } ZEND_HASH_FILL_END();
}

static PHP_METHOD(Swow_SharedBuffer, writeF64)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long offset;
    zval *values;
    zend_long count;
    zend_long index = 0;
    zend_long endian = SWOW_SHARED_BUFFER_ENDIAN_NATIVE;
    cat_bool_t big_endian;
    zval *value;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_LONG(offset)
        Z_PARAM_ARRAY(values)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(endian)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(object->buffer == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "SharedBuffer has not been constructed", 0);
        RETURN_THROWS();
    }
    if (unlikely(!swow_shared_buffer_parse_endian(endian, &big_endian))) {
        RETURN_THROWS();
    }

    count = (zend_long) zend_hash_num_elements(Z_ARRVAL_P(values));
    if (unlikely(!swow_shared_buffer_check_count(object->buffer, offset, count, sizeof(double)))) {
        RETURN_THROWS();
    }

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(values), value) {
        swow_shared_buffer_store_f64(
            (char *) object->buffer->address + offset + index * (zend_long) sizeof(double),
            zval_get_double(value), big_endian);
        index++;
    } ZEND_HASH_FOREACH_END();
}

static PHP_METHOD(Swow_SharedBuffer, viewF64)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long offset;
    zend_long length;
    zend_long i;
    zend_long endian = SWOW_SHARED_BUFFER_ENDIAN_NATIVE;
    cat_bool_t big_endian;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_LONG(offset)
        Z_PARAM_LONG(length)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(endian)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(object->buffer == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "SharedBuffer has not been constructed", 0);
        RETURN_THROWS();
    }
    if (unlikely(!swow_shared_buffer_parse_endian(endian, &big_endian))) {
        RETURN_THROWS();
    }
    if (unlikely(!swow_shared_buffer_check_count(object->buffer, offset, length, sizeof(double)))) {
        RETURN_THROWS();
    }

    array_init_size(return_value, (uint32_t) length);
    zend_hash_real_init_packed(Z_ARRVAL_P(return_value));
    ZEND_HASH_FILL_PACKED(Z_ARRVAL_P(return_value)) {
        for (i = 0; i < length; i++) {
            ZEND_HASH_FILL_SET_DOUBLE(swow_shared_buffer_load_f64(
                (char *) object->buffer->address + offset + i * (zend_long) sizeof(double),
                big_endian));
            ZEND_HASH_FILL_NEXT();
        }
    } ZEND_HASH_FILL_END();
}

#define SWOW_SHARED_BUFFER_INT_METHOD(name, width, is_signed) \
static PHP_METHOD(Swow_SharedBuffer, write##name) \
{ \
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS)); \
    zend_long offset; \
    zval *values; \
    zend_long endian = SWOW_SHARED_BUFFER_ENDIAN_NATIVE; \
    cat_bool_t big_endian; \
    ZEND_PARSE_PARAMETERS_START(2, 3) \
        Z_PARAM_LONG(offset) \
        Z_PARAM_ARRAY(values) \
        Z_PARAM_OPTIONAL \
        Z_PARAM_LONG(endian) \
    ZEND_PARSE_PARAMETERS_END(); \
    if (unlikely(object->buffer == NULL)) { \
        zend_throw_exception(swow_sync_exception_ce, "SharedBuffer has not been constructed", 0); \
        RETURN_THROWS(); \
    } \
    if (unlikely(!swow_shared_buffer_parse_endian(endian, &big_endian))) { \
        RETURN_THROWS(); \
    } \
    if (unlikely(!swow_shared_buffer_write_ints( \
                     object->buffer, offset, values, width, is_signed, big_endian))) { \
        RETURN_THROWS(); \
    } \
} \
static PHP_METHOD(Swow_SharedBuffer, view##name) \
{ \
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS)); \
    zend_long offset; \
    zend_long length; \
    zend_long endian = SWOW_SHARED_BUFFER_ENDIAN_NATIVE; \
    cat_bool_t big_endian; \
    ZEND_PARSE_PARAMETERS_START(2, 3) \
        Z_PARAM_LONG(offset) \
        Z_PARAM_LONG(length) \
        Z_PARAM_OPTIONAL \
        Z_PARAM_LONG(endian) \
    ZEND_PARSE_PARAMETERS_END(); \
    if (unlikely(object->buffer == NULL)) { \
        zend_throw_exception(swow_sync_exception_ce, "SharedBuffer has not been constructed", 0); \
        RETURN_THROWS(); \
    } \
    if (unlikely(!swow_shared_buffer_parse_endian(endian, &big_endian))) { \
        RETURN_THROWS(); \
    } \
    if (unlikely(!swow_shared_buffer_check_count(object->buffer, offset, length, width))) { \
        RETURN_THROWS(); \
    } \
    swow_shared_buffer_view_ints(return_value, object->buffer, offset, length, width, is_signed, big_endian); \
}

SWOW_SHARED_BUFFER_INT_METHOD(I8, 1, cat_true)
SWOW_SHARED_BUFFER_INT_METHOD(U8, 1, cat_false)
SWOW_SHARED_BUFFER_INT_METHOD(I16, 2, cat_true)
SWOW_SHARED_BUFFER_INT_METHOD(U16, 2, cat_false)
SWOW_SHARED_BUFFER_INT_METHOD(I32, 4, cat_true)
SWOW_SHARED_BUFFER_INT_METHOD(U32, 4, cat_false)
SWOW_SHARED_BUFFER_INT_METHOD(I64, 8, cat_true)
SWOW_SHARED_BUFFER_INT_METHOD(U64, 8, cat_false)
#undef SWOW_SHARED_BUFFER_INT_METHOD

static PHP_METHOD(Swow_SharedBuffer, fill)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long offset;
    zend_long length;
    zend_long byte;

    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_LONG(offset)
        Z_PARAM_LONG(length)
        Z_PARAM_LONG(byte)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(object->buffer == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "SharedBuffer has not been constructed", 0);
        RETURN_THROWS();
    }
    if (unlikely(byte < 0 || byte > UINT8_MAX)) {
        zend_value_error("The fill byte must be between 0 and 255");
        RETURN_THROWS();
    }
    if (unlikely(!swow_shared_buffer_check_range(object->buffer, offset, length))) {
        RETURN_THROWS();
    }

    memset((char *) object->buffer->address + offset, (int) byte, (size_t) length);
}

static PHP_METHOD(Swow_SharedBuffer, clear)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(object->buffer == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "SharedBuffer has not been constructed", 0);
        RETURN_THROWS();
    }

    memset(object->buffer->address, 0, object->buffer->size);
}

static PHP_METHOD(Swow_SharedBuffer, copyFrom)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *source_object;
    zend_long source_offset;
    zend_long destination_offset;
    zend_long length;
    swow_shared_buffer_object_t *source;

    ZEND_PARSE_PARAMETERS_START(4, 4)
        Z_PARAM_OBJECT_OF_CLASS(source_object, swow_shared_buffer_ce)
        Z_PARAM_LONG(source_offset)
        Z_PARAM_LONG(destination_offset)
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(object->buffer == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "SharedBuffer has not been constructed", 0);
        RETURN_THROWS();
    }

    source = swow_shared_buffer_get_from_object(Z_OBJ_P(source_object));
    if (unlikely(source->buffer == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "Source SharedBuffer has not been constructed", 0);
        RETURN_THROWS();
    }
    if (unlikely(!swow_shared_buffer_check_range(source->buffer, source_offset, length) ||
                 !swow_shared_buffer_check_range(object->buffer, destination_offset, length))) {
        RETURN_THROWS();
    }

    if (length > 0) {
        memmove((char *) object->buffer->address + destination_offset,
                (char *) source->buffer->address + source_offset, (size_t) length);
    }
}

static PHP_METHOD(Swow_SharedBuffer, __serialize)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(object->buffer == NULL)) {
        zend_throw_exception(swow_sync_exception_ce,
                             "SharedBuffer has not been constructed", 0);
        RETURN_THROWS();
    }

    if (object->buffer->name != NULL) {
        array_init(return_value);
        add_assoc_string(return_value, "name", object->buffer->name);
        return;
    }

    swow_shared_object_serialize(return_value, &object->buffer->object);
}

static PHP_METHOD(Swow_SharedBuffer, __unserialize)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *data;
    zval *z_name;
    swow_shared_buffer_t *buffer;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY(data)
    ZEND_PARSE_PARAMETERS_END();

    z_name = zend_hash_str_find(Z_ARRVAL_P(data), ZEND_STRL("name"));
    if (z_name != NULL) {
        if (unlikely(Z_TYPE_P(z_name) != IS_STRING)) {
            zend_throw_exception(swow_sync_exception_ce,
                                 "Invalid serialized SharedBuffer", 0);
            RETURN_THROWS();
        }
        buffer = swow_shared_buffer_attach(Z_STRVAL_P(z_name));
    } else {
        swow_shared_object_t *shared;

        if (unlikely(!swow_shared_object_unserialize(Z_ARRVAL_P(data), swow_shared_buffer_type,
                                                     &shared))) {
            swow_throw_exception_with_last(swow_sync_exception_ce);
            RETURN_THROWS();
        }
        buffer = (swow_shared_buffer_t *) shared;
        if (unlikely(buffer->magic != SWOW_SHARED_BUFFER_MAGIC)) {
            swow_shared_object_release(shared);
            zend_throw_exception(swow_sync_exception_ce,
                                 "Invalid serialized SharedBuffer", 0);
            RETURN_THROWS();
        }
        if (object->buffer != NULL) {
            swow_shared_object_release(&object->buffer->object);
        }
        object->buffer = buffer;
        return;
    }

    if (unlikely(buffer == NULL)) {
        swow_throw_exception_with_last(swow_sync_exception_ce);
        RETURN_THROWS();
    }
    if (object->buffer != NULL) {
        swow_shared_object_release(&object->buffer->object);
    }
    object->buffer = buffer;
}

static PHP_METHOD(Swow_SharedBuffer, __debugInfo)
{
    swow_shared_buffer_object_t *object = swow_shared_buffer_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);
    if (object->buffer != NULL) {
        if (object->buffer->name != NULL) {
            add_assoc_string(return_value, "name", object->buffer->name);
        }
        add_assoc_long(return_value, "size", (zend_long) object->buffer->size);
    }
}

static const zend_function_entry swow_shared_buffer_methods[] = {
    PHP_ME(Swow_SharedBuffer, __construct,   arginfo_class_Swow_SharedBuffer___construct,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, createNamed,   arginfo_class_Swow_SharedBuffer_createNamed,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_SharedBuffer, attachNamed,   arginfo_class_Swow_SharedBuffer_attachNamed,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_SharedBuffer, unlink,        arginfo_class_Swow_SharedBuffer_unlink,        ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_SharedBuffer, getSize,       arginfo_class_Swow_SharedBuffer_getSize,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, getName,       arginfo_class_Swow_SharedBuffer_getName,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, getSharedId,   arginfo_class_Swow_SharedBuffer_getSharedId,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, read,          arginfo_class_Swow_SharedBuffer_read,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, write,         arginfo_class_Swow_SharedBuffer_write,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, writeF32,      arginfo_class_Swow_SharedBuffer_writeF32,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, viewF32,       arginfo_class_Swow_SharedBuffer_viewF32,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, writeF64,      arginfo_class_Swow_SharedBuffer_writeF64,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, viewF64,       arginfo_class_Swow_SharedBuffer_viewF64,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, writeI8,       arginfo_class_Swow_SharedBuffer_writeI8,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, viewI8,        arginfo_class_Swow_SharedBuffer_viewI8,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, writeU8,       arginfo_class_Swow_SharedBuffer_writeU8,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, viewU8,        arginfo_class_Swow_SharedBuffer_viewU8,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, writeI16,      arginfo_class_Swow_SharedBuffer_writeI16,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, viewI16,       arginfo_class_Swow_SharedBuffer_viewI16,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, writeU16,      arginfo_class_Swow_SharedBuffer_writeU16,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, viewU16,       arginfo_class_Swow_SharedBuffer_viewU16,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, writeI32,      arginfo_class_Swow_SharedBuffer_writeI32,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, viewI32,       arginfo_class_Swow_SharedBuffer_viewI32,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, writeU32,      arginfo_class_Swow_SharedBuffer_writeU32,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, viewU32,       arginfo_class_Swow_SharedBuffer_viewU32,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, writeI64,      arginfo_class_Swow_SharedBuffer_writeI64,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, viewI64,       arginfo_class_Swow_SharedBuffer_viewI64,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, writeU64,      arginfo_class_Swow_SharedBuffer_writeU64,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, viewU64,       arginfo_class_Swow_SharedBuffer_viewU64,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, fill,          arginfo_class_Swow_SharedBuffer_fill,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, clear,         arginfo_class_Swow_SharedBuffer_clear,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, copyFrom,      arginfo_class_Swow_SharedBuffer_copyFrom,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, __serialize,   arginfo_class_Swow_SharedBuffer___serialize,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, __unserialize, arginfo_class_Swow_SharedBuffer___unserialize, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SharedBuffer, __debugInfo,   arginfo_class_Swow_SharedBuffer___debugInfo,   ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_shared_buffer_module_init(INIT_FUNC_ARGS)
{
    swow_shared_buffer_ce = swow_register_internal_class(
        "Swow\\SharedBuffer", NULL, swow_shared_buffer_methods,
        &swow_shared_buffer_handlers, NULL,
        cat_false, cat_true,
        swow_shared_buffer_create_object, swow_shared_buffer_free_object,
        offsetof(swow_shared_buffer_object_t, std)
    );

    zend_declare_class_constant_long(
        swow_shared_buffer_ce, ZEND_STRL("NATIVE_ENDIAN"), SWOW_SHARED_BUFFER_ENDIAN_NATIVE);
    zend_declare_class_constant_long(
        swow_shared_buffer_ce, ZEND_STRL("LITTLE_ENDIAN"), SWOW_SHARED_BUFFER_ENDIAN_LITTLE);
    zend_declare_class_constant_long(
        swow_shared_buffer_ce, ZEND_STRL("BIG_ENDIAN"), SWOW_SHARED_BUFFER_ENDIAN_BIG);

    return SUCCESS;
}

zend_result swow_shared_buffer_module_shutdown(SHUTDOWN_FUNC_ARGS)
{
    return SUCCESS;
}

#endif /* ZTS */
