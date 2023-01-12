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

CAT_API CAT_GLOBALS_DECLARE(cat);

static cat_bool_t cat_args_registered = cat_false;

static uv_thread_t cat_main_thread_tid;

#if CAT_USE_BUG_DETECTOR
void cat_bug_detector_callback(int signum)
{
    (void) signum;
#ifndef CAT_BUG_REPORT
#define CAT_BUG_REPORT \
    "A bug occurred in libcat-v" CAT_VERSION ", please report it.\n" \
    "The libcat developers probably don't know about it,\n" \
    "and unless you report it, chances are it won't be fixed.\n" \
    "You can read How to report a bug doc before submitting any bug reports:\n" \
    ">> https://github.com/libcat/libcat/blob/master/.github/ISSUE.md \n"
#endif
    fprintf(stderr, CAT_BUG_REPORT);
    abort();
}
#endif

CAT_API cat_bool_t cat_module_init(void)
{
    cat_main_thread_tid = uv_thread_self();

    cat_log_function = cat_log_standard;

#ifdef CAT_USE_DYNAMIC_ALLOCATOR
    cat_register_allocator(NULL);
#endif
#if 0
    /* https://github.com/libuv/libuv/pull/2760 */
    uv_replace_allocator(
        cat_malloc_function,
        cat_realloc_function,
        cat_calloc_function,
        cat_free_function
    );
#endif

    CAT_GLOBALS_MODULE_INIT();
    CAT_GLOBALS_REGISTER(cat);

#if CAT_USE_BUG_DETECTOR
    if (cat_env_is_true("CAT_BUG_DETECTOR", cat_true)) {
        (void) signal(SIGSEGV, cat_bug_detector_callback);
    }
#endif

    cat_error_module_init();

    return cat_true;
}

CAT_API cat_bool_t cat_module_shutdown(void)
{
    cat_error_module_shutdown();

    CAT_GLOBALS_UNREGISTER(cat);
    CAT_GLOBALS_MODULE_SHUTDOWN();

    return cat_true;
}

CAT_API cat_bool_t cat_runtime_init(void)
{
    CAT_GLOBALS_RUNTIME_INIT();

    srand((unsigned int) time(NULL));

    cat_const_string_init(&CAT_G(exepath));

    CAT_LOG_G(types) = CAT_LOG_TYPES_DEFAULT;
    CAT_LOG_G(error_output) = stderr;
#ifdef CAT_SOURCE_POSITION
    CAT_LOG_G(show_source_postion) = cat_false;
#endif
    CAT_G(show_last_error) = cat_false;
    memset(&CAT_G(last_error), 0, sizeof(CAT_G(last_error)));

    /* error log */
    if (!cat_env_is_empty("CAT_LOG_ERROR_OUTPUT")) {
        char *error_log = cat_env_get_silent("CAT_LOG_ERROR_OUTPUT", NULL);
        if (cat_strcasecmp(error_log, "stdout") == 0) {
            CAT_LOG_G(error_output) = stdout;
        } else if (cat_strcasecmp(error_log, "stderr") == 0) {
            CAT_LOG_G(error_output) = stderr;
        }  /* TODO: log file support */
        cat_free(error_log);
    }
    /* log str size */
    CAT_LOG_G(str_size) = (size_t) cat_env_get_i("CAT_LOG_STR_SIZE", 32);
    /* log module name filter */
    if (!cat_env_is_empty("CAT_LOG_MODULE_NAME_FILTER")) {
        CAT_LOG_G(module_name_filter) = cat_env_get_silent("CAT_LOG_MODULE_NAME_FILTER", NULL);
    } else {
        CAT_LOG_G(module_name_filter) = NULL;
    }
    CAT_LOG_G(show_timestamps) = (unsigned int) cat_env_get_i("CAT_LOG_SHOW_TIMESTAMPS", 0);
    CAT_LOG_G(timestamps_format) = "%F %T";
    CAT_LOG_G(show_timestamps_as_relative) = cat_false;
    do {
        char *timestamps_format = cat_env_get_silent("CAT_LOG_TIMESTAMPS_FORMAT", NULL);
        if (timestamps_format != NULL) {
            if (cat_strcasecmp(timestamps_format, "time") == 0) {
                CAT_LOG_G(timestamps_format) = "%T";
            } else if (cat_strcasecmp(timestamps_format, "unix") == 0) {
                CAT_LOG_G(timestamps_format) = "%s";
            }
        } else {
            cat_free(timestamps_format);
        }
    } while (0);
    if (cat_env_is_true("CAT_LOG_SHOW_TIMESTAMPS_AS_RELATIVE", cat_false)) {
        CAT_LOG_G(show_timestamps_as_relative) = cat_true;
    }
#ifdef CAT_DEBUG
    CAT_LOG_G(last_debug_log_level) = 0;
    CAT_LOG_G(debug_level) = (unsigned int) cat_env_get_i("CAT_DEBUG", 0);
    if (CAT_LOG_G(debug_level) > 0) {
        /* enable all log types and log module types */
        CAT_LOG_G(types) = CAT_LOG_TYPES_ALL;
        /* enable SLE if there is no env to set it explicitly */
        if (!cat_env_exists("CAT_SHOW_LAST_ERROR")) {
            CAT_G(show_last_error) = cat_true;
        }
    }
# ifdef CAT_SOURCE_POSITION
    /* show source position */
    if (cat_env_is_true("CAT_LOG_SHOW_SOURCE_POSITION", cat_false)) {
        CAT_LOG_G(show_source_postion) = cat_true;
    }
# endif
#endif
    /* show last error */
    if (cat_env_is_true("CAT_SHOW_LAST_ERROR", cat_false)) {
        CAT_G(show_last_error) = cat_true;
    }

    CAT_G(runtime) = cat_true;

    return cat_true;
}

CAT_API cat_bool_t cat_runtime_shutdown(void)
{
    cat_clear_last_error();

    if (CAT_LOG_G(module_name_filter) != NULL) {
        cat_free(CAT_LOG_G(module_name_filter));
    }
    if (CAT_G(exepath).data != NULL) {
        cat_free((void *) CAT_G(exepath).data);
    }
    CAT_G(runtime) = cat_false;

    return cat_true;
}

CAT_API cat_bool_t cat_runtime_close(void)
{
    CAT_GLOBALS_RUNTIME_CLOSE();

    return cat_true;
}

CAT_API char **cat_setup_args(int argc, char** argv)
{
    if (cat_args_registered) {
        CAT_CORE_ERROR(PROCESS, "API misuse: setup_args() should be called only once");
    }
    cat_args_registered = cat_true;

    return uv_setup_args(argc, argv);
}

CAT_API const cat_const_string_t *cat_exepath(void)
{
    cat_const_string_t *exepath = &CAT_G(exepath);

    if (exepath->data == NULL) {
        char *buffer;
        size_t buffer_size;
        int error;

        buffer = (char *) cat_malloc(CAT_EXEPATH_MAX);
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(buffer == NULL)) {
            cat_update_last_error_of_syscall("Malloc for exepath failed");
            return NULL;
        }
#endif
        buffer_size = CAT_EXEPATH_MAX;

        error = uv_exepath(buffer, &buffer_size);

        if (unlikely(error != 0)) {
            cat_update_last_error_with_reason(error, "Executable path get failed");
            cat_free(buffer);
            return NULL;
        }

        exepath->data = buffer;
        exepath->length = buffer_size;
    }

    return exepath;
}

CAT_API char *cat_get_process_title(char* buffer, size_t size)
{
    cat_bool_t allocated = cat_false;
    int error;

    if (buffer == NULL) {
        if (size == 0) {
            size = CAT_PROCESS_TITLE_BUFFER_SIZE;
        }
        buffer = (char *) cat_malloc(size);
        if (unlikely(buffer == NULL)) {
            cat_update_last_error_of_syscall("Malloc for process title failed");
            return NULL;
        }
        allocated = cat_true;
    }

    error = uv_get_process_title(buffer, size);

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Process get title failed");
        if (allocated) {
            cat_free(buffer);
        }
        return NULL;
    }

    return buffer;
}

CAT_API cat_bool_t cat_set_process_title(const char* title)
{
    int error;

    error = uv_set_process_title(title);

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Process set title failed");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_is_main_thread(void)
{
    uv_thread_t current_thread = uv_thread_self();
    return !!uv_thread_equal(&cat_main_thread_tid, &current_thread);
}
