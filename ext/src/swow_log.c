/*
  +--------------------------------------------------------------------------+
  | Swow                                                                     |
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

#include "swow_log.h"
#include "swow_coroutine.h" /* for coroutine id (TODO: need to decouple it?) */

#include "SAPI.h"

SWOW_API zend_class_entry *swow_log_ce;

static void swow_log_va_standard(CAT_LOG_VA_PARAMETERS)
{
#ifndef CAT_ENABLE_DEBUG_LOG
    if (type == CAT_LOG_TYPE_DEBUG) {
        return;
    }
#endif

    /* Zend put()/error() may call PHP callbacks,
     * we can not do it in a pure C or interned coroutine */
    bool has_executor = swow_coroutine_get_current()->coroutine.flags & SWOW_COROUTINE_FLAG_HAS_EXECUTOR;
    if (!has_executor) {
        if (!(type & CAT_LOG_TYPES_ABNORMAL)) {
            cat_log_va_standard(type, module_name CAT_SOURCE_POSITION_RELAY_CC, code, format, args);
        } else {
            smart_str str;
            memset(&str, 0, sizeof(str));
            switch (type) {
                case CAT_LOG_TYPE_NOTICE: {
                    smart_str_appends(&str, "Notice");
                    break;
                }
                case CAT_LOG_TYPE_WARNING: {
                    smart_str_appends(&str, "Warning");
                    break;
                }
                case CAT_LOG_TYPE_ERROR: {
                    smart_str_appends(&str, "Error");
                    break;
                }
                case CAT_LOG_TYPE_CORE_ERROR: {
                    smart_str_appends(&str, "Core Error");
                    break;
                }
                default:
                    CAT_NEVER_HERE("Unknown log type");
            }
            smart_str_appends(&str, ": ");
            php_printf_to_smart_str(&str, format, args);
            smart_str_appends(&str, " in ");
            do {
                const char *name = cat_coroutine_get_current_role_name();
                if (name == NULL) {
                    smart_str_appendc(&str, 'R');
                    smart_str_append_long(&str, cat_coroutine_get_current_id());
                } else {
                    smart_str_appends(&str, name);
                }
            } while (0);
            smart_str_appends(&str, "\n");
            smart_str_0(&str);
            cat_set_last_error(code, ZSTR_VAL(str.s));
            /* Write CLI/CGI errors to stderr if display_errors = "stderr" */
            if ((!strcmp(sapi_module.name, "cli") ||
                 !strcmp(sapi_module.name, "micro") ||
                 !strcmp(sapi_module.name, "cgi") ||
                 !strcmp(sapi_module.name, "phpdbg")) &&
                PG(display_errors) == PHP_DISPLAY_ERRORS_STDERR
            ) {
                cat_log_fwrite(stderr, ZSTR_VAL(str.s), ZSTR_LEN(str.s));
#ifdef PHP_WIN32
                fflush(stderr);
#endif
            } else {
                cat_log_fwrite(stdout, ZSTR_VAL(str.s), ZSTR_LEN(str.s));
            }
            smart_str_free(&str);
        }
        return;
    }

    if (!(type & CAT_LOG_TYPES_ABNORMAL)) {
        smart_str str;
        memset(&str, 0, sizeof(str));
        php_printf_to_smart_str(&str, format, args);
        smart_str_append_printf(&str, " in %s:%u", zend_get_executed_filename(), zend_get_executed_lineno());
        smart_str_0(&str);
        cat_log_standard(type, module_name CAT_SOURCE_POSITION_RELAY_CC, code, "%s", ZSTR_VAL(str.s));
    } else {
        char *message = cat_vsprintf(format, args);
        if (unlikely(message == NULL)) {
            fprintf(stderr, "Sprintf log message failed\n");
            return;
        }
        cat_set_last_error(code, message);
        switch (type) {
            case CAT_LOG_TYPE_NOTICE: {
                zend_error(E_NOTICE, "%s", message);
                break;
            }
            case CAT_LOG_TYPE_WARNING: {
                zend_error(E_WARNING, "%s", message);
                break;
            }
            case CAT_LOG_TYPE_ERROR: {
                zend_error_noreturn(E_ERROR, "%s", message);
                break;
            }
            case CAT_LOG_TYPE_CORE_ERROR: {
                zend_try {
                    if (EG(exception)) {
                        OBJ_RELEASE(EG(exception));
                        EG(exception) = NULL;
                    }
                    zend_error(E_CORE_ERROR, "%s", message);
                } zend_catch {
                    cat_abort();
                } zend_end_try();
                break;
            }
            default:
                CAT_NEVER_HERE("Unknown log type");
        }
        cat_free(message);
    }
}

static void swow_log_standard(CAT_LOG_PARAMETERS)
{
    va_list args;
    va_start(args, format);

    swow_log_va_standard(type, module_name CAT_SOURCE_POSITION_RELAY_CC, code, format, args);

    va_end(args);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Log_getTypes, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Log, getTypes)
{
    RETURN_LONG(CAT_LOG_G(types));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Log_setTypes, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, types, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Log, setTypes)
{
    zend_long types;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(types)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED((types & ~CAT_LOG_TYPES_ALL) != 0)) {
        zend_argument_value_error(1, "is unrecognized");
        RETURN_THROWS();
    }
    CAT_LOG_G(types) = types;
}

static const zend_function_entry swow_log_methods[] = {
    PHP_ME(Swow_Log, getTypes,       arginfo_class_Swow_Log_getTypes,       ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Log, setTypes,       arginfo_class_Swow_Log_setTypes,       ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_log_module_init(INIT_FUNC_ARGS)
{
    cat_log_function = swow_log_standard;
    swow_log_ce = swow_register_internal_class(
        "Swow\\Log", NULL, swow_log_methods,
        NULL, NULL, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );
    zend_declare_class_constant_long(swow_log_ce, ZEND_STRL("TYPE_DEBUG"),         CAT_LOG_TYPE_DEBUG);
    zend_declare_class_constant_long(swow_log_ce, ZEND_STRL("TYPE_INFO"),          CAT_LOG_TYPE_INFO);
    zend_declare_class_constant_long(swow_log_ce, ZEND_STRL("TYPE_NOTICE"),        CAT_LOG_TYPE_NOTICE);
    zend_declare_class_constant_long(swow_log_ce, ZEND_STRL("TYPE_WARNING"),       CAT_LOG_TYPE_WARNING);
    zend_declare_class_constant_long(swow_log_ce, ZEND_STRL("TYPE_ERROR"),         CAT_LOG_TYPE_ERROR);
    zend_declare_class_constant_long(swow_log_ce, ZEND_STRL("TYPE_CORE_ERROR"),    CAT_LOG_TYPE_CORE_ERROR);
    zend_declare_class_constant_long(swow_log_ce, ZEND_STRL("TYPES_ALL"),          CAT_LOG_TYPES_ALL);
    zend_declare_class_constant_long(swow_log_ce, ZEND_STRL("TYPES_DEFAULT"),      CAT_LOG_TYPES_DEFAULT);
    zend_declare_class_constant_long(swow_log_ce, ZEND_STRL("TYPES_ABNORMAL"),     CAT_LOG_TYPES_ABNORMAL);
    zend_declare_class_constant_long(swow_log_ce, ZEND_STRL("TYPES_UNFILTERABLE"), CAT_LOG_TYPES_UNFILTERABLE);

    return SUCCESS;
}
