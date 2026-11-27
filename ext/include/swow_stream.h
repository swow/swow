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

#ifndef SWOW_STREAM_H
#define SWOW_STREAM_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"
#include "cat_socket.h"
#include "cat_ssl.h"

#include "php_network.h" /* for php_netstream_data_t */


#if defined(PHP_WIN32) || defined(__riscos__)
#undef AF_UNIX
#endif

extern SWOW_API const php_stream_ops swow_stream_generic_socket_ops;
extern SWOW_API const php_stream_ops swow_stream_tcp_socket_ops;
extern SWOW_API const php_stream_ops swow_stream_udp_socket_ops;
extern SWOW_API const php_stream_ops swow_stream_pipe_socket_ops;
#ifdef AF_UNIX
extern SWOW_API const php_stream_ops swow_stream_unix_socket_ops;
extern SWOW_API const php_stream_ops swow_stream_udg_socket_ops;
#endif
#ifdef CAT_SSL
extern SWOW_API const php_stream_ops swow_stream_ssl_socket_ops;
#endif

typedef struct swow_stream_ops_list_s {
    const php_stream_ops *generic_socket_ops;
    const php_stream_ops *tcp_socket_ops;
    const php_stream_ops *udp_socket_ops;
    const php_stream_ops *pipe_socket_ops;
#ifdef AF_UNIX
    const php_stream_ops *unix_socket_ops;
    const php_stream_ops *udg_socket_ops;
#endif
#ifdef CAT_SSL
    const php_stream_ops *ssl_socket_ops;
#endif
} swow_stream_ops_list_t;

extern SWOW_API const php_stream_ops *swow_stream_builtin_ops[sizeof(swow_stream_ops_list_t) / sizeof(php_stream_ops *)];
extern SWOW_API const size_t swow_stream_builtin_ops_count;

#ifdef CAT_SSL
typedef struct swow_netstream_ssl_s {
    bool enable_on_connect;
    bool is_client;
    struct timeval connect_timeout;
    php_stream_xport_crypt_method_t method;
    char *url_name;
    zend_long reneg_limit;
    zend_long reneg_window;
    cat_msec_t reneg_prev_handshake;
    double reneg_tokens;
    bool reneg_callback_pending;
} swow_netstream_ssl_t;
#endif

typedef struct swow_netstream_data_s {
    php_netstream_data_t sock;
    cat_socket_t socket;
    /* links the persistent stream into SWOW_STREAM_G(persistent_streams) */
    cat_queue_node_t node;
    bool persistent;
#ifdef CAT_SSL
    swow_netstream_ssl_t ssl;
#endif
} swow_netstream_data_t;

/* Wait on the event loop until a blocking read (POLLIN) or write (POLLOUT) on fd
 * will not block; returns 1 when ready (or in error), 0 on timeout, -1 when the
 * wait was cancelled or another coroutine already waits to read fd (CAT_ELOCKED) */
SWOW_API int swow_stream_wait_fd(php_socket_t fd, int events, cat_timeout_t timeout);
/* The same, but readers of fd that have no buffer of their own (ext/sockets) may wait together */
SWOW_API int swow_stream_wait_fd_ex(php_socket_t fd, int events, cat_timeout_t timeout, bool exclusive_reader);
/* Register a wait on fd made by other means (cat_poll()), so that its blocking mode, which
 * libuv turns off while it polls, is restored once the last wait on fd ends */
SWOW_API void swow_stream_fd_wait_begin(php_socket_t fd, bool reading);
SWOW_API void swow_stream_fd_wait_end(php_socket_t fd, bool reading);
/* The size that a send() on fd queues for sure after POLLOUT, 0 when the whole write does */
SWOW_API size_t swow_stream_fd_send_limit(php_socket_t fd);
/* Whether another coroutine waits to read fd: a read now must be refused */
SWOW_API bool swow_stream_fd_is_being_read(php_socket_t fd);
SWOW_API void swow_stream_fd_update_busy_error(void);
/* The blocking mode of fd, also while a wait holds it in O_NONBLOCK (libuv) */
SWOW_API bool swow_stream_fd_is_blocking(php_socket_t fd);

/* Let a stream that PHP opened around an existing socket wait on the event loop */
SWOW_API void swow_stream_native_socket_adopt(php_stream *stream);
SWOW_API void swow_stream_native_socket_adopt_zval(zval *zstream);

zend_result swow_stream_module_init(INIT_FUNC_ARGS);
zend_result swow_stream_runtime_init(INIT_FUNC_ARGS);
zend_result swow_stream_runtime_shutdown(INIT_FUNC_ARGS);
zend_result swow_stream_module_shutdown(INIT_FUNC_ARGS);

CAT_GLOBALS_STRUCT_BEGIN(swow_stream) {
    bool hooking_stdio_ops;
    bool hooking_tty;
    bool hooking_plain_wrapper;
    /* streams that PHP opens around an existing socket, see swow_stream_native_socket_adopt() */
    bool hooking_native_socket_ops;
    /* descriptors that coroutines wait for in swow_stream_wait_fd() */
    HashTable waiting_fds;
    cat_socket_t *tty_sockets[3];
    cat_queue_t persistent_streams;
} CAT_GLOBALS_STRUCT_END(swow_stream);

extern CAT_GLOBALS_DECLARE(swow_stream);

#define SWOW_STREAM_G(x) CAT_GLOBALS_GET(swow_stream, x)

#ifdef __cplusplus
}
#endif
#endif /* SWOW_STREAM_H */
