/*
  +--------------------------------------------------------------------------+
  | libcat                                                                   |
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

#include "cat_buffer.h"

CAT_API cat_buffer_allocator_t cat_buffer_allocator;

static char *cat_buffer_alloc_standard(size_t size)
{
    char *value = (char *) cat_malloc(size);

#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(value == NULL)) {
        cat_update_last_error_of_syscall("Malloc for buffer value failed with size %zu", size);
        return NULL;
    }
#endif

    return value;
}

static char *cat_buffer_realloc_standard(char *old_value, size_t new_size)
{
    char *new_value = (char *) cat_realloc(old_value, new_size);

#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(new_value == NULL)) {
        cat_update_last_error_of_syscall("Realloc for buffer value failed with new size %zu", new_size);
        return NULL;
    }
#endif

    return new_value;
}

static void cat_buffer_free_standard(char *value)
{
    cat_free(value);
}

CAT_API cat_bool_t cat_buffer_module_init(void)
{
    static const cat_buffer_allocator_t allocator = {
        cat_buffer_alloc_standard,
        cat_buffer_realloc_standard,
        NULL,
        cat_buffer_free_standard
    };

    cat_buffer_allocator = allocator;

    return cat_true;
}

CAT_API cat_bool_t cat_buffer_register_allocator(const cat_buffer_allocator_t *allocator)
{
    if (
        allocator->alloc_function == NULL ||
        allocator->realloc_function == NULL ||
        allocator->free_function == NULL
    ) {
        cat_update_last_error(CAT_EINVAL, "Allocator must be filled (except update)");
        return cat_false;
    }
    cat_buffer_allocator = *allocator;

    return cat_true;
}

static cat_always_inline void cat_buffer__init(cat_buffer_t *buffer)
{
    buffer->value = NULL; /* support lazy loading */
    buffer->size = 0;
    buffer->length = 0;
}

static cat_always_inline cat_bool_t cat_buffer__alloc(cat_buffer_t *buffer, size_t size)
{
    char *value;

    if (size == 0) {
        /* do nothing */
        return cat_true;
    }

    value = cat_buffer_allocator.alloc_function(size);

    if (unlikely(value == NULL)) {
        return cat_false;
    }

    buffer->value = value;
    buffer->size = size;

    return cat_true;
}

static cat_always_inline void cat_buffer__update(cat_buffer_t *buffer, size_t new_length)
{
    buffer->length = new_length;
    if (cat_buffer_allocator.update_function != NULL) {
        cat_buffer_allocator.update_function(buffer->value, new_length);
    }
}

CAT_API size_t cat_buffer_align_size(size_t size, size_t alignment)
{
    if (size == 0) {
        size = 1;
    }
    if (alignment == 0) {
        alignment = CAT_MEMORY_DEFAULT_ALIGNED_SIZE;
    }
    return CAT_MEMORY_ALIGNED_SIZE_EX(size, alignment);;
}

CAT_API void cat_buffer_init(cat_buffer_t *buffer)
{
    cat_buffer__init(buffer);
}

CAT_API cat_bool_t cat_buffer_alloc(cat_buffer_t *buffer, size_t size)
{
    if (unlikely(buffer->value != NULL)) {
        cat_update_last_error(CAT_EMISUSE, "Buffer has been allocated");
        return cat_false;
    }

    return cat_buffer__alloc(buffer, size);
}

CAT_API cat_bool_t cat_buffer_create(cat_buffer_t *buffer, size_t size)
{
    cat_buffer__init(buffer);
    return cat_buffer__alloc(buffer, size);
}

CAT_API cat_bool_t cat_buffer_realloc(cat_buffer_t *buffer, size_t new_size)
{
    char *new_value;

    if (unlikely(new_size == buffer->size)) {
        return cat_true;
    }
    if (unlikely(new_size == 0)) {
        /* realloc may return valid ptr, but we do not need it */
        cat_buffer_close(buffer);
        return cat_true;
    }

    new_value = cat_buffer_allocator.realloc_function(buffer->value, new_size);

    if (unlikely(new_value == NULL)) {
        return cat_false;
    }

    buffer->value = new_value;
    buffer->size = new_size;
    if (unlikely(new_size < buffer->length)) {
        /* need not call update, it should be done in realloc */
        buffer->length = new_size;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_buffer_extend(cat_buffer_t *buffer, size_t recommend_size)
{
    size_t new_size = buffer->size;

    if (unlikely(new_size == 0)) {
        new_size = cat_buffer_align_size(recommend_size, 0);
    } else {
        new_size = cat_buffer_align_size(new_size, 0);
        do {
            new_size += new_size;
        } while (new_size < recommend_size);
    }

    CAT_ASSERT(new_size > buffer->size);

    return cat_buffer_realloc(buffer, new_size);
}

CAT_API cat_bool_t cat_buffer_prepare(cat_buffer_t *buffer, size_t append_length)
{
    size_t expected_size = buffer->length + append_length;
    if (unlikely(expected_size > buffer->size)) {
        return cat_buffer_extend(buffer, expected_size);
    }
    return cat_true;
}

CAT_API cat_bool_t cat_buffer_malloc_trim(cat_buffer_t *buffer)
{
    if (unlikely(buffer->length == buffer->size)) {
        return cat_true;
    }

    return cat_buffer_realloc(buffer, buffer->length);
}

CAT_API cat_bool_t cat_buffer_write(cat_buffer_t *buffer, size_t offset, const void *ptr, size_t length)
{
    size_t new_length = offset + length;

    if (new_length > buffer->size) {
        if (unlikely(!cat_buffer_extend(buffer, new_length))) {
            return cat_false;
        }
    }
    if (likely(length > 0)) {
        // do not use memcpy, ptr maybe at the same scope with dest
        memmove(buffer->value + offset, ptr, length);
        if (new_length > buffer->length) {
            cat_buffer__update(buffer, new_length);
        }
    }

    return cat_true;
}

CAT_API cat_bool_t cat_buffer_append(cat_buffer_t *buffer, const void *ptr, size_t length)
{
    size_t new_length = buffer->length + length;

    if (new_length > buffer->size) {
        if (unlikely(!cat_buffer_extend(buffer, new_length))) {
            return cat_false;
        }
    }
    if (likely(length > 0)) {
        // make sure that memory is not overlapped
        CAT_ASSERT(
            (const char *) ptr > buffer->value + buffer->length + length ||
            (const char *) ptr < buffer->value + buffer->length
        );
        memcpy(buffer->value + buffer->length, ptr, length);
        cat_buffer__update(buffer, new_length);
    }

    return cat_true;
}

CAT_API void cat_buffer_truncate(cat_buffer_t *buffer, size_t length)
{
    cat_buffer_truncate_from(buffer, 0, length);
}

CAT_API void cat_buffer_truncate_from(cat_buffer_t *buffer, size_t offset, size_t length)
{
    if (unlikely(buffer->value == NULL)) {
        return;
    }
    if (length == 0 || offset >= buffer->length) {
        cat_buffer_clear(buffer);
        return;
    }
    size_t readable_size = buffer->length - offset;
    if (length > readable_size) {
        length = readable_size;
    }
    if (offset != 0) {
        memmove(buffer->value, buffer->value + offset, length);
    } else if (length == buffer->length) {
        /* nothing changed */
        return;
    }
    cat_buffer__update(buffer, length);
}

CAT_API void cat_buffer_clear(cat_buffer_t *buffer)
{
    if (unlikely(buffer->value == NULL)) {
        return;
    }
    cat_buffer__update(buffer, 0);
}

CAT_API char *cat_buffer_fetch(cat_buffer_t *buffer)
{
    char *value = buffer->value;

    cat_buffer__init(buffer);

    return value;
}

CAT_API cat_bool_t cat_buffer_dup(cat_buffer_t *buffer, cat_buffer_t *new_buffer)
{
    cat_bool_t ret;

    CAT_ASSERT(buffer != new_buffer);

    ret = cat_buffer_create(new_buffer, buffer->size);

    if (unlikely(!ret)) {
        return cat_false;
    }

    ret = cat_buffer_write(new_buffer, 0, buffer->value, buffer->length);

    if (unlikely(!ret)) {
        cat_buffer_close(new_buffer);
        return cat_false;
    }

    return cat_true;
}

CAT_API void cat_buffer_close(cat_buffer_t *buffer)
{
    if (buffer->value == NULL) {
        return;
    }
    cat_buffer_allocator.free_function(buffer->value);
    cat_buffer__init(buffer);
}

CAT_API char *cat_buffer_get_value(const cat_buffer_t *buffer)
{
    return buffer->value;
}

CAT_API size_t cat_buffer_get_size(const cat_buffer_t *buffer)
{
    return buffer->size;
}

CAT_API size_t cat_buffer_get_length(const cat_buffer_t *buffer)
{
    return buffer->length;
}

CAT_API cat_bool_t cat_buffer_make_pair(cat_buffer_t *read_buffer, size_t rsize, cat_buffer_t *write_buffer, size_t wsize)
{
    cat_bool_t ret;

    ret = cat_buffer_create(read_buffer, rsize);

    if (unlikely(!ret)) {
        return cat_false;
    }

    ret = cat_buffer_create(write_buffer, wsize);

    if (unlikely(!ret)) {
        cat_buffer_close(read_buffer);
        return cat_false;
    }

    return cat_true;
}

CAT_API void cat_buffer_dump(cat_buffer_t *buffer)
{
    CAT_LOG_INFO(BUFFER, "Buffer(%p) {\n"
        "    [\"value\"]=>\n"
        "    string(%zu) \"%.*s\"\n"
        "    [\"length\"]=>\n"
        "    size_t(%zu)\n"
        "    [\"size\"]=>\n"
        "    size_t(%zu)\n"
        "}\n",
        buffer,
        buffer->length, (int) buffer->length, buffer->value,
        buffer->length,
        buffer->size
    );
}

/* smart_str-like APIs */

CAT_API cat_bool_t cat_buffer_append_unsigned(cat_buffer_t *buffer, size_t value)
{
    char tmp[32];
    size_t length = snprintf(tmp, sizeof(tmp), "%zu", value);
    return cat_buffer_append(buffer, tmp, length);
}

CAT_API cat_bool_t cat_buffer_append_signed(cat_buffer_t *buffer, ssize_t value)
{
    intmax_t i = (intmax_t) value;
    char tmp[32];
    size_t length = snprintf(tmp, sizeof(tmp), "%jd", i);
    return cat_buffer_append(buffer, tmp, length);
}

CAT_API cat_bool_t cat_buffer_append_double(cat_buffer_t *buffer, double value)
{
    char tmp[32];
    size_t length = snprintf(tmp, sizeof(tmp), "%f", value);
    return cat_buffer_append(buffer, tmp, length);
}

CAT_API cat_bool_t cat_buffer_append_char(cat_buffer_t *buffer, char c)
{
    return cat_buffer_append(buffer, &c, 1);
}

CAT_API cat_bool_t cat_buffer_append_str(cat_buffer_t *buffer, const char *str)
{
    return cat_buffer_append(buffer, str, strlen(str));
}

CAT_API cat_bool_t cat_buffer_append_vprintf(cat_buffer_t *buffer, const char *format, va_list args)
{
    va_list _args;
    size_t length;

    va_copy(_args, args);
    length = vsnprintf(NULL, 0, format, _args);
    va_end(_args);

    /* TODO: drop +1 will lead coredump, figure out why */
    if (unlikely(!cat_buffer_prepare(buffer, length + 1))) {
        return cat_false;
    }
    if (unlikely(
        vsnprintf(buffer->value + buffer->length,
                  buffer->size - buffer->length,
                  format, args) < 0)) {
        return cat_false;
    }
    buffer->length += length;

    return cat_true;
}

CAT_API cat_bool_t cat_buffer_append_printf(cat_buffer_t *buffer, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    cat_bool_t ret = cat_buffer_append_vprintf(buffer, format, args);
    va_end(args);
    return ret;
}

CAT_API cat_bool_t cat_buffer_zero_terminate(cat_buffer_t *buffer)
{
    cat_bool_t ret = cat_buffer_append_char(buffer, '\0');
    if (unlikely(!ret)) {
        return cat_false;
    }
    buffer->length--;
    return cat_true;
}

CAT_API cat_bool_t cat_buffer_append_with_padding(cat_buffer_t *buffer, const void *ptr, size_t length, const char padding_char, size_t width)
{
    ssize_t padding_length = width - length;
    size_t padding_left = padding_length / 2;
    size_t padding_right = padding_length - padding_left;
    size_t i;

    if (!cat_buffer_prepare(buffer, length + padding_length)) {
        return cat_false;
    }

    for (i = 0; i < padding_left; i++) {
        (void) cat_buffer_append_char(buffer, padding_char);
    }
    (void) cat_buffer_append(buffer, ptr, length);
    for (i = 0; i < padding_right; i++) {
        (void) cat_buffer_append_char(buffer, padding_char);
    }

    return cat_true;
}

CAT_API cat_bool_t cat_buffer_append_str_with_padding(cat_buffer_t *buffer, const char *str, const char padding_char, size_t width)
{
    return cat_buffer_append_with_padding(buffer, str, strlen(str), padding_char, width);
}

/* buffer_str */

CAT_API CAT_BUFFER_STR_FREE char *cat_buffer_export_str(cat_buffer_t *buffer)
{
    cat_buffer_zero_terminate(buffer);
    return buffer->value;
}

CAT_API void cat_buffer_str_free(char *str)
{
    cat_buffer_allocator.free_function(str);
}
