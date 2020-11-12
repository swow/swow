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

static cat_always_inline zend_string *swow_buffer_get_string_from_value(char *value)
{
    return (zend_string *) (value - offsetof(zend_string, val));
}

static cat_always_inline zend_string *swow_buffer_get_string_from_handle(cat_buffer_t *buffer) SWOW_UNSAFE
{
    return swow_buffer_get_string_from_value(buffer->value);
}

static cat_always_inline void swow_buffer_string_addref(zend_string *string)
{
    /* Notice: if string may be interned, then we should use zend_string_addref instead of GC_ADDREF */
    CAT_ASSERT(!ZSTR_IS_INTERNED(string));
    GC_ADDREF(string);
}

static cat_always_inline zend_string* swow_buffer_string_copy(zend_string *string)
{
    swow_buffer_string_addref(string);
    return string;
}

static cat_always_inline void swow_buffer_string_release(zend_string *string)
{
    /* Notice: if string may be interned or persistent, then we should use zend_string_release_ex(string, persistent) instead */
    CAT_ASSERT(!ZSTR_IS_INTERNED(string));
    if (GC_DELREF(string) == 0) {
        efree(string);
    }
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

static cat_always_inline void swow_buffer__virtual_write(swow_buffer_t *sbuffer, size_t length)
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

static cat_always_inline void swow_buffer_init(swow_buffer_t *sbuffer)
{
    sbuffer->offset = 0;
    sbuffer->locked = cat_false;
    sbuffer->shared = cat_false;
}

static zend_object *swow_buffer_create_object(zend_class_entry *ce)
{
    swow_buffer_t *sbuffer = swow_object_alloc(swow_buffer_t, ce, swow_buffer_handlers);

    cat_buffer_init(&sbuffer->buffer);
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

#define SWOW_BUFFER_CAN_BE_SHARED(sbuffer, buffer, string, _offset, length) \
    ((buffer)->value == NULL && sbuffer->offset == 0 && _offset == 0 && (size_t) length == ZSTR_LEN(string))

#define SWOW_BUFFER_SHARED(sbuffer) do { \
    (sbuffer)->shared = cat_true; \
} while (0)

#define SWOW_BUFFER_UNSHARED(sbuffer) do { \
    (sbuffer)->shared = cat_false; \
} while (0)

static void swow_buffer_separate_by_handle(cat_buffer_t *buffer)
{
    zend_string *string = swow_buffer_get_string_from_handle(buffer);

    if (GC_REFCOUNT(string) != 1 || ZSTR_IS_INTERNED(string) || (GC_FLAGS(string) & IS_STR_PERSISTENT)) {
        cat_buffer_t new_buffer;
        cat_buffer_dup(buffer, &new_buffer);
        cat_buffer_close(buffer);
        *buffer = new_buffer;
    }
}

#define SWOW_BUFFER_TRY_UNSHARED(sbuffer, buffer) do { \
    if (UNEXPECTED((sbuffer)->shared)) { \
        swow_buffer_separate_by_handle(buffer); \
        SWOW_BUFFER_UNSHARED(sbuffer); \
    } \
} while (0)

#ifdef CAT_DEBUG
#define SWOW_BUFFER_UNSHARED_START(sbuffer, buffer) do { \
    const char *__old_value = (buffer)->value; (void) __old_value;
#else
#define SWOW_BUFFER_UNSHARED_START(_sbuffer, _buffer)
#endif

#ifdef CAT_DEBUG
#define SWOW_BUFFER_UNSHARED_END(sbuffer, buffer) \
        CAT_ASSERT(((buffer)->value != __old_value) && "expect buffer always change here"); \
        SWOW_BUFFER_UNSHARED(sbuffer); \
} while (0)
#else
#define SWOW_BUFFER_UNSHARED_END(sbuffer, buffer) \
        SWOW_BUFFER_UNSHARED(sbuffer)
#endif

#define SWOW_BUFFER_UNSHARED_CHECK_START(_sbuffer, _buffer) do { \
    const char *__old_value = _buffer->value;

#define SWOW_BUFFER_UNSHARED_CHECK_END(_sbuffer, _buffer) \
    if (_buffer->value != __old_value) { \
        SWOW_BUFFER_UNSHARED(_sbuffer); \
    } \
} while (0)

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_buffer_alignSize, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, alignment, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, alignSize)
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

static PHP_METHOD_EX(swow_buffer, create, zend_bool return_this)
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

ZEND_BEGIN_ARG_INFO_EX(arginfo_swow_buffer___construct, 0, ZEND_RETURN_VALUE, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "Swow\\Buffer::DEFAULT_SIZE")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, __construct)
{
    PHP_METHOD_CALL(swow_buffer, create, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_buffer_alloc, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "Swow\\Buffer::DEFAULT_SIZE")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, alloc)
{
    PHP_METHOD_CALL(swow_buffer, create, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_buffer_getSize, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, getSize)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(buffer->size);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_buffer_getLong, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_buffer_getLength arginfo_swow_buffer_getLong

static PHP_METHOD(swow_buffer, getLength)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(buffer->length);
}

#define arginfo_swow_buffer_getAvailableSize arginfo_swow_buffer_getLong

static PHP_METHOD(swow_buffer, getAvailableSize)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(buffer->size - buffer->length);
}

#define arginfo_swow_buffer_getReadableLength arginfo_swow_buffer_getLong

static PHP_METHOD(swow_buffer, getReadableLength)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(buffer->length - sbuffer->offset);
}

#define arginfo_swow_buffer_getWritableSize arginfo_swow_buffer_getLong

static PHP_METHOD(swow_buffer, getWritableSize)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(buffer->size - sbuffer->offset);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_buffer_getBool, ZEND_RETURN_VALUE, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_buffer_isReadable arginfo_swow_buffer_getBool

static PHP_METHOD(swow_buffer, isReadable)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(!sbuffer->locked);
}

#define arginfo_swow_buffer_isWritable arginfo_swow_buffer_getBool

static PHP_METHOD(swow_buffer, isWritable)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(!sbuffer->locked);
}

#define arginfo_swow_buffer_isSeekable arginfo_swow_buffer_getBool

static PHP_METHOD(swow_buffer, isSeekable)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(!sbuffer->locked);
}

#define arginfo_swow_buffer_isAvailable arginfo_swow_buffer_getBool

static PHP_METHOD(swow_buffer, isAvailable)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(buffer->value != NULL);
}

#define arginfo_swow_buffer_isEmpty arginfo_swow_buffer_getBool

static PHP_METHOD(swow_buffer, isEmpty)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(buffer->length == 0);
}

#define arginfo_swow_buffer_isFull arginfo_swow_buffer_getBool

static PHP_METHOD(swow_buffer, isFull)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(buffer->length == buffer->size);
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_buffer_realloc, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, newSize, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, realloc)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_long new_size = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(new_size)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(new_size < 0)) {
        zend_argument_value_error(1, "can not be negative");
        RETURN_THROWS();
    }

    SWOW_BUFFER_UNSHARED_CHECK_START(sbuffer, buffer) {
        cat_bool_t ret;

        ret = cat_buffer_realloc(buffer, new_size);

        if (UNEXPECTED(!ret)) {
            swow_throw_exception_with_last(swow_buffer_exception_ce);
            RETURN_THROWS();
        }
    } SWOW_BUFFER_UNSHARED_CHECK_END(sbuffer, buffer);

    /* shrink lead to overflow */
    if (UNEXPECTED(sbuffer->offset > buffer->length)) {
        sbuffer->offset = buffer->length;
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_buffer_extend, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, recommendSize, IS_LONG, 0, "\'$this->getSize() * 2\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, extend)
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

    /* if extend success, buffer value always changed */
    SWOW_BUFFER_UNSHARED_START(sbuffer, buffer) {
        cat_bool_t ret;

        ret = cat_buffer_extend(buffer, recommend_size);

        if (UNEXPECTED(!ret)) {
            swow_throw_exception_with_last(swow_buffer_exception_ce);
            RETURN_THROWS();
        }
    } SWOW_BUFFER_UNSHARED_END(sbuffer, buffer);

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_buffer_mallocTrim, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, mallocTrim)
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_buffer_tell, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, tell)
{
    swow_buffer_t *sbuffer = getThisBuffer();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(sbuffer->offset);
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_buffer_rewind, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, rewind)
{
    swow_buffer_t *sbuffer = getThisBuffer();
    SWOW_BUFFER_CHECK_LOCK(sbuffer);

    ZEND_PARSE_PARAMETERS_NONE();

    sbuffer->offset = 0;

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_buffer_eof, ZEND_RETURN_VALUE, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, eof)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(sbuffer->offset == buffer->length);
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_buffer_seek, 1)
    ZEND_ARG_INFO(0, offset)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, whence, "SEEK_SET")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, seek)
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
        swow_throw_exception(swow_buffer_exception_ce, CAT_EINVAL, "Offset would be a value which cannot be represented correctly");
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

static PHP_METHOD_EX(swow_buffer, _read, swow_buffer_read_type_t type)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK_IF(sbuffer, !(type & SWOW_BUFFER_PEEK));
    zend_long length = 0;
    zend_long offset = sbuffer->offset;

    ZEND_PARSE_PARAMETERS_START(0, type != SWOW_BUFFER_PEEK_FROM ? 1 : 2)
        Z_PARAM_OPTIONAL
        if (type == SWOW_BUFFER_PEEK_FROM) {
            Z_PARAM_LONG(offset)
        }
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(length < 0)) {
        zend_argument_value_error(type != SWOW_BUFFER_PEEK_FROM ? 2 : 3, "can not be negative");
        RETURN_THROWS();
    }
    if (type == SWOW_BUFFER_PEEK_FROM) {
        if (UNEXPECTED(offset < 0)) {
            zend_argument_value_error(3, "can not be negative");
            RETURN_THROWS();
        }
        if (UNEXPECTED((size_t) offset > buffer->length)) {
            swow_throw_exception(swow_buffer_exception_ce, CAT_EINVAL, "Offset would be a value which cannot be represented correctly");
            RETURN_THROWS();
        }
    }

    if (length == 0) {
        length = buffer->length - offset;
    } else if (UNEXPECTED((size_t) offset + length > buffer->length)) {
        length = buffer->length - offset;
    }
    if (!(type & SWOW_BUFFER_PEEK)) {
        sbuffer->offset += length;
    }

    RETURN_STRINGL_FAST(buffer->value + offset, length);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_buffer_read, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, length, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, read)
{
    PHP_METHOD_CALL(swow_buffer, _read, SWOW_BUFFER_READ);
}

#define arginfo_swow_buffer_peek arginfo_swow_buffer_read

static PHP_METHOD(swow_buffer, peek)
{
    PHP_METHOD_CALL(swow_buffer, _read, SWOW_BUFFER_PEEK);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_buffer_peekFrom, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "\'$this->getOffset()\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, peekFrom)
{
    PHP_METHOD_CALL(swow_buffer, _read, SWOW_BUFFER_PEEK_FROM);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_buffer_getContents, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, getContents)
{
    ZEND_PARSE_PARAMETERS_NONE();

    PHP_METHOD_CALL(swow_buffer, _read, SWOW_BUFFER_READ);
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_buffer_write, 1)
    ZEND_ARG_INFO(0, string)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, write)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_string *string;
    zend_long offset = 0;
    zend_long length = 0;

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

    if (SWOW_BUFFER_CAN_BE_SHARED(sbuffer, buffer, string, offset, length)) {
        /* Notice: string may be interned, so we must use zend_string_addref instead of GC_ADDREF */
        swow_buffer_string_addref(string);
        buffer->value = ZSTR_VAL(string);
        buffer->size = buffer->length = ZSTR_LEN(string);
        SWOW_BUFFER_SHARED(sbuffer);
    } else {
        cat_bool_t ret;

        SWOW_BUFFER_TRY_UNSHARED(sbuffer, buffer);

        ret = cat_buffer_write(buffer, sbuffer->offset, ZSTR_VAL(string) + offset, length);

        if (UNEXPECTED(!ret)) {
            swow_throw_exception_with_last(swow_buffer_exception_ce);
            RETURN_THROWS();
        }
    }

    sbuffer->offset += length;

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_buffer_truncate, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, start, IS_LONG, 0, "\'$this->getOffset()\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, truncate)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    zend_long start = sbuffer->offset;
    zend_long length = 0;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(start)
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(start < 0)) {
        zend_argument_value_error(1, "can not be negative");
        RETURN_THROWS();
    }
    if (UNEXPECTED(length < 0)) {
        zend_argument_value_error(2, "can not be negative");
        RETURN_THROWS();
    }

    SWOW_BUFFER_TRY_UNSHARED(sbuffer, buffer);

    cat_buffer_truncate(buffer, start, length);
    sbuffer->offset = 0;

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_swow_buffer_clear, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, clear)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);

    ZEND_PARSE_PARAMETERS_NONE();

    SWOW_BUFFER_TRY_UNSHARED(sbuffer, buffer);

    cat_buffer_clear(buffer);
    sbuffer->offset = 0;

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_buffer_getString, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_buffer_fetchString arginfo_swow_buffer_getString

/* diffrent from __toString , it transfers ownership to return_value */
static PHP_METHOD(swow_buffer, fetchString)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);
    char *value;

    ZEND_PARSE_PARAMETERS_NONE();

    value = cat_buffer_fetch(buffer);
    if (UNEXPECTED(value == NULL)) {
        RETURN_EMPTY_STRING();
    }
    sbuffer->offset = 0;

    RETURN_STR((zend_string *) (value - offsetof(zend_string, val)));
}

#define arginfo_swow_buffer_dupString arginfo_swow_buffer_getString

static PHP_METHOD(swow_buffer, dupString)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRINGL_FAST(buffer->value, buffer->length);
}

#define arginfo_swow_buffer_toString arginfo_swow_buffer_getString

/* return string is just readonly (COW) */
static PHP_METHOD(swow_buffer, toString)
{
    zend_string *string;

    ZEND_PARSE_PARAMETERS_NONE();

    string = swow_buffer_fetch_string(getThisBuffer());
    if (UNEXPECTED(string == NULL)) {
        RETURN_EMPTY_STRING();
    }

    RETURN_STR(swow_buffer_string_copy(string));
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_swow_buffer_close, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, close)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    SWOW_BUFFER_CHECK_LOCK(sbuffer);

    ZEND_PARSE_PARAMETERS_NONE();

    cat_buffer_close(buffer);
    swow_buffer_init(sbuffer);
}

#define arginfo_swow_buffer___toString arginfo_swow_buffer_toString

#define zim_swow_buffer___toString zim_swow_buffer_toString

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_buffer___debugInfo, ZEND_RETURN_VALUE, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_buffer, __debugInfo)
{
    SWOW_BUFFER_GETTER(sbuffer, buffer);
    zval zdebug_info;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(&zdebug_info);
    if (buffer->value == NULL){
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
    PHP_ME(swow_buffer, alignSize,         arginfo_swow_buffer_alignSize,         ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swow_buffer, __construct,       arginfo_swow_buffer___construct,       ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, alloc,             arginfo_swow_buffer_alloc,             ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, getSize,           arginfo_swow_buffer_getSize,           ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, getLength,         arginfo_swow_buffer_getLength,         ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, getAvailableSize,  arginfo_swow_buffer_getAvailableSize,  ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, getReadableLength, arginfo_swow_buffer_getReadableLength, ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, getWritableSize,   arginfo_swow_buffer_getWritableSize,   ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, isReadable,        arginfo_swow_buffer_isReadable,        ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, isWritable,        arginfo_swow_buffer_isWritable,        ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, isSeekable,        arginfo_swow_buffer_isSeekable,        ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, isAvailable,       arginfo_swow_buffer_isAvailable,       ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, isEmpty,           arginfo_swow_buffer_isEmpty,           ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, isFull,            arginfo_swow_buffer_isFull,            ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, realloc,           arginfo_swow_buffer_realloc,           ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, extend,            arginfo_swow_buffer_extend,            ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, mallocTrim,        arginfo_swow_buffer_mallocTrim,        ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, tell,              arginfo_swow_buffer_tell,              ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, rewind,            arginfo_swow_buffer_rewind,            ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, eof,               arginfo_swow_buffer_eof,               ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, seek,              arginfo_swow_buffer_seek,              ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, read,              arginfo_swow_buffer_read,              ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, peek,              arginfo_swow_buffer_peek,              ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, peekFrom,          arginfo_swow_buffer_peekFrom,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, getContents,       arginfo_swow_buffer_getContents,       ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, write,             arginfo_swow_buffer_write,             ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, truncate,          arginfo_swow_buffer_truncate,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, clear,             arginfo_swow_buffer_clear,             ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, fetchString,       arginfo_swow_buffer_fetchString,       ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, dupString,         arginfo_swow_buffer_dupString,         ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, toString,          arginfo_swow_buffer_toString,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, close,             arginfo_swow_buffer_close,             ZEND_ACC_PUBLIC)
    /* magic */
    PHP_ME(swow_buffer, __toString,        arginfo_swow_buffer___toString,        ZEND_ACC_PUBLIC)
    PHP_ME(swow_buffer, __debugInfo,       arginfo_swow_buffer___debugInfo,       ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static zend_object *swow_buffer_clone_object(zend7_object *object)
{
    swow_buffer_t *sbuffer, *new_sbuffer;
    zend_string *string, *new_string;

    sbuffer = swow_buffer_get_from_object(Z7_OBJ_P(object));
    string = swow_buffer_fetch_string(sbuffer);
    new_sbuffer = swow_buffer_get_from_object(swow_object_create(sbuffer->std.ce));
    memcpy(new_sbuffer, sbuffer, offsetof(swow_buffer_t, std));
    if (string != NULL) {
        new_string = zend_string_alloc(sbuffer->buffer.size, 0);
        memcpy(ZSTR_VAL(new_string), ZSTR_VAL(string), ZSTR_LEN(string));
        ZSTR_VAL(new_string)[ZSTR_LEN(new_string) = ZSTR_LEN(string)] = '\0';
        new_sbuffer->buffer.value = ZSTR_VAL(new_string);
    }

    zend_objects_clone_members(&new_sbuffer->std, Z7_OBJ_P(object));

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
        swow_buffer_string_release(old_string);
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
    swow_buffer_string_release(swow_buffer_get_string_from_value(value));
}

SWOW_API const cat_buffer_allocator_t swow_buffer_allocator = {
    swow_buffer_alloc_standard,
    swow_buffer_realloc_standard,
    swow_buffer_update_standard,
    swow_buffer_free_standard,
};

int swow_buffer_module_init(INIT_FUNC_ARGS)
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
        cat_true, cat_false, cat_false,
        swow_buffer_create_object,
        swow_buffer_free_object,
        XtOffsetOf(swow_buffer_t, std)
    );
    swow_buffer_handlers.clone_obj = swow_buffer_clone_object;

    zend_declare_class_constant_long(swow_buffer_ce, ZEND_STRL("PAGE_SIZE"), cat_getpagesize());
    zend_declare_class_constant_long(swow_buffer_ce, ZEND_STRL("DEFAULT_SIZE"), CAT_BUFFER_DEFAULT_SIZE);

    swow_buffer_exception_ce = swow_register_internal_class(
        "Swow\\Buffer\\Exception", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}
