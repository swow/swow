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
#include "cat_coroutine.h" /* for coroutine id (TODO: need to decouple it?) */

CAT_API cat_log_t cat_log_function;

#if 0
static const char *cat_log_get_date()
{
    static char date[24];
    time_t raw_time;
    struct tm *info;

    time(&raw_time);
    info = localtime(&raw_time);
    strftime(date, 24, "%Y-%m-%d %H:%M:%S", info);

    return date;
}
#endif

CAT_API void cat_log_standard(CAT_LOG_PARAMATERS)
{
    const char *type_string;
    char *message;
    FILE *output;
    (void) module_type;

    switch (type) {
        case CAT_LOG_TYPE_DEBUG : {
#ifndef CAT_ENABLE_DEBUG_LOG
            return;
#else
            type_string = "Debug";
            output = stdout;
            break;
#endif
        }
        case CAT_LOG_TYPE_INFO : {
            type_string = "Info";
            output = stdout;
            break;
        }
        case CAT_LOG_TYPE_NOTICE : {
            type_string = "Notice";
            output = CAT_G(error_log);
            break;
        }
        case CAT_LOG_TYPE_WARNING : {
            type_string = "Warning";
            output = CAT_G(error_log);
            break;
        }
        case CAT_LOG_TYPE_ERROR : {
            type_string = "Error";
            output = CAT_G(error_log);
            break;
        }
        case CAT_LOG_TYPE_CORE_ERROR : {
            type_string = "Core Error";
            output = CAT_G(error_log);
            break;
        }
        default:
            CAT_NEVER_HERE("Unknown log type");
    }

    do {
        va_list args;
        va_start(args, format);
        message = cat_vsprintf(format, args);
        if (unlikely(message == NULL)) {
            fprintf(CAT_G(error_log), "Sprintf log message failed" CAT_EOL);
            return;
        }
        va_end(args);
    } while (0);

    do {
        const char *name = cat_coroutine_get_current_role_name();
        if (name != NULL) {
            fprintf(
                output,
                "%s: <%s> %s in %s" CAT_EOL,
                type_string, module_name, message, name
            );
        } else {
            fprintf(
                output,
                "%s: <%s> %s in R" CAT_COROUTINE_ID_FMT CAT_EOL,
                type_string, module_name, message, cat_coroutine_get_current_id()
            );
        }
    } while (0);
#ifdef CAT_SOURCE_POSITION
    if (CAT_G(log_source_postion)) {
        fprintf(
            output,
            "SP: " CAT_SOURCE_POSITION_FMT CAT_EOL,
            CAT_SOURCE_POSITION_RELAY_C
        );
    }
#endif

    if (type & CAT_LOG_TYPES_ABNORMAL) {
        cat_set_last_error(code, message);
    } else {
        cat_free(message);
    }

    if (type & (CAT_LOG_TYPE_ERROR | CAT_LOG_TYPE_CORE_ERROR)) {
        cat_abort();
    }
}

CAT_API const char *cat_log_buffer_quote(const char *buffer, ssize_t n, char **tmp_str)
{
    return cat_log_buffer_quote_unlimited(buffer, CAT_MIN((size_t) n, CAT_G(log_str_size)), tmp_str);
}

CAT_API const char *cat_log_buffer_quote_unlimited(const char *buffer, ssize_t n, char **tmp_str)
{
    if (n > 0) {
        char *quoted_data;
        cat_str_quote(buffer, n, &quoted_data, NULL);
        *tmp_str = quoted_data;
        return quoted_data;
    }

    return *tmp_str = cat_sprintf("%p", buffer);
}
