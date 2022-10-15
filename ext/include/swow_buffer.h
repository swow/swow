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

#ifndef SWOW_BUFFER_H
#define SWOW_BUFFER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"
#include "swow_coroutine.h"

#include "cat_buffer.h"

extern SWOW_API zend_class_entry *swow_buffer_ce;
extern SWOW_API zend_object_handlers swow_buffer_handlers;

extern SWOW_API zend_class_entry *swow_buffer_exception_ce;

extern SWOW_API const cat_buffer_allocator_t swow_buffer_allocator;

typedef struct swow_buffer_s {
    /* === public ===  */
    cat_buffer_t buffer;
    /* === private ===  */
    /* Notice: we must lock this buffer during coroutine scheduling
     * otherwise, other coroutines will change the internal value of buffer.
     * for reading on buffer, we do not need to lock it because refcount + COW
     * will make sure that string will always be dupped when writing by someone else. */
    swow_coroutine_t *locker;
    /* ================ */
    zend_object std;
} swow_buffer_t;

/* loader */

zend_result swow_buffer_module_init(INIT_FUNC_ARGS);

/* helper */

static zend_always_inline swow_buffer_t *swow_buffer_get_from_handle(cat_buffer_t *buffer)
{
    return cat_container_of(buffer, swow_buffer_t, buffer);
}

static zend_always_inline swow_buffer_t *swow_buffer_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_buffer_t, std);
}

/* buffer string getter */

static zend_always_inline zend_string *swow_buffer_get_string_from_value(char *value)
{
    return (zend_string *) (value - offsetof(zend_string, val));
}

static zend_always_inline zend_string *swow_buffer_get_string_from_handle(cat_buffer_t *buffer) SWOW_UNSAFE
{
    return swow_buffer_get_string_from_value(buffer->value);
}

static zend_always_inline zend_string *swow_buffer_get_string(swow_buffer_t *s_buffer)
{
    if (UNEXPECTED(s_buffer->buffer.value == NULL)) {
        return NULL;
    }
    return swow_buffer_get_string_from_handle(&s_buffer->buffer);
}

static zend_always_inline zend_string *swow_buffer_get_string_for_reading(swow_buffer_t *s_buffer)
{
    if (UNEXPECTED(s_buffer->buffer.value == NULL)) {
        return zend_empty_string;
    }
    return swow_buffer_get_string_from_handle(&s_buffer->buffer);
}

/* buffer space/region getter */

SWOW_API const char *swow_string_get_readable_space_v(zend_string *string, zend_long start, zend_long *length, uint32_t vector_arg_num, uint32_t vector_index, uint32_t base_arg_num);
SWOW_API const char *swow_buffer_get_readable_space_v(swow_buffer_t *s_buffer, zend_long start, zend_long *length, uint32_t vector_arg_num, uint32_t vector_index, uint32_t base_arg_num);
SWOW_API char *swow_buffer_get_writable_space_v(swow_buffer_t *s_buffer, zend_long offset, zend_long *size, uint32_t vector_arg_num, uint32_t vector_index, uint32_t base_arg_num);

static zend_always_inline const char *swow_string_get_readable_space(zend_string *string, zend_long start, zend_long *length, uint32_t arg_num)
{
    return swow_string_get_readable_space_v(string, start, length, 0, 0, arg_num);
}

static zend_always_inline const char *swow_buffer_get_readable_space(swow_buffer_t *s_buffer, zend_long start, zend_long *length, uint32_t arg_num)
{
    return swow_buffer_get_readable_space_v(s_buffer, start, length, 0, 0, arg_num);
}

static zend_always_inline char *swow_buffer_get_writable_space(swow_buffer_t *s_buffer, zend_long offset, zend_long *size, uint32_t arg_num)
{
    return swow_buffer_get_writable_space_v(s_buffer, offset, size, 0, 0, arg_num);
}

static zend_always_inline const char *swow_buffer_or_string_get_readable_space_v(swow_buffer_t *s_buffer, zend_string *string, zend_long start, zend_long *length, uint32_t vector_arg_num, uint32_t vector_index, uint32_t base_arg_num)
{
    if (s_buffer != NULL) {
        return swow_buffer_get_readable_space_v(s_buffer, start, length, vector_arg_num, vector_index, base_arg_num);
    } else {
        ZEND_ASSERT(string != NULL);
        return swow_string_get_readable_space_v(string, start, length, vector_arg_num, vector_index, base_arg_num);
    }
}

static zend_always_inline const char *swow_buffer_or_string_get_readable_space(swow_buffer_t *s_buffer, zend_string *string, zend_long start, zend_long *length, uint32_t arg_num)
{
    return swow_buffer_or_string_get_readable_space_v(s_buffer, string, start, length, 0, 0, arg_num);
}

/* buffer writer */

SWOW_API void swow_buffer_virtual_write(swow_buffer_t *s_buffer, size_t offset, size_t length); SWOW_INTERNAL SWOW_UNSAFE
SWOW_API void swow_buffer_update(swow_buffer_t *s_buffer, size_t length); SWOW_INTERNAL SWOW_UNSAFE
/* this API should be called before write data to the buffer */
SWOW_API void swow_buffer_cow(swow_buffer_t *s_buffer);

/* buffer locker */

static zend_always_inline swow_coroutine_t *swow_buffer_get_locker(const swow_buffer_t *s_buffer)
{
    return s_buffer->locker;
}

static zend_always_inline zend_bool swow_buffer_check_lock(swow_buffer_t *s_buffer)
{
    swow_coroutine_t *locker = swow_buffer_get_locker(s_buffer);
    if (UNEXPECTED(locker != NULL)) {
        zend_throw_error(
            NULL, "Buffer has been locked by Coroutine#" CAT_COROUTINE_ID_FMT "%s",
            locker->coroutine.id, locker == swow_coroutine_get_current() ? " (self)" : ""
        );
        return false;
    }
    return true;
}

static zend_always_inline zend_bool swow_buffer_lock(swow_buffer_t *s_buffer)
{
    if (UNEXPECTED(!swow_buffer_check_lock(s_buffer))) {
        return false;
    }
    s_buffer->locker = swow_coroutine_get_current();
    return true;
}

static zend_always_inline zend_bool swow_buffer_unlock(swow_buffer_t *s_buffer)
{
    swow_coroutine_t *locker = swow_buffer_get_locker(s_buffer);
    if (UNEXPECTED(locker != swow_coroutine_get_current())) {
        if (locker != NULL) {
            zend_throw_error(
                NULL, "Buffer was locked by Coroutine#" CAT_COROUTINE_ID_FMT ", "
                "can not be unlocked by current Coroutine#" CAT_COROUTINE_ID_FMT,
                locker->coroutine.id, cat_coroutine_get_current_id()
            );
        } else {
            zend_throw_error(NULL, "Buffer was not locked");
        }
        return false;
    }
    s_buffer->locker = NULL;
    return true;
}

#define SWOW_BUFFER_CHECK_LOCK_EX(s_buffer, failure) do { \
    if (UNEXPECTED(!swow_buffer_check_lock(s_buffer))) { \
        failure; \
    } \
} while (0)

#define SWOW_BUFFER_CHECK_LOCK(s_buffer) \
        SWOW_BUFFER_CHECK_LOCK_EX(s_buffer, RETURN_THROWS())

SWOW_INTERNAL
#define SWOW_BUFFER_LOCK_EX(s_buffer, failure) do { \
    if (UNEXPECTED(!swow_buffer_lock(s_buffer))) { \
        failure; \
    } \
} while (0)

SWOW_INTERNAL
#define SWOW_BUFFER_LOCK(s_buffer) \
        SWOW_BUFFER_LOCK_EX(s_buffer, RETURN_THROWS())

SWOW_INTERNAL
#define SWOW_BUFFER_UNLOCK(s_buffer) \
        if (UNEXPECTED(!swow_buffer_unlock(s_buffer))) { \
            abort(); \
        }

/* buffer ZPP helper */

#define SWOW_PARAM_STRINGABLE_EXPECT_BUFFER_FOR_READING(dest_string) \
    Z_PARAM_PROLOGUE(0, 0); \
    if (UNEXPECTED(!swow_parse_arg_stringable_expect_buffer_for_reading(_arg, &dest_string, _i))) { \
        _error = ZSTR_VAL((zend_ce_stringable)->name); \
        _error_code = ZPP_ERROR_WRONG_CLASS_OR_STRING; \
        break; \
    }

#define SWOW_PARAM_BUFFER_OR_STRINGABLE_FOR_READING(dest_s_buffer, dest_string) \
    Z_PARAM_PROLOGUE(0, 0); \
    if (UNEXPECTED(!swow_parse_arg_buffer_or_stringable_for_reading(_arg, &dest_s_buffer, &dest_string, _i))) { \
        _error = ZSTR_VAL((zend_ce_stringable)->name); \
        _error_code = ZPP_ERROR_WRONG_CLASS_OR_STRING; \
        break; \
    }

/**
 * this API is used by vector array args parsing, because we can not replace args of vector array directly
 * as native args did for weak string conversion, so we should get ptr of buffer_t * and
 * add refcount of buffer string and release it after reading was done.
 */
static zend_always_inline bool swow_parse_arg_buffer_or_stringable_for_reading(
    zval *arg, swow_buffer_t **dest_s_buffer, zend_string **dest_string, uint32_t arg_num
)
{
    if (Z_TYPE_P(arg) == IS_OBJECT) {
        zend_class_entry *ce = Z_OBJCE_P(arg);
        if (EXPECTED(
            /* we just compare __tostring to fast check it */
            // instanceof_function(ce, swow_buffer_ce) &&
            /* __toString() function maybe rewritten on PHP layer */
            ce->__tostring == swow_buffer_ce->__tostring
        )) {
            *dest_s_buffer = swow_buffer_get_from_object(Z_OBJ_P(arg));
            *dest_string = NULL;
            return true;
        }
        /* use weak to handle stringable object even if we are in strict mode */
        return swow_parse_arg_str_weak(arg, dest_string, arg_num);
    }

    *dest_s_buffer = NULL;
    return swow_parse_arg_stringable(arg, dest_string, arg_num);
}

static zend_always_inline bool swow_parse_arg_stringable_expect_buffer_for_reading(
    zval *arg, zend_string **dest_string, uint32_t arg_num
)
{
    if (Z_TYPE_P(arg) == IS_OBJECT) {
        zend_class_entry *ce = Z_OBJCE_P(arg);
        if (EXPECTED(
            /* we just compare __tostring to fast check it */
            // instanceof_function(ce, swow_buffer_ce) &&
            /* __toString() function maybe rewritten on PHP layer */
            ce->__tostring == swow_buffer_ce->__tostring
        )) {
            swow_buffer_t *s_buffer = swow_buffer_get_from_object(Z_OBJ_P(arg));
            zend_object_release(&s_buffer->std);
            ZVAL_STR_COPY(arg, *dest_string = swow_buffer_get_string_for_reading(s_buffer));
            return true;
        }
        /* use weak to handle stringable object even if we are in strict mode */
        return swow_parse_arg_str_weak(arg, dest_string, arg_num);
    }

    return swow_parse_arg_stringable(arg, dest_string, arg_num);
}

#ifdef __cplusplus
}
#endif
#endif /* SWOW_BUFFER_H */
