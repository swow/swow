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

SWOW_API zend_class_entry *swow_log_ce;

static void swow_log_standard(CAT_LOG_PARAMATERS)
{
#ifndef CAT_ENABLE_DEBUG_LOG
    if (type == CAT_LOG_TYPE_DEBUG) {
        return;
    }
#endif

    char *message;
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

    /* Zend put()/error() may call PHP callbacks,
     * we can not do it in a pure C or interned coroutine */
    if (swow_coroutine_get_current()->executor == NULL) {
        cat_log_standard(type, module_type, module_name CAT_SOURCE_POSITION_RELAY_CC, code, "%s", message);
        cat_free(message);
        return;
    }

    if (!(type & CAT_LOG_TYPES_ABNORMAL)) {
        char *output;
        const char *type_string = "Unknown";
        cat_bool_t failed = cat_false;
        switch (type) {
#ifdef CAT_ENABLE_DEBUG_LOG
            case CAT_LOG_TYPE_DEBUG:
                type_string = "Debug";
                break;
#endif
            case CAT_LOG_TYPE_INFO:
                type_string = "Info";
                break;
            default:
                CAT_NEVER_HERE("Unknown log type");
        }
        /* stdout */
        do {
            const char *name = cat_coroutine_get_current_role_name();
            if (name != NULL) {
                output = cat_sprintf(
                    "%s: <%s> %s in %s in %s on line %u" PHP_EOL,
                    type_string, module_name, message, name,
                    zend_get_executed_filename(), zend_get_executed_lineno()
                );
            } else {
                output = cat_sprintf(
                    "%s: <%s> %s in R" CAT_COROUTINE_ID_FMT " in %s on line %u" PHP_EOL,
                    type_string, module_name, message, cat_coroutine_get_current_id(),
                    zend_get_executed_filename(), zend_get_executed_lineno()
                );
            }
        } while (0);
        if (likely(output != NULL)) {
            ZEND_PUTS(output);
            cat_free(output);
#ifdef CAT_SOURCE_POSITION
            if (CAT_G(log_source_postion)) {
                output = cat_sprintf("CSP: in %s on line %u" PHP_EOL, file, line);
                if (likely(output != NULL)) {
                    ZEND_PUTS(output);
                    cat_free(output);
                } else {
                    failed = cat_true;
                }
            }
#endif
        } else {
            failed = cat_true;
        }
        cat_free(message);
        if (unlikely(failed)) {
            fprintf(stderr, "Sprintf log output failed" CAT_EOL);
        }
    } else {
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
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Log_getTypes, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Log, getTypes)
{
    RETURN_LONG(CAT_G(log_types));
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
    CAT_G(log_types) = types;
}

#define arginfo_class_Swow_Log_getModuleTypes arginfo_class_Swow_Log_getTypes

static PHP_METHOD(Swow_Log, getModuleTypes)
{
    RETURN_LONG(CAT_G(log_module_types));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Log_setModuleTypes, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, moduleTypes, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Log, setModuleTypes)
{
    zend_long module_types;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(module_types)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED((module_types & ~CAT_MODULE_TYPES_ALL) != 0)) {
        zend_argument_value_error(1, "is unrecognized");
        RETURN_THROWS();
    }
    CAT_G(log_module_types) = module_types;
}

static const zend_function_entry swow_log_methods[] = {
    PHP_ME(Swow_Log, getTypes,       arginfo_class_Swow_Log_getTypes,       ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Log, setTypes,       arginfo_class_Swow_Log_setTypes,       ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Log, getModuleTypes, arginfo_class_Swow_Log_getModuleTypes, ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Log, setModuleTypes, arginfo_class_Swow_Log_setModuleTypes, ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
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
