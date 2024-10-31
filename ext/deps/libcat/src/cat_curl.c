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

typedef struct cat_curl_multi_event_s {
    cat_queue_node_t node;
    curl_socket_t sockfd;
    int action;
} cat_curl_multi_event_t;

typedef struct cat_curl_multi_context_s {
    RB_ENTRY(cat_curl_multi_context_s) tree_entry;
    CURLM *multi;
    uv_timer_t timer;
    cat_coroutine_t *waiter;
    cat_curl_multi_event_t event_storage;
    cat_queue_t events;
} cat_curl_multi_context_t;

typedef struct cat_curl_multi_socket_context_s {
    cat_curl_multi_context_t *context;
    curl_socket_t sockfd;
    uv_poll_t poll;
} cat_curl_multi_socket_context_t;

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

/* globals */

CAT_GLOBALS_STRUCT_BEGIN(cat_curl) {
    struct cat_curl_multi_context_tree_s multi_tree;
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

static cat_always_inline void cat_curl_multi_socket_schedule(cat_curl_multi_context_t *context, curl_socket_t sockfd, int action)
{
    cat_curl_multi_event_t *event;
    if (cat_queue_empty(&context->events)) {
        event = &context->event_storage;
    } else {
        event = (cat_curl_multi_event_t *) cat_malloc_unrecoverable(sizeof(*event));
    }
    event->sockfd = sockfd;
    event->action = action;
    cat_queue_push_back(&context->events, &event->node);
    if (context->waiter != NULL) {
        cat_coroutine_schedule(context->waiter, CURL, "Poll event");
    }
}

static void cat_curl_multi_socket_poll_callback(uv_poll_t *poll, int status, int events)
{
    cat_curl_multi_socket_context_t *socket_context = (cat_curl_multi_socket_context_t *) poll->data;
    cat_curl_multi_context_t *context = socket_context->context;
    curl_socket_t sockfd = socket_context->sockfd;
    int action = 0;

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
        case CURL_POLL_OUT:
        case CURL_POLL_INOUT: {
            int uv_events = 0;
            if (socket_context == NULL) {
                socket_context = (cat_curl_multi_socket_context_t *) cat_malloc(sizeof(*socket_context));
#if CAT_ALLOC_HANDLE_ERRORS
                if (unlikely(fd == NULL)) {
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
                socket_context->context = context;
                socket_context->sockfd = sockfd;
                (void) uv_poll_init_socket(&CAT_EVENT_G(loop), &socket_context->poll, sockfd);
                socket_context->poll.data = socket_context;
            }
            if(action != CURL_POLL_OUT) {
                uv_events |= UV_READABLE;
            }
            if(action != CURL_POLL_IN) {
                uv_events |= UV_WRITABLE;
            }
            (void) uv_poll_start(&socket_context->poll, uv_events, cat_curl_multi_socket_poll_callback);
            break;
        }
        case CURL_POLL_REMOVE:
            if (socket_context != NULL) {
                uv_poll_stop(&socket_context->poll);
                uv_close((uv_handle_t*) &socket_context->poll, cat_curl_multi_socket_context_close_callback);
                curl_multi_assign(multi, sockfd, NULL);
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
    cat_curl_multi_socket_schedule(context, CURL_SOCKET_TIMEOUT, 0);
}

static int cat_curl_multi_timeout_function(CURLM *multi, long timeout_ms, cat_curl_multi_context_t *context)
{
    (void) multi;
    CAT_LOG_DEBUG_V2(CURL, "libcurl::curl_multi_timeout_function(multi: %p, timeout_ms=%ld)", multi, timeout_ms);

    if (timeout_ms < 0) {
        (void) uv_timer_stop(&context->timer);
    } else {
        if (timeout_ms <= 0) {
            /* 0 means directly call socket_action, but we'll do it in a bit */
            timeout_ms = 1;
        }
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
    CAT_ASSERT(cat_queue_empty(&context->events));
    RB_REMOVE(cat_curl_multi_context_tree_s, &CAT_CURL_G(multi_tree), context);
    uv_close((uv_handle_t *) &context->timer, cat_curl_multi_context_close_callback);
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
    uv_timer_init(&CAT_EVENT_G(loop), &context->timer);
    context->timer.data = context;
    context->waiter = NULL;
    cat_queue_init(&context->events);
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
    // :) we just use at least 1ms to avoid CPU 100%
    cat_timeout_t timeout = timeout_ms >= 0 ? CAT_MAX(1, timeout_ms) : timeout_ms;
    int socket_poll_event_count = 0;

    mcode = cat_curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0, running_handles);
    if (unlikely(mcode != CURLM_OK)) {
        return mcode;
    }
    if (*running_handles == 0) {
        return CURLM_OK;
    }
    if (context->waiter != NULL) {
        // since 7.59.0
#ifdef CURLM_RECURSIVE_API_CALL
        return CURLM_RECURSIVE_API_CALL;
#else
        return CURLM_INTERNAL_ERROR;
#endif
    }
    while (1) {
        cat_ret_t ret;
        context->waiter = CAT_COROUTINE_G(current);
        ret = cat_time_delay(timeout);
        context->waiter = NULL;
        if (unlikely(ret != CAT_RET_NONE)) {
            // timeout or error
            break;
        }
        cat_curl_multi_event_t *event;
        int socket_event_count = 0;
        while ((event = cat_queue_front_data(&context->events, cat_curl_multi_event_t, node))) {
            cat_queue_remove(&event->node);
            socket_event_count++;
            if (event->sockfd != CURL_SOCKET_TIMEOUT) {
                socket_poll_event_count++;
            }
            CURLMcode action_mcode;
            action_mcode = cat_curl_multi_socket_action(multi, event->sockfd, event->action, running_handles);
            if (event != &context->event_storage) {
                cat_free(event);
            }
            if (unlikely(action_mcode != CURLM_OK)) {
                mcode = action_mcode;
            }
        }
        if (socket_event_count == 0)  {
            // cancelled
            break;
        }
        if (socket_poll_event_count > 0) {
            // has events
            break;
        }
        if (*running_handles == 0) {
            // done
            break;
        }
    }
    if (numfds != NULL) {
        /** FIXME: socket_poll_event_count maybe bigger than numfds (when repeated),
         * but it should not matter for our usage scenarios. */
        *numfds = socket_poll_event_count;
    }
    return mcode;
}

static cat_always_inline CURLMcode cat_curl_multi_perform_impl(CURLM *multi, int *running_handles)
{
    return cat_curl_multi_wait_impl(multi, 0, NULL, running_handles);
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
        int numfds = 0;
        mcode = cat_curl_multi_wait_impl(multi, -1, &numfds, &running_handles);
        if (unlikely(mcode != CURLM_OK)) {
            goto _error;
        }
        if (running_handles == 0) {
            break;
        }
        if (numfds == 0) {
            // timedout or cancelled
            goto _error;
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

    return cat_true;
}

CAT_API cat_bool_t cat_curl_runtime_close(void)
{
    CAT_ASSERT(RB_MIN(cat_curl_multi_context_tree_s, &CAT_CURL_G(multi_tree)) == NULL);

    return cat_true;
}

#endif /* CAT_CURL */
