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

#include "cat_poll.h"

#include "cat_coroutine.h"
#include "cat_event.h"
#include "cat_time.h"

#ifdef CAT_ENABLE_DEBUG_LOG
#include "cat_buffer.h"
#endif

#ifdef CAT_OS_UNIX_LIKE
/* for uv__fd_exists */
#  ifdef CAT_IDE_HELPER
#    include "unix/internal.h"
#  else
#    include "../deps/libuv/src/unix/internal.h"
#  endif
#endif

#define UV_EVENT_MAP(XX) \
    XX(READABLE) \
    XX(WRITABLE) \
    XX(DISCONNECT) \
    XX(PRIORITIZED)

typedef enum uv_event_e {
    UV_EVENT_NONE = 0,
#define UV_EVENT_GEN(name) UV_EVENT_##name = UV_##name,
    UV_EVENT_MAP(UV_EVENT_GEN)
#undef UV_EVENT_GEN
} uv_event_t;

typedef int uv_events_t;

/* poll event will never be triggered if timeout is 0 on Windows,
 * because uv__poll() treats 0 timeout specially.
 * TODO: could we fix it in libuv?  */
#ifndef CAT_OS_WIN
#define CAT_POLL_CHECK_TIMEOUT(timeout)
#else
#define CAT_POLL_CHECK_TIMEOUT(timeout) do { \
    if (timeout == 0) { \
        timeout = 1; \
    } \
} while (0)
#endif

static cat_always_inline cat_pollfd_events_t cat_poll_translate_uv_events_to_sys_events(uv_events_t uv_events)
{
    cat_pollfd_events_t revents = 0;

    if (uv_events & UV_EVENT_READABLE) {
        revents |= POLLIN;
    }
    if (uv_events & UV_EVENT_WRITABLE) {
        revents |= POLLOUT;
    }
    if (uv_events & UV_EVENT_DISCONNECT) {
        revents |= POLLHUP;
    }
    if (uv_events & UV_EVENT_PRIORITIZED) {
        revents |= POLLPRI;
    }

    return revents;
}

static cat_always_inline int cat_poll_translate_sys_events_to_uv_events(cat_pollfd_events_t sys_events)
{
    uv_events_t uv_events = 0;

    if (sys_events & POLLIN) {
        uv_events |= (UV_EVENT_READABLE | UV_EVENT_DISCONNECT);
    }
    if (sys_events & POLLOUT) {
        uv_events |= (UV_EVENT_WRITABLE | UV_EVENT_DISCONNECT);
    }
    if (sys_events & POLLPRI) {
        uv_events |= UV_EVENT_PRIORITIZED;
    }

    return uv_events;
}

static CAT_COLD cat_pollfd_events_t cat_poll_translate_error_to_sys_events(cat_pollfd_events_t input_events, int error)
{
    switch (error) {
        case CAT_EBADF:
            return POLLNVAL;
        case CAT_EEXIST:
            /* can not poll the same fd twice at the same time */
            return 0;
        case CAT_ENOTSOCK:
            /* File descriptors associated with regular files shall always select true
             * for ready to read, ready to write, and error conditions. */
            return (input_events & (POLLIN | POLLOUT)) | POLLERR;
        default:
            return POLLERR;
    }
}

static CAT_COLD int cat_poll_filter_init_error(int error)
{
    switch (error) {
        case CAT_EEXIST:
        case CAT_EBADF:
            return error;
        default:
            /* for select/poll regular file */
            return CAT_ENOTSOCK;
    }
}

#ifdef CAT_ENABLE_DEBUG_LOG
static CAT_BUFFER_STR_FREE char *cat_poll_uv_events_str(uv_events_t events)
{
    cat_buffer_t buffer;
    cat_buffer_create(&buffer, 64);
#define UV_EVENT_STR_GEN(name) \
    if (events & UV_EVENT_##name) { \
        cat_buffer_append_str(&buffer, #name "|"); \
    }
    UV_EVENT_MAP(UV_EVENT_STR_GEN)
#undef UV_EVENT_STR_GEN
    if (buffer.length == 0) {
        cat_buffer_append_str(&buffer, "NONE");
    } else {
        buffer.length--;
    }
    return cat_buffer_export_str(&buffer);
}

static CAT_BUFFER_STR_FREE char *cat_pollfd_events_str(cat_pollfd_events_t events)
{
    cat_buffer_t buffer;
    cat_buffer_create(&buffer, 64);
#define CAT_POLLFD_EVENT_NAME_GEN(name) \
    if (events & POLL##name) { \
        cat_buffer_append_str(&buffer, #name "|"); \
    }
    CAT_POLLFD_EVENT_MAP(CAT_POLLFD_EVENT_NAME_GEN)
#undef CAT_POLLFD_EVENT_NAME_GEN
    if (buffer.length == 0) {
        cat_buffer_append_str(&buffer, "NONE");
    } else {
        buffer.length--;
    }
    return cat_buffer_export_str(&buffer);
}
#endif

/* TODO: Optimize dup() in poll() (can not find a better way temporarily) */

typedef struct cat_poll_one_s {
    union {
        cat_coroutine_t *coroutine;
        uv_handle_t handle;
        uv_poll_t poll;
    } u;
#ifdef CAT_ENABLE_DEBUG_LOG
    cat_os_socket_t fd;
#endif
    struct {
        int status; // uv status
        uv_events_t events; // uv events, e.g UV_EVENT_READABLE, UV_EVENT_WRITABLE...
    } ret;
    cat_bool_t deferred_done_callback;
} cat_poll_one_t;

static void cat_poll_one_done_callback(cat_data_t *data)
{
    cat_poll_one_t *poll = (cat_poll_one_t *) data;

    cat_coroutine_schedule(poll->u.coroutine, EVENT, "Poll one");
}

static void cat_poll_one_callback(uv_poll_t* handle, int status, uv_events_t events)
{
    cat_poll_one_t *poll = (cat_poll_one_t *) handle;

    CAT_LOG_DEBUG_VA_WITH_LEVEL(POLL, 2, {
        char *events_str = cat_poll_uv_events_str(events);
        CAT_LOG_DEBUG_D(POLL, "poll_one_callback(fd: " CAT_OS_SOCKET_FMT ", status: %d" CAT_LOG_STRERRNO_FMT ", events: %s)",
            poll->fd, status, CAT_LOG_STRERRNO_C(status == 0, status), events_str);
        cat_buffer_str_free(events_str);
    });

    poll->ret.status = status;
    /* Note: uv may return multi events in multi callbacks */
    poll->ret.events |= events;

    /* Note: for get all revents at once,
     * we should schedule coroutine in io defer callback */
    if (!poll->deferred_done_callback) {
        (void) cat_event_io_defer(cat_poll_one_done_callback, poll);
        poll->deferred_done_callback = cat_true;
    }
}

static void cat_poll_one_close_callback(uv_handle_t *handle)
{
    cat_poll_one_t *poll = cat_container_of(handle, cat_poll_one_t, u.handle);
    cat_free(poll);
}

static cat_ret_t cat_poll_one_impl(cat_os_socket_t fd, cat_pollfd_events_t events, cat_pollfd_events_t *revents, cat_timeout_t timeout)
{
    CAT_POLL_CHECK_TIMEOUT(timeout);
    cat_poll_one_t *poll;
    cat_os_socket_t _fd = fd;
    cat_ret_t ret;
    int error;

    *revents = POLLNONE;

    poll = (cat_poll_one_t *) cat_malloc(sizeof(*poll));
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(poll == NULL)) {
        cat_update_last_error_of_syscall("Malloc for poll failed");
        return CAT_RET_ERROR;
    }
#endif
    _fd = fd;
#ifdef CAT_OS_UNIX_LIKE
    if (unlikely(uv__fd_exists(&CAT_EVENT_G(loop), fd))) {
        _fd = dup(fd);
        if (unlikely(_fd == CAT_OS_INVALID_SOCKET)) {
            cat_update_last_error_of_syscall("Dup for poll_one() failed");
            cat_free(poll);
            return CAT_RET_ERROR;
        }
        /* uv_poll_init_socket() and uv_poll_start() must return success if fd exists */
    }
#endif
    error = uv_poll_init_socket(&CAT_EVENT_G(loop), &poll->u.poll, _fd);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Poll init failed");
        cat_free(poll);
        *revents = cat_poll_translate_error_to_sys_events(events, cat_poll_filter_init_error(error));
        return CAT_RET_ERROR;
    }
    error = uv_poll_start(&poll->u.poll, cat_poll_translate_sys_events_to_uv_events(events), cat_poll_one_callback);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Poll start failed");
        uv_close(&poll->u.handle, cat_poll_one_close_callback);
        *revents = cat_poll_translate_error_to_sys_events(events, error);
        return CAT_RET_ERROR;
    }
#ifdef CAT_ENABLE_DEBUG_LOG
    poll->fd = fd;
#endif
    poll->ret.status = CAT_ECANCELED;
    poll->ret.events = UV_EVENT_NONE;
    poll->u.coroutine = CAT_COROUTINE_G(current);
    poll->deferred_done_callback = cat_false;

    ret = cat_time_delay(timeout);

#ifdef CAT_OS_UNIX_LIKE
    if (_fd != fd) {
        uv__close(_fd);
    }
#endif
    uv_close(&poll->u.handle, cat_poll_one_close_callback);

    switch (ret) {
        /* delay cancelled */
        case CAT_RET_NONE: {
            if (unlikely(poll->ret.status < 0)) {
                if (poll->ret.status == CAT_ECANCELED) {
                    cat_update_last_error(CAT_ECANCELED, "Poll has been canceled");
                    ret = CAT_RET_ERROR;
                }
#ifndef CAT_OS_WIN
                else if (poll->ret.status == CAT_EBADF) {
                    /* see: https://github.com/libuv/libuv/pull/1040#discussion_r80087447 */
                    *revents = POLLERR;
                    ret = CAT_RET_OK;
                }
#endif
                else {
                    cat_update_last_error_with_reason(poll->ret.status, "Poll failed");
                    *revents = cat_poll_translate_error_to_sys_events(events, poll->ret.status);
                    ret = CAT_RET_ERROR;
                }
            } else {
                ret = CAT_RET_OK;
                *revents = cat_poll_translate_uv_events_to_sys_events(poll->ret.events);
            }
            break;
        }
        /* timedout */
        case CAT_RET_OK:
            ret = CAT_RET_NONE;
            break;
        /* error */
        case CAT_RET_ERROR:
            cat_update_last_error_with_previous("Poll wait failed");
            break;
        default:
            CAT_NEVER_HERE("Impossible");
    }

    return ret;
}

CAT_API cat_ret_t cat_poll_one(cat_os_socket_t fd, cat_pollfd_events_t events, cat_pollfd_events_t *revents, cat_timeout_t timeout)
{
    cat_pollfd_events_t _revents;
    if (revents == NULL) {
        revents = &_revents;
    }

    CAT_LOG_DEBUG_VA(POLL, {
        char *events_str = cat_pollfd_events_str(events);
        CAT_LOG_DEBUG_D(POLL, "poll_one(fd: " CAT_OS_SOCKET_FMT ", events: %s, *revents: " CAT_LOG_UNFILLED_STR ", timeout: " CAT_TIMEOUT_FMT ") = " CAT_LOG_UNFINISHED_STR,
            fd, events_str, timeout);
        cat_buffer_str_free(events_str);
    });

    cat_ret_t ret = cat_poll_one_impl(fd, events, revents, timeout);

    CAT_LOG_DEBUG_VA(POLL, {
        char *events_str = cat_pollfd_events_str(events);
        char *revents_str = cat_pollfd_events_str(*revents);
        CAT_LOG_DEBUG_D(POLL, "poll_one(fd: " CAT_OS_SOCKET_FMT ", events: %s, *revents: %s, timeout: " CAT_TIMEOUT_FMT ") = " CAT_LOG_RET_RET_FMT,
            fd, events_str, revents_str, timeout, CAT_LOG_RET_RET_C(ret));
        cat_buffer_str_free(revents_str);
        cat_buffer_str_free(events_str);
    });

    return ret;
}

typedef struct cat_poll_context_s {
    cat_coroutine_t *coroutine;
    cat_bool_t done;
    cat_bool_t deferred_done_callback;
    cat_bool_t deferred_free_callback;
} cat_poll_context_t;

typedef struct {
    union {
        cat_poll_context_t *context;
        uv_handle_t handle;
        uv_poll_t poll;
    } u;
#ifdef CAT_ENABLE_DEBUG_LOG
    cat_os_socket_t fd;
#endif
    struct {
        int status; // uv status
        uv_events_t events; // uv events, e.g UV_EVENT_READABLE, UV_EVENT_WRITABLE...
    } ret;
    cat_bool_t initialized;
#ifdef CAT_OS_UNIX_LIKE
    cat_os_fd_t fd_dup;
#endif
} cat_poll_t;

static void cat_poll_free_callback(void *data)
{
    /** @record: previously, we double defer here to avoid use-after-free,
     * but after we refactor defer mechanism in uv_crun(), it seems that
     * we do not need double defer again, but it had to be verified, just
     * let us try to remove the double defer for now.
     *
     * code before: `cat_event_defer(cat_free_function, data);` */
    cat_free(data);
}

static void cat_poll_close_callback(uv_handle_t *handle)
{
    cat_poll_t *poll = cat_container_of(handle, cat_poll_t, u.handle);
    cat_poll_context_t *context = poll->u.context;

#ifdef CAT_OS_UNIX_LIKE
    if (poll->fd_dup != CAT_OS_INVALID_FD) {
        uv__close(poll->fd_dup);
    }
#endif
    if (!context->deferred_free_callback) {
        (void) cat_event_defer(cat_poll_free_callback, context);
        context->deferred_free_callback = cat_true;
    }
}

static void cat_poll_done_callback(cat_data_t *data)
{
    cat_poll_context_t *context = (cat_poll_context_t *) data;

    if (unlikely(context->done)) {
        // done
        return;
    }
    cat_coroutine_schedule(context->coroutine, EVENT, "Poll");
}

static void cat_poll_callback(uv_poll_t* handle, int status, uv_events_t events)
{
    cat_poll_t *poll = (cat_poll_t *) handle;
    cat_poll_context_t *context = poll->u.context;

    CAT_LOG_DEBUG_VA_WITH_LEVEL(POLL, 2, {
        char *events_str = cat_poll_uv_events_str(events);
        CAT_LOG_DEBUG_D(POLL, "poll_callback(fd: " CAT_OS_SOCKET_FMT ", status: %d" CAT_LOG_STRERRNO_FMT ", events: %s)",
            poll->fd, status, CAT_LOG_STRERRNO_C(status == 0, status), events_str);
        cat_buffer_str_free(events_str);
    });

    poll->ret.status = status;
    /* Note: uv may return multi events in multi callbacks */
    poll->ret.events |= events;

    /* Note: for get all revents of all pollfd at once,
     * we should schedule coroutine in io defer callback */
    if (!context->deferred_done_callback) {
        (void) cat_event_io_defer(cat_poll_done_callback, context);
        context->deferred_done_callback = cat_true;
    }
}

static int cat_poll_impl(cat_pollfd_t *fds, cat_nfds_t nfds, cat_timeout_t timeout)
{
    CAT_POLL_CHECK_TIMEOUT(timeout);
    cat_poll_context_t *context;
    cat_poll_t *polls;
    cat_nfds_t i, n = 0, e = 0;
    cat_ret_t ret;
    int error = 0;

    context = (cat_poll_context_t *) cat_malloc(sizeof(*context) + sizeof(*polls) * nfds);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(context == NULL)) {
        cat_update_last_error_of_syscall("Malloc for poll failed");
        return CAT_RET_ERROR;
    }
#endif
    polls = (cat_poll_t *) (((char *) context) + sizeof(*context));

    context->coroutine = CAT_COROUTINE_G(current);
    context->done = cat_false;
    context->deferred_done_callback = cat_false;
    context->deferred_free_callback = cat_false;
    for (i = 0; i < nfds;) {
        cat_pollfd_t *fd = &fds[i];
        cat_poll_t *poll = &polls[i];
        fd->revents = POLLNONE; // clear it
#ifdef CAT_ENABLE_DEBUG_LOG
        poll->fd = fd->fd;
#endif
        poll->initialized = cat_false;
        poll->ret.events = UV_EVENT_NONE;
        poll->u.context = context;
        do {
            cat_os_socket_t fd_no = fd->fd;
#ifdef CAT_OS_UNIX_LIKE
            poll->fd_dup = CAT_OS_INVALID_FD;
            if (unlikely(uv__fd_exists(&CAT_EVENT_G(loop), fd->fd))) {
                /* uv_poll_init_socket() and uv_poll_start() will return error if fd exists */
                poll->fd_dup = dup(fd->fd);
                if (unlikely(fd->fd == CAT_OS_INVALID_SOCKET)) {
                    poll->ret.status = cat_translate_sys_error(cat_sys_errno);
                    e++;
                    break;
                }
                fd_no = poll->fd_dup;
            }
#endif
            error = uv_poll_init_socket(&CAT_EVENT_G(loop), &poll->u.poll, fd_no);
            if (unlikely(error != 0)) {
                /* ENOTSOCK means it maybe a regular file */
                poll->ret.status = cat_poll_filter_init_error(error);
                e++;
                break;
            }
            poll->initialized = cat_true;
            if (e > 0) {
                /* fast return without starting handle */
                break;
            }
            error = uv_poll_start(&poll->u.poll, cat_poll_translate_sys_events_to_uv_events(fd->events), cat_poll_callback);
            if (unlikely(error != 0)) {
                poll->ret.status = error;
                e++;
                break;
            }
            poll->ret.status = CAT_ECANCELED;
        } while (0);
        i++;
    }

    if (e > 0) {
        /* fast return without waiting */
        ret = CAT_RET_NONE;
    } else {
        ret = cat_time_delay(timeout);
        if (unlikely(ret == CAT_RET_ERROR)) {
            cat_update_last_error_with_previous("Poll wait failed");
            goto _error;
        }
    }

    context->done = cat_true; // let defer know it was done

    for (; i-- > 0;) {
        cat_pollfd_t *fd = &fds[i];
        cat_poll_t *poll = &polls[i];
        if (poll->initialized) {
            uv_close(&poll->u.handle, cat_poll_close_callback);
        }
        if (unlikely(ret == CAT_RET_NONE && poll->ret.status < 0)) {
            if (poll->ret.status == CAT_ECANCELED) {
                fd->revents = POLLNONE;
            }
#ifndef CAT_OS_WIN
            else if (poll->ret.status == CAT_EBADF) {
                /* see: https://github.com/libuv/libuv/pull/1040#discussion_r80087447 */
                fd->revents = POLLERR;
                n++;
            }
#endif
            else {
                fd->revents = cat_poll_translate_error_to_sys_events(fd->events, poll->ret.status);
                n++;
            }
        } else {
            fd->revents = cat_poll_translate_uv_events_to_sys_events(poll->ret.events);
            if (fd->revents != POLLNONE) {
                n++;
            }
        }
    }

    if (!(nfds > e)) {
        cat_free(context);
    }

    return n;

    _error:
    CAT_ASSERT(!context->deferred_done_callback);
    if (nfds > e) {
        for (; i > 0; i--) {
            cat_poll_t *poll = &polls[i];
            if (poll->initialized) {
                uv_close(&poll->u.handle, cat_poll_close_callback);
            }
        }
    } else {
        cat_free(context);
    }
    return CAT_RET_ERROR;
}

#ifdef CAT_ENABLE_DEBUG_LOG
static CAT_BUFFER_STR_FREE char *cat_pollfds_str(cat_pollfd_t *fds, cat_nfds_t nfds, const cat_bool_t out)
{
    cat_buffer_t buffer;
    cat_nfds_t i;
    cat_buffer_create(&buffer, 256);
    cat_buffer_append_char(&buffer, '[');
    for (i = 0; i < nfds; i++) {
        char *events_str = cat_pollfd_events_str(fds[i].events);
        char *revents_str = !out ? NULL : cat_pollfd_events_str(fds[i].revents);
        if (i > 0) {
            cat_buffer_append_str(&buffer, ", ");
        }
        cat_buffer_append_printf(&buffer, "{" CAT_OS_SOCKET_FMT ", %s, %s}",
            fds[i].fd, events_str, revents_str != NULL ? revents_str : CAT_LOG_UNFILLED_STR);
        cat_buffer_str_free(events_str);
        if (revents_str != NULL) {
            cat_buffer_str_free(revents_str);
        }
    }
    cat_buffer_append_char(&buffer, ']');
    return cat_buffer_export_str(&buffer);
}
#endif

CAT_API int cat_poll(cat_pollfd_t *fds, cat_nfds_t nfds, cat_timeout_t timeout)
{
    CAT_LOG_DEBUG_VA(POLL, {
        char *fds_str = cat_pollfds_str(fds, nfds, cat_false);
        CAT_LOG_DEBUG_D(POLL, "poll(fds: %s, nfds: %zu, timeout: " CAT_TIMEOUT_FMT ") = " CAT_LOG_UNFINISHED_STR, fds_str, (size_t) nfds, timeout);
        cat_buffer_str_free(fds_str);
    });

    int ret = cat_poll_impl(fds, nfds, timeout);

    CAT_LOG_DEBUG_VA(POLL, {
        char *fds_str = cat_pollfds_str(fds, nfds, cat_true);
        CAT_LOG_DEBUG_D(POLL, "poll(fds: %s, nfds: %zu, timeout: " CAT_TIMEOUT_FMT ") = " CAT_LOG_INT_RET_FMT,
            fds_str, (size_t) nfds, timeout, CAT_LOG_INT_RET_C(ret));
        cat_buffer_str_free(fds_str);
    });

    return ret;
}

#define SAFE_FD_ZERO(set) do { \
    if (set != NULL) { \
        FD_ZERO(set); \
    } \
} while (0)

#define SAFE_FD_SET(fd, set) do { \
    CAT_ASSERT(set != NULL); \
    FD_SET(fd, set); \
} while (0)

#define SAFE_FD_ISSET(fd, set) (set != NULL && FD_ISSET(fd, set))

CAT_API int cat_select(int max_fd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    cat_pollfd_t *pfds, *pfd;
    cat_nfds_t nfds = 0, ifds;
    int fd, ret;

    if (unlikely(max_fd < 0)) {
        cat_update_last_error(CAT_EINVAL, "Select nfds is negative");
        return -1;
    }
    /* count nfds */
    for (fd = 0; fd < max_fd; fd++) {
        if (SAFE_FD_ISSET(fd, readfds) || SAFE_FD_ISSET(fd, writefds) || SAFE_FD_ISSET(fd, exceptfds)) {
            nfds++;
        }
    }
    /* nothing to do */
    if (nfds == 0) {
        return 0;
    }

    /* malloc for poll fds */
    pfds = (cat_pollfd_t *) cat_malloc(sizeof(*pfds) * nfds);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(pfds == NULL)) {
        cat_update_last_error_of_syscall("Malloc for poll fds failed");
        return -1;
    }
#endif

    /* translate from select structure to pollfd structure */
    ifds = 0;
    for (fd = 0; fd < max_fd; fd++) {
        cat_pollfd_events_t events = POLLNONE;
        if (SAFE_FD_ISSET(fd, readfds)) {
            events |= POLLIN;
        }
        if (SAFE_FD_ISSET(fd, writefds)) {
            events |= POLLOUT;
        }
        if (events == POLLNONE) {
            continue;
        }
        pfd = &pfds[ifds];
        pfd->fd = fd;
        pfd->events = events;
        pfd->revents = POLLNONE;
        ifds++;
    }
    CAT_ASSERT(ifds == nfds);

    ret = cat_poll(pfds, nfds, cat_time_tv2to(timeout));

    if (unlikely(ret < 0)) {
        goto _out;
    }

    /* translate from poll structure to select structrue */
    SAFE_FD_ZERO(readfds);
    SAFE_FD_ZERO(writefds);
    SAFE_FD_ZERO(exceptfds);
    for (ifds = 0; ifds < nfds; ifds++) {
        pfd = &pfds[ifds];
        if (pfd->revents & (POLLIN | POLLHUP | POLLPRI)) {
            SAFE_FD_SET(pfd->fd, readfds);
        }
        if (pfd->revents & POLLOUT) {
            SAFE_FD_SET(pfd->fd, writefds);
        }
        // ignore errors when event is already processed at IN and OUT handler.
        if ((pfd->revents & ~(POLLIN | POLLOUT)) != 0 &&
            !(pfd->revents & (POLLIN | POLLOUT))) {
            SAFE_FD_SET(pfd->fd, exceptfds);
        }
    }

    _out:
    cat_free(pfds);
    return ret;
}
