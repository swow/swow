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

#ifndef SWOW_SEMAPHORE_H
#define SWOW_SEMAPHORE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_thread_sync.h"

#ifdef ZTS

extern SWOW_API zend_class_entry *swow_semaphore_ce;
extern SWOW_API zend_object_handlers swow_semaphore_handlers;

typedef struct swow_semaphore_s swow_semaphore_t; /* opaque */

/*
 * The borrowed pointer of a Swow\Semaphore object, or NULL when the object is
 * not one or has not been constructed. The pointer lives until the object is
 * freed
 */
SWOW_API swow_semaphore_t *swow_semaphore_get(zend_object *object);

SWOW_API cat_bool_t swow_semaphore_acquire(swow_semaphore_t *semaphore,
                                           cat_timeout_t timeout);
SWOW_API cat_bool_t swow_semaphore_tryacquire(swow_semaphore_t *semaphore);
/* A negative count is CAT_EINVAL, and so is a count that passes the limit */
SWOW_API cat_bool_t swow_semaphore_release(swow_semaphore_t *semaphore, int32_t count);

#endif /* ZTS */

#ifdef __cplusplus
}
#endif
#endif /* SWOW_SEMAPHORE_H */
