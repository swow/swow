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

#include "swow_thannel.h"

SWOW_API zend_class_entry *swow_thannel_ce;
SWOW_API zend_object_handlers swow_thannel_handlers;

SWOW_API zend_class_entry *swow_thannel_exception_ce;

SWOW_API CAT_GLOBALS_DECLARE(swow_thannel);

typedef struct swow_thannel_creation_context_s {
    zend_string *closure_hash;
    void *server_context;
} swow_thannel_creation_context_t;

static zend_object *swow_thannel_create_object(zend_class_entry *ce)
{
    swow_thannel_t *s_thannel = swow_object_alloc(swow_thannel_t, ce, swow_thannel_handlers);
    s_thannel->state = SWOW_THANNEL_STATE_NONE;
    return &s_thannel->std;
}

#define getThisThannel() (swow_thannel_get_from_object(Z_OBJ_P(ZEND_THIS)))

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Thannel___construct, 0, 0, 1)
    ZEND_ARG_OBJ_INFO(0, callable, Closure, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thannel, __construct)
{
    SWOW_CHANNEL_GETTER(sthannel, thannel);
    zend_long capacity = 0;

    if (UNEXPECTED(CHANNEL_HAS_CONSTRUCTED(thannel))) {
        zend_throw_error(NULL, "%s can be constructed only once", ZEND_THIS_NAME);
        RETURN_THROWS();
    }

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(capacity)
    ZEND_PARSE_PARAMETERS_END();

    if (capacity == -1) {
        capacity = CAT_CHANNEL_SIZE_MAX;
    }
    if (UNEXPECTED(capacity < 0)) {
        zend_argument_value_error(1, "can not be negative");
        RETURN_THROWS();
    }

    thannel = cat_thannel_create(thannel, capacity, sizeof(zval), (cat_thannel_data_dtor_t) zval_ptr_dtor);

    if (UNEXPECTED(thannel == NULL)) {
        swow_throw_exception_with_last(swow_thannel_exception_ce);
        RETURN_THROWS();
    }
}

static PHP_METHOD(Swow_Thannel, run)
{
    uv_thannel_create(&s_thannel->thannel, (uv_thannel_cb) swow_thannel_callback, &context);
}

static const zend_function_entry swow_thannel_methods[] = {
    PHP_ME(Swow_Thannel, __construct, arginfo_class_Swow_Thannel___construct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_thannel_module_init(INIT_FUNC_ARGS)
{
    CAT_GLOBALS_REGISTER(swow_thannel);

    swow_thannel_ce = swow_register_internal_class(
        "Swow\\Thannel", NULL, swow_thannel_methods,
        &swow_thannel_handlers, NULL,
        cat_false, cat_false,
        swow_thannel_create_object, NULL,
        XtOffsetOf(swow_thannel_t, std)
    );

    swow_thannel_exception_ce = swow_register_internal_class(
        "Swow\\ThannelException", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}

zend_result swow_thannel_module_shutdown(INIT_FUNC_ARGS)
{
    CAT_GLOBALS_UNREGISTER(swow_thannel);

    return SUCCESS;
}
