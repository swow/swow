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

#include "cat_sync.h"
#include "cat_time.h"

CAT_API cat_sync_wait_group_t *cat_sync_wait_group_create(cat_sync_wait_group_t *wg)
{
    wg->coroutine = NULL;
    wg->count = 0;

    return wg;
}

CAT_API cat_bool_t cat_sync_wait_group_add(cat_sync_wait_group_t *wg, ssize_t delta)
{
    ssize_t count;

    if (unlikely(wg->coroutine != NULL)) {
        cat_update_last_error(CAT_EMISUSE, "WaitGroup add called concurrently with wait");
        return cat_false;
    }
    count = wg->count + delta;
    if (unlikely(count < 0)) {
        cat_update_last_error(CAT_EMISUSE, "WaitGroup counter can not be negative");
        return cat_false;
    }
    wg->count = count;

    return cat_true;
}

CAT_API cat_bool_t cat_sync_wait_group_wait(cat_sync_wait_group_t *wg, cat_timeout_t timeout)
{
    if (unlikely(wg->coroutine != NULL)) {
        cat_update_last_error(CAT_EMISUSE, "WaitGroup can not be reused before previous wait has returned");
        return cat_false;
    }

    if (likely(wg->count > 0)) {
        cat_bool_t ret;
        wg->coroutine = CAT_COROUTINE_G(current);
        ret = cat_time_wait(timeout);
        wg->coroutine = NULL;
        if (unlikely(!ret)) {
            cat_update_last_error_with_previous("WaitGroup waiting for completion failed");
            return cat_false;
        }
        if (wg->count != 0) {
            cat_update_last_error(CAT_ECANCELED, "WaitGroup waiting has been canceled");
            return cat_false;
        }
    }

    return cat_true;
}

CAT_API cat_bool_t cat_sync_wait_group_done(cat_sync_wait_group_t *wg)
{
    ssize_t count;

    count = wg->count - 1;
    if (unlikely(count < 0)) {
        cat_update_last_error(CAT_EMISUSE, "WaitGroup counter can not be negative");
        return cat_false;
    }
    wg->count = count;
    if (count == 0 && wg->coroutine != NULL) {
        cat_coroutine_schedule(wg->coroutine, SYNC, "WaitGroup");
    }

    return cat_true;
}
