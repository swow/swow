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

#include "cat_socket.h"
#include "cat_event.h"
#include "cat_time.h"

#include "uv-common.h"

#ifdef CAT_OS_UNIX_LIKE
/* For EINTR */
#include <errno.h>
/* For syscall recv */
#include <sys/types.h>
#include <sys/socket.h>
/* for sockaddr_un*/
#include <sys/un.h>
#endif /* CAT_OS_UNIX_LIKE */

#ifdef CAT_OS_WIN
#include <winsock2.h>
#endif /* CAT_OS_WIN */

#ifdef __linux__
#define cat_sockaddr_is_linux_abstract_name(path) (path[0] == '\0')
#else
#define cat_sockaddr_is_linux_abstract_name(path) 0
#endif

#if defined(__APPLE__)
/* Due to a possible kernel bug at least in OS X 10.10 "Yosemite",
 * EPROTOTYPE can be returned while trying to write to a socket that is
 * shutting down. If we retry the write, we should get the expected EPIPE
 * instead. */
#define CAT_SOCKET_RETRY_ON_WRITE_ERROR(errno) (errno == EINTR || errno == EPROTOTYPE)
#else
#define CAT_SOCKET_RETRY_ON_WRITE_ERROR(errno) (errno == EINTR)
#endif /* defined(__APPLE__) */

#define CAT_SOCKET_IS_TRANSIENT_WRITE_ERROR(errno) \
    (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS)

#ifdef AF_UNIX
CAT_STATIC_ASSERT(AF_LOCAL == AF_UNIX);
#endif

CAT_API const char* cat_sockaddr_af_name(cat_sa_family_t af)
{
    switch (af) {
        case AF_UNSPEC:
            return "UNSPEC";
        case AF_INET:
            return "INET";
        case AF_INET6:
            return "INET6";
        case AF_LOCAL:
#ifdef CAT_OS_UNIX_LIKE
            return "UNIX";
#else
            return "LOCAL";
#endif
    }
    return "UNKNOWN";
}

CAT_API cat_bool_t cat_sockaddr_get_address(const cat_sockaddr_t *address, cat_socklen_t address_length, char *buffer, size_t *buffer_size)
{
    size_t size = *buffer_size;
    int error;

    /* for failure */
    *buffer_size = 0;
    if (size > 0) {
        buffer[0] = '\0';
    }
    switch (address->sa_family) {
        case AF_INET: {
            error = uv_ip4_name((const cat_sockaddr_in_t *) address, buffer, size);
            if (unlikely(error != 0)) {
                if (error == CAT_ENOSPC) {
                    *buffer_size = CAT_SOCKET_IPV4_BUFFER_SIZE;
                }
                break;
            }
            *buffer_size = strlen(buffer);
            return cat_true;
        }
        case AF_INET6: {
            error = uv_ip6_name((const cat_sockaddr_in6_t *) address, buffer, size);
            if (unlikely(error != 0)) {
                if (error == CAT_ENOSPC) {
                    *buffer_size = CAT_SOCKET_IPV6_BUFFER_SIZE;
                }
                break;
            }
            *buffer_size = strlen(buffer);
            return cat_true;
        }
        case AF_LOCAL: {
            const char *path = ((cat_sockaddr_local_t *) address)->sl_path;
            size_t length = address_length - offsetof(cat_sockaddr_local_t, sl_path);
            if (!cat_sockaddr_is_linux_abstract_name(path) && path[length - 1] != '\0') {
                length += 1; /* musb be zero-termination */
            }
            if (unlikely(length > size)) {
                error = CAT_ENOSPC;
                *buffer_size = length;
                break;
            }
            if (!cat_sockaddr_is_linux_abstract_name(path)) {
                length -= 1;
                buffer[length] = '\0';
            }
            if (length > 0) {
                memcpy(buffer, path, length);
            }
            *buffer_size = length;
            return cat_true;
        }
        default: {
            cat_update_last_error(CAT_EAFNOSUPPORT, "Socket address family %d is unknown", address->sa_family);
            return cat_false;
        }
    }

    cat_update_last_error_with_reason(error, "Socket convert address to name failed");
    return cat_false;
}

CAT_API int cat_sockaddr_get_port(const cat_sockaddr_t *address)
{
    switch (address->sa_family)
    {
        case AF_INET:
            return ntohs(((const cat_sockaddr_in_t *) address)->sin_port);
        case AF_INET6:
            return ntohs(((const cat_sockaddr_in6_t *) address)->sin6_port);
        case AF_LOCAL:
            return 0;
        default:
            cat_update_last_error(CAT_EAFNOSUPPORT, "Socket address family %d does not belong to INET", address->sa_family);
    }

    return -1;
}

CAT_API cat_bool_t cat_sockaddr_set_port(cat_sockaddr_t *address, int port)
{
    switch (address->sa_family)
    {
        case AF_INET:
            ((cat_sockaddr_in_t *) address)->sin_port = htons(port);
            return cat_true;
        case AF_INET6:
            ((cat_sockaddr_in6_t *) address)->sin6_port = htons(port);
            return cat_true;
        default:
            cat_update_last_error(CAT_EINVAL, "Socket address family %d does not belong to INET", address->sa_family);
    }

    return cat_false;
}

static int cat_sockaddr__getbyname(cat_sockaddr_t *address, cat_socklen_t *address_length, const char *name, size_t name_length, int port)
{
    cat_sa_family_t af = address->sa_family;
    cat_socklen_t length = *address_length;
    cat_bool_t unspec = af == AF_UNSPEC;
    int error;

    if (af == AF_LOCAL) {
        size_t real_length = name_length + !(cat_sockaddr_is_linux_abstract_name(name));

        address->sa_family = AF_LOCAL;
        if (unlikely(real_length > CAT_SOCKADDR_MAX_PATH)) {
            *address_length = 0;
            return CAT_ENAMETOOLONG;
        }
        real_length += offsetof(cat_sockaddr_t, sa_data);
        if (unlikely(real_length > length)) {
            *address_length = (cat_socklen_t) real_length;
            return CAT_ENOSPC;
        }
        if (name_length > 0) {
            memcpy(address->sa_data, name, name_length);
        }
        if (!cat_sockaddr_is_linux_abstract_name(name)) {
            address->sa_data[name_length++] = '\0';
        }
        *address_length = (cat_socklen_t) (offsetof(cat_sockaddr_local_t, sl_path) + name_length);

        return 0;
    }

    if (unspec) {
        af = AF_INET; /* try IPV4 first */
    }
    while (1) {
        if (af == AF_INET) {
            *address_length = sizeof(cat_sockaddr_in_t);
            if (unlikely(length < sizeof(cat_sockaddr_in_t))) {
                error = CAT_ENOSPC;
            } else {
                error = uv_ip4_addr(name, port, (cat_sockaddr_in_t *) address);
                if (error == CAT_EINVAL && unspec) {
                    af = AF_INET6;
                    continue; /* try IPV6 again */
                }
            }
        } else if (af == AF_INET6) {
            *address_length = sizeof(cat_sockaddr_in6_t);
            if (unlikely(length < sizeof(cat_sockaddr_in6_t))) {
                error = CAT_ENOSPC;
            } else {
                error = uv_ip6_addr(name, port, (cat_sockaddr_in6_t *) address);
            }
        } else {
            error = CAT_EAFNOSUPPORT;
        }
        break;
    }

    if (error != 0) { /* may need DNS resolve */
        *address_length = 0;
        return error;
    }
    if (unspec) {
        address->sa_family = af;
    }

    return 0;
}

CAT_API cat_bool_t cat_sockaddr_getbyname(cat_sockaddr_t *address, cat_socklen_t *address_length, const char *name, size_t name_length, int port)
{
    int error;

    error = cat_sockaddr__getbyname(address, address_length, name, name_length, port);

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket get address by name failed");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_sockaddr_to_name(const cat_sockaddr_t *address, cat_socklen_t address_length, char *name, size_t *name_length, int *port)
{
    cat_bool_t ret = cat_true;

    if (address_length > 0) {
        if (likely(name != NULL && name_length != NULL)) {
            ret = cat_sockaddr_get_address(address, address_length, name, name_length);
        }
        if (port != NULL) {
            *port = cat_sockaddr_get_port(address);
        }
    } else {
        if (likely(name != NULL && name_length != NULL)) {
            if (*name_length > 0) {
                name[0] = '\0';
            }
            *name_length = 0;
        }
        if (port != NULL) {
            *port = 0;
        }
    }

    return ret;
}

CAT_API int cat_sockaddr_copy(cat_sockaddr_t *to, cat_socklen_t *to_length, const cat_sockaddr_t *from, cat_socklen_t from_length)
{
    int error = CAT_EINVAL;

    if (to != NULL && to_length != NULL) {
        if (unlikely(*to_length < from_length)) {
            /* ENOSPC, do not copy (meaningless) */
            if (likely(*to_length >= cat_offsize_of(cat_sockaddr_t, sa_family))) {
                to->sa_family = from->sa_family;
            } // else is impossible?
            error = CAT_ENOSPC;
        } else {
            memcpy(to, from, from_length);
            error = 0;
        }
    }
    if (to_length != NULL) {
        *to_length = from_length;
    }

    return error;
}

CAT_API cat_bool_t cat_sockaddr_check(const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    cat_socklen_t min_length;

    switch (address->sa_family) {
        case AF_INET:
            min_length = sizeof(cat_sockaddr_in_t);
            break;
        case AF_INET6:
            min_length = sizeof(cat_sockaddr_in6_t);
            break;
        case AF_LOCAL:
            min_length = offsetof(cat_sockaddr_local_t, sl_path);
            break;
        default:
            cat_update_last_error(CAT_EAFNOSUPPORT, "Socket address family %d is not supported", address->sa_family);
            return cat_false;
    }
    if (unlikely(address_length < min_length)) {
        cat_update_last_error(
            CAT_EINVAL, "Socket address length is too short, "
            "at least " CAT_SOCKLEN_FMT " bytes needed, but got " CAT_SOCKLEN_FMT,
            min_length, address_length
        );
        return cat_false;
    }
    if (address->sa_family == AF_LOCAL) {
        if (
            (((char *) address)[address_length - 1]) != '\0'
#ifdef __linux__
            && address_length > offsetof(cat_sockaddr_local_t, sl_path)
            && !cat_sockaddr_is_linux_abstract_name(address->sa_data)
#endif
        ) {
            cat_update_last_error(CAT_EINVAL, "Socket local path must be zero-termination");
            return cat_false;
        }
    }

    return cat_true;
}

CAT_API CAT_GLOBALS_DECLARE(cat_socket)

CAT_GLOBALS_CTOR_DECLARE_SZ(cat_socket)

static const cat_socket_timeout_options_t cat_socket_default_global_timeout_options = {
    CAT_TIMEOUT_FOREVER,
    CAT_TIMEOUT_FOREVER,
    CAT_TIMEOUT_FOREVER,
    CAT_TIMEOUT_FOREVER,
    CAT_TIMEOUT_FOREVER,
    CAT_TIMEOUT_FOREVER,
};

CAT_STATIC_ASSERT(6 == CAT_SOCKET_TIMEOUT_OPTIONS_COUNT);

static const cat_socket_timeout_options_t cat_socket_default_timeout_options = {
    CAT_SOCKET_TIMEOUT_STORAGE_DEFAULT,
    CAT_SOCKET_TIMEOUT_STORAGE_DEFAULT,
    CAT_SOCKET_TIMEOUT_STORAGE_DEFAULT,
    CAT_SOCKET_TIMEOUT_STORAGE_DEFAULT,
    CAT_SOCKET_TIMEOUT_STORAGE_DEFAULT,
    CAT_SOCKET_TIMEOUT_STORAGE_DEFAULT,
};

CAT_STATIC_ASSERT(6 == CAT_SOCKET_TIMEOUT_OPTIONS_COUNT);

CAT_API cat_bool_t cat_socket_module_init(void)
{
    CAT_GLOBALS_REGISTER(cat_socket, CAT_GLOBALS_CTOR(cat_socket), NULL);
    return cat_true;
}

CAT_API cat_bool_t cat_socket_runtime_init(void)
{
    CAT_SOCKET_G(options.timeout) = cat_socket_default_global_timeout_options;
    CAT_SOCKET_G(options.tcp_keepalive_delay) = 60;

    return cat_true;
}

#define CAT_SOCKET_INTERNAL_GETTER_WITHOUT_ERROR(_socket, _isocket, _failure) \
    cat_socket_internal_t *_isocket = _socket->internal;\
    do { if (_isocket == NULL) { \
        _failure; \
    }} while (0)

#define CAT_SOCKET_INTERNAL_GETTER(_socket, _isocket, _failure) \
    cat_socket_internal_t *_isocket = _socket->internal;\
    do { if (_isocket == NULL) { \
        cat_update_last_error(CAT_EBADF, "Socket has been closed"); \
        _failure; \
    }} while (0)

#define CAT_SOCKET_INTERNAL_GETTER_WITH_IO(_socket, _isocket, _io_flags, _failure) \
    CAT_SOCKET_INTERNAL_GETTER(_socket, _isocket, _failure); \
    do { if (unlikely(_isocket->io_flags & _io_flags)) { \
        cat_update_last_error( \
            CAT_ELOCKED, "Socket is %s now, unable to %s", \
            cat_socket_io_state_naming(_isocket->io_flags), \
            cat_socket_io_state_name(_io_flags) \
        ); \
        _failure; \
    }} while (0)

#define CAT_SOCKET_INTERNAL_FD_GETTER(_isocket, _fd, _failure) \
    cat_socket_fd_t _fd = cat_socket_internal_get_fd(isocket); \
    do { if (unlikely(_fd == CAT_SOCKET_INVALID_FD)) { \
        _failure; \
    }} while (0)

static cat_always_inline cat_bool_t cat_socket_internal_is_established(cat_socket_internal_t *isocket)
{
    if (!(isocket->flags & CAT_SOCKET_INTERNAL_FLAG_CONNECTED)) {
        return cat_false;
    }
#ifdef CAT_SSL
    if (isocket->ssl != NULL && !cat_ssl_is_established(isocket->ssl)) {
        return cat_false;
    }
#endif

    return cat_true;
}

#define CAT_SOCKET_INTERNAL_ESTABLISHED_ONLY(_isocket, _failure) do { \
    if (unlikely(!cat_socket_internal_is_established(_isocket))) { \
        cat_update_last_error(CAT_ENOTCONN, "Socket has not been established"); \
        _failure; \
    } \
} while (0)

#define CAT_SOCKET_INTERNAL_ESTABLISHED_ONCE(_isocket, _failure) do { \
    if (cat_socket_internal_is_established(_isocket)) { \
        cat_update_last_error(CAT_EISCONN, "Socket has been established"); \
        _failure; \
    } \
} while (0)

#define CAT_SOCKET_WHICH_ONLY(_socket, _flags, _errstr, _failure) do { \
    if (!(_socket->type & (_flags))) { \
        cat_update_last_error(CAT_EMISUSE, _errstr); \
        _failure; \
    } \
} while (0)

#define CAT_SOCKET_INET_STREAM_ONLY(_socket, _failure) \
    CAT_SOCKET_WHICH_ONLY(socket, CAT_SOCKET_TYPE_FLAG_STREAM | CAT_SOCKET_TYPE_FLAG_INET, "Socket should be type of inet stream", _failure);

#define CAT_SOCKET_TCP_ONLY(_socket, _failure) \
    CAT_SOCKET_WHICH_ONLY(_socket, CAT_SOCKET_TYPE_TCP, "Socket is not of type TCP", _failure)

#define CAT_SOCKET_WHICH_SIDE_ONLY(_socket, _name, _errstr, _failure) do { \
    if (!cat_socket_is_##_name(_socket)) { \
        cat_update_last_error(CAT_EMISUSE, _errstr); \
        _failure; \
    } \
} while (0)

#define CAT_SOCKET_SERVER_ONLY(_socket, _failure) \
        CAT_SOCKET_WHICH_SIDE_ONLY(_socket, server, "Socket is not listening for connections", _failure)

#define CAT_SOCKET_CHECK_INPUT_ADDRESS(_address, _address_length, failure) do { \
    if (_address != NULL && _address_length > 0) { \
        if (unlikely(!cat_sockaddr_check(_address, _address_length))) { \
            failure; \
        } \
    } \
} while (0)

static void cat_socket_internal_close(cat_socket_internal_t *isocket);

#ifdef CAT_OS_UNIX_LIKE
static int socket_create(int domain, int type, int protocol)
{
    return socket(domain, type, protocol);
}
#endif

static cat_timeout_t cat_socket_get_dns_timeout_fast(const cat_socket_t *socket);

static cat_bool_t cat_socket_getaddrbyname_ex(cat_socket_t *socket, cat_sockaddr_info_t *address_info, const char *name, size_t name_length, int port, cat_bool_t *is_host_name)
{
    cat_sockaddr_union_t *address = &address_info->address;
    cat_sa_family_t af = cat_socket_get_af(socket);;
    int error;

    /* for failure */
    if (is_host_name != NULL) {
        *is_host_name = cat_false;
    }

    address->common.sa_family = af;
    address_info->length = sizeof(address_info->address);
    error = cat_sockaddr__getbyname(&address->common, &address_info->length, name, name_length, port);

    /* why only unspec is allowed ? */
    if ((socket->type & CAT_SOCKET_TYPE_FLAG_UNSPEC)) {
        if (error == CAT_EINVAL) {
            // try to solve the name
            struct addrinfo hints;
            struct addrinfo *response;
            int sock_type;
            int protocol;
            cat_bool_t ret;
            if (socket->type & CAT_SOCKET_TYPE_FLAG_STREAM) {
                sock_type = SOCK_STREAM;
                protocol = IPPROTO_TCP;
            } else if (socket->type & CAT_SOCKET_TYPE_FLAG_DGRAM) {
                sock_type = SOCK_DGRAM;
                protocol = IPPROTO_IP;
            } else {
                sock_type = 0;
                protocol = IPPROTO_IP;
            }
            hints.ai_family = af;
            hints.ai_socktype = sock_type;
            hints.ai_protocol = protocol;
            hints.ai_flags = 0;
            response = cat_dns_getaddrinfo_ex(name, NULL, &hints, cat_socket_get_dns_timeout_fast(socket));
            if (unlikely(response == NULL)) {
                return cat_false;
            }
            if (is_host_name != NULL) {
                *is_host_name = cat_true;
            }
            memcpy(&address->common, response->ai_addr, response->ai_addrlen);
            cat_dns_freeaddrinfo(response);
            ret = cat_sockaddr_set_port(&address->common, port);
            if (unlikely(!ret)) {
                return cat_false;
            }
            switch (address->common.sa_family)
            {
                case AF_INET:
                    address_info->length = sizeof(cat_sockaddr_in_t);
                    break;
                case AF_INET6:
                    address_info->length = sizeof(cat_sockaddr_in6_t);
                    break;
                default:
                    CAT_NEVER_HERE("Must be INET");
            }
        }
        switch (address->common.sa_family) {
            case AF_INET:
                socket->type |= CAT_SOCKET_TYPE_FLAG_IPV4;
                break;
            case AF_INET6:
                socket->type |= CAT_SOCKET_TYPE_FLAG_IPV6;
                break;
            default:
                CAT_NEVER_HERE("Unknown destination family");
        }

        return cat_true;
    }

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket get address by name failed");
        return cat_false;
    }

    return cat_true;
}

static cat_always_inline cat_bool_t cat_socket_getaddrbyname(cat_socket_t *socket, cat_sockaddr_info_t *address_info, const char *name, size_t name_length, int port)
{
    return cat_socket_getaddrbyname_ex(socket, address_info, name, name_length, port, NULL);
}

static void cat_socket_tcp_on_open(cat_socket_t *socket)
{
    cat_socket_internal_t *isocket = socket->internal;

    CAT_ASSERT(isocket != NULL);
    CAT_ASSERT((socket->type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP);

    if (!(socket->type & CAT_SOCKET_TYPE_FLAG_TCP_DELAY)) {
        /* TCP always nodelay by default */
        (void) uv_tcp_nodelay(&isocket->u.tcp, 1);
    }
    if (socket->type & CAT_SOCKET_TYPE_FLAG_TCP_KEEPALIVE) {
        (void) uv_tcp_keepalive(&isocket->u.tcp, 1, CAT_SOCKET_G(options.tcp_keepalive_delay));
    }
}

CAT_API void cat_socket_init(cat_socket_t *socket)
{
    socket->type = CAT_SOCKET_TYPE_ANY;
    socket->internal = NULL;
}

CAT_API cat_socket_t *cat_socket_create(cat_socket_t *socket, cat_socket_type_t type)
{
    return cat_socket_create_ex(socket, type, CAT_SOCKET_INVALID_FD);
}

CAT_API cat_socket_t *cat_socket_create_ex(cat_socket_t *socket, cat_socket_type_t type, cat_socket_fd_t fd)
{
    size_t isocket_size;
    cat_socket_internal_t *isocket = NULL;
    cat_sa_family_t af = 0;
    int error = CAT_EINVAL;

#ifndef CAT_DO_NOT_OPTIMIZE
    /* dynamic memory allocation so we can save some space */
    if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
        isocket_size = cat_offsize_of(cat_socket_internal_t, u.tcp);
    } else if ((type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) {
        isocket_size = cat_offsize_of(cat_socket_internal_t, u.udp);
    } else if (type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
        isocket_size = cat_offsize_of(cat_socket_internal_t, u.pipe);
    } else if ((type & CAT_SOCKET_TYPE_TTY) == CAT_SOCKET_TYPE_TTY) {
        isocket_size = cat_offsize_of(cat_socket_internal_t, u.tty);
    } else {
        goto _pre_error;
    }
    /* u must be the last member */
    CAT_STATIC_ASSERT(cat_offsize_of(cat_socket_internal_t, u) == sizeof(*isocket));
#else
    isocket_size = sizeof(*isocket);
#endif
    isocket = (cat_socket_internal_t *) cat_malloc(isocket_size + (socket == NULL ? sizeof(*socket) : 0));
    if (unlikely(isocket == NULL)) {
        cat_update_last_error_of_syscall("Malloc for socket failed");
        goto _syscall_error;
    }
    if (socket == NULL) {
        socket = (cat_socket_t *) ((char *) isocket + isocket_size);
    }

    /* solve type and get af */
    if (type & CAT_SOCKET_TYPE_FLAG_SERVER) {
        type &= ~CAT_SOCKET_TYPE_FLAGS_DO_NOT_EXTENDS;
        type |= CAT_SOCKET_TYPE_FLAG_SESSION;
        /* Notice: use AF_UNSPEC to make sure that
         * socket will not be created for now
         * (it should be created when accept) */
        af = AF_UNSPEC;
    } else {
        type &= ~CAT_SOCKET_TYPE_FLAGS_DO_NOT_EXTENDS;
        if (type & CAT_SOCKET_TYPE_FLAG_UNSPEC) {
            af = AF_UNSPEC;
        } else if (type & CAT_SOCKET_TYPE_FLAG_IPV4) {
            af = AF_INET;
        } else if (type & CAT_SOCKET_TYPE_FLAG_IPV6) {
            af = AF_INET6;
        } else {
            af = AF_UNSPEC;
            if (type & CAT_SOCKET_TYPE_FLAG_INET) {
                type |= CAT_SOCKET_TYPE_FLAG_UNSPEC;
            }
        }
    }
    /* init handler */
    if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
        error = uv_tcp_init_ex(cat_event_loop, &isocket->u.tcp, af);
    } else if ((type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) {
        error = uv_udp_init_ex(cat_event_loop, &isocket->u.udp, af);
    } else if (type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
#ifdef CAT_OS_UNIX_LIKE
        if ((type & CAT_SOCKET_TYPE_UDG) == CAT_SOCKET_TYPE_UDG) {
            fd = socket_create(AF_UNIX, SOCK_DGRAM, IPPROTO_IP);
            if (unlikely(fd < 0)) {
                error = cat_translate_sys_error(errno);
                goto _error;
            }
            type &= ~CAT_SOCKET_TYPE_FLAG_IPC;
        }
        else
#endif
        if (unlikely((type & CAT_SOCKET_TYPE_PIPE) != CAT_SOCKET_TYPE_PIPE)) {
            error = CAT_ENOTSUP;
            goto _error;
        }
        error = uv_pipe_init(cat_event_loop, &isocket->u.pipe, !!(type & CAT_SOCKET_TYPE_FLAG_IPC));
    } else if ((type & CAT_SOCKET_TYPE_TTY) == CAT_SOCKET_TYPE_TTY) {
        /* convert SOCKET to int on Windows */
        cat_os_fd_t os_fd = (cat_os_fd_t) fd;
        if (fd == CAT_SOCKET_INVALID_FD) {
            if (type & CAT_SOCKET_TYPE_FLAG_STDIN) {
                os_fd = STDIN_FILENO;
            } else if (type & CAT_SOCKET_TYPE_FLAG_STDOUT) {
                os_fd = STDOUT_FILENO;
            } else if (type & CAT_SOCKET_TYPE_FLAG_STDERR) {
                os_fd = STDERR_FILENO;
            }
        }
        type &= ~(CAT_SOCKET_TYPE_FLAG_STDIN | CAT_SOCKET_TYPE_FLAG_STDOUT | CAT_SOCKET_TYPE_FLAG_STDERR);
        if (os_fd == STDIN_FILENO) {
            type |= CAT_SOCKET_TYPE_FLAG_STDIN;
        } else if (os_fd == STDOUT_FILENO) {
            type |= CAT_SOCKET_TYPE_FLAG_STDOUT;
        } else if (os_fd == STDERR_FILENO) {
            type |= CAT_SOCKET_TYPE_FLAG_STDERR;
        }
        error = uv_tty_init(cat_event_loop, &isocket->u.tty, os_fd, 0);
    }
    if (unlikely(error != 0)) {
        goto _error;
    }
    if (fd != CAT_SOCKET_INVALID_FD) {
        if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
            error = uv_tcp_open(&isocket->u.tcp, fd);
        } else if ((type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) {
            error = uv_udp_open(&isocket->u.udp, fd);
        } else if (type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
            /* convert SOCKET to int on Windows */
            error = uv_pipe_open(&isocket->u.pipe, (cat_os_fd_t) fd);
        } else if ((type & CAT_SOCKET_TYPE_TTY) == CAT_SOCKET_TYPE_TTY) {
            error = 0;
        } else {
            error = CAT_EINVAL;
        }
    }
    if (unlikely(error != 0)) {
        goto _ext_error;
    }

    /* init properties of socket */
    socket->type = type;
    socket->internal = isocket;

    /* init properties of socket internal */
    isocket->u.socket = socket;
    isocket->flags = CAT_SOCKET_INTERNAL_FLAG_NONE;
    isocket->io_flags = CAT_SOCKET_IO_FLAG_NONE;
    memset(&isocket->context.io.read, 0, sizeof(isocket->context.io.read));
    cat_queue_init(&isocket->context.io.write.coroutines);
    /* part of cache */
    isocket->cache.sockname = NULL;
    isocket->cache.peername = NULL;
    isocket->cache.recv_buffer_size = -1;
    isocket->cache.send_buffer_size = -1;
    /* options */
    isocket->options.timeout = cat_socket_default_timeout_options;
#ifdef CAT_SSL
    isocket->ssl = NULL;
    isocket->ssl_peer_name = NULL;
#endif

    if (fd != CAT_SOCKET_INVALID_FD) {
        if (
            ((type & CAT_SOCKET_TYPE_TTY) == CAT_SOCKET_TYPE_TTY) ||
            cat_socket_getpeername_fast(socket) != NULL
        ) {
            isocket->flags |= CAT_SOCKET_INTERNAL_FLAG_CONNECTED;
        }
        if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
            cat_socket_tcp_on_open(socket);
        }
    }

    return socket;

    _ext_error:
    uv_close(&isocket->u.handle, NULL);
    _error:
    cat_free(isocket);
    _pre_error:
    cat_update_last_error_with_reason(error, "Socket init with type %s failed", cat_socket_type_name(type));
    _syscall_error:

    return NULL;
}

CAT_API const char *cat_socket_type_name(cat_socket_type_t type)
{
    if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
        if (type & CAT_SOCKET_TYPE_FLAG_IPV4) {
            return "TCP4";
        } else if (type & CAT_SOCKET_TYPE_FLAG_IPV6) {
            return "TCP6";
        } else {
            return "TCP";
        }
    } else if ((type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) {
        if (type & CAT_SOCKET_TYPE_FLAG_IPV4) {
            return "UDP4";
        } else if (type & CAT_SOCKET_TYPE_FLAG_IPV6) {
            return "UDP6";
        } else {
            return "UDP";
        }
    } else if ((type & CAT_SOCKET_TYPE_PIPE) == CAT_SOCKET_TYPE_PIPE) {
#ifdef CAT_OS_UNIX_LIKE
        return "UNIX";
#else
        return "PIPE";
#endif
    } else if ((type & CAT_SOCKET_TYPE_TTY) == CAT_SOCKET_TYPE_TTY) {
        if (type & CAT_SOCKET_TYPE_FLAG_STDIN) {
            return "STDIN";
        } else if (type & CAT_SOCKET_TYPE_FLAG_STDOUT) {
            return "STDOUT";
        } else if (type & CAT_SOCKET_TYPE_FLAG_STDERR) {
            return "STDERR";
        } else {
            return "TTY";
        }
    }
#ifdef CAT_OS_UNIX_LIKE
    else if ((type & CAT_SOCKET_TYPE_UDG) == CAT_SOCKET_TYPE_UDG) {
       return "UDG";
    }
#endif

    return "UNKNOWN";
}

CAT_API const char *cat_socket_get_type_name(const cat_socket_t *socket)
{
    return cat_socket_type_name(socket->type);
}

CAT_API cat_sa_family_t cat_socket_get_af_of_type(cat_socket_type_t type)
{
    if (type & CAT_SOCKET_TYPE_FLAG_IPV4) {
        return AF_INET;
    } else if (type & CAT_SOCKET_TYPE_FLAG_IPV6) {
        return AF_INET6;
    } else if (type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
        return AF_LOCAL;
    } else {
        return AF_UNSPEC;
    }
}

CAT_API cat_sa_family_t cat_socket_get_af(const cat_socket_t *socket)
{
    return cat_socket_get_af_of_type(socket->type);
}

static cat_socket_fd_t cat_socket_internal_get_fd_fast(const cat_socket_internal_t *isocket)
{
    cat_os_handle_t fd = CAT_OS_INVALID_HANDLE;

    (void) uv_fileno(&isocket->u.handle, &fd);

    return (cat_socket_fd_t) fd;
}

CAT_API cat_socket_fd_t cat_socket_get_fd_fast(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER_WITHOUT_ERROR(socket, isocket, return CAT_SOCKET_INVALID_FD);

    return cat_socket_internal_get_fd_fast(isocket);
}

static cat_socket_fd_t cat_socket_internal_get_fd(const cat_socket_internal_t *isocket)
{
    cat_os_handle_t fd = CAT_OS_INVALID_HANDLE;
    int error;

    error = uv_fileno(&isocket->u.handle, &fd);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket get fd failed");
    }

    return (cat_socket_fd_t) fd;
}

CAT_API cat_socket_fd_t cat_socket_get_fd(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return CAT_SOCKET_INVALID_FD);

    return cat_socket_internal_get_fd(isocket);
}

static cat_always_inline cat_socket_timeout_storage_t cat_socket_align_global_timeout(cat_timeout_t timeout)
{
    if (unlikely(timeout < CAT_SOCKET_TIMEOUT_STORAGE_MIN || timeout > CAT_SOCKET_TIMEOUT_STORAGE_MAX)) {
        return -1;
    }

    return (cat_socket_timeout_storage_t) timeout;
}

static cat_always_inline cat_socket_timeout_storage_t cat_socket_align_timeout(cat_timeout_t timeout)
{
    if (unlikely(
        (timeout < CAT_SOCKET_TIMEOUT_STORAGE_MIN && timeout != CAT_SOCKET_TIMEOUT_STORAGE_DEFAULT) ||
        timeout > CAT_SOCKET_TIMEOUT_STORAGE_MAX)) {
        return -1;
    }

    return (cat_socket_timeout_storage_t) timeout;
}

static void cat_socket_set_timeout_to_options(cat_socket_timeout_options_t *options, cat_timeout_t timeout)
{
    cat_socket_timeout_storage_t timeout_storage = cat_socket_align_timeout(timeout);

    options->dns = timeout_storage;
    /* do not set accept timeout, we usually expect it is -1 */
    /* options->accept = timeout_storage; */
    options->connect = timeout_storage;
    options->handshake = timeout_storage;
    options->read = timeout_storage;
    options->write = timeout_storage;
}

CAT_API void cat_socket_set_global_timeout(cat_timeout_t timeout)
{
    cat_socket_set_timeout_to_options(&CAT_SOCKET_G(options.timeout), timeout);
}

CAT_API cat_bool_t cat_socket_set_timeout(cat_socket_t *socket, cat_timeout_t timeout)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);

    cat_socket_set_timeout_to_options(&isocket->options.timeout, timeout);

    return cat_true;
}

#define CAT_SOCKET_TIMEOUT_API_GEN(type) \
\
CAT_API cat_timeout_t cat_socket_get_global_##type##_timeout(void) \
{ \
    return CAT_SOCKET_G(options.timeout.type); \
} \
\
CAT_API void cat_socket_set_global_##type##_timeout(cat_timeout_t timeout) \
{ \
    CAT_SOCKET_G(options.timeout.type) = cat_socket_align_global_timeout(timeout); \
} \
\
static cat_always_inline CAT_ATTRIBUTE_UNUSED cat_timeout_t cat_socket_internal_get_##type##_timeout(const cat_socket_internal_t *isocket) \
{ \
    if (isocket->options.timeout.type == CAT_SOCKET_TIMEOUT_STORAGE_DEFAULT) { \
        return cat_socket_get_global_##type##_timeout(); \
    } \
    \
    return isocket->options.timeout.type; \
} \
\
static cat_always_inline CAT_ATTRIBUTE_UNUSED cat_timeout_t cat_socket_get_##type##_timeout_fast(const cat_socket_t *socket) \
{ \
    CAT_SOCKET_INTERNAL_GETTER_WITHOUT_ERROR(socket, isocket, return CAT_TIMEOUT_INVALID); \
    \
    return cat_socket_internal_get_##type##_timeout(isocket); \
} \
\
CAT_API cat_timeout_t cat_socket_get_##type##_timeout(const cat_socket_t *socket) \
{ \
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return CAT_TIMEOUT_INVALID); \
    \
    return cat_socket_internal_get_##type##_timeout(isocket); \
} \
\
CAT_API cat_bool_t cat_socket_set_##type##_timeout(cat_socket_t *socket, cat_timeout_t timeout) \
{ \
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false); \
    \
    isocket->options.timeout.type = cat_socket_align_timeout(timeout); \
    \
    return cat_true; \
}

CAT_SOCKET_TIMEOUT_API_GEN(dns)
CAT_SOCKET_TIMEOUT_API_GEN(accept)
CAT_SOCKET_TIMEOUT_API_GEN(connect)
CAT_SOCKET_TIMEOUT_API_GEN(handshake)
CAT_SOCKET_TIMEOUT_API_GEN(read)
CAT_SOCKET_TIMEOUT_API_GEN(write)

#undef CAT_SOCKET_TIMEOUT_API_GEN

static cat_bool_t cat_socket__bind(
    cat_socket_t *socket, cat_socket_internal_t *isocket,
    const cat_sockaddr_t *address, cat_socklen_t address_length,
    cat_socket_bind_flags_t flags
)
{
    CAT_SOCKET_CHECK_INPUT_ADDRESS(address, address_length, return cat_false);
    cat_socket_type_t type = socket->type;
    int error = CAT_EINVAL;

    if (!(type & CAT_SOCKET_TYPE_FLAG_LOCAL)) {
        int uv_flags = 0;
        /* check flags */
        if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
            if (flags & CAT_SOCKET_BIND_FLAG_IPV6ONLY) {
                flags ^= CAT_SOCKET_BIND_FLAG_IPV6ONLY;
                uv_flags |= UV_TCP_IPV6ONLY;
            }
            if (flags & CAT_SOCKET_BIND_FLAG_REUSEADDR) {
                flags ^= CAT_SOCKET_BIND_FLAG_REUSEADDR;
                /* enable by default */
            }
            if (flags & CAT_SOCKET_BIND_FLAG_REUSEPORT) {
                flags ^= CAT_SOCKET_BIND_FLAG_REUSEPORT;
                uv_flags |= UV_TCP_REUSEPORT;
            }
            if (flags == CAT_SOCKET_BIND_FLAG_NONE) {
                error = uv_tcp_bind(&isocket->u.tcp, address, uv_flags);
            }
        } else if ((type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) {
            if (flags & CAT_SOCKET_BIND_FLAG_IPV6ONLY) {
                flags ^= CAT_SOCKET_BIND_FLAG_IPV6ONLY;
                uv_flags |= UV_UDP_IPV6ONLY;
            }
            if (flags & CAT_SOCKET_BIND_FLAG_REUSEADDR) {
                flags ^= CAT_SOCKET_BIND_FLAG_REUSEADDR;
                uv_flags |= UV_UDP_REUSEADDR;
            }
            if (flags & CAT_SOCKET_BIND_FLAG_REUSEPORT) {
                flags ^= CAT_SOCKET_BIND_FLAG_REUSEPORT;
                uv_flags |= UV_UDP_REUSEPORT;
            }
            if (flags == CAT_SOCKET_BIND_FLAG_NONE) {
                error = uv_udp_bind(&isocket->u.udp, address, uv_flags);
            }
        } else {
            error = CAT_EAFNOSUPPORT;
        }
    } else {
        if (flags == CAT_SOCKET_BIND_FLAG_NONE) {
            address_length -= offsetof(cat_sockaddr_t, sa_data);
            if (address_length > 0 && !cat_sockaddr_is_linux_abstract_name(address->sa_data)) {
                address_length -= 1;
            }
            error = uv_pipe_bind_ex(&isocket->u.pipe, address->sa_data, address_length);
        }
    }
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket bind failed");
        return cat_false;
    }
    /* bind done successfully, we can do something here before transfer data */
    if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
        cat_socket_tcp_on_open(socket);
    }
    /* clear previous cache (maybe impossible here) */
    if (unlikely(isocket->cache.sockname)) {
        cat_free(isocket->cache.sockname);
        isocket->cache.sockname = NULL;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_socket_bind(cat_socket_t *socket, const char *name, size_t name_length, int port)
{
    return cat_socket_bind_ex(socket, name, name_length, port, CAT_SOCKET_BIND_FLAG_NONE);
}

CAT_API cat_bool_t cat_socket_bind_ex(cat_socket_t *socket, const char *name, size_t name_length, int port, cat_socket_bind_flags_t flags)
{
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(socket, isocket, CAT_SOCKET_IO_FLAG_BIND, return cat_false);
    cat_sockaddr_info_t address_info;
    cat_sockaddr_t *address;
    cat_socklen_t address_length;

    /* resolve address (DNS query may be triggered) */
    do {
        cat_bool_t ret;
        isocket->context.bind.coroutine = CAT_COROUTINE_G(current);
        isocket->io_flags = CAT_SOCKET_IO_FLAG_BIND;
        ret  = cat_socket_getaddrbyname(socket, &address_info, name, name_length, port);
        isocket->io_flags = CAT_SOCKET_IO_FLAG_NONE;
        isocket->context.bind.coroutine = NULL;
        if (unlikely(!ret)) {
           cat_update_last_error_with_previous("Socket bind failed");
           return cat_false;
        }
        address = &address_info.address.common;
        address_length = address_info.length;
    } while (0);

    return cat_socket__bind(socket, isocket, address, address_length, flags);
}

CAT_API cat_bool_t cat_socket_bind_to(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    return cat_socket_bind_to_ex(socket, address, address_length, CAT_SOCKET_BIND_FLAG_NONE);
}

CAT_API cat_bool_t cat_socket_bind_to_ex(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_socket_bind_flags_t flags)
{
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(socket, isocket, CAT_SOCKET_IO_FLAG_BIND, return cat_false);

    return cat_socket__bind(socket, isocket, address, address_length, flags);
}

static void cat_socket_accept_connection_callback(uv_stream_t *stream, int status)
{
    cat_socket_internal_t *iserver = cat_container_of(stream, cat_socket_internal_t, u.stream);

    if (iserver->io_flags == CAT_SOCKET_IO_FLAG_ACCEPT) {
        cat_coroutine_t *coroutine = iserver->context.accept.coroutine;
        CAT_ASSERT(coroutine != NULL);
        iserver->context.accept.data.status = status;
        if (unlikely(!cat_coroutine_resume(coroutine, NULL, NULL))) {
            cat_core_error_with_last(SOCKET, "Connect schedule failed");
        }
    }
    // else we can call uv_accept to get it later
}

CAT_API cat_bool_t cat_socket_listen(cat_socket_t *socket, int backlog)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);
    int error;

    error = uv_listen(&isocket->u.stream, backlog, cat_socket_accept_connection_callback);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket listen(%d) failed", backlog);
        return cat_false;
    }
    uv_unref(&isocket->u.handle);
    socket->type |= CAT_SOCKET_TYPE_FLAG_SERVER;

    return cat_true;
}

CAT_API cat_socket_t *cat_socket_accept(cat_socket_t *server, cat_socket_t *client)
{
    return cat_socket_accept_ex(server, client, cat_socket_get_accept_timeout_fast(server));
}

CAT_API cat_socket_t *cat_socket_accept_ex(cat_socket_t *server, cat_socket_t *client, cat_timeout_t timeout)
{
    CAT_SOCKET_SERVER_ONLY(server, return NULL);
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(server, iserver, CAT_SOCKET_IO_FLAG_ACCEPT, return NULL);
    int error;

    client = cat_socket_create(client, server->type);
    if (unlikely(client == NULL)) {
        cat_update_last_error_with_previous("Socket create for accepting connection failed");
        return NULL;
    }

    while (1) {
        cat_bool_t ret;
        error = uv_accept(&iserver->u.stream, &client->internal->u.stream);
        if (error == 0) {
            cat_socket_internal_t *iclient = client->internal;
            /* init client properties */
            iclient->flags |= CAT_SOCKET_INTERNAL_FLAG_CONNECTED;
            /* TODO: socket_extends() ? */
            memcpy(&iclient->options, &iserver->options, sizeof(iclient->options));
            /* TODO: socket_on_open() ? */
            if ((client->type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
                cat_socket_tcp_on_open(client);
            }
            return client;
        }
        if (unlikely(error != CAT_EAGAIN)) {
            cat_update_last_error_with_reason(error, "Socket accept failed");
            break;
        }
        uv_ref(&iserver->u.handle);
        iserver->context.accept.data.status = CAT_ECANCELED;
        iserver->context.accept.coroutine = CAT_COROUTINE_G(current);
        iserver->io_flags = CAT_SOCKET_IO_FLAG_ACCEPT;
        ret = cat_time_wait(timeout);
        iserver->io_flags = CAT_SOCKET_IO_FLAG_NONE;
        iserver->context.accept.coroutine = NULL;
        uv_unref(&iserver->u.handle);
        if (unlikely(!ret)) {
            cat_update_last_error_with_previous("Socket accept wait failed");
            break;
        }
        if (unlikely(iserver->context.accept.data.status == CAT_ECANCELED)) {
            cat_update_last_error(CAT_ECANCELED, "Socket accept has been canceled");
            break;
        }
    }

    /* failed case */
    cat_socket_close(client);

    return NULL;
}

static void cat_socket_connect_callback(uv_connect_t* request, int status)
{
    cat_socket_internal_t *isocket = cat_container_of(request->handle, cat_socket_internal_t, u.stream);

    /* if status == ECANCELED means that socket is closed (now in uv__stream_destroy())
     * and handle->close_cb() always after the uv__stream_destroy() */
    if (isocket->io_flags == CAT_SOCKET_IO_FLAG_CONNECT) {
        cat_coroutine_t *coroutine = isocket->context.connect.coroutine;
        CAT_ASSERT(coroutine != NULL);
        isocket->context.connect.data.status = status;
        if (unlikely(!cat_coroutine_resume(coroutine, NULL, NULL))) {
            cat_core_error_with_last(SOCKET, "Connect schedule failed");
        }
    }

    cat_free(request);
}

static cat_bool_t cat_socket__connect(
    cat_socket_t *socket, cat_socket_internal_t *isocket,
    const cat_sockaddr_t *address, cat_socklen_t address_length,
    cat_timeout_t timeout
)
{
    CAT_SOCKET_CHECK_INPUT_ADDRESS(address, address_length, return cat_false);
    cat_socket_type_t type = socket->type;
    uv_connect_t *request;
    int error;

    /* only TCP and PIPE need request */
    if (((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) || (type & CAT_SOCKET_TYPE_FLAG_LOCAL)) {
        /* malloc for request (we must free it in the callback if it has been started) */
        request = (uv_connect_t *) cat_malloc(sizeof(*request));
        if (unlikely(request == NULL)) {
            cat_update_last_error_of_syscall("Malloc for Socket connect request failed");
            return cat_false;
        }
    } else {
        request = NULL;
    }
    if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
        error = uv_tcp_connect(request, &isocket->u.tcp, address, cat_socket_connect_callback);
        if (unlikely(error != 0)) {
            cat_update_last_error_with_reason(error, "Tcp connect init failed");
            cat_free(request);
            return cat_false;
        }
    } else if ((type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) {
        error = uv_udp_connect(&isocket->u.udp, address);
    } else if (type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
        address_length -= offsetof(cat_sockaddr_t, sa_data);
        if (address_length > 0 && !cat_sockaddr_is_linux_abstract_name(address->sa_data)) {
            address_length -= 1;
        }
        (void) uv_pipe_connect_ex(request, &isocket->u.pipe, address->sa_data, address_length, cat_socket_connect_callback);
    } else {
        error = CAT_EAFNOSUPPORT;
    }
    if (request != NULL) {
        cat_bool_t ret;
        isocket->context.connect.data.status = CAT_ECANCELED;
        isocket->context.connect.coroutine = CAT_COROUTINE_G(current);
        isocket->io_flags = CAT_SOCKET_IO_FLAG_CONNECT;
        ret = cat_time_wait(timeout);
        isocket->io_flags = CAT_SOCKET_IO_FLAG_NONE;
        isocket->context.connect.coroutine = NULL;
        if (unlikely(!ret)) {
            cat_update_last_error_with_previous("Socket connect wait failed");
            /* interrupt can not recover */
            cat_socket_internal_close(isocket);
            return cat_false;
        }
        error = isocket->context.connect.data.status;
        if (error == CAT_ECANCELED) {
            cat_update_last_error(CAT_ECANCELED, "Socket connect has been canceled");
            /* intterupt can not recover */
            cat_socket_internal_close(isocket);
            return cat_false;
        }
    }
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket connect failed");
        return cat_false;
    }
    /* connect done successfully, we can do something here before transfer data */
    socket->type |= CAT_SOCKET_TYPE_FLAG_CLIENT;
    isocket->flags |= CAT_SOCKET_INTERNAL_FLAG_CONNECTED;
    if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP){
        cat_socket_tcp_on_open(socket);
    }
    /* clear previous cache (maybe impossible here) */
    if (unlikely(isocket->cache.peername)) {
        cat_free(isocket->cache.peername);
        isocket->cache.peername = NULL;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_socket_connect(cat_socket_t *socket, const char *name, size_t name_length, int port)
{
    return cat_socket_connect_ex(socket, name, name_length, port, cat_socket_get_connect_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_connect_ex(cat_socket_t *socket, const char *name, size_t name_length, int port, cat_timeout_t timeout)
{
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(socket, isocket, CAT_SOCKET_IO_FLAG_CONNECT, return cat_false);
    CAT_SOCKET_INTERNAL_ESTABLISHED_ONCE(isocket, return cat_false);
    cat_sockaddr_info_t address_info;
    cat_sockaddr_t *address;
    cat_socklen_t address_length;
    cat_bool_t is_host_name;
    cat_bool_t ret;

    /* resolve address (DNS query may be triggered) */
    /* Notice: address can not be NULL if it's not UDP */
    if (!(name_length == 0 && ((socket->type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP))) {
        cat_bool_t ret;
        isocket->context.connect.coroutine = CAT_COROUTINE_G(current);
        isocket->io_flags = CAT_SOCKET_IO_FLAG_CONNECT;
        ret  = cat_socket_getaddrbyname_ex(socket, &address_info, name, name_length, port, &is_host_name);
        isocket->io_flags = CAT_SOCKET_IO_FLAG_NONE;
        isocket->context.connect.coroutine = NULL;
        if (unlikely(!ret)) {
           cat_update_last_error_with_previous("Socket connect failed");
           return cat_false;
        }
        address = &address_info.address.common;
        address_length = address_info.length;
    } else {
        address = NULL;
        address_length = 0;
    }

    ret = cat_socket__connect(socket, isocket, address, address_length, timeout);

#ifdef CAT_SSL
    if (ret && is_host_name) {
        isocket->ssl_peer_name = cat_strndup(name, name_length);
    }
#endif

    return ret;
}

CAT_API cat_bool_t cat_socket_connect_to(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    return cat_socket_connect_to_ex(socket, address, address_length, cat_socket_get_connect_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_connect_to_ex(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout)
{
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(socket, isocket, CAT_SOCKET_IO_FLAG_CONNECT, return cat_false);
    CAT_SOCKET_INTERNAL_ESTABLISHED_ONCE(isocket, return cat_false);

    return cat_socket__connect(socket, isocket, address, address_length, timeout);
}

#ifdef CAT_SSL
CAT_API void cat_socket_crypto_options_init(cat_socket_crypto_options_t *options)
{
    cat_const_string_init(&options->peer_name);
    options->verify_peer = cat_true;
    options->verify_peer_name = cat_true;
    options->allow_self_signed = cat_false;
    cat_const_string_init(&options->passphrase);
}

CAT_API cat_bool_t cat_socket_enable_crypto(cat_socket_t *socket, cat_socket_crypto_context_t *context, const cat_socket_crypto_options_t *options)
{
    return cat_socket_enable_crypto_ex(socket, context, options, cat_socket_get_handshake_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_enable_crypto_ex(cat_socket_t *socket, cat_socket_crypto_context_t *context, const cat_socket_crypto_options_t *options, cat_timeout_t timeout)
{
    /* TODO: DTLS support */
    CAT_SOCKET_INET_STREAM_ONLY(socket, return cat_false);
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(socket, isocket, CAT_SOCKET_IO_FLAG_RDWR, return cat_false);
    if (unlikely(isocket->ssl != NULL)) {
        CAT_SOCKET_INTERNAL_ESTABLISHED_ONCE(isocket, return cat_false);
    } else {
        CAT_SOCKET_INTERNAL_ESTABLISHED_ONLY(isocket, return cat_false);
    }
    cat_ssl_t *ssl;
    cat_buffer_t *buffer;
    cat_socket_crypto_options_t ioptions;
    cat_bool_t is_client = cat_socket_is_client(socket);
    cat_bool_t use_tmp_context;
    cat_bool_t ret = cat_false;

    CAT_ASSERT(isocket->ssl == NULL);

    /* check options */
    if (options == NULL) {
        cat_socket_crypto_options_init(&ioptions);
    } else {
        ioptions = *options;
    }

    /* check context */
    use_tmp_context = context == NULL;
    if (use_tmp_context) {
        cat_ssl_method_t method;
        if (socket->type & CAT_SOCKET_TYPE_FLAG_STREAM) {
            method = CAT_SSL_METHOD_TLS;
        } else if (socket->type & CAT_SOCKET_TYPE_FLAG_DGRAM)  {
            method = CAT_SSL_METHOD_DTLS;
        } else {
            cat_update_last_error(CAT_ESSL, "Socket type is not supported by SSL");
            goto _create_error;
        }
        context = cat_ssl_context_create(method);
        if (unlikely(context == NULL)) {
            goto _create_error;
        }
    }

#ifndef CAT_OS_WIN
    /* check context related options */
    if (is_client && ioptions.verify_peer) {
        (void) cat_ssl_context_set_default_verify_paths(context);
    }
#else
    cat_ssl_context_configure_verify(context);
#endif

    /* create ssl connection */
    ssl = cat_ssl_create(NULL, context);
    if (use_tmp_context) {
        /* deref/free context */
        cat_ssl_context_close(context);
    }
    if (unlikely(ssl == NULL)) {
        goto _create_error;
    }

    /* set state */
    if (!is_client) {
        cat_ssl_set_accept_state(ssl);
    } else {
        cat_ssl_set_connect_state(ssl);
    }

    /* connection related options */
    if (ioptions.peer_name.length == 0 && isocket->ssl_peer_name != NULL) {
        ioptions.peer_name.data = isocket->ssl_peer_name;
        ioptions.peer_name.length = strlen(isocket->ssl_peer_name);
    }
    if (is_client && ioptions.peer_name.length > 0) {
        CAT_ASSERT(ioptions.peer_name.data[ioptions.peer_name.length] == '\0');
        cat_ssl_set_sni_server_name(ssl, ioptions.peer_name.data);
    }
    ssl->allow_self_signed = ioptions.allow_self_signed;
    if (ioptions.passphrase.length > 0) {
        if (unlikely(!cat_ssl_set_passphrase(ssl, ioptions.passphrase.data, ioptions.passphrase.length))) {
            goto _set_options_error;
        }
    }

    buffer = &ssl->read_buffer;

    while (1) {
        ssize_t n;
        cat_ssl_ret_t ssl_ret;

        ssl_ret = cat_ssl_handshake(ssl);
        if (unlikely(ssl_ret == CAT_SSL_RET_ERROR)) {
            break;
        }
        n = cat_ssl_read_encrypted_bytes(ssl, buffer->value, buffer->size);
        if (unlikely(n == CAT_RET_ERROR)) {
            break;
        }
        if (n > 0) {
            cat_bool_t ret;
            CAT_TIME_WAIT_START() {
                ret = cat_socket_send_ex(socket, buffer->value, n, timeout);
            } CAT_TIME_WAIT_END(timeout);
            if (unlikely(!ret)) {
                break;
            }
            continue;
        }
        if (ssl_ret == CAT_SSL_RET_OK) {
            cat_debug(SOCKET, "SSL handshake completed");
            ret = cat_true;
            break;
        }
        CAT_ASSERT(n == CAT_RET_NONE); {
            ssize_t nread, nwritten;
            CAT_TIME_WAIT_START() {
                nread = cat_socket_recv_ex(socket, buffer->value, buffer->size, timeout);
            } CAT_TIME_WAIT_END(timeout);
            if (unlikely(nread <= 0)) {
                break;
            }
            nwritten = cat_ssl_write_encrypted_bytes(ssl, buffer->value, nread);
            if (unlikely(nwritten != nread)) {
                break;
            }
            continue;
        }
    }

    if (unlikely(!ret)) {
        /* Notice: io error can not recover */
        goto _io_error;
    }

    if (ioptions.verify_peer) {
        if (!cat_ssl_verify_peer(ssl, ioptions.allow_self_signed)) {
            goto _verify_error;
        }
    }
    if (ioptions.verify_peer_name) {
        if (ioptions.peer_name.length == 0) {
            cat_update_last_error(CAT_EINVAL, "SSL verify peer is enabled but peer name is empty");
            goto _verify_error;
        }
        if (!cat_ssl_check_host(ssl, ioptions.peer_name.data, ioptions.peer_name.length)) {
            goto _verify_error;
        }
    }

    isocket->ssl = ssl;

    return cat_true;

    _verify_error:
    _io_error:
    cat_socket_internal_close(isocket);
    _set_options_error:
    cat_ssl_close(ssl);
    _create_error:
    cat_update_last_error_with_previous("Socket enable crypto failed");

    return cat_false;
}
#endif

CAT_API cat_bool_t cat_socket_getname(const cat_socket_t *socket, cat_sockaddr_t *address, cat_socklen_t *address_length, cat_bool_t is_peer)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);
    cat_socket_type_t type = socket->type;
    cat_sockaddr_info_t *cache = !is_peer ? isocket->cache.sockname : isocket->cache.peername;
    int error;

    if (cache != NULL) {
        /* use cache */
        error = cat_sockaddr_copy(address, address_length, &cache->address.common, cache->length);
    } else {
        error = CAT_EAFNOSUPPORT;
        if (type & CAT_SOCKET_TYPE_FLAG_INET) {
            int length = *address_length; /* sizeof(cat_socklen_t) is not always equal to sizeof(int) */
            if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP)  {
                if (!is_peer) {
                    error = uv_tcp_getsockname(&isocket->u.tcp, address, &length);
                } else {
                    error = uv_tcp_getpeername(&isocket->u.tcp, address, &length);
                }
            } else if ((type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) {
                if (!is_peer) {
                    error = uv_udp_getsockname(&isocket->u.udp, address, &length);
                } else {
                    error = uv_udp_getpeername(&isocket->u.udp, address, &length);
                }
            }
            *address_length = length;
        } else if (type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
            size_t length = *address_length;
            const size_t offset = offsetof(cat_sockaddr_local_t, sl_path);
            char *buffer = ((cat_sockaddr_local_t *) address)->sl_path;
            if (length >= offset) {
                length -= offset;
            }
            address->sa_family = AF_LOCAL;
            /* Notice: uv_pipe_getname has already considered zero-termination (so we nned not `length += 1` ) */
            if (!is_peer) {
                error = uv_pipe_getsockname(&isocket->u.pipe, buffer, &length);
            } else {
                error = uv_pipe_getpeername(&isocket->u.pipe, buffer, &length);
            }
            length += offset;
            if (error == 0 && !cat_sockaddr_is_linux_abstract_name(buffer)) {
                length += 1;
            }
            *address_length = (cat_socklen_t) length;
        }
    }
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket get%sname failed", !is_peer ? "sock" : "peer");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_socket_getsockname(const cat_socket_t *socket, cat_sockaddr_t *address, cat_socklen_t *length)
{
    return cat_socket_getname(socket, address, length, cat_false);
}

CAT_API cat_bool_t cat_socket_getpeername(const cat_socket_t *socket, cat_sockaddr_t *address, cat_socklen_t *length)
{
    return cat_socket_getname(socket, address, length, cat_true);
}

CAT_API const cat_sockaddr_info_t *cat_socket_getname_fast(cat_socket_t *socket, cat_bool_t is_peer)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return NULL);
    cat_sockaddr_info_t *cache, **cache_ptr;
    cat_sockaddr_info_t tmp;
    size_t size;
    cat_bool_t ret;

    if (!is_peer) {
        cache_ptr = &isocket->cache.sockname;
    } else {
        cache_ptr = &isocket->cache.peername;
    }
    cache = *cache_ptr;
    if (cache != NULL) {
        return cache;
    }
    tmp.length = sizeof(tmp.address);
    ret = cat_socket_getname(socket, &tmp.address.common, &tmp.length, is_peer);
    if (unlikely(!ret && cat_get_last_error_code() != CAT_ENOSPC)) {
        return NULL;
    }
    size = offsetof(cat_sockaddr_info_t, address) + tmp.length;
    cache = (cat_sockaddr_info_t *) cat_malloc(size);
    if (unlikely(cache == NULL)) {
        cat_update_last_error_of_syscall("Malloc for socket address info failed with size %zu", size);
        return NULL;
    }
    if (!ret) {
        /* ENOSPC, retry now */
        ret = cat_socket_getname(socket, &cache->address.common, &cache->length, is_peer);
        if (unlikely(!ret)) {
            /* almost impossible */
            cat_free(cache);
            return NULL;
        }
    } else {
        memcpy(cache, &tmp, size);
    }
    *cache_ptr = cache;

    return cache;
}

CAT_API const cat_sockaddr_info_t *cat_socket_getsockname_fast(cat_socket_t *socket)
{
    return cat_socket_getname_fast(socket, cat_false);
}

CAT_API const cat_sockaddr_info_t *cat_socket_getpeername_fast(cat_socket_t *socket)
{
    return cat_socket_getname_fast(socket, cat_true);
}

typedef struct
{
    cat_bool_t once;
    char *buffer;
    size_t size;
    size_t nread;
    cat_sockaddr_t *address;
    cat_socklen_t *address_length;
    ssize_t error;
} cat_socket_read_context_t;

static void cat_socket_read_alloc_callback(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    cat_socket_internal_t *isocket = cat_container_of(handle, cat_socket_internal_t, u.handle);
    cat_socket_read_context_t *context = (cat_socket_read_context_t *) isocket->context.io.read.data.ptr;

    buf->base = context->buffer + context->nread;
    buf->len =  (cat_socket_vector_length_t) (context->size - context->nread);
}

static void cat_socket_read_callback(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    cat_socket_internal_t *isocket = cat_container_of(stream, cat_socket_internal_t, u.stream);
    cat_socket_read_context_t *context = (cat_socket_read_context_t *) isocket->context.io.read.data.ptr;

    CAT_ASSERT(context != NULL);

    /* 0 == EAGAIN */
    if (nread == 0) {
        return;
    }

    if (nread > 0) {
        context->nread += nread;
    }
    if (unlikely(nread == CAT_EOF)) {
        if (!context->once && context->nread != context->size) {
            context->error = CAT_ECONNRESET;
        } else {
            /* connection closed normally */
            context->error = 0;
        }
    } else if (unlikely(nread < 0 && nread != CAT_ENOBUFS)) {
        /* io error */
        context->error = nread;
    } else {
        /* success (may buffer full) or eof */
        context->error = 0;
    }
    /* read once or buffer full (got all data) or error occurred */
    if (context->once || context->nread == context->size || context->error != 0) {
        cat_coroutine_t *coroutine = isocket->context.io.read.coroutine;
        CAT_ASSERT(coroutine != NULL);
        if (unlikely(!cat_coroutine_resume(coroutine, NULL, NULL))) {
            cat_core_error_with_last(SOCKET, "Stream read schedule failed");
        }
    }
}

static void cat_socket_udp_recv_callback(uv_udp_t *udp, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *address, unsigned flags)
{
    cat_socket_internal_t *isocket = cat_container_of(udp, cat_socket_internal_t, u.udp);
    cat_socket_read_context_t *context = (cat_socket_read_context_t *) isocket->context.io.read.data.ptr;

    CAT_ASSERT(context != NULL);

    /* FIXME: flags & UV_UDP_PARTIAL */
    if (unlikely(nread == 0 && address == NULL)) {
        return; // EAGAIN (if address is non-empty, it is a empty UDP packet)
    }
    if (address != NULL) {
        cat_socklen_t address_length;
        switch (address->sa_family) {
            case AF_INET:
                address_length = sizeof(struct sockaddr_in);
                break;
            case AF_INET6:
                address_length = sizeof(struct sockaddr_in6);
                break;
#ifdef CAT_OS_UNIX_LIKE
            case AF_UNIX:
                address_length = sizeof(struct sockaddr_un);
                break;
#endif
            default:
                address_length = 0;
        }
        /* it can handle empty context->address internally */
        (void) cat_sockaddr_copy(context->address, context->address_length, address, address_length);
    } else {
        if (context->address_length != NULL) {
            *context->address_length = 0;
        }
    }
    if (nread >= 0) {
        context->nread = nread;
        context->error = 0;
    } else {
        context->error = nread;
    }
    do {
        cat_coroutine_t *coroutine = isocket->context.io.read.coroutine;
        CAT_ASSERT(coroutine != NULL);
        if (unlikely(!cat_coroutine_resume(coroutine, NULL, NULL))) {
            cat_core_error_with_last(SOCKET, "UDP recv schedule failed");
        }
    } while (0);
}

static ssize_t cat_socket_internal_read_raw(
    cat_socket_internal_t *isocket,
    char *buffer, size_t size,
    cat_sockaddr_t *address, cat_socklen_t *address_length,
    cat_timeout_t timeout,
    cat_bool_t once
)
{
    cat_socket_t *socket = isocket->u.socket; CAT_ASSERT(socket != NULL);
    cat_bool_t is_dgram = (socket->type & CAT_SOCKET_TYPE_FLAG_DGRAM);
    cat_bool_t is_udp = ((socket->type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP);
#ifdef CAT_OS_UNIX_LIKE
    cat_bool_t is_udg = (socket->type & CAT_SOCKET_TYPE_UDG) == CAT_SOCKET_TYPE_UDG;
    cat_bool_t support_inline_read = isocket->u.stream.type != UV_TTY && !(isocket->u.stream.type == UV_NAMED_PIPE && isocket->u.pipe.ipc);
#endif
    size_t nread = 0;
    ssize_t error;

    if (unlikely(size == 0)) {
        error = CAT_ENOBUFS;
        goto _error;
    }

    if (is_dgram) {
        once = cat_true;
    } else {
        if (unlikely(address_length != NULL)) {
            *address_length = 0;
        }
    }

#ifdef CAT_OS_UNIX_LIKE /* (TODO: io_uring way) do not inline read on WIN, proactor way is faster */
    do {
        /* Notice: when IO is low/slow, this is deoptimization,
         * because recv usually returns EAGAIN error,
         * and there is an additional system call overhead */
        cat_socket_fd_t fd = cat_socket_internal_get_fd_fast(isocket);
        if (support_inline_read) {
            _recv:
            while (1) {
                if (!is_dgram) {
                    error = recv(fd, buffer + nread, size - nread, 0);
                } else {
                    error = recvfrom(fd, buffer, size, 0, address, address_length);
                }
                if (error < 0) {
                    error = cat_translate_sys_error(cat_sys_errno);
                    if (likely(error == CAT_EAGAIN)) {
                        break;
                    }
                    if (unlikely(error == CAT_EINTR)) {
                        continue;
                    }
                    goto _error;
                }
                if (once) {
                    return error;
                }
                nread += error;
                if (nread == size) {
                    return (ssize_t) nread;
                }
                if (error == 0) {
                    error = CAT_ECONNRESET;
                    goto _error;
                }
                break; /* next call must be EAGAIN */
            }
        }
    } while (0);
#endif

    /* async read */
    {
        cat_socket_read_context_t context;
        cat_bool_t ret;
        /* construct context */
        context.once = once;
        context.buffer = buffer;
#ifdef CAT_OS_UNIX_LIKE
        if (!is_udg)
        {
#endif
            context.size = size;
            context.nread = (size_t) nread;
#ifdef CAT_OS_UNIX_LIKE
        } else {
            /* wait for readable (TODO: if one day we use io_uring, we should change it to uv_poll way) */
            context.size = 0;
            context.nread = 0;
        }
#endif
        if (is_dgram) {
            context.address = address;
            context.address_length = address_length;
        }
        context.error = CAT_ECANCELED;
        /* wait */
        isocket->context.io.read.data.ptr = &context;
        isocket->context.io.read.coroutine = CAT_COROUTINE_G(current);
        isocket->io_flags |= CAT_SOCKET_IO_FLAG_READ;
        /* Notice: we must put (read/recv)_start here,
         * because read_alloc_callback may triggered immediately on Windows,
         * and it requires some data from context. */
        if (!is_udp) {
            error = uv_read_start(&isocket->u.stream, cat_socket_read_alloc_callback, cat_socket_read_callback);
        } else {
            error = uv_udp_recv_start(&isocket->u.udp, cat_socket_read_alloc_callback, cat_socket_udp_recv_callback);
        }
        ret = error == 0 && cat_time_wait(timeout);
        isocket->io_flags ^= CAT_SOCKET_IO_FLAG_READ;
        isocket->context.io.read.coroutine = NULL;
        isocket->context.io.read.data.ptr = NULL;
        if (unlikely(error != 0)) {
            goto _error;
        }
        /* read stop after wait done */
        if (!is_udp) {
            uv_read_stop(&isocket->u.stream);
        } else {
            uv_udp_recv_stop(&isocket->u.udp);
        }
        /* handle error */
        if (unlikely(!ret)) {
            cat_update_last_error_with_previous("Socket read wait failed");
            goto _wait_error;
        }
        nread = context.nread;
        error = context.error;
        if (unlikely(error != 0)) {
            goto _error;
        }
    }

#ifdef CAT_OS_UNIX_LIKE
    if (is_udg) {
        goto _recv;
    }
#endif

    return (ssize_t) nread;

    _error:
    if (error == CAT_ECANCELED) {
        cat_update_last_error(CAT_ECANCELED, "Socket read has been canceled");
    } else {
        cat_update_last_error_with_reason((cat_errno_t) error, "Socket read %s", nread != 0 ? "uncompleted" : "failed");
    }
    _wait_error:
    if (nread != 0) {
        /* for possible data */
        return (ssize_t) nread;
    } else {
        if (is_dgram && address_length != NULL) {
            *address_length = 0;
        }
        return -1;
    }
}

#ifdef CAT_SSL
static ssize_t cat_socket_internal_read_decrypted(
    cat_socket_internal_t *isocket,
    char *buffer, size_t size,
    cat_sockaddr_t *address, cat_socklen_t *address_length,
    cat_timeout_t timeout,
    cat_bool_t once
)
{
    cat_ssl_t *ssl = isocket->ssl; CAT_ASSERT(ssl != NULL);
    cat_buffer_t *read_buffer = &ssl->read_buffer;
    size_t nread = 0;

    while (1) {
        size_t out_length = size - nread;
        ssize_t n;

        if (!cat_ssl_decrypt(ssl, buffer, &out_length)) {
            cat_update_last_error_with_previous("Socket SSL decrypt failed");
            goto _error;
        }

        if (out_length > 0) {
            nread += out_length;
            if (once) {
                break;
            }
            if (nread == size) {
                break;
            }
        }

        CAT_TIME_WAIT_START() {
            n = cat_socket_internal_read_raw(
                isocket, read_buffer->value + read_buffer->length, read_buffer->size - read_buffer->length,
                address, address_length, timeout, cat_true
            );
        } CAT_TIME_WAIT_END(timeout);

        if (unlikely(n <= 0)) {
            if (once && n == 0) {
                /* do not treat it as error */
                break;
            }
            goto _error;
        }

        read_buffer->length += n;
    }

    return (ssize_t) nread;

    _error:
    if (nread == 0) {
        return -1;
    }
    return (ssize_t) nread;
}
#endif

static cat_always_inline ssize_t cat_socket_internal_read(
    cat_socket_internal_t *isocket,
    char *buffer, size_t size,
    cat_sockaddr_t *address, cat_socklen_t *address_length,
    cat_timeout_t timeout,
    cat_bool_t once
)
{
#ifdef CAT_SSL
    if (isocket->ssl != NULL) {
        return cat_socket_internal_read_decrypted(isocket, buffer, size, address, address_length, timeout, once);
    }
#endif
    return cat_socket_internal_read_raw(isocket, buffer, size, address, address_length, timeout, once);
}

CAT_STATIC_ASSERT(sizeof(cat_socket_write_vector_t) == sizeof(uv_buf_t));
CAT_STATIC_ASSERT(offsetof(cat_socket_write_vector_t, base) == offsetof(uv_buf_t, base));
CAT_STATIC_ASSERT(offsetof(cat_socket_write_vector_t, length) == offsetof(uv_buf_t, len));

CAT_API size_t cat_socket_write_vector_length(const cat_socket_write_vector_t *vector, unsigned int vector_count)
{
    return cat_io_vector_length((const cat_io_vector_t *) vector, vector_count);
}

typedef struct {
    int error;
    union {
        cat_coroutine_t *coroutine;
        uv_write_t stream;
        uv_udp_send_t udp;
    } u;
} cat_socket_write_request_t;

/* u is variable-length */
CAT_STATIC_ASSERT(cat_offsize_of(cat_socket_write_request_t, u) == sizeof(cat_socket_write_request_t));

/* IOCP/io_uring may not support wait writable */
static cat_always_inline void cat_socket_internal_write_callback(cat_socket_internal_t *isocket, cat_socket_write_request_t *request, int status)
{
    cat_coroutine_t *coroutine = request->u.coroutine;

    if (coroutine != NULL) {
        CAT_ASSERT(cat_queue_is_in(&coroutine->waiter.node));
        CAT_ASSERT(isocket->io_flags & CAT_SOCKET_IO_FLAG_WRITE);
        request->error = status;
        /* just resume and it will retry to send on while loop */
        if (unlikely(!cat_coroutine_resume(coroutine, NULL, NULL))) {
            cat_core_error_with_last(SOCKET, "UDP send schedule failed");
        }
    }

    cat_free(request);
}

static void cat_socket_write_callback(uv_write_t *request, int status)
{
    cat_socket_write_request_t *context = cat_container_of(request, cat_socket_write_request_t, u.stream);
    cat_socket_internal_t *isocket = cat_container_of(context->u.stream.handle, cat_socket_internal_t, u.stream);
    cat_socket_internal_write_callback(isocket, context, status);
}

static void cat_socket_udp_send_callback(uv_udp_send_t *request, int status)
{
    cat_socket_write_request_t *context = cat_container_of(request, cat_socket_write_request_t, u.udp);
    cat_socket_internal_t *isocket = cat_container_of(context->u.udp.handle, cat_socket_internal_t, u.udp);
    cat_socket_internal_write_callback(isocket, context, status);
}

#ifdef CAT_OS_UNIX_LIKE
static cat_never_inline cat_bool_t cat_socket_internal_udg_write(
    cat_socket_internal_t *isocket,
    const cat_socket_write_vector_t *vector, unsigned int vector_count,
    const cat_sockaddr_t *address, cat_socklen_t address_length,
    cat_timeout_t timeout
)
{
    cat_socket_fd_t fd = cat_socket_internal_get_fd_fast(isocket);
    cat_bool_t ret = cat_false;
    ssize_t error;

    isocket->io_flags |= CAT_SOCKET_IO_FLAG_WRITE;
    cat_queue_push_back(&isocket->context.io.write.coroutines, &CAT_COROUTINE_G(current)->waiter.node);

    while (1) {
        struct msghdr msg = { };
        msg.msg_name = (struct sockaddr *) address;
        msg.msg_namelen = address_length;
        msg.msg_iov = (struct iovec *) vector;
        msg.msg_iovlen = vector_count;
        do {
            error = sendmsg(fd, &msg, 0);
        } while (error < 0 && CAT_SOCKET_RETRY_ON_WRITE_ERROR(cat_sys_errno));
        if (unlikely(error < 0)) {
            if (CAT_SOCKET_IS_TRANSIENT_WRITE_ERROR(cat_sys_errno)) {
                // FIXME: wait write and queued (we should support multi write)
                // FIXME: support timeout
                if (likely(cat_time_msleep(1) == 0)) {
                    continue;
                }
                cat_update_last_error_with_previous("Socket UDG write wait failed");
            } else {
                cat_update_last_error_with_reason(error, "Socket write failed");
            }
        } else {
            ret = cat_true;
        }
        break;
    }

    cat_queue_remove(&CAT_COROUTINE_G(current)->waiter.node);
    if (cat_queue_empty(&isocket->context.io.write.coroutines)) {
        isocket->io_flags ^= CAT_SOCKET_IO_FLAG_WRITE;
    }

    return ret;
}
#endif

static cat_bool_t cat_socket_internal_write_raw(
    cat_socket_internal_t *isocket,
    const cat_socket_write_vector_t *vector, unsigned int vector_count,
    const cat_sockaddr_t *address, cat_socklen_t address_length,
    cat_timeout_t timeout
)
{
    CAT_SOCKET_CHECK_INPUT_ADDRESS(address, address_length, return cat_false);
    cat_socket_t *socket = isocket->u.socket; CAT_ASSERT(socket != NULL);
    cat_bool_t is_udp = ((socket->type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP);
    cat_bool_t is_dgram = (socket->type & CAT_SOCKET_TYPE_FLAG_DGRAM);
    cat_bool_t ret = cat_false;
    cat_socket_write_request_t *request;
    size_t context_size;
    ssize_t error;

#ifdef CAT_OS_UNIX_LIKE
    /* libuv does not support dymanic address length */
    CAT_ASSERT(
        (!is_udp || address == NULL || address->sa_family != AF_UNIX || address_length >= sizeof(struct sockaddr_un)) &&
        "Socket does not support variable size UNIX address on UDP"
    );
#endif

#ifdef CAT_OS_UNIX_LIKE
    if (is_dgram && !is_udp) {
        ret = cat_socket_internal_udg_write(isocket, vector, vector_count, address, address_length, timeout);
        goto _out;
    }
#endif

    /* why we do not try write: on high-traffic scenarios, try_write will instead lead to performance */
    if (!is_udp) {
        context_size = cat_offsize_of(cat_socket_write_request_t, u.stream);
    } else {
        context_size = cat_offsize_of(cat_socket_write_request_t, u.udp);
    }
    request = (cat_socket_write_request_t *) cat_malloc(context_size);
    if (unlikely(request == NULL)) {
        cat_update_last_error_of_syscall("Malloc for write reuqest failed");
        goto _out;
    }
    if (!is_dgram) {
        error = uv_write(
            &request->u.stream, &isocket->u.stream,
            (const uv_buf_t *) vector, vector_count,
            cat_socket_write_callback
        );
    } else {
        error = uv_udp_send(
            &request->u.udp, &isocket->u.udp,
            (const uv_buf_t *) vector, vector_count, address,
            cat_socket_udp_send_callback
        );
    }
    if (likely(error == 0)) {
        request->error = CAT_ECANCELED;
        request->u.coroutine = CAT_COROUTINE_G(current);
        cat_queue_push_back(&isocket->context.io.write.coroutines, &CAT_COROUTINE_G(current)->waiter.node);
        isocket->io_flags |= CAT_SOCKET_IO_FLAG_WRITE;
        ret = cat_time_wait(timeout);
        cat_queue_remove(&CAT_COROUTINE_G(current)->waiter.node);
        request->u.coroutine = NULL;
        if (cat_queue_empty(&isocket->context.io.write.coroutines)) {
            isocket->io_flags ^= CAT_SOCKET_IO_FLAG_WRITE;
        }
        if (request->error == CAT_ECANCELED) {
            /* write request is in progress, we must cancel it by close */
            cat_socket_internal_close(isocket);
#if 0
            /* event scheduler will wake up the current coroutine on cat_socket_write_callback with ECANCELED */
            cat_coroutine_wait_for(CAT_COROUTINE_G(scheduler));
#endif
        }
        if (unlikely(!ret)) {
            cat_update_last_error_with_previous("Socket write wait failed");
            goto _out;
        }
        error = request->error;
    }
    ret = error == 0;
    if (unlikely(!ret)) {
        if (error == CAT_ECANCELED) {
            cat_update_last_error(CAT_ECANCELED, "Socket write has been canceled");
        } else {
            cat_update_last_error_with_reason((cat_errno_t) error, "Socket write failed");
        }
    }

    _out:
    return ret;
}

#ifdef CAT_SSL
static cat_bool_t cat_socket_internal_write_encrypted(
    cat_socket_internal_t *isocket,
    const cat_socket_write_vector_t *vector, unsigned int vector_count,
    const cat_sockaddr_t *address, cat_socklen_t address_length,
    cat_timeout_t timeout
)
{
    cat_ssl_t *ssl = isocket->ssl; CAT_ASSERT(ssl != NULL);
    cat_io_vector_t ssl_vector[8];
    unsigned int ssl_vector_count = CAT_ARRAY_SIZE(ssl_vector);
    cat_bool_t ret;

    /* Notice: we must encrypt all buffers at once,
     * otherwise we will not be able to support queued writes */
    ret = cat_ssl_encrypt(
        isocket->ssl,
        (const cat_io_vector_t *) vector, vector_count,
        ssl_vector, &ssl_vector_count
    );

    if (unlikely(!ret)) {
        cat_update_last_error_with_previous("Socket SSL write failed");
        return cat_false;
    }

    ret = cat_socket_internal_write_raw(
        isocket, (cat_socket_write_vector_t *) ssl_vector, ssl_vector_count,
        address, address_length, timeout
    );

    cat_ssl_encrypted_vector_free(ssl, ssl_vector, ssl_vector_count);

    return ret;
}
#endif

static cat_always_inline cat_bool_t cat_socket_internal_write(
    cat_socket_internal_t *isocket,
    const cat_socket_write_vector_t *vector, unsigned int vector_count,
    const cat_sockaddr_t *address, cat_socklen_t address_length,
    cat_timeout_t timeout
)
{
#ifdef CAT_SSL
    if (isocket->ssl != NULL) {
        return cat_socket_internal_write_encrypted(isocket, vector, vector_count, address, address_length, timeout);
    }
#endif
    return cat_socket_internal_write_raw(isocket, vector, vector_count, address, address_length, timeout);
}

#define CAT_SOCKET_IO_CHECK(_socket, _isocket, _io_flag) \
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(_socket, _isocket, _io_flag, return -1); \
    do { \
        if ((_socket->type & CAT_SOCKET_TYPE_FLAG_DGRAM) != CAT_SOCKET_TYPE_FLAG_DGRAM) { \
            CAT_SOCKET_INTERNAL_ESTABLISHED_ONLY(_isocket, return -1); \
        } \
    } while (0)

static cat_always_inline ssize_t cat_socket__read(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length, cat_timeout_t timeout, cat_bool_t once)
{
    CAT_SOCKET_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_READ);
    return cat_socket_internal_read(isocket, buffer, size, address, address_length, timeout, once);
}

static cat_always_inline cat_bool_t cat_socket__write(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout)
{
    CAT_SOCKET_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_NONE);
    return cat_socket_internal_write(isocket, vector, vector_count, address, address_length, timeout);
}

static cat_always_inline cat_bool_t cat_socket__send(cat_socket_t *socket, const char *buffer, size_t length, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout)
{
    cat_socket_write_vector_t vector = cat_socket_write_vector_init(buffer, (cat_socket_vector_length_t) length);
    return cat_socket__write(socket, &vector, 1, address, address_length, timeout);
}

#define CAT_SOCKET_READ_ADDRESS_CONTEXT(_address, _name, _name_length, _port) \
    cat_sockaddr_info_t _address##_info; \
    cat_sockaddr_t *_address; \
    cat_socklen_t *_address##_length; do { \
    \
    if (unlikely((_name == NULL || _name_length == NULL || *_name_length == 0) && _port == NULL)) { \
        _address = NULL; \
        _address##_length = NULL; \
    } else { \
        _address = &_address##_info.address.common; \
        _address##_length = &_address##_info.length; \
        _address##_info.length = sizeof(_address##_info.address); \
    } \
} while (0)

#define CAT_SOCKET_READ_ADDRESS_TO_NAME(_address, _name, _name_length, _port) \
CAT_PROTECT_LAST_ERROR_START() { \
    if (unlikely(_address##_info.length > sizeof(_address##_info.address))) { \
        _address##_info.length = 0; /* address is imcomplete, just discard it */ \
    } \
    /* always call this (it can handle empty case internally) */ \
    cat_sockaddr_to_name(_address, _address##_info.length, _name, _name_length, _port); \
} CAT_PROTECT_LAST_ERROR_END()

static ssize_t cat_socket__read_from(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port, cat_timeout_t timeout)
{
    CAT_SOCKET_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_READ);
    CAT_SOCKET_READ_ADDRESS_CONTEXT(address, name, name_length, port);
    ssize_t ret;

    ret = cat_socket_internal_read(isocket, buffer, size, address, address_length, timeout, cat_true);

    CAT_SOCKET_READ_ADDRESS_TO_NAME(address, name, name_length, port);

    return ret;
}

static cat_bool_t cat_socket__write_to(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const char *name, size_t name_length, int port, cat_timeout_t timeout)
{
    CAT_SOCKET_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_NONE);
    cat_sockaddr_info_t address_info;
    cat_sockaddr_t *address;
    cat_socklen_t address_length;

    /* resolve address (DNS query may be triggered) */
    if (name_length != 0) {
        cat_bool_t ret;
        cat_queue_push_back(&isocket->context.io.write.coroutines, &CAT_COROUTINE_G(current)->waiter.node);
        isocket->io_flags |= CAT_SOCKET_IO_FLAG_WRITE;
        ret  = cat_socket_getaddrbyname(socket, &address_info, name, name_length, port);
        if (cat_queue_empty(&isocket->context.io.write.coroutines)) {
            isocket->io_flags ^= CAT_SOCKET_IO_FLAG_WRITE;
        }
        cat_queue_remove(&CAT_COROUTINE_G(current)->waiter.node);
        if (unlikely(!ret)) {
           cat_update_last_error_with_previous("Socket write failed");
           return cat_false;
        }
        address = &address_info.address.common;
        address_length = address_info.length;
    } else {
        address = NULL;
        address_length = 0;
    }

    return cat_socket_internal_write(isocket, vector, vector_count, address, address_length, timeout);
}

static cat_always_inline cat_bool_t cat_socket__send_to(cat_socket_t *socket, const char *buffer, size_t length, const char *name, size_t name_length, int port, cat_timeout_t timeout)
{
    cat_socket_write_vector_t vector = cat_socket_write_vector_init(buffer, (cat_socket_vector_length_t) length);
    return cat_socket__write_to(socket, &vector, 1, name, name_length, port, timeout);
}

CAT_API ssize_t cat_socket_read(cat_socket_t *socket, char *buffer, size_t length)
{
    return cat_socket__read(socket, buffer, length, 0, NULL, cat_socket_get_read_timeout_fast(socket), cat_false);
}

CAT_API ssize_t cat_socket_read_ex(cat_socket_t *socket, char *buffer, size_t length, cat_timeout_t timeout)
{
    return cat_socket__read(socket, buffer, length, 0, NULL, timeout, cat_false);
}

CAT_API cat_bool_t cat_socket_write(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count)
{
    return cat_socket__write(socket, vector, vector_count, NULL, 0, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_write_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, cat_timeout_t timeout)
{
    return cat_socket__write(socket, vector, vector_count, NULL, 0, timeout);
}

CAT_API ssize_t cat_socket_read_from(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port)
{
    return cat_socket__read_from(socket, buffer, size, name, name_length, port, cat_socket_get_read_timeout_fast(socket));
}

CAT_API ssize_t cat_socket_read_from_ex(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port, cat_timeout_t timeout)
{
    return cat_socket__read_from(socket, buffer, size, name, name_length, port, timeout);
}

CAT_API cat_bool_t cat_socket_writeto(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    return cat_socket__write(socket, vector, vector_count, address, address_length, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_writeto_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout)
{
    return cat_socket__write(socket, vector, vector_count, address, address_length, timeout);
}

CAT_API cat_bool_t cat_socket_write_to(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const char *name, size_t name_length, int port)
{
    return cat_socket__write_to(socket, vector, vector_count, name, name_length, port, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_write_to_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const char *name, size_t name_length, int port, cat_timeout_t timeout)
{
    return cat_socket__write_to(socket, vector, vector_count, name, name_length, port, timeout);
}

CAT_API ssize_t cat_socket_recv(cat_socket_t *socket, char *buffer, size_t size)
{
    return cat_socket__read(socket, buffer, size, 0, NULL, cat_socket_get_read_timeout_fast(socket), cat_true);
}

CAT_API ssize_t cat_socket_recv_ex(cat_socket_t *socket, char *buffer, size_t size, cat_timeout_t timeout)
{
    return cat_socket__read(socket, buffer, size, 0, NULL, timeout, cat_true);
}

CAT_API ssize_t cat_socket_recvfrom(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length)
{
    return cat_socket__read(socket, buffer, size, address, address_length, cat_socket_get_read_timeout_fast(socket), cat_true);
}

CAT_API ssize_t cat_socket_recvfrom_ex(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length, cat_timeout_t timeout)
{
    return cat_socket__read(socket, buffer, size, address, address_length, timeout, cat_true);
}

CAT_API cat_bool_t cat_socket_send(cat_socket_t *socket, const char *buffer, size_t length)
{
    return cat_socket__send(socket, buffer, length, NULL, 0, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_send_ex(cat_socket_t *socket, const char *buffer, size_t length, cat_timeout_t timeout)
{
    return cat_socket__send(socket, buffer, length, NULL, 0, timeout);
}

CAT_API cat_bool_t cat_socket_sendto(cat_socket_t *socket, const char *buffer, size_t length, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    return cat_socket__send(socket, buffer, length, address, address_length, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_sendto_ex(cat_socket_t *socket, const char *buffer, size_t length, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout)
{
    return cat_socket__send(socket, buffer, length, address, address_length, timeout);
}

CAT_API cat_bool_t cat_socket_send_to(cat_socket_t *socket, const char *buffer, size_t length, const char *name, size_t name_length, int port)
{
    return cat_socket__send_to(socket, buffer, length, name, name_length, port, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_send_to_ex(cat_socket_t *socket, const char *buffer, size_t length, const char *name, size_t name_length, int port, cat_timeout_t timeout)
{
    return cat_socket__send_to(socket, buffer, length, name, name_length, port, timeout);
}

static ssize_t cat_socket_internal_peekfrom(
    const cat_socket_internal_t *isocket,
    char *buffer, size_t size,
    cat_sockaddr_t *address, cat_socklen_t *address_length
)
{
    CAT_SOCKET_INTERNAL_FD_GETTER(isocket, fd, return cat_false);
    ssize_t nread;

#ifdef CAT_OS_UNIX_LIKE
    do {
#endif
        nread = recvfrom(
            fd,
            buffer, (cat_socket_recv_length_t) size,
            MSG_PEEK,
            address, address_length
        );
#ifdef CAT_OS_UNIX_LIKE
    } while (unlikely(nread < 0 && errno == EINTR));
#endif
    if (nread < 0) {
        if (unlikely(nread != CAT_EAGAIN && nread != CAT_EMSGSIZE)) {
            /* there was an unrecoverable error */
            cat_update_last_error_of_syscall("Socket peek failed");
        } else {
            /* not real error */
            nread = 0;
        }
    }

    return nread;
}

CAT_API ssize_t cat_socket_peek(const cat_socket_t *socket, char *buffer, size_t size)
{
    return cat_socket_peekfrom(socket, buffer, size, NULL, NULL);
}

CAT_API ssize_t cat_socket_peekfrom(const cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);

    return cat_socket_internal_peekfrom(isocket, buffer, size, address, address_length);
}

CAT_API ssize_t cat_socket_peek_from(const cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);
    CAT_SOCKET_READ_ADDRESS_CONTEXT(address, name, name_length, port);
    ssize_t nread;

    nread = cat_socket_internal_peekfrom(isocket, buffer, size, address, address_length);

    CAT_SOCKET_READ_ADDRESS_TO_NAME(address, name, name_length, port);

    return nread;
}

static cat_always_inline void cat_socket_cancel_io(cat_coroutine_t *coroutine, const char *type_name)
{
    if (coroutine != NULL) {
        /* interrupt the operation */
        if (unlikely(!cat_coroutine_resume(coroutine, NULL, NULL))) {
            cat_core_error_with_last(SOCKET, "Cancel %s failed when closing", type_name);
        }
    }
}

static void cat_socket_close_callback(uv_handle_t *handle)
{
    cat_socket_internal_t *isocket = cat_container_of(handle, cat_socket_internal_t, u.handle);

    if (isocket->io_flags == CAT_SOCKET_IO_FLAG_BIND) {
        cat_socket_cancel_io(isocket->context.bind.coroutine, "bind");
    } else if (isocket->io_flags == CAT_SOCKET_IO_FLAG_ACCEPT) {
        cat_socket_cancel_io(isocket->context.accept.coroutine, "accept");
    } else if (isocket->io_flags == CAT_SOCKET_IO_FLAG_CONNECT) {
        cat_socket_cancel_io(isocket->context.connect.coroutine, "connect");
    } else {
        /* Notice: we cancel write first */
        if (isocket->io_flags & CAT_SOCKET_IO_FLAG_WRITE) {
            cat_queue_t *write_coroutines = &isocket->context.io.write.coroutines;
            cat_coroutine_t *write_coroutine;
            while ((write_coroutine = cat_queue_front_data(write_coroutines, cat_coroutine_t, waiter.node))) {
                cat_socket_cancel_io(write_coroutine, "write");
            }
            CAT_ASSERT(cat_queue_empty(write_coroutines));
        }
        if (isocket->io_flags & CAT_SOCKET_IO_FLAG_READ) {
            cat_socket_cancel_io(isocket->context.io.read.coroutine, "read");
        }
    }

#ifdef CAT_SSL
    if (isocket->ssl_peer_name != NULL) {
        cat_free(isocket->ssl_peer_name);
    }
    if (isocket->ssl != NULL) {
        cat_ssl_close(isocket->ssl);
    }
#endif

    if (isocket->cache.sockname != NULL) {
        cat_free(isocket->cache.sockname);
    }
    if (isocket->cache.peername != NULL) {
        cat_free(isocket->cache.peername);
    }

    cat_free(isocket);
}

/* Notice: socket may be freed before isocket closed, so we can not use socket anymore after IO wait failure  */
static void cat_socket_internal_close(cat_socket_internal_t *isocket)
{
    cat_socket_t *socket = isocket->u.socket;

    if (socket == NULL) {
        return;
    }
    socket->internal = NULL;
    isocket->u.socket = NULL;
    if (unlikely(cat_socket_is_server(socket))) {
        uv_ref(&isocket->u.handle); /* unref in listen (references are idempotent) */
    }
    uv_close(&isocket->u.handle, cat_socket_close_callback);
}

CAT_API cat_bool_t cat_socket_close(cat_socket_t *socket)
{
    cat_socket_internal_t *isocket = socket->internal;

    if (isocket == NULL) {
        /* we do not update the last error here
         * because the only reason for close failure is
         * it has been closed */
        return cat_false;
    }

    cat_socket_internal_close(isocket);

    return cat_true;
}

/* getter / status / options */

CAT_API cat_bool_t cat_socket_is_available(const cat_socket_t *socket)
{
    return socket->internal != NULL;
}

CAT_API cat_bool_t cat_socket_is_open(const cat_socket_t *socket)
{
    return cat_socket_get_fd_fast(socket) != CAT_SOCKET_INVALID_FD;
}

CAT_API cat_bool_t cat_socket_is_established(const cat_socket_t *socket)
{
    cat_socket_internal_t *isocket = socket->internal;
    return isocket != NULL && cat_socket_internal_is_established(isocket);
}

CAT_API cat_bool_t cat_socket_is_server(const cat_socket_t *socket)
{
    return !!(socket->type & CAT_SOCKET_TYPE_FLAG_SERVER);
}

CAT_API cat_bool_t cat_socket_is_client(const cat_socket_t *socket)
{
    return !!(socket->type & CAT_SOCKET_TYPE_FLAG_CLIENT);
}

CAT_API cat_bool_t cat_socket_is_session(const cat_socket_t *socket)
{
    return !!(socket->type & CAT_SOCKET_TYPE_FLAG_SESSION);
}

CAT_API const char *cat_socket_get_role_name(const cat_socket_t *socket)
{
    if (cat_socket_is_session(socket)) {
        return "session";
    } else if (cat_socket_is_client(socket)) {
        return "client";
    } else if (cat_socket_is_server(socket)) {
        return "server";
    }

    return "none";
}

CAT_API cat_bool_t cat_socket_check_liveness(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);
    CAT_SOCKET_INTERNAL_FD_GETTER(isocket, fd, return cat_false);
    char buffer;
    ssize_t error;

#ifdef CAT_OS_UNIX_LIKE
    do {
#endif
        error = recv(fd, &buffer, 1, MSG_PEEK);
#ifdef CAT_OS_UNIX_LIKE
    } while (unlikely(error < 0 && errno == EINTR));
#endif

    if (unlikely(error == 0)) {
        /* connection closed */
        error = CAT_ECONNRESET;
    } else if (unlikely(error < 0)) {
        error = cat_translate_sys_error(cat_sys_errno);
    } else {
        error = 0;
    }
    if (unlikely(error != 0 && error != CAT_EAGAIN && error != CAT_EMSGSIZE)) {
        /* there was an unrecoverable error */
        cat_update_last_error((cat_errno_t) error, "Socket connection is unavailable");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_socket_is_eof_error(cat_errno_t error)
{
    CAT_ASSERT(error < 0);
    CAT_ASSERT(error != CAT_EAGAIN);
    return error != CAT_ETIMEDOUT && error != CAT_ECANCELED;
}

CAT_API cat_bool_t cat_socket_get_address(cat_socket_t *socket, char *buffer, size_t *buffer_size, cat_bool_t is_peer)
{
    const cat_sockaddr_info_t *info;

    info = cat_socket_getname_fast(socket, is_peer);
    if (unlikely(info == NULL)) {
        return cat_false;
    }

    return cat_sockaddr_get_address(&info->address.common, info->length, buffer, buffer_size);
}

CAT_API cat_bool_t cat_socket_get_sock_address(cat_socket_t *socket, char *buffer, size_t *buffer_size)
{
    return cat_socket_get_address(socket, buffer, buffer_size, cat_false);
}

CAT_API cat_bool_t cat_socket_get_peer_address(cat_socket_t *socket, char *buffer, size_t *buffer_size)
{
    return cat_socket_get_address(socket, buffer, buffer_size, cat_true);
}

CAT_API int cat_socket_get_port(cat_socket_t *socket, cat_bool_t is_peer)
{
    const cat_sockaddr_info_t *address;

    address = cat_socket_getname_fast(socket, is_peer);
    if (unlikely(address == NULL)) {
        return -1;
    }

    return cat_sockaddr_get_port(&address->address.common);
}

CAT_API int cat_socket_get_sock_port(cat_socket_t *socket)
{
    return cat_socket_get_port(socket, cat_false);
}

CAT_API int cat_socket_get_peer_port(cat_socket_t *socket)
{
    return cat_socket_get_port(socket, cat_true);
}

CAT_API const char *cat_socket_io_state_name(cat_socket_io_flags_t io_state)
{
    switch (io_state) {
        case CAT_SOCKET_IO_FLAG_READ:
            return "read";
        case CAT_SOCKET_IO_FLAG_CONNECT:
            return "connect";
        case CAT_SOCKET_IO_FLAG_ACCEPT:
            return "accept";
        case CAT_SOCKET_IO_FLAG_WRITE:
            return "write";
        case CAT_SOCKET_IO_FLAG_RDWR:
            return "read or write";
        case CAT_SOCKET_IO_FLAG_BIND:
            return "bind";
        case CAT_SOCKET_IO_FLAG_NONE:
            return "idle";
    }
    CAT_NEVER_HERE("Unknown IO flags");
}

CAT_API const char *cat_socket_io_state_naming(cat_socket_io_flags_t io_state)
{
    switch (io_state) {
        case CAT_SOCKET_IO_FLAG_READ:
            return "reading";
        case CAT_SOCKET_IO_FLAG_CONNECT:
            return "connecting";
        case CAT_SOCKET_IO_FLAG_ACCEPT:
            return "accepting";
        case CAT_SOCKET_IO_FLAG_WRITE:
            return "writing";
        case CAT_SOCKET_IO_FLAG_RDWR:
            return "reading and writing";
        case CAT_SOCKET_IO_FLAG_BIND:
            return "binding";
        case CAT_SOCKET_IO_FLAG_NONE:
            return "idle";
    }
    CAT_NEVER_HERE("Unknown IO flags");
}

CAT_API cat_socket_io_flags_t cat_socket_get_io_state(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return 0);

    return isocket->io_flags;
}

CAT_API const char *cat_socket_get_io_state_name(const cat_socket_t *socket)
{
    return cat_socket_io_state_name(cat_socket_get_io_state(socket));
}

CAT_API const char *cat_socket_get_io_state_naming(const cat_socket_t *socket)
{
    return cat_socket_io_state_naming(cat_socket_get_io_state(socket));
}

/* setter / options */

static cat_always_inline int cat_socket_align_buffer_size(int size)
{
    int pagesize = cat_getpagesize();

    if (unlikely(size < pagesize)) {
        return pagesize;
    }

    return size;
}

static int cat_socket_buffer_size(const cat_socket_t *socket, cat_bool_t is_send, size_t size)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return -1);
    int value = (int) size, error;

    if (size == 0) {
        /* try to get cache */
        int cache_size;
        if (!is_send) {
            cache_size = isocket->cache.recv_buffer_size;
        } else {
            cache_size = isocket->cache.send_buffer_size;
        }
        if (cache_size > 0) {
            return cache_size;
        }
    }

    if (!is_send) {
        error = uv_recv_buffer_size(&isocket->u.handle, &value);
    } else {
        error = uv_send_buffer_size(&isocket->u.handle, &value);
    }

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket get recv buffer size failed");
        return -1;
    }

    /* update cache */
    if (size == 0) {
        value = cat_socket_align_buffer_size(value);
        /* get */
        if (!is_send) {
            isocket->cache.recv_buffer_size = value;
        } else {
            isocket->cache.send_buffer_size = value;
        }
    } else {
        /* set */
        if (!is_send) {
            isocket->cache.recv_buffer_size = -1;
        } else {
            isocket->cache.send_buffer_size = -1;
        }
    }

    return value;
}

CAT_API int cat_socket_get_recv_buffer_size(const cat_socket_t *socket)
{
    return cat_socket_buffer_size(socket, cat_false, 0);
}

CAT_API int cat_socket_get_send_buffer_size(const cat_socket_t *socket)
{
    return cat_socket_buffer_size(socket, cat_true, 0);
}

CAT_API int cat_socket_set_recv_buffer_size(cat_socket_t *socket, int size)
{
    return cat_socket_buffer_size(socket, cat_false, cat_socket_align_buffer_size(size));
}

CAT_API int cat_socket_set_send_buffer_size(cat_socket_t *socket, int size)
{
    return cat_socket_buffer_size(socket, cat_true, cat_socket_align_buffer_size(size));
}

/* we always set the flag for extending (whether it works or not) */
#define CAT_SOCKET_SET_FLAG(_socket, _flag, _value) do { \
    if (_value) { \
        _socket->type &= ~(CAT_SOCKET_TYPE_FLAG_##_flag); \
    } else { \
        _socket->type |= (CAT_SOCKET_TYPE_FLAG_##_flag); \
    } \
} while (0)

CAT_API cat_bool_t cat_socket_set_tcp_nodelay(cat_socket_t *socket, cat_bool_t enable)
{
    CAT_SOCKET_TCP_ONLY(socket, return cat_false);
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);

    CAT_SOCKET_SET_FLAG(socket, TCP_DELAY, enable);
    if (!cat_socket_is_open(socket)) {
        return cat_true;
    }
    if (!!(isocket->u.tcp.flags & UV_HANDLE_TCP_NODELAY) != enable) {
        int error = uv_tcp_nodelay(&isocket->u.tcp, enable);
        if (unlikely(error != 0)) {
            cat_update_last_error_with_reason(error, "Socket %s TCP nodelay failed", enable ? "enable" : "disable");
            return cat_false;
        }
    }

    return cat_true;
}

CAT_API cat_bool_t cat_socket_set_tcp_keepalive(cat_socket_t *socket, cat_bool_t enable, unsigned int delay)
{
    CAT_SOCKET_TCP_ONLY(socket, return cat_false);
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);

    CAT_SOCKET_SET_FLAG(socket, TCP_KEEPALIVE, enable);
    if (!cat_socket_is_open(socket)) {
        return cat_true;
    }
    if (!!(isocket->u.tcp.flags & UV_HANDLE_TCP_KEEPALIVE) != enable) {
        if (delay == 0) {
            delay = CAT_SOCKET_G(options.tcp_keepalive_delay);
        }
        int error = uv_tcp_keepalive(&isocket->u.tcp, enable, delay);
        if (unlikely(error != 0)) {
            cat_update_last_error_with_reason(error, "Socket %s TCP Keep-Alive to %u failed", enable ? "enable" : "disable", delay);
            return cat_false;
        }
    }

    return cat_true;
}

CAT_API cat_bool_t cat_socket_set_tcp_accept_balance(cat_socket_t *socket, cat_bool_t enable)
{
    CAT_SOCKET_TCP_ONLY(socket, return cat_false);
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);
    int error;

    error = uv_tcp_simultaneous_accepts(&isocket->u.tcp, !enable);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket %s TCP balance accepts failed", enable ? "enable" : "disable");
        return cat_false;
    }

    return cat_true;
}

/* helper */

CAT_API int cat_socket_get_local_free_port(void)
{
    cat_socket_t socket;
    int port = -1;

    if (cat_socket_create(&socket, CAT_SOCKET_TYPE_TCP) == &socket) {
        if (cat_socket_bind(&socket, CAT_STRL("127.0.0.1"), 0)) {
            port = cat_socket_get_sock_port(&socket);
        }
        cat_socket_close(&socket);
    }
    if (port < 0) {
        cat_update_last_error_with_previous("Socket get local free port failed");
    }

    return port;
}

static void cat_socket_dump_callback(uv_handle_t* handle, void* arg)
{
    cat_socket_t *socket;
    cat_socket_internal_t *isocket;
    cat_socket_fd_t fd;
    const char *type_name, *io_state_naming, *role;
    char sock_addr[CAT_SOCKADDR_MAX_PATH] = "unknown", peer_addr[CAT_SOCKADDR_MAX_PATH] = "unknown";
    size_t sock_addr_size = sizeof(sock_addr), peer_addr_size = sizeof(peer_addr);
    int sock_port, peer_port;

    switch (handle->type) {
        case UV_TCP:
            isocket = cat_container_of(handle, cat_socket_internal_t, u.tcp);
            type_name = "TCP";
            break;
        case UV_NAMED_PIPE:
            isocket = cat_container_of(handle, cat_socket_internal_t, u.pipe);
            type_name = "PIPE";
            break;
        case UV_TTY:
            isocket = cat_container_of(handle, cat_socket_internal_t, u.tty);
            type_name = "TTY";
            break;
        case UV_UDP:
            isocket = cat_container_of(handle, cat_socket_internal_t, u.udp);
            type_name = "UDP";
            break;
        default:
            return;
    }

    if (isocket->u.socket == NULL) {
        fd = CAT_SOCKET_INVALID_FD;
        io_state_naming = "unavailable";
        role = "unknown";
        sock_port = peer_port = -1;
    } else {
        socket = isocket->u.socket;
        fd = cat_socket_internal_get_fd_fast(isocket);
        type_name = cat_socket_get_type_name(socket);
        io_state_naming = cat_socket_get_io_state_naming(socket);
        role = cat_socket_get_role_name(socket);
        (void) cat_socket_get_sock_address(socket, sock_addr, &sock_addr_size);
        sock_port = cat_socket_get_sock_port(socket);
        (void) cat_socket_get_sock_address(socket, peer_addr, &peer_addr_size);
        peer_port = cat_socket_get_peer_port(socket);
    }

    cat_info(SOCKET, "%-4s fd: %-6d io: %-12s role: %-7s addr: %s:%d, peer: %s:%d",
                     type_name, (int) fd, io_state_naming, role, sock_addr, sock_port, peer_addr, peer_port);
}

CAT_API void cat_socket_dump_all(void)
{
    uv_walk(cat_event_loop, cat_socket_dump_callback, NULL);
}
