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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#ifndef SWOW_FUTEX_H
#define SWOW_FUTEX_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_futex.h"

extern SWOW_API zend_class_entry *swow_futex_ce;
extern SWOW_API zend_object_handlers swow_futex_handlers;

typedef struct swow_futex_s swow_futex_t; /* opaque */

/*
 * The borrowed pointer of a Swow\Futex object, or NULL when the object is not
 * one or has not been constructed. The pointer lives until the object is freed
 */
SWOW_API swow_futex_t *swow_futex_get(zend_object *object);

SWOW_API cat_bool_t swow_futex_wait(swow_futex_t *futex, int32_t expected,
                                    cat_timeout_t timeout);
SWOW_API cat_bool_t swow_futex_wake(swow_futex_t *futex, int32_t count);
SWOW_API int32_t swow_futex_get_value(const swow_futex_t *futex);
/* The result wraps on overflow. Returns the value before the add */
SWOW_API int32_t swow_futex_add(swow_futex_t *futex, int32_t delta);
/* Strong compare-exchange: stores new_value only when the value equals expected */
SWOW_API cat_bool_t swow_futex_compare_exchange(swow_futex_t *futex, int32_t expected,
                                                int32_t new_value);
/* Returns the value before the replace */
SWOW_API int32_t swow_futex_exchange(swow_futex_t *futex, int32_t new_value);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_FUTEX_H */
