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

#if defined(CAT_DEBUG) && !defined(CAT_DISABLE_DEBUG_LOG)
#define CAT_ENABLE_DEBUG_LOG 1
#endif

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

#define CAT_LOG_STRING_OR_X_PARAM(string, x) \
        (string != NULL ? "\"" : ""), (string != NULL ? string : x), (string != NULL ? "\"" : "")

#define CAT_LOG_STRINGL_OR_X_PARAM(string, length, x) \
        ((string != NULL && length > 0) ? "\"" : ""), (int) ((string != NULL && length > 0) ? length : strlen(x)), ((string != NULL && length > 0) ? string : x), ((string != NULL && length > 0) ? "\"" : "")

#define CAT_LOG_STRING_OR_NULL_FMT "%s%s%s"
#define CAT_LOG_STRING_OR_NULL_PARAM(string) \
        CAT_LOG_STRING_OR_X_PARAM(string, "NULL")

#define CAT_LOG_STRINGL_OR_NULL_FMT "%s%.*s%s"
#define CAT_LOG_STRINGL_OR_NULL_PARAM(string, length) \
        CAT_LOG_STRINGL_OR_X_PARAM(string, length, "NULL")

/* Notes for log macros:
 * [XXX_WITH_TYPE] macros designed for dynamic types,
 * [XXX_D] macros means log directly without checking user config */

#define CAT_LOG_SCOPE_WITH_TYPE_START(type, module_type) do { \
    if (( \
            (((type) & CAT_G(log_types)) == (type)) && \
            ((CAT_MODULE_TYPE_##module_type & CAT_G(log_module_types)) == CAT_MODULE_TYPE_##module_type) \
        ) || \
        unlikely(((type) & CAT_LOG_TYPES_UNFILTERABLE) == (type)) \
    ) {

#define CAT_LOG_SCOPE_WITH_TYPE_END() \
    } \
} while (0)

#define CAT_LOG_SCOPE_START(type, module_type) \
        CAT_LOG_SCOPE_WITH_TYPE_START(CAT_LOG_TYPE_##type, module_type)

#define CAT_LOG_SCOPE_END CAT_LOG_SCOPE_WITH_TYPE_END

#define CAT_LOG_D_WITH_TYPE(type, module_type, code, format, ...) \
        cat_log_function( \
            type, CAT_MODULE_TYPE_##module_type, #module_type \
            CAT_SOURCE_POSITION_CC, code, format, ##__VA_ARGS__ \
        )

#define CAT_LOG_D(type, module_type, code, format, ...) \
        CAT_LOG_D_WITH_TYPE( \
            CAT_LOG_TYPE_##type, module_type, \
            code, format, ##__VA_ARGS__ \
        )

#define CAT_LOG_WITH_TYPE(type, module_type, code, format, ...) \
        CAT_LOG_SCOPE_WITH_TYPE_START(type, module_type) { \
            CAT_LOG_D_WITH_TYPE(type, module_type, code, format, ##__VA_ARGS__); \
        } CAT_LOG_SCOPE_END()

#define CAT_LOG(type, module_type, code, format, ...) \
        CAT_LOG_WITH_TYPE(CAT_LOG_TYPE_##type, module_type, code, format, ##__VA_ARGS__)

#define CAT_LOG_V(type, module_type, code, ...) \
        CAT_LOG(type, module_type, code, ##__VA_ARGS__) /* make MSVC happy */

#define CAT_LOG_NORETURN(type, module_type, code, format, ...) do { \
        CAT_LOG_D_WITH_TYPE(CAT_LOG_TYPE_##type, module_type, code, format, ##__VA_ARGS__); \
        cat_abort(); /* make static analysis happy */ \
} while (0)

#define CAT_LOG_NORETURN_V(type, module_type, code, ...) \
        CAT_LOG_NORETURN(type, module_type, code, ##__VA_ARGS__) /* make MSVC happy */

#define CAT_LOG_WITH_REASON(type, module_type, code, reason, format, ...) \
        CAT_LOG_V(type, module_type, code, format ", reason: %s", reason, ##__VA_ARGS__)

#define CAT_LOG_WITH_REASON_NORETURN(type, module_type, code, reason, format, ...) \
        CAT_LOG_NORETURN_V(type, module_type, code, format ", reason: %s", reason, ##__VA_ARGS__)

#ifndef CAT_ENABLE_DEBUG_LOG
#define CAT_LOG_DEBUG_SCOPE_START(module_type)
#define CAT_LOG_DEBUG_SCOPE_END()
#define CAT_LOG_DEBUG_SCOPE_START_WITH_LEVEL(module_type, level)
#define CAT_LOG_DEBUG_SCOPE_END_WITH_LEVEL()
#define CAT_LOG_DEBUG_SCOPE_START_EX(module_type, pre)
#define CAT_LOG_DEBUG_SCOPE_END_EX(end)
#define CAT_LOG_DEBUG_SCOPE_START_WITH_LEVEL_EX(module_type, level, pre)
#define CAT_LOG_DEBUG_SCOPE_END_WITH_LEVEL_EX(end)
#define CAT_LOG_DEBUG(module_type, format, ...)
#define CAT_LOG_DEBUG_V2(module_type, format, ...)
#define CAT_LOG_DEBUG_V3(module_type, format, ...)
#define CAT_LOG_DEBUG_WITH_LEVEL(module_type, level, format, ...)
#define CAT_LOG_DEBUG_D(module_type, format, ...)
#else
#define CAT_LOG_DEBUG_LEVEL_SCOPE_START(level) do { \
    if (CAT_G(log_debug_level) >= level) {

#define CAT_LOG_DEBUG_LEVEL_SCOPE_END() \
    } \
} while (0)

#define CAT_LOG_DEBUG_SCOPE_START(module_type) \
        CAT_LOG_DEBUG_SCOPE_START_WITH_LEVEL(module_type, 1)

#define CAT_LOG_DEBUG_SCOPE_END() \
        CAT_LOG_DEBUG_SCOPE_END_WITH_LEVEL()

#define CAT_LOG_DEBUG_SCOPE_START_WITH_LEVEL(module_type, level) \
        CAT_LOG_DEBUG_LEVEL_SCOPE_START(level) { \
            CAT_LOG_SCOPE_START(DEBUG, module_type) {

#define CAT_LOG_DEBUG_SCOPE_END_WITH_LEVEL() \
            } CAT_LOG_SCOPE_END(); \
        } CAT_LOG_DEBUG_LEVEL_SCOPE_END()

#define CAT_LOG_DEBUG_SCOPE_START_EX(module_type, pre) \
        CAT_LOG_DEBUG_SCOPE_START_WITH_LEVEL_EX(module_type, 1, pre)

#define CAT_LOG_DEBUG_SCOPE_END_EX(end) \
        CAT_LOG_DEBUG_SCOPE_END_WITH_LEVEL_EX(end)

#define CAT_LOG_DEBUG_SCOPE_START_WITH_LEVEL_EX(module_type, level, pre) \
        CAT_LOG_DEBUG_LEVEL_SCOPE_START(level) { \
            CAT_LOG_SCOPE_START(DEBUG, module_type) { pre;

#define CAT_LOG_DEBUG_SCOPE_END_WITH_LEVEL_EX(end) \
            end; } CAT_LOG_SCOPE_END(); \
        } CAT_LOG_DEBUG_LEVEL_SCOPE_END()

#define CAT_LOG_DEBUG(module_type, format, ...) \
        CAT_LOG_DEBUG_WITH_LEVEL(module_type, 1, format, ##__VA_ARGS__)

#define CAT_LOG_DEBUG_V2(module_type, format, ...) \
        CAT_LOG_DEBUG_WITH_LEVEL(module_type, 2, format, ##__VA_ARGS__)

#define CAT_LOG_DEBUG_V3(module_type, format, ...) \
        CAT_LOG_DEBUG_WITH_LEVEL(module_type, 3, format, ##__VA_ARGS__)

#define CAT_LOG_DEBUG_WITH_LEVEL(module_type, level, format, ...) \
        CAT_LOG_DEBUG_LEVEL_SCOPE_START(level) { \
            CAT_LOG(DEBUG, module_type, 0, format, ##__VA_ARGS__); \
        } CAT_LOG_DEBUG_LEVEL_SCOPE_END()

#define CAT_LOG_DEBUG_D(module_type, format, ...) \
        CAT_LOG_D(DEBUG, module_type, 0, format, ##__VA_ARGS__)
#endif /* CAT_ENABLE_DEBUG_LOG */

#define CAT_LOG_INFO(module_type, format, ...)                     CAT_LOG(INFO, module_type, 0, format, ##__VA_ARGS__)

#define CAT_NOTICE(module_type, format, ...)                       CAT_NOTICE_EX(module_type, CAT_UNCODED, format, ##__VA_ARGS__)
#define CAT_NOTICE_EX(module_type, code, format, ...)              CAT_LOG(NOTICE, module_type, code, format, ##__VA_ARGS__)
#define CAT_NOTICE_WITH_LAST(module_type, format, ...)             CAT_LOG_WITH_REASON(NOTICE, module_type, cat_get_last_error_code(), cat_get_last_error_message(), format, ##__VA_ARGS__)
#define CAT_NOTICE_WITH_REASON(module_type, code, format, ...)     CAT_LOG_WITH_REASON(NOTICE, module_type, code, cat_strerror(code), format, ##__VA_ARGS__)

#define CAT_WARN(module_type, format, ...)                         CAT_WARN_EX(module_type, CAT_UNCODED, format, ##__VA_ARGS__)
#define CAT_WARN_EX(module_type, code, format, ...)                CAT_LOG(WARNING, module_type, code, format, ##__VA_ARGS__)
#define CAT_WARN_WITH_LAST(module_type, format, ...)               CAT_LOG_WITH_REASON(WARNING, module_type, cat_get_last_error_code(), cat_get_last_error_message(), format, ##__VA_ARGS__)
#define CAT_WARN_WITH_REASON(module_type, code, format, ...)       CAT_LOG_WITH_REASON(WARNING, module_type, code, cat_strerror(code), format, ##__VA_ARGS__)

#define CAT_ERROR(module_type, format, ...)                        CAT_ERROR_EX(module_type, CAT_UNCODED, format, ##__VA_ARGS__)
#define CAT_ERROR_EX(module_type, code, format, ...)               CAT_LOG_NORETURN(ERROR, module_type, code, format, ##__VA_ARGS__)
#define CAT_ERROR_WITH_LAST(module_type, format, ...)              CAT_LOG_WITH_REASON_NORETURN(ERROR, module_type, cat_get_last_error_code(), cat_get_last_error_message(), format, ##__VA_ARGS__)
#define CAT_ERROR_WITH_REASON(module_type, code, format, ...)      CAT_LOG_WITH_REASON_NORETURN(ERROR, module_type, code, cat_strerror(code), format, ##__VA_ARGS__)

#define CAT_CORE_ERROR(module_type, format, ...)                   CAT_CORE_ERROR_EX(module_type, CAT_UNCODED, format, ##__VA_ARGS__)
#define CAT_CORE_ERROR_EX(module_type, code, format, ...)          CAT_LOG_NORETURN(CORE_ERROR, module_type, code, format, ##__VA_ARGS__); abort() /* make IDE happy */
#define CAT_CORE_ERROR_WITH_LAST(module_type, format, ...)         CAT_LOG_WITH_REASON_NORETURN(CORE_ERROR, module_type, cat_get_last_error_code(), cat_get_last_error_message(), format, ##__VA_ARGS__); abort() /* make IDE happy */
#define CAT_CORE_ERROR_WITH_REASON(module_type, code, format, ...) CAT_LOG_WITH_REASON_NORETURN(CORE_ERROR, module_type, code, cat_strerror(code), format, ##__VA_ARGS__); abort() /* make IDE happy */

#define CAT_SYSCALL_FAILURE(type, module_type, format, ...) do { \
    cat_errno_t _error = cat_translate_sys_error(cat_sys_errno); \
    CAT_LOG_V(type, module_type, _error, format " (syscall failure " CAT_ERRNO_FMT ": %s)", ##__VA_ARGS__, _error, cat_strerror(_error)); \
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

extern CAT_API cat_log_t cat_log_function CAT_LOG_ATTRIBUTES;

CAT_API void cat_log_standard(CAT_LOG_PARAMATERS);

/* Notice: n will be limited to CAT_G(log_str_size) if it exceed CAT_G(log_str_size) */
CAT_API const char *cat_log_buffer_quote(const char *buffer, ssize_t n, char **tmp_str); CAT_FREE
/* Notice: n will not be limited anyway */
CAT_API const char *cat_log_buffer_quote_unlimited(const char *buffer, ssize_t n, char **tmp_str); CAT_FREE
