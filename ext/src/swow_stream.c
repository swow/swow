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
#include "cat_poll.h" /* for select() */
#include "cat_ssl.h"

#include "php.h"
#include "php_network.h"
#include "streams/php_streams_int.h"
#include "ext/standard/file.h"
#include "ext/standard/url.h"
#include "ext/standard/php_fopen_wrappers.h"

#if defined(AF_UNIX)
# include <sys/un.h>
#endif

CAT_GLOBALS_DECLARE(swow_stream)

CAT_GLOBALS_CTOR_DECLARE_SZ(swow_stream)

/* $Id: f078bca729f4ab1bc2d60370e83bfa561f86b88d $ */

#ifdef CAT_SSL
typedef struct swow_netstream_ssl_s {
    zend_bool enable_on_connect;
    zend_bool is_client;
    struct timeval connect_timeout;
    php_stream_xport_crypt_method_t method;
    char *url_name;
} swow_netstream_ssl_t;
#endif

typedef struct swow_netstream_data_s {
    php_netstream_data_t sock;
#ifdef CAT_SSL
    swow_netstream_ssl_t ssl;
#endif
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

#ifdef CAT_SSL
/* Flags for determining allowed stream crypto methods */
# define STREAM_CRYPTO_IS_CLIENT            (1<<0)
# define STREAM_CRYPTO_METHOD_SSLv2         (1<<1)
# define STREAM_CRYPTO_METHOD_SSLv3         (1<<2)
# define STREAM_CRYPTO_METHOD_TLSv1_0       (1<<3)
# define STREAM_CRYPTO_METHOD_TLSv1_1       (1<<4)
# define STREAM_CRYPTO_METHOD_TLSv1_2       (1<<5)
# define STREAM_CRYPTO_METHOD_TLSv1_3       (1<<6)

# ifndef OPENSSL_NO_TLS1_METHOD
#  define HAVE_TLS1 1
# endif

# ifndef OPENSSL_NO_TLS1_1_METHOD
#  define HAVE_TLS11 1
# endif

# ifndef OPENSSL_NO_TLS1_2_METHOD
#  define HAVE_TLS12 1
# endif

# if OPENSSL_VERSION_NUMBER >= 0x10101000 && !defined(OPENSSL_NO_TLS1_3)
#  define HAVE_TLS13 1
# endif

# ifndef OPENSSL_NO_ECDH
#  define HAVE_ECDH 1
# endif

# ifndef OPENSSL_NO_TLSEXT
#  define HAVE_TLS_SNI 1
#  define HAVE_TLS_ALPN 1
# endif

# if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(LIBRESSL_VERSION_NUMBER)
#  define HAVE_SEC_LEVEL 1
# endif

# ifndef OPENSSL_NO_SSL3
#  define HAVE_SSL3 1
#  define PHP_OPENSSL_MIN_PROTO_VERSION STREAM_CRYPTO_METHOD_SSLv3
# else
#  define PHP_OPENSSL_MIN_PROTO_VERSION STREAM_CRYPTO_METHOD_TLSv1_0
# endif
# ifdef HAVE_TLS13
#  define PHP_OPENSSL_MAX_PROTO_VERSION STREAM_CRYPTO_METHOD_TLSv1_3
# else
#  define PHP_OPENSSL_MAX_PROTO_VERSION STREAM_CRYPTO_METHOD_TLSv1_2
# endif

/* Simplify ssl context option retrieval */
# define GET_VER_OPT(name) \
    (PHP_STREAM_CONTEXT(stream) && \
    (val = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "ssl", name)) != NULL)

# define GET_VER_OPT_STRING(name, str) \
    do { \
        if (GET_VER_OPT(name)) { \
            if (try_convert_to_string(val)) { \
                str = Z_STRVAL_P(val); \
            } \
        } \
    } while (0)

# define GET_VER_OPT_LONG(name, num) \
    do { \
        if (GET_VER_OPT(name)) { \
            num = zval_get_long(val); \
        } \
    } while (0)

static zend_long swow_stream_get_crypto_method(php_stream_context *ctx, zend_long crypto_method) /* {{{ */
{
    zval *val;

    if (ctx && (val = php_stream_context_get_option(ctx, "ssl", "crypto_method")) != NULL) {
        crypto_method = zval_get_long(val);
        crypto_method |= STREAM_CRYPTO_IS_CLIENT;
    }

    return crypto_method;
}

static char *swow_stream_ssl_get_url_name(const char *resourcename, size_t resourcenamelen, int is_persistent)
{
    php_url *url;

    if (!resourcename) {
        return NULL;
    }

    url = php_url_parse_ex(resourcename, resourcenamelen);
    if (!url) {
        return NULL;
    }

    if (url->host) {
        const char *host = ZSTR_VAL(url->host);
        char *url_name = NULL;
        size_t len = ZSTR_LEN(url->host);

        /* skip trailing dots */
        while (len && host[len - 1] == '.') {
            --len;
        }

        if (len) {
            url_name = pestrndup(host, len, is_persistent);
        }

        php_url_free(url);
        return url_name;
    }

    php_url_free(url);
    return NULL;
}
#endif

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

    /* alloc and init */
    swow_sock = (swow_netstream_data_t *) pecalloc(1, sizeof(*swow_sock), persistent_id ? 1 : 0);
    sock = &swow_sock->sock;
    sock->is_blocked = 1;
    sock->timeout.tv_sec = FG(default_socket_timeout);
    sock->timeout.tv_usec = 0;
    /* we don't know the socket until we have determined if we are binding or connecting */
    sock->socket = -1;
#ifdef CAT_SSL
    swow_sock->ssl.connect_timeout = sock->timeout;
#endif

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
#ifdef CAT_SSL
    else if (strncmp(proto, "ssl", protolen) == 0) {
        ops = &swow_stream_ssl_socket_ops;
        swow_sock->ssl.enable_on_connect = 1;
        swow_sock->ssl.method = swow_stream_get_crypto_method(context, STREAM_CRYPTO_METHOD_TLS_ANY_CLIENT);
    } else if (strncmp(proto, "sslv2", protolen) == 0) {
        php_error_docref(NULL, E_WARNING, "SSLv2 unavailable in this PHP version");
        goto _error;
    } else if (strncmp(proto, "sslv3", protolen) == 0) {
# ifdef HAVE_SSL3
        ops = &swow_stream_ssl_socket_ops;
        swow_sock->ssl.enable_on_connect = 1;
        swow_sock->ssl.method = STREAM_CRYPTO_METHOD_SSLv3_CLIENT;
# else
        php_error_docref(
            NULL, E_WARNING, "SSLv3 support is not compiled into the OpenSSL library against which PHP is linked");
        goto _error;
# endif
    } else if (strncmp(proto, "tls", protolen) == 0) {
        ops = &swow_stream_ssl_socket_ops;
        swow_sock->ssl.enable_on_connect = 1;
        swow_sock->ssl.method = swow_stream_get_crypto_method(context, STREAM_CRYPTO_METHOD_TLS_ANY_CLIENT);
        goto _error;
    } else if (strncmp(proto, "tlsv1.0", protolen) == 0) {
        ops = &swow_stream_ssl_socket_ops;
        swow_sock->ssl.enable_on_connect = 1;
        swow_sock->ssl.method = STREAM_CRYPTO_METHOD_TLSv1_0_CLIENT;
        goto _error;
    } else if (strncmp(proto, "tlsv1.1", protolen) == 0) {
# ifdef HAVE_TLS11
        ops = &swow_stream_ssl_socket_ops;
        swow_sock->ssl.enable_on_connect = 1;
        swow_sock->ssl.method = STREAM_CRYPTO_METHOD_TLSv1_1_CLIENT;
# else
        php_error_docref(
            NULL, E_WARNING, "TLSv1.1 support is not compiled into the OpenSSL library against which PHP is linked");
        goto _error;
# endif
    } else if (strncmp(proto, "tlsv1.2", protolen) == 0) {
# ifdef HAVE_TLS12
        ops = &swow_stream_ssl_socket_ops;
        swow_sock->ssl.enable_on_connect = 1;
        swow_sock->ssl.method = STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT;
# else
        php_error_docref(
            NULL, E_WARNING, "TLSv1.2 support is not compiled into the OpenSSL library against which PHP is linked");
        goto _error;
# endif
    } else if (strncmp(proto, "tlsv1.3", protolen) == 0) {
# ifdef HAVE_TLS13
        ops = &swow_stream_ssl_socket_ops;
        swow_sock->ssl.enable_on_connect = 1;
        swow_sock->ssl.method = STREAM_CRYPTO_METHOD_TLSv1_3_CLIENT;
# else
        php_error_docref(
            NULL, E_WARNING, "TLSv1.3 support is not compiled into the OpenSSL library against which PHP is linked");
        goto _error;
# endif
    }
#endif
    else {
        ops = NULL;
        CAT_NEVER_HERE("Unknown protocol");
    }

    /* alloc php_stream (php_stream_ops * is not const on PHP-7.x) */
    stream = php_stream_alloc_rel((php_stream_ops *) ops, sock, persistent_id, "r+");
    if (UNEXPECTED(stream == NULL)) {
        goto _error;
    }
    stream->abstract = (swow_netstream_data_t *) swow_sock;
#ifdef CAT_SSL
    swow_sock->ssl.url_name = swow_stream_ssl_get_url_name(resourcename, resourcenamelen, persistent_id != NULL);
#endif

    return stream;

    _error:
    pefree(swow_sock, persistent_id != NULL);
    return NULL;
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
#endif // PHP_WIN32
    } else /* swow_stream_generic_socket_ops */ {
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

#ifdef AF_UNIX
// from main/streams/xp_socket.c:parse_unix_address @ 3e01f5afb1b52fe26a956190296de0192eedeec1
// should we remove this in the future?
// TODO: should we remove this in the future?
static inline void swow_stream_check_unix_path_len(size_t *len)
{
    const size_t max_path = sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path);
    if (*len >= max_path) {
        *len = max_path - 1;
        php_error_docref(NULL, E_NOTICE,
            "socket path exceeded the maximum allowed length of %lu bytes "
            "and was truncated", max_path);
    }
}
#else
# define swow_stream_check_unix_path_len(len)
#endif

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

        if (
            PHP_STREAM_CONTEXT(stream) &&
            (tmpzval = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "ipv6_v6only")) != NULL &&
            Z_TYPE_P(tmpzval) != IS_NULL &&
            zend_is_true(tmpzval)
        ) {
            bind_flags |= CAT_SOCKET_BIND_FLAG_IPV6ONLY;
        }

        if (
            PHP_STREAM_CONTEXT(stream) &&
            (tmpzval = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "so_reuseport")) != NULL &&
            zend_is_true(tmpzval)
        ) {
            bind_flags |= CAT_SOCKET_BIND_FLAG_REUSEPORT;
        }
    } else {
        swow_stream_check_unix_path_len(&xparam->inputs.namelen);
        host = xparam->inputs.name;
        host_len = xparam->inputs.namelen;
    }

    socket = cat_socket_create(socket, type);

    if (UNEXPECTED(socket == NULL)) {
        goto _error;
    }

    ret = cat_socket_bind_ex(socket, host, host_len, port, bind_flags) ? 0 : -1;

    if (UNEXPECTED(ret != 0)) {
        goto _error;
    }

    swow_sock->sock.socket = cat_socket_get_fd(socket);

    if (
        stream->ops == &swow_stream_udp_socket_ops && /* SO_BROADCAST is only applicable for UDP */
        PHP_STREAM_CONTEXT(stream) &&
        (tmpzval = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "so_broadcast")) != NULL &&
        zend_is_true(tmpzval)
    ) {
        (void) cat_socket_set_udp_broadcast(socket, cat_true);
    }

    if (0) {
        _error:
        if (socket != NULL) {
            if (cat_socket_is_available(socket)) {
                cat_socket_close(socket);
            }
            socket = NULL;
        }
        xparam->outputs.error_code = cat_orig_errno(cat_get_last_error_code());
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
    zend_bool tcp_nodelay = 1;
    zval *tmpzval = NULL;
    cat_bool_t ret;

    client_sock = (swow_netstream_data_t *) ecalloc(1, sizeof(*client_sock));

    server = &server_sock->socket;
    client = &client_sock->socket;

    xparam->outputs.client = NULL;

    if ((NULL != PHP_STREAM_CONTEXT(stream)) &&
        (tmpzval = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "tcp_nodelay")) != NULL &&
        !zend_is_true(tmpzval)) {
        tcp_nodelay = 0;
    }

    client = cat_socket_create(client, cat_socket_get_simple_type(server));

    if (UNEXPECTED(client == NULL)) {
        goto _creation_error;
    }

    ret = cat_socket_accept_ex(server, client, cat_time_tv2to(xparam->inputs.timeout));

    if (UNEXPECTED(!ret)) {
        goto _accept_error;
    }

    address_info = cat_socket_getsockname_fast(client);

    if (UNEXPECTED(address_info == NULL)) {
        goto _accept_error;
    }

    php_network_populate_name_from_sockaddr(
        (struct sockaddr *) &address_info->address.common, address_info->length,
        xparam->want_textaddr ? &xparam->outputs.textaddr : NULL,
        xparam->want_addr ? &xparam->outputs.addr : NULL,
        xparam->want_addr ? &xparam->outputs.addrlen : NULL
    );

    if (!tcp_nodelay) {
        (void) cat_socket_set_tcp_nodelay(client, cat_false);
    }

    memcpy(&client_sock->sock, &server_sock->sock, sizeof(client_sock->sock));
    client_sock->sock.socket = cat_socket_get_fd(client);

    xparam->outputs.client = php_stream_alloc_rel(stream->ops, &client_sock->sock, NULL, "r+");
    if (xparam->outputs.client) {
        xparam->outputs.client->ctx = stream->ctx;
        if (stream->ctx) {
            GC_ADDREF(stream->ctx);
        }
    }

#ifdef CAT_SSL
    if (xparam->outputs.client && server_sock->ssl.enable_on_connect) {
        /* remove the client bit */
        server_sock->ssl.method &= ~STREAM_CRYPTO_IS_CLIENT;
        client_sock->ssl.method = server_sock->ssl.method;
        if (php_stream_xport_crypto_setup(xparam->outputs.client, client_sock->ssl.method, NULL) < 0 ||
            php_stream_xport_crypto_enable(xparam->outputs.client, 1) < 0
        ) {
            php_error_docref(NULL, E_WARNING, "Failed to enable crypto");
            php_stream_close(xparam->outputs.client);
            xparam->outputs.client = NULL;
            xparam->outputs.returncode = -1;
            return -1;
        }
    }
#endif

    return 0;

    _accept_error:
    cat_socket_close(client);
    _creation_error:
    efree(client_sock);
    xparam->outputs.error_code = cat_orig_errno(cat_get_last_error_code());
    if (xparam->want_errortext) {
        xparam->outputs.error_text = strpprintf(0, "%s", cat_get_last_error_message());
    }
    return -1;
}

static zend_always_inline zend_bool swow_stream_socket_connect(cat_socket_t *socket, const char *name, size_t name_length, int port, const struct timeval *timeout, zend_bool asynchronous)
{
    if (!asynchronous) {
        return cat_socket_connect_ex(socket, name, name_length, port, cat_time_tv2to(timeout));
    } else {
        return cat_socket_try_connect(socket, name, name_length, port);
    }
}

static int swow_stream_connect(php_stream *stream, swow_netstream_data_t *swow_sock, php_stream_xport_param *xparam, zend_bool asynchronous)
{
    cat_socket_type_t type = swow_stream_parse_socket_type(stream->ops);
    cat_socket_t *socket = &swow_sock->socket;
    char *host = NULL, *bind_host = NULL;
    int port, bind_port = 0;
    zval *ztmp = NULL;

    if (type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
        socket = cat_socket_create(socket, type);

        if (UNEXPECTED(socket == NULL)) {
            goto _error;
        }

        swow_stream_check_unix_path_len(&xparam->inputs.namelen);

        if (UNEXPECTED(!swow_stream_socket_connect(socket, xparam->inputs.name, xparam->inputs.namelen, 0, xparam->inputs.timeout, asynchronous))) {
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
    if (UNEXPECTED(!swow_stream_socket_connect(socket, host, strlen(host), port, xparam->inputs.timeout, asynchronous))) {
        goto _error;
    }

    if (stream->ops == &swow_stream_udp_socket_ops && /* SO_BROADCAST is only applicable for UDP */
        PHP_STREAM_CONTEXT(stream) &&
        (ztmp = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "so_broadcast")) != NULL &&
        zend_is_true(ztmp)
    ) {
        (void) cat_socket_set_udp_broadcast(socket, cat_true);
    }
    if (
        stream->ops == &swow_stream_tcp_socket_ops && /* TCP_NODELAY is only applicable for TCP */
        PHP_STREAM_CONTEXT(stream)  &&
        (ztmp = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream), "socket", "tcp_nodelay")) != NULL &&
        !zend_is_true(ztmp)
    ) {
        (void) cat_socket_set_tcp_nodelay(socket, cat_false);
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
        xparam->outputs.error_code = cat_orig_errno(cat_get_last_error_code());
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
    cat_socket_t *socket, char *buf, size_t buflen, int flags,
    zend_string **textaddr,
    struct sockaddr **addr, socklen_t *addrlen)
{
    int ret;
    int want_addr = textaddr || addr;

#if 0 /* TODO: support MSG_OOB */
    if (flags & STREAM_OOB) {
        // ...
    }
#endif

    if (want_addr) {
        php_sockaddr_storage sa;
        socklen_t sl = sizeof(sa);
        if (!(flags & STREAM_PEEK)) {
            ret = cat_socket_recvfrom(socket, buf, buflen, (struct sockaddr *) &sa, &sl);
        } else {
            ret = cat_socket_peekfrom(socket, buf, buflen, (struct sockaddr *) &sa, &sl);
        }
#ifdef PHP_WIN32
        /* POSIX discards excess bytes without signalling failure; emulate this on Windows */
        if (ret == -1 && cat_get_last_error_code() == CAT_EMSGSIZE) {
            ret = (int) buflen;
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
        if (!(flags & STREAM_PEEK)) {
            ret = cat_socket_recv(socket, buf, buflen);
        } else {
            ret = cat_socket_peek(socket, buf, buflen);
        }
    }

    return ret;
}

#ifdef CAT_SSL
static int swow_stream_setup_crypto(php_stream *stream,
    swow_netstream_data_t *swow_sock, php_netstream_data_t *sock, cat_socket_t *socket,
    php_stream_xport_crypto_param *cparam)
{
    if (cat_socket_is_encrypted(socket)) {
        php_error_docref(NULL, E_WARNING, "SSL/TLS already set-up for this stream");
        return FAILURE;
    }
    // else if (!sock->is_blocked && cat_socket_has_crypto(socket)) {
    //     return SUCCESS;
    // }

    /* We need to do slightly different things based on client/server method
     * so lets remember which method was selected */
    swow_sock->ssl.is_client = cparam->inputs.method & STREAM_CRYPTO_IS_CLIENT;

    return SUCCESS;
}

static int swow_stream_enable_crypto(php_stream *stream,
    swow_netstream_data_t *swow_sock, php_netstream_data_t *sock, cat_socket_t *socket,
    php_stream_xport_crypto_param *cparam)
{
    zend_bool encrypted = cat_socket_is_encrypted(socket);

    if (cparam->inputs.activate && !encrypted) {
        cat_socket_crypto_options_t options;
        zend_bool is_client = swow_sock->ssl.is_client;
        zval *val;

        cat_socket_crypto_options_init(&options, is_client);
        if (GET_VER_OPT("verify_peer") && !zend_is_true(val)) {
            options.verify_peer = cat_false;
        }
        if (GET_VER_OPT("verify_peer_name") && !zend_is_true(val)) {
            options.verify_peer_name = cat_false;
        }
        if (GET_VER_OPT("allow_self_signed") && zend_is_true(val)) {
            options.allow_self_signed = cat_true;
        }
        GET_VER_OPT_LONG("verify_depth", options.verify_depth);
        GET_VER_OPT_STRING("cafile", options.ca_file);
        GET_VER_OPT_STRING("capath", options.ca_path);
        if (options.ca_file == NULL) {
            options.ca_file = zend_ini_string((char *) ZEND_STRL("openssl.cafile"), 0);
            options.ca_file = strlen(options.ca_file) != 0 ? options.ca_file : NULL;
            options.no_client_ca_list = cat_true;
        }
        GET_VER_OPT_STRING("passphrase", options.passphrase);
        GET_VER_OPT_STRING("local_cert", options.certificate);
        GET_VER_OPT_STRING("local_pk", options.certificate_key);
        if (GET_VER_OPT("no_ticket") && zend_is_true(val)) {
            options.no_ticket = cat_true;
        }
        if (!GET_VER_OPT("disable_compression") || zend_is_true(val)) {
            options.no_compression = cat_true;
        }
        GET_VER_OPT_STRING("peer_name", options.passphrase);
        if (is_client) {
            /* If SNI is explicitly disabled we're finished here */
            if (!GET_VER_OPT("SNI_enabled") || zend_is_true(val)) {
                GET_VER_OPT_STRING("peer_name", options.peer_name);
                if (options.peer_name == NULL) {
                    options.peer_name = swow_sock->ssl.url_name;
                }
            }
        }
        cat_timeout_t timeout = cat_time_tv2to(swow_sock->ssl.is_client ?
            &swow_sock->ssl.connect_timeout :
            &swow_sock->sock.timeout
        );
        return cat_socket_enable_crypto_ex(socket, &options, timeout) ? 1 : -1;
    } else if (!cparam->inputs.activate && encrypted) {
        /* deactivate - common for server/client */
        // cat_socket_disable_crypto(socket->internal->ssl);
        return 1;
    }
    return -1;
}
#endif

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
            int oldmode = sock->is_blocked;
            sock->is_blocked = value;
            return oldmode;
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
                    xparam->outputs.returncode = swow_stream_socket_recvfrom(
                        socket, xparam->inputs.buf, xparam->inputs.buflen, xparam->inputs.flags,
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
        case PHP_STREAM_OPTION_XPORT_API: {
            xparam = (php_stream_xport_param *) ptrparam;
            switch (xparam->op) {
                case STREAM_XPORT_OP_CONNECT:
                case STREAM_XPORT_OP_CONNECT_ASYNC: {
                    xparam->outputs.returncode = swow_stream_connect(stream, swow_sock, xparam, xparam->op == STREAM_XPORT_OP_CONNECT_ASYNC);
#ifdef CAT_SSL
                    if (swow_sock->ssl.enable_on_connect && xparam->outputs.returncode == 0) {
                        /* TODO: ssl non-blocking handshake support
                        || (xparam->op == STREAM_XPORT_OP_CONNECT_ASYNC &&
                            xparam->outputs.returncode == 1 && xparam->outputs.error_code == EINPROGRESS */
                        if (php_stream_xport_crypto_setup(stream, swow_sock->ssl.method, NULL) < 0 ||
                            php_stream_xport_crypto_enable(stream, 1) < 0) {
                            php_error_docref(NULL, E_WARNING, "Failed to enable crypto");
                            xparam->outputs.returncode = -1;
                        }
                    }
#endif
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
            break;
        }
#ifdef CAT_SSL
        case PHP_STREAM_OPTION_CRYPTO_API: {
            php_stream_xport_crypto_param *cparam = (php_stream_xport_crypto_param *) ptrparam;
            switch (cparam->op) {
                case STREAM_XPORT_CRYPTO_OP_SETUP:
                    cparam->outputs.returncode = swow_stream_setup_crypto(stream, swow_sock, sock, socket, cparam);
                    return PHP_STREAM_OPTION_RETURN_OK;
                case STREAM_XPORT_CRYPTO_OP_ENABLE:
                    cparam->outputs.returncode = swow_stream_enable_crypto(stream, swow_sock, sock, socket, cparam);
                    return PHP_STREAM_OPTION_RETURN_OK;
                default:
                    break;
            }
            break;
        }
#endif
    }

    return swow_stream_set_option(stream, option, value, ptrparam);
}

static ssize_t swow_stream_read(php_stream *stream, char *buffer, size_t size)
{
    SWOW_STREAM_SOCKET_GETTER(stream, swow_sock, sock, socket, return 0);
    ssize_t nr_bytes = 0;

#if 1 /* PHP_VERSION_ID < 80100 */
    sock->timeout_event = 0; // clear error state
#endif

    if (sock->is_blocked) {
        nr_bytes = cat_socket_recv_ex(socket, buffer, size, cat_time_tv2to(&sock->timeout));
    } else {
        nr_bytes = cat_socket_try_recv(socket, buffer, size);
        if (nr_bytes == CAT_EAGAIN) {
            return 0;
        }
    }

    if (EXPECTED(nr_bytes > 0)) {
        php_stream_notify_progress_increment(PHP_STREAM_CONTEXT(stream), nr_bytes, 0);
    } else if (nr_bytes < 0) {
        cat_errno_t error = cat_get_last_error_code();
        stream->eof = cat_socket_is_eof_error(error);
        sock->timeout_event = (error == CAT_ETIMEDOUT);
        if (sock->timeout_event) {
            nr_bytes = 0;
        } else {
            nr_bytes = -1;
        }
    } else {
        stream->eof = 1;
    }

    return nr_bytes;
}

static ssize_t swow_stream_write(php_stream *stream, const char *buffer, size_t length)
{
    SWOW_STREAM_SOCKET_GETTER(stream, swow_sock, sock, socket, return 0);
    ssize_t didwrite;
    cat_bool_t ret;

    if (sock->is_blocked) {
        ret = cat_socket_send_ex(socket, buffer, length, cat_time_tv2to(&sock->timeout));
        didwrite = length;
    } else {
        didwrite = cat_socket_try_send(socket, buffer, length);
        if (UNEXPECTED(didwrite == CAT_EAGAIN)) {
            /* EWOULDBLOCK/EAGAIN is not an error for a non-blocking stream.
             * Report zero byte wrote instead. */
            return 0;
        }
        ret = didwrite >= 0;
    }
    if (UNEXPECTED(!ret)) {
        cat_errno_t error = cat_get_last_error_code();
#ifdef PHP_STREAM_FLAG_SUPPRESS_ERRORS
        if (!(stream->flags & PHP_STREAM_FLAG_SUPPRESS_ERRORS))
#endif
        {
            php_error_docref(
                NULL, E_NOTICE, "Send of " ZEND_LONG_FMT " bytes failed with errno=%d %s",
                (zend_long) length, error, cat_get_last_error_message()
            );
        }
        sock->timeout_event = (error == CAT_ETIMEDOUT);
        return -1;
    }

    php_stream_notify_progress_increment(PHP_STREAM_CONTEXT(stream), didwrite, 0);

    return didwrite;
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

#ifdef CAT_SSL
    if (swow_sock->ssl.url_name != NULL) {
        pefree(swow_sock->ssl.url_name, php_stream_is_persistent(stream));
    }
#endif
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
    SWOW_STREAM_MEMBERS(stream, swow_sock, sock, socket);

    if (!sock) {
        return FAILURE;
    }

#ifdef CAT_SSL
    if (castas != PHP_STREAM_AS_FD_FOR_SELECT &&
        cat_socket_is_encrypted(socket)) {
        return FAILURE;
    }
#endif

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
            if (ret) {
                *(php_socket_t *) ret = sock->socket;
            }
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

#ifdef CAT_SSL
SWOW_API const php_stream_ops swow_stream_ssl_socket_ops = {
    swow_stream_write, swow_stream_read,
    swow_stream_close, swow_stream_flush,
    "tcp_socket/ssl",
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

static cat_socket_t *swow_stream_stdio_init(const php_stream *stream)
{
    const php_stdio_stream_data *data = (const php_stdio_stream_data *) stream->abstract;
    int fd = data->fd >= 0 ? data->fd : fileno(data->file);
    cat_socket_t *socket;

    if (!IS_TTY(fd)) {
        return NULL;
    }

    socket = SWOW_STREAM_G(tty_sockets)[fd];

    if (unlikely(socket == INVALID_TTY_SOCKET)) {
        return NULL;
    }

    if (unlikely(socket == NULL)) {
        /* convert int to SOCKET on Windows, and internal will parse it as int */
        socket = cat_socket_open_os_fd(NULL, CAT_SOCKET_TYPE_TTY, fd);
        if (unlikely(socket == NULL)) {
            SWOW_STREAM_G(tty_sockets)[fd] = INVALID_TTY_SOCKET;
            return NULL;
        }
        SWOW_STREAM_G(tty_sockets)[fd] = socket;
    }

    return socket;
}

// stream ops things
// original standard stream operators holder
extern SWOW_API php_stream_ops swow_stream_stdio_ops_sync; // in swow_fs.c
// our modified async stream operators
extern SWOW_API const php_stream_ops swow_stream_stdio_ops_async; // in swow_fs.c
// orginal plain wrapper holder
SWOW_API php_stream_wrapper swow_plain_files_wrapper_sync;
// our modified async stream wrapper
extern SWOW_API const php_stream_wrapper swow_plain_files_wrapper_async; // in swow_fs.c

// plain stream operators proxies
static ssize_t swow_stream_stdio_proxy_read(php_stream *stream, char *buffer, size_t size)
{
    cat_socket_t *socket;

    if (!SWOW_STREAM_G(hooking_stdio_ops)) {
        // we are not hooking stdio ops
        return swow_stream_stdio_ops_sync.read(stream, buffer, size);
    }
    if (NULL == (socket = swow_stream_stdio_init(stream))) {
        // this is not a socket
        return swow_stream_stdio_ops_async.read(stream, buffer, size);
    }

    return cat_socket_recv(socket, buffer, size);
}

static ssize_t swow_stream_stdio_proxy_write(php_stream *stream, const char *buffer, size_t length)
{
    cat_socket_t *socket = swow_stream_stdio_init(stream);
    cat_bool_t ret;

    if (!SWOW_STREAM_G(hooking_stdio_ops)) {
        // we are not hooking stdio ops
        return swow_stream_stdio_ops_sync.write(stream, buffer, length);
    }
    if (NULL == (socket = swow_stream_stdio_init(stream))) {
        // this is not a socket
        return swow_stream_stdio_ops_async.write(stream, buffer, length);
    }

    ret = cat_socket_send(socket, buffer, length);

    if (unlikely(!ret)) {
        return -1;
    }

    return length;
}

static int swow_stream_stdio_proxy_close(php_stream *stream, int close_handle)
{
    if (!SWOW_STREAM_G(hooking_stdio_ops)) {
        // we are not hooking stdio ops
        return swow_stream_stdio_ops_sync.close(stream, close_handle);
    }
    return swow_stream_stdio_ops_async.close(stream, close_handle);
}

static int swow_stream_stdio_proxy_flush(php_stream *stream)
{
    if (!SWOW_STREAM_G(hooking_stdio_ops)) {
        // we are not hooking stdio ops
        return swow_stream_stdio_ops_sync.flush(stream);
    }
    return swow_stream_stdio_ops_async.flush(stream);
}

static int swow_stream_stdio_proxy_seek(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset)
{
    if (!SWOW_STREAM_G(hooking_stdio_ops)) {
        // we are not hooking stdio ops
        return swow_stream_stdio_ops_sync.seek(stream, offset, whence, newoffset);
    }
    return swow_stream_stdio_ops_async.seek(stream, offset, whence, newoffset);
}

static int swow_stream_stdio_proxy_cast(php_stream *stream, int castas, void **ret)
{
    if (!SWOW_STREAM_G(hooking_stdio_ops)) {
        // we are not hooking stdio ops
        return swow_stream_stdio_ops_sync.cast(stream, castas, ret);
    }
    return swow_stream_stdio_ops_async.cast(stream, castas, ret);
}

static int swow_stream_stdio_proxy_stat(php_stream *stream, php_stream_statbuf *ssb)
{
    if (!SWOW_STREAM_G(hooking_stdio_ops)) {
        // we are not hooking stdio ops
        return swow_stream_stdio_ops_sync.stat(stream, ssb);
    }
    return swow_stream_stdio_ops_async.stat(stream, ssb);
}

static int swow_stream_stdio_proxy_set_option(php_stream *stream, int option, int value, void *ptrparam)
{
    if (!SWOW_STREAM_G(hooking_stdio_ops)) {
        // we are not hooking stdio ops
        return swow_stream_stdio_ops_sync.set_option(stream, option, value, ptrparam);
    }
    return swow_stream_stdio_ops_async.set_option(stream, option, value, ptrparam);
}

SWOW_API const php_stream_ops swow_stream_stdio_ops_proxy = {
    swow_stream_stdio_proxy_write, swow_stream_stdio_proxy_read,
    swow_stream_stdio_proxy_close, swow_stream_stdio_proxy_flush,
    "STDIO",
    swow_stream_stdio_proxy_seek,
    swow_stream_stdio_proxy_cast,
    swow_stream_stdio_proxy_stat,
    swow_stream_stdio_proxy_set_option,
};

// plain wrapper proxies
static php_stream* swow_plain_files_proxy_stream_opener(
    php_stream_wrapper *wrapper, const char *filename, const char *mode,
    int options, zend_string **opened_path, php_stream_context *context STREAMS_DC) {
    if (!SWOW_STREAM_G(hooking_plain_wrapper)) {
        // we are not hooking plain wrapper ops
        return swow_plain_files_wrapper_sync.wops->stream_opener(
            wrapper, filename, mode, options, opened_path, context STREAMS_REL_CC
        );
    }
    return swow_plain_files_wrapper_async.wops->stream_opener(
        wrapper, filename, mode, options, opened_path, context STREAMS_REL_CC
    );
}

/*
// these two proxies is not used
static int swow_plain_files_proxy_stream_closer(php_stream_wrapper *wrapper, php_stream *stream) {
    if (!SWOW_STREAM_G(hooking_plain_wrapper)) {
        // we are not hooking plain wrapper ops
        return swow_plain_files_wrapper_sync.wops->stream_closer(wrapper, stream);
    }
    return swow_plain_files_wrapper_async.wops->stream_closer(wrapper, stream);
}

static int swow_plain_files_proxy_stream_stat(php_stream_wrapper *wrapper, php_stream *stream,
    php_stream_statbuf *ssb) {
    if (!SWOW_STREAM_G(hooking_plain_wrapper)) {
        // we are not hooking plain wrapper ops
        return swow_plain_files_wrapper_sync.wops->stream_stat(wrapper, stream, ssb);
    }
    return swow_plain_files_wrapper_async.wops->stream_stat(wrapper, stream, ssb);
}
*/

static int swow_plain_files_proxy_url_stat(php_stream_wrapper *wrapper,
    const char *url, int flags, php_stream_statbuf *ssb, php_stream_context *context) {
    if (!SWOW_STREAM_G(hooking_plain_wrapper)) {
        // we are not hooking plain wrapper ops
        return swow_plain_files_wrapper_sync.wops->url_stat(wrapper, url, flags, ssb, context);
    }
    return swow_plain_files_wrapper_async.wops->url_stat(wrapper, url, flags, ssb, context);
}

static php_stream *swow_plain_files_proxy_dir_opener(
    php_stream_wrapper *wrapper, const char *filename, const char *mode,
    int options, zend_string **opened_path, php_stream_context *context STREAMS_DC) {
    if (!SWOW_STREAM_G(hooking_plain_wrapper)) {
        // we are not hooking plain wrapper ops
        return swow_plain_files_wrapper_sync.wops->dir_opener(
            wrapper, filename, mode, options, opened_path, context STREAMS_REL_CC);
    }
    return swow_plain_files_wrapper_async.wops->dir_opener(
        wrapper, filename, mode, options, opened_path, context STREAMS_REL_CC);
}

static int swow_plain_files_proxy_unlink(php_stream_wrapper *wrapper, const char *url, int options,
    php_stream_context *context) {
    if (!SWOW_STREAM_G(hooking_plain_wrapper)) {
        // we are not hooking plain wrapper ops
        return swow_plain_files_wrapper_sync.wops->unlink(wrapper, url, options, context);
    }
    return swow_plain_files_wrapper_async.wops->unlink(wrapper, url, options, context);
}

static int swow_plain_files_proxy_rename(php_stream_wrapper *wrapper, const char *url_from,
    const char *url_to, int options, php_stream_context *context) {
    if (!SWOW_STREAM_G(hooking_plain_wrapper)) {
        // we are not hooking plain wrapper ops
        return swow_plain_files_wrapper_sync.wops->rename(wrapper, url_from, url_to, options, context);
    }
    return swow_plain_files_wrapper_async.wops->rename(wrapper, url_from, url_to, options, context);
}

static int swow_plain_files_proxy_stream_mkdir(php_stream_wrapper *wrapper, const char *url,
    int mode, int options, php_stream_context *context) {
    if (!SWOW_STREAM_G(hooking_plain_wrapper)) {
        // we are not hooking plain wrapper ops
        return swow_plain_files_wrapper_sync.wops->stream_mkdir(wrapper, url, mode, options, context);
    }
    return swow_plain_files_wrapper_async.wops->stream_mkdir(wrapper, url, mode, options, context);
}
static int swow_plain_files_proxy_stream_rmdir(php_stream_wrapper *wrapper, const char *url,
    int options, php_stream_context *context) {
    if (!SWOW_STREAM_G(hooking_plain_wrapper)) {
        // we are not hooking plain wrapper ops
        return swow_plain_files_wrapper_sync.wops->stream_rmdir(wrapper, url, options, context);
    }
    return swow_plain_files_wrapper_async.wops->stream_rmdir(wrapper, url, options, context);
}

static int swow_plain_files_proxy_stream_metadata(php_stream_wrapper *wrapper, const char *url,
    int options, void *value, php_stream_context *context) {
    if (!SWOW_STREAM_G(hooking_plain_wrapper)) {
        // we are not hooking plain wrapper ops
        return swow_plain_files_wrapper_sync.wops->stream_metadata(wrapper, url, options, value, context);
    }
    return swow_plain_files_wrapper_async.wops->stream_metadata(wrapper, url, options, value, context);
}

static php_stream_wrapper_ops swow_plain_files_wrapper_ops_proxy = {
    swow_plain_files_proxy_stream_opener,
    NULL,
    NULL,
    swow_plain_files_proxy_url_stat,
    swow_plain_files_proxy_dir_opener,
    "plainfile",
    swow_plain_files_proxy_unlink,
    swow_plain_files_proxy_rename,
    swow_plain_files_proxy_stream_mkdir,
    swow_plain_files_proxy_stream_rmdir,
    swow_plain_files_proxy_stream_metadata
};

static php_stream_wrapper swow_plain_files_wrapper_proxy = {
    &swow_plain_files_wrapper_ops_proxy,
    NULL,
    0
};

/* {{{ proto int|false stream_socket_sendto(resource stream, string data [, int flags [, string target_addr]])
   Send data to a socket stream.  If target_addr is specified it must be in dotted quad (or [ipv6]) format */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_swow_stream_socket_sendto, 0, 2, MAY_BE_LONG | MAY_BE_FALSE)
    ZEND_ARG_INFO(0, socket)
    ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address, IS_STRING, 0, "''")
ZEND_END_ARG_INFO()

static PHP_FUNCTION(swow_stream_socket_sendto)
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

    RETURN_LONG(php_stream_xport_sendto(stream, data, datalen, (int) flags, target_addr_len ? &sa : NULL, sl));
}
/* }}} */

/* {{{ stream_select related functions */
static int swow_stream_array_to_fd_set(zval *stream_array, fd_set *fds, php_socket_t *max_fd)
{
    zval *elem;
    php_stream *stream;
    int cnt = 0;

    if (Z_TYPE_P(stream_array) != IS_ARRAY) {
        return 0;
    }

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(stream_array), elem) {
        /* Temporary int fd is needed for the STREAM data type on windows, passing this_fd directly to php_stream_cast()
            would eventually bring a wrong result on x64. php_stream_cast() casts to int internally, and this will leave
            the higher bits of a SOCKET variable uninitialized on systems with little endian. */
        php_socket_t this_fd;

        ZVAL_DEREF(elem);
        php_stream_from_zval_no_verify(stream, elem);
        if (stream == NULL) {
            continue;
        }
        /* get the fd.
         * NB: Most other code will NOT use the PHP_STREAM_CAST_INTERNAL flag
         * when casting.  It is only used here so that the buffered data warning
         * is not displayed.
         * */
        if (
            SUCCESS == php_stream_cast(
                stream, PHP_STREAM_AS_FD_FOR_SELECT | PHP_STREAM_CAST_INTERNAL, (void *) &this_fd, 1) &&
            this_fd != -1
        ) {

            PHP_SAFE_FD_SET(this_fd, fds);

            if (this_fd > *max_fd) {
                *max_fd = this_fd;
            }
            cnt++;
        }
    } ZEND_HASH_FOREACH_END();
    return cnt ? 1 : 0;
}

static int swow_stream_array_from_fd_set(zval *stream_array, fd_set *fds)
{
    zval *elem, *dest_elem;
    HashTable *ht;
    php_stream *stream;
    int ret = 0;
    zend_string *key;
    zend_ulong num_ind;

    if (Z_TYPE_P(stream_array) != IS_ARRAY) {
        return 0;
    }
    ht = zend_new_array(zend_hash_num_elements(Z_ARRVAL_P(stream_array)));

    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(stream_array), num_ind, key, elem) {
        php_socket_t this_fd;

        ZVAL_DEREF(elem);
        php_stream_from_zval_no_verify(stream, elem);
        if (stream == NULL) {
            continue;
        }
        /* get the fd
         * NB: Most other code will NOT use the PHP_STREAM_CAST_INTERNAL flag
         * when casting.  It is only used here so that the buffered data warning
         * is not displayed.
         */
        if (
            SUCCESS ==
                php_stream_cast(
                    stream, PHP_STREAM_AS_FD_FOR_SELECT | PHP_STREAM_CAST_INTERNAL, (void *) &this_fd, 1) &&
            this_fd != SOCK_ERR
        ) {
            if (PHP_SAFE_FD_ISSET(this_fd, fds)) {
                if (!key) {
                    dest_elem = zend_hash_index_update(ht, num_ind, elem);
                } else {
                    dest_elem = zend_hash_update(ht, key, elem);
                }

                zval_add_ref(dest_elem);
                ret++;
                continue;
            }
        }
    } ZEND_HASH_FOREACH_END();

    /* destroy old array and add new one */
    zval_ptr_dtor(stream_array);
    ZVAL_ARR(stream_array, ht);

    return ret;
}

static int swow_stream_array_emulate_read_fd_set(zval *stream_array)
{
    zval *elem, *dest_elem;
    HashTable *ht;
    php_stream *stream;
    int ret = 0;
    zend_ulong num_ind;
    zend_string *key;

    if (Z_TYPE_P(stream_array) != IS_ARRAY) {
        return 0;
    }
    ht = zend_new_array(zend_hash_num_elements(Z_ARRVAL_P(stream_array)));

    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(stream_array), num_ind, key, elem) {
        ZVAL_DEREF(elem);
        php_stream_from_zval_no_verify(stream, elem);
        if (stream == NULL) {
            continue;
        }
        if ((stream->writepos - stream->readpos) > 0) {
            /* allow readable non-descriptor based streams to participate in stream_select.
             * Non-descriptor streams will only "work" if they have previously buffered the
             * data.  Not ideal, but better than nothing.
             * This branch of code also allows blocking streams with buffered data to
             * operate correctly in stream_select.
             * */
            if (!key) {
                dest_elem = zend_hash_index_update(ht, num_ind, elem);
            } else {
                dest_elem = zend_hash_update(ht, key, elem);
            }
            zval_add_ref(dest_elem);
            ret++;
            continue;
        }
    } ZEND_HASH_FOREACH_END();

    if (ret > 0) {
        /* destroy old array and add new one */
        zval_ptr_dtor(stream_array);
        ZVAL_ARR(stream_array, ht);
    } else {
        zend_array_destroy(ht);
    }

    return ret;
}
/* }}} */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_swow_stream_select, 0, 4, MAY_BE_LONG|MAY_BE_FALSE)
    ZEND_ARG_TYPE_INFO(1, read, IS_ARRAY, 1)
    ZEND_ARG_TYPE_INFO(1, write, IS_ARRAY, 1)
    ZEND_ARG_TYPE_INFO(1, except, IS_ARRAY, 1)
    ZEND_ARG_TYPE_INFO(0, seconds, IS_LONG, 1)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, microseconds, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

/* {{{ Runs the select() system call on the sets of streams with a timeout specified by tv_sec and tv_usec */
PHP_FUNCTION(swow_stream_select)
{
    zval *r_array, *w_array, *e_array;
    struct timeval tv, *tv_p = NULL;
    fd_set rfds, wfds, efds;
    php_socket_t max_fd = 0;
    int retval, sets = 0;
    zend_long sec, usec = 0;
    zend_bool secnull;
#if PHP_VERSION_ID >= 80100
    zend_bool usecnull = 1;
#endif // PHP_VERSION_ID >= 80100
    int set_count, max_set_count = 0;

    ZEND_PARSE_PARAMETERS_START(4, 5)
        Z_PARAM_ARRAY_EX2(r_array, 1, 1, 0)
        Z_PARAM_ARRAY_EX2(w_array, 1, 1, 0)
        Z_PARAM_ARRAY_EX2(e_array, 1, 1, 0)
        Z_PARAM_LONG_OR_NULL(sec, secnull)
        Z_PARAM_OPTIONAL
#if PHP_VERSION_ID < 80100
        Z_PARAM_LONG(usec)
#else
        Z_PARAM_LONG_OR_NULL(usec, usecnull)
#endif // PHP_VERSION_ID < 80100
    ZEND_PARSE_PARAMETERS_END();

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    if (r_array != NULL) {
        set_count = swow_stream_array_to_fd_set(r_array, &rfds, &max_fd);
        if (set_count > max_set_count)
            max_set_count = set_count;
        sets += set_count;
    }

    if (w_array != NULL) {
        set_count = swow_stream_array_to_fd_set(w_array, &wfds, &max_fd);
        if (set_count > max_set_count)
            max_set_count = set_count;
        sets += set_count;
    }

    if (e_array != NULL) {
        set_count = swow_stream_array_to_fd_set(e_array, &efds, &max_fd);
        if (set_count > max_set_count)
            max_set_count = set_count;
        sets += set_count;
    }

    if (!sets) {
        zend_value_error("No stream arrays were passed");
        RETURN_THROWS();
    }

    PHP_SAFE_MAX_FD(max_fd, max_set_count);

#if PHP_VERSION_ID >= 80100
    if (secnull && !usecnull) {
        if (usec == 0) {
            php_error_docref(NULL, E_DEPRECATED, "Argument #5 ($microseconds) should be null instead of 0 when argument #4 ($seconds) is null");
        } else {
            zend_argument_value_error(5, "must be null when argument #4 ($seconds) is null");
            RETURN_THROWS();
        }
    }
#endif // PHP_VERSION_ID >= 80100

    /* If seconds is not set to null, build the timeval, else we wait indefinitely */
    if (!secnull) {

        if (sec < 0) {
            zend_argument_value_error(4, "must be greater than or equal to 0");
            RETURN_THROWS();
        } else if (usec < 0) {
            // there's a bug before b751c24e233945281b08ef15b569a63feb6e0c48
            // here we fixed it
            zend_argument_value_error(5, "must be greater than or equal to 0");
            RETURN_THROWS();
        }

        /* Windows, Solaris and BSD do not like microsecond values which are >= 1 sec */
        tv.tv_sec = (long) (sec + (usec / 1000000));
        tv.tv_usec = (long) (usec % 1000000);
        tv_p = &tv;
    }

    /* slight hack to support buffered data; if there is data sitting in the
     * read buffer of any of the streams in the read array, let's pretend
     * that we selected, but return only the readable sockets */
    if (r_array != NULL) {
        retval = swow_stream_array_emulate_read_fd_set(r_array);
        if (retval > 0) {
            if (w_array != NULL) {
                zval_ptr_dtor(w_array);
                ZVAL_EMPTY_ARRAY(w_array);
            }
            if (e_array != NULL) {
                zval_ptr_dtor(e_array);
                ZVAL_EMPTY_ARRAY(e_array);
            }
            RETURN_LONG(retval);
        }
    }

    retval = cat_select(max_fd + 1, &rfds, &wfds, &efds, tv_p);

    if (retval == -1) {
        php_error_docref(NULL, E_WARNING, "Unable to select [%d]: %s (max_fd=%d)",
            errno, strerror(errno), max_fd);
        RETURN_FALSE;
    }

    if (r_array != NULL) {
        swow_stream_array_from_fd_set(r_array, &rfds);
    }
    if (w_array != NULL) {
        swow_stream_array_from_fd_set(w_array, &wfds);
    }
    if (e_array != NULL) {
        swow_stream_array_from_fd_set(e_array, &efds);
    }

    RETURN_LONG(retval);
}
/* }}} */

static zif_handler PHP_FN(original_socket_export_stream) = (zif_handler) -1;

/* hook for socket_export_stream */
static PHP_FUNCTION(swow_socket_export_stream)
{
    PHP_FN(original_socket_export_stream)(execute_data, return_value);
    if (Z_TYPE_P(return_value) != IS_RESOURCE) {
        return;
    }

    php_stream *stream;
    php_stream_from_zval(stream, return_value);

    swow_netstream_data_t *data = (swow_netstream_data_t *) stream->abstract;
    php_socket_t sock = data->sock.socket;
    cat_socket_t *socket = &data->socket;
    int type, af = AF_UNSPEC;
    socklen_t option_len;

    option_len = sizeof(type);
    if (getsockopt(sock, SOL_SOCKET, SO_TYPE, (char *) &type, &option_len) != 0) {
        zval_ptr_dtor(return_value);
        RETURN_FALSE;
    }
#ifdef SO_DOMAIN
    option_len = sizeof(af);
    (void) getsockopt(sock, SOL_SOCKET, SO_DOMAIN, &af, &option_len);
#endif
    cat_socket_type_t socket_type = CAT_SOCKET_TYPE_ANY;
    // determine type by sock type and domain
    if (AF_INET == af || AF_INET6 == af || AF_UNSPEC == af) {
        if (SOCK_STREAM == type) {
            socket_type = CAT_SOCKET_TYPE_TCP;
        } else if (SOCK_DGRAM == type) {
            socket_type = CAT_SOCKET_TYPE_UDP;
        }
    }
#ifdef CAT_OS_UNIX_LIKE
    else if (af == AF_LOCAL) {
        if (SOCK_STREAM == type) {
            socket_type = CAT_SOCKET_TYPE_UNIX;
        } else if (SOCK_DGRAM == type) {
            socket_type = CAT_SOCKET_TYPE_UDG;
        }
    }
#endif
#ifdef CAT_OS_UNIX_LIKE
    if (socket_type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
        socket = cat_socket_open_os_fd(socket, socket_type, sock);
    } else
#endif
    {
        socket = cat_socket_open_os_socket(socket, socket_type, sock);
    }
    if (socket == NULL) {
        zval_ptr_dtor(return_value);
        RETURN_FALSE;
    }
}

static const zend_function_entry swow_stream_functions[] = {
    PHP_FENTRY(stream_socket_sendto, PHP_FN(swow_stream_socket_sendto), arginfo_swow_stream_socket_sendto, 0)
    PHP_FENTRY(stream_select, PHP_FN(swow_stream_select), arginfo_swow_stream_select, 0)
    PHP_FE_END
};

int swow_unhook_stream_wrapper(void); // in swow_stream_wrapper.c
int swow_rehook_stream_wrappers(void); // in swow_stream_wrapper.c

// stream initializer / finalizers
zend_result swow_stream_module_init(INIT_FUNC_ARGS)
{
#ifdef CAT_SSL
    SWOW_MODULES_CHECK_PRE_START() {
        "openssl"
    } SWOW_MODULES_CHECK_PRE_END();
#endif

    CAT_GLOBALS_REGISTER(swow_stream, CAT_GLOBALS_CTOR(swow_stream), NULL);

    if (!swow_hook_internal_functions(swow_stream_functions)) {
        return FAILURE;
    }

    if (php_stream_xport_register("tcp", swow_stream_socket_factory) != SUCCESS) {
        return FAILURE;
    }
    if (php_stream_xport_register("udp", swow_stream_socket_factory) != SUCCESS) {
        return FAILURE;
    }
    if (php_stream_xport_register("pipe", swow_stream_socket_factory) != SUCCESS) {
        return FAILURE;
    }
#ifdef AF_UNIX
    if (php_stream_xport_register("unix", swow_stream_socket_factory) != SUCCESS) {
        return FAILURE;
    }
    if (php_stream_xport_register("udg", swow_stream_socket_factory) != SUCCESS) {
        return FAILURE;
    }
#endif
#ifdef CAT_SSL
    php_stream_xport_register("ssl", swow_stream_socket_factory);
# ifndef OPENSSL_NO_SSL3
    php_stream_xport_register("sslv3", swow_stream_socket_factory);
# endif
    php_stream_xport_register("tls", swow_stream_socket_factory);
    php_stream_xport_register("tlsv1.0", swow_stream_socket_factory);
    php_stream_xport_register("tlsv1.1", swow_stream_socket_factory);
    php_stream_xport_register("tlsv1.2", swow_stream_socket_factory);
# if OPENSSL_VERSION_NUMBER >= 0x10101000
    php_stream_xport_register("tlsv1.3", swow_stream_socket_factory);
# endif
    php_register_url_stream_wrapper("https", &php_stream_http_wrapper);
    php_register_url_stream_wrapper("ftps", &php_stream_ftp_wrapper);
#endif

    // backup blocking stdio operators (for include/require)
    memcpy(&swow_stream_stdio_ops_sync, &php_stream_stdio_ops, sizeof(php_stream_stdio_ops));
    // hook std ops
    memcpy(&php_stream_stdio_ops, &swow_stream_stdio_ops_proxy, sizeof(php_stream_stdio_ops));
    // backup blocking plain files wrapper
    memcpy(&swow_plain_files_wrapper_sync, &php_plain_files_wrapper, sizeof(php_plain_files_wrapper));
    // hook plain wrapper
    memcpy(&php_plain_files_wrapper, &swow_plain_files_wrapper_proxy, sizeof(php_plain_files_wrapper));
    // prepare tty sockets
    memset(SWOW_STREAM_G(tty_sockets), 0, sizeof(SWOW_STREAM_G(tty_sockets)));

    if ("phar unhook") {
        swow_unhook_stream_wrapper();
    }

    return SUCCESS;
}

zend_result swow_stream_runtime_init(INIT_FUNC_ARGS)
{
    SWOW_STREAM_G(hooking_stdio_ops) = cat_true;
    SWOW_STREAM_G(hooking_plain_wrapper) = cat_true;

    if (PHP_FN(original_socket_export_stream) == (zif_handler) -1) {
        (void) swow_hook_internal_function_handler_ex(
            ZEND_STRL("socket_export_stream"),
            PHP_FN(swow_socket_export_stream),
            &PHP_FN(original_socket_export_stream)
        );
    }

    return SUCCESS;
}

zend_result swow_stream_runtime_shutdown(INIT_FUNC_ARGS)
{
    size_t i;

    SWOW_STREAM_G(hooking_plain_wrapper) = cat_false;
    SWOW_STREAM_G(hooking_stdio_ops) = cat_false;

    for (i = 0; i < CAT_ARRAY_SIZE(SWOW_STREAM_G(tty_sockets)); i++) {
        cat_socket_t *socket = SWOW_STREAM_G(tty_sockets)[i];
        if (socket != NULL && socket != INVALID_TTY_SOCKET) {
            cat_socket_close(socket);
        }
    }

    return SUCCESS;
}

zend_result swow_stream_module_shutdown(INIT_FUNC_ARGS)
{
    swow_rehook_stream_wrappers();

    // unhook std ops
    memcpy(&php_stream_stdio_ops, &swow_stream_stdio_ops_sync, sizeof(php_stream_stdio_ops));
    // unhook plain wrapper
    memcpy(&php_plain_files_wrapper, &swow_plain_files_wrapper_sync, sizeof(php_plain_files_wrapper));

    return SUCCESS;
}
