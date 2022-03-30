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
#include "cat_coroutine.h"
#include "cat_event.h"
#include "cat_poll.h"
#include "cat_queue.h"
#include "cat_time.h"

#ifdef CAT_CURL

typedef struct cat_curl_easy_context_s {
    CURLM *multi;
    cat_coroutine_t *coroutine;
    curl_socket_t sockfd;
    cat_pollfd_events_t events;
    long timeout;
} cat_curl_easy_context_t;

typedef struct cat_curl_multi_context_s {
    cat_queue_node_t node;
    CURLM *multi;
    cat_coroutine_t *coroutine;
    cat_queue_t fds;
    cat_nfds_t nfds;
    long timeout;
} cat_curl_multi_context_t;

typedef struct cat_curl_pollfd_s {
    cat_queue_node_t node;
    curl_socket_t sockfd;
    int action;
} cat_curl_pollfd_t;

CAT_GLOBALS_STRUCT_BEGIN(cat_curl)
    cat_queue_t multi_map;
CAT_GLOBALS_STRUCT_END(cat_curl)

CAT_GLOBALS_DECLARE(cat_curl)

#define CAT_CURL_G(x) CAT_GLOBALS_GET(cat_curl, x)

CAT_GLOBALS_CTOR_DECLARE_SZ(cat_curl)

static cat_always_inline void cat_curl_multi_configure(CURLM *multi, void *socket_function, void *timer_function, void *context)
{
    curl_multi_setopt(multi, CURLMOPT_SOCKETFUNCTION, socket_function);
    curl_multi_setopt(multi, CURLMOPT_SOCKETDATA, context);
    curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, timer_function);
    curl_multi_setopt(multi, CURLMOPT_TIMERDATA, context);
}

static cat_always_inline int cat_curl_translate_poll_flags_from_sys(int revents)
{
    int action = CURL_POLL_NONE;

    if (revents & POLLIN) {
        action |= CURL_CSELECT_IN;
    }
    if (revents & POLLOUT) {
        action |= CURL_CSELECT_OUT;
    }

    return action;
}

static cat_always_inline cat_pollfd_events_t cat_curl_translate_poll_flags_to_sys(int action)
{
    cat_pollfd_events_t events = POLLNONE;

    if (action != CURL_POLL_REMOVE) {
        if (action != CURL_POLL_IN) {
            events |= POLLOUT;
        }
        if (action != CURL_POLL_OUT) {
            events |= POLLIN;
        }
    }

    return events;
}

#ifdef CAT_DEBUG
static const char *cat_curl_translate_action_name(int action)
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

static cat_always_inline cat_timeout_t cat_curl_timeout_min(cat_timeout_t timeout1, cat_timeout_t timeout2)
{
    if (timeout1 < timeout2 && timeout1 >= 0) {
        return timeout1;
    }
    return timeout2;
}

static int cat_curl_easy_socket_function(CURL *ch, curl_socket_t sockfd, int action, cat_curl_easy_context_t *context, void *unused)
{
    (void) ch;
    (void) unused;
    CAT_LOG_DEBUG(EXT, "curl_easy_socket_function(multi=%p, sockfd=%d, action=%s), timeout=%ld",
        context->multi, sockfd, cat_curl_translate_action_name(action), context->timeout);

    context->sockfd = sockfd;
    context->events = cat_curl_translate_poll_flags_to_sys(action);

    return 0;
}

static int cat_curl_easy_timeout_function(CURLM *multi, long timeout, cat_curl_easy_context_t *context)
{
    (void) multi;
    CAT_LOG_DEBUG(EXT, "curl_easy_timeout_function(multi=%p, timeout=%ld)", multi, timeout);

    context->timeout = timeout;

    return 0;
}

static int cat_curl_multi_socket_function(
    CURL *ch, curl_socket_t sockfd, int action,
    cat_curl_multi_context_t *context, cat_curl_pollfd_t *fd)
{
    (void) ch;
    CURLM *multi = context->multi;

    CAT_LOG_DEBUG(EXT, "curl_multi_socket_function(multi=%p, sockfd=%d, action=%s), nfds=%zu, timeout=%ld",
        multi, sockfd, cat_curl_translate_action_name(action), (size_t) context->nfds, context->timeout);

    if (action != CURL_POLL_REMOVE) {
        if (fd == NULL) {
            fd = (cat_curl_pollfd_t *) cat_malloc(sizeof(*fd));
#if CAT_ALLOC_HANDLE_ERRORS
            if (unlikely(fd == NULL)) {
                return CURLM_OUT_OF_MEMORY;
            }
#endif
            cat_queue_push_back(&context->fds, &fd->node);
            context->nfds++;
            fd->sockfd = sockfd;
            curl_multi_assign(multi, sockfd, fd);
        }
        fd->action = action;
    } else {
        cat_queue_remove(&fd->node);
        cat_free(fd);
        context->nfds--;
        curl_multi_assign(multi, sockfd, NULL);
    }

    return CURLM_OK;
}

static int cat_curl_multi_timeout_function(CURLM *multi, long timeout, cat_curl_multi_context_t *context)
{
    (void) multi;
    CAT_LOG_DEBUG(EXT, "curl_multi_timeout_function(multi=%p, timeout=%ld)", multi, timeout);

    context->timeout = timeout;

    return 0;
}

static cat_curl_multi_context_t *cat_curl_multi_create_context(CURLM *multi)
{
    cat_curl_multi_context_t *context;

    CAT_LOG_DEBUG(EXT, "curl_multi_context_create(multi=%p)", multi);

    context = (cat_curl_multi_context_t *) cat_malloc(sizeof(*context));
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(context == NULL)) {
        return NULL;
    }
#endif

    context->multi = multi;
    context->coroutine = NULL;
    cat_queue_init(&context->fds);
    context->nfds = 0;
    context->timeout = -1;
    /* latest multi has higher priority
     * (previous may leak and they would be free'd in shutdown) */
    cat_queue_push_front(&CAT_CURL_G(multi_map), &context->node);

    cat_curl_multi_configure(
        multi,
        (void *) cat_curl_multi_socket_function,
        (void *) cat_curl_multi_timeout_function,
        context
    );

    return context;
}

static cat_curl_multi_context_t *cat_curl_multi_get_context(CURLM *multi)
{
    size_t i = 0;

    CAT_QUEUE_FOREACH_DATA_START(&CAT_CURL_G(multi_map), cat_curl_multi_context_t, node, context) {
        if (context->multi == NULL) {
            return NULL; // eof
        }
        if (context->multi == multi) {
            return context; // hit
        }
        i++;
    } CAT_QUEUE_FOREACH_DATA_END();

    return NULL;
}

static void cat_curl_multi_context_close(cat_curl_multi_context_t *context)
{
#ifdef CAT_DONT_OPTIMIZE /* fds should have been free'd in curl_multi_socket_function() */
    cat_curl_pollfd_t *fd;
    while ((fd = cat_queue_front_data(&context->fds, cat_curl_pollfd_t, node))) {
        cat_queue_remove(&fd->node);
        cat_free(fd);
        context->nfds--;
    }
#endif
    CAT_ASSERT(context->nfds == 0);
    cat_queue_remove(&context->node);
    cat_free(context);
}

static void cat_curl_multi_close_context(CURLM *multi)
{
    cat_curl_multi_context_t *context;

    context = cat_curl_multi_get_context(multi);
    CAT_ASSERT(context != NULL);
    cat_curl_multi_context_close(context);
}

CAT_API cat_bool_t cat_curl_module_init(void)
{
    CAT_GLOBALS_REGISTER(cat_curl, CAT_GLOBALS_CTOR(cat_curl), NULL);

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        CAT_WARN_WITH_REASON(EXT, CAT_UNKNOWN, "Curl init failed");
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
    cat_queue_init(&CAT_CURL_G(multi_map));

    return cat_true;
}

CAT_API cat_bool_t cat_curl_runtime_shutdown(void)
{
    CAT_ASSERT(cat_queue_empty(&CAT_CURL_G(multi_map)));

    return cat_true;
}

CAT_API CURLcode cat_curl_easy_perform(CURL *ch)
{
    cat_curl_easy_context_t context;
    CURLMsg *message = NULL;
    CURLMcode mcode = CURLM_INTERNAL_ERROR;
    CURLcode code = CURLE_RECV_ERROR;
    int running_handles;

    context.multi = curl_multi_init();
    if (unlikely(context.multi == NULL)) {
        return CURLE_OUT_OF_MEMORY;
    }
    context.coroutine = CAT_COROUTINE_G(current);
    context.sockfd = -1;
    context.timeout = -1;
    context.events = POLLNONE;
    cat_curl_multi_configure(
        context.multi,
        (void *) cat_curl_easy_socket_function,
        (void *) cat_curl_easy_timeout_function,
        &context
    );
    mcode = curl_multi_add_handle(context.multi, ch);
    if (unlikely(mcode != CURLM_OK)) {
#if (LIBCURL_VERSION_NUM >= ((7) << 16 | (32) << 8 | (1)))
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
        if (context.events == POLLNONE) {
            cat_ret_t ret = cat_time_delay(context.timeout);
            if (unlikely(ret != CAT_RET_OK)) {
                code = CURLE_RECV_ERROR;
                break;
            }
            mcode = curl_multi_socket_action(context.multi, CURL_SOCKET_TIMEOUT, 0, &running_handles);
        } else {
            cat_pollfd_events_t revents;
            int action;
            cat_ret_t ret;
            ret = cat_poll_one(context.sockfd, context.events, &revents, context.timeout);
            if (unlikely(ret == CAT_RET_ERROR)) {
                code = CURLE_RECV_ERROR;
                break;
            }
            action = cat_curl_translate_poll_flags_from_sys(revents);
            mcode = curl_multi_socket_action(
                context.multi,
                action != 0 ? context.sockfd : CURL_SOCKET_TIMEOUT,
                action,
                &running_handles
            );
        }
        if (unlikely(mcode != CURLM_OK)) {
            break;
        }
        message = curl_multi_info_read(context.multi, &running_handles);
        if (message != NULL) {
            CAT_ASSERT(message->msg == CURLMSG_DONE);
            CAT_ASSERT(running_handles == 0);
            CAT_LOG_DEBUG_SCOPE_START_EX(EXT, char *done_url;
                curl_easy_getinfo(message->easy_handle, CURLINFO_EFFECTIVE_URL, &done_url)) {
                CAT_LOG_DEBUG(EXT, "curl_easy_perform(multi=%p, url='%s') DONE", context.multi, done_url);
            } CAT_LOG_DEBUG_SCOPE_END();
            code = message->data.result;
            break;
        }
        CAT_ASSERT(running_handles == 0 || running_handles == 1);
    }

    curl_multi_remove_handle(context.multi, ch);
    _add_failed:
    curl_multi_cleanup(context.multi);

    return code;
}

CAT_API CURLM *cat_curl_multi_init(void)
{
    CURLM *multi = curl_multi_init();
    cat_curl_multi_context_t *context;

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
    /* we do not know whether libcurl would do somthing during cleanup,
     * so we close the context later */
    cat_curl_multi_close_context(multi);

    return mcode;
}

static CURLMcode cat_curl_multi_exec(
    CURLM *multi,
    struct curl_waitfd *extra_fds, unsigned int extra_nfds,
    int timeout_ms, int *numfds, int *running_handles
)
{
    cat_curl_multi_context_t *context;
    cat_pollfd_t *fds = NULL;
    CURLMcode mcode = CURLM_OK;
    cat_msec_t start_line = cat_time_msec_cached();
    cat_timeout_t timeout = timeout_ms; /* maybe reduced in the loop */
    int ret = 0, _running_handles;

    CAT_LOG_DEBUG(EXT, "curl_multi_wait(multi=%p, timeout_ms=%d)", multi, timeout_ms);

    /* TODO: Support it? */
    CAT_ASSERT(extra_fds == NULL && "Not support yet");
    CAT_ASSERT(extra_nfds == 0 && "Not support yet");

    if (running_handles == NULL) {
        running_handles = &_running_handles;
    }

    context = cat_curl_multi_get_context(multi);

    CAT_ASSERT(context != NULL);

    while (1) {
        if (context->nfds == 0) {
            cat_timeout_t op_timeout = cat_curl_timeout_min(context->timeout, timeout);
            cat_ret_t ret;
            CAT_LOG_DEBUG(EXT, "curl_time_delay(multi=%p, timeout=" CAT_TIMEOUT_FMT ")", multi, op_timeout);
            ret = cat_time_delay(op_timeout);
            if (unlikely(ret != CAT_RET_OK)) {
                goto _out;
            }
            CAT_LOG_DEBUG(EXT, "curl_multi_socket_action(multi=%p, CURL_SOCKET_TIMEOUT) after delay", multi);
            mcode = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0, running_handles);
            if (unlikely(mcode != CURLM_OK) || *running_handles == 0) {
                goto _out;
            }
        } else {
            cat_nfds_t i;
            fds = (cat_pollfd_t *) cat_malloc(sizeof(*fds) * context->nfds);
#if CAT_ALLOC_HANDLE_ERRORS
            if (unlikely(fds == NULL)) {
                mcode = CURLM_OUT_OF_MEMORY;
                goto _out;
            }
#endif
            i = 0;
            CAT_QUEUE_FOREACH_DATA_START(&context->fds, cat_curl_pollfd_t, node, curl_fd) {
                cat_pollfd_t *fd = &fds[i];
                fd->fd = curl_fd->sockfd;
                fd->events = cat_curl_translate_poll_flags_to_sys(curl_fd->action);
                i++;
            } CAT_QUEUE_FOREACH_DATA_END();
            ret = cat_poll(fds, context->nfds, cat_curl_timeout_min(context->timeout, timeout));
            if (unlikely(ret == CAT_RET_ERROR)) {
                mcode = CURLM_OUT_OF_MEMORY; // or internal error?
                goto _out;
            }
            if (ret != 0) {
                for (i = 0; i < context->nfds; i++) {
                    cat_pollfd_t *fd = &fds[i];
                    int action;
                    if (fd->revents == POLLNONE) {
                        continue;
                    }
                    action = cat_curl_translate_poll_flags_from_sys(fd->revents);
                    CAT_LOG_DEBUG(EXT, "curl_multi_socket_action(multi=%p, fd=%d, %s) after poll", multi, fd->fd, cat_curl_translate_action_name(action));
                    mcode = curl_multi_socket_action(multi, fd->fd, action, running_handles);
                    if (unlikely(mcode != CURLM_OK)) {
                        continue; // shall we handle it?
                    }
                    if (*running_handles == 0) {
                        goto _out;
                    }
                }
            } else {
                CAT_LOG_DEBUG(EXT, "curl_multi_socket_action(multi=%p, CURL_SOCKET_TIMEOUT) after poll return 0", multi);
                mcode = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0, running_handles);
                if (unlikely(mcode != CURLM_OK) || *running_handles == 0) {
                    goto _out;
                }
            }
            goto _out;
        }
        do {
            cat_msec_t new_start_line = cat_time_msec_cached();
            timeout -= (new_start_line - start_line);
            start_line = new_start_line;
            if (timeout <= 0) {
                /* timeout */
                goto _out;
            }
        } while (0);
    }

    _out:
    CAT_LOG_DEBUG(EXT, "curl_multi_wait(multi=%p, timeout_ms=%d) = %d, numfds=%d", multi, timeout_ms, mcode, ret);
    if (fds != NULL) {
        cat_free(fds);
    }
    if (numfds != NULL) {
        *numfds = ret;
    }
    return mcode;
}

CAT_API CURLMcode cat_curl_multi_perform(CURLM *multi, int *running_handles)
{
    CAT_LOG_DEBUG(EXT, "curl_multi_perform(multi=%p)", multi);

    /* this way even can solve the problem of CPU 100% if we perform in while loop */
    return cat_curl_multi_exec(multi, NULL, 0, 0, NULL, running_handles);
}

CAT_API CURLMcode cat_curl_multi_wait(
    CURLM *multi,
    struct curl_waitfd *extra_fds, unsigned int extra_nfds,
    int timeout_ms, int *numfds
)
{
    return cat_curl_multi_exec(multi, extra_fds, extra_nfds, timeout_ms, numfds, NULL);
}

#endif /* CAT_CURL */
