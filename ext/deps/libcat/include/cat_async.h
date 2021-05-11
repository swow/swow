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

#ifndef CAT_THREAD_H
#define CAT_THREAD_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#include "cat_coroutine.h"

typedef struct cat_async_s cat_async_t;
typedef void (*cat_async_cleanup_callback)(cat_async_t *async);

struct cat_async_s {

    union {
        uv_handle_t handle;
        uv_async_t async;
    } u;
    cat_async_cleanup_callback cleanup;
    cat_coroutine_t *coroutine;
    cat_bool_t allocated;
    cat_bool_t done;
    cat_bool_t closing;
};

CAT_API cat_async_t *cat_async_create(cat_async_t *async);
CAT_API int cat_async_notify(cat_async_t *async);
/* eq to wait + cleanup/close */
CAT_API cat_bool_t cat_async_wait_and_close(cat_async_t *async, cat_async_cleanup_callback cleanup, cat_timeout_t timeout);
/* clean up callback will be called after async was notified, and async closed */
CAT_API cat_bool_t cat_async_cleanup(cat_async_t *async, cat_async_cleanup_callback cleanup);
/* async handle will be closed immediately, clean up callback will be called at the same time  */
CAT_API cat_bool_t cat_async_close(cat_async_t *async, cat_async_cleanup_callback cleanup);

#ifdef __cplusplus
}
#endif
#endif /* CAT_THREAD_H */
