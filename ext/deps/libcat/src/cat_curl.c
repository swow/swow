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
#include "cat_event.h"
#include "cat_poll.h"
#include "cat_queue.h"
#include "cat_time.h"

#include "uv/tree.h"

/* declarations */

typedef struct cat_curl_pollfd_s {
    cat_queue_node_t node;
    curl_socket_t sockfd;
    int action;
} cat_curl_pollfd_t;

typedef struct cat_curl_multi_context_s {
    RB_ENTRY(cat_curl_multi_context_s) tree_entry;
    CURLM *multi;
    cat_queue_t waiters;
    cat_queue_t fds;
    cat_nfds_t nfds;
    long timeout;
    cat_bool_t waiting;
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

static cat_always_inline int cat_curl_translate_sys_events_to_action(int events, int revents)
{
    int action = CURL_CSELECT_NONE;

    if (revents & POLLIN) {
        action |= CURL_CSELECT_IN;
    }
    if (revents & POLLOUT) {
        action |= CURL_CSELECT_OUT;
    }
    if (revents & POLLERR) {
        action |= CURL_CSELECT_ERR;
    }
    if ((revents &~ (POLLIN | POLLOUT | POLLERR)) != POLLNONE) {
        if (events & POLLIN) {
            action |= CURL_CSELECT_IN;
        } else if (events & POLLOUT) {
            action |= CURL_CSELECT_OUT;
        } else if (events & POLLERR) {
            action |= CURL_CSELECT_ERR;
        }
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

/* easy */

static CURLMcode cat_curl_multi_wait_impl(CURLM *multi, int timeout_ms, int *numfds, int *running_handles);

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

/* multi */

static int cat_curl_multi_socket_function(
    CURL *ch, curl_socket_t sockfd, int action,
    cat_curl_multi_context_t *context, cat_curl_pollfd_t *fd)
{
    (void) ch;
    CURLM *multi = context->multi;

    CAT_LOG_DEBUG_V2(CURL, "libcurl::curl_multi_socket_function(multi: %p, sockfd: %d, action=%s), nfds=%zu",
        multi, sockfd, cat_curl_action_name(action), (size_t) context->nfds);

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
        fd->action |= action;
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
    CAT_LOG_DEBUG_V2(CURL, "libcurl::curl_multi_timeout_function(multi: %p, timeout=%ld)", multi, timeout);

    context->timeout = timeout;

    return 0;
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
    cat_queue_init(&context->waiters);
    cat_queue_init(&context->fds);
    context->nfds = 0;
    context->timeout = -1;
    context->waiting = cat_false;
    /* following is outdated comment, but I didn't understand
     * the specific meaning, so I won't remove it yet:
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

static cat_always_inline cat_curl_multi_context_t *cat_curl_multi_get_context(CURLM *multi)
{
    cat_curl_multi_context_t lookup;
    lookup.multi = multi;
    return RB_FIND(cat_curl_multi_context_tree_s, &CAT_CURL_G(multi_tree), &lookup);
}

static void cat_curl_multi_context_close(cat_curl_multi_context_t *context)
{
    /* we assume that fds should have been free'd in curl_multi_socket_function() before,
     * but when fatal error occurred and we called curl_multi_cleanup() without calling
     * curl_multi_remove_handle(), fd storage would not be removed from fds.  */
    if (context->nfds > 0) {
        CAT_LOG_DEBUG_V2(CURL, "curl_multi_context_close(multi: %p) with %zu fds", context->multi, (size_t) context->nfds);
        cat_curl_pollfd_t *fd;
        while ((fd = cat_queue_front_data(&context->fds, cat_curl_pollfd_t, node))) {
            cat_queue_remove(&fd->node);
            cat_free(fd);
            context->nfds--;
        }
    }
    CAT_ASSERT(context->nfds == 0);
    RB_REMOVE(cat_curl_multi_context_tree_s, &CAT_CURL_G(multi_tree), context);
    cat_free(context);
}

static void cat_curl_multi_close_context(CURLM *multi)
{
    cat_curl_multi_context_t *context;

    context = cat_curl_multi_get_context(multi);
    CAT_ASSERT(context != NULL);
    cat_curl_multi_context_close(context);
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

/**
 * @param option_timeout timeout by timeout_function, usually be 0 when fd is in prepare,
 * it maybe timeout set by curl_multi_setopt().
 * @param operation_timeout timeout which is passed in by argument
 * @return minimal timeout
 */
static cat_always_inline cat_timeout_t cat_curl_multi_timeout_solve(cat_timeout_t option_timeout, cat_timeout_t operation_timeout)
{
    if (option_timeout < operation_timeout && option_timeout >= 0) {
        return option_timeout;
    }
    return operation_timeout;
}

static cat_always_inline CURLMcode cat_curl_multi_socket_action(CURLM *multi, curl_socket_t sockfd, int action, int *running_handles)
{
    CURLMcode mcode = curl_multi_socket_action(multi, sockfd, action, running_handles);
    CAT_LOG_DEBUG_V2(CURL, "libcurl::curl_multi_socket_action(multi: %p, fd: %d, %s) = %d (%s)",
        multi, sockfd, cat_curl_action_name(action), mcode, curl_multi_strerror(mcode));
    return mcode;
}

static cat_always_inline CURLMcode cat_curl_multi_socket_all(CURLM *multi, int *running_handles)
{
    CURLMcode mcode;
    do {
        /* workaround for cURL SSL connection bug... */
        mcode = curl_multi_perform(multi, running_handles);
        CAT_LOG_DEBUG_V2(CURL, "libcurl::curl_multi_perform(multi: %p, running_handles: %d) = %d (%s) workaround",
            multi, *running_handles, mcode, curl_multi_strerror(mcode));
        if (unlikely(mcode != CURLM_OK)) {
            break;
        }
        mcode = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0, running_handles);
        CAT_LOG_DEBUG_V2(CURL, "libcurl::curl_multi_socket_action(multi: %p, TIMEOUT, running_handles: %d) = %d (%s) in socket_all()",
            multi, *running_handles, mcode, curl_multi_strerror(mcode));
    } while (0);
    return mcode;
}

static CURLMcode cat_curl_multi_wait_impl(
    CURLM *multi, int timeout_ms, int *numfds, int *running_handles
)
{
    cat_curl_multi_context_t *context;
    cat_queue_t *waiters;
    cat_pollfd_t *fds = NULL;
    cat_bool_t use_stacked_fds = cat_false;
    CURLMcode mcode = CURLM_OK;
    cat_msec_t start_line = cat_time_msec_cached();
    cat_timeout_t timeout = timeout_ms; /* maybe reduced in the loop */
    int previous_running_handles;
    int poll_rc = 0;

    CAT_ASSERT(running_handles != NULL);

    context = cat_curl_multi_get_context(multi);
    waiters = &context->waiters;

    CAT_ASSERT(context != NULL);

    // first call to update timeout and get running_handles to prevent it has been 0 after the previous call
    mcode = cat_curl_multi_socket_all(multi, running_handles);
    if (unlikely(mcode != CURLM_OK) || *running_handles == 0) {
        goto _fast_out;
    }

    /** @note we should only push coroutine node to waiters when time_wait(),
     * even if poll APIs would not modify queue node, but cURL callback functions
     * may be called in cURL action calls, then user may call any coroutine related
     * APIs in the callback functions, which may cause the queue node to be modified. */
    if (context->waiting) {
        cat_bool_t wait_ret;
        cat_queue_push_back(waiters, &CAT_COROUTINE_G(current)->waiter.node);
        CAT_TIME_WAIT_START() {
            wait_ret = cat_time_wait(timeout);
        } CAT_TIME_WAIT_END(timeout);
        cat_queue_remove(&CAT_COROUTINE_G(current)->waiter.node);
        if (unlikely(!wait_ret)) {
            // timedout
            goto _fast_out;
        }
        if (cat_queue_front_data(waiters, cat_coroutine_t, waiter.node) != CAT_COROUTINE_G(current)) {
            // canceled
            goto _fast_out;
        }
    }
    context->waiting = cat_true;

    while (1) {
        cat_timeout_t op_timeout = cat_curl_multi_timeout_solve(context->timeout, timeout);
        if (context->nfds == 0 || (op_timeout >= 0 && op_timeout <= 1)) {
            // use 1 instead of 0 to reduce high cost sys overhead
            op_timeout = 1;
            while (1) {
                cat_ret_t ret;
                CAT_LOG_DEBUG_V2(CURL, "libcurl::curl_time_delay(multi: %p, timeout: " CAT_TIMEOUT_FMT ") when %s",
                    multi, op_timeout, context->nfds == 0 ? "nfds is 0" : "timeout is tiny");
                ret = cat_time_delay(op_timeout);
                if (unlikely(ret != CAT_RET_OK)) {
                    goto _out;
                }
                previous_running_handles = *running_handles;
                mcode = cat_curl_multi_socket_all(multi, running_handles);
                if (unlikely(mcode != CURLM_OK) || *running_handles < previous_running_handles) {
                    goto _out;
                }
                if (unlikely(context->nfds == 0)) {
                    /* as https://curl.se/libcurl/c/curl_multi_wait.html shows:
                     * 'numfds' being zero means either a timeout or no file descriptors to
                     * wait for. Try timeout on first occurrence, then assume no file descriptors
                     * and no file descriptors to wait for means wait for 100 ms. */
                    if (op_timeout == 1) {
                        op_timeout = 10;
                    } else if (op_timeout < 100) {
                        op_timeout += 10;
                    }
                    continue;
                }
                break;
            }
        } else {
            if (context->nfds == 1) {
                cat_curl_pollfd_t *curl_fd = cat_queue_front_data(&context->fds, cat_curl_pollfd_t, node);
                CAT_LOG_DEBUG_V2(CURL, "poll_one() for multi<%p>", multi);
                cat_pollfd_events_t events = cat_curl_translate_poll_flags_to_sys(curl_fd->action);
                cat_pollfd_events_t revents;
                cat_ret_t ret = cat_poll_one(
                    curl_fd->sockfd,
                    events,
                    &revents,
                    op_timeout
                );
                if (unlikely(ret == CAT_RET_ERROR)) {
                    mcode = CURLM_OUT_OF_MEMORY; // or internal error?
                    goto _out;
                }
                if (ret == CAT_RET_OK) {
                    int action = cat_curl_translate_sys_events_to_action(events, revents);
                    (void) cat_curl_multi_socket_action(multi, curl_fd->sockfd, action, running_handles);
                    poll_rc = 1;
                    goto _out;
                }
            } else {
                cat_pollfd_t stacked_fds[8];
                cat_nfds_t i;
                use_stacked_fds = context->nfds <= 8;
                if (use_stacked_fds) {
                    fds = stacked_fds;
                } else {
                    fds = (cat_pollfd_t *) cat_malloc(sizeof(*fds) * context->nfds);
    #if CAT_ALLOC_HANDLE_ERRORS
                    if (unlikely(fds == NULL)) {
                        mcode = CURLM_OUT_OF_MEMORY;
                        goto _out;
                    }
    #endif
                }
                i = 0;
                CAT_QUEUE_FOREACH_DATA_START(&context->fds, cat_curl_pollfd_t, node, curl_fd) {
                    cat_pollfd_t *fd = &fds[i];
                    fd->fd = curl_fd->sockfd;
                    fd->events = cat_curl_translate_poll_flags_to_sys(curl_fd->action);
                    i++;
                } CAT_QUEUE_FOREACH_DATA_END();
                CAT_LOG_DEBUG_V2(CURL, "poll() for multi<%p>", multi);
                poll_rc = cat_poll(fds, context->nfds, op_timeout);
                if (unlikely(poll_rc == CAT_RET_ERROR)) {
                    mcode = CURLM_OUT_OF_MEMORY; // or internal error?
                    goto _out;
                }
                if (poll_rc != 0) {
                    for (i = 0; i < context->nfds; i++) {
                        cat_pollfd_t *fd = &fds[i];
                        int action = cat_curl_translate_sys_events_to_action(fd->events, fd->revents);
                        (void) cat_curl_multi_socket_action(multi, fd->fd, action, running_handles);
                    }
                    goto _out;
                }
            }
            // timeout case
            previous_running_handles = *running_handles;
            mcode = cat_curl_multi_socket_all(multi, running_handles);
            if (unlikely(mcode != CURLM_OK) || *running_handles < previous_running_handles) {
                goto _out;
            }
            if (fds != NULL && !use_stacked_fds) {
                cat_free(fds);
            }
            fds = NULL;
        }
        if (timeout_ms >= 0) {
            cat_msec_t new_start_line = cat_time_msec_cached();
            cat_msec_t time_passed = new_start_line - start_line;
            if (((cat_msec_t) timeout) < time_passed) {
                /* timeout */
                goto _out;
            }
            timeout -= ((cat_timeout_t) time_passed);
            CAT_LOG_DEBUG_V2(CURL, "multi_wait() inner loop continue, remaining timeout is " CAT_TIMEOUT_FMT, timeout);
            start_line = new_start_line;
        }
    }
    _out:
    if (fds != NULL && !use_stacked_fds) {
        cat_free(fds);
    }

    context->waiting = cat_false;
    if (!cat_queue_empty(waiters)) {
        /* resume the next queued coroutine */
        cat_coroutine_t *waiter = cat_queue_front_data(waiters, cat_coroutine_t, waiter.node);
        cat_coroutine_schedule(waiter, CURL, "Multi wait");
    }
    _fast_out:
    if (numfds != NULL) {
        *numfds = poll_rc;
    }
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
    CURLMcode code = cat_curl_multi_wait_impl(multi, 0, NULL, running_handles);

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
