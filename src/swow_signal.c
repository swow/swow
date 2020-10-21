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

#include "swow_signal.h"

SWOW_API zend_class_entry *swow_signal_ce;
SWOW_API zend_object_handlers swow_signal_handlers;

SWOW_API zend_class_entry *swow_signal_exception_ce;

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_signal_wait, ZEND_RETURN_VALUE, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, num, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, timeout, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_signal, wait)
{
    zend_long signum;
    zend_long timeout = -1;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(signum)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    ret = cat_signal_wait(signum, timeout);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_signal_exception_ce);
        RETURN_THROWS();
    }
}

static const zend_function_entry swow_signal_methods[] = {
    PHP_ME(swow_signal, wait, arginfo_swow_signal_wait, ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_FE_END
};

int swow_signal_module_init(INIT_FUNC_ARGS)
{
    swow_signal_ce = swow_register_internal_class(
        "Swow\\Signal", NULL, swow_signal_methods,
        &swow_signal_handlers, NULL,
        cat_false, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );

#define SWOW_SIGNAL_GEN(name) zend_declare_class_constant_long(swow_signal_ce, ZEND_STRL(#name), SIG##name);
    CAT_SIGNAL_MAP(SWOW_SIGNAL_GEN)
#undef SWOW_SIGNAL_GEN

    swow_signal_exception_ce = swow_register_internal_class(
        "Swow\\Signal\\Exception", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}
