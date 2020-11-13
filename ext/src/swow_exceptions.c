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

#include "swow_exceptions.h"

SWOW_API zend_class_entry *swow_uncatchable_ce;
SWOW_API zend_class_entry *swow_exception_ce;
SWOW_API zend_class_entry *swow_call_exception_ce;

SWOW_API swow_object_create_t swow_exception_create_object;

static user_opcode_handler_t original_zend_catch_handler = NULL;

static int swow_catch_handler(zend_execute_data *execute_data)
{
    zend_exception_restore();
    if (UNEXPECTED(EG(exception))) {
        if (instanceof_function(EG(exception)->ce, swow_uncatchable_ce)) {
            return ZEND_USER_OPCODE_RETURN;
        }
    }
    if (UNEXPECTED(original_zend_catch_handler)) {
        return original_zend_catch_handler(execute_data);
    }

    return ZEND_USER_OPCODE_DISPATCH;
}

SWOW_API CAT_COLD void swow_exception_set_properties(zend_object *exception, const char *message, zend_long code)
{
    zval ztmp;
    ZVAL7_ALLOC_OBJECT(exception);
    if (message != NULL) {
        ZVAL_STRING(&ztmp, message);
        Z_SET_REFCOUNT(ztmp, 0);
        zend_update_property_ex(exception->ce, ZVAL7_OBJECT(exception), ZSTR_KNOWN(ZEND_STR_MESSAGE), &ztmp);
    }
    if (code != (zend_long) ~0) {
        ZVAL_LONG(&ztmp, code);
        zend_update_property_ex(exception->ce, ZVAL7_OBJECT(exception), ZSTR_KNOWN(ZEND_STR_CODE), &ztmp);
    }
}

SWOW_API CAT_COLD zend_object *swow_throw_exception(zend_class_entry *ce, zend_long code, const char *format, ...)
{
    va_list va;
    char *message;
    zend_object *object;

    if (code == CAT_EMISUSE || code == CAT_EVALUE || code == CAT_ELOCKED) {
        ce = zend_ce_error;
    }
    if (ce == NULL) {
        ce = swow_exception_ce;
    }
    va_start(va, format);
    zend_vspprintf(&message, 0, format, va);
    va_end(va);
    object = zend_throw_exception(ce, message, code);
    efree(message);

    return object;
}

SWOW_API CAT_COLD zend_object *swow_throw_exception_with_last(zend_class_entry *ce)
{
    return swow_throw_exception(ce, cat_get_last_error_code(), "%s", cat_get_last_error_message());
}

SWOW_API CAT_COLD void swow_call_exception_set_return_value(zend_object *exception, zval *return_value)
{
    ZVAL7_ALLOC_OBJECT(exception);
    CAT_ASSERT(instanceof_function(exception->ce, swow_call_exception_ce));
    zend_update_property(exception->ce, ZVAL7_OBJECT(exception), ZEND_STRL("returnValue"), return_value);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Call_Exception_getReturnValue, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Call_Exception, getReturnValue)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_THIS_PROPERTY("returnValue");
}

static const zend_function_entry swow_call_exception_methods[] = {
    PHP_ME(Swow_Call_Exception, getReturnValue, arginfo_class_Swow_Call_Exception_getReturnValue, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_FE_END
};

int swow_exceptions_module_init(INIT_FUNC_ARGS)
{
    swow_uncatchable_ce = swow_register_internal_interface("Swow\\Uncatchable", NULL, NULL);
    /* Uncatchable hook */
    original_zend_catch_handler = zend_get_user_opcode_handler(ZEND_CATCH);
    zend_set_user_opcode_handler(ZEND_CATCH, swow_catch_handler);

    /* Exception for user */
    swow_exception_ce = swow_register_internal_class(
        "Swow\\Exception", spl_ce_RuntimeException, NULL, NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );

    swow_call_exception_ce = swow_register_internal_class(
        "Swow\\CallException", swow_exception_ce, swow_call_exception_methods,
        NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );
    zend_declare_property_null(swow_call_exception_ce, ZEND_STRL("returnValue"), ZEND_ACC_PROTECTED);

    /* fast call */
    swow_exception_create_object = zend_ce_exception->create_object;

    return SUCCESS;
}

SWOW_API const char *swow_strerrortype(int type)
{
    const char *error_type_str;

#ifdef E_DONT_BAIL
    type &= ~(E_DONT_BAIL);
#endif
    switch (type) {
        case E_ERROR:
        case E_CORE_ERROR:
        case E_COMPILE_ERROR:
        case E_USER_ERROR:
            error_type_str = "Fatal error";
            break;
        case E_RECOVERABLE_ERROR:
            error_type_str = "Recoverable fatal error";
            break;
        case E_WARNING:
        case E_CORE_WARNING:
        case E_COMPILE_WARNING:
        case E_USER_WARNING:
            error_type_str = "Warning";
            break;
        case E_PARSE:
            error_type_str = "Parse error";
            break;
        case E_NOTICE:
        case E_USER_NOTICE:
            error_type_str = "Notice";
            break;
        case E_STRICT:
            error_type_str = "Strict Standards";
            break;
        case E_DEPRECATED:
        case E_USER_DEPRECATED:
            error_type_str = "Deprecated";
            break;
        default:
            error_type_str = "Unknown error";
            break;
    }

    return error_type_str;
}
