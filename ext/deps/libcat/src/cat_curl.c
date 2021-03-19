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

#include "cat_curl.h"
#include "cat_event.h"
#include "cat_time.h"

#ifdef CAT_CURL

typedef struct {
    uv_poll_t poll_handle;
    curl_socket_t sockfd;
    CURLM *multi;
    cat_coroutine_t *coroutine;
} cat_curl_socket_context_t;

typedef struct {
    CURLM *multi;
    cat_coroutine_t *coroutine;
    uv_timer_t *timer;
} cat_curl_multi_context_t;

typedef struct {
    cat_bool_t defered;
    uint32_t numfds;
} cat_curl_coroutine_transfer_t;

CAT_GLOBALS_STRUCT_BEGIN(cat_curl)
    cat_curl_multi_context_t context;
    uv_timer_t timer;
CAT_GLOBALS_STRUCT_END(cat_curl)

CAT_API CAT_GLOBALS_DECLARE(cat_curl)

#define CAT_CURL_G(x) CAT_GLOBALS_GET(cat_curl, x)

CAT_API CAT_GLOBALS_DECLARE(cat_curl)

CAT_GLOBALS_CTOR_DECLARE_SZ(cat_curl)

static void cat_curl_multi_check_info(CURLM *multi)
{
    CURLMsg *message;
    int pending;

    while ((message = curl_multi_info_read(multi, &pending))) {
        switch (message->msg) {
            case CURLMSG_DONE: {
#ifdef CAT_DEBUG
                char *done_url;
                curl_easy_getinfo(message->easy_handle, CURLINFO_EFFECTIVE_URL, &done_url);
                cat_debug(EXT, "Curl query '%s' DONE", done_url);
#endif
                // curl_easy_cleanup(message->easy_handle);
                cat_coroutine_t *coroutine = NULL;
                curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &coroutine);
                CAT_ASSERT(coroutine != NULL && "handle should be removed if coroutine is not in waiting");
                cat_coroutine_resume(coroutine, (cat_data_t *) &message->data.result, NULL);
                break;
            }
            default:
                CAT_NEVER_HERE("Impossible");
        }
    }
}

static void cat_curl_multi_defer_callback(cat_data_t *data)
{
    cat_coroutine_resume((cat_coroutine_t *) data, NULL, NULL);
}

static void cat_curl_multi_defer(cat_curl_coroutine_transfer_t *transfer)
{
    transfer->numfds++;
    if (!transfer->defered) {
        cat_bool_t ret;
        ret = cat_event_defer(cat_curl_multi_defer_callback, transfer);
        if (!ret) {
            cat_warn_with_last(EXT, "Curl multi defer failed");
            return;
        }
        transfer->defered = cat_true;
    }
}

static cat_always_inline void cat_curl_multi_notify(CURLM *multi, cat_coroutine_t *coroutine, cat_bool_t immediately)
{
    if (coroutine == NULL) {
        cat_curl_multi_check_info(multi);
    } else {
        cat_curl_coroutine_transfer_t *transfer = (cat_curl_coroutine_transfer_t *) coroutine;
        if (!immediately || transfer->defered) {
            cat_curl_multi_defer(transfer);
        } else {
            cat_coroutine_resume(coroutine, NULL, NULL);
        }
    }
}

static void cat_curl_poll_callback(uv_poll_t *request, int status, int events)
{
    cat_curl_socket_context_t *context;
    int flags = 0, running_handles;

    if (events & UV_READABLE) {
        flags |= CURL_CSELECT_IN;
    }
    if (events & UV_WRITABLE) {
        flags |= CURL_CSELECT_OUT;
    }

    context = (cat_curl_socket_context_t *) request->data;
    curl_multi_socket_action(context->multi, context->sockfd, flags, &running_handles);
    cat_curl_multi_notify(context->multi, context->coroutine, cat_false);
}

static int cat_curl_socket_function(CURL *ch, curl_socket_t sockfd, int action, cat_curl_multi_context_t *mcontext, cat_curl_socket_context_t *context)
{
    int events = 0;

    switch (action) {
        case CURL_POLL_IN:
        case CURL_POLL_OUT:
        case CURL_POLL_INOUT:
            if (unlikely(context == NULL)) {
                context = (cat_curl_socket_context_t *) cat_malloc(sizeof(*context));
                context->multi = mcontext->multi;
                context->coroutine = mcontext->coroutine;
                context->sockfd = sockfd;
                uv_poll_init_socket(cat_event_loop, &context->poll_handle, sockfd);
                context->poll_handle.data = context;
            }
            curl_multi_assign(mcontext->multi, sockfd, (void *) context);
            if (action != CURL_POLL_IN) {
                events |= UV_WRITABLE;
            }
            if (action != CURL_POLL_OUT) {
                events |= UV_READABLE;
            }
            uv_poll_start(&context->poll_handle, events, cat_curl_poll_callback);
            break;
        case CURL_POLL_REMOVE:
            if (context != NULL) {
                uv_poll_stop(&(context)->poll_handle);
                uv_close((uv_handle_t *) &context->poll_handle, (uv_close_cb) cat_free_function);
                curl_multi_assign(mcontext->multi, sockfd, NULL);
            }
            break;
        default:
            CAT_NEVER_HERE("Impossible");
    }

    return 0;
}

static void cat_curl_on_timeout(uv_timer_t *timer)
{
    cat_curl_multi_context_t *context = (cat_curl_multi_context_t *) timer->data;
    int running_handles;

    curl_multi_socket_action(context->multi, CURL_SOCKET_TIMEOUT, 0, &running_handles);
    cat_curl_multi_notify(context->multi, context->coroutine, cat_true);
}

static int cat_curl_start_timeout(CURLM *multi, long timeout, cat_curl_multi_context_t *context)
{
    if (timeout >= 0) {
        if (timeout == 0) {
            timeout = 1; /* 0 means directly call socket_action, but we'll do it in a bit */
        }
        context->timer->data = context;
        uv_timer_start(context->timer, cat_curl_on_timeout, timeout, 0);
    } else if (context->timer != NULL) {
        uv_timer_stop(context->timer);
    }

    return 0;
}

static cat_always_inline void cat_curl_multi_configure(CURLM *multi, void *context)
{
    curl_multi_setopt(multi, CURLMOPT_SOCKETFUNCTION, cat_curl_socket_function);
    curl_multi_setopt(multi, CURLMOPT_SOCKETDATA, context);
    curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, cat_curl_start_timeout);
    curl_multi_setopt(multi, CURLMOPT_TIMERDATA, context);
}

static cat_always_inline void cat_curl_multi_unconfigure(CURLM *multi)
{
    curl_multi_setopt(multi, CURLMOPT_SOCKETFUNCTION, NULL);
    curl_multi_setopt(multi, CURLMOPT_SOCKETDATA, NULL);
    curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, NULL);
    curl_multi_setopt(multi, CURLMOPT_TIMERDATA, NULL);
}

CAT_API cat_bool_t cat_curl_module_init(void)
{
    CAT_GLOBALS_REGISTER(cat_curl, CAT_GLOBALS_CTOR(cat_curl), NULL);

	if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        cat_warn_with_reason(EXT, CAT_UNKNOWN, "Curl init failed");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_curl_module_shutdown(void)
{
    curl_global_cleanup();

    return cat_true;
}

CAT_API cat_bool_t cat_curl_runtime_init(void)
{
    CAT_CURL_G(context).multi = curl_multi_init();
    CAT_CURL_G(context).coroutine = NULL;
    CAT_CURL_G(context).timer = &CAT_CURL_G(timer);
    (void) uv_timer_init(cat_event_loop, &CAT_CURL_G(timer));
    cat_curl_multi_configure(CAT_CURL_G(context).multi, &CAT_CURL_G(context));

    return cat_true;
}

CAT_API cat_bool_t cat_curl_runtime_shutdown(void)
{
    if (CAT_CURL_G(context).timer != NULL) {
        uv_close((uv_handle_t *) CAT_CURL_G(context).timer, NULL);
    }
    curl_multi_cleanup(CAT_CURL_G(context).multi);

    return cat_true;
}

CAT_API CURLcode cat_curl_easy_perform(CURL *ch)
{
    CURLcode *ret = NULL;

    do {
        cat_coroutine_t *coroutine = NULL;
        curl_easy_getinfo(ch, CURLINFO_PRIVATE, &coroutine);
        if (unlikely(coroutine != NULL)) {
            /* can not find appropriate error code */
            return CURLE_AGAIN;
        }
    } while (0);

    curl_multi_add_handle(CAT_CURL_G(context).multi, ch);
    curl_easy_setopt(ch, CURLOPT_PRIVATE, CAT_COROUTINE_G(current));
    cat_coroutine_yield(NULL, (cat_data_t **) &ret);
    curl_easy_setopt(ch, CURLOPT_PRIVATE, NULL);
    curl_multi_remove_handle(CAT_CURL_G(context).multi, ch);
    if (ret == NULL) {
        /* canceled */
        return CURLE_RECV_ERROR;
    }

    return *ret;
}

CAT_API CURLMcode cat_curl_multi_wait(
    CURLM *multi,
    struct curl_waitfd *extra_fds, unsigned int extra_nfds,
    int timeout_ms, int *numfds
)
{
    cat_curl_multi_context_t context;
    CURLMcode mcode;

    CAT_ASSERT(extra_fds == NULL && "Not support");
    CAT_ASSERT(extra_nfds == 0 && "Not support");

    context.multi = multi;
    context.coroutine = CAT_COROUTINE_G(current);
    context.timer = (uv_timer_t *) cat_malloc(sizeof(*context.timer));
    if (unlikely(context.timer == NULL)) {
        return CURLM_OUT_OF_MEMORY;
    }
    (void) uv_timer_init(cat_event_loop, context.timer);

    cat_curl_multi_configure(multi, &context);

    do {
        int still_running;
        mcode = curl_multi_perform(multi, &still_running);
    } while (mcode == CURLM_CALL_MULTI_PERFORM);

    while (1) {
        cat_curl_coroutine_transfer_t *transfer;
        cat_bool_t ret;

        transfer = (cat_curl_coroutine_transfer_t *) CAT_COROUTINE_G(current);
        transfer->defered = cat_false;
        transfer->numfds = 0;

        ret = cat_time_wait(timeout_ms);

        if (numfds) {
            *numfds = transfer->numfds;
        }

        if (!ret) {
            break;
        }
        if (!uv_is_active((uv_handle_t *) context.timer)) {
            break;
        }
    }

    if (context.timer != NULL) {
        uv_close((uv_handle_t *) context.timer, (uv_close_cb) cat_free_function);
    }

    cat_curl_multi_unconfigure(multi);
    do {
        int still_running;
        mcode = curl_multi_perform(multi, &still_running);
    } while (mcode == CURLM_CALL_MULTI_PERFORM);

    return CURLM_OK;
}

#endif /* CAT_CURL */
