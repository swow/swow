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

#include "swow_{{file_name}}.h"

SWOW_API zend_class_entry *swow_{{type_name}}_ce;
SWOW_API zend_object_handlers swow_{{type_name}}_handlers;

static zend_object* swow_{{type_name}}_create_object(zend_class_entry *ce)
{
    swow_{{type_name}}_t *{{php_var_name}} = (swow_{{type_name}}_t *) zend_object_alloc(sizeof(*{{php_var_name}}), ce);

    zend_object_std_init(&{{php_var_name}}->std, ce);
    object_properties_init(&{{php_var_name}}->std, ce);
    {{php_var_name}}->std.handlers = &swow_{{type_name}}_handlers;
    (void) cat_{{type_name}}_init(&{{php_var_name}}->{{cat_var_name}});

    return &{{php_var_name}}->std;
}

static void swow_{{type_name}}_free_object(zend_object *object)
{
    swow_{{type_name}}_t *{{php_var_name}} = swow_{{type_name}}_get_from_object(object);
    cat_{{type_name}}_t *{{cat_var_name}} = &{{php_var_name}}->{{cat_var_name}};

    cat_{{type_name}}_close({{cat_var_name}});

    zend_object_std_dtor(&{{php_var_name}}->std);
}

#define getThisS{{cat_var_name}}() (swow_{{type_name}}_get_from_object(Z_OBJ_P(ZEND_THIS)))

#define SWOW_{{TYPE_NAME}}_GETTER(_{{php_var_name}}, _{{cat_var_name}}) \
    swow_{{type_name}}_t *_{{php_var_name}} = getThisS{{cat_var_name}}(); \
    cat_{{type_name}}_t *_{{cat_var_name}} = &_{{php_var_name}}->{{cat_var_name}}

ZEND_BEGIN_ARG_INFO_EX(arginfo_swow_{{type_name}}___construct, 0, ZEND_RETURN_VALUE, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_{{type_name}}, __construct)
{
    zend_long value;

    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 1, 1)
        Z_PARAM_LONG(value)
    ZEND_PARSE_PARAMETERS_END();
}

static const zend_function_entry swow_{{type_name}}_methods[] = {
    PHP_ME(swow_{{type_name}}, __construct, arginfo_swow_{{type_name}}___construct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

int swow_{{module_name}}_module_init(INIT_FUNC_ARGS)
{
    swow_{{type_name}}_ce = swow_register_internal_class(
        "Swow\\{{class_name}}", NULL, swow_{{type_name}}_methods,
        &swow_{{type_name}}_handlers, NULL,
        cat_false, cat_false,
        swow_{{type_name}}_create_object,
        swow_{{type_name}}_free_object,
        XtOffsetOf(swow_{{type_name}}_t, std)
    );

    return SUCCESS;
}
