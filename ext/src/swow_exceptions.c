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

SWOW_API zend_class_entry *swow_exception_ce;
SWOW_API zend_class_entry *swow_call_exception_ce;

SWOW_API swow_object_create_t swow_exception_create_object;

SWOW_API CAT_COLD void swow_exception_set_properties(zend_object *exception, const char *message, zend_long code)
{
    zval ztmp;
    if (message != NULL) {
        ZVAL_STRING(&ztmp, message);
        Z_SET_REFCOUNT(ztmp, 0);
        zend_update_property_ex(exception->ce, exception, ZSTR_KNOWN(ZEND_STR_MESSAGE), &ztmp);
    }
    if (code != (zend_long) ~0) {
        ZVAL_LONG(&ztmp, code);
        zend_update_property_ex(exception->ce, exception, ZSTR_KNOWN(ZEND_STR_CODE), &ztmp);
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
    if (!instanceof_function(exception->ce, swow_call_exception_ce)) {
        ZEND_ASSERT(instanceof_function(exception->ce, zend_ce_error));
        return;
    }
    zend_update_property(exception->ce, exception, ZEND_STRL("returnValue"), return_value);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_CallException_getReturnValue, 0, 0, IS_MIXED, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_CallException, getReturnValue)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_THIS_PROPERTY("returnValue");
}

static const zend_function_entry swow_call_exception_methods[] = {
    PHP_ME(Swow_CallException, getReturnValue, arginfo_class_Swow_CallException_getReturnValue, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_exceptions_module_init(INIT_FUNC_ARGS)
{
    /* Exception for user */
    swow_exception_ce = swow_register_internal_class(
        "Swow\\Exception", spl_ce_RuntimeException, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    swow_call_exception_ce = swow_register_internal_class(
        "Swow\\CallException", swow_exception_ce, swow_call_exception_methods,
        NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );
    do {
        zval default_value;
        ZVAL_NULL(&default_value);
        zend_string *name = zend_string_init(ZEND_STRL("returnValue"), 1);
        zend_declare_typed_property(swow_call_exception_ce, name, &default_value, ZEND_ACC_PROTECTED, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ANY));
        zend_string_release(name);
    } while (0);

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
