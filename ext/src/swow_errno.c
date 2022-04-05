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

#include "swow_errno.h"

SWOW_API zend_class_entry *swow_errno_ce;

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Errno_getDescriptionFor, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, error, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Errno, getDescriptionFor)
{
    zend_long error;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(error)
    ZEND_PARSE_PARAMETERS_END();

    // overflow for int
    if (error < INT_MIN || error > INT_MAX) {
        zend_value_error("Errno passed in is not in errno_t range");
        RETURN_THROWS();
    }

    RETURN_STRING(cat_strerror(error));
}

static const zend_function_entry swow_errno_methods[] = {
    PHP_ME(Swow_Errno, getDescriptionFor, arginfo_Swow_Errno_getDescriptionFor, ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_errno_module_init(INIT_FUNC_ARGS)
{
    swow_errno_ce = swow_register_internal_class(
        "Swow\\Errno", NULL, swow_errno_methods,
        NULL, NULL, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );

#define SWOW_ERRNO_GEN(name, message) do { \
    zend_declare_class_constant_long(swow_errno_ce, ZEND_STRL(#name), CAT_##name); \
} while (0);
    CAT_ERRNO_MAP(SWOW_ERRNO_GEN)
#undef SWOW_ERRNO_GEN

    return SUCCESS;
}
