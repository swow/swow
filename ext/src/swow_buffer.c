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

#define CAT_BUFFER_GETTER(sbuffer, buffer) \
    cat_buffer_t *buffer = &sbuffer->buffer

#define CAT_BUFFER_GETTER_NOT_EMPTY(sbuffer, buffer, failure) \
    CAT_BUFFER_GETTER(sbuffer, buffer); do { \
    if (UNEXPECTED(buffer->value == NULL)) { \
        failure; \
    } \
} while (0)

static zend_always_inline zend_string *swow_buffer_get_string_from_value(char *value)
{
    return (zend_string *) (value - offsetof(zend_string, val));
}

static zend_always_inline zend_string *swow_buffer_get_string_from_handle(cat_buffer_t *buffer) SWOW_UNSAFE
{
    return swow_buffer_get_string_from_value(buffer->value);
}

SWOW_API zend_string *swow_buffer_fetch_string(swow_buffer_t *sbuffer)
{
    CAT_BUFFER_GETTER_NOT_EMPTY(sbuffer, buffer, return NULL);

    return swow_buffer_get_string_from_handle(buffer);
}

SWOW_API size_t swow_buffer_get_readable_space(swow_buffer_t *sbuffer, const char **ptr)
{
    CAT_BUFFER_GETTER_NOT_EMPTY(sbuffer, buffer, return 0);

    *ptr = buffer->value + sbuffer->offset;

    return buffer->length - sbuffer->offset;
}

SWOW_API size_t swow_buffer_get_writable_space(swow_buffer_t *sbuffer, char **ptr)
{
    CAT_BUFFER_GETTER_NOT_EMPTY(sbuffer, buffer, return 0);

    *ptr = buffer->value + sbuffer->offset;

    return buffer->size - sbuffer->offset;
}

SWOW_API void swow_buffer_virtual_read(swow_buffer_t *sbuffer, size_t length)
{
    sbuffer->offset += length;
}

static zend_always_inline void swow_buffer__virtual_write(swow_buffer_t *sbuffer, size_t length)
{
    CAT_BUFFER_GETTER(sbuffer, buffer);
    zend_string *string = swow_buffer_get_string_from_handle(buffer);
    size_t new_length = sbuffer->offset + length;

    if (EXPECTED(new_length > buffer->length)) {
        ZSTR_VAL(string)[ZSTR_LEN(string) = (buffer->length = new_length)] = '\0';
    }
}

SWOW_API void swow_buffer_virtual_write(swow_buffer_t *sbuffer, size_t length)
{
    swow_buffer__virtual_write(sbuffer, length);
    sbuffer->offset += length;
}

SWOW_API void swow_buffer_virtual_write_no_seek(swow_buffer_t *sbuffer, size_t length)
{
    swow_buffer__virtual_write(sbuffer, length);
}

static zend_always_inline void swow_buffer_reset(swow_buffer_t *sbuffer)
{
    sbuffer->offset = 0;
    sbuffer->locked = cat_false;
    sbuffer->user_locked = cat_false;
    sbuffer->shared = cat_false;
}

static zend_always_inline void swow_buffer_init(swow_buffer_t *sbuffer)
{
    cat_buffer_init(&sbuffer->buffer);
    swow_buffer_reset(sbuffer);
}

static zend_object *swow_buffer_create_object(zend_class_entry *ce)
{
    swow_buffer_t *sbuffer = swow_object_alloc(swow_buffer_t, ce, swow_buffer_handlers);

    swow_buffer_init(sbuffer);

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
    CAT_BUFFER_GETTER(_sbuffer, _buffer)

static zend_never_inline void swow_buffer_separate(swow_buffer_t *sbuffer)
{
    cat_buffer_t new_buffer;
    cat_buffer_dup(&sbuffer->buffer, &new_buffer);
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
        sbuffer->shared = cat_false;
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

static PHP_METHOD_EX(Swow_Buffer, create, zend_bool return_this)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    zend_long size = CAT_BUFFER_DEFAULT_SIZE;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
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

    if (return_this) {
        RETURN_THIS();
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Buffer___construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "Swow\\Buffer::DEFAULT_SIZE")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, __construct)
{
    PHP_METHOD_CALL(Swow_Buffer, create, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_alloc, 0, 0, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "Swow\\Buffer::DEFAULT_SIZE")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, alloc)
{
    PHP_METHOD_CALL(Swow_Buffer, create, 1);
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

#define arginfo_class_Swow_Buffer_getReadableLength arginfo_class_Swow_Buffer_getSize

static PHP_METHOD(Swow_Buffer, getReadableLength)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(buffer->length - sbuffer->offset);
}

#define arginfo_class_Swow_Buffer_getWritableSize arginfo_class_Swow_Buffer_getSize

static PHP_METHOD(Swow_Buffer, getWritableSize)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(buffer->size - sbuffer->offset);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_isReadable, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, isReadable)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(!sbuffer->locked);
}

#define arginfo_class_Swow_Buffer_isWritable arginfo_class_Swow_Buffer_isReadable

static PHP_METHOD(Swow_Buffer, isWritable)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(!sbuffer->locked);
}

#define arginfo_class_Swow_Buffer_isSeekable arginfo_class_Swow_Buffer_isReadable

static PHP_METHOD(Swow_Buffer, isSeekable)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(!sbuffer->locked);
}

#define arginfo_class_Swow_Buffer_isAvailable arginfo_class_Swow_Buffer_isReadable

static PHP_METHOD(Swow_Buffer, isAvailable)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(buffer->value != NULL);
}

#define arginfo_class_Swow_Buffer_isEmpty arginfo_class_Swow_Buffer_isReadable

static PHP_METHOD(Swow_Buffer, isEmpty)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(buffer->length == 0);
}

#define arginfo_class_Swow_Buffer_isFull arginfo_class_Swow_Buffer_isReadable

static PHP_METHOD(Swow_Buffer, isFull)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(buffer->length == buffer->size);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_realloc, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, newSize, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, realloc)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_long new_size;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(new_size)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(new_size < 0)) {
        zend_argument_value_error(1, "can not be negative");
        RETURN_THROWS();
    }

    do {
        const char *old_buffer_value = buffer->value;

        (void) cat_buffer_realloc(buffer, new_size);

        if (buffer->value != old_buffer_value) {
            sbuffer->shared = cat_false;
        }
    } while (0);

    /* realloc may lead offset to overflow if new_size is too small */
    if (UNEXPECTED(sbuffer->offset > buffer->length)) {
        sbuffer->offset = buffer->length;
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_extend, 0, 0, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, recommendSize, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, extend)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_long recommend_size = buffer->size + buffer->size;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(recommend_size)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(recommend_size < 0 || (size_t) recommend_size <= buffer->size)) {
        zend_argument_value_error(1, "must be greater than current buffer size");
        RETURN_THROWS();
    }

    (void) cat_buffer_extend(buffer, recommend_size);

    /* buffer value always changed after extended */
    sbuffer->shared = cat_false;

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_mallocTrim, 0, 0, IS_STATIC, 0)
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

    RETURN_THIS();
}

#define arginfo_class_Swow_Buffer_tell arginfo_class_Swow_Buffer_getSize

static PHP_METHOD(Swow_Buffer, tell)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(sbuffer->offset);
}

#define arginfo_class_Swow_Buffer_rewind arginfo_class_Swow_Buffer_mallocTrim

static PHP_METHOD(Swow_Buffer, rewind)
{
    swow_buffer_t *sbuffer = getThisBuffer();
    SWOW_BUFFER_CHECK_LOCK(sbuffer);

    ZEND_PARSE_PARAMETERS_NONE();

    sbuffer->offset = 0;

    RETURN_THIS();
}

#define arginfo_class_Swow_Buffer_eof arginfo_class_Swow_Buffer_isReadable

static PHP_METHOD(Swow_Buffer, eof)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(sbuffer->offset == buffer->length);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_seek, 0, 1, IS_STATIC, 0)
    ZEND_ARG_INFO(0, offset)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, whence, "SEEK_SET")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, seek)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);

    zend_long offset;
    zend_long whence = SEEK_SET;
    char *ptr;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(offset)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(whence)
    ZEND_PARSE_PARAMETERS_END();

    switch (whence) {
        case SEEK_SET:
            ptr = buffer->value;
            break;
        case SEEK_CUR:
            ptr = buffer->value + sbuffer->offset;
            break;
        case SEEK_END:
            ptr = buffer->value + buffer->length;
            break;
        default:
            zend_argument_value_error(2, "is unknown (" ZEND_LONG_FMT ")", whence);
            RETURN_THROWS();
    }
    ptr += offset;
    if (UNEXPECTED((ptr < buffer->value) || (ptr > (buffer->value + buffer->length)))) {
        swow_throw_exception(swow_buffer_exception_ce, CAT_EINVAL, "Offset overflow");
        RETURN_THROWS();
    }
    sbuffer->offset = ptr - buffer->value;

    RETURN_THIS();
}

typedef enum {
    SWOW_BUFFER_READ      = 1 << 0,
    SWOW_BUFFER_PEEK      = 1 << 1,
    SWOW_BUFFER_PEEK_FROM = 1 << 2 | SWOW_BUFFER_PEEK
} swow_buffer_read_type_t;

static PHP_METHOD_EX(Swow_Buffer, _read, swow_buffer_read_type_t type)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK_IF(sbuffer, !(type & SWOW_BUFFER_PEEK));
    zend_long length = -1;
    zend_long offset = sbuffer->offset;

    ZEND_PARSE_PARAMETERS_START(0, type != SWOW_BUFFER_PEEK_FROM ? 1 : 2)
        Z_PARAM_OPTIONAL
        if (type == SWOW_BUFFER_PEEK_FROM) {
            Z_PARAM_LONG(offset)
        }
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    if (type == SWOW_BUFFER_PEEK_FROM) {
        if (UNEXPECTED(offset < 0)) {
            zend_argument_value_error(3, "can not be negative");
            RETURN_THROWS();
        }
        if (UNEXPECTED((size_t) offset > buffer->length)) {
            swow_throw_exception(swow_buffer_exception_ce, CAT_EINVAL, "Offset overflow");
            RETURN_THROWS();
        }
    }
    if (UNEXPECTED(length < -1)) {
        zend_argument_value_error(type != SWOW_BUFFER_PEEK_FROM ? 2 : 3, "should be greater than or equal to -1");
        RETURN_THROWS();
    }
    if (length == -1) {
        length = buffer->length - offset;
    } else if (UNEXPECTED((size_t) length > buffer->length - ((size_t) offset))) {
        length = buffer->length - offset;
    }
    if (!(type & SWOW_BUFFER_PEEK)) {
        sbuffer->offset += length;
    }

    RETURN_STRINGL_FAST(buffer->value + offset, length);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_read, 0, 0, IS_STRING, 0)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, length, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, read)
{
    PHP_METHOD_CALL(Swow_Buffer, _read, SWOW_BUFFER_READ);
}

#define arginfo_class_Swow_Buffer_peek arginfo_class_Swow_Buffer_read

static PHP_METHOD(Swow_Buffer, peek)
{
    PHP_METHOD_CALL(Swow_Buffer, _read, SWOW_BUFFER_PEEK);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_peekFrom, 0, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, peekFrom)
{
    PHP_METHOD_CALL(Swow_Buffer, _read, SWOW_BUFFER_PEEK_FROM);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_getContents, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, getContents)
{
    ZEND_PARSE_PARAMETERS_NONE();

    PHP_METHOD_CALL(Swow_Buffer, _read, SWOW_BUFFER_READ);
}

static PHP_METHOD_EX(Swow_Buffer, _write, zend_bool no_seek)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_string *string;
    zend_long offset = 0;
    zend_long length = -1;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_STR(string)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(offset)
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    SWOW_BUFFER_CHECK_STRING_SCOPE(string, offset, length);

    if (UNEXPECTED(length == 0)) {
        RETURN_THIS();
    }

    if (buffer->value == NULL && offset == 0 && (size_t) length == ZSTR_LEN(string)) {
        ZEND_ASSERT(sbuffer->offset == 0);
        /* Notice: string maybe interned, so we must use zend_string_addref() here */
        zend_string_addref(string);
        buffer->value = ZSTR_VAL(string);
        buffer->size = buffer->length = ZSTR_LEN(string);
        sbuffer->shared = cat_true;
    } else {
        swow_buffer_cow(sbuffer);
        (void) cat_buffer_write(buffer, sbuffer->offset, ZSTR_VAL(string) + offset, length);
    }
    if (!no_seek) {
        sbuffer->offset += length;
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_write, 0, 1, IS_STATIC, 0)
    ZEND_ARG_INFO(0, string)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, write)
{
    PHP_METHOD_CALL(Swow_Buffer, _write, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_copy, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, string, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, copy)
{
    PHP_METHOD_CALL(Swow_Buffer, _write, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_truncate, 0, 0, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, truncate)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_long length = -1;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    if (EXPECTED(length == -1)) {
        length = sbuffer->offset;
    } else if (UNEXPECTED(length < 0)) {
        zend_argument_value_error(1, "should be greater than or equal to -1");
        RETURN_THROWS();
    }

    swow_buffer_cow(sbuffer);

    cat_buffer_truncate(buffer, length);

    if (sbuffer->offset > buffer->length) {
        /* offset right overflow, reset to eof */
        sbuffer->offset = buffer->length;
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_truncateFrom, 0, 0, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, truncateFrom)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_long offset = sbuffer->offset;
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

    if (sbuffer->offset < (size_t) offset) {
        /* offset target has been removed, reset to zero */
        sbuffer->offset = 0;
    } else if (sbuffer->offset > (size_t) (offset + buffer->length)) {
        /* offset right overflow, reset to eof */
        sbuffer->offset = buffer->length;
    } else {
        /* offset should be reset to the same position of data */
        sbuffer->offset -= offset;
    }

    RETURN_THIS();
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

    RETURN_THIS();
}

#define arginfo_class_Swow_Buffer_fetchString arginfo_class_Swow_Buffer_getContents

/* diffrent from __toString , it transfers ownership to return_value */
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

#define arginfo_class_Swow_Buffer_dupString arginfo_class_Swow_Buffer_getContents

/* return string copy immediately */
static PHP_METHOD(Swow_Buffer, dupString)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRINGL_FAST(buffer->value, buffer->length);
}

#define arginfo_class_Swow_Buffer_toString arginfo_class_Swow_Buffer_getContents

/* return string is just readonly (COW) */
static PHP_METHOD(Swow_Buffer, toString)
{
    zend_string *string;

    ZEND_PARSE_PARAMETERS_NONE();

    string = swow_buffer_fetch_string(getThisBuffer());

    if (UNEXPECTED(string == NULL)) {
        RETURN_EMPTY_STRING();
    }

    /* Notice: string maybe interned, so we must use zend_string_copy() here */
    RETURN_STR(zend_string_copy(string));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer_lock, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, lock)
{
    swow_buffer_t *sbuffer = getThisBuffer();
    SWOW_BUFFER_CHECK_LOCK(sbuffer);

    ZEND_PARSE_PARAMETERS_NONE();

    sbuffer->user_locked = cat_true;
}

#define arginfo_class_Swow_Buffer_tryLock arginfo_class_Swow_Buffer_isReadable

static PHP_METHOD(Swow_Buffer, tryLock)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(swow_buffer_is_locked(sbuffer))) {
        RETURN_FALSE;
    }

    sbuffer->user_locked = cat_true;
    RETURN_TRUE;
}

#define arginfo_class_Swow_Buffer_unlock arginfo_class_Swow_Buffer_lock

static PHP_METHOD(Swow_Buffer, unlock)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    sbuffer->user_locked = cat_false;
}

#define arginfo_class_Swow_Buffer_close arginfo_class_Swow_Buffer_lock

static PHP_METHOD(Swow_Buffer, close)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);

    ZEND_PARSE_PARAMETERS_NONE();

    cat_buffer_close(buffer);
    swow_buffer_reset(sbuffer);
}

#define arginfo_class_Swow_Buffer___toString arginfo_class_Swow_Buffer_getContents

#define zim_Swow_Buffer___toString zim_Swow_Buffer_toString

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Buffer___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Buffer, __debugInfo)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    zval zdebug_info;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(&zdebug_info);
    if (buffer->value == NULL) {
        add_assoc_str(&zdebug_info, "value", zend_empty_string);
    } else {
        const int maxlen = -1;
        char *chunk = NULL;
        if (maxlen > 0 && buffer->length > (size_t) maxlen) {
            chunk = cat_sprintf("%.*s...", maxlen, buffer->value);
        }
        if (chunk != NULL) {
            add_assoc_string(&zdebug_info, "value", chunk);
            cat_free(chunk);
        } else {
            zend_string *string = swow_buffer_fetch_string(sbuffer);
            add_assoc_str(&zdebug_info, "value", zend_string_copy(string));
        }
    }
    add_assoc_long(&zdebug_info, "size", buffer->size);
    add_assoc_long(&zdebug_info, "length", buffer->length);
    add_assoc_long(&zdebug_info, "offset", sbuffer->offset);
    if (sbuffer->locked) {
        add_assoc_bool(&zdebug_info, "locked", sbuffer->locked);
    }
    if (sbuffer->shared) {
        add_assoc_bool(&zdebug_info, "shared", sbuffer->shared);
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
    PHP_ME(Swow_Buffer, getReadableLength, arginfo_class_Swow_Buffer_getReadableLength, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, getWritableSize,   arginfo_class_Swow_Buffer_getWritableSize,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, isReadable,        arginfo_class_Swow_Buffer_isReadable,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, isWritable,        arginfo_class_Swow_Buffer_isWritable,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, isSeekable,        arginfo_class_Swow_Buffer_isSeekable,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, isAvailable,       arginfo_class_Swow_Buffer_isAvailable,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, isEmpty,           arginfo_class_Swow_Buffer_isEmpty,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, isFull,            arginfo_class_Swow_Buffer_isFull,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, realloc,           arginfo_class_Swow_Buffer_realloc,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, extend,            arginfo_class_Swow_Buffer_extend,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, mallocTrim,        arginfo_class_Swow_Buffer_mallocTrim,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, tell,              arginfo_class_Swow_Buffer_tell,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, rewind,            arginfo_class_Swow_Buffer_rewind,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, eof,               arginfo_class_Swow_Buffer_eof,               ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, seek,              arginfo_class_Swow_Buffer_seek,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, read,              arginfo_class_Swow_Buffer_read,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, peek,              arginfo_class_Swow_Buffer_peek,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, peekFrom,          arginfo_class_Swow_Buffer_peekFrom,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, getContents,       arginfo_class_Swow_Buffer_getContents,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, write,             arginfo_class_Swow_Buffer_write,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, copy,              arginfo_class_Swow_Buffer_copy,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, truncate,          arginfo_class_Swow_Buffer_truncate,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, truncateFrom,      arginfo_class_Swow_Buffer_truncateFrom,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, clear,             arginfo_class_Swow_Buffer_clear,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, fetchString,       arginfo_class_Swow_Buffer_fetchString,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, dupString,         arginfo_class_Swow_Buffer_dupString,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, toString,          arginfo_class_Swow_Buffer_toString,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, lock,              arginfo_class_Swow_Buffer_lock,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Buffer, tryLock,           arginfo_class_Swow_Buffer_tryLock,           ZEND_ACC_PUBLIC)
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
    zend_string *string, *new_string;

    sbuffer = swow_buffer_get_from_object(object);
    string = swow_buffer_fetch_string(sbuffer);
    new_sbuffer = swow_buffer_get_from_object(swow_object_create(sbuffer->std.ce));
    memcpy(new_sbuffer, sbuffer, offsetof(swow_buffer_t, std));
    if (string != NULL) {
        new_string = zend_string_alloc(sbuffer->buffer.size, 0);
        memcpy(ZSTR_VAL(new_string), ZSTR_VAL(string), ZSTR_LEN(string));
        ZSTR_VAL(new_string)[ZSTR_LEN(new_string) = ZSTR_LEN(string)] = '\0';
        new_sbuffer->buffer.value = ZSTR_VAL(new_string);
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

static char *swow_buffer_realloc_standard(char *old_value, size_t old_length, size_t new_size)
{
    zend_string *new_string;

    new_string = zend_string_alloc(new_size, 0);
    if (old_value != NULL) {
        zend_string *old_string = swow_buffer_get_string_from_value(old_value);
        if (old_length > 0) {
            if (unlikely(new_size < old_length)) {
                old_length = new_size;
            }
            memcpy(ZSTR_VAL(new_string), ZSTR_VAL(old_string), old_length);
            ZSTR_VAL(new_string)[ZSTR_LEN(new_string) = old_length] = '\0';
        }
        /* Notice: string maybe interned or persistent, so we should use zend_string_release() here */
        zend_string_release(old_string);
    }

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
    zend_declare_class_constant_long(swow_buffer_ce, ZEND_STRL("DEFAULT_SIZE"), CAT_BUFFER_DEFAULT_SIZE);

    swow_buffer_exception_ce = swow_register_internal_class(
        "Swow\\BufferException", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}
