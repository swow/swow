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

#ifndef CAT_SOCKET_H
#define CAT_SOCKET_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#include "cat_coroutine.h"
#include "cat_time.h"
#include "cat_dns.h"
#include "cat_ssl.h"

#ifdef CAT_OS_UNIX_LIKE
#include <sys/socket.h>
#include <sys/uio.h> /* writev */
#include <sys/un.h>
#endif
#ifdef CAT_OS_WIN
#include <winsock2.h>
#endif

/* sockaddr */

#define CAT_SOCKET_DEFAULT_BACKLOG  128

#define CAT_SOCKET_IPV4_BUFFER_SIZE 16  /* >= 16 */
#define CAT_SOCKET_IPV6_BUFFER_SIZE 48  /* >= 46 */
#define CAT_SOCKET_IP_BUFFER_SIZE   CAT_SOCKET_IPV6_BUFFER_SIZE

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif
#ifdef CAT_OS_UNIX_LIKE
#define CAT_SOCKADDR_MAX_PATH (sizeof(((struct sockaddr_un *) 0)->sun_path))
#define CAT_SOCKLEN_FMT "%u"
typedef socklen_t cat_socklen_t; /* Notice: it's unsigned on drawin */
typedef sa_family_t cat_sa_family_t;
typedef in_port_t cat_in_port_t;
#else
#define CAT_SOCKADDR_MAX_PATH 260
#define CAT_SOCKLEN_FMT "%d"
typedef int cat_socklen_t;
typedef short cat_sa_family_t;
typedef unsigned short cat_in_port_t;
#endif

typedef struct sockaddr     cat_sockaddr_t;
typedef struct sockaddr_in  cat_sockaddr_in_t;
typedef struct sockaddr_in6 cat_sockaddr_in6_t;
typedef struct
{
#ifndef _MSC_VER
    char __sl_pad1[offsetof(cat_sockaddr_t, sa_family)];
#endif
    cat_sa_family_t sl_family;
#ifndef _MSC_VER
    char __sl_pad2[offsetof(cat_sockaddr_t, sa_data) - cat_offsize_of(cat_sockaddr_t, sa_family)];
#endif
    char sl_path[CAT_SOCKADDR_MAX_PATH];
} cat_sockaddr_local_t;
typedef struct sockaddr_storage cat_sockaddr_storage_t;

typedef union
{
    cat_sockaddr_t common;
    cat_sockaddr_in_t in;
    cat_sockaddr_in6_t in6;
} cat_sockaddr_inet_union_t;

typedef union
{
    cat_sockaddr_t common;
    cat_sockaddr_in_t in;
    cat_sockaddr_in6_t in6;
    cat_sockaddr_local_t local;
    cat_sockaddr_storage_t storage;
} cat_sockaddr_union_t;

typedef struct
{
    cat_socklen_t length;
    cat_sockaddr_inet_union_t address;
} cat_sockaddr_inet_info_t;

typedef struct
{
    cat_socklen_t length;
    cat_sockaddr_union_t address;
} cat_sockaddr_info_t;

CAT_API const char* cat_sockaddr_af_name(cat_sa_family_t af);

/* Notice: input buffer_size is size of buffer (not length)
 * and if ENOSPC, *buffer_size will be the minimum required buffer **size**, otherwise, *buffer_size will be strlen(buffer)
 * buffer is always zero-termination */
CAT_API cat_bool_t cat_sockaddr_get_address(const cat_sockaddr_t *address, cat_socklen_t address_length, char *buffer, size_t *buffer_size);
CAT_API int cat_sockaddr_get_port(const cat_sockaddr_t *address);
CAT_API cat_bool_t cat_sockaddr_set_port(cat_sockaddr_t *address, int port);

/* Notice: do not forget to init address->sa_family and address_length */
CAT_API cat_bool_t cat_sockaddr_getbyname(cat_sockaddr_t *address, cat_socklen_t *address_length, const char *name, size_t name_length, int port);
/* Notice: it can handle empty case internally, but address must be valid or empty */
CAT_API cat_bool_t cat_sockaddr_to_name(const cat_sockaddr_t *address, cat_socklen_t address_length, char *name, size_t *name_length, int *port);

/* Notice: it can handle empty "to" internally */
CAT_API int cat_sockaddr_copy(cat_sockaddr_t *to, cat_socklen_t *to_length, const cat_sockaddr_t *from, cat_socklen_t from_length);
CAT_API cat_bool_t cat_sockaddr_check(const cat_sockaddr_t *address, cat_socklen_t address_length);

/* socket fd */

#define CAT_SOCKET_FD_FMT "%d"
typedef uv_os_fd_t cat_socket_fd_t;

#ifndef CAT_OS_WIN
#define CAT_SOCKET_INVALID_FD -1
#else
#define CAT_SOCKET_INVALID_FD INVALID_SOCKET
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO     0 /* standard input file descriptor */
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO    1 /* standard output file descriptor */
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO    2 /* standard error file descriptor */
#endif

typedef union
{
    cat_socket_fd_t fd;
    uv_os_sock_t socket;
    uv_os_sock_t tcp;
    uv_os_sock_t udp;
    uv_file pipe;
    uv_file tty;
} cat_socket_fd_union_t;

/* socket vector */

#ifndef CAT_OS_WIN
typedef size_t cat_socket_vector_length_t;
#else
typedef ULONG cat_socket_vector_length_t;
#endif

#ifndef CAT_OS_WIN
/* Note: May be cast to struct iovec. See writev(2). */
typedef struct
{
    const char *base;
    cat_socket_vector_length_t length;
} cat_socket_write_vector_t;
#else
/**
 * It should be possible to cast uv_buf_t[] to WSABUF[]
 * see http://msdn.microsoft.com/en-us/library/ms741542(v=vs.85).aspx
 */
typedef struct {
    cat_socket_vector_length_t length;
    const char* base;
} cat_socket_write_vector_t;
#endif

static cat_always_inline cat_socket_write_vector_t cat_socket_write_vector_init(const char *base, cat_socket_vector_length_t length)
{
    cat_socket_write_vector_t vector;

    vector.base = base;
    vector.length = length;

    return vector;
}

CAT_API size_t cat_socket_write_vectors_length(const cat_socket_write_vector_t *vectors, unsigned int vector_count);

/* socket */

#ifdef CAT_OS_UNIX_LIKE
#define CAT_SOCKET_USE_REACTOR 1
#endif

#ifdef TCP_KEEPALIVE /* naming conflict */
#undef TCP_KEEPALIVE
#endif

/* 0 ~ 23 */
#define CAT_SOCKET_TYPE_FLAG_MAP_EX(XX, SS) \
    /* 0 - 3 (sock) */ \
    XX(STREAM, 1 << 0) \
    XX(DGRAM, 1 << 1) \
    SS(SSL,   1 << 2) \
    /* 4 ~ 9 (af) */ \
    XX(INET,  1 << 4) \
    XX(IPV4,  1 << 5) \
    XX(IPV6,  1 << 6) \
    XX(LOCAL, 1 << 7) \
    /* 10 ~ 19 ((tcp|udp)-extra) */ \
    XX(UNSPEC,        1 << 10)  CAT_INTERNAL /* (it is AF_UNSPEC when was created by user) */ \
    XX(TCP_DELAY,     1 << 11)  CAT_INTERNAL /* (disable tcp_nodelay) */ \
    XX(TCP_KEEPALIVE, 1 << 12)  CAT_INTERNAL /* (enable keep-alive) */ \
    XX(UDP_BROADCAST, 1 << 11)  CAT_INTERNAL /* (enable broadcast) TODO: support it or remove */ \
    /* 10 ~ 19 (pipe-extra) */ \
    XX(IPC,    1 << 10) \
    /* 10 ~ 19 (tty-extra) */ \
    XX(STDIN,  1 << 10) \
    XX(STDOUT, 1 << 11) \
    XX(STDERR, 1 << 12) \
    /* 20 ~ 23 (stream (tcp|pipe|tty)) */ \
    XX(SERVER, 1 << 20)         CAT_INTERNAL \
    XX(CLIENT, 1 << 21)         CAT_INTERNAL \

#define CAT_SOCKET_TYPE_FLAG_MAP(XX) CAT_SOCKET_TYPE_FLAG_MAP_EX(XX, CAT_SSL_ENUM_GEN(XX))

typedef enum
{
#define CAT_SOCKET_TYPE_FLAG_GEN(name, value) CAT_ENUM_GEN(CAT_SOCKET_TYPE_FLAG_, name, value)
    CAT_SOCKET_TYPE_FLAG_MAP(CAT_SOCKET_TYPE_FLAG_GEN)
#undef CAT_SOCKET_TYPE_FLAG_GEN
} cat_socket_type_flag_t;

#define CAT_SOCKET_TYPE_MAP_EX(XX, UN, SS) \
    XX(ANY,    0) \
    /* stream */ \
    XX(TCP,    1 << 24 | CAT_SOCKET_TYPE_FLAG_STREAM | CAT_SOCKET_TYPE_FLAG_INET) \
    XX(TCP4,   CAT_SOCKET_TYPE_TCP | CAT_SOCKET_TYPE_FLAG_IPV4) \
    XX(TCP6,   CAT_SOCKET_TYPE_TCP | CAT_SOCKET_TYPE_FLAG_IPV6) \
    XX(PIPE,   1 << 25 | CAT_SOCKET_TYPE_FLAG_STREAM | CAT_SOCKET_TYPE_FLAG_LOCAL) \
    XX(TTY,    1 << 26 | CAT_SOCKET_TYPE_FLAG_STREAM) \
    XX(STDIN,  CAT_SOCKET_TYPE_TTY | CAT_SOCKET_TYPE_FLAG_STDIN) \
    XX(STDOUT, CAT_SOCKET_TYPE_TTY | CAT_SOCKET_TYPE_FLAG_STDOUT) \
    XX(STDERR, CAT_SOCKET_TYPE_TTY | CAT_SOCKET_TYPE_FLAG_STDERR) \
    /* dgram */ \
    XX(UDP,    1 << 27 | CAT_SOCKET_TYPE_FLAG_DGRAM | CAT_SOCKET_TYPE_FLAG_INET) \
    XX(UDP4,   CAT_SOCKET_TYPE_UDP | CAT_SOCKET_TYPE_FLAG_IPV4) \
    XX(UDP6,   CAT_SOCKET_TYPE_UDP | CAT_SOCKET_TYPE_FLAG_IPV6) \
    UN(UNIX,   CAT_SOCKET_TYPE_PIPE) \
    UN(UDG,    1 << 28  | CAT_SOCKET_TYPE_FLAG_DGRAM | CAT_SOCKET_TYPE_FLAG_LOCAL) \
    SS(TLS,    CAT_SOCKET_TYPE_TCP | CAT_SOCKET_TYPE_FLAG_SSL) \
    SS(TLS4,   CAT_SOCKET_TYPE_TCP4 | CAT_SOCKET_TYPE_FLAG_SSL) \
    SS(TLS6,   CAT_SOCKET_TYPE_TCP6 | CAT_SOCKET_TYPE_FLAG_SSL) \
    SS(DTLS,   CAT_SOCKET_TYPE_UDP | CAT_SOCKET_TYPE_FLAG_SSL) \
    SS(DTLS4,  CAT_SOCKET_TYPE_UDP4 | CAT_SOCKET_TYPE_FLAG_SSL) \
    SS(DTLS6,  CAT_SOCKET_TYPE_UDP6 | CAT_SOCKET_TYPE_FLAG_SSL) \

#ifndef CAT_OS_UNIX_LIKE
#define CAT_SOCKET_UNIX_ENUM_GEN(XX) CAT_ENUM_EMPTY_GEN
#else
#define CAT_SOCKET_UNIX_ENUM_GEN(XX) XX
#endif
#define CAT_SOCKET_TYPE_MAP(XX) CAT_SOCKET_TYPE_MAP_EX(XX, CAT_SOCKET_UNIX_ENUM_GEN(XX), CAT_SSL_ENUM_GEN(XX))

/* 24 ~ 31 */
typedef enum
{
#define CAT_SOCKET_TYPE_GEN(name, value) CAT_ENUM_GEN(CAT_SOCKET_TYPE_, name, value)
    CAT_SOCKET_TYPE_MAP(CAT_SOCKET_TYPE_GEN)
#undef CAT_SOCKET_TYPE_GEN
} cat_socket_common_type_t;

typedef enum
{
    CAT_SOCKET_TYPE_FLAGS_DO_NOT_EXTENDS =
        /* CAT_SOCKET_TYPE_FLAG_SERVER | (server will be solved in create) */
        CAT_SOCKET_TYPE_FLAG_CLIENT,
} cat_socket_union_type_flas_t;

typedef uint32_t cat_socket_type_t;

#define CAT_SOCKET_IO_FLAG_MAP_EX(XX, SS) \
    XX(NONE,      0) \
    XX(READ,      1 << 0) \
    XX(WRITE,     1 << 1) \
    XX(RDWR,      CAT_SOCKET_IO_FLAG_READ | CAT_SOCKET_IO_FLAG_WRITE) \
    XX(BIND,      1 << 2 | CAT_SOCKET_IO_FLAG_RDWR) \
    XX(ACCEPT,    1 << 3 | CAT_SOCKET_IO_FLAG_RDWR) \
    XX(CONNECT,   1 << 4 | CAT_SOCKET_IO_FLAG_RDWR) \
    SS(HANDSHAKE, 1 << 5 | CAT_SOCKET_IO_FLAG_RDWR) \

#define CAT_SOCKET_IO_FLAG_MAP(XX) CAT_SOCKET_IO_FLAG_MAP_EX(XX, CAT_SSL_ENUM_GEN(XX))

typedef enum
{
#define CAT_SOCKET_IO_FLAG_GEN(name, value) CAT_ENUM_GEN(CAT_SOCKET_IO_FLAG_, name, value)
    CAT_SOCKET_IO_FLAG_MAP(CAT_SOCKET_IO_FLAG_GEN)
#undef CAT_SOCKET_IO_FLAG_GEN
} cat_socket_io_flag_t;

typedef uint8_t cat_socket_io_flags_t;

#define CAT_SOCKET_BIND_FLAG_MAP(XX) \
    XX(NONE, 0) \
    XX(IPV6ONLY, 1 << 0) \
    XX(REUSEADDR, 1 << 1) \
    XX(REUSEPORT, 1 << 2) \

typedef enum
{
#define CAT_SOCKET_BIND_FLAG_GEN(name, value) CAT_ENUM_GEN(CAT_SOCKET_BIND_FLAG_, name, value)
    CAT_SOCKET_BIND_FLAG_MAP(CAT_SOCKET_BIND_FLAG_GEN)
#undef CAT_SOCKET_BIND_FLAG_GEN
} cat_socket_bind_flag_t;

typedef uint8_t cat_socket_bind_flags_t;

typedef enum
{
    CAT_SOCKET_BIND_FLAGS_ALL = CAT_SOCKET_BIND_FLAG_IPV6ONLY | CAT_SOCKET_BIND_FLAG_REUSEADDR
} cat_socket_bind_union_flags_t;

typedef int32_t cat_socket_timeout_storage_t;
#define CAT_SOCKET_TIMEOUT_STORAGE_FMT     "%d"
#define CAT_SOCKET_TIMEOUT_STORAGE_MIN     CAT_TIMEOUT_FOREVER
#define CAT_SOCKET_TIMEOUT_STORAGE_MAX     INT32_MAX
#define CAT_SOCKET_TIMEOUT_STORAGE_DEFAULT -CAT_MAGIC_NUMBER

typedef struct
{
    cat_socket_timeout_storage_t dns;
    cat_socket_timeout_storage_t accept;
    cat_socket_timeout_storage_t connect;
    cat_socket_timeout_storage_t read;
    cat_socket_timeout_storage_t write;
} cat_socket_timeout_options_t;

typedef struct
{
    union {
        void *ptr;
        int status;
    } data;
    cat_coroutine_t *coroutine;
} cat_socket_context_t;

typedef struct
{
    cat_queue_t coroutines;
} cat_socket_write_context_t;

typedef struct cat_socket_s cat_socket_t;
typedef struct cat_socket_internal_s cat_socket_internal_t;

struct cat_socket_internal_s
{
    /* === public === */
    /* options */
    struct {
        cat_socket_timeout_options_t timeout;
    } options;
    /* === private === */
    /* internal bits */
    cat_bool_t connected:1;
    /* io context */
    cat_socket_io_flags_t io_flags;
    union {
        cat_socket_context_t bind;
        cat_socket_context_t accept;
        cat_socket_context_t connect;
#ifdef CAT_SSL
        cat_socket_context_t handshake;
#endif
        struct {
            cat_socket_context_t read;
            cat_socket_write_context_t write;
        } io;
    } context;
    /* cache */
    struct {
        cat_sockaddr_info_t *sockname;
        cat_sockaddr_info_t *peername;
    } cache;
    /* ext */
#ifdef CAT_SSL
    cat_ssl_t *ssl;
#endif
    /* u must be the last one (due to dynamic alloc) */
    union {
        cat_socket_t *socket;
        uv_handle_t handle;
        uv_stream_t stream;
        uv_tcp_t tcp;
        uv_pipe_t pipe;
        uv_tty_t tty;
        uv_udp_t udp;
    } u;
};

struct cat_socket_s
{
    cat_socket_type_t type;
    cat_socket_internal_t *internal;
};

/* globals */

CAT_GLOBALS_STRUCT_BEGIN(cat_socket)
    /* socket */
    struct {
        cat_socket_timeout_options_t timeout;
        unsigned int tcp_keepalive_delay;
    } options;
    /* dns */
    // TODO: dns_cache (we should implement lru_cache)
CAT_GLOBALS_STRUCT_END(cat_socket)

extern CAT_API CAT_GLOBALS_DECLARE(cat_socket)

#define CAT_SOCKET_G(x) CAT_GLOBALS_GET(cat_socket, x)

/* module initialization */

CAT_API cat_bool_t cat_socket_module_init(void);
CAT_API cat_bool_t cat_socket_runtime_init(void);

/* common methods */
/* tip: functions of fast version will never change the last error */

CAT_API void cat_socket_init(cat_socket_t *socket);
CAT_API cat_socket_t *cat_socket_create(cat_socket_t *socket, cat_socket_type_t type);
CAT_API cat_socket_t *cat_socket_create_ex(cat_socket_t *socket, cat_socket_type_t type, cat_socket_fd_t fd);

CAT_API const char *cat_socket_type_name(cat_socket_type_t type);
CAT_API const char *cat_socket_get_type_name(const cat_socket_t *socket);
CAT_API cat_sa_family_t cat_socket_get_af_of_type(cat_socket_type_t type);
CAT_API cat_sa_family_t cat_socket_get_af(const cat_socket_t *socket);
CAT_API cat_socket_fd_t cat_socket_get_fd_fast(const cat_socket_t *socket);
CAT_API cat_socket_fd_t cat_socket_get_fd(const cat_socket_t *socket);

CAT_API cat_timeout_t cat_socket_get_global_dns_timeout(void);
CAT_API cat_timeout_t cat_socket_get_global_accept_timeout(void);
CAT_API cat_timeout_t cat_socket_get_global_connect_timeout(void);
CAT_API cat_timeout_t cat_socket_get_global_read_timeout(void);
CAT_API cat_timeout_t cat_socket_get_global_write_timeout(void);

CAT_API void cat_socket_set_global_dns_timeout(cat_timeout_t timeout);
CAT_API void cat_socket_set_global_accept_timeout(cat_timeout_t timeout);
CAT_API void cat_socket_set_global_connect_timeout(cat_timeout_t timeout);
CAT_API void cat_socket_set_global_read_timeout(cat_timeout_t timeout);
CAT_API void cat_socket_set_global_write_timeout(cat_timeout_t timeout);

CAT_API cat_timeout_t cat_socket_get_dns_timeout(const cat_socket_t *socket);
CAT_API cat_timeout_t cat_socket_get_accept_timeout(const cat_socket_t *socket);
CAT_API cat_timeout_t cat_socket_get_connect_timeout(const cat_socket_t *socket);
CAT_API cat_timeout_t cat_socket_get_read_timeout(const cat_socket_t *socket);
CAT_API cat_timeout_t cat_socket_get_write_timeout(const cat_socket_t *socket);

CAT_API cat_bool_t cat_socket_set_dns_timeout(cat_socket_t *socket, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_set_accept_timeout(cat_socket_t *socket, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_set_connect_timeout(cat_socket_t *socket, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_set_read_timeout(cat_socket_t *socket, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_set_write_timeout(cat_socket_t *socket, cat_timeout_t timeout);

CAT_API cat_bool_t cat_socket_bind(cat_socket_t *socket, const char *name, size_t name_length, int port);
CAT_API cat_bool_t cat_socket_bind_ex(cat_socket_t *socket, const char *name, size_t name_length, int port, cat_socket_bind_flags_t flags);
CAT_API cat_bool_t cat_socket_bind_to(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length);
CAT_API cat_bool_t cat_socket_bind_to_ex(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_socket_bind_flags_t flags);
CAT_API cat_bool_t cat_socket_listen(cat_socket_t *socket, int backlog);
CAT_API cat_socket_t *cat_socket_accept(cat_socket_t *server, cat_socket_t *client);
CAT_API cat_socket_t *cat_socket_accept_ex(cat_socket_t *server, cat_socket_t *client, cat_timeout_t timeout);

CAT_API cat_bool_t cat_socket_connect(cat_socket_t *socket, const char *name, size_t name_length, int port);
CAT_API cat_bool_t cat_socket_connect_ex(cat_socket_t *socket, const char *name, size_t name_length, int port, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_connect_to(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length);
CAT_API cat_bool_t cat_socket_connect_to_ex(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout);

CAT_API cat_bool_t cat_socket_getname(const cat_socket_t *socket, cat_sockaddr_t *address, cat_socklen_t *length, cat_bool_t is_peer);
CAT_API cat_bool_t cat_socket_getsockname(const cat_socket_t *socket, cat_sockaddr_t *address, cat_socklen_t *length);
CAT_API cat_bool_t cat_socket_getpeername(const cat_socket_t *socket, cat_sockaddr_t *address, cat_socklen_t *length);
CAT_API const cat_sockaddr_info_t *cat_socket_getname_fast(cat_socket_t *socket, cat_bool_t is_peer);
CAT_API const cat_sockaddr_info_t *cat_socket_getsockname_fast(cat_socket_t *socket);
CAT_API const cat_sockaddr_info_t *cat_socket_getpeername_fast(cat_socket_t *socket);

/* read: it always reads data of the specified length as far as possible, unless interrupted by errors  */
CAT_API ssize_t cat_socket_read(cat_socket_t *socket, char *buffer, size_t length);
CAT_API ssize_t cat_socket_read_ex(cat_socket_t *socket, char *buffer, size_t length, cat_timeout_t timeout);
/* write: it always writes all data as much as possible, unless interrupted by errors */
CAT_API cat_bool_t cat_socket_write(cat_socket_t *socket, const cat_socket_write_vector_t *vectors, unsigned int vector_count);
CAT_API cat_bool_t cat_socket_write_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vectors, unsigned int vector_count, cat_timeout_t timeout);

CAT_API ssize_t cat_socket_read_from(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port);
CAT_API ssize_t cat_socket_read_from_ex(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_writeto(cat_socket_t *socket, const cat_socket_write_vector_t *vectors, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length);
CAT_API cat_bool_t cat_socket_writeto_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vectors, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_write_to(cat_socket_t *socket, const cat_socket_write_vector_t *vectors, unsigned int vector_count, const char *name, size_t name_length, int port);
CAT_API cat_bool_t cat_socket_write_to_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vectors, unsigned int vector_count, const char *name, size_t name_length, int port, cat_timeout_t timeout);

/* recv: same as recv system call, it always returns as soon as possible */
CAT_API ssize_t cat_socket_recv(cat_socket_t *socket, char *buffer, size_t size);
CAT_API ssize_t cat_socket_recv_ex(cat_socket_t *socket, char *buffer, size_t size, cat_timeout_t timeout);
/* send: it always sends all data as much as possible, unless interrupted by errors */
CAT_API cat_bool_t cat_socket_send(cat_socket_t *socket, const char *buffer, size_t length);
CAT_API cat_bool_t cat_socket_send_ex(cat_socket_t *socket, const char *buffer, size_t length, cat_timeout_t timeout);

/* Notice: [name rule] only native APIs use conjunctions, e.g. recvfrom/sendto/getsockname/getpeername...  */
CAT_API ssize_t cat_socket_recvfrom(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length);
CAT_API ssize_t cat_socket_recvfrom_ex(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_sendto(cat_socket_t *socket, const char *buffer, size_t length, const cat_sockaddr_t *address, cat_socklen_t address_length);
CAT_API cat_bool_t cat_socket_sendto_ex(cat_socket_t *socket, const char *buffer, size_t length, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_send_to(cat_socket_t *socket, const char *buffer, size_t length, const char *name, size_t name_length, int port);
CAT_API cat_bool_t cat_socket_send_to_ex(cat_socket_t *socket, const char *buffer, size_t length, const char *name, size_t name_length, int port, cat_timeout_t timeout);

CAT_API ssize_t cat_socket_peek(const cat_socket_t *socket, char *buffer, size_t size);
CAT_API ssize_t cat_socket_peekfrom(const cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length);
CAT_API ssize_t cat_socket_peek_from(const cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port);

CAT_API cat_bool_t cat_socket_close(cat_socket_t *socket);

/* getter */

CAT_API cat_bool_t cat_socket_is_available(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_is_open(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_is_established(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_is_server(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_is_client(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_check_liveness(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_is_eof_error(cat_errno_t error);

/* Notice: input buffer_size is size of buffer (not length), output buffer_size is length (strlen(buffer))
 * buffer is always zero-termination */
CAT_API cat_bool_t cat_socket_get_address(cat_socket_t *socket, char *buffer, size_t *buffer_size, cat_bool_t is_peer);
CAT_API cat_bool_t cat_socket_get_peer_address(cat_socket_t *socket, char *buffer, size_t *buffer_size);
CAT_API cat_bool_t cat_socket_get_sock_address(cat_socket_t *socket, char *buffer, size_t *buffer_size);
CAT_API int cat_socket_get_port(cat_socket_t *socket, cat_bool_t is_peer);
CAT_API int cat_socket_get_sock_port(cat_socket_t *socket);
CAT_API int cat_socket_get_peer_port(cat_socket_t *socket);

CAT_API const char *cat_socket_io_state_name(cat_socket_io_flags_t io_state);
CAT_API const char *cat_socket_io_state_naming(cat_socket_io_flags_t io_state);

CAT_API cat_socket_io_flags_t cat_socket_get_io_state(const cat_socket_t *socket);
CAT_API const char *cat_socket_get_io_state_name(const cat_socket_t *socket);
CAT_API const char *cat_socket_get_io_state_naming(const cat_socket_t *socket);

/* setter */

CAT_API int cat_socket_get_recv_buffer_size(const cat_socket_t *socket);
CAT_API int cat_socket_get_send_buffer_size(const cat_socket_t *socket);
CAT_API int cat_socket_set_recv_buffer_size(cat_socket_t *socket, int size);
CAT_API int cat_socket_set_send_buffer_size(cat_socket_t *socket, int size);

CAT_API cat_bool_t cat_socket_set_tcp_nodelay(cat_socket_t *socket, cat_bool_t enable);
CAT_API cat_bool_t cat_socket_set_tcp_keepalive(cat_socket_t *socket, cat_bool_t enable, unsigned int delay);
CAT_API cat_bool_t cat_socket_set_tcp_accept_balance(cat_socket_t *socket, cat_bool_t enable);

/* helper */

CAT_API int cat_socket_get_local_free_port(void);
CAT_API void cat_socket_dump_all(void);

#ifdef __cplusplus
}
#endif
#endif /* CAT_SOCKET_H */
