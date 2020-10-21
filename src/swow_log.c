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

SWOW_API zend_class_entry *swow_log_ce;
SWOW_API zend_object_handlers swow_log_handlers;

static void swow_log_standard(CAT_LOG_PARAMATERS)
{
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

    if (type & (CAT_LOG_TYPE_DEBUG | CAT_LOG_TYPE_INFO))
    {
        char *output;
        const char *type_string;
        cat_bool_t failed = cat_false;
        switch (type)
        {
#ifdef CAT_DEBUG
        case CAT_LOG_TYPE_DEBUG:
            type_string = "Debug";
            break;
#endif
        default:
            type_string = "Info";
        }
        /* stdout */
        output = cat_sprintf(
            "%s: <%s> %s in %s on line %u" PHP_EOL,
            type_string, module_name, message, zend_get_executed_filename(), zend_get_executed_lineno()
        );
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_log_getTypes, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_log, getTypes)
{
    RETURN_LONG(CAT_G(log_types));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_log_setTypes, ZEND_RETURN_VALUE, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, types, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_log, setTypes)
{
    zend_long types;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(types)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED((types &~ CAT_LOG_TYPES_ALL) != 0)) {
        zend_argument_value_error(1, "is unrecognized");
        RETURN_THROWS();
    }
    CAT_G(log_types) = types;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_log_getModuleTypes, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_log, getModuleTypes)
{
    RETURN_LONG(CAT_G(log_module_types));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_log_setModuleTypes, ZEND_RETURN_VALUE, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, moduleTypes, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_log, setModuleTypes)
{
    zend_long module_types;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(module_types)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED((module_types &~ CAT_MODULE_TYPES_ALL) != 0)) {
        zend_argument_value_error(1, "is unrecognized");
        RETURN_THROWS();
    }
    CAT_G(log_module_types) = module_types;
}

static const zend_function_entry swow_log_methods[] = {
    PHP_ME(swow_log, getTypes,       arginfo_swow_log_getTypes,       ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(swow_log, setTypes,       arginfo_swow_log_setTypes,       ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(swow_log, getModuleTypes, arginfo_swow_log_getModuleTypes, ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(swow_log, setModuleTypes, arginfo_swow_log_setModuleTypes, ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_FE_END
};

int swow_log_module_init(INIT_FUNC_ARGS)
{
    cat_log = swow_log_standard;
    swow_log_ce = swow_register_internal_class(
        "Swow\\Log", NULL, swow_log_methods,
        &swow_log_handlers, NULL,
        cat_false, cat_false, cat_false,
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
