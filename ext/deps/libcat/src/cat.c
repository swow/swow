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

CAT_API CAT_GLOBALS_DECLARE(cat)

CAT_GLOBALS_CTOR_DECLARE_SZ(cat)

static cat_bool_t cat_args_registered = cat_false;

#ifndef CAT_OS_WIN
#define CAT_EXEPATH_MAX (PATH_MAX + PATH_MAX + 1)
#else
#define CAT_EXEPATH_MAX 32768
#endif

CAT_API cat_bool_t cat_module_init(void)
{
    cat_log = cat_log_standard;

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

    CAT_GLOBALS_REGISTER(cat, CAT_GLOBALS_CTOR(cat), NULL);

    return cat_true;
}

CAT_API cat_bool_t cat_module_shutdown(void)
{
    return cat_true;
}

CAT_API cat_bool_t cat_runtime_init(void)
{
    srand((unsigned int) time(NULL));

    CAT_G(log_types) = CAT_LOG_TYPES_DEFAULT;
    CAT_G(log_module_types) = CAT_MODULE_TYPES_ALL;
    CAT_G(error_log) = stderr;
    cat_const_string_init(&CAT_G(exepath));
#ifdef CAT_SOURCE_POSITION
    CAT_G(log_source_postion) = cat_false;
#endif
#ifdef CAT_DEBUG
    CAT_G(show_last_error) = cat_false;
#endif
    memset(&CAT_G(last_error), 0, sizeof(CAT_G(last_error)));

    /* error log */
    if (cat_env_exists("CAT_ERROR_LOG")) {
        char *error_log = cat_env_get("CAT_ERROR_LOG");
        if (cat_strcasecmp(error_log, "stdout") == 0) {
            CAT_G(error_log) = stdout;
        } else if (cat_strcasecmp(error_log, "stderr") == 0) {
            CAT_G(error_log) = stderr;
        }  /* TODO: log file support */
        cat_free(error_log);
    }
#ifdef CAT_DEBUG
do {
    /* enable all log types and log module types */
    if (cat_env_is_true("CAT_DEBUG", cat_false)) {
        CAT_G(log_types) = CAT_LOG_TYPES_ALL;
        CAT_G(log_module_types) = CAT_MODULE_TYPES_ALL;
    }
#ifdef CAT_SOURCE_POSITION
    /* show source position */
    if (cat_env_is_true("CAT_SP", cat_false)) {
        CAT_G(log_source_postion) = cat_true;
    }
#endif
    /* show last error */
    if (cat_env_is_true("CAT_SLE", cat_false)) {
        CAT_G(show_last_error) = cat_true;
    }
} while (0);
#endif

    CAT_G(runtime) = cat_true;

    return cat_true;
}

CAT_API cat_bool_t cat_runtime_shutdown(void)
{
    cat_clear_last_error();

    if (CAT_G(exepath).data != NULL) {
        cat_free((void *) CAT_G(exepath).data);
    }
    CAT_G(runtime) = cat_false;

    return cat_true;
}

CAT_API char **cat_setup_args(int argc, char** argv)
{
    if (cat_args_registered) {
        cat_core_error(PROCESS, "API misuse: setup_args() should be called only once");
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
