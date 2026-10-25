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

#ifndef SWOW_THREAD_BARRIER_H
#define SWOW_THREAD_BARRIER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_thread_sync.h"

#ifdef ZTS

extern SWOW_API zend_class_entry *swow_thread_barrier_ce;
extern SWOW_API zend_object_handlers swow_thread_barrier_handlers;

typedef struct swow_thread_barrier_s swow_thread_barrier_t; /* opaque */

/*
 * The borrowed pointer of a Swow\Sync\ThreadBarrier object, or NULL when the
 * object is not one or has not been constructed. The pointer lives until the
 * object is freed
 */
SWOW_API swow_thread_barrier_t *swow_thread_barrier_get(zend_object *object);

/* cat_false when the wait timed out before the barrier tripped */
SWOW_API cat_bool_t swow_thread_barrier_wait(swow_thread_barrier_t *barrier,
                                             cat_timeout_t timeout);
SWOW_API uint32_t swow_thread_barrier_get_count(const swow_thread_barrier_t *barrier);
SWOW_API uint32_t swow_thread_barrier_get_waiting(const swow_thread_barrier_t *barrier);

#endif /* ZTS */

#ifdef __cplusplus
}
#endif
#endif /* SWOW_THREAD_BARRIER_H */
