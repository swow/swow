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

#define CAT_SOCKET_DEFAULT_BACKLOG  511

#define CAT_SOCKET_IPV4_BUFFER_SIZE 16  /* >= 16 */
#define CAT_SOCKET_IPV6_BUFFER_SIZE 48  /* >= 46 */
#define CAT_SOCKET_IP_BUFFER_SIZE   CAT_SOCKET_IPV6_BUFFER_SIZE

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif
#ifdef CAT_OS_UNIX_LIKE
#define CAT_SOCKADDR_MAX_PATH (sizeof(((struct sockaddr_un *) 0)->sun_path))
#define CAT_SOCKLEN_FMT "%u"
#define CAT_SOCKLEN_FMT_SPEC "u"
typedef socklen_t cat_socklen_t; /* Notice: it's unsigned on darwin */
typedef sa_family_t cat_sa_family_t;
typedef in_port_t cat_in_port_t;
#else
#define CAT_SOCKADDR_MAX_PATH 260
#define CAT_SOCKLEN_FMT "%d"
#define CAT_SOCKLEN_FMT_SPEC "d"
typedef int cat_socklen_t;
typedef short cat_sa_family_t;
typedef unsigned short cat_in_port_t;
#endif

#define CAT_SOCKADDR_HEADER_LENGTH offsetof(cat_sockaddr_t, sa_data)

typedef struct sockaddr     cat_sockaddr_t;
typedef struct sockaddr_in  cat_sockaddr_in_t;
typedef struct sockaddr_in6 cat_sockaddr_in6_t;
typedef struct cat_sockaddr_local_s {
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

typedef union {
    cat_sockaddr_t common;
    cat_sockaddr_in_t in;
    cat_sockaddr_in6_t in6;
} cat_sockaddr_inet_union_t;

typedef union {
    cat_sockaddr_t common;
    cat_sockaddr_in_t in;
    cat_sockaddr_in6_t in6;
    cat_sockaddr_local_t local;
    cat_sockaddr_storage_t storage;
} cat_sockaddr_union_t;

typedef struct cat_sockaddr_inet_info_s {
    cat_socklen_t length;
    cat_sockaddr_inet_union_t address;
} cat_sockaddr_inet_info_t;

typedef struct cat_sockaddr_info_s {
    cat_socklen_t length;
    cat_sockaddr_union_t address;
} cat_sockaddr_info_t;

CAT_API const char* cat_sockaddr_af_name(cat_sa_family_t af);

/* Notice: input buffer_size is size of buffer (not length)
 * and if ENOSPC, *buffer_size will be the minimum required buffer **size**, otherwise, *buffer_size will be strlen(buffer)
 * buffer is always zero-termination */
CAT_API cat_errno_t cat_sockaddr_get_address_silent(const cat_sockaddr_t *address, cat_socklen_t address_length, char *buffer, size_t *buffer_size);
CAT_API int cat_sockaddr_get_port_silent(const cat_sockaddr_t *address);
CAT_API cat_bool_t cat_sockaddr_get_address(const cat_sockaddr_t *address, cat_socklen_t address_length, char *buffer, size_t *buffer_size);
CAT_API int cat_sockaddr_get_port(const cat_sockaddr_t *address);
CAT_API cat_bool_t cat_sockaddr_set_port(cat_sockaddr_t *address, int port);

/* Notice: do not forget to init address->sa_family and address_length */
CAT_API cat_bool_t cat_sockaddr_getbyname(cat_sockaddr_t *address, cat_socklen_t *address_length, const char *name, size_t name_length, int port);
/* Notice: it can handle empty case internally, but address must be valid or empty */
CAT_API cat_errno_t cat_sockaddr_to_name_silent(const cat_sockaddr_t *address, cat_socklen_t address_length, char *name, size_t *name_length, int *port);
CAT_API cat_bool_t cat_sockaddr_to_name(const cat_sockaddr_t *address, cat_socklen_t address_length, char *name, size_t *name_length, int *port);

/* Notice: it can handle empty "to" internally */
CAT_API int cat_sockaddr_copy(cat_sockaddr_t *to, cat_socklen_t *to_length, const cat_sockaddr_t *from, cat_socklen_t from_length);
CAT_API cat_errno_t cat_sockaddr_check_silent(const cat_sockaddr_t *address, cat_socklen_t address_length);
CAT_API cat_bool_t cat_sockaddr_check(const cat_sockaddr_t *address, cat_socklen_t address_length);

/* socket id */

typedef uint64_t cat_socket_id_t;

/* socket fd */

typedef cat_os_socket_t cat_socket_fd_t;
#define CAT_SOCKET_FD_FMT CAT_OS_SOCKET_FMT
#define CAT_SOCKET_FD_FMT_SPEC CAT_OS_SOCKET_FMT_SPEC
#define CAT_SOCKET_INVALID_FD CAT_OS_INVALID_SOCKET

/* stdio os fd */

#ifndef STDIN_FILENO
#define STDIN_FILENO  0 /* standard input file descriptor */
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1 /* standard output file descriptor */
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2 /* standard error file descriptor */
#endif

/* socket length */

#ifndef CAT_OS_WIN
typedef size_t cat_socket_recv_length_t;
#else
typedef int cat_socket_recv_length_t;
#endif

/* socket vector */

#ifndef CAT_OS_WIN
typedef size_t cat_socket_vector_length_t;
#else
typedef ULONG cat_socket_vector_length_t;
#endif

#ifndef CAT_OS_WIN
/* Note: May be cast to struct iovec. See writev(2). */
typedef struct cat_socket_write_vector_s {
    const char *base;
    cat_socket_vector_length_t length;
} cat_socket_write_vector_t;
#else
/* Note: May be cast to WSABUF[]
 * see http://msdn.microsoft.com/en-us/library/ms741542(v=vs.85).aspx */
typedef struct cat_socket_write_vector_s {
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

CAT_API size_t cat_socket_write_vector_length(const cat_socket_write_vector_t *vector, unsigned int vector_count);

/* socket */

#ifdef CAT_OS_UNIX_LIKE
#define CAT_SOCKET_USE_REACTOR 1
#endif

#ifdef TCP_KEEPALIVE /* naming conflict */
#undef TCP_KEEPALIVE
#endif

#define CAT_SOCKET_CREATION_FLAG_MAP(XX) \
    XX(NONE,              0) \
    XX(OPEN_FD,      1 << 0) \
    XX(OPEN_SOCKET,  1 << 1) \
    XX(OPEN_HANDLE,  1 << 2) \

typedef enum cat_socket_creation_flag_e {
#define CAT_SOCKET_CREATION_FLAG_GEN(name, value) CAT_ENUM_GEN(CAT_SOCKET_CREATION_FLAG_, name, value)
    CAT_SOCKET_CREATION_FLAG_MAP(CAT_SOCKET_CREATION_FLAG_GEN)
#undef CAT_SOCKET_CREATION_FLAG_GEN
} cat_socket_creation_flag_t;

typedef enum cat_socket_creation_union_flags_e {
    CAT_SOCKET_CREATION_OPEN_FLAGS = CAT_SOCKET_CREATION_FLAG_OPEN_FD |
                                     CAT_SOCKET_CREATION_FLAG_OPEN_SOCKET |
                                     CAT_SOCKET_CREATION_FLAG_OPEN_HANDLE,
} cat_socket_creation_union_flags_t;

typedef uint32_t cat_socket_creation_flags_t;

typedef struct cat_socket_creation_options_s {
    cat_socket_creation_flags_t flags;
    union {
        cat_os_fd_t fd;
        cat_os_socket_t socket;
        cat_os_handle_t handle;
    } o;
} cat_socket_creation_options_t;

/* 0 ~ 23 */
#define CAT_SOCKET_TYPE_FLAG_MAP_EX(XX, SS) \
    /* 0 - 3 (sock) */ \
    XX(STREAM, 1 << 0) \
    XX(DGRAM,  1 << 1) \
    /* 4 ~ 9 (af) */ \
    XX(INET,   1 << 4) \
    XX(IPV4,   1 << 5) \
    XX(IPV6,   1 << 6) \
    XX(LOCAL,  1 << 7) \
    /* 10 ~ 19 (pipe-extra) */ \
    XX(IPC,    1 << 10) \
    /* 10 ~ 19 (tty-extra) */ \
    XX(STDIN,  1 << 10) \
    XX(STDOUT, 1 << 11) \
    XX(STDERR, 1 << 12) \

#define CAT_SOCKET_TYPE_FLAG_MAP(XX) CAT_SOCKET_TYPE_FLAG_MAP_EX(XX, CAT_SSL_ENUM_GEN(XX))

typedef enum cat_socket_type_flag_e {
#define CAT_SOCKET_TYPE_FLAG_GEN(name, value) CAT_ENUM_GEN(CAT_SOCKET_TYPE_FLAG_, name, value)
    CAT_SOCKET_TYPE_FLAG_MAP(CAT_SOCKET_TYPE_FLAG_GEN)
#undef CAT_SOCKET_TYPE_FLAG_GEN
} cat_socket_type_flag_t;

#define CAT_SOCKET_TYPE_MAP_EX(XX, UN) \
    XX(ANY,    0) \
    /* stream */ \
    XX(TCP,    1 << 24 | CAT_SOCKET_TYPE_FLAG_STREAM | CAT_SOCKET_TYPE_FLAG_INET) \
    XX(TCP4,   CAT_SOCKET_TYPE_TCP | CAT_SOCKET_TYPE_FLAG_IPV4) \
    XX(TCP6,   CAT_SOCKET_TYPE_TCP | CAT_SOCKET_TYPE_FLAG_IPV6) \
    XX(PIPE,   1 << 25 | CAT_SOCKET_TYPE_FLAG_STREAM | CAT_SOCKET_TYPE_FLAG_LOCAL) \
    XX(IPCC,   CAT_SOCKET_TYPE_PIPE | CAT_SOCKET_TYPE_FLAG_IPC) /* IPC channel */ \
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

#ifndef CAT_OS_UNIX_LIKE
#define CAT_SOCKET_UNIX_ENUM_GEN(XX) CAT_ENUM_EMPTY_GEN
#else
#define CAT_SOCKET_UNIX_ENUM_GEN(XX) XX
#endif
#define CAT_SOCKET_TYPE_MAP(XX) CAT_SOCKET_TYPE_MAP_EX(XX, CAT_SOCKET_UNIX_ENUM_GEN(XX))

/* 24 ~ 31 */
typedef enum cat_socket_type_e {
#define CAT_SOCKET_TYPE_GEN(name, value) CAT_ENUM_GEN(CAT_SOCKET_TYPE_, name, value)
    CAT_SOCKET_TYPE_MAP(CAT_SOCKET_TYPE_GEN)
#undef CAT_SOCKET_TYPE_GEN
} cat_socket_type_t;

#define CAT_SOCKET_OPTION_FLAG_MAP(XX) \
    XX(NONE,               0) \
    /* 1 ~ 8 ((tcp|udp)-extra) */ \
    XX(TCP_DELAY,     1 << 0)  /* (disable tcp_nodelay) */ \
    XX(TCP_KEEPALIVE, 1 << 1)  /* (enable keep-alive) */ \
    XX(UDP_BROADCAST, 1 << 2)  /* (enable broadcast) TODO: support it or remove */ \

typedef enum cat_socket_option_flag_e {
#define CAT_SOCKET_OPTION_FLAG_GEN(name, value) CAT_ENUM_GEN(CAT_SOCKET_OPTION_FLAG_, name, value)
    CAT_SOCKET_OPTION_FLAG_MAP(CAT_SOCKET_OPTION_FLAG_GEN)
#undef CAT_SOCKET_OPTION_FLAG_GEN
} cat_socket_option_flag_t;

typedef uint32_t cat_socket_option_flags_t;

/* 0 ~ 8 */
#define CAT_SOCKET_FLAG_MAP(XX) \
    XX(NONE,                     0) \
    XX(ALLOCATED,           1 << 0) \
    XX(CLOSED,              1 << 1) \
    XX(UNRECOVERABLE_ERROR, 1 << 2) \

typedef enum cat_socket_flag_e {
#define CAT_SOCKET_FLAG_GEN(name, value) CAT_ENUM_GEN(CAT_SOCKET_FLAG_, name, value)
    CAT_SOCKET_FLAG_MAP(CAT_SOCKET_FLAG_GEN)
#undef CAT_SOCKET_FLAG_GEN
} cat_socket_flag_t;

typedef uint8_t cat_socket_flags_t;

#define CAT_SOCKET_INTERNAL_FLAG_MAP(XX) \
    XX(NONE,                   0) \
    XX(OPENED,            1 << 0) \
    XX(ESTABLISHED,       1 << 1) \
    /* socket may be a pipe file, which is created by pipe2()
     * and can only work with read()/write() */ \
    XX(NOT_SOCK,          1 << 2) \
    /* 20 ~ 23 (stream (tcp|pipe|tty)) */ \
    XX(SERVER,            1 << 20) \
    XX(SERVER_CONNECTION, 1 << 21) \
    XX(CLIENT,            1 << 22) \

typedef enum cat_socket_internal_flag_e {
#define CAT_SOCKET_INTERNAL_FLAG_GEN(name, value) CAT_ENUM_GEN(CAT_SOCKET_INTERNAL_FLAG_, name, value)
    CAT_SOCKET_INTERNAL_FLAG_MAP(CAT_SOCKET_INTERNAL_FLAG_GEN)
#undef CAT_SOCKET_INTERNAL_FLAG_GEN
} cat_socket_internal_flag_t;

typedef uint32_t cat_socket_internal_flags_t;

#define CAT_SOCKET_IO_FLAG_MAP(XX) \
    XX(NONE,      0) \
    XX(READ,      1 << 0) \
    XX(WRITE,     1 << 1) \
    XX(RDWR,      CAT_SOCKET_IO_FLAG_READ | CAT_SOCKET_IO_FLAG_WRITE) \
    XX(BIND,      1 << 2 | CAT_SOCKET_IO_FLAG_RDWR) \
    XX(ACCEPT,    1 << 3 | CAT_SOCKET_IO_FLAG_RDWR) \
    XX(CONNECT,   1 << 4 | CAT_SOCKET_IO_FLAG_RDWR)

typedef enum cat_socket_io_flag_e {
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

typedef enum cat_socket_bind_flag_e {
#define CAT_SOCKET_BIND_FLAG_GEN(name, value) CAT_ENUM_GEN(CAT_SOCKET_BIND_FLAG_, name, value)
    CAT_SOCKET_BIND_FLAG_MAP(CAT_SOCKET_BIND_FLAG_GEN)
#undef CAT_SOCKET_BIND_FLAG_GEN
} cat_socket_bind_flag_t;

typedef uint8_t cat_socket_bind_flags_t;

typedef enum cat_socket_bind_union_flags_e {
    CAT_SOCKET_BIND_FLAGS_ALL = CAT_SOCKET_BIND_FLAG_IPV6ONLY | CAT_SOCKET_BIND_FLAG_REUSEADDR
} cat_socket_bind_union_flags_t;

typedef int32_t cat_socket_timeout_storage_t;
#define CAT_SOCKET_TIMEOUT_STORAGE_FMT      "%d"
#define CAT_SOCKET_TIMEOUT_STORAGE_FMT_SPEC "d"
#define CAT_SOCKET_TIMEOUT_STORAGE_MIN      CAT_TIMEOUT_FOREVER
#define CAT_SOCKET_TIMEOUT_STORAGE_MAX      INT32_MAX
#define CAT_SOCKET_TIMEOUT_STORAGE_DEFAULT  -CAT_MAGIC_NUMBER

typedef struct cat_socket_timeout_options_s {
    cat_socket_timeout_storage_t dns;
    cat_socket_timeout_storage_t accept;
    cat_socket_timeout_storage_t connect;
    cat_socket_timeout_storage_t handshake;
    cat_socket_timeout_storage_t read;
    cat_socket_timeout_storage_t write;
} cat_socket_timeout_options_t;

#define CAT_SOCKET_TIMEOUT_OPTIONS_COUNT (sizeof(cat_socket_timeout_options_t) / sizeof(cat_socket_timeout_storage_t))

typedef struct cat_socket_context_s {
    union {
        void *ptr;
        int status;
    } data;
    cat_coroutine_t *coroutine;
} cat_socket_context_t;

typedef struct cat_socket_write_context_s {
    cat_queue_t coroutines;
} cat_socket_write_context_t;

typedef struct cat_socket_write_request_s {
    int error;
    union {
        cat_coroutine_t *coroutine;
        uv_write_t stream;
        uv_udp_send_t udp;
    } u;
} cat_socket_write_request_t;

#ifdef CAT_OS_UNIX_LIKE
typedef struct uv_udg_s {
    uv_pipe_t pipe;
    cat_socket_fd_t readfd;
    cat_socket_fd_t writefd;
} uv_udg_t;
#endif

typedef struct cat_socket_s cat_socket_t;
typedef struct cat_socket_internal_s cat_socket_internal_t;

typedef struct cat_socket_options_s {
    cat_socket_timeout_options_t timeout;
    unsigned int tcp_keepalive_delay;
} cat_socket_options_t;

typedef struct cat_socket_inheritance_info_s {
    cat_socket_type_t type;
    cat_socket_options_t options;
} cat_socket_inheritance_info_t;

struct cat_socket_internal_s
{
    /* === public === */
    /* type (readonly) */
    cat_socket_type_t type;
    /* options */
    cat_socket_option_flags_t option_flags;
    cat_socket_options_t options;
    /* === private === */
    /* internal bits */
    cat_socket_internal_flags_t flags;
    /* io context */
    cat_socket_io_flags_t io_flags;
    union {
        cat_socket_context_t bind;
        cat_socket_context_t accept;
        cat_socket_context_t connect;
        struct {
            cat_socket_context_t read;
            cat_socket_write_context_t write;
        } io;
    } context;
    /* cache */
    struct {
        cat_socket_fd_t fd;
        cat_socket_write_request_t *write_request;
        cat_socket_inheritance_info_t *ipcc_handle_info;
        cat_sockaddr_info_t *sockname;
        cat_sockaddr_info_t *peername;
        int recv_buffer_size;
        int send_buffer_size;
    } cache;
    /* ext */
#ifdef CAT_SSL
    cat_ssl_t *ssl;
    char *ssl_peer_name;
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
#ifdef CAT_OS_UNIX_LIKE
        uv_udg_t udg;
#endif
    } u;
};

struct cat_socket_s
{
    cat_socket_id_t id;
    cat_socket_flags_t flags;
    cat_socket_internal_t *internal;
};

/* globals */

CAT_GLOBALS_STRUCT_BEGIN(cat_socket)
    /* socket */
    cat_socket_id_t last_id;
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
CAT_API cat_socket_t *cat_socket_create_ex(cat_socket_t *socket, cat_socket_type_t type, const cat_socket_creation_options_t *options);

CAT_API cat_socket_t *cat_socket_open_os_fd(cat_socket_t *socket, cat_socket_type_t type, cat_os_fd_t os_fd);
CAT_API cat_socket_t *cat_socket_open_os_socket(cat_socket_t *socket, cat_socket_type_t type, cat_os_socket_t os_socket);

CAT_API cat_socket_type_t cat_socket_type_simplify(cat_socket_type_t type);
CAT_API const char *cat_socket_type_name(cat_socket_type_t type);
CAT_API cat_sa_family_t cat_socket_type_to_af(cat_socket_type_t type);

CAT_API cat_socket_id_t cat_socket_get_id(const cat_socket_t *socket);
CAT_API cat_socket_type_t cat_socket_get_type(const cat_socket_t *socket);
CAT_API cat_socket_type_t cat_socket_get_simple_type(const cat_socket_t *socket);
CAT_API const char *cat_socket_get_type_name(const cat_socket_t *socket);
CAT_API const char *cat_socket_get_simple_type_name(const cat_socket_t *socket);
CAT_API cat_sa_family_t cat_socket_get_af(const cat_socket_t *socket);
CAT_API cat_socket_fd_t cat_socket_get_fd_fast(const cat_socket_t *socket);
CAT_API cat_socket_fd_t cat_socket_get_fd(const cat_socket_t *socket);

CAT_API cat_timeout_t cat_socket_get_global_dns_timeout(void);
CAT_API cat_timeout_t cat_socket_get_global_accept_timeout(void);
CAT_API cat_timeout_t cat_socket_get_global_connect_timeout(void);
CAT_API cat_timeout_t cat_socket_get_global_handshake_timeout(void);
CAT_API cat_timeout_t cat_socket_get_global_read_timeout(void);
CAT_API cat_timeout_t cat_socket_get_global_write_timeout(void);

CAT_API void cat_socket_set_global_timeout(cat_timeout_t timeout);
CAT_API void cat_socket_set_global_dns_timeout(cat_timeout_t timeout);
CAT_API void cat_socket_set_global_accept_timeout(cat_timeout_t timeout);
CAT_API void cat_socket_set_global_connect_timeout(cat_timeout_t timeout);
CAT_API void cat_socket_set_global_handshake_timeout(cat_timeout_t timeout);
CAT_API void cat_socket_set_global_read_timeout(cat_timeout_t timeout);
CAT_API void cat_socket_set_global_write_timeout(cat_timeout_t timeout);

CAT_API cat_timeout_t cat_socket_get_dns_timeout(const cat_socket_t *socket);
CAT_API cat_timeout_t cat_socket_get_accept_timeout(const cat_socket_t *socket);
CAT_API cat_timeout_t cat_socket_get_connect_timeout(const cat_socket_t *socket);
CAT_API cat_timeout_t cat_socket_get_handshake_timeout(const cat_socket_t *socket);
CAT_API cat_timeout_t cat_socket_get_read_timeout(const cat_socket_t *socket);
CAT_API cat_timeout_t cat_socket_get_write_timeout(const cat_socket_t *socket);

CAT_API cat_bool_t cat_socket_set_timeout(cat_socket_t *socket, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_set_dns_timeout(cat_socket_t *socket, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_set_accept_timeout(cat_socket_t *socket, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_set_connect_timeout(cat_socket_t *socket, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_set_handshake_timeout(cat_socket_t *socket, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_set_read_timeout(cat_socket_t *socket, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_set_write_timeout(cat_socket_t *socket, cat_timeout_t timeout);

CAT_API cat_bool_t cat_socket_bind(cat_socket_t *socket, const char *name, size_t name_length, int port);
CAT_API cat_bool_t cat_socket_bind_ex(cat_socket_t *socket, const char *name, size_t name_length, int port, cat_socket_bind_flags_t flags);
CAT_API cat_bool_t cat_socket_bind_to(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length);
CAT_API cat_bool_t cat_socket_bind_to_ex(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_socket_bind_flags_t flags);
CAT_API cat_bool_t cat_socket_listen(cat_socket_t *socket, int backlog);
CAT_API cat_bool_t cat_socket_accept(cat_socket_t *server, cat_socket_t *client);
CAT_API cat_bool_t cat_socket_accept_ex(cat_socket_t *server, cat_socket_t *client, cat_timeout_t timeout);

CAT_API cat_bool_t cat_socket_connect(cat_socket_t *socket, const char *name, size_t name_length, int port);
CAT_API cat_bool_t cat_socket_connect_ex(cat_socket_t *socket, const char *name, size_t name_length, int port, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_connect_to(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length);
CAT_API cat_bool_t cat_socket_connect_to_ex(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout);

/* Notice: it returns true even if it's in progress, we should check it by socket_is_established() to get real state of it */
CAT_API cat_bool_t cat_socket_try_connect(cat_socket_t *socket, const char *name, size_t name_length, int port);
CAT_API cat_bool_t cat_socket_try_connect_to(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length);

#ifdef CAT_SSL
typedef struct cat_socket_crypto_options_s {
    const char *peer_name;
    const char *ca_file;
    const char *ca_path;
    const char *certificate;
    const char *certificate_key;
    const char *passphrase;
    cat_ssl_protocols_t protocols;
    int verify_depth;
    cat_bool_t is_client;
    cat_bool_t verify_peer;
    cat_bool_t verify_peer_name;
    cat_bool_t allow_self_signed;
    cat_bool_t no_ticket;
    cat_bool_t no_compression;
    cat_bool_t no_client_ca_list;
} cat_socket_crypto_options_t;

CAT_API void cat_socket_crypto_options_init(cat_socket_crypto_options_t *options, cat_bool_t is_client);

CAT_API cat_bool_t cat_socket_enable_crypto(cat_socket_t *socket, const cat_socket_crypto_options_t *options);
CAT_API cat_bool_t cat_socket_enable_crypto_ex(cat_socket_t *socket, const cat_socket_crypto_options_t *options, cat_timeout_t timeout);
#endif

CAT_API cat_bool_t cat_socket_getname(const cat_socket_t *socket, cat_sockaddr_t *address, cat_socklen_t *length, cat_bool_t is_peer);
CAT_API cat_bool_t cat_socket_getsockname(const cat_socket_t *socket, cat_sockaddr_t *address, cat_socklen_t *length);
CAT_API cat_bool_t cat_socket_getpeername(const cat_socket_t *socket, cat_sockaddr_t *address, cat_socklen_t *length);
CAT_API const cat_sockaddr_info_t *cat_socket_getname_fast(cat_socket_t *socket, cat_bool_t is_peer);
CAT_API const cat_sockaddr_info_t *cat_socket_getsockname_fast(cat_socket_t *socket);
CAT_API const cat_sockaddr_info_t *cat_socket_getpeername_fast(cat_socket_t *socket);

/* recv: same as recv system call, it always returns as soon as possible */
CAT_API ssize_t cat_socket_recv(cat_socket_t *socket, char *buffer, size_t size);
CAT_API ssize_t cat_socket_recv_ex(cat_socket_t *socket, char *buffer, size_t size, cat_timeout_t timeout);
/* send: it always sends all data as much as possible, unless interrupted by errors */
CAT_API cat_bool_t cat_socket_send(cat_socket_t *socket, const char *buffer, size_t length);
CAT_API cat_bool_t cat_socket_send_ex(cat_socket_t *socket, const char *buffer, size_t length, cat_timeout_t timeout);

/* read: it always reads data of the specified length as far as possible, unless interrupted by errors  */
CAT_API ssize_t cat_socket_read(cat_socket_t *socket, char *buffer, size_t length);
CAT_API ssize_t cat_socket_read_ex(cat_socket_t *socket, char *buffer, size_t length, cat_timeout_t timeout);
/* write: it always writes all data as much as possible, unless interrupted by errors */
CAT_API cat_bool_t cat_socket_write(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count);
CAT_API cat_bool_t cat_socket_write_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, cat_timeout_t timeout);

/* Notice: [name rule] only native APIs use conjunctions, e.g. recvfrom/sendto/getsockname/getpeername...  */
CAT_API ssize_t cat_socket_recvfrom(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length);
CAT_API ssize_t cat_socket_recvfrom_ex(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_sendto(cat_socket_t *socket, const char *buffer, size_t length, const cat_sockaddr_t *address, cat_socklen_t address_length);
CAT_API cat_bool_t cat_socket_sendto_ex(cat_socket_t *socket, const char *buffer, size_t length, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_writeto(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length);
CAT_API cat_bool_t cat_socket_writeto_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout);

CAT_API ssize_t cat_socket_recv_from(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port);
CAT_API ssize_t cat_socket_recv_from_ex(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_send_to(cat_socket_t *socket, const char *buffer, size_t length, const char *name, size_t name_length, int port);
CAT_API cat_bool_t cat_socket_send_to_ex(cat_socket_t *socket, const char *buffer, size_t length, const char *name, size_t name_length, int port, cat_timeout_t timeout);
CAT_API cat_bool_t cat_socket_write_to(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const char *name, size_t name_length, int port);
CAT_API cat_bool_t cat_socket_write_to_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const char *name, size_t name_length, int port, cat_timeout_t timeout);

/* try_* APIs will return read/write bytes immediately, if error occurred, it returns E* errno */
CAT_API ssize_t cat_socket_try_recv(cat_socket_t *socket, char *buffer, size_t size);
CAT_API ssize_t cat_socket_try_recvfrom(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length);
CAT_API ssize_t cat_socket_try_recv_from(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port);
CAT_API ssize_t cat_socket_try_send(cat_socket_t *socket, const char *buffer, size_t length);
CAT_API ssize_t cat_socket_try_sendto(cat_socket_t *socket, const char *buffer, size_t length, const cat_sockaddr_t *address, cat_socklen_t address_length);
CAT_API ssize_t cat_socket_try_send_to(cat_socket_t *socket, const char *buffer, size_t length, const char *name, size_t name_length, int port);
CAT_API ssize_t cat_socket_try_write(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count);
CAT_API ssize_t cat_socket_try_writeto(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length);
CAT_API ssize_t cat_socket_try_write_to(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const char *name, size_t name_length, int port);

CAT_API ssize_t cat_socket_peek(const cat_socket_t *socket, char *buffer, size_t size);
CAT_API ssize_t cat_socket_peekfrom(const cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length);
CAT_API ssize_t cat_socket_peek_from(const cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port);

CAT_API cat_bool_t cat_socket_send_handle(cat_socket_t *socket, cat_socket_t *handle);
CAT_API cat_bool_t cat_socket_send_handle_ex(cat_socket_t *socket, cat_socket_t *handle, cat_timeout_t timeout);

CAT_API cat_bool_t cat_socket_close(cat_socket_t *socket);

/* getter */

CAT_API cat_bool_t cat_socket_is_available(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_is_open(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_is_established(const cat_socket_t *socket);
#ifdef CAT_SSL
CAT_API cat_bool_t cat_socket_has_crypto(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_is_encrypted(const cat_socket_t *socket);
#endif
CAT_API cat_bool_t cat_socket_is_server(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_is_server_connection(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_is_client(const cat_socket_t *socket);
CAT_API const char *cat_socket_get_role_name(const cat_socket_t *socket);
CAT_API cat_errno_t cat_socket_get_connection_error(const cat_socket_t *socket);
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

CAT_API cat_bool_t cat_socket_get_tcp_nodelay(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_set_tcp_nodelay(cat_socket_t *socket, cat_bool_t enable);

CAT_API cat_bool_t cat_socket_get_tcp_keepalive(const cat_socket_t *socket);
CAT_API unsigned int cat_socket_get_tcp_keepalive_delay(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_set_tcp_keepalive(cat_socket_t *socket, cat_bool_t enable, unsigned int delay);

CAT_API cat_bool_t cat_socket_set_tcp_accept_balance(cat_socket_t *socket, cat_bool_t enable);

CAT_API cat_bool_t cat_socket_get_udp_broadcast(const cat_socket_t *socket);
CAT_API cat_bool_t cat_socket_set_udp_broadcast(cat_socket_t *socket, cat_bool_t enable);

/* helper */

CAT_API int cat_socket_get_local_free_port(void);

CAT_API void cat_socket_dump_all(void);
CAT_API void cat_socket_close_all(void);

CAT_API cat_bool_t cat_socket_move(cat_socket_t *from, cat_socket_t *to);

/* pipe */

typedef enum cat_pipe_flag_e {
    CAT_PIPE_FLAG_NONE = 0,
    CAT_PIPE_FLAG_NONBLOCK = UV_NONBLOCK_PIPE,
} cat_pipe_flag_t;

typedef int cat_pipe_flags;

CAT_API cat_bool_t cat_pipe(cat_os_fd_t fds[2], cat_pipe_flags read_flags, cat_pipe_flags write_flags);

#ifdef __cplusplus
}
#endif
#endif /* CAT_SOCKET_H */
