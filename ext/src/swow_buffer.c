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

#include "swow_buffer.h"

SWOW_API zend_class_entry *swow_buffer_ce;
SWOW_API zend_object_handlers swow_buffer_handlers;

SWOW_API zend_class_entry *swow_buffer_exception_ce;

static zend_always_inline zend_string *swow_buffer_get_string_from_value(char *value)
{
    return (zend_string *) (value - offsetof(zend_string, val));
}

static zend_always_inline zend_string *swow_buffer_get_string_from_handle(cat_buffer_t *buffer) SWOW_UNSAFE
{
    return swow_buffer_get_string_from_value(buffer->value);
}

#define VECTOR_POSITION_FMT "[%u][%u] "
#define VECTOR_POSTION_C    vector_index, arg_num - 1
#define ZEND_LONG_ARG_FMT   "($%s = " ZEND_LONG_FMT ") "
#define ZEND_LONG_ARG_C     arg_name, arg

static ZEND_COLD zend_never_inline void swow_buffer_unallocated_argument_error(uint32_t vector_arg_num, uint32_t vector_index, uint32_t arg_num)
{
#define BUFFER_IS_UNALLOCATED_FMT "buffer is not unallocated"
    if (vector_arg_num == 0) {
        zend_argument_value_error(arg_num, BUFFER_IS_UNALLOCATED_FMT);
    } else {
        zend_argument_value_error(vector_arg_num, VECTOR_POSITION_FMT "($buffer) " BUFFER_IS_UNALLOCATED_FMT, VECTOR_POSTION_C);
    }
#undef BUFFER_IS_UNALLOCATED_FMT
}

static ZEND_COLD zend_never_inline void swow_buffer_argument_can_not_be_negative_error(uint32_t vector_arg_num, uint32_t vector_index, uint32_t arg_num, const char *arg_name, zend_long arg)
{
#define CAN_NOT_BE_NEGATIVE_FMT "can not be negative"
    if (vector_arg_num == 0) {
        zend_argument_value_error(arg_num, CAN_NOT_BE_NEGATIVE_FMT);
    } else {
        zend_argument_value_error(vector_arg_num, VECTOR_POSITION_FMT ZEND_LONG_ARG_FMT CAN_NOT_BE_NEGATIVE_FMT, VECTOR_POSTION_C, ZEND_LONG_ARG_C);
    }
#undef CAN_NOT_BE_NEGATIVE_FMT
}

static ZEND_COLD zend_never_inline void swow_buffer_argument_can_not_be_greater_than_its_x_error(const char *type, uint32_t vector_arg_num, uint32_t vector_index, uint32_t arg_num, const char *arg_name, zend_long arg, const char *x_name, size_t x)
{
#define CAN_NOT_BE_GREATER_THAN_ITS_X_FMT "can not be greater than %s %s (%zu)"
#define CAN_NOT_BE_GREATER_THAN_ITS_X_C   type, x_name, x
    if (vector_arg_num == 0) {
        zend_argument_value_error(arg_num, CAN_NOT_BE_GREATER_THAN_ITS_X_FMT, CAN_NOT_BE_GREATER_THAN_ITS_X_C);
    } else {
        zend_argument_value_error(vector_arg_num, VECTOR_POSITION_FMT ZEND_LONG_ARG_FMT CAN_NOT_BE_GREATER_THAN_ITS_X_FMT, VECTOR_POSTION_C, ZEND_LONG_ARG_C, CAN_NOT_BE_GREATER_THAN_ITS_X_C);
    }
#undef CAN_NOT_BE_GREATER_THAN_ITS_X_C
}

static ZEND_COLD zend_never_inline void swow_buffer_argument_can_not_be_equal_to_its_x_error(const char *type, uint32_t vector_arg_num, uint32_t vector_index, uint32_t arg_num, const char *arg_name, zend_long arg, const char *x_name, size_t x)
{
#define CAN_NOT_BE_EQUAL_TO_ITS_X_FMT "can not be equal to %s %s (%zu)"
#define CAN_NOT_BE_EQUAL_TO_ITS_X_C   type, x_name, x
    if (vector_arg_num == 0) {
        zend_argument_value_error(arg_num, CAN_NOT_BE_EQUAL_TO_ITS_X_FMT, CAN_NOT_BE_EQUAL_TO_ITS_X_C);
    } else {
        zend_argument_value_error(vector_arg_num, VECTOR_POSITION_FMT ZEND_LONG_ARG_FMT CAN_NOT_BE_EQUAL_TO_ITS_X_FMT, VECTOR_POSTION_C, ZEND_LONG_ARG_C, CAN_NOT_BE_EQUAL_TO_ITS_X_C);
    }
#undef CAN_NOT_BE_EQUAL_TO_ITS_X_C
}

static ZEND_COLD zend_never_inline void swow_buffer_argument_with_x_can_not_be_greater_than_its_y_error(const char *type, uint32_t vector_arg_num, uint32_t vector_index, uint32_t arg_num, const char *arg_name, zend_long arg, const char *x_name, size_t x, const char *y_name, size_t y)
{
#define WITH_X_CAN_NOT_BE_GREATER_THAN_ITS_Y_FMT "with %s (" ZEND_LONG_FMT ") can not be greater than %s %s (%zu)"
#define WITH_X_CAN_NOT_BE_GREATER_THAN_ITS_Y_C   x_name, x, type, y_name, y
    if (vector_arg_num == 0) {
        zend_argument_value_error(arg_num + 0, WITH_X_CAN_NOT_BE_GREATER_THAN_ITS_Y_FMT, WITH_X_CAN_NOT_BE_GREATER_THAN_ITS_Y_C);
    } else {
        zend_argument_value_error(vector_arg_num, VECTOR_POSITION_FMT ZEND_LONG_ARG_FMT WITH_X_CAN_NOT_BE_GREATER_THAN_ITS_Y_FMT, VECTOR_POSTION_C, ZEND_LONG_ARG_C, WITH_X_CAN_NOT_BE_GREATER_THAN_ITS_Y_C);
    }
#undef WITH_X_CAN_NOT_BE_GREATER_THAN_ITS_Y_FMT
}

static ZEND_COLD zend_never_inline void swow_buffer_argument_can_only_be_minus_1_to_refer_to_unlimited_when_it_is_negative_error(uint32_t vector_arg_num, uint32_t vector_index, uint32_t arg_num, const char *arg_name, zend_long arg)
{
#define CAN_ONLY_BE_MINUS_1_TO_REFER_TO_UNLIMITED_WHEN_IT_IS_NEGATIVE_FMT "can only be -1 to refer to unlimited when it is negative"
    if (vector_arg_num == 0) {
        zend_argument_value_error(arg_num, CAN_ONLY_BE_MINUS_1_TO_REFER_TO_UNLIMITED_WHEN_IT_IS_NEGATIVE_FMT);
    } else {
        zend_argument_value_error(vector_arg_num, VECTOR_POSITION_FMT ZEND_LONG_ARG_FMT CAN_ONLY_BE_MINUS_1_TO_REFER_TO_UNLIMITED_WHEN_IT_IS_NEGATIVE_FMT, VECTOR_POSTION_C, ZEND_LONG_ARG_C);
    }
#undef CAN_ONLY_BE_MINUS_1_TO_REFER_TO_UNLIMITED_WHEN_IT_IS_NEGATIVE_FMT
}

static ZEND_COLD zend_never_inline void swow_buffer_argument_can_not_be_zero_error(uint32_t vector_arg_num, uint32_t vector_index, uint32_t arg_num, const char *arg_name)
{
#define NO_ENOUGH_WRITABLE_BUFFER_SPACE_FMT "can not be 0"
    if (vector_arg_num == 0) {
        zend_argument_value_error(arg_num, NO_ENOUGH_WRITABLE_BUFFER_SPACE_FMT);
    } else {
        zend_argument_value_error(
            vector_arg_num, VECTOR_POSITION_FMT "($%s)" NO_ENOUGH_WRITABLE_BUFFER_SPACE_FMT,
            VECTOR_POSTION_C, arg_name
        );
    }
#undef NO_ENOUGH_WRITABLE_BUFFER_SPACE_FMT
}

#undef VECTOR_POSTION_C
#undef VECTOR_POSITION_FMT

static zend_always_inline bool swow_buffer__check_readable_space(const char *type, size_t buffer_length, zend_long user_start, zend_long *user_length_ptr, uint32_t vector_arg_num, uint32_t vector_index, uint32_t base_arg_num)
{
    if (UNEXPECTED(user_start < 0)) {
        swow_buffer_argument_can_not_be_negative_error(vector_arg_num, vector_index, base_arg_num, "start", user_start);
        return false;
    }
    if (UNEXPECTED(((size_t) user_start) > buffer_length)) {
        /* we do not expect user to read uninitialized memory */
        swow_buffer_argument_can_not_be_greater_than_its_x_error(type, vector_arg_num, vector_index, base_arg_num, "start", user_start, "length", buffer_length);
        return false;
    }
    zend_long user_length = *user_length_ptr;
    if (user_length == -1) {
        user_length = buffer_length - user_start;
    }
    if (UNEXPECTED(user_length < 0)) {
        swow_buffer_argument_can_only_be_minus_1_to_refer_to_unlimited_when_it_is_negative_error(vector_arg_num, vector_index, base_arg_num + 1, "length", user_length);
        return false;
    }
    if (UNEXPECTED(((size_t) (user_start + user_length)) > buffer_length)) {
        swow_buffer_argument_with_x_can_not_be_greater_than_its_y_error(type, vector_arg_num, vector_index, base_arg_num + 1, "length", user_length, "start", user_start, "length", buffer_length);
        return false;
    }
    *user_length_ptr = user_length;
    return true;
}

static zend_always_inline bool swow_buffer__check_writable_space(size_t buffer_size, size_t buffer_length, zend_long user_offset, zend_long *user_size_ptr, uint32_t vector_arg_num, uint32_t vector_index, uint32_t base_arg_num)
{
    if (UNEXPECTED(user_offset < 0)) {
        swow_buffer_argument_can_not_be_negative_error(vector_arg_num, vector_index, base_arg_num, "offset", user_offset);
        return false;
    }
    if (UNEXPECTED(((size_t) user_offset) > buffer_length)) {
        ZEND_ASSERT(buffer_length <= buffer_size);
        /* we do not expect user to cross uninitialized memory to write */
        swow_buffer_argument_can_not_be_greater_than_its_x_error("buffer", vector_arg_num, vector_index, base_arg_num, "offset", user_offset, "length", buffer_length);
        return false;
    }
    if (UNEXPECTED(((size_t) user_offset) == buffer_size)) {
        swow_buffer_argument_can_not_be_equal_to_its_x_error("buffer", vector_arg_num, vector_index, base_arg_num, "offset", user_offset, "size", buffer_size);
        return false;
    }
    zend_long user_size = *user_size_ptr;
    if (user_size == -1) {
        user_size = buffer_size - user_offset;
    }
    if (UNEXPECTED(user_size < 0)) {
        swow_buffer_argument_can_only_be_minus_1_to_refer_to_unlimited_when_it_is_negative_error(vector_arg_num, vector_index, base_arg_num + 1, "size", user_size);
        return false;
    }
    if (UNEXPECTED(user_size == 0)) {
        swow_buffer_argument_can_not_be_zero_error(vector_arg_num, vector_index, base_arg_num + 1, "size");
        return false;
    }
    if (UNEXPECTED(((size_t) (user_offset + user_size)) > buffer_size)) {
        swow_buffer_argument_with_x_can_not_be_greater_than_its_y_error("buffer", vector_arg_num, vector_index, base_arg_num + 1, "size", user_size, "offset", user_offset, "size", buffer_size);
        return false;
    }
    *user_size_ptr = user_size;
    return true;
}

SWOW_API zend_string *swow_buffer_get_string(swow_buffer_t *sbuffer)
{
    cat_buffer_t *buffer = &sbuffer->buffer;
    if (UNEXPECTED(buffer->value == NULL)) {
        return NULL;
    }
    return swow_buffer_get_string_from_handle(buffer);
}

SWOW_API char *swow_string_get_readable_space_v(zend_string *string, zend_long start, zend_long *length, uint32_t vector_arg_num, uint32_t vector_index, uint32_t base_arg_num)
{
    if (!swow_buffer__check_readable_space("string", ZSTR_LEN(string), start, length, vector_arg_num, vector_index, base_arg_num + 1)) {
        return NULL;
    }
    return ZSTR_VAL(string) + start;
}

SWOW_API char *swow_buffer_get_readable_space_v(swow_buffer_t *s_buffer, zend_long start, zend_long *length, uint32_t vector_arg_num, uint32_t vector_index, uint32_t base_arg_num)
{
    if (UNEXPECTED(s_buffer->buffer.value == NULL)) {
        swow_buffer_unallocated_argument_error(vector_arg_num, vector_index, base_arg_num);
        return NULL;
    }
    if (!swow_buffer__check_readable_space("buffer", s_buffer->buffer.length, start, length, vector_arg_num, vector_index, base_arg_num + 1)) {
        return NULL;
    }
    return s_buffer->buffer.value + start;
}

SWOW_API char *swow_buffer_get_writable_space_v(swow_buffer_t *s_buffer, zend_long offset, zend_long *size, uint32_t vector_arg_num, uint32_t vector_index, uint32_t base_arg_num)
{
    if (UNEXPECTED(s_buffer->buffer.value == NULL)) {
        swow_buffer_unallocated_argument_error(vector_arg_num, vector_index, base_arg_num);
        return NULL;
    }
    if (!swow_buffer__check_writable_space(s_buffer->buffer.size, s_buffer->buffer.length, offset, size, vector_arg_num, vector_index, base_arg_num + 1)) {
        return NULL;
    }
    return s_buffer->buffer.value + offset;
}

SWOW_API void swow_buffer_virtual_write(swow_buffer_t *sbuffer, size_t offset, size_t length)
{
    cat_buffer_t *buffer = &sbuffer->buffer;
    zend_string *string = swow_buffer_get_string_from_handle(buffer);
    size_t new_length = offset + length;

    if (EXPECTED(new_length > buffer->length)) {
        ZSTR_VAL(string)[ZSTR_LEN(string) = (buffer->length = new_length)] = '\0';
    }
}

SWOW_API void swow_buffer_update(swow_buffer_t *sbuffer, size_t length)
{
    cat_buffer_t *buffer = &sbuffer->buffer;
    zend_string *string = swow_buffer_get_string_from_handle(buffer);

    ZSTR_VAL(string)[ZSTR_LEN(string) = (buffer->length = length)] = '\0';
}

static zend_always_inline void swow_buffer_reset(swow_buffer_t *sbuffer)
{
    ZEND_ASSERT(sbuffer->locker == NULL);
}

static zend_always_inline void swow_buffer_close(swow_buffer_t *sbuffer)
{
    cat_buffer_close(&sbuffer->buffer);
    swow_buffer_reset(sbuffer);
}

static zend_object *swow_buffer_create_object(zend_class_entry *ce)
{
    swow_buffer_t *sbuffer = swow_object_alloc(swow_buffer_t, ce, swow_buffer_handlers);

    cat_buffer_init(&sbuffer->buffer);
    sbuffer->locker = NULL;

    return &sbuffer->std;
}

static void swow_buffer_free_object(zend_object *object)
{
    swow_buffer_t *sbuffer = swow_buffer_get_from_object(object);

    cat_buffer_close(&sbuffer->buffer);

    zend_object_std_dtor(&sbuffer->std);
}

#define getThisBuffer() (swow_buffer_get_from_object(Z_OBJ_P(ZEND_THIS)))

#define SWOW_BUFFER_GETTER(_sbuffer, _buffer) \
    swow_buffer_t *_sbuffer = getThisBuffer(); \
    cat_buffer_t *buffer = &sbuffer->buffer; \

static zend_never_inline void swow_buffer_separate(swow_buffer_t *sbuffer)
{
    cat_buffer_t new_buffer;
    CAT_LOG_DEBUG_VA_WITH_LEVEL(BUFFER, 5, {
        char *s;
        CAT_LOG_DEBUG_D(HTTP, "Buffer separate occurred (length=%zu, value=%s)",
            sbuffer->buffer.length, cat_log_buffer_quote(sbuffer->buffer.value, sbuffer->buffer.length, &s));
        cat_free(s);
    });
    (void) cat_buffer_dup(&sbuffer->buffer, &new_buffer);
    cat_buffer_close(&sbuffer->buffer);
    sbuffer->buffer = new_buffer;
}

SWOW_API void swow_buffer_cow(swow_buffer_t *sbuffer)
{
    if (sbuffer->buffer.value != NULL) {
        zend_string *string = swow_buffer_get_string_from_handle(&sbuffer->buffer);
        if (GC_REFCOUNT(string) != 1 || ZSTR_IS_INTERNED(string) || (GC_FLAGS(string) & IS_STR_PERSISTENT)) {
            swow_buffer_separate(sbuffer);
        }
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_alignSize, 0, 0, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, alignment, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, alignSize)
{
    zend_long size = 0, alignment = 0;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(size)
        Z_PARAM_LONG(alignment)
    ZEND_PARSE_PARAMETERS_END();

    size = cat_buffer_align_size(size, alignment);

    RETURN_LONG(size);
}

static PHP_METHOD_EX(Swow_Buffer, create)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    zend_long size;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(size)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(size < 0)) {
        zend_argument_value_error(1, "can not be negative");
        RETURN_THROWS();
    }

    ret = cat_buffer_alloc(buffer, size);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_buffer_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Buffer___construct, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, size, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, __construct)
{
    PHP_METHOD_CALL(Swow_Buffer, create);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_alloc, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, size, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, alloc)
{
    PHP_METHOD_CALL(Swow_Buffer, create);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_getSize, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, getSize)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(buffer->size);
}

#define arginfo_class_Swow_Buffer_getLength arginfo_class_Swow_Buffer_getSize

static PHP_METHOD(Swow_Buffer, getLength)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(buffer->length);
}

#define arginfo_class_Swow_Buffer_getAvailableSize arginfo_class_Swow_Buffer_getSize

static PHP_METHOD(Swow_Buffer, getAvailableSize)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(buffer->size - buffer->length);
}


ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_isAvailable, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, isAvailable)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(buffer->value != NULL);
}

#define arginfo_class_Swow_Buffer_isEmpty arginfo_class_Swow_Buffer_isAvailable

static PHP_METHOD(Swow_Buffer, isEmpty)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(buffer->length == 0);
}

#define arginfo_class_Swow_Buffer_isFull arginfo_class_Swow_Buffer_isAvailable

static PHP_METHOD(Swow_Buffer, isFull)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(buffer->length == buffer->size);
}

#define arginfo_class_Swow_Buffer_realloc arginfo_class_Swow_Buffer_alloc

static PHP_METHOD(Swow_Buffer, realloc)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_long size;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(size)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(size < 0)) {
        zend_argument_value_error(1, "can not be negative");
        RETURN_THROWS();
    }

    (void) cat_buffer_realloc(buffer, size);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_extend, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, recommendSize, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, extend)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_long recommend_size = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(recommend_size)
    ZEND_PARSE_PARAMETERS_END();

    if (recommend_size == 0) {
        recommend_size = buffer->size + buffer->size;
    }
    if (UNEXPECTED(recommend_size < 0 || (size_t) recommend_size <= buffer->size)) {
        zend_argument_value_error(1, "must be greater than current buffer size");
        RETURN_THROWS();
    }

    (void) cat_buffer_extend(buffer, recommend_size);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_mallocTrim, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, mallocTrim)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = cat_buffer_malloc_trim(buffer);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_buffer_exception_ce);
        RETURN_THROWS();
    }

    /* Notice: about shared: it not works when it's shared (due to length == size) */
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_read, 0, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, start, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, read)
{
    swow_buffer_t *sbuffer = getThisBuffer();
    zend_long length = -1;
    zend_long start = 0;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(start)
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    const char *ptr = swow_buffer_get_readable_space(sbuffer, start, &length, 1);

    if (UNEXPECTED(ptr == NULL)) {
        RETURN_THROWS();
    }

    RETURN_STRINGL_FAST(ptr, length);
}

static PHP_METHOD_EX(Swow_Buffer, _write, const zend_bool append)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_string *string;
    zend_long offset = append ? buffer->length : 0;
    zend_long start = 0;
    zend_long length = -1;
    const char *ptr;

    ZEND_PARSE_PARAMETERS_START(append ? 1 : 2, append ? 3 : 4)
        if (!append) {
            Z_PARAM_LONG(offset)
        }
        Z_PARAM_STR(string)
        Z_PARAM_OPTIONAL
        /* where to start copying strings */
        Z_PARAM_LONG(start)
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    ptr = swow_string_get_readable_space(string, start, &length, append ? 1 : 2);

    // FIXME: strpos argument error is mis pos
    if (UNEXPECTED(ptr == NULL)) {
        RETURN_THROWS();
    }

    if (UNEXPECTED(length == 0)) {
        RETURN_LONG(0);
    }

    if (!append) {
        if (UNEXPECTED(offset < 0)) {
            zend_argument_value_error(1, "can not be negative");
            RETURN_THROWS();
        }
        if (UNEXPECTED(offset > buffer->length)) {
            zend_argument_value_error(1, "can not be greater than current buffer length");
            RETURN_THROWS();
        }
    }

    if (offset == 0 && start == 0 && (size_t) length == ZSTR_LEN(string) && length >= buffer->size) {
        swow_buffer_close(sbuffer);
        /* Notice: string maybe interned, so we must use zend_string_copy() here */
        buffer->value = ZSTR_VAL(zend_string_copy(string));
        buffer->size = buffer->length = ZSTR_LEN(string);
    } else {
        /* TODO: Optimized COW: it's not required to always copy string,
         * if we just write to offset which is not the end of buffer,
         * it maybe better that only copy the data that doesn't change. */
        swow_buffer_cow(sbuffer);
        (void) cat_buffer_write(buffer, offset, ptr, length);
    }

    RETURN_LONG(length);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_write, 0, 2, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, offset, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, string, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, start, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, write)
{
    PHP_METHOD_CALL(Swow_Buffer, _write, false);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_append, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, string, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, start, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, append)
{
    PHP_METHOD_CALL(Swow_Buffer, _write, true);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_truncate, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, length, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, truncate)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_long length;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(length < 0)) {
        zend_argument_value_error(1, "can not be negative");
        RETURN_THROWS();
    }

    swow_buffer_cow(sbuffer);

    cat_buffer_truncate(buffer, length);

    RETURN_LONG(buffer->length);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_truncateFrom, 0, 0, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, truncateFrom)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_long offset = 0;
    zend_long length = -1;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(offset)
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(offset < 0)) {
        zend_argument_value_error(1, "can not be negative");
        RETURN_THROWS();
    }
    if (EXPECTED(length == -1)) {
        length = ZEND_LONG_MAX;
    } else if (UNEXPECTED(length < 0)) {
        zend_argument_value_error(2, "should be greater than or equal to -1");
        RETURN_THROWS();
    }

    swow_buffer_cow(sbuffer);

    cat_buffer_truncate_from(buffer, offset, length);

    RETURN_LONG(buffer->length);
}

#define arginfo_class_Swow_Buffer_clear arginfo_class_Swow_Buffer_mallocTrim

static PHP_METHOD(Swow_Buffer, clear)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);

    ZEND_PARSE_PARAMETERS_NONE();

    swow_buffer_cow(sbuffer);

    cat_buffer_clear(buffer);
    swow_buffer_reset(sbuffer);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_fetchString, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* different from __toString , it transfers ownership to return_value */
static PHP_METHOD(Swow_Buffer, fetchString)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    char *value;

    ZEND_PARSE_PARAMETERS_NONE();

    value = cat_buffer_fetch(buffer);

    if (UNEXPECTED(value == NULL)) {
        RETURN_EMPTY_STRING();
    }

    swow_buffer_reset(sbuffer);

    RETURN_STR((zend_string *) (value - offsetof(zend_string, val)));
}

#define arginfo_class_Swow_Buffer_dupString arginfo_class_Swow_Buffer_fetchString

/* return string copy immediately */
static PHP_METHOD(Swow_Buffer, dupString)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRINGL_FAST(buffer->value, buffer->length);
}

#define arginfo_class_Swow_Buffer_toString arginfo_class_Swow_Buffer_fetchString

/* return string is just readonly (COW) */
static PHP_METHOD(Swow_Buffer, toString)
{
    zend_string *string;

    ZEND_PARSE_PARAMETERS_NONE();

    string = swow_buffer_get_string(getThisBuffer());

    if (UNEXPECTED(string == NULL)) {
        RETURN_EMPTY_STRING();
    }

    /* Notice: string maybe interned, so we must use zend_string_copy() here */
    RETURN_STR(zend_string_copy(string));
}

#define arginfo_class_Swow_Buffer_isLocked arginfo_class_Swow_Buffer_isAvailable

static PHP_METHOD(Swow_Buffer, isLocked)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(swow_buffer_get_locker(sbuffer) != NULL);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Buffer_getLocker, 0, 0, Swow\\Coroutine, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, getLocker)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    swow_coroutine_t *locker = swow_buffer_get_locker(sbuffer);

    if (UNEXPECTED(locker == NULL)) {
        RETURN_NULL();
    }

    RETVAL_OBJ_COPY(&locker->std);
}

#define arginfo_class_Swow_Buffer_tryLock arginfo_class_Swow_Buffer_isAvailable

static PHP_METHOD(Swow_Buffer, tryLock)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(swow_buffer_get_locker(sbuffer) != NULL)) {
        RETURN_FALSE;
    }

    bool ret = swow_buffer_lock(sbuffer);
    ZEND_ASSERT(ret);

    RETURN_BOOL(ret);
}

#define arginfo_class_Swow_Buffer_lock arginfo_class_Swow_Buffer_mallocTrim

static PHP_METHOD(Swow_Buffer, lock)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(!swow_buffer_lock(sbuffer))) {
        RETURN_THROWS();
    }
}

#define arginfo_class_Swow_Buffer_unlock arginfo_class_Swow_Buffer_mallocTrim

static PHP_METHOD(Swow_Buffer, unlock)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(!swow_buffer_unlock(sbuffer))) {
        RETURN_THROWS();
    }
}

#define arginfo_class_Swow_Buffer_close arginfo_class_Swow_Buffer_mallocTrim

static PHP_METHOD(Swow_Buffer, close)
{
    swow_buffer_t *sbuffer = getThisBuffer();
    SWOW_BUFFER_CHECK_LOCK(sbuffer);

    ZEND_PARSE_PARAMETERS_NONE();

    swow_buffer_close(sbuffer);
}

#define arginfo_class_Swow_Buffer___toString arginfo_class_Swow_Buffer_fetchString

#define zim_Swow_Buffer___toString zim_Swow_Buffer_toString

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, __debugInfo)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    zval zdebug_info;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(&zdebug_info);
    // TODO: support configure it to use str_quote() and maxlen
    if (buffer->value == NULL) {
        add_assoc_str(&zdebug_info, "value", zend_empty_string);
    } else {
        const int maxlen = -1;
        char *chunk = NULL;
        if (maxlen > 0 && buffer->length > (size_t) maxlen) {
            chunk = cat_sprintf("%.*s...", maxlen, buffer->value);
        }
        if (chunk != NULL) {
            add_assoc_stringl(&zdebug_info, "value", chunk, maxlen);
            cat_free(chunk);
        } else {
            zend_string *string = swow_buffer_get_string(sbuffer);
            add_assoc_str(&zdebug_info, "value", zend_string_copy(string));
        }
    }
    add_assoc_long(&zdebug_info, "size", buffer->size);
    add_assoc_long(&zdebug_info, "length", buffer->length);
    if (sbuffer->locker) {
        GC_ADDREF(&sbuffer->locker->std);
        add_assoc_object(&zdebug_info, "locker", &sbuffer->locker->std);
    }

    RETURN_DEBUG_INFO_WITH_PROPERTIES(&zdebug_info);
}

static const zend_function_entry swow_buffer_methods[] = {
    PHP_ME(Swow_Buffer, alignSize,         arginfo_class_Swow_Buffer_alignSize,         ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Buffer, __construct,       arginfo_class_Swow_Buffer___construct,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, alloc,             arginfo_class_Swow_Buffer_alloc,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, getSize,           arginfo_class_Swow_Buffer_getSize,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, getLength,         arginfo_class_Swow_Buffer_getLength,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, getAvailableSize,  arginfo_class_Swow_Buffer_getAvailableSize,  ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, isAvailable,       arginfo_class_Swow_Buffer_isAvailable,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, isEmpty,           arginfo_class_Swow_Buffer_isEmpty,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, isFull,            arginfo_class_Swow_Buffer_isFull,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, realloc,           arginfo_class_Swow_Buffer_realloc,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, extend,            arginfo_class_Swow_Buffer_extend,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, mallocTrim,        arginfo_class_Swow_Buffer_mallocTrim,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, read,              arginfo_class_Swow_Buffer_read,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, write,             arginfo_class_Swow_Buffer_write,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, append,            arginfo_class_Swow_Buffer_append,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, truncate,          arginfo_class_Swow_Buffer_truncate,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, truncateFrom,      arginfo_class_Swow_Buffer_truncateFrom,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, clear,             arginfo_class_Swow_Buffer_clear,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, fetchString,       arginfo_class_Swow_Buffer_fetchString,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, dupString,         arginfo_class_Swow_Buffer_dupString,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, toString,          arginfo_class_Swow_Buffer_toString,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, isLocked,          arginfo_class_Swow_Buffer_isLocked,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, getLocker,         arginfo_class_Swow_Buffer_getLocker,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, tryLock,           arginfo_class_Swow_Buffer_tryLock,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, lock,              arginfo_class_Swow_Buffer_lock,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, unlock,            arginfo_class_Swow_Buffer_unlock,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, close,             arginfo_class_Swow_Buffer_close,             ZEND_ACC_PUBLIC)
    /* magic */
    PHP_ME(Swow_Buffer, __toString,        arginfo_class_Swow_Buffer___toString,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, __debugInfo,       arginfo_class_Swow_Buffer___debugInfo,       ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static zend_object *swow_buffer_clone_object(zend_object *object)
{
    swow_buffer_t *sbuffer, *new_sbuffer;
    zend_string *string;

    sbuffer = swow_buffer_get_from_object(object);
    string = swow_buffer_get_string(sbuffer);
    new_sbuffer = swow_buffer_get_from_object(swow_object_create(sbuffer->std.ce));
    memcpy(new_sbuffer, sbuffer, offsetof(swow_buffer_t, std));
    if (string != NULL) {
        /* Notice: string maybe interned, so we must use zend_string_copy() here */
        new_sbuffer->buffer.value = ZSTR_VAL(zend_string_copy(string));
    }

    zend_objects_clone_members(&new_sbuffer->std, object);

    return &new_sbuffer->std;
}

static char *swow_buffer_alloc_standard(size_t size)
{
    zend_string *string = zend_string_alloc(size, 0);

    ZSTR_VAL(string)[ZSTR_LEN(string) = 0] = '\0';

    return ZSTR_VAL(string);
}

static char *swow_buffer_realloc_standard(char *old_value, size_t new_size)
{
    zend_string *old_string = old_value != NULL ? swow_buffer_get_string_from_value(old_value) : NULL;
    zend_string *new_string;
    size_t old_length = old_string != NULL ? ZSTR_LEN(old_string) : 0;
    size_t new_length = MIN(old_length, new_size);
    zend_bool do_erealloc;

    if (
        /* zend string do not support realloc from NULL */
        old_string != NULL &&
        /* realloc on persistent or interned, it means buffer is shared,
         * we should allocate a new non-persistent string */
        !(GC_FLAGS(old_string) & IS_STR_PERSISTENT) &&
        !ZSTR_IS_INTERNED(old_string) &&
        /* COW can be skipped */
        GC_REFCOUNT(old_string) == 1
    ) {
        do_erealloc = true;
    } else {
        do_erealloc = false;
    }

    if (do_erealloc) {
        new_string = (zend_string *) erealloc(old_string, ZEND_MM_ALIGNED_SIZE(_ZSTR_STRUCT_SIZE(new_size)));
        zend_string_forget_hash_val(new_string);
    } else {
        new_string = zend_string_alloc(new_size, false);
        memcpy(ZSTR_VAL(new_string), ZSTR_VAL(old_string), new_length);
        if (old_string != NULL && !ZSTR_IS_INTERNED(old_string)) {
            GC_DELREF(old_string);
        }
    }
    ZSTR_VAL(new_string)[ZSTR_LEN(new_string) = new_length] = '\0';

    return ZSTR_VAL(new_string);
}

static void swow_buffer_update_standard(char *value, size_t new_length)
{
    zend_string *string = swow_buffer_get_string_from_value(value);

    ZSTR_VAL(string)[ZSTR_LEN(string) = new_length] = '\0';
}

static void swow_buffer_free_standard(char *value)
{
    /* Notice: string maybe interned or persistent, so we should use zend_string_release() here */
    zend_string_release(swow_buffer_get_string_from_value(value));
}

SWOW_API const cat_buffer_allocator_t swow_buffer_allocator = {
    swow_buffer_alloc_standard,
    swow_buffer_realloc_standard,
    swow_buffer_update_standard,
    swow_buffer_free_standard,
};

zend_result swow_buffer_module_init(INIT_FUNC_ARGS)
{
    if (unlikely(!cat_buffer_module_init())) {
        return FAILURE;
    }

    if (unlikely(!cat_buffer_register_allocator(&swow_buffer_allocator))) {
        return FAILURE;
    }

    swow_buffer_ce = swow_register_internal_class(
        "Swow\\Buffer", NULL, swow_buffer_methods,
        &swow_buffer_handlers, NULL,
        cat_true, cat_false,
        swow_buffer_create_object,
        swow_buffer_free_object,
        XtOffsetOf(swow_buffer_t, std)
    );
    swow_buffer_handlers.clone_obj = swow_buffer_clone_object;

    zend_declare_class_constant_long(swow_buffer_ce, ZEND_STRL("PAGE_SIZE"), cat_getpagesize());
    zend_declare_class_constant_long(swow_buffer_ce, ZEND_STRL("COMMON_SIZE"), CAT_BUFFER_COMMON_SIZE);

    swow_buffer_exception_ce = swow_register_internal_class(
        "Swow\\BufferException", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}
