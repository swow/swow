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

#ifdef CAT_CURL

#include "cat_coroutine.h"
#include "cat_buffer.h"
#include "cat_event.h"
#include "cat_poll.h"
#include "cat_queue.h"
#include "cat_time.h"

#include "uv/tree.h"

/* declarations */

// CURLM_CALL_MULTI_PERFORM/CURLM_CALL_MULTI_SOCKET, deprecated since curl 7.20.0
#define CAT_CURL_MULTI_CANCELLED ((CURLMcode)-1)

typedef struct cat_curl_multi_context_s {
    RB_ENTRY(cat_curl_multi_context_s) tree_entry;
    CURLM *multi;
    uv_timer_t timer;
    cat_coroutine_t *waiter;
    cat_msec_t timeout_due_time;
    cat_bool_t timedout; // timed out or initializing
    cat_queue_t socket_contexts;
} cat_curl_multi_context_t;

RB_HEAD(cat_curl_multi_context_tree_s, cat_curl_multi_context_s);

static int cat_curl__multi_context_compare(cat_curl_multi_context_t* c1, cat_curl_multi_context_t* c2)
{
    uintptr_t m1 = (uintptr_t) c1->multi;
    uintptr_t m2 = (uintptr_t) c2->multi;
    if (m1 < m2) {
        return -1;
    }
    if (m1 > m2) {
        return 1;
    }
    return 0;
}

RB_GENERATE_STATIC(cat_curl_multi_context_tree_s,
                   cat_curl_multi_context_s, tree_entry,
                   cat_curl__multi_context_compare);

typedef struct cat_curl_multi_socket_context_s {
    cat_queue_node_t node;
    curl_socket_t sockfd;
    int poll_uv_events; // events to be polled
    int curl_events; // events to be processed
    cat_curl_multi_context_t *context;
    uv_poll_t poll;
} cat_curl_multi_socket_context_t;

#define CAT_CURL_MULTI_FOREACH_SOCKET_CONTEXT(queue, var) \
    for ( \
        cat_curl_multi_socket_context_t *var = \
            (cat_curl_multi_socket_context_t *) cat_queue_next(queue); \
        var != (cat_curl_multi_socket_context_t *) (queue); \
        var = (cat_curl_multi_socket_context_t *) cat_queue_next((cat_queue_t *) var) \
    )

/* globals */

CAT_GLOBALS_STRUCT_BEGIN(cat_curl) {
    struct cat_curl_multi_context_tree_s multi_tree;
    cat_queue_t socket_contexts;
} CAT_GLOBALS_STRUCT_END(cat_curl);

CAT_GLOBALS_DECLARE(cat_curl);

#define CAT_CURL_G(x) CAT_GLOBALS_GET(cat_curl, x)

/* utils */

#define CURL_CSELECT_NONE 0

static cat_always_inline void cat_curl_multi_configure(CURLM *multi, void *socket_function, void *timer_function, void *context)
{
    curl_multi_setopt(multi, CURLMOPT_SOCKETFUNCTION, socket_function);
    curl_multi_setopt(multi, CURLMOPT_SOCKETDATA, context);
    curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, timer_function);
    curl_multi_setopt(multi, CURLMOPT_TIMERDATA, context);
}

#ifdef CAT_ENABLE_DEBUG_LOG
static const char *cat_curl_action_name(int action)
{
#define CAT_CURL_ACTION_MAP(XX) \
    XX(NONE) \
    XX(IN) \
    XX(OUT) \
    XX(INOUT) \
    XX(REMOVE)
#define CAT_CURL_ACTION_NAME_GEN(name) case CURL_POLL_##name: return "CURL_POLL_" #name;
    switch (action) {
        CAT_CURL_ACTION_MAP(CAT_CURL_ACTION_NAME_GEN);
        default: CAT_NEVER_HERE("Non-exist");
    }
#undef CAT_CURL_ACTION_NAME_GEN
#undef CAT_CURL_ACTION_MAP
}
#endif

#ifdef CAT_ENABLE_DEBUG_LOG
extern char *cat_poll_uv_events_str(int events);
#endif

/* multi backends */

static cat_always_inline CURLMcode cat_curl_multi_socket_action(CURLM *multi, curl_socket_t sockfd, int action, int *running_handles)
{
    CAT_ASSERT(running_handles != NULL);
    CURLMcode mcode = curl_multi_socket_action(multi, sockfd, action, running_handles);
    CAT_LOG_DEBUG_VA_WITH_LEVEL(CURL, 2, {
        if (sockfd == CURL_SOCKET_TIMEOUT) {
            CAT_LOG_DEBUG_D(CURL, "libcurl::curl_multi_socket_action(multi: %p, TIMEOUT, running_handles: %d) = %d (%s)",
                multi, *running_handles, mcode, curl_multi_strerror(mcode));
        } else {
            CAT_LOG_DEBUG_D(CURL, "libcurl::curl_multi_socket_action(multi: %p, sockfd: %d, %s, running_handles: %d) = %d (%s)",
                multi, sockfd, cat_curl_action_name(action), *running_handles, mcode, curl_multi_strerror(mcode));
        }
    });
    return mcode;
}

static void cat_curl_multi_socket_context_close_callback(uv_handle_t *handle)
{
    cat_curl_multi_socket_context_t *socket_context = (cat_curl_multi_socket_context_t*) handle->data;
    cat_free(socket_context);
}

static cat_always_inline void cat_curl_multi_socket_context_close(cat_curl_multi_socket_context_t *socket_context)
{
    cat_queue_remove(&socket_context->node);
    (void) uv_poll_stop(&socket_context->poll);
    uv_close((uv_handle_t*) &socket_context->poll, cat_curl_multi_socket_context_close_callback);
}

static cat_always_inline void cat_curl_multi_socket_schedule(cat_curl_multi_context_t *context, curl_socket_t sockfd, int action)
{
    // insert to context->socket_contexts
#ifdef CAT_DEBUG
    cat_bool_t found = cat_false;
#endif
    CAT_CURL_MULTI_FOREACH_SOCKET_CONTEXT(&context->socket_contexts, socket_context) {
        if (socket_context->sockfd == sockfd) {
            socket_context->curl_events |= action;
#ifdef CAT_DEBUG
            found = cat_true;
#endif
            break;
        }
    }
#ifdef CAT_DEBUG
    CAT_ASSERT(found);
#endif
    if (context->waiter != NULL) {
        cat_coroutine_schedule(context->waiter, CURL, "Poll event from CURL socket function");
    }
}

static void cat_curl_multi_socket_poll_callback(uv_poll_t *poll, int status, int events)
{
    cat_curl_multi_socket_context_t *socket_context = (cat_curl_multi_socket_context_t *) poll->data;
    cat_curl_multi_context_t *context = socket_context->context;
    curl_socket_t sockfd = socket_context->sockfd;
    int action = 0;

    // always one shot
    (void) uv_poll_stop(&socket_context->poll);

    CAT_LOG_DEBUG_VA_WITH_LEVEL(POLL, 2, {
        char *events_str = cat_poll_uv_events_str(events);
        CAT_LOG_DEBUG_D(POLL, "curl_multi_socket_poll_callback(sockfd: " CAT_OS_SOCKET_FMT ", status: %d" CAT_LOG_STRERRNO_FMT ", events: %s)",
            sockfd, status, CAT_LOG_STRERRNO_C(status == 0, status), events_str);
        cat_buffer_str_free(events_str);
    });

    if (status < 0) {
        action = CURL_CSELECT_ERR;
    } else {
        if (events & UV_READABLE) {
            action |= CURL_CSELECT_IN;
        }
        if (events & UV_WRITABLE) {
            action |= CURL_CSELECT_OUT;
        }
        if (events & UV_DISCONNECT) {
            action |= CURL_CSELECT_ERR;
        }
    }

    cat_curl_multi_socket_schedule(context, sockfd, action);
}

static int cat_curl_multi_socket_function(
    CURL *ch, curl_socket_t sockfd, int action,
    cat_curl_multi_context_t *context,
    cat_curl_multi_socket_context_t *socket_context
) {
    (void) ch;
    CURLM *multi = context->multi;

    CAT_LOG_DEBUG_V2(CURL, "libcurl::curl_multi_socket_function(multi: %p, sockfd: %d, action=%s, socket_context=%p)",
        multi, sockfd, cat_curl_action_name(action), socket_context);

    switch (action) {
        case CURL_POLL_IN:
            /* fallthrough */
        case CURL_POLL_OUT:
            /* fallthrough */
        case CURL_POLL_INOUT: {
            if (socket_context == NULL) {
                // first time to assign
                socket_context = (cat_curl_multi_socket_context_t *) cat_malloc(sizeof(*socket_context));
#if CAT_ALLOC_HANDLE_ERRORS
                if (unlikely(socket_context == NULL)) {
                    return CURLM_OUT_OF_MEMORY;
                }
#endif
                cat_bool_t assigned = curl_multi_assign(multi, sockfd, socket_context) == CURLM_OK;
                if (unlikely(!assigned)) {
                    cat_free(socket_context);
                    /* Fixed after 8.10.1, but i don't know which version it starts from,
                     * see https://github.com/curl/curl/pull/15206. */
#if LIBCURL_VERSION_NUM <= 0x080a01
                    return CURLM_OK; // ignore
#else
# ifdef CAT_DEBUG
                    abort();
# else
                    return CURLM_INTERNAL_ERROR;
# endif
#endif
                }

                socket_context->sockfd = sockfd;
                socket_context->curl_events = 0;
                socket_context->context = context;
                (void) uv_poll_init_socket(&CAT_EVENT_G(loop), &socket_context->poll, sockfd);
                socket_context->poll.data = socket_context;
                cat_queue_push_back(&context->socket_contexts, &socket_context->node);
            }

            // update uv_events
            socket_context->poll_uv_events = 0;
            if(action != CURL_POLL_OUT) {
                socket_context->poll_uv_events |= UV_READABLE;
            }
            if(action != CURL_POLL_IN) {
                socket_context->poll_uv_events |= UV_WRITABLE;
            }
            (void) uv_poll_start(
                &socket_context->poll, socket_context->poll_uv_events, cat_curl_multi_socket_poll_callback
            );
            break;
        }
        case CURL_POLL_REMOVE:
            if (socket_context != NULL) {
                curl_multi_assign(multi, sockfd, NULL);
                cat_curl_multi_socket_context_close(socket_context);
            }
            break;
        default:
            abort();
    }

    return CURLM_OK;
}

static void cat_curl_multi_timeout_callback(uv_timer_t *timer)
{
    cat_curl_multi_context_t *context = timer->data;
    CAT_LOG_DEBUG_V2(CURL, "libcurl::cat_curl_multi_on_timeout(multi: %p)", context->multi);
    cat_msec_t now = cat_time_msec();
    if (unlikely(context->timeout_due_time > now)) {
        // this callback returns early (due to uv and libcurl clock alignment), sleep again
        // should we use real time slice here?
#ifdef CAT_OS_WIN
        // for default tick 15.6ms
        cat_msec_t retry_timeout_ms = 16;
#else
        // for CONFIG_HZ=100
        cat_msec_t retry_timeout_ms = 10;
#endif
        if (retry_timeout_ms < context->timeout_due_time - now) {
            retry_timeout_ms = context->timeout_due_time - now;
        }
        uv_timer_start(timer, cat_curl_multi_timeout_callback, retry_timeout_ms, 0);
        return;
    }

    context->timedout = cat_true;
    if (context->waiter != NULL) {
        cat_coroutine_schedule(context->waiter, CURL, "Timeout from CURL timeout callback");
    }
}

static int cat_curl_multi_timeout_function(CURLM *multi, long timeout_ms, cat_curl_multi_context_t *context)
{
    (void) multi;
    CAT_LOG_DEBUG_V2(CURL, "libcurl::curl_multi_timeout_function(multi: %p, timeout_ms=%ld)", multi, timeout_ms);

    if (timeout_ms < 0) {
        (void) uv_timer_stop(&context->timer);
    } else {
        if (timeout_ms == 0) {
            /* 0 means directly call socket_action, but we'll do it in a bit */
            timeout_ms = 1;
        }
        context->timeout_due_time = cat_time_msec() + (cat_msec_t) timeout_ms;
        (void) uv_timer_start(&context->timer, cat_curl_multi_timeout_callback, timeout_ms, 0);
    }

    return CURLM_OK;
}

/* multi context */

static cat_always_inline cat_curl_multi_context_t *cat_curl_multi_get_context(CURLM *multi)
{
    cat_curl_multi_context_t lookup;
    lookup.multi = multi;
    return RB_FIND(cat_curl_multi_context_tree_s, &CAT_CURL_G(multi_tree), &lookup);
}

static void cat_curl_multi_context_close_callback(uv_handle_t *handle)
{
    uv_timer_t *timer = (uv_timer_t *) handle;
    cat_curl_multi_context_t *context = cat_container_of(timer, cat_curl_multi_context_t, timer);
    cat_free(context);
}

static void cat_curl_multi_context_close(cat_curl_multi_context_t *context)
{
    /* we assume that all resources should have been released in curl_multi_socket_function() before,
     * but when fatal error occurred and we called curl_multi_cleanup() without calling
     * curl_multi_remove_handle(), some will not be removed from context.  */
    RB_REMOVE(cat_curl_multi_context_tree_s, &CAT_CURL_G(multi_tree), context);
    if (context->waiter != NULL) {
        cat_coroutine_schedule(context->waiter, CURL, "Multi context close");
    }
    uv_close((uv_handle_t *) &context->timer, cat_curl_multi_context_close_callback);
    // should be empty, but if user called curl_multi_wait() and the transfer is not finished,
    // some sockets may not be removed from the multi context due to a curl bug.
    while (!cat_queue_empty(&context->socket_contexts)) {
        cat_curl_multi_socket_context_t *socket_context = 
            (cat_curl_multi_socket_context_t *) cat_queue_front(&context->socket_contexts);
        cat_curl_multi_socket_context_close(socket_context);
    }
}

static void cat_curl_multi_close_context(CURLM *multi)
{
    cat_curl_multi_context_t *context;

    context = cat_curl_multi_get_context(multi);
    CAT_ASSERT(context != NULL);
    cat_curl_multi_context_close(context);
}

static cat_curl_multi_context_t *cat_curl_multi_create_context(CURLM *multi)
{
    cat_curl_multi_context_t *context;

    CAT_LOG_DEBUG_V2(CURL, "curl_multi_context_create(multi: %p)", multi);

    context = (cat_curl_multi_context_t *) cat_malloc(sizeof(*context));
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(context == NULL)) {
        return NULL;
    }
#endif

    context->multi = multi;
    context->waiter = NULL;
    context->timedout = cat_true;
    cat_queue_init(&context->socket_contexts);
    uv_timer_init(&CAT_EVENT_G(loop), &context->timer);
    context->timer.data = context;

    /* following is outdated comment, but I didn't understand the specific meaning,
     * so I won't remove it yet:
     *   latest multi has higher priority
     *   (previous may leak and they would be free'd in shutdown)
     * code before:
     *   cat_queue_push_front(&CAT_CURL_G(multi_map), &context->node); */
    RB_INSERT(cat_curl_multi_context_tree_s, &CAT_CURL_G(multi_tree), context);

    cat_curl_multi_configure(
        multi,
        (void *) cat_curl_multi_socket_function,
        (void *) cat_curl_multi_timeout_function,
        context
    );

    return context;
}

/* multi impl */

static CURLMcode cat_curl_multi_wait_impl(
    CURLM *multi, int timeout_ms, int *numfds, int *running_handles
)
{
    CAT_ASSERT(running_handles != NULL);
    cat_curl_multi_context_t *context = cat_curl_multi_get_context(multi);
    CAT_ASSERT(context != NULL);
    CURLMcode mcode;
    cat_timeout_t remaining = timeout_ms;
    cat_ret_t ret;
    int _numfds;
    if (numfds == NULL) {
        numfds = &_numfds;
    }
    *numfds = 0;

    // kickstart wait
    context->timedout = cat_false;
    mcode = cat_curl_multi_socket_action(
        multi, CURL_SOCKET_TIMEOUT, 0, running_handles
    );
    if (unlikely(mcode != CURLM_OK || *running_handles == 0)) {
        // failed or this multi handle is already done
        goto end;
    }

    if (context->waiter != NULL) {
        // since 7.59.0
#ifdef CURLM_RECURSIVE_API_CALL
        return CURLM_RECURSIVE_API_CALL;
#else
        return CURLM_INTERNAL_ERROR;
#endif
    }

    mcode = CURLM_OK;
    CAT_CURL_MULTI_FOREACH_SOCKET_CONTEXT(&context->socket_contexts, socket_context) {
        if (socket_context->curl_events != 0) {
            // process remaining curl events
            (*numfds)++;
            mcode = cat_curl_multi_socket_action(
                multi, socket_context->sockfd, socket_context->curl_events, running_handles
            );
            if (unlikely(mcode != CURLM_OK)) {
                break;
            }
            socket_context->curl_events = 0;
        }
        if (*numfds == 0 && socket_context->poll_uv_events != 0) {
            // if no events happened, restore polls
            // otherwise, multi_wait processed events, return immediately
            (void) uv_poll_start(
                &socket_context->poll, socket_context->poll_uv_events, cat_curl_multi_socket_poll_callback
            );
        }
    }
    if (*numfds > 0) {
        // some (remaining) events is processed
        goto end;
    }

    // wait for timeout or event
    context->waiter = CAT_COROUTINE_G(current);
    if (remaining >= 0) {
        if (remaining == 0) {
            remaining = 1;
        }
        ret = cat_time_delay((cat_timeout_t) remaining);
    } else {
        ret = cat_time_delay(-1);
    }
    context->waiter = NULL;
    if (unlikely(ret == CAT_RET_ERROR)) {
        // CAT_RET_ERROR for wait encountered errors
        mcode = CURLM_INTERNAL_ERROR;
        goto end;
    } else if (ret == CAT_RET_OK) {
        // CAT_RET_OK for wait arrived, timeout in argument arrived
        goto end;
    }
#ifdef CAT_DEBUG
    // else, CAT_RET_NONE for wait being interrupted, cancelled or events happened
    CAT_ASSERT(ret == CAT_RET_NONE);
#endif
    if (context->timedout) {
        // wait is interrupted with timer function
        // call socket_action and stop waiting
        context->timedout = cat_false;
        mcode = cat_curl_multi_socket_action(
            multi, CURL_SOCKET_TIMEOUT, 0, running_handles
        );
        goto end;
    }

    // process events
    CAT_CURL_MULTI_FOREACH_SOCKET_CONTEXT(&context->socket_contexts, socket_context) {
        if (socket_context->curl_events == 0) {
            continue;
        }
        (*numfds)++;
        mcode = cat_curl_multi_socket_action(
            multi, socket_context->sockfd, socket_context->curl_events, running_handles
        );
        if (unlikely(mcode != CURLM_OK)) {
            break;
        }
        socket_context->curl_events = 0;
    }

    if (*numfds == 0) {
        // no events happened, wait is interrupted, cancelled
        mcode = CAT_CURL_MULTI_CANCELLED;
        goto end;
    }

end:
    // stop all polls
    CAT_CURL_MULTI_FOREACH_SOCKET_CONTEXT(&context->socket_contexts, socket_context) {
        if (socket_context->poll_uv_events == 0) {
            continue;
        }
        (void) uv_poll_stop(&socket_context->poll);
    }

    return mcode;
}

static cat_always_inline CURLMcode cat_curl_multi_perform_impl(CURLM *multi, int *running_handles)
{
    CURLMcode mcode = cat_curl_multi_wait_impl(multi, 0, NULL, running_handles);
    if (unlikely(mcode == CAT_CURL_MULTI_CANCELLED)) {
        // do not return CAT_CURL_MULTI_CANCELLED to user, use it as an internal error
        return CURLM_INTERNAL_ERROR;
    }
    return mcode;
}

/* easy APIs  */

static CURLcode cat_curl_easy_perform_impl(CURL *ch)
{
    CURLM *multi;
    CURLMsg *message = NULL;
    CURLcode code = CURLE_RECV_ERROR;
    CURLMcode mcode;
    int running_handles;

    multi = cat_curl_multi_init();
    if (unlikely(multi == NULL)) {
        return CURLE_OUT_OF_MEMORY;
    }
    mcode = curl_multi_add_handle(multi, ch);
    if (unlikely(mcode != CURLM_OK)) {
#if LIBCURL_VERSION_NUM >= 0x072001 /* Available since 7.32.1 */
/* See: https://github.com/curl/curl/commit/19122c07682c268c2383218f62e09c3d24a41e76 */
        if (mcode == CURLM_ADDED_ALREADY) {
            /* cURL is busy with IO,
             * and can not find appropriate error code. */
            code = CURLE_AGAIN;
        }
#endif
        goto _add_failed;
    }

    while (1) {
        mcode = cat_curl_multi_wait_impl(multi, -1, NULL, &running_handles);
        if (unlikely(mcode != CURLM_OK)) {
            if (unlikely(mcode == CAT_CURL_MULTI_CANCELLED)) {
                // do not return CAT_CURL_MULTI_CANCELLED to user, use it as an internal error
                mcode = CURLM_INTERNAL_ERROR;
            }
            goto _error;
        }
        if (running_handles == 0) {
            // done
            break;
        }
    }

    message = curl_multi_info_read(multi, &running_handles);
    CAT_LOG_DEBUG_V2(CURL, "libcurl::curl_multi_info_read(ch: %p) = %p", ch, message);
    if (message != NULL) {
        CAT_ASSERT(message->msg == CURLMSG_DONE);
        CAT_LOG_DEBUG_VA(CURL, {
            char *done_url;
            curl_easy_getinfo(message->easy_handle, CURLINFO_EFFECTIVE_URL, &done_url);
            CAT_LOG_DEBUG_V2(CURL, "libcurl::curl_easy_getinfo(ch: %p, CURLINFO_EFFECTIVE_URL, url=\"%s\")", message->easy_handle, done_url);
        });
        code = message->data.result;
    }

    _error:
    curl_multi_remove_handle(multi, ch);
    _add_failed:
    cat_curl_multi_cleanup(multi);

    return code;
}

CAT_API CURLcode cat_curl_easy_perform(CURL *ch)
{
    CAT_LOG_DEBUG(CURL, "curl_easy_perform(ch: %p) = " CAT_LOG_UNFINISHED_STR, ch);

    CURLcode code = cat_curl_easy_perform_impl(ch);

    CAT_LOG_DEBUG(CURL, "curl_easy_perform(ch: %p) = %d (%s)", ch, code, curl_easy_strerror(code));

    return code;
}

CAT_API CURLM *cat_curl_multi_init(void)
{
    CURLM *multi = curl_multi_init();
    cat_curl_multi_context_t *context;

    CAT_LOG_DEBUG(CURL, "curl_multi_init(multi: %p)", multi);

    if (unlikely(multi == NULL)) {
        return NULL;
    }

    context = cat_curl_multi_create_context(multi);
#ifdef CAT_ALLOC_NEVER_RETURNS_NULL
    (void) context;
#else
    if (unlikely(context == NULL)) {
        (void) curl_multi_cleanup(multi);
        return NULL;
    }
#endif

    return multi;
}

CAT_API CURLMcode cat_curl_multi_cleanup(CURLM *multi)
{
    CURLMcode mcode;

    mcode = curl_multi_cleanup(multi);
    /* we do not know whether libcurl would do something during cleanup,
     * so we close the context later */
    cat_curl_multi_close_context(multi);

    CAT_LOG_DEBUG(CURL, "curl_multi_cleanup(multi: %p) = %d (%s)", multi, mcode, curl_multi_strerror(mcode));

    return mcode;
}

CAT_API CURLMcode cat_curl_multi_perform(CURLM *multi, int *running_handles)
{
    int _running_handles = -1;
    if (running_handles == NULL) {
        running_handles = &_running_handles;
    }

    CAT_LOG_DEBUG(CURL, "curl_multi_perform(multi: %p, running_handles: " CAT_LOG_UNFILLED_STR ") = " CAT_LOG_UNFINISHED_STR, multi);

    /* this way even can solve the problem of CPU 100% if we perform in while loop */
    CURLMcode code = cat_curl_multi_perform_impl(multi, running_handles);

    CAT_LOG_DEBUG(CURL, "curl_multi_perform(multi: %p, running_handles: %d) = %d (%s)", multi, *running_handles, code, curl_multi_strerror(code));

    return code;
}

CAT_API CURLMcode cat_curl_multi_wait(
    CURLM *multi,
    struct curl_waitfd *extra_fds, unsigned int extra_nfds,
    int timeout_ms, int *numfds
)
{
    int _numfds = -1;
    int _running_handles = -1;
    int *running_handles = &_running_handles;
    if (numfds == NULL) {
        numfds = &_numfds;
    }

    /* TODO: Support it? */
    if (unlikely(extra_fds != NULL || extra_nfds != 0)) {
        return CURLM_INTERNAL_ERROR;
    }

    CAT_LOG_DEBUG(CURL, "curl_multi_wait(multi: %p, timeout_ms: %d, numfds: " CAT_LOG_UNFILLED_STR ") = " CAT_LOG_UNFINISHED_STR, multi, timeout_ms);

    CURLMcode mcode = cat_curl_multi_wait_impl(multi, timeout_ms, numfds, running_handles);
    if (unlikely(mcode == CAT_CURL_MULTI_CANCELLED)) {
        // do not return CAT_CURL_MULTI_CANCELLED to user, use it as an internal error
        mcode = CURLM_INTERNAL_ERROR;
    }

    CAT_LOG_DEBUG(CURL, "curl_multi_wait(multi: %p, timeout_ms: %d, numfds: %d, running_handles: %d) = %d (%s)", multi, timeout_ms, *numfds, *running_handles, mcode, curl_multi_strerror(mcode));

    return mcode;
}

/* module/runtime */

CAT_API cat_bool_t cat_curl_module_init(void)
{
    CAT_GLOBALS_REGISTER(cat_curl);

#ifdef CAT_DEBUG
    curl_version_info_data *curl_vid = curl_version_info(CURLVERSION_NOW);
    int curl_major_version = curl_vid->version_num >> 16;
    int curl_minor_version = (curl_vid->version_num >> 8) & 0xFF;
    /* cURL ABI is stable, we just pay attention to new features,
     * so it should running with same or higher major and minor version */
    if (curl_major_version < LIBCURL_VERSION_MAJOR ||
        (curl_major_version == LIBCURL_VERSION_MAJOR && curl_minor_version < LIBCURL_VERSION_MINOR)) {
        CAT_MODULE_ERROR(CURL, "cURL version mismatch, built with \"%s\", but running with \"%s\"",
            LIBCURL_VERSION, curl_vid->version);
    }
#endif

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        CAT_MODULE_ERROR(CURL, "curl_global_init() init failed");
    }

    return cat_true;
}

CAT_API cat_bool_t cat_curl_module_shutdown(void)
{
    curl_global_cleanup();

    CAT_GLOBALS_UNREGISTER(cat_curl);

    return cat_true;
}

CAT_API cat_bool_t cat_curl_runtime_init(void)
{
    RB_INIT(&CAT_CURL_G(multi_tree));

    return cat_true;
}

CAT_API cat_bool_t cat_curl_runtime_shutdown(void)
{

    return cat_true;
}

CAT_API cat_bool_t cat_curl_runtime_close(void)
{
    CAT_ASSERT(RB_MIN(cat_curl_multi_context_tree_s, &CAT_CURL_G(multi_tree)) == NULL);

    return cat_true;
}

#endif /* CAT_CURL */
