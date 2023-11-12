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
} swow_netstream_ssl_t;
#endif

typedef struct swow_netstream_data_s {
    php_netstream_data_t sock;
    cat_socket_t socket;
#ifdef CAT_SSL
    swow_netstream_ssl_t ssl;
#endif
} swow_netstream_data_t;

zend_result swow_stream_module_init(INIT_FUNC_ARGS);
zend_result swow_stream_runtime_init(INIT_FUNC_ARGS);
zend_result swow_stream_runtime_shutdown(INIT_FUNC_ARGS);
zend_result swow_stream_module_shutdown(INIT_FUNC_ARGS);

CAT_GLOBALS_STRUCT_BEGIN(swow_stream) {
    bool hooking_stdio_ops;
    bool hooking_tty;
    bool hooking_plain_wrapper;
    cat_socket_t *tty_sockets[3];
} CAT_GLOBALS_STRUCT_END(swow_stream);

extern CAT_GLOBALS_DECLARE(swow_stream);

#define SWOW_STREAM_G(x) CAT_GLOBALS_GET(swow_stream, x)

#ifdef __cplusplus
}
#endif
#endif /* SWOW_STREAM_H */
