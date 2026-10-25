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

#ifndef SWOW_THANNEL_H
#define SWOW_THANNEL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#ifdef ZTS

extern SWOW_API zend_class_entry *swow_thannel_ce;
extern SWOW_API zend_class_entry *swow_thannel_exception_ce;
extern SWOW_API zend_object_handlers swow_thannel_handlers;

typedef struct swow_thannel_s swow_thannel_t; /* opaque */

/*
 * The borrowed pointer of a Swow\Thannel object, or NULL when the object is not
 * one or has not been constructed. The pointer lives until the object is freed
 */
SWOW_API swow_thannel_t *swow_thannel_get(zend_object *object);

/* The queue copies the value, so the caller keeps its buffer */
SWOW_API cat_bool_t swow_thannel_push(swow_thannel_t *thannel, const void *data,
                                      size_t size, cat_timeout_t timeout);
/*
 * The caller owns the copy that pop writes to *data and frees it with
 * cat_sys_free. A message of size 0 gives a null pointer and a size of 0
 */
SWOW_API cat_bool_t swow_thannel_pop(swow_thannel_t *thannel, void **data,
                                     size_t *size, cat_timeout_t timeout);
SWOW_API cat_bool_t swow_thannel_close(swow_thannel_t *thannel);
SWOW_API size_t swow_thannel_get_length(const swow_thannel_t *thannel);
SWOW_API size_t swow_thannel_get_capacity(const swow_thannel_t *thannel);

#endif /* ZTS */

#ifdef __cplusplus
}
#endif
#endif /* SWOW_THANNEL_H */
