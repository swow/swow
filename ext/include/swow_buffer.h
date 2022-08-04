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

#include "cat_buffer.h"

extern SWOW_API zend_class_entry *swow_buffer_ce;
extern SWOW_API zend_object_handlers swow_buffer_handlers;

extern SWOW_API zend_class_entry *swow_buffer_exception_ce;

extern SWOW_API const cat_buffer_allocator_t swow_buffer_allocator;

typedef struct swow_buffer_s {
    /* === public ===  */
    cat_buffer_t buffer;
    size_t offset;
    /* === private ===  */
    /* Notice: we must lock this buffer during coroutine scheduling
     * otherwise, other coroutines will change the internal value of buffer
     * TODO: can we use swow_coroutine_t* and show it trace here?
     * but this module should not have any other dependency */
    cat_bool_t locked;
    cat_bool_t user_locked;
    /* ownership is not on the current object, it needs to be separated when writing  */
    cat_bool_t shared;
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

/* internal use */

SWOW_API zend_string *swow_buffer_get_string(swow_buffer_t *sbuffer);

#define swow_string_get_readable_space(string, offset, length, arg_num) \
        swow_string_get_readable_space_v(string, offset, length, arg_num, 1, arg_num)
#define swow_buffer_get_readable_space(s_buffer, offset, length, arg_num) \
        swow_buffer_get_readable_space_v(s_buffer, offset, length, arg_num, 1, arg_num)
#define swow_buffer_get_writable_space(s_buffer, offset, size, arg_num) \
        swow_buffer_get_writable_space_v(s_buffer, offset, size, arg_num, 1, arg_num)

SWOW_API char *swow_string_get_readable_space_v(zend_string *string, zend_long offset, zend_long *length, uint32_t vector_arg_num, uint32_t vector_count, uint32_t base_arg_num);
SWOW_API char *swow_buffer_get_readable_space_v(swow_buffer_t *s_buffer, zend_long offset, zend_long *length, uint32_t vector_arg_num, uint32_t vector_count, uint32_t base_arg_num);
SWOW_API char *swow_buffer_get_writable_space_v(swow_buffer_t *s_buffer, zend_long offset, zend_long *size, uint32_t vector_arg_num, uint32_t vector_count, uint32_t base_arg_num);

SWOW_API void swow_buffer_virtual_write(swow_buffer_t *sbuffer, size_t length); SWOW_INTERNAL SWOW_UNSAFE
/* this API should be called before write data to the buffer */
SWOW_API void swow_buffer_cow(swow_buffer_t *sbuffer);

SWOW_INTERNAL
#define SWOW_BUFFER_CHECK_STRING_SCOPE_EX(string, offset, length, failure) do { \
    if (UNEXPECTED(offset < 0)) { \
        zend_value_error("String offset can not be negative"); \
        failure; \
    } \
    if (UNEXPECTED(((size_t) offset) > ZSTR_LEN(string))) { \
        zend_value_error("String offset overflow"); \
        failure; \
    } \
    if (length == -1) { \
        length = ZSTR_LEN(string) - offset; \
    } \
    if (UNEXPECTED(((size_t) (offset + length)) > ZSTR_LEN(string))) { \
        zend_value_error("String scope overflow"); \
        failure; \
    } \
    if (UNEXPECTED(length < 0)) { \
        zend_value_error("String length should be greater than 0 or equal to -1 (unlimited)"); \
        failure; \
    } \
} while (0)

SWOW_INTERNAL
#define SWOW_BUFFER_CHECK_STRING_SCOPE(string, offset, length) \
        SWOW_BUFFER_CHECK_STRING_SCOPE_EX(string, offset, length, RETURN_THROWS())

static zend_always_inline zend_bool swow_buffer_is_locked(const swow_buffer_t *sbuffer)
{
    return sbuffer->locked || sbuffer->user_locked;
}

#define SWOW_BUFFER_CHECK_LOCK_EX(sbuffer, failure) do { \
    if (UNEXPECTED(swow_buffer_is_locked(sbuffer))) { \
        swow_throw_exception( \
            swow_buffer_exception_ce, \
            CAT_ELOCKED, "Buffer has been locked by %s", \
            (sbuffer)->user_locked ? "user" : "internal" \
        ); \
        failure; \
    } \
} while (0)

#define SWOW_BUFFER_CHECK_LOCK(sbuffer) \
        SWOW_BUFFER_CHECK_LOCK_EX(sbuffer, RETURN_THROWS())

#define SWOW_BUFFER_CHECK_LOCK_IF(sbuffer, condition) do { \
    if (condition) { \
        SWOW_BUFFER_CHECK_LOCK(sbuffer); \
    } \
} while (0)

SWOW_INTERNAL
#define SWOW_BUFFER_LOCK_EX(sbuffer, failure) do { \
    SWOW_BUFFER_CHECK_LOCK_EX(sbuffer, failure); \
    (sbuffer)->locked = cat_true; \
} while (0)

SWOW_INTERNAL
#define SWOW_BUFFER_LOCK(sbuffer) do { \
    SWOW_BUFFER_CHECK_LOCK(sbuffer); \
    (sbuffer)->locked = cat_true; \
} while (0)

SWOW_INTERNAL
#define SWOW_BUFFER_UNLOCK(sbuffer) do { \
    ZEND_ASSERT((sbuffer)->locked && "unlock an unlocked buffer"); \
    (sbuffer)->locked = cat_false; \
} while (0)

#ifdef __cplusplus
}
#endif
#endif /* SWOW_BUFFER_H */
