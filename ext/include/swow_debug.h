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

#ifndef SWOW_DEBUG_H
#define SWOW_DEBUG_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "swow_util.h"

CAT_GLOBALS_STRUCT_BEGIN(swow_debug)
    cat_queue_t extended_statement_handlers;
CAT_GLOBALS_STRUCT_END(swow_debug)

extern SWOW_API CAT_GLOBALS_DECLARE(swow_debug)

#define SWOW_DEBUG_G(x) CAT_GLOBALS_GET(swow_debug, x)

zend_result swow_debug_module_init(INIT_FUNC_ARGS);
zend_result swow_debug_runtime_init(INIT_FUNC_ARGS);
zend_result swow_debug_runtime_shutdown(INIT_FUNC_ARGS);

static zend_always_inline zend_bool swow_debug_is_user_call(zend_execute_data *call)
{
    return call && call->func && ZEND_USER_CODE(call->func->common.type);
}

SWOW_API zend_long swow_debug_backtrace_depth(zend_execute_data *call, zend_long limit);
SWOW_API zend_execute_data *swow_debug_backtrace_resolve(zend_execute_data *call, zend_long level);
SWOW_API zend_execute_data *swow_debug_backtrace_resolve_ex(zend_execute_data *call, zend_long level, zend_long limit, zend_long *depth);

SWOW_API smart_str *swow_debug_build_trace_as_smart_str(smart_str *str, HashTable *trace); SWOW_INTERNAL
SWOW_API zend_string *swow_debug_build_trace_as_string(HashTable *trace);

SWOW_API HashTable *swow_debug_get_trace(zend_long options, zend_long limit);
SWOW_API smart_str *swow_debug_get_trace_as_smart_str(smart_str *str, zend_long options, zend_long limit); SWOW_INTERNAL
SWOW_API zend_string *swow_debug_get_trace_as_string(zend_long options, zend_long limit);
SWOW_API HashTable *swow_debug_get_trace_as_list(zend_long options, zend_long limit);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_DEBUG_H */
