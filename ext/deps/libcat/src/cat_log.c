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

#include "cat.h"

#ifdef CAT_IDE_HELPER
#include "cat_log.h"
#endif

#include "cat_buffer.h"
#include "cat_coroutine.h" /* for coroutine id (TODO: need to decouple it?) */

CAT_API cat_log_t cat_log_function;

static cat_always_inline const char *cat_log_type_dispatch(cat_log_type_t type, FILE **output_ptr)
{
    const char *type_name;
    FILE *output;

    switch (type) {
        case CAT_LOG_TYPE_DEBUG : {
#ifndef CAT_ENABLE_DEBUG_LOG
            return NULL;
#else
            type_name = "Debug";
            output = CAT_LOG_G(debug_output);
            break;
#endif
        }
        case CAT_LOG_TYPE_INFO : {
            type_name = "Info";
            output = stdout;
            break;
        }
        case CAT_LOG_TYPE_NOTICE : {
            type_name = "Notice";
            output = CAT_LOG_G(error_output);
            break;
        }
        case CAT_LOG_TYPE_WARNING : {
            type_name = "Warning";
            output = CAT_LOG_G(error_output);
            break;
        }
        case CAT_LOG_TYPE_ERROR : {
            type_name = "Error";
            output = CAT_LOG_G(error_output);
            break;
        }
        case CAT_LOG_TYPE_CORE_ERROR : {
            type_name = "Core Error";
            output = CAT_LOG_G(error_output);
            break;
        }
        default:
            CAT_NEVER_HERE("Unknown log type");
    }

    if (output_ptr != NULL) {
        *output_ptr = output;
    }
    return type_name;
}

static cat_always_inline void cat_log_timespec_sub(struct timespec *tv, const struct timespec *a, const struct timespec *b)
{
    tv->tv_sec = a->tv_sec - b->tv_sec;
    tv->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (tv->tv_nsec < 0) {
        tv->tv_sec--;
        tv->tv_nsec += 1000000000;
    }
}

static void cat_log_buffer_append_timestamps(cat_buffer_t *buffer, unsigned int level, const char *format, cat_bool_t relative)
{
    const struct {
        const char *name;
        unsigned int width;
        unsigned int scale;
    } scale_options[] = {
        { "s" , 0, 1000000000 }, // placeholder
        { "ms", 3, 1000000 },
        { "us", 6, 1000 },
        { "ns", 9, 1 },
    };
    if (unlikely(!cat_buffer_prepare(buffer, 32))) {
        return;
    }
    if (!relative) {
        struct timespec ts;
        cat_clock_gettime_realtime(&ts);
        time_t local = ts.tv_sec;
        struct tm *tm = localtime(&local);
        size_t t_length;
        t_length = strftime(
            buffer->value + buffer->length,
            buffer->size - buffer->length,
            format,
            tm
        );
        if (t_length == 0) {
            return;
        }
        buffer->length += t_length;
        level = CAT_MIN(level, CAT_ARRAY_SIZE(scale_options));
        if (level > 1) {
            // TODO: now just use "us" as default, supports more in the future
            (void) cat_buffer_append_printf(
                buffer, ".%0*ld",
                scale_options[level - 1].width,
                (long) (ts.tv_nsec / scale_options[level - 1].scale)
            );
        }
    } else {
        struct timespec ts;
        cat_clock_gettime_monotonic(&ts);
        static struct timespec ots;
        if (ots.tv_sec == 0) {
            ots = ts;
        }
        struct timespec dts;
        cat_log_timespec_sub(&dts, &ts, &ots);
        ots = ts;
        int t_length = snprintf(
            buffer->value + buffer->length,
            buffer->size - buffer->length,
            "%6ld", (long) dts.tv_sec
        );
        if (t_length == 0) {
            return;
        }
        buffer->length += t_length;
        // starts with msec, not sec
        level = CAT_MIN(level, CAT_ARRAY_SIZE(scale_options) - 1);
        (void) cat_buffer_append_printf(
            buffer, ".%0*ld",
            scale_options[level].width,
            (long) dts.tv_nsec / scale_options[level].scale
        );
    }
}

static cat_always_inline cat_bool_t cat_log_select_writable(cat_os_socket_t fd)
{
    fd_set wfd;
    struct timeval tv;
    int ret;
    FD_ZERO(&wfd);
#ifndef CAT_OS_WIN
    if (fd < FD_SETSIZE) {
#endif
        FD_SET(fd, &wfd);
#ifndef CAT_OS_WIN
    }
#endif
    tv.tv_sec = -1;
    tv.tv_usec = 0;
    ret = cat_sys_select(fd + 1, NULL, &wfd, NULL, &tv);
    return ret != -1;
}

#ifdef CAT_OS_WIN
# undef fileno
# define fileno _fileno
typedef unsigned int cat_syscall_write_length_t;
#else
typedef size_t cat_syscall_write_length_t;
#endif

static cat_bool_t cat_log_fwrite_use_libc_impl = cat_false;


static cat_bool_t cat_log_fwrite_libc_impl(FILE *file, const char *str, size_t length)
{
    size_t n = fwrite(str, 1, length, file);
    if (unlikely(n != length)) {
        char *s;
        fprintf(CAT_LOG_G(error_output),
            "libcat error: write(fd: %d, length: %d) for log message failed (error:%d:%s)\n",
            (int) fileno(file), (int) length, cat_sys_errno, cat_strerror(cat_sys_errno));
        fprintf(CAT_LOG_G(error_output),
            "libcat error: un-outputted log message: %s\n",
            cat_log_str_quote(str, CAT_MIN(length, 128), &s)
        );
        cat_free(s);
        return cat_false;
    }
    fflush(file);
    return cat_true;
}

static cat_bool_t cat_log_fwrite_syscall_impl(FILE *file, const char *str, size_t length)
{
    const char *p = str;
    const char *pe = p + length;

    int fd = fileno(file);
    while (1) {
        size_t l = pe - p;
        ssize_t n;
        do {
            n = write(fd, p, (cat_syscall_write_length_t) l);
        } while (n <= 0 && cat_sys_errno == EAGAIN && cat_log_select_writable(fd));
        if (unlikely(n < 0)) {
            cat_log_fwrite_use_libc_impl = cat_true;
            return cat_log_fwrite_libc_impl(file, p, l);
        }
        p += n;
        if (p == pe) {
            break;
        }
    }

    return cat_true;
}

CAT_API cat_bool_t cat_log_fwrite(FILE *file, const char *str, size_t length)
{
    if (likely(!cat_log_fwrite_use_libc_impl)) {
        return cat_log_fwrite_syscall_impl(file, str, length);
    } else {
        return cat_log_fwrite_libc_impl(file, str, length);
    }
}

CAT_API void cat_log_va_list_standard(CAT_LOG_VA_LIST_PARAMETERS)
{
    cat_buffer_t buffer;
    cat_bool_t ret;
    const char *type_name;
    FILE *output;

    if (unlikely(type & CAT_LOG_TYPES_ABNORMAL)) {
        va_list _args;
        va_copy(_args, args);
        cat_set_last_error(code, cat_vsprintf(format, _args));
        va_end(_args);
    }

    type_name = cat_log_type_dispatch(type, &output);
    if (unlikely(type_name == NULL)) {
        return;
    }

    ret = cat_buffer_create(&buffer, 0);
    if (unlikely(!ret)) {
        fprintf(CAT_LOG_G(error_output), "libcat error: create log buffer failed (%s)\n", cat_get_last_error_message());
        return;
    }

    do {
        unsigned int timestamps_level = CAT_LOG_G(show_timestamps);
        if (timestamps_level == 0) {
            break;
        }
        (void) cat_buffer_append_char(&buffer, '[');
        (void) cat_log_buffer_append_timestamps(
            &buffer,
            timestamps_level,
            CAT_LOG_G(timestamps_format),
            CAT_LOG_G(show_timestamps_as_relative)
        );
        (void) cat_buffer_append_str(&buffer, "] ");
    } while (0);

    // role_name + log_type + module_name
    do {
        const int name_width = 12;
        const char *name = cat_coroutine_get_current_role_name_in_uppercase();
        size_t name_length;
        char name_buffer[32];
        if (name == NULL) {
            name_buffer[0] = 'R';
            int n = snprintf(name_buffer + 1, sizeof(name_buffer) - 1,
                CAT_COROUTINE_ID_FMT, cat_coroutine_get_current_id());
            if (n < 0) {
                break;
            }
            name_length = (size_t) n + 1;
            name_buffer[name_length] = '\0';
            name = name_buffer;
        } else {
            name_length = strlen(name);
        }
        (void) cat_buffer_append_char(&buffer, '[');
        (void) cat_buffer_append_str_with_padding(&buffer, name, ' ', name_width);
        (void) cat_buffer_append_str(&buffer, "] ");
        (void) cat_buffer_append_str(&buffer, type_name);
#ifdef CAT_ENABLE_DEBUG_LOG
        if (type == CAT_LOG_TYPE_DEBUG && CAT_LOG_G(debug_level) > 1) {
            (void) cat_buffer_append_str(&buffer, "(v");
            (void) cat_buffer_append_signed(&buffer, CAT_LOG_G(last_debug_log_level));
            (void) cat_buffer_append_char(&buffer, ')');
        }
#endif
        (void) cat_buffer_append_str(&buffer, ": <");
        (void) cat_buffer_append_str(&buffer, module_name);
        (void) cat_buffer_append_str(&buffer, "> ");
    } while (0);

    ret = cat_buffer_append_vprintf(&buffer, format, args);
    if (unlikely(!ret)) {
        fprintf(CAT_LOG_G(error_output), "libcat error: vprintf() log message failed (%s)\n", cat_get_last_error_message());
        goto _error;
    }

    cat_buffer_append_str(&buffer, "\n");

#ifdef CAT_SOURCE_POSITION
    if (CAT_LOG_G(show_source_postion)) {
        (void) cat_buffer_append_printf(
            &buffer,
            "  ^ " "%s:%d | %s()\n",
            file, line, function
        );
    }
#endif

    cat_buffer_zero_terminate(&buffer);

    if (!cat_log_fwrite(output, buffer.value, buffer.length)) {
        goto _error;
    }

    cat_buffer_close(&buffer);

    if (0) {
        _error:
        cat_buffer_close(&buffer);
        if (type & CAT_LOG_TYPES_ABNORMAL) {
            cat_set_last_error(code, NULL);
        }
    }

    if (type & (CAT_LOG_TYPE_ERROR | CAT_LOG_TYPE_CORE_ERROR)) {
        cat_abort();
    }
}

CAT_API void cat_log_standard(CAT_LOG_PARAMETERS)
{
    va_list args;
    va_start(args, format);

    cat_log_va_list_standard(type, module_name CAT_SOURCE_POSITION_RELAY_CC, code, format, args);

    va_end(args);
}

CAT_API const char *cat_log_str_quote(const char *str, size_t n, char **tmp_str)
{
    cat_str_quote_style_flag_t style = CAT_STR_QUOTE_STYLE_FLAG_NONE;
    char *quoted_data;
    if (n > CAT_LOG_G(str_size)) {
        n = CAT_LOG_G(str_size);
        style |= CAT_STR_QUOTE_STYLE_FLAG_ELLIPSIS;
    }
    cat_str_quote_ex(str, n, &quoted_data, NULL, style, NULL, NULL);
    return *tmp_str = quoted_data;
}

CAT_API const char *cat_log_str_quote_unlimited(const char *str, size_t n, char **tmp_str)
{
    char *quoted_data;
    cat_str_quote(str, n, &quoted_data, NULL);
    return *tmp_str = quoted_data;
}
