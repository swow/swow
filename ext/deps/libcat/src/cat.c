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

    CAT_G(runtime) = cat_false;

    return cat_true;
}
