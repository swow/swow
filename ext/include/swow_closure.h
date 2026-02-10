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
  |         dixyes <dixyes@gmail.com>                                        |
  +--------------------------------------------------------------------------+
 */

#ifndef SWOW_CLOSURE_H
#define SWOW_CLOSURE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

/* globals */

CAT_GLOBALS_STRUCT_BEGIN(swow_closure) {
    zend_object *current_object;
} CAT_GLOBALS_STRUCT_END(swow_closure);

extern SWOW_API CAT_GLOBALS_DECLARE(swow_closure);

#define SWOW_CLOSURE_G(x) CAT_GLOBALS_GET(swow_closure, x)

/* helper functions */

static zend_always_inline bool swow_object_is_closure(zend_object *object)
{
    return instanceof_function(object->ce, zend_ce_closure);
}

static zend_always_inline bool swow_zval_is_closure(zval *z_value)
{
    return Z_TYPE_P(z_value) == IS_OBJECT && swow_object_is_closure(Z_OBJ_P(z_value));
}

static zend_always_inline bool swow_function_is_anonymous(const zend_function *function)
{
    return (function->common.fn_flags & (ZEND_ACC_CLOSURE | ZEND_ACC_FAKE_CLOSURE)) == ZEND_ACC_CLOSURE;
}

static zend_always_inline bool swow_function_is_user_anonymous(const zend_function *function)
{
    return function->type == ZEND_USER_FUNCTION && swow_function_is_anonymous(function);
}

static zend_always_inline bool swow_function_is_named(const zend_function *function)
{
    return function->common.function_name != NULL && !swow_function_is_anonymous(function);
}

/* loader */

zend_result swow_closure_module_init(INIT_FUNC_ARGS);
zend_result swow_closure_module_shutdown(INIT_FUNC_ARGS);

/* APIs */

SWOW_API SWOW_MAY_THROW HashTable *swow_serialize_user_anonymous_function(zend_function *function);
SWOW_API SWOW_MAY_THROW HashTable *swow_serialize_named_function(zend_function *function);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_CLOSURE_H */
