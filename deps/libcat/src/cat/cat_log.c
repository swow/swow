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

CAT_API cat_log_t cat_log;

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

    do {
        va_list args;
        va_start(args, format);
        message = cat_vsprintf(format, args);
        if (unlikely(message == NULL)) {
            fprintf(stderr, "Sprintf log message failed" CAT_EOL);
            return;
        }
        va_end(args);
    } while (0);

    switch (type)
    {
#ifdef CAT_DEBUG
        case CAT_LOG_TYPE_DEBUG : {
            type_string = "Debug";
            output = stdout;
            break;
        }
#endif
        case CAT_LOG_TYPE_INFO : {
            type_string = "Info";
            output = stdout;
            break;
        }
        case CAT_LOG_TYPE_NOTICE : {
            type_string = "Notice";
            output = stderr;
            break;
        }
        case CAT_LOG_TYPE_WARNING : {
            type_string = "Warning";
            output = stderr;
            break;
        }
        case CAT_LOG_TYPE_ERROR : {
            type_string = "Error";
            output = stderr;
            break;
        }
        case CAT_LOG_TYPE_CORE_ERROR : {
            type_string = "Core Error";
            output = stderr;
            break;
        }
        default:
            CAT_NEVER_HERE("Unknown log type");
    }

    fprintf(
        output,
        "%s: <%s> %s in R" CAT_COROUTINE_ID_FMT CAT_EOL,
        type_string, module_name, message, CAT_COROUTINE_G(current)->id
    );
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
