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

#ifndef SWOW_SYNC_H
#define SWOW_SYNC_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"
#include "swow_coroutine.h"

#include "cat_sync.h"

extern SWOW_API zend_class_entry *swow_sync_wait_reference_ce;
extern SWOW_API zend_object_handlers swow_sync_wait_reference_handlers;

extern SWOW_API zend_class_entry *swow_sync_wait_group_ce;
extern SWOW_API zend_object_handlers swow_sync_wait_group_handlers;

extern SWOW_API zend_class_entry *swow_sync_exception_ce;

typedef struct swow_sync_wait_reference_s {
    swow_coroutine_t *scoroutine;
    cat_bool_t done;
    zend_object std;
} swow_sync_wait_reference_t;

typedef struct swow_sync_wait_group_s {
    cat_sync_wait_group_t wg;
    zend_object std;
} swow_sync_wait_group_t;

/* loader */

zend_result swow_sync_module_init(INIT_FUNC_ARGS);

/* helper*/

static zend_always_inline swow_sync_wait_reference_t *swow_sync_wait_reference_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_sync_wait_reference_t, std);
}

static zend_always_inline swow_sync_wait_group_t *swow_sync_wait_group_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_sync_wait_group_t, std);
}

#ifdef __cplusplus
}
#endif
#endif /* SWOW_SYNC_H */
