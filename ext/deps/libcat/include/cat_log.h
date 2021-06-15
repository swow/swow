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

typedef enum cat_log_type_e {
    CAT_LOG_TYPE_DEBUG           = 1 << 0,
    CAT_LOG_TYPE_INFO            = 1 << 1,
    CAT_LOG_TYPE_NOTICE          = 1 << 2,
    CAT_LOG_TYPE_WARNING         = 1 << 3,
    CAT_LOG_TYPE_ERROR           = 1 << 4,
    CAT_LOG_TYPE_CORE_ERROR      = 1 << 5,
} cat_log_type_t;

typedef uint16_t cat_log_types_t;

typedef enum cat_log_union_types_e {
    CAT_LOG_TYPES_ALL             = CAT_LOG_TYPE_DEBUG | CAT_LOG_TYPE_INFO | CAT_LOG_TYPE_NOTICE | CAT_LOG_TYPE_WARNING | CAT_LOG_TYPE_ERROR | CAT_LOG_TYPE_CORE_ERROR,
    CAT_LOG_TYPES_DEFAULT         = CAT_LOG_TYPES_ALL ^ CAT_LOG_TYPE_DEBUG,
    CAT_LOG_TYPES_ABNORMAL        = CAT_LOG_TYPE_NOTICE | CAT_LOG_TYPE_WARNING | CAT_LOG_TYPE_ERROR | CAT_LOG_TYPE_CORE_ERROR,
    CAT_LOG_TYPES_UNFILTERABLE    = CAT_LOG_TYPE_ERROR | CAT_LOG_TYPE_CORE_ERROR,
} cat_log_union_types_t;

#define cat_log_with_type(type, module_type, code, format, ...) do { \
    if (( \
            (((type) & CAT_G(log_types)) == (type)) && \
            ((CAT_MODULE_TYPE_##module_type & CAT_G(log_module_types)) == CAT_MODULE_TYPE_##module_type) \
        ) || \
        unlikely(((type) & CAT_LOG_TYPES_UNFILTERABLE) == (type)) \
    ) { \
        cat_log( \
            (type), CAT_MODULE_TYPE_##module_type, #module_type \
            CAT_SOURCE_POSITION_CC, code, format, ##__VA_ARGS__ \
        ); \
    } \
} while (0)

#define cat_log_helper(type, module_type, code, format, ...) \
        cat_log_with_type(CAT_LOG_TYPE_##type, module_type, code, format, ##__VA_ARGS__)

/* make MSVC happy */
#define cat_log_helper_with_reason(type, module_type, code, reason, format, ...) \
        cat_log_helper__with_reason(type, module_type, code, format ", reason: %s", reason, ##__VA_ARGS__ )

#define cat_log_helper__with_reason(type, module_type, code, ...) \
        cat_log_helper(type, module_type, code, ##__VA_ARGS__)

#ifndef CAT_DEBUG
#define cat_debug(module_type, format, ...)
#else
#define cat_debug(module_type, format, ...)                           cat_log_helper(DEBUG, module_type, 0, format, ##__VA_ARGS__);
#endif

#define cat_info(module_type, format, ...)                            cat_log_helper(INFO, module_type, 0, format, ##__VA_ARGS__)

#define cat_notice(module_type, format, ...)                          cat_notice_ex(module_type, CAT_UNCODED, format, ##__VA_ARGS__)
#define cat_notice_ex(module_type, code, format, ...)                 cat_log_helper(NOTICE, module_type, code, format, ##__VA_ARGS__)
#define cat_notice_with_last(module_type, format, ...)                cat_log_helper_with_reason(NOTICE, module_type, cat_get_last_error_code(), cat_get_last_error_message(), format, ##__VA_ARGS__)
#define cat_notice_with_reason(module_type, code, format, ...)        cat_log_helper_with_reason(NOTICE, module_type, code, cat_strerror(code), format, ##__VA_ARGS__)

#define cat_warn(module_type, format, ...)                            cat_warn_ex(module_type, CAT_UNCODED, format, ##__VA_ARGS__)
#define cat_warn_ex(module_type, code, format, ...)                   cat_log_helper(WARNING, module_type, code, format, ##__VA_ARGS__)
#define cat_warn_with_last(module_type, format, ...)                  cat_log_helper_with_reason(WARNING, module_type, cat_get_last_error_code(), cat_get_last_error_message(), format, ##__VA_ARGS__)
#define cat_warn_with_reason(module_type, code, format, ...)          cat_log_helper_with_reason(WARNING, module_type, code, cat_strerror(code), format, ##__VA_ARGS__)

#define cat_error(module_type, format, ...)                           cat_error_ex(module_type, CAT_UNCODED, format, ##__VA_ARGS__)
#define cat_error_ex(module_type, code, format, ...)                  cat_log_helper(ERROR, module_type, code, format, ##__VA_ARGS__)
#define cat_error_with_last(module_type, format, ...)                 cat_log_helper_with_reason(ERROR, module_type, cat_get_last_error_code(), cat_get_last_error_message(), format, ##__VA_ARGS__)
#define cat_error_with_reason(module_type, code, format, ...)         cat_log_helper_with_reason(ERROR, module_type, code, cat_strerror(code), format, ##__VA_ARGS__)

#define cat_core_error(module_type, format, ...)                      cat_core_error_ex(module_type, CAT_UNCODED, format, ##__VA_ARGS__)
#define cat_core_error_ex(module_type, code, format, ...)             cat_log_helper(CORE_ERROR, module_type, code, format, ##__VA_ARGS__); abort() /* make IDE happy */
#define cat_core_error_with_last(module_type, format, ...)            cat_log_helper_with_reason(CORE_ERROR, module_type, cat_get_last_error_code(), cat_get_last_error_message(), format, ##__VA_ARGS__); abort() /* make IDE happy */
#define cat_core_error_with_reason(module_type, code, format, ...)    cat_log_helper_with_reason(CORE_ERROR, module_type, code, cat_strerror(code), format, ##__VA_ARGS__); abort() /* make IDE happy */

#define cat_syscall_failure(type, module_type, format, ...) do { \
    cat_errno_t _error = cat_translate_sys_error(cat_sys_errno); \
    cat_log_helper(type, module_type, _error, format " (syscall failure " CAT_ERRNO_FMT ": %s)", ##__VA_ARGS__, _error, cat_strerror(_error)); \
} while (0)

#define CAT_LOG_PARAMATERS \
    cat_log_type_t type, const cat_module_type_t module_type, const char *module_name \
    CAT_SOURCE_POSITION_DC, int code, const char *format, ...

#ifdef CAT_SOURCE_POSITION
#ifndef CAT_DISABLE___FUNCTION__
#define CAT_LOG_ATTRIBUTES CAT_ATTRIBUTE_PTR_FORMAT(printf, 8, 9)
#else
#define CAT_LOG_ATTRIBUTES CAT_ATTRIBUTE_PTR_FORMAT(printf, 7, 8)
#endif
#else
#define CAT_LOG_ATTRIBUTES CAT_ATTRIBUTE_PTR_FORMAT(printf, 5, 6)
#endif

typedef void (*cat_log_t)(CAT_LOG_PARAMATERS);

extern CAT_API cat_log_t cat_log CAT_LOG_ATTRIBUTES;

CAT_API void cat_log_standard(CAT_LOG_PARAMATERS);
