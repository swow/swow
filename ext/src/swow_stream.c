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

#include "swow_stream.h"

#include "swow_hook.h"

#include "cat_socket.h"
#include "cat_time.h" /* for time_tv2to() */

#include "php.h"
#include "ext/standard/file.h"
#include "streams/php_streams_int.h"
#include "php_network.h"

#if defined(AF_UNIX)
#include <sys/un.h>
#endif

/* $Id: f078bca729f4ab1bc2d60370e83bfa561f86b88d $ */

#if PHP_VERSION_ID < 70400
#define bytes_t size_t
#define PHP_STREAM_SOCKET_RETURN_ERR 0
#else
#define bytes_t ssize_t
#define PHP_STREAM_SOCKET_RETURN_ERR -1
#endif

typedef struct
{
    php_netstream_data_t sock;
    cat_socket_t socket;
} swow_netstream_data_t;

#define SWOW_STREAM_MEMBERS(stream, swow_sock, sock, socket) \
    swow_netstream_data_t *swow_sock = (swow_netstream_data_t *) (stream)->abstract; \
    php_netstream_data_t *sock = EXPECTED(swow_sock != NULL) ? &swow_sock->sock : NULL; \
    cat_socket_t *socket = EXPECTED(swow_sock != NULL) ? &swow_sock->socket : NULL; \
    (void) swow_sock; (void) sock; (void) socket

#define SWOW_STREAM_SOCKET_GETTER(stream, swow_sock, sock, socket, failure) \
    SWOW_STREAM_MEMBERS(stream, swow_sock, sock, socket); \
    do { \
        if (socket == NULL) { \
            failure; \
        } \
    } while (0)

SWOW_API php_stream *swow_stream_socket_factory(
    const char *proto, size_t protolen,
    const char *resourcename, size_t resourcenamelen,
    const char *persistent_id, int options, int flags,
    struct timeval *timeout,
    php_stream_context *context STREAMS_DC
)
{
    swow_netstream_data_t *swow_sock;
    php_netstream_data_t *sock;
    php_stream *stream;
    const php_stream_ops *ops;

    /* check */
    if (UNEXPECTED(flags & STREAM_XPORT_CONNECT_ASYNC)) {
        // php_error_docref(NULL, E_WARNING, "Stream can only be in blocking mode");
        // ignore
    }

    /* alloc and init */
    swow_sock = (swow_netstream_data_t *) pecalloc(1, sizeof(*swow_sock), persistent_id ? 1 : 0);
    sock = &swow_sock->sock;
    sock->is_blocked = 1;
    sock->timeout.tv_sec = FG(default_socket_timeout);
    sock->timeout.tv_usec = 0;
    /* we don't know the socket until we have determined if we are binding or connecting */
    sock->socket = -1;

    /* which type of socket ? */
    if (strncmp(proto, "tcp", protolen) == 0) {
        ops = &swow_stream_tcp_socket_ops;
    } else if (strncmp(proto, "udp", protolen) == 0) {
        ops = &swow_stream_udp_socket_ops;
    } else if (strncmp(proto, "pipe", protolen) == 0) {
        ops = &swow_stream_pipe_socket_ops;
    }
#ifdef AF_UNIX
    else if (strncmp(proto, "unix", protolen) == 0) {
        ops = &swow_stream_unix_socket_ops;
    } else if (strncmp(proto, "udg", protolen) == 0) {
        ops = &swow_stream_udg_socket_ops;
    }
#endif
    else {
        ops = NULL;
        CAT_NEVER_HERE("Unknown protocol");
    }

    /* alloc php_stream (php_stream_ops * is not const on PHP-7.x) */
    stream = php_stream_alloc_rel((php_stream_ops *) ops, sock, persistent_id, "r+");
    if (UNEXPECTED(stream == NULL)) {
        pefree(swow_sock, persistent_id ? 1 : 0);
        return NULL;
    }
    stream->abstract = (swow_netstream_data_t *) swow_sock;

    return stream;
}

static cat_socket_type_t swow_stream_parse_socket_type(const php_stream_ops *ops)
{
    if (ops == &swow_stream_tcp_socket_ops) {
        return CAT_SOCKET_TYPE_TCP;
    } else if (ops == &swow_stream_udp_socket_ops) {
        return CAT_SOCKET_TYPE_UDP;
#ifndef PHP_WIN32
    } else if (ops == &swow_stream_unix_socket_ops) {
        return CAT_SOCKET_TYPE_UNIX;
    } else if (ops == &swow_stream_udg_socket_ops) {
        return CAT_SOCKET_TYPE_UDG;
#endif //PHP_WIN32
    } else /* swow_stream_generic_socket_ops */{
        return CAT_SOCKET_TYPE_TCP;
    }
}

static char *swow_stream_parse_ip_address_ex(const char *str, size_t str_len, int *portno, int get_err, zend_string **err)
{
    char *colon;
    char *host = NULL;

#ifdef HAVE_IPV6
    char *p;

    if (*(str) == '[' && str_len > 1) {
        /* IPV6 notation to specify raw address with port (i.e. [fe80::1]:80) */
        p = memchr(str + 1, ']', str_len - 2);
        if (!p || *(p + 1) != ':') {
            if (get_err) {
                *err = strpprintf(0, "Failed to parse IPv6 address \"%s\"", str);
            }
            return NULL;
        }
        *portno = atoi(p + 2);
        return estrndup(str + 1, p - str - 1);
    }
#endif
    if (str_len) {
        colon = memchr(str, ':', str_len - 1);
    } else {
        colon = NULL;
    }
    if (colon) {
        *portno = atoi(colon + 1);
        host = estrndup(str, colon - str);
    } else {
        if (get_err) {
            *err = strpprintf(0, "Failed to parse address \"%s\"", str);
        }
        return NULL;
    }

    return host;
}

static char *swow_stream_parse_ip_address(php_stream_xport_param *xparam, int *portno)
{
    return swow_stream_parse_ip_address_ex(xparam->inputs.name, xparam->inputs.namelen, portno, xparam->want_errortext, &xparam->outputs.error_text);
}

static inline int swow_stream_bind(php_stream *stream, swow_netstream_data_t *swow_sock, php_stream_xport_param *xparam)
{
    cat_socket_type_t type = swow_stream_parse_socket_type(stream->ops);
    cat_socket_t *socket = &swow_sock->socket;
    cat_socket_bind_flags_t bind_flags = CAT_SOCKET_BIND_FLAG_NONE;
    char *ip = NULL;
    const char *host;
    size_t host_len;
    int port = 0;
    zval *tmpzval = NULL;
    int ret;

    if (!(type & CAT_SOCKET_TYPE_FLAG_LOCAL)) {

        ip = swow_stream_parse_ip_address(xparam, &port);

        if (ip == NULL) {
            return -1;
        }

        host = ip;
        host_len = strlen(ip);

#ifdef IPV6_V6ONLY
        if (PHP_STREAM_CONTEXT(stream)
            && (tmpzval = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "ipv6_v6only")) != NULL
            && Z_TYPE_P(tmpzval) != IS_NULL
            && zend_is_true(tmpzval)
        ) {
            type |= CAT_SOCKET_TYPE_FLAG_IPV6;
        }
#endif

#ifdef SO_REUSEPORT
        if (PHP_STREAM_CONTEXT(stream)
            && (tmpzval = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "so_reuseport")) != NULL
            && zend_is_true(tmpzval)
        ) {
            bind_flags |= CAT_SOCKET_BIND_FLAG_REUSEPORT;
        }
#endif

#ifdef SO_BROADCAST
        if (stream->ops == &swow_stream_udp_socket_ops /* SO_BROADCAST is only applicable for UDP */
            && PHP_STREAM_CONTEXT(stream)
            && (tmpzval = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "so_broadcast")) != NULL
            && zend_is_true(tmpzval)
        ) {
            type |= CAT_SOCKET_TYPE_FLAG_UDP_BROADCAST;
        }
#endif
    } else {
        host = xparam->inputs.name;
        host_len = xparam->inputs.namelen;
    }

    socket = cat_socket_create(socket, type);

    if (UNEXPECTED(socket == NULL)) {
        goto _error;
    }

    ret = cat_socket_bind(socket, host, host_len, port) ? 0 : -1;

    if (UNEXPECTED(ret != 0)) {
        goto _error;
    }

    swow_sock->sock.socket = cat_socket_get_fd(socket);

    if (0) {
        _error:
        if (socket != NULL) {
            if (cat_socket_is_available(socket)) {
                cat_socket_close(socket);
            }
            socket = NULL;
        }
        xparam->outputs.error_code = cat_get_last_error_code();
        if (xparam->want_errortext) {
            xparam->outputs.error_text = strpprintf(0, "%s", cat_get_last_error_message());
        }
    }

    if (ip) {
        efree(ip);
    }

    return socket != NULL ? 0 : -1;
}

static inline int swow_stream_accept(php_stream *stream, swow_netstream_data_t *server_sock, php_stream_xport_param *xparam STREAMS_DC)
{
    swow_netstream_data_t *client_sock;
    cat_socket_t *server, *client;
    const cat_sockaddr_info_t *address_info;
    zend_bool delay = 0;
    zval *tmpzval = NULL;

    client_sock = (swow_netstream_data_t *) ecalloc(1, sizeof(*client_sock));

    server = &server_sock->socket;
    client = &client_sock->socket;

    xparam->outputs.client = NULL;

    if ((NULL != PHP_STREAM_CONTEXT(stream)) &&
        (tmpzval = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "tcp_nodelay")) != NULL &&
        !zend_is_true(tmpzval)) {
        delay = 1;
    }

    client = cat_socket_accept_ex(server, client, cat_time_tv2to(xparam->inputs.timeout));

    if (UNEXPECTED(client == NULL)) {
        efree(client_sock);
        goto _error;
    }

    address_info = cat_socket_getsockname_fast(client);

    if (UNEXPECTED(address_info == NULL)) {
        cat_socket_close(client);
        efree(client_sock);
        goto _error;
    }

    php_network_populate_name_from_sockaddr(
        (struct sockaddr *) &address_info->address.common,address_info->length,
        xparam->want_textaddr ? &xparam->outputs.textaddr : NULL,
        xparam->want_addr ? &xparam->outputs.addr : NULL,
        xparam->want_addr ? &xparam->outputs.addrlen : NULL
    );

    if (delay) {
        (void) cat_socket_set_tcp_nodelay(client, cat_false);
    }

    client_sock->sock.socket = cat_socket_get_fd(client);

    xparam->outputs.client = php_stream_alloc_rel(stream->ops, &client_sock->sock, NULL, "r+");
    if (xparam->outputs.client) {
        xparam->outputs.client->ctx = stream->ctx;
        if (stream->ctx) {
            GC_ADDREF(stream->ctx);
        }
    }

    return 0;

    _error:

    xparam->outputs.error_code = cat_get_last_error_code();
    if (xparam->want_errortext) {
        xparam->outputs.error_text = strpprintf(0, "%s", cat_get_last_error_message());
    }

    return -1;
}

static int swow_stream_connect(php_stream *stream, swow_netstream_data_t *swow_sock, php_stream_xport_param *xparam)
{
    cat_socket_type_t type = swow_stream_parse_socket_type(stream->ops);
    cat_socket_t *socket = &swow_sock->socket;
    char *host = NULL, *bind_host = NULL;
    int port, bind_port = 0;
    zval *ztmp = NULL;

    if (type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
        int ret;

        socket = cat_socket_create(socket, type);

        if (UNEXPECTED(socket == NULL)) {
            goto _error;
        }

        ret = cat_socket_connect_ex(socket, xparam->inputs.name, xparam->inputs.namelen, 0, cat_time_tv2to(xparam->inputs.timeout)) ? 0 : -1;

        if (UNEXPECTED(ret != 0)) {
            goto _error;
        }

        goto _success;
    }

    host = swow_stream_parse_ip_address(xparam, &port);

    if (host == NULL) {
        return -1;
    }

    if (PHP_STREAM_CONTEXT(stream) && (ztmp = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "bindto")) != NULL) {
        if (Z_TYPE_P(ztmp) != IS_STRING) {
            if (xparam->want_errortext) {
                xparam->outputs.error_text = strpprintf(0, "local_addr context option is not a string.");
            }
            efree(host);
            return -1;
        }
        bind_host = swow_stream_parse_ip_address_ex(Z_STRVAL_P(ztmp), Z_STRLEN_P(ztmp), &bind_port, xparam->want_errortext, &xparam->outputs.error_text);
        if (bind_host == NULL) {
            efree(host);
            return -1;
        }
    }

#ifdef SO_BROADCAST
    if (stream->ops == &swow_stream_udp_socket_ops && /* SO_BROADCAST is only applicable for UDP */
        PHP_STREAM_CONTEXT(stream) &&
        (ztmp = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "so_broadcast")) != NULL &&
        zend_is_true(ztmp)
    ) {
        type |= CAT_SOCKET_TYPE_FLAG_UDP_BROADCAST;
    }
#endif
    if (
        stream->ops == &swow_stream_tcp_socket_ops && /* TCP_NODELAY is only applicable for TCP */
        PHP_STREAM_CONTEXT(stream)  &&
        (ztmp = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "tcp_nodelay")) != NULL &&
        !zend_is_true(ztmp)
    ) {
        type |= CAT_SOCKET_TYPE_FLAG_TCP_DELAY;
    }

    socket = cat_socket_create(socket, type);
    if (UNEXPECTED(socket == NULL)) {
        goto _error;
    }
    if (bind_host) {
        if (UNEXPECTED(!cat_socket_bind(socket, bind_host, strlen(bind_host), bind_port))) {
            goto _error;
        }
    }
    if (UNEXPECTED(!cat_socket_connect_ex(socket, host, strlen(host), port, cat_time_tv2to(xparam->inputs.timeout)))) {
        goto _error;
    }

    if (1) {
        _success:
        swow_sock->sock.socket = cat_socket_get_fd(socket);
    } else {
        _error:
        if (socket != NULL) {
            if (cat_socket_is_available(socket)) {
                cat_socket_close(socket);
            }
            socket = NULL;
        }
        xparam->outputs.error_code = cat_get_last_error_code();
        if (xparam->want_errortext) {
            xparam->outputs.error_text = strpprintf(0, "%s", cat_get_last_error_message());
        }
    }

    if (host != NULL) {
        efree(host);
    }
    if (bind_host != NULL) {
        efree(bind_host);
    }

    return socket != NULL ? 0 : -1;
}

static inline int swow_stream_socket_recvfrom(
    cat_socket_t *socket, char *buf, size_t buflen, zend_bool peek,
    zend_string **textaddr,
    struct sockaddr **addr, socklen_t *addrlen
)
{
    int ret;
    int want_addr = textaddr || addr;

    if (want_addr) {
        php_sockaddr_storage sa;
        socklen_t sl = sizeof(sa);
        if (!peek) {
            ret = cat_socket_recvfrom(socket, buf, buflen, (struct sockaddr *) &sa, &sl);
        } else {
            ret = cat_socket_peekfrom(socket, buf, buflen, (struct sockaddr *) &sa, &sl);
        }
#ifdef PHP_WIN32
        /* POSIX discards excess bytes without signalling failure; emulate this on Windows */
        if (ret == -1 && cat_get_last_error_code() == CAT_EMSGSIZE) {
            ret = buflen;
        }
#endif
        if (sl) {
            php_network_populate_name_from_sockaddr((struct sockaddr *) &sa, sl, textaddr, addr, addrlen);
        } else {
            if (textaddr) {
                *textaddr = ZSTR_EMPTY_ALLOC();
            }
            if (addr) {
                *addr = NULL;
                *addrlen = 0;
            }
        }
    } else {
        if (!peek) {
            ret = cat_socket_recv(socket, buf, buflen);
        } else {
            ret = cat_socket_peek(socket, buf, buflen);
        }
    }

    return ret;
}

static int swow_stream_set_option(php_stream *stream, int option, int value, void *ptrparam)
{
    SWOW_STREAM_MEMBERS(stream, swow_sock, sock, socket);
    php_stream_xport_param *xparam;

    if (!swow_sock) {
        return PHP_STREAM_OPTION_RETURN_ERR;
    }

    switch (option) {
        case PHP_STREAM_OPTION_CHECK_LIVENESS: {
            if (!socket || !cat_socket_check_liveness(socket)) {
                return PHP_STREAM_OPTION_RETURN_ERR;
            }
            return PHP_STREAM_OPTION_RETURN_OK;
        }
        case PHP_STREAM_OPTION_BLOCKING: {
            /* always blocked */
            return PHP_STREAM_OPTION_RETURN_OK;
        }
        case PHP_STREAM_OPTION_READ_TIMEOUT: {
            sock->timeout = *(struct timeval *) ptrparam;
            sock->timeout_event = 0;
            return PHP_STREAM_OPTION_RETURN_OK;
        }
        case PHP_STREAM_OPTION_META_DATA_API: {
            add_assoc_bool((zval *) ptrparam, "timed_out", sock->timeout_event);
            add_assoc_bool((zval *) ptrparam, "blocked", sock->is_blocked);
            add_assoc_bool((zval *) ptrparam, "eof", stream->eof);
            return PHP_STREAM_OPTION_RETURN_OK;
        }
        case PHP_STREAM_OPTION_XPORT_API:
            xparam = (php_stream_xport_param *) ptrparam;
            switch (xparam->op) {
                case STREAM_XPORT_OP_LISTEN: {
                    xparam->outputs.returncode = (socket && cat_socket_listen(socket, xparam->inputs.backlog)) ? 0 : -1;
                    return PHP_STREAM_OPTION_RETURN_OK;
                }
                case STREAM_XPORT_OP_GET_NAME: {
                    const cat_sockaddr_info_t *name = cat_socket_getsockname_fast(socket);

                    if (UNEXPECTED(name == NULL)) {
                        xparam->outputs.returncode = -1;
                    } else {
                        php_network_populate_name_from_sockaddr(
                            (struct sockaddr *) &name->address.common, name->length,
                            xparam->want_textaddr ? &xparam->outputs.textaddr : NULL,
                            xparam->want_addr ? &xparam->outputs.addr : NULL,
                            xparam->want_addr ? &xparam->outputs.addrlen : NULL
                        );
                        xparam->outputs.returncode = 0;
                    }

                    return PHP_STREAM_OPTION_RETURN_OK;
                }
                case STREAM_XPORT_OP_GET_PEER_NAME: {
                    const cat_sockaddr_info_t *name = cat_socket_getpeername_fast(socket);

                    if (UNEXPECTED(name == NULL)) {
                        xparam->outputs.returncode = -1;
                    } else {
                        php_network_populate_name_from_sockaddr(
                            (struct sockaddr *) &name->address.common, name->length,
                            xparam->want_textaddr ? &xparam->outputs.textaddr : NULL,
                            xparam->want_addr ? &xparam->outputs.addr : NULL,
                            xparam->want_addr ? &xparam->outputs.addrlen : NULL
                        );
                        xparam->outputs.returncode = 0;
                    }

                    return PHP_STREAM_OPTION_RETURN_OK;
                }
                case STREAM_XPORT_OP_SEND: {
#if 0
                    int flags;
                    flags = 0;
                    if ((xparam->inputs.flags & STREAM_OOB) == STREAM_OOB) {
                        flags |= MSG_OOB;
                    }
#endif
                    xparam->outputs.returncode = cat_socket_sendto(
                        socket, xparam->inputs.buf, xparam->inputs.buflen,
                        xparam->inputs.addr, xparam->inputs.addrlen
                    );
                    if (xparam->outputs.returncode == -1) {
                        php_error_docref(NULL, E_WARNING, "%s\n", cat_get_last_error_message());
                    }
                    return PHP_STREAM_OPTION_RETURN_OK;
                }
                case STREAM_XPORT_OP_RECV: {
                    zend_bool peek;
#if 0
                    int flags;
                    flags = 0;
                    if ((xparam->inputs.flags & STREAM_OOB) == STREAM_OOB) {
                        flags |= MSG_OOB;
                    }
#endif
                    if ((xparam->inputs.flags & STREAM_PEEK) == STREAM_PEEK) {
                        peek = 1;
                    } else {
                        peek = 0;
                    }
                    xparam->outputs.returncode = swow_stream_socket_recvfrom(
                        socket, xparam->inputs.buf, xparam->inputs.buflen, peek,
                        xparam->want_textaddr ? &xparam->outputs.textaddr : NULL, xparam->want_addr ? &xparam->outputs.addr : NULL,
                        xparam->want_addr ? &xparam->outputs.addrlen : NULL
                    );
                    return PHP_STREAM_OPTION_RETURN_OK;
                }
#ifdef HAVE_SHUTDOWN
# ifndef SHUT_RD
#  define SHUT_RD 0
# endif
# ifndef SHUT_WR
#  define SHUT_WR 1
# endif
# ifndef SHUT_RDWR
#  define SHUT_RDWR 2
# endif
                case STREAM_XPORT_OP_SHUTDOWN: {
                    static const int shutdown_how[] = { SHUT_RD, SHUT_WR, SHUT_RDWR };

                    xparam->outputs.returncode = shutdown(sock->socket, shutdown_how[xparam->how]);

                    return PHP_STREAM_OPTION_RETURN_OK;
                }
#endif
                default:
                    return PHP_STREAM_OPTION_RETURN_NOTIMPL;
            }
            break;
        default:
            return PHP_STREAM_OPTION_RETURN_NOTIMPL;
    }
}

static int swow_stream_set_tcp_option(php_stream *stream, int option, int value, void *ptrparam)
{
    SWOW_STREAM_MEMBERS(stream, swow_sock, sock, socket);
    php_stream_xport_param *xparam;

    switch (option) {
        case PHP_STREAM_OPTION_XPORT_API:
            xparam = (php_stream_xport_param *) ptrparam;
            switch (xparam->op) {
                case STREAM_XPORT_OP_CONNECT:
                case STREAM_XPORT_OP_CONNECT_ASYNC: {
                    xparam->outputs.returncode = swow_stream_connect(stream, swow_sock, xparam);
                    return PHP_STREAM_OPTION_RETURN_OK;
                }
                case STREAM_XPORT_OP_BIND: {
                    xparam->outputs.returncode = swow_stream_bind(stream, swow_sock, xparam);
                    return PHP_STREAM_OPTION_RETURN_OK;
                }
                case STREAM_XPORT_OP_ACCEPT: {
                    xparam->outputs.returncode = swow_stream_accept(stream, swow_sock, xparam STREAMS_CC);
                    return PHP_STREAM_OPTION_RETURN_OK;
                }
                default:
                    break;
            }
    }

    return swow_stream_set_option(stream, option, value, ptrparam);
}

static bytes_t swow_stream_read(php_stream *stream, char *buffer, size_t size)
{
    SWOW_STREAM_SOCKET_GETTER(stream, swow_sock, sock, socket, return 0);
    ssize_t nr_bytes = 0;

    nr_bytes = cat_socket_recv_ex(socket, buffer, size, cat_time_tv2to(&sock->timeout));

    if (EXPECTED(nr_bytes > 0)) {
        php_stream_notify_progress_increment(PHP_STREAM_CONTEXT(stream), nr_bytes, 0);
    }
    else if (nr_bytes < 0) {
        cat_errno_t error =  cat_get_last_error_code();
        stream->eof = cat_socket_is_eof_error(error);
        sock->timeout_event = (error == CAT_ETIMEDOUT);
        nr_bytes = PHP_STREAM_SOCKET_RETURN_ERR;
    } else {
        stream->eof = 1;
    }

    return nr_bytes;
}

static bytes_t swow_stream_write(php_stream *stream, const char *buffer, size_t length)
{
    SWOW_STREAM_SOCKET_GETTER(stream, swow_sock, sock, socket, return 0);
    cat_bool_t ret;

    ret = cat_socket_send_ex(socket, buffer, length, cat_time_tv2to(&sock->timeout));

    if (UNEXPECTED(!ret)) {
        cat_errno_t error =  cat_get_last_error_code();
        php_error_docref(
            NULL, E_NOTICE, "Send of " ZEND_LONG_FMT " bytes failed with errno=%d %s",
            (zend_long) length, error, cat_get_last_error_message()
        );
        sock->timeout_event = (error == CAT_ETIMEDOUT);
        return PHP_STREAM_SOCKET_RETURN_ERR;
    }

    php_stream_notify_progress_increment(PHP_STREAM_CONTEXT(stream), length, 0);

    return length;
}

static int swow_stream_close(php_stream *stream, int close_handle)
{
    SWOW_STREAM_MEMBERS(stream, swow_sock, sock, socket);

    if (!swow_sock) {
        return 0;
    }

    if (close_handle) {
#ifdef PHP_WIN32
        if (sock->socket < 0) {
            sock->socket = SOCK_ERR;
        }
#endif
        if (sock->socket != SOCK_ERR) {
            cat_socket_close(socket);
            sock->socket = SOCK_ERR;
        }
    }

    pefree(swow_sock, php_stream_is_persistent(stream));

    return 0;
}

/* useless methods */

static int swow_stream_flush(php_stream *stream)
{
    return 0;
}

static int swow_stream_cast(php_stream *stream, int castas, void **ret)
{
    php_netstream_data_t *sock = (php_netstream_data_t *) stream->abstract;

    if (!sock) {
        return FAILURE;
    }

    switch (castas) {
        case PHP_STREAM_AS_STDIO:
            if (ret) {
                *(FILE **) ret = fdopen(sock->socket, stream->mode);
                if (*ret)
                    return SUCCESS;
                return FAILURE;
            }
            return SUCCESS;
        case PHP_STREAM_AS_FD_FOR_SELECT:
        case PHP_STREAM_AS_FD:
        case PHP_STREAM_AS_SOCKETD:
            if (ret)
                *(php_socket_t *) ret = sock->socket;
            return SUCCESS;
        default:
            return FAILURE;
    }
}

static int swow_stream_stat(php_stream *stream, php_stream_statbuf *ssb)
{
#if ZEND_WIN32
    return 0;
#else
    php_netstream_data_t *sock = (php_netstream_data_t *) stream->abstract;

    return zend_fstat(sock->socket, &ssb->sb);
#endif
}

/* These may look identical, but we need them this way so that
 * we can determine which type of socket we are dealing with
 * by inspecting stream->ops.
 * A "useful" side-effect is that the user's scripts can then
 * make similar decisions using stream_get_meta_data.
 * */

SWOW_API const php_stream_ops swow_stream_generic_socket_ops = {
    swow_stream_write, swow_stream_read,
    swow_stream_close, swow_stream_flush,
    "generic_socket",
    NULL, /* seek */
    swow_stream_cast,
    swow_stream_stat,
    swow_stream_set_option,
};

SWOW_API const php_stream_ops swow_stream_tcp_socket_ops = {
    swow_stream_write, swow_stream_read,
    swow_stream_close, swow_stream_flush,
    "tcp_socket",
    NULL, /* seek */
    swow_stream_cast,
    swow_stream_stat,
    swow_stream_set_tcp_option,
};

SWOW_API const php_stream_ops swow_stream_udp_socket_ops = {
    swow_stream_write, swow_stream_read,
    swow_stream_close, swow_stream_flush,
    "udp_socket",
    NULL, /* seek */
    swow_stream_cast,
    swow_stream_stat,
    swow_stream_set_tcp_option,
};

SWOW_API const php_stream_ops swow_stream_pipe_socket_ops = {
    swow_stream_write, swow_stream_read,
    swow_stream_close, swow_stream_flush,
    "pipe_socket",
    NULL, /* seek */
    swow_stream_cast,
    swow_stream_stat,
    swow_stream_set_tcp_option,
};

#ifdef AF_UNIX
SWOW_API const php_stream_ops swow_stream_unix_socket_ops = {
    swow_stream_write, swow_stream_read,
    swow_stream_close, swow_stream_flush,
    "unix_socket",
    NULL, /* seek */
    swow_stream_cast,
    swow_stream_stat,
    swow_stream_set_tcp_option,
};

SWOW_API const php_stream_ops swow_stream_udg_socket_ops = {
    swow_stream_write, swow_stream_read,
    swow_stream_close, swow_stream_flush,
    "udg_socket",
    NULL, /* seek */
    swow_stream_cast,
    swow_stream_stat,
    swow_stream_set_tcp_option,
};
#endif

/* $Id: 547d98b81d674c48f04eab5c24aa065eba4838cc $ */

#define IS_TTY(fd) ((fd) == STDIN_FILENO || (fd) == STDOUT_FILENO || (fd) == STDERR_FILENO)
#define INVALID_TTY_SOCKET ((cat_socket_t *) -1)

/* beginning of struct, see main/streams/plain_wrapper.c line 111 */
typedef struct {
    FILE *file;
    int fd;
} php_stdio_stream_data;

static php_stream_ops swow_stream_stdio_raw_ops;
static cat_socket_t *swow_stream_tty_sockets[3];

static cat_socket_t *swow_stream_stdio_init(php_stream *stream)
{
    php_stdio_stream_data *data = (php_stdio_stream_data *) stream->abstract;
    cat_socket_fd_t fd = data->fd >= 0 ? data->fd : fileno(data->file);
    cat_socket_t *socket;

    if (!IS_TTY(fd)) {
        return NULL;
    }

    socket = swow_stream_tty_sockets[fd];

    if (unlikely(socket == INVALID_TTY_SOCKET)) {
        return NULL;
    }

    if (unlikely(socket == NULL)) {
        socket = cat_socket_create_ex(NULL, CAT_SOCKET_TYPE_TTY, fd);
        if (unlikely(socket == NULL)) {
            swow_stream_tty_sockets[fd] = INVALID_TTY_SOCKET;
            return NULL;
        }
        swow_stream_tty_sockets[fd] = socket;
    }

    return socket;
}

static bytes_t swow_stream_stdio_read(php_stream *stream, char *buffer, size_t size)
{
    cat_socket_t *socket = swow_stream_stdio_init(stream);

    if (socket == NULL) {
        return swow_stream_stdio_raw_ops.read(stream, buffer, size);
    }

    return cat_socket_recv(socket, buffer, size);
}

static bytes_t swow_stream_stdio_write(php_stream *stream, const char *buffer, size_t length)
{
    cat_socket_t *socket = swow_stream_stdio_init(stream);
    cat_bool_t ret;

    if (socket == NULL) {
        return swow_stream_stdio_raw_ops.write(stream, buffer, length);
    }

    ret = cat_socket_send(socket, buffer, length);

    if (unlikely(!ret)) {
        return -1;
    }

    return length;
}

static int swow_stream_stdio_close(php_stream *stream, int close_handle)
{
    return swow_stream_stdio_raw_ops.close(stream, close_handle);
}

static int swow_stream_stdio_flush(php_stream *stream)
{
    return swow_stream_stdio_raw_ops.flush(stream);
}

static int swow_stream_stdiop_seek(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset)
{
    return swow_stream_stdio_raw_ops.seek(stream, offset, whence, newoffset);
}

static int swow_stream_stdio_cast(php_stream *stream, int castas, void **ret)
{
    return swow_stream_stdio_raw_ops.cast(stream, castas, ret);
}

static int swow_stream_stdio_stat(php_stream *stream, php_stream_statbuf *ssb)
{
    return swow_stream_stdio_raw_ops.stat(stream, ssb);
}

static int swow_stream_stdio_set_option(php_stream *stream, int option, int value, void *ptrparam)
{
    return swow_stream_stdio_raw_ops.set_option(stream, option, value, ptrparam);
}

SWOW_API const php_stream_ops swow_stream_stdio_ops = {
    swow_stream_stdio_write, swow_stream_stdio_read,
    swow_stream_stdio_close, swow_stream_stdio_flush,
    "STDIO",
    swow_stream_stdiop_seek,
    swow_stream_stdio_cast,
    swow_stream_stdio_stat,
    swow_stream_stdio_set_option,
};

#undef INVALID_TTY_SOCKET
#undef IS_TTY

/* {{{ proto int|false stream_socket_sendto(resource stream, string data [, int flags [, string target_addr]])
   Send data to a socket stream.  If target_addr is specified it must be in dotted quad (or [ipv6]) format */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_swow_stream_socket_sendto, 0, 2, MAY_BE_LONG | MAY_BE_FALSE)
    ZEND_ARG_INFO(0, socket)
    ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address, IS_STRING, 0, "\"\"")
ZEND_END_ARG_INFO()

PHP_FUNCTION(swow_stream_socket_sendto)
{
    php_stream *stream;
    zval *zstream;
    zend_long flags = 0;
    char *data, *target_addr = NULL;
    size_t datalen, target_addr_len = 0;
    int port;
    cat_sockaddr_union_t sa;
    cat_socklen_t sl = 0;

    ZEND_PARSE_PARAMETERS_START(2, 4)
        Z_PARAM_RESOURCE(zstream)
        Z_PARAM_STRING(data, datalen)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(flags)
        Z_PARAM_STRING(target_addr, target_addr_len)
    ZEND_PARSE_PARAMETERS_END();
    php_stream_from_zval(stream, zstream);

    if (target_addr_len) {
        char *host;
        cat_bool_t ret;
        /* parse the address */
        sa.common.sa_family = AF_UNSPEC;
        sl = sizeof(sa);
        host = swow_stream_parse_ip_address_ex(target_addr, target_addr_len, &port, 0, NULL);
        if (host == NULL) {
            php_error_docref(NULL, E_WARNING, "Failed to parse `%s' into a valid network address", target_addr);
            RETURN_FALSE;
        }
        ret = cat_sockaddr_getbyname(&sa.common, &sl, host, strlen(host), port);
        efree(host);
        if (unlikely(!ret)) {
            php_error_docref(NULL, E_WARNING, "Failed to parse `%s' into a valid network address, resaon: %s", target_addr, cat_get_last_error_message());
            RETURN_FALSE;
        }
    }

    RETURN_LONG(php_stream_xport_sendto(stream, data, datalen, (int)flags, target_addr_len ? &sa : NULL, sl));
}
/* }}} */

static const zend_function_entry swow_stream_functions[] = {
    PHP_FENTRY(stream_socket_sendto, PHP_FN(swow_stream_socket_sendto), arginfo_swow_stream_socket_sendto, 0)
    PHP_FE_END
};

int swow_stream_module_init(INIT_FUNC_ARGS)
{
    SWOW_MODULES_CHECK_PRE_START() {
        "openssl"
    } SWOW_MODULES_CHECK_PRE_END();

    if (php_stream_xport_register("tcp", swow_stream_socket_factory) != SUCCESS) {
        return FAILURE;
    }
    if (php_stream_xport_register("udp", swow_stream_socket_factory) != SUCCESS) {
        return FAILURE;
    }
    if (php_stream_xport_register("pipe", swow_stream_socket_factory) != SUCCESS) {
        return FAILURE;
    }
    if (php_stream_xport_register("unix", swow_stream_socket_factory) != SUCCESS) {
        return FAILURE;
    }
    if (php_stream_xport_register("udg", swow_stream_socket_factory) != SUCCESS) {
        return FAILURE;
    }
    if ("tty") {
        memcpy(&swow_stream_stdio_raw_ops, &php_stream_stdio_ops, sizeof(php_stream_stdio_ops));
        memcpy(&php_stream_stdio_ops, &swow_stream_stdio_ops, sizeof(php_stream_stdio_ops));
    }
    if (!swow_hook_internal_functions(swow_stream_functions)) {
        return FAILURE;
    }

    return SUCCESS;
}

int swow_stream_runtime_init(INIT_FUNC_ARGS)
{
    memset(swow_stream_tty_sockets, 0, sizeof(swow_stream_tty_sockets));

    return SUCCESS;
}

int swow_stream_runtime_shutdown(INIT_FUNC_ARGS)
{
    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(swow_stream_tty_sockets); i++) {
        cat_socket_t *socket = swow_stream_tty_sockets[i];
        if (socket != NULL) {
            cat_socket_close(socket);
        }
    }

    return SUCCESS;
}
