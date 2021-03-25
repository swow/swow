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

typedef struct
{
    union {
        cat_coroutine_t *coroutine;
        uv_req_t req;
        uv_work_t work;
    } request;
    cat_work_function_t function;
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
        if (unlikely(!cat_coroutine_resume(context->request.coroutine, NULL, NULL))) {
            cat_core_error_with_last(WORK, "Work schedule failed");
        }
    }

    cat_free(context);
}

CAT_API cat_bool_t cat_work(cat_work_kind_t kind, cat_work_function_t function, cat_data_t *data, cat_timeout_t timeout)
{
    cat_work_context_t *context = (cat_work_context_t *) cat_malloc(sizeof(*context));
    int error;
    cat_bool_t ret;

    if (unlikely(context == NULL)) {
        cat_update_last_error_of_syscall("Malloc for work context failed");
        return cat_false;
    }
    context->function = function;
    context->data = data;
    error = uv_queue_work_ex(cat_event_loop, &context->request.work, (uv_work_kind) kind, cat_work_callback, cat_work_after_done);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Work queue failed");
        return cat_false;
    }
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
        if (context->status == CAT_ECANCELED) {
            cat_update_last_error(CAT_ECANCELED, "Work has been canceled");
            (void) uv_cancel(&context->request.req);
        } else {
            cat_update_last_error_with_reason(context->status, "Work failed");
        }
        return cat_false;
    }

    return cat_true;
}
