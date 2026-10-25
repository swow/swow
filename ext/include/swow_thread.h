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

#ifndef SWOW_THREAD_H
#define SWOW_THREAD_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_thread.h"

#ifdef ZTS

extern SWOW_API zend_class_entry *swow_thread_ce;
extern SWOW_API zend_class_entry *swow_thread_exception_ce;
extern SWOW_API zend_object_handlers swow_thread_handlers;

typedef struct swow_thread_s swow_thread_t; /* opaque */

/*
 * The borrowed pointer of a Swow\Thread object, or NULL when the object is not
 * one. The pointer lives until the object is freed
 */
SWOW_API swow_thread_t *swow_thread_get(zend_object *object);

/* The id of the underlying thread, 0 before the thread starts */
SWOW_API cat_thread_id_t swow_thread_get_id(const swow_thread_t *thread);
SWOW_API cat_bool_t swow_thread_is_running(const swow_thread_t *thread);
/* cat_false with the last error set when the thread cannot be killed */
SWOW_API cat_bool_t swow_thread_kill(swow_thread_t *thread);

#endif /* ZTS */

#ifdef __cplusplus
}
#endif
#endif /* SWOW_THREAD_H */
