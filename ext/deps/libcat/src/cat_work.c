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

#include "cat_work.h"
#include "cat_coroutine.h"
#include "cat_event.h"
#include "cat_time.h"

typedef struct cat_work_context_s {
    union {
        cat_coroutine_t *coroutine;
        uv_req_t req;
        uv_work_t work;
    } request;
    cat_work_function_t function;
    cat_work_cleanup_callback_t cleanup;
    cat_data_t *data;
    int status;
} cat_work_context_t;

void cat_work_callback(uv_work_t *request)
{
    cat_work_context_t *context = (cat_work_context_t *) request;
    context->function(context->data);
}

static void cat_work_after_done(uv_work_t *request, int status)
{
    cat_work_context_t *context = (cat_work_context_t *) request;

    if (likely(context->request.coroutine != NULL)) {
        context->status = status;
        cat_coroutine_schedule(context->request.coroutine, WORK, "Work");
    }

    if (context->cleanup != NULL) {
        context->cleanup(context->data);
    }
    cat_free(context);
}

CAT_API cat_bool_t cat_work(cat_work_kind_t kind, cat_work_function_t function, cat_work_cleanup_callback_t cleanup, cat_data_t *data, cat_timeout_t timeout)
{
    cat_work_context_t *context = (cat_work_context_t *) cat_malloc(sizeof(*context));
    cat_bool_t ret;

#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(context == NULL)) {
        cat_update_last_error_of_syscall("Malloc for work context failed");
        if (cleanup != NULL) {
            cleanup(data);
        }
        return cat_false;
    }
#endif
    context->function = function;
    context->cleanup = cleanup;
    context->data = data;
    (void) uv_queue_work_ex(&CAT_EVENT_G(loop), &context->request.work, (uv_work_kind) kind, cat_work_callback, cat_work_after_done);
    context->status = CAT_ECANCELED;
    context->request.coroutine = CAT_COROUTINE_G(current);
    ret = cat_time_wait(timeout);
    context->request.coroutine = NULL;
    if (unlikely(!ret)) {
        cat_update_last_error_with_previous("Work wait failed");
        (void) uv_cancel(&context->request.req);
        return cat_false;
    }
    if (unlikely(context->status != 0)) {
        cat_update_last_error_with_reason(context->status, "Work failed");
        return cat_false;
    }

    return cat_true;
}
