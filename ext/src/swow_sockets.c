/*
  +--------------------------------------------------------------------------+
  | Swow                                                                     |
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

#include "swow_sockets.h"

/*
 * Blocking calls of ext/sockets wait on the event loop, so that only the calling coroutine
 * waits, with the results, errors and timeouts (SO_RCVTIMEO, SO_SNDTIMEO) of the native call.
 *
 * | function                          | how                                                   |
 * | --------------------------------- | ----------------------------------------------------- |
 * | socket_read, socket_recv,         | ported: wait for POLLIN/POLLOUT, then the syscall;    |
 * | socket_write, socket_send         | a stream socket gets the whole buffer as the native   |
 * |                                   | blocking call does, in pieces that never block        |
 * | socket_recvfrom, socket_recvmsg,  | wait, then the native function, whose syscall is      |
 * | socket_sendto, socket_sendmsg     | ready; on timeout the native function runs with       |
 * |                                   | O_NONBLOCK and fails with EAGAIN as a timeout does    |
 * | socket_accept, socket_connect     | the native function with O_NONBLOCK, waiting while it |
 * |                                   | reports EAGAIN or EINPROGRESS (without a warning)     |
 * | socket_select                     | wait in cat_poll(), then the native select() with a   |
 * |                                   | zero timeout fills the arrays                         |
 *
 * A socket in non-blocking mode, or a call with MSG_DONTWAIT, goes to the native function.
 * The layout of the Socket object comes from php_sockets.h; its symbols (socket_ce, the module
 * globals, sockets_strerror()) are looked up at runtime, as ext/sockets may be a shared module.
 * Windows keeps the native functions.
 */

#if !defined(PHP_WIN32) && defined(__has_include)
# if __has_include("ext/sockets/php_sockets.h")
#  define SWOW_SOCKETS_HOOK 1
# endif
#endif

#ifdef SWOW_SOCKETS_HOOK

#include "swow_hook.h"
#include "swow_stream.h"

#include "cat_poll.h"
#include "cat_time.h"

/* the header declares its contents for the build of ext/sockets only */
#ifndef HAVE_SOCKETS
# define HAVE_SOCKETS 1
#endif
#include "ext/sockets/php_sockets.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>

static zend_class_entry *swow_sockets_socket_ce = NULL;
static zend_module_entry *swow_sockets_module = NULL;

#define SWOW_SOCKETS_ORIGINAL(name) static zif_handler swow_original_##name = NULL
SWOW_SOCKETS_ORIGINAL(socket_accept);
SWOW_SOCKETS_ORIGINAL(socket_connect);
SWOW_SOCKETS_ORIGINAL(socket_recvfrom);
SWOW_SOCKETS_ORIGINAL(socket_sendto);
SWOW_SOCKETS_ORIGINAL(socket_recvmsg);
SWOW_SOCKETS_ORIGINAL(socket_sendmsg);
SWOW_SOCKETS_ORIGINAL(socket_select);
#undef SWOW_SOCKETS_ORIGINAL

static zend_sockets_globals *swow_sockets_globals(void)
{
#ifdef ZTS
    return TSRMG_BULK(*swow_sockets_module->globals_id_ptr, zend_sockets_globals *);
#else
    return (zend_sockets_globals *) swow_sockets_module->globals_ptr;
#endif
}

/* PHP_SOCKET_ERROR(): sockets_strerror() is strerror() for errno values on Unix */
static void swow_sockets_error(php_socket *sock, const char *message, int error)
{
    sock->error = error;
    swow_sockets_globals()->last_error = error;
    if (error != EAGAIN && error != EWOULDBLOCK && error != EINPROGRESS) {
        php_error_docref(NULL, E_WARNING, "%s [%d]: %s", message, error, strerror(error));
    }
}

/* SO_RCVTIMEO or SO_SNDTIMEO, which limit a blocking call; zero means no limit */
static cat_timeout_t swow_sockets_timeout(const php_socket *sock, int option)
{
    struct timeval tv;
    socklen_t len = sizeof(tv);

    if (getsockopt(sock->bsd_socket, SOL_SOCKET, option, &tv, &len) != 0 || (tv.tv_sec == 0 && tv.tv_usec == 0)) {
        return CAT_TIMEOUT_FOREVER;
    }
    return cat_time_tv2to(&tv);
}

/* Wait until a blocking call for events does not block: 1 when ready, 0 when the timeout of
 * the socket expires, -1 with errno set when the wait fails (the coroutine was cancelled).
 * Coroutines may wait to read the same socket together: it has no buffer in PHP. */
static int swow_sockets_wait_timeout(const php_socket *sock, int events, cat_timeout_t timeout)
{
    int ready = swow_stream_wait_fd_ex(sock->bsd_socket, events, timeout, false);

    if (ready < 0) {
        errno = cat_orig_errno(cat_get_last_error_code());
    }
    return ready;
}

static int swow_sockets_wait(const php_socket *sock, int events)
{
    return swow_sockets_wait_timeout(sock, events, swow_sockets_timeout(sock, events & POLLOUT ? SO_SNDTIMEO : SO_RCVTIMEO));
}

/* The mode that socket_set_block() and socket_set_nonblock() set. The descriptor flag is no
 * guide: libuv turns O_NONBLOCK on while it polls, on the open file description, which the
 * processes of a prefork server share. */
static bool swow_sockets_is_blocking(const php_socket *sock)
{
    return sock->blocking;
}

/* Remaining time of a call that waits several times */
static cat_timeout_t swow_sockets_remaining(cat_timeout_t timeout, cat_msec_t start)
{
    if (timeout < 0) {
        return timeout;
    }
    cat_msec_t elapsed = cat_time_msec() - start;
    return elapsed >= (cat_msec_t) timeout ? 0 : (cat_timeout_t) ((cat_msec_t) timeout - elapsed);
}

/* The open Socket of the first argument, before the native function parses it, or NULL to
 * leave everything (errors included) to the native function */
static php_socket *swow_sockets_from_args(zend_execute_data *execute_data)
{
    zval *arg;
    php_socket *sock;

    if (ZEND_NUM_ARGS() < 1) {
        return NULL;
    }
    arg = ZEND_CALL_ARG(execute_data, 1);
    ZVAL_DEREF(arg);
    if (Z_TYPE_P(arg) != IS_OBJECT || Z_OBJCE_P(arg) != swow_sockets_socket_ce) {
        return NULL;
    }
    sock = Z_SOCKET_P(arg);
    return IS_INVALID_SOCKET(sock) ? NULL : sock;
}

/* The flags argument of the native function, if it was passed as an int */
static zend_long swow_sockets_flags_from_args(zend_execute_data *execute_data, uint32_t position)
{
    zval *arg;

    if (ZEND_NUM_ARGS() < position) {
        return 0;
    }
    arg = ZEND_CALL_ARG(execute_data, position);
    ZVAL_DEREF(arg);
    return Z_TYPE_P(arg) == IS_LONG ? Z_LVAL_P(arg) : 0;
}

/* Run the native function on the socket in non-blocking mode: nothing else runs meanwhile */
static void swow_sockets_call_nonblocking(php_socket *sock, zif_handler handler, INTERNAL_FUNCTION_PARAMETERS)
{
    php_socket_t fd = sock->bsd_socket;
    int flags = fcntl(fd, F_GETFL);
    bool toggle = flags != -1 && !(flags & O_NONBLOCK);

    if (toggle) {
        (void) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    handler(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    /* the native function may have closed or replaced nothing, but check the descriptor */
    if (toggle && sock->bsd_socket == fd) {
        (void) fcntl(fd, F_SETFL, flags);
    }
}

/* {{{ reads and writes */

/* recv() of a blocking socket */
static ssize_t swow_sockets_recv(php_socket *sock, char *buffer, size_t length, int flags)
{
    php_socket_t fd = sock->bsd_socket;
    cat_timeout_t timeout;
    cat_msec_t start;
    size_t received = 0;
    int ready;

    if ((flags & MSG_DONTWAIT) || !swow_sockets_is_blocking(sock)) {
        return recv(fd, buffer, length, flags);
    }
    timeout = swow_sockets_timeout(sock, SO_RCVTIMEO);
    ready = swow_sockets_wait_timeout(sock, POLLIN, timeout);
    if (ready <= 0) {
        if (ready == 0) {
            errno = EAGAIN;
        }
        return -1;
    }
    if (!(flags & MSG_WAITALL) || (flags & MSG_PEEK)) {
        return recv(fd, buffer, length, flags);
    }
    /* MSG_WAITALL: gather the whole length, or stop at the end of the stream, an error or
     * the timeout, returning what was received before, as the kernel does */
    start = cat_time_msec();
    for (;;) {
        ssize_t n = recv(fd, buffer + received, length - received, (flags & ~MSG_WAITALL) | MSG_DONTWAIT);
        if (n > 0) {
            received += (size_t) n;
            if (received == length) {
                return (ssize_t) received;
            }
        } else if (n == 0) {
            return (ssize_t) received;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            return received > 0 ? (ssize_t) received : -1;
        }
        ready = swow_sockets_wait_timeout(sock, POLLIN, swow_sockets_remaining(timeout, start));
        if (ready <= 0) {
            if (received > 0) {
                return (ssize_t) received;
            }
            if (ready == 0) {
                errno = EAGAIN;
            }
            return -1;
        }
    }
}

/* write() or send() of a blocking socket: a stream socket takes the whole buffer unless an
 * error or the timeout stops it, as the kernel does; the pieces are sized so that no send()
 * blocks (see swow_stream_fd_send_limit()) */
static ssize_t swow_sockets_send(php_socket *sock, const char *buffer, size_t length, int flags)
{
    php_socket_t fd = sock->bsd_socket;
    cat_timeout_t timeout;
    cat_msec_t start;
    size_t sent = 0, limit;
    int type = SOCK_STREAM, ready;
    socklen_t type_length = sizeof(type);

    if (length == 0 || (flags & MSG_DONTWAIT) || !swow_sockets_is_blocking(sock)) {
        return send(fd, buffer, length, flags);
    }
    timeout = swow_sockets_timeout(sock, SO_SNDTIMEO);
    (void) getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &type_length);
    if (type != SOCK_STREAM) {
        /* a datagram is sent whole or not at all */
        ready = swow_sockets_wait_timeout(sock, POLLOUT, timeout);
        if (ready <= 0) {
            if (ready == 0) {
                errno = EAGAIN;
            }
            return -1;
        }
        return send(fd, buffer, length, flags);
    }
    limit = swow_stream_fd_send_limit(fd);
    start = cat_time_msec();
    for (;;) {
        ready = swow_sockets_wait_timeout(sock, POLLOUT, swow_sockets_remaining(timeout, start));
        if (ready <= 0) {
            if (sent > 0) {
                return (ssize_t) sent;
            }
            if (ready == 0) {
                errno = EAGAIN;
            }
            return -1;
        }
        size_t piece = length - sent;
        if (limit > 0 && piece > limit) {
            piece = limit;
        }
        ssize_t n = send(fd, buffer + sent, piece, flags | MSG_DONTWAIT);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            return sent > 0 ? (ssize_t) sent : -1;
        }
        sent += (size_t) n;
        if (sent == length) {
            return (ssize_t) sent;
        }
    }
}

/* php_read() of ext/sockets, for PHP_NORMAL_READ: one byte at a time up to a line end. The
 * native function keeps retrying after a SO_RCVTIMEO timeout, so it waits without one. */
static int swow_sockets_read_line(php_socket *sock, char *buffer, size_t maxlen)
{
    int m = 0;
    size_t n = 0;
    int no_read = 0;
    bool nonblock = !swow_sockets_is_blocking(sock);
    char *t = buffer;

    errno = 0;
    *t = '\0';
    while (*t != '\n' && *t != '\r' && n < maxlen) {
        if (m > 0) {
            t++;
            n++;
        } else if (m == 0) {
            no_read++;
            /* the first pass reads nothing yet, so a non-blocking socket returns on the second */
            if (nonblock && no_read >= 2) {
                return (int) n;
            }
            if (no_read > 200) {
                errno = ECONNRESET;
                return -1;
            }
        }
        if (n < maxlen) {
            if (!nonblock) {
                if (swow_sockets_wait_timeout(sock, POLLIN, CAT_TIMEOUT_FOREVER) < 0) {
                    return -1;
                }
                /* errno is checked below whatever recv() returns, and the wait may leave one */
                errno = 0;
            }
            m = (int) recv(sock->bsd_socket, (void *) t, 1, 0);
        }
        if (errno != 0 && errno != ESPIPE && errno != EAGAIN) {
            return -1;
        }
        errno = 0;
    }
    if (n < maxlen) {
        /* the line end is counted too */
        n++;
    }

    return (int) n;
}

static PHP_FUNCTION(swow_socket_read)
{
    zval *arg1;
    php_socket *php_sock;
    zend_string *tmpbuf;
    int retval;
    zend_long length, type = PHP_BINARY_READ;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_OBJECT_OF_CLASS(arg1, swow_sockets_socket_ce)
        Z_PARAM_LONG(length)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(type)
    ZEND_PARSE_PARAMETERS_END();

    php_sock = Z_SOCKET_P(arg1);
    ENSURE_SOCKET_VALID(php_sock);

    /* overflow check */
    if (length <= 0 || length == ZEND_LONG_MAX) {
        RETURN_FALSE;
    }

    tmpbuf = zend_string_alloc(length, 0);

    if (type == PHP_NORMAL_READ) {
        retval = swow_sockets_read_line(php_sock, ZSTR_VAL(tmpbuf), length);
    } else {
        retval = (int) swow_sockets_recv(php_sock, ZSTR_VAL(tmpbuf), length, 0);
    }

    if (retval == -1) {
        /* no warning for a socket that has no data yet */
        if (PHP_IS_TRANSIENT_ERROR(errno)) {
            php_sock->error = errno;
            swow_sockets_globals()->last_error = errno;
        } else {
            swow_sockets_error(php_sock, "unable to read from socket", errno);
        }

        zend_string_efree(tmpbuf);
        RETURN_FALSE;
    } else if (!retval) {
        zend_string_efree(tmpbuf);
        RETURN_EMPTY_STRING();
    }

    tmpbuf = zend_string_truncate(tmpbuf, retval, 0);
    ZSTR_LEN(tmpbuf) = retval;
    ZSTR_VAL(tmpbuf)[ZSTR_LEN(tmpbuf)] = '\0';

    RETURN_NEW_STR(tmpbuf);
}

static PHP_FUNCTION(swow_socket_write)
{
    zval *arg1;
    php_socket *php_sock;
    ssize_t retval;
    size_t str_len;
    zend_long length = 0;
    bool length_is_null = 1;
    char *str;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_OBJECT_OF_CLASS(arg1, swow_sockets_socket_ce)
        Z_PARAM_STRING(str, str_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(length, length_is_null)
    ZEND_PARSE_PARAMETERS_END();

    php_sock = Z_SOCKET_P(arg1);
    ENSURE_SOCKET_VALID(php_sock);

    if (length < 0) {
        zend_argument_value_error(3, "must be greater than or equal to 0");
        RETURN_THROWS();
    }

    if (length_is_null) {
        length = str_len;
    }

    /* the native function uses write(), the same as send() without flags on a socket */
    retval = swow_sockets_send(php_sock, str, MIN((size_t) length, str_len), 0);

    if (retval < 0) {
        swow_sockets_error(php_sock, "unable to write to socket", errno);
        RETURN_FALSE;
    }

    RETURN_LONG((zend_long) retval);
}

static PHP_FUNCTION(swow_socket_recv)
{
    zval *php_sock_res, *buf;
    zend_string *recv_buf;
    php_socket *php_sock;
    int retval;
    zend_long len, flags;

    ZEND_PARSE_PARAMETERS_START(4, 4)
        Z_PARAM_OBJECT_OF_CLASS(php_sock_res, swow_sockets_socket_ce)
        Z_PARAM_ZVAL(buf)
        Z_PARAM_LONG(len)
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    php_sock = Z_SOCKET_P(php_sock_res);
    ENSURE_SOCKET_VALID(php_sock);

    /* overflow check */
    if (len <= 0 || len == ZEND_LONG_MAX) {
        RETURN_FALSE;
    }

    recv_buf = zend_string_alloc(len, 0);

    if ((retval = (int) swow_sockets_recv(php_sock, ZSTR_VAL(recv_buf), len, (int) flags)) < 1) {
        zend_string_efree(recv_buf);
        ZEND_TRY_ASSIGN_REF_NULL(buf);
    } else {
        ZSTR_LEN(recv_buf) = retval;
        ZSTR_VAL(recv_buf)[ZSTR_LEN(recv_buf)] = '\0';
        ZEND_TRY_ASSIGN_REF_NEW_STR(buf, recv_buf);
    }

    if (retval == -1) {
        swow_sockets_error(php_sock, "Unable to read from socket", errno);
        RETURN_FALSE;
    }

    RETURN_LONG(retval);
}

static PHP_FUNCTION(swow_socket_send)
{
    zval *arg1;
    php_socket *php_sock;
    size_t buf_len;
    ssize_t retval;
    zend_long len, flags;
    char *buf;

    ZEND_PARSE_PARAMETERS_START(4, 4)
        Z_PARAM_OBJECT_OF_CLASS(arg1, swow_sockets_socket_ce)
        Z_PARAM_STRING(buf, buf_len)
        Z_PARAM_LONG(len)
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    php_sock = Z_SOCKET_P(arg1);
    ENSURE_SOCKET_VALID(php_sock);

    if (len < 0) {
        zend_argument_value_error(3, "must be greater than or equal to 0");
        RETURN_THROWS();
    }

    retval = swow_sockets_send(php_sock, buf, (buf_len < (size_t) len ? buf_len : (size_t) len), (int) flags);

    if (retval == -1) {
        swow_sockets_error(php_sock, "Unable to write to socket", errno);
        RETURN_FALSE;
    }

    RETURN_LONG((zend_long) retval);
}

/* socket_recvfrom(), socket_sendto(), socket_recvmsg() and socket_sendmsg(): wait, then the
 * native function; flags_position is the position of their flags argument */
static void swow_sockets_wait_then_call(INTERNAL_FUNCTION_PARAMETERS, int events, uint32_t flags_position, zif_handler original_handler)
{
    php_socket *sock = swow_sockets_from_args(execute_data);

    if (sock != NULL && !(swow_sockets_flags_from_args(execute_data, flags_position) & MSG_DONTWAIT) && swow_sockets_is_blocking(sock)) {
        int ready = swow_sockets_wait(sock, events);
        /* the socket may have been closed while this coroutine waited */
        if (ready <= 0 && !IS_INVALID_SOCKET(sock)) {
            /* the native function then fails with EAGAIN, as after a timeout */
            swow_sockets_call_nonblocking(sock, original_handler, INTERNAL_FUNCTION_PARAM_PASSTHRU);
            return;
        }
    }
    original_handler(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

static PHP_FUNCTION(swow_socket_recvfrom)
{
    swow_sockets_wait_then_call(INTERNAL_FUNCTION_PARAM_PASSTHRU, POLLIN, 4, swow_original_socket_recvfrom);
}

static PHP_FUNCTION(swow_socket_sendto)
{
    swow_sockets_wait_then_call(INTERNAL_FUNCTION_PARAM_PASSTHRU, POLLOUT, 4, swow_original_socket_sendto);
}

static PHP_FUNCTION(swow_socket_recvmsg)
{
    swow_sockets_wait_then_call(INTERNAL_FUNCTION_PARAM_PASSTHRU, POLLIN, 3, swow_original_socket_recvmsg);
}

static PHP_FUNCTION(swow_socket_sendmsg)
{
    swow_sockets_wait_then_call(INTERNAL_FUNCTION_PARAM_PASSTHRU, POLLOUT, 3, swow_original_socket_sendmsg);
}

/* }}} */

/* {{{ accept and connect */

static PHP_FUNCTION(swow_socket_accept)
{
    php_socket *sock = swow_sockets_from_args(execute_data);
    int last_error;

    if (sock == NULL || !swow_sockets_is_blocking(sock)) {
        swow_original_socket_accept(INTERNAL_FUNCTION_PARAM_PASSTHRU);
        return;
    }
    /* Processes of a prefork server wait on the same listening socket, and another one may
     * take the connection between the wake-up and the accept(): the accept() then runs in
     * non-blocking mode, and the wait goes on after an EAGAIN, which the native function
     * reports without a warning. */
    last_error = swow_sockets_globals()->last_error;
    for (;;) {
        int ready = swow_sockets_wait(sock, POLLIN);
        if (IS_INVALID_SOCKET(sock)) {
            /* closed while this coroutine waited: the native function reports it */
            swow_original_socket_accept(INTERNAL_FUNCTION_PARAM_PASSTHRU);
            return;
        }
        swow_sockets_call_nonblocking(sock, swow_original_socket_accept, INTERNAL_FUNCTION_PARAM_PASSTHRU);
        if (Z_TYPE_P(return_value) == IS_OBJECT) {
            php_socket *new_sock = Z_SOCKET_P(return_value);
            /* BSD systems copy O_NONBLOCK of the listening socket to the accepted one */
            int flags = fcntl(new_sock->bsd_socket, F_GETFL);
            if (flags != -1 && (flags & O_NONBLOCK)) {
                (void) fcntl(new_sock->bsd_socket, F_SETFL, flags & ~O_NONBLOCK);
            }
            return;
        }
        if (ready <= 0 || EG(exception) || swow_sockets_globals()->last_error != EAGAIN) {
            /* a real error, or the timeout of the socket (SO_RCVTIMEO) that ends in EAGAIN */
            return;
        }
        swow_sockets_globals()->last_error = last_error;
        zval_ptr_dtor(return_value);
        ZVAL_UNDEF(return_value);
    }
}

static PHP_FUNCTION(swow_socket_connect)
{
    php_socket *sock = swow_sockets_from_args(execute_data);
    int sock_error, last_error, error = 0;
    socklen_t error_length = sizeof(error);

    if (sock == NULL || !swow_sockets_is_blocking(sock)) {
        swow_original_socket_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU);
        return;
    }
    sock_error = sock->error;
    last_error = swow_sockets_globals()->last_error;
    /* the native function parses the address and reports EINPROGRESS without a warning */
    swow_sockets_call_nonblocking(sock, swow_original_socket_connect, INTERNAL_FUNCTION_PARAM_PASSTHRU);
    if (Z_TYPE_P(return_value) != IS_FALSE || IS_INVALID_SOCKET(sock)) {
        return;
    }
    if (sock->error == EAGAIN) {
        /* Linux refuses a non-blocking connect() to a UNIX socket whose backlog is full,
         * which poll() cannot wait for: the native function waits */
        sock->error = sock_error;
        swow_sockets_globals()->last_error = last_error;
        swow_original_socket_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU);
        return;
    }
    if (sock->error != EINPROGRESS) {
        return;
    }
    /* Linux limits a blocking connect() with SO_SNDTIMEO and then fails with EINPROGRESS */
    if (swow_sockets_wait(sock, POLLOUT) <= 0 || IS_INVALID_SOCKET(sock)) {
        return;
    }
    if (getsockopt(sock->bsd_socket, SOL_SOCKET, SO_ERROR, &error, &error_length) != 0) {
        error = errno;
    }
    if (error != 0) {
        swow_sockets_error(sock, "unable to connect", error);
        return;
    }
    sock->error = sock_error;
    swow_sockets_globals()->last_error = last_error;
    RETURN_TRUE;
}

/* }}} */

/* {{{ select */

/* Add the sockets of an array argument of socket_select() to fds; false when an element is
 * not an open Socket, and the native function reports it */
static bool swow_sockets_select_add(zval *array, short events, cat_pollfd_t **fds, cat_nfds_t *nfds, cat_nfds_t *size)
{
    zval *element;

    if (array == NULL) {
        return true;
    }
    ZVAL_DEREF(array);
    if (Z_TYPE_P(array) == IS_NULL) {
        return true;
    }
    if (Z_TYPE_P(array) != IS_ARRAY) {
        return false;
    }
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(array), element) {
        php_socket *sock;
        ZVAL_DEREF(element);
        if (Z_TYPE_P(element) != IS_OBJECT || Z_OBJCE_P(element) != swow_sockets_socket_ce) {
            return false;
        }
        sock = Z_SOCKET_P(element);
        if (IS_INVALID_SOCKET(sock)) {
            return false;
        }
        if (*nfds == *size) {
            *size = *size * 2 + 8;
            *fds = (cat_pollfd_t *) erealloc(*fds, sizeof(**fds) * *size);
        }
        (*fds)[*nfds].fd = sock->bsd_socket;
        (*fds)[*nfds].events = events;
        (*fds)[*nfds].revents = 0;
        (*nfds)++;
    } ZEND_HASH_FOREACH_END();

    return true;
}

static PHP_FUNCTION(swow_socket_select)
{
    cat_pollfd_t *fds = NULL;
    cat_nfds_t nfds = 0, size = 0, i;
    cat_timeout_t timeout = CAT_TIMEOUT_FOREVER;
    zval *seconds, *microseconds = NULL;
    zval saved_seconds, saved_microseconds;
    bool ok;

    /* argument errors are left to the native function */
    if (ZEND_NUM_ARGS() < 4) {
        swow_original_socket_select(INTERNAL_FUNCTION_PARAM_PASSTHRU);
        return;
    }
    seconds = ZEND_CALL_ARG(execute_data, 4);
    if (ZEND_NUM_ARGS() >= 5) {
        microseconds = ZEND_CALL_ARG(execute_data, 5);
        if (Z_TYPE_P(microseconds) != IS_LONG) {
            swow_original_socket_select(INTERNAL_FUNCTION_PARAM_PASSTHRU);
            return;
        }
    }
    if (Z_TYPE_P(seconds) == IS_LONG) {
        zend_long usec = microseconds != NULL ? Z_LVAL_P(microseconds) : 0;
        if (Z_LVAL_P(seconds) < 0 || usec < 0) {
            swow_original_socket_select(INTERNAL_FUNCTION_PARAM_PASSTHRU);
            return;
        }
        timeout = (cat_timeout_t) (Z_LVAL_P(seconds) * 1000 + usec / 1000);
    } else if (Z_TYPE_P(seconds) != IS_NULL) {
        swow_original_socket_select(INTERNAL_FUNCTION_PARAM_PASSTHRU);
        return;
    }
    if (timeout == 0) {
        swow_original_socket_select(INTERNAL_FUNCTION_PARAM_PASSTHRU);
        return;
    }

    ok = swow_sockets_select_add(ZEND_CALL_ARG(execute_data, 1), POLLIN, &fds, &nfds, &size) &&
         swow_sockets_select_add(ZEND_CALL_ARG(execute_data, 2), POLLOUT, &fds, &nfds, &size) &&
         swow_sockets_select_add(ZEND_CALL_ARG(execute_data, 3), POLLPRI, &fds, &nfds, &size);
    if (ok && nfds > 0) {
        /* libuv turns O_NONBLOCK on while it polls; the registry restores the blocking mode */
        for (i = 0; i < nfds; i++) {
            swow_stream_fd_wait_begin(fds[i].fd, !(fds[i].events & POLLOUT));
        }
        (void) cat_poll(fds, nfds, timeout);
        for (i = 0; i < nfds; i++) {
            swow_stream_fd_wait_end(fds[i].fd, !(fds[i].events & POLLOUT));
        }
        /* Whatever the wait found, the native select() with a zero timeout gives the result
         * and fills the arrays; it reports errors too. */
        ZVAL_COPY_VALUE(&saved_seconds, seconds);
        ZVAL_LONG(seconds, 0);
        if (microseconds != NULL) {
            ZVAL_COPY_VALUE(&saved_microseconds, microseconds);
            ZVAL_LONG(microseconds, 0);
        }
        swow_original_socket_select(INTERNAL_FUNCTION_PARAM_PASSTHRU);
        ZVAL_COPY_VALUE(seconds, &saved_seconds);
        if (microseconds != NULL) {
            ZVAL_COPY_VALUE(microseconds, &saved_microseconds);
        }
    } else {
        swow_original_socket_select(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    }
    if (fds != NULL) {
        efree(fds);
    }
}

/* }}} */

#endif /* SWOW_SOCKETS_HOOK */

zend_result swow_sockets_module_init(INIT_FUNC_ARGS)
{
#ifdef SWOW_SOCKETS_HOOK
    /* ext/sockets is optional, and loaded before Swow when present (see swow_module_deps) */
    swow_sockets_module = (zend_module_entry *) zend_hash_str_find_ptr(&module_registry, ZEND_STRL("sockets"));
    swow_sockets_socket_ce = (zend_class_entry *) zend_hash_str_find_ptr(CG(class_table), ZEND_STRL("socket"));
    if (swow_sockets_module == NULL || swow_sockets_socket_ce == NULL) {
        return SUCCESS;
    }
#ifdef ZTS
    if (swow_sockets_module->globals_id_ptr == NULL) {
#else
    if (swow_sockets_module->globals_ptr == NULL) {
#endif
        return SUCCESS;
    }

    (void) swow_hook_internal_function_handler(ZEND_STRL("socket_read"), PHP_FN(swow_socket_read));
    (void) swow_hook_internal_function_handler(ZEND_STRL("socket_write"), PHP_FN(swow_socket_write));
    (void) swow_hook_internal_function_handler(ZEND_STRL("socket_recv"), PHP_FN(swow_socket_recv));
    (void) swow_hook_internal_function_handler(ZEND_STRL("socket_send"), PHP_FN(swow_socket_send));
#define SWOW_SOCKETS_HOOK_EX(name) \
    (void) swow_hook_internal_function_handler_ex(ZEND_STRL(#name), PHP_FN(swow_##name), &swow_original_##name)
    SWOW_SOCKETS_HOOK_EX(socket_recvfrom);
    SWOW_SOCKETS_HOOK_EX(socket_sendto);
    SWOW_SOCKETS_HOOK_EX(socket_recvmsg);
    SWOW_SOCKETS_HOOK_EX(socket_sendmsg);
    SWOW_SOCKETS_HOOK_EX(socket_accept);
    SWOW_SOCKETS_HOOK_EX(socket_connect);
    SWOW_SOCKETS_HOOK_EX(socket_select);
#undef SWOW_SOCKETS_HOOK_EX
#endif

    return SUCCESS;
}
