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

#include "cat_async.h"

#include "cat_event.h"
#include "cat_time.h"

#define CAT_ASYNC_CHECK_AVAILABILITY(async, failure) do { \
    if (async->coroutine != NULL) { \
        cat_update_last_error(CAT_EINVAL, "Async is in waiting"); \
        failure; \
    } \
    if (async->closing) { \
        cat_update_last_error(CAT_EINVAL, "Async is closing"); \
        return cat_false; \
    } \
} while (0)

static void cat_async_close_callback(uv_handle_t *handle)
{
    cat_async_t *async = cat_container_of(handle, cat_async_t, u.handle);
    /* async maybe free'd in cleanup if it's not allocated */
    cat_bool_t allocated = async->allocated;

    if (async->cleanup != NULL) {
        async->cleanup(async);
    }
    if (allocated) {
        cat_free(async);
    }
}

static void cat_async_callback(uv_async_t *handle)
{
    cat_async_t *async = cat_container_of(handle, cat_async_t, u.async);
    cat_coroutine_t *coroutine = async->coroutine;
    /* coroutine must be in waiting and not in closing,
     * or coroutine (have not had time to wait / wait failed / cancelled). */
    CAT_ASSERT((coroutine != NULL && !async->closing) || coroutine == NULL);

    async->done = cat_true;
    if (coroutine != NULL) {
        cat_coroutine_schedule(coroutine, THREAD, "Async");
    } else if (async->closing) {
        /* wait failed/cancelled and reach here */
        uv_close(&async->u.handle, cat_async_close_callback);
    }
}

CAT_API cat_async_t *cat_async_create(cat_async_t *async)
{
    int error;

    if (async == NULL) {
        async = (cat_async_t *) cat_malloc(sizeof(*async));
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(async == NULL)) {
            cat_update_last_error_of_syscall("Malloc for async failed");
            return NULL;
        }
#endif
        async->allocated = cat_true;
    } else {
        async->allocated = cat_false;
    }
    async->cleanup = NULL;
    async->done = cat_false;
    async->closing = cat_false;
    async->coroutine = NULL;

    error = uv_async_init(&CAT_EVENT_G(loop), &async->u.async, cat_async_callback);

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Async init failed");
        return NULL;
    }

    return async;
}

/* Notice: we can not update last error here,
 * because it maybe called in other threads */
CAT_API int cat_async_notify(cat_async_t *async)
{
    return uv_async_send(&async->u.async);
}

CAT_API cat_bool_t cat_async_wait_and_close(cat_async_t *async, cat_async_cleanup_callback cleanup, cat_timeout_t timeout)
{
    CAT_ASYNC_CHECK_AVAILABILITY(async, return cat_false);
    cat_bool_t ret;

    async->cleanup = cleanup;

    if (!async->done) {
        async->coroutine = CAT_COROUTINE_G(current);
        ret = cat_time_wait(timeout);
        async->coroutine = NULL;
        /* we can not close it here, because thread may access it later,
         * it will be closed in notify callback */
        async->closing = cat_true;

        if (unlikely(!ret)) {
            CAT_ASSERT(!async->done);
            cat_update_last_error_with_previous("Async wait failed");
            return cat_false;
        }
        if (unlikely(!async->done)) {
            cat_update_last_error(CAT_ECANCELED, "Async wait has been canceled");
            return cat_false;
        }
    } /* else thread done the work before we wait it, fast return */
    uv_close(&async->u.handle, cat_async_close_callback); /* close it if it was done */

    return cat_true;
}

CAT_API cat_bool_t cat_async_cleanup(cat_async_t *async, cat_async_cleanup_callback cleanup)
{
    CAT_ASYNC_CHECK_AVAILABILITY(async, return cat_false);

    async->cleanup = cleanup;

    if (async->done) {
        /* fast close it if it was done */
        uv_close(&async->u.handle, cat_async_close_callback);
    } else {
        /* we can not close it here, because thread may access it later,
         * it will be closed in notify callback */
        async->closing = cat_true;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_async_close(cat_async_t *async, cat_async_cleanup_callback cleanup)
{
    CAT_ASYNC_CHECK_AVAILABILITY(async, return cat_false);

    async->cleanup = cleanup;
    uv_close(&async->u.handle, cat_async_close_callback);

    return cat_true;
}
