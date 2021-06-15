/*
  +--------------------------------------------------------------------------+
  | libcat                                                                   |
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

#ifndef CAT_SYNC_H
#define CAT_SYNC_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#include "cat_coroutine.h"

typedef struct cat_sync_wait_group_s {
    cat_coroutine_t *coroutine;
    size_t count;
} cat_sync_wait_group_t;

CAT_API cat_sync_wait_group_t *cat_sync_wait_group_create(cat_sync_wait_group_t *wg);
CAT_API cat_bool_t cat_sync_wait_group_add(cat_sync_wait_group_t *wg, ssize_t delta);
CAT_API cat_bool_t cat_sync_wait_group_wait(cat_sync_wait_group_t *wg, cat_timeout_t timeout);
CAT_API cat_bool_t cat_sync_wait_group_done(cat_sync_wait_group_t *wg);

#ifdef __cplusplus
}
#endif
#endif /* CAT_SYNC_H */
