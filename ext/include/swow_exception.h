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

#ifndef SWOW_EXCEPTION_H
#define SWOW_EXCEPTION_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "zend_errors.h"
#include "zend_exceptions.h"
#include "ext/spl/spl_exceptions.h"

#ifndef E_NONE
#define E_NONE 0
#endif

#define SWOW_ERROR_CONTROL_START(level) do { \
    int error_reporting = EG(error_reporting); \
    EG(error_reporting) = level;
#define SWOW_ERROR_CONTROL_END() \
    EG(error_reporting) = error_reporting; \
} while (0);

extern SWOW_API zend_class_entry *swow_exception_ce;
extern SWOW_API zend_class_entry *swow_call_exception_ce;

zend_result swow_exceptions_module_init(INIT_FUNC_ARGS);

extern SWOW_API swow_object_create_t swow_exception_create_object;

SWOW_API CAT_COLD void swow_exception_set_properties(zend_object *exception, const char *message, zend_long code);

SWOW_API CAT_COLD zend_object *swow_throw_exception(zend_class_entry *ce, zend_long code, const char *format, ...) ZEND_ATTRIBUTE_FORMAT(printf, 3, 4);
SWOW_API CAT_COLD zend_object *swow_throw_exception_with_last(zend_class_entry *ce);
#define swow_throw_exception_with_last_as_reason(ce, format, ...) \
        swow_throw_exception(ce, cat_get_last_error_code(), format ", reason: %s", ##__VA_ARGS__, cat_get_last_error_message())

SWOW_API CAT_COLD void swow_call_exception_set_return_value(zend_object *exception, zval *return_value);

#define swow_throw_call_exception(ce, code, format, ...) do { \
    ZEND_ASSERT(instanceof_function(ce, swow_call_exception_ce)); \
    zend_object *exception = swow_throw_exception(ce, code, format, ##__VA_ARGS__); \
    swow_call_exception_set_return_value(exception, return_value); \
} while (0)

#define swow_throw_call_exception_with_last(ce)  do { \
    ZEND_ASSERT(instanceof_function(ce, swow_call_exception_ce)); \
    zend_object *exception = swow_throw_exception_with_last(ce); \
    swow_call_exception_set_return_value(exception, return_value); \
} while (0)

#define swow_throw_call_exception_with_last_as_reason(ce, format, ...)  do { \
    ZEND_ASSERT(instanceof_function(ce, swow_call_exception_ce)); \
    zend_object *exception = swow_throw_exception_with_last_as_reason(ce, format, ##__VA_ARGS__); \
    swow_call_exception_set_return_value(exception, return_value); \
} while (0)

SWOW_API const char *swow_strerrortype(int type); SWOW_INTERNAL

#ifdef __cplusplus
}
#endif
#endif /* SWOW_EXCEPTION_H */
