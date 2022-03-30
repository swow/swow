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

#include "cat_api.h"

CAT_API cat_bool_t cat_init_all(void)
{
    return cat_module_init_all() &&
           cat_runtime_init_all();
}

CAT_API cat_bool_t cat_shutdown_all(void)
{
    cat_bool_t ret = cat_true;

    ret = cat_runtime_shutdown_all();
    ret = cat_runtime_close_all() && ret;
    ret = cat_module_shutdown_all() && ret;

    return  ret;
}

CAT_API cat_bool_t cat_module_init_all(void)
{
    return cat_module_init() &&
           cat_coroutine_module_init() &&
           cat_event_module_init() &&
           cat_buffer_module_init() &&
#ifdef CAT_SSL
           cat_ssl_module_init() &&
#endif
           cat_socket_module_init() &&
#ifdef CAT_OS_WAIT
           cat_os_wait_module_init() &&
#endif
           cat_watchdog_module_init() &&
           cat_true;
}

CAT_API cat_bool_t cat_module_shutdown_all(void)
{
    cat_bool_t ret = cat_true;

    ret = cat_module_shutdown() && ret;

    return ret;
}

CAT_API cat_bool_t cat_runtime_init_all(void)
{
    return cat_runtime_init() &&
           cat_coroutine_runtime_init() &&
           cat_event_runtime_init() &&
           cat_socket_runtime_init() &&
#ifdef CAT_OS_WAIT
           cat_os_wait_runtime_init() &&
#endif
           cat_watchdog_runtime_init() &&
           cat_true;
}

CAT_API cat_bool_t cat_runtime_shutdown_all(void)
{
    cat_bool_t ret = cat_true;

    ret = cat_watchdog_runtime_shutdown() && ret;
#ifdef CAT_OS_WAIT
    ret = cat_os_wait_runtime_shutdown() && ret;
#endif
    ret = cat_event_runtime_shutdown() && ret;
    ret = cat_coroutine_runtime_shutdown() && ret;
    ret = cat_runtime_shutdown() && ret;

    return ret;
}

CAT_API cat_bool_t cat_runtime_close_all(void)
{
    cat_bool_t ret = cat_true;

    ret = cat_event_runtime_close() && ret;

    return ret;
}

CAT_API cat_bool_t cat_run(cat_run_mode_t run_mode)
{
    switch (run_mode) {
        case CAT_RUN_EASY: {
            return cat_event_scheduler_run(NULL) != NULL;
        }
    }
    CAT_NEVER_HERE("Unknown run mode");
}

CAT_API cat_bool_t cat_stop(void)
{
    return cat_event_scheduler_close() != NULL;
}

#ifdef CAT_DEBUG
CAT_API void cat_enable_debug_mode(void)
{
    CAT_G(log_types) = CAT_LOG_TYPES_ALL;
    CAT_G(log_module_types) = CAT_MODULE_TYPES_ALL;
}
#endif

CAT_API FILE *cat_get_error_log(void)
{
    return CAT_G(error_log);
}

CAT_API void cat_set_error_log(FILE *file)
{
    CAT_G(error_log) = file;
}
