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

#ifndef SWOW_UTIL_H
#define SWOW_UTIL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_queue.h"

extern SWOW_API zend_class_entry *swow_utils_handler_ce;
extern SWOW_API zend_object_handlers swow_utils_handler_handlers;

typedef struct swow_utils_handler_s {
    cat_queue_t node;
    swow_fcall_storage_t fcall;
    zend_object std;
} swow_utils_handler_t;

/* helper */

static zend_always_inline swow_utils_handler_t *swow_utils_handler_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_utils_handler_t, std);
}

/* loader */

zend_result swow_util_module_init(INIT_FUNC_ARGS);

/* APIs */

SWOW_API swow_utils_handler_t *swow_utils_handler_create(zval *z_callable);
SWOW_API void swow_utils_handler_push_back_to(swow_utils_handler_t *handler, cat_queue_t *queue);
SWOW_API void swow_utils_handler_remove(swow_utils_handler_t *handler);

SWOW_API void swow_utils_handlers_release(cat_queue_t *handlers);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_UTIL_H */
