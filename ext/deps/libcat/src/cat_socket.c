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
#include "cat_poll.h"

#ifdef CAT_IDE_HELPER
#include "uv-common.h"
#else
#include "../deps/libuv/src/uv-common.h"
#endif

#ifdef CAT_OS_UNIX_LIKE
/* for uv__close */
#ifdef CAT_IDE_HELPER
#include "unix/internal.h"
#else
#include "../deps/libuv/src/unix/internal.h"
#endif
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
#define cat_sockaddr_is_linux_abstract_name(path, length) (length > 0 && path[0] == '\0')
#else
#define cat_sockaddr_is_linux_abstract_name(path, length) 0
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
            return "LOCAL";
    }
    return "UNKNOWN";
}

CAT_API cat_errno_t cat_sockaddr_get_address_silent(const cat_sockaddr_t *address, cat_socklen_t address_length, char *buffer, size_t *buffer_size)
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
            return 0;
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
            return 0;
        }
        case AF_LOCAL: {
            const char *path = ((cat_sockaddr_local_t *) address)->sl_path;
            size_t length = address_length - CAT_SOCKADDR_HEADER_LENGTH;
            cat_bool_t is_lan = cat_sockaddr_is_linux_abstract_name(path, length);
            if (!is_lan) {
                /* Notice: syscall always add '\0' to the end of path and return length + 1,
                 * but system also allow user pass arg both with '\0' or without '\0'...
                 * so we need to check it real length by strnlen here */
                length = cat_strnlen(path, length);
            }
            /* Notice: we need length + 1 to set '\0' for termination */
            if (unlikely(length + !is_lan > size)) {
                error = CAT_ENOSPC;
                *buffer_size = length + !is_lan;
                break;
            }
            if (length > 0) {
                memcpy(buffer, path, length);
            }
            if (!is_lan) {
                buffer[length] = '\0';
            }
            *buffer_size = length;
            return 0;
        }
        default: {
            return CAT_EAFNOSUPPORT;
        }
    }

    return error;
}

CAT_API int cat_sockaddr_get_port_silent(const cat_sockaddr_t *address)
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
            return CAT_EAFNOSUPPORT;
    }
}

CAT_API cat_bool_t cat_sockaddr_get_address(const cat_sockaddr_t *address, cat_socklen_t address_length, char *buffer, size_t *buffer_size)
{
    cat_errno_t error = cat_sockaddr_get_address_silent(address, address_length, buffer, buffer_size);

    if (unlikely(error != 0)) {
        if (error == CAT_EAFNOSUPPORT) {
            cat_update_last_error(CAT_EAFNOSUPPORT, "Socket address family %d is unknown", address->sa_family);
        } else {
            cat_update_last_error_with_reason(error, "Socket convert address to name failed");
        }
        return cat_false;
    }

    return cat_true;
}

CAT_API int cat_sockaddr_get_port(const cat_sockaddr_t *address)
{
    int port = cat_sockaddr_get_port_silent(address);

    if (unlikely(port == CAT_EAFNOSUPPORT)) {
        cat_update_last_error(CAT_EAFNOSUPPORT, "Socket address family %d does not belong to INET", address->sa_family);
        return -1;
    }

    return port;
}

CAT_API cat_bool_t cat_sockaddr_set_port(cat_sockaddr_t *address, int port)
{
    switch (address->sa_family)
    {
        case AF_INET:
            ((cat_sockaddr_in_t *) address)->sin_port = htons((uint16_t)port);
            return cat_true;
        case AF_INET6:
            ((cat_sockaddr_in6_t *) address)->sin6_port = htons((uint16_t)port);
            return cat_true;
        default:
            cat_update_last_error(CAT_EINVAL, "Socket address family %d does not belong to INET", address->sa_family);
    }

    return cat_false;
}

static int cat_sockaddr__getbyname(cat_sockaddr_t *address, cat_socklen_t *address_length, const char *name, size_t name_length, int port)
{
    cat_socklen_t size = address_length != NULL ? *address_length : 0;
    if (unlikely(((int) size) < ((int) sizeof(address->sa_family)))) {
        return CAT_EINVAL;
    }
    cat_sa_family_t af = address->sa_family;
    cat_bool_t unspec = af == AF_UNSPEC;
    int error;

    *address_length = 0;

    if (af == AF_LOCAL) {
        cat_socklen_t real_length;
        cat_bool_t is_lan;

        if (unlikely(name_length > CAT_SOCKADDR_MAX_PATH)) {
            *address_length = 0;
            return CAT_ENAMETOOLONG;
        }
        is_lan = cat_sockaddr_is_linux_abstract_name(name, name_length);
        real_length = (cat_socklen_t) (CAT_SOCKADDR_HEADER_LENGTH + name_length + !is_lan);
        if (unlikely(real_length > size)) {
            *address_length = (cat_socklen_t) real_length;
            return CAT_ENOSPC;
        }
        address->sa_family = AF_LOCAL;
        if (name_length > 0) {
            memcpy(address->sa_data, name, name_length);
            /* Add the '\0' terminator as much as possible */
            if (!is_lan) {
                address->sa_data[name_length] = '\0';
            }
        }
        *address_length = real_length;

        return 0;
    }

    if (unspec) {
        af = AF_INET; /* try IPV4 first */
    }
    while (1) {
        if (af == AF_INET) {
            *address_length = sizeof(cat_sockaddr_in_t);
            if (unlikely(size < sizeof(cat_sockaddr_in_t))) {
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
            if (unlikely(size < sizeof(cat_sockaddr_in6_t))) {
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

static cat_always_inline void cat_sockaddr_zero_name(char *name, size_t *name_length, int *port)
{
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

CAT_API cat_errno_t cat_sockaddr_to_name_silent(const cat_sockaddr_t *address, cat_socklen_t address_length, char *name, size_t *name_length, int *port)
{
    cat_errno_t ret = 0;

    if (address_length > 0) {
        if (likely(name != NULL && name_length != NULL)) {
            ret = cat_sockaddr_get_address_silent(address, address_length, name, name_length);
        }
        if (port != NULL) {
            *port = cat_sockaddr_get_port_silent(address);
        }
    } else {
        cat_sockaddr_zero_name(name, name_length, port);
    }

    return ret;
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
        cat_sockaddr_zero_name(name, name_length, port);
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

CAT_API cat_errno_t cat_sockaddr_check_silent(const cat_sockaddr_t *address, cat_socklen_t address_length)
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
            return CAT_EAFNOSUPPORT;
    }
    if (unlikely(address_length < min_length)) {
        return CAT_EINVAL;
    }

    return 0;
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
    CAT_SOCKET_G(last_id) = 0;
    CAT_SOCKET_G(options.timeout) = cat_socket_default_global_timeout_options;
    CAT_SOCKET_G(options.tcp_keepalive_delay) = 60;

    return cat_true;
}

static cat_never_inline const char *cat_socket_get_error_from_flags(cat_errno_t *error, cat_socket_flags_t flags)
{
    if (flags & CAT_SOCKET_FLAG_UNRECOVERABLE_ERROR) {
        return "Socket has been broken due to unrecoverable IO error";
    } else if (flags & CAT_SOCKET_FLAG_CLOSED) {
        return "Socket has been closed";
    } else {
        *error = CAT_EMISUSE;
        return "Socket has not been constructed";
    }
}

#define CAT_SOCKET_INTERNAL_GETTER_SILENT(_socket, _isocket, _failure) \
    cat_socket_internal_t *_isocket = _socket->internal;\
    do { if (_isocket == NULL) { \
        cat_errno_t error = CAT_EBADF; (void) error; \
        _failure; \
    }} while (0)

#define CAT_SOCKET_INTERNAL_GETTER(_socket, _isocket, _failure) \
        CAT_SOCKET_INTERNAL_GETTER_SILENT(_socket, _isocket, { \
            const char *error_message = cat_socket_get_error_from_flags(&error, _socket->flags); \
            cat_update_last_error(error, "%s", error_message); \
            _failure; \
        })

#define CAT_SOCKET_INTERNAL_CHECK_IO_SILENT(_socket, _isocket, _io_flags, _failure) do { \
    if (unlikely(_isocket->io_flags & _io_flags)) { \
        cat_errno_t error = CAT_ELOCKED; \
        _failure; \
    } \
} while (0)

#define CAT_SOCKET_INTERNAL_GETTER_WITH_IO_SILENT(_socket, _isocket, _io_flags, _failure) \
    CAT_SOCKET_INTERNAL_GETTER_SILENT(_socket, _isocket, _failure); \
    CAT_SOCKET_INTERNAL_CHECK_IO_SILENT(_socket, _isocket, _io_flags, _failure)

#define CAT_SOCKET_INTERNAL_GETTER_WITH_IO(_socket, _isocket, _io_flags, _failure) \
    CAT_SOCKET_INTERNAL_GETTER(_socket, _isocket, _failure); \
    CAT_SOCKET_INTERNAL_CHECK_IO_SILENT(_socket, _isocket, _io_flags, { \
        cat_update_last_error( \
            error, "Socket is %s now, unable to %s", \
            cat_socket_io_state_naming(_isocket->io_flags), \
            cat_socket_io_state_name(_io_flags) \
        ); \
        _failure; \
    })

#define CAT_SOCKET_INTERNAL_FD_GETTER_SILENT(_isocket, _fd, _failure) \
    cat_socket_fd_t _fd = cat_socket_internal_get_fd_fast(isocket); \
    do { if (unlikely(_fd == CAT_SOCKET_INVALID_FD)) { \
        _failure; \
    }} while (0)

#define CAT_SOCKET_INTERNAL_FD_GETTER(_isocket, _fd, _failure) \
        CAT_SOCKET_INTERNAL_FD_GETTER_SILENT(_isocket, _fd, { \
            cat_update_last_error(CAT_EBADF, "Socket file descriptor is bad"); \
            _failure; \
        })

static cat_always_inline cat_bool_t cat_socket_internal_is_established(cat_socket_internal_t *isocket)
{
    if (!(isocket->flags & CAT_SOCKET_INTERNAL_FLAG_ESTABLISHED)) {
        return cat_false;
    }
#ifdef CAT_SSL
    if (isocket->ssl != NULL && !cat_ssl_is_established(isocket->ssl)) {
        return cat_false;
    }
#endif

    return cat_true;
}

#define CAT_SOCKET_INTERNAL_ESTABLISHED_ONLY_SILENT(_isocket, _failure) do { \
    if (unlikely(!cat_socket_internal_is_established(_isocket))) { \
        cat_errno_t error = CAT_ENOTCONN; \
        _failure; \
    } \
} while (0)

#define CAT_SOCKET_INTERNAL_ESTABLISHED_ONLY(_isocket, _failure) \
        CAT_SOCKET_INTERNAL_ESTABLISHED_ONLY_SILENT(_isocket, { \
            cat_update_last_error(error, "Socket has not been established"); \
            _failure; \
        })

#define CAT_SOCKET_INTERNAL_ESTABLISHED_ONCE(_isocket, _failure) do { \
    if (cat_socket_internal_is_established(_isocket)) { \
        cat_update_last_error(CAT_EISCONN, "Socket has been established"); \
        _failure; \
    } \
} while (0)

#define CAT_SOCKET_INTERNAL_WHICH_ONLY(_isocket, _flags, _errstr, _failure) do { \
    if (!(_isocket->type & (_flags))) { \
        cat_update_last_error(CAT_EMISUSE, _errstr); \
        _failure; \
    } \
} while (0)

#define CAT_SOCKET_INTERNAL_INET_STREAM_ONLY(_isocket, _failure) \
    CAT_SOCKET_INTERNAL_WHICH_ONLY(_isocket, CAT_SOCKET_TYPE_FLAG_STREAM | CAT_SOCKET_TYPE_FLAG_INET, "Socket should be type of inet stream", _failure);

#define CAT_SOCKET_INTERNAL_TCP_ONLY(_isocket, _failure) \
    CAT_SOCKET_INTERNAL_WHICH_ONLY(_isocket, CAT_SOCKET_TYPE_TCP, "Socket is not of type TCP", _failure)

#define CAT_SOCKET_INTERNAL_WHICH_SIDE_ONLY(_isocket, _name, _errstr, _failure) do { \
    if (!(_isocket->flags & CAT_SOCKET_INTERNAL_FLAG_##_name)) { \
        cat_update_last_error(CAT_EMISUSE, _errstr); \
        _failure; \
    } \
} while (0)

#define CAT_SOCKET_INTERNAL_SERVER_ONLY(_isocket, _failure) \
        CAT_SOCKET_INTERNAL_WHICH_SIDE_ONLY(_isocket, SERVER, "Socket is not listening for connections", _failure)

#define CAT_SOCKET_CHECK_INPUT_ADDRESS(_address, _address_length, failure) do { \
    if (_address != NULL && _address_length > 0) { \
        if (unlikely(!cat_sockaddr_check(_address, _address_length))) { \
            failure; \
        } \
    } \
} while (0)

#define CAT_SOCKET_CHECK_INPUT_ADDRESS_SILENT(_address, _address_length, failure) do { \
    if (_address != NULL && _address_length > 0) { \
        cat_errno_t error = cat_sockaddr_check_silent(_address, _address_length); \
        if (unlikely(error != 0)) { \
            failure; \
        } \
    } \
} while (0)

#define CAT_SOCKET_CHECK_INPUT_ADDRESS_REQUIRED(_address, _address_length, failure) do { \
    if (_address == NULL || _address_length == 0) { \
        cat_update_last_error(CAT_EINVAL, "Socket input address can not be empty"); \
        failure; \
    } \
    if (unlikely(!cat_sockaddr_check(_address, _address_length))) { \
        failure; \
    } \
} while (0)

static void cat_socket_internal_close(cat_socket_internal_t *isocket, cat_bool_t unrecoverable_error);

static CAT_COLD void cat_socket_internal_unrecoverable_io_error(cat_socket_internal_t *isocket);
#ifdef CAT_SSL
static cat_always_inline void cat_socket_internal_ssl_recoverability_check(cat_socket_internal_t *isocket);
#endif

#ifdef CAT_OS_UNIX_LIKE
static int socket_create(int domain, int type, int protocol)
{
    return socket(domain, type, protocol);
}
#endif

static cat_always_inline cat_timeout_t cat_socket_internal_get_dns_timeout(const cat_socket_internal_t *isocket);
static cat_always_inline cat_sa_family_t cat_socket_internal_get_af(const cat_socket_internal_t *isocket);
static const cat_sockaddr_info_t *cat_socket_internal_getname_fast(cat_socket_internal_t *isocket, cat_bool_t is_peer, int *error_ptr);

static cat_bool_t cat_socket_internal_getaddrbyname(
    cat_socket_internal_t *isocket,
    cat_sockaddr_info_t *address_info,
    const char *name, size_t name_length, int port,
    cat_bool_t *is_host_name
)
{
    cat_sockaddr_union_t *address = &address_info->address;
    cat_sa_family_t af = cat_socket_internal_get_af(isocket);
    int error;

    /* for failure */
    if (is_host_name != NULL) {
        *is_host_name = cat_false;
    }

    address->common.sa_family = af;
    address_info->length = sizeof(address_info->address);
    error = cat_sockaddr__getbyname(&address->common, &address_info->length, name, name_length, port);

    if (likely(error == 0)) {
        return cat_true;
    }
    if (unlikely(error != CAT_EINVAL)) {
        cat_update_last_error_with_reason(error, "Socket get address by name failed");
        return cat_false;
    }

    /* try to solve the name */
    do {
        struct addrinfo hints = {0};
        struct addrinfo *response;
        cat_bool_t ret;
        hints.ai_family = af;
        hints.ai_flags = 0;
        if (isocket->type & CAT_SOCKET_TYPE_FLAG_STREAM) {
            hints.ai_socktype = SOCK_STREAM;
        } else if (isocket->type & CAT_SOCKET_TYPE_FLAG_DGRAM) {
            hints.ai_socktype = SOCK_DGRAM;
        } else {
            hints.ai_socktype = 0;
        }
        response = cat_dns_getaddrinfo_ex(name, NULL, &hints, cat_socket_internal_get_dns_timeout(isocket));
        if (unlikely(response == NULL)) {
            break;
        }
        if (is_host_name != NULL) {
            *is_host_name = cat_true;
        }
        memcpy(&address->common, response->ai_addr, response->ai_addrlen);
        cat_dns_freeaddrinfo(response);
        ret = cat_sockaddr_set_port(&address->common, port);
        if (unlikely(!ret)) {
            break;
        }
        switch (address->common.sa_family) {
            case AF_INET:
                address_info->length = sizeof(cat_sockaddr_in_t);
                break;
            case AF_INET6:
                address_info->length = sizeof(cat_sockaddr_in6_t);
                break;
            default:
                CAT_NEVER_HERE("Must be AF_INET/AF_INET6");
        }
        return cat_true;
    } while (0);

    cat_update_last_error_with_previous("Socket get address by name failed");
    return cat_false;
}

static void cat_socket_internal_detect_family(cat_socket_internal_t *isocket, cat_sa_family_t af)
{
    CAT_ASSERT(af == AF_INET || af == AF_INET6);
    if (af == AF_INET) {
        isocket->type |= CAT_SOCKET_TYPE_FLAG_IPV4;
    } else if (af == AF_INET6) {
        isocket->type |= CAT_SOCKET_TYPE_FLAG_IPV6;
    }
}

static cat_always_inline cat_bool_t cat_socket_internal_can_be_transfer_by_ipc(cat_socket_internal_t *isocket)
{
    return
            ((isocket->type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) ||
#ifndef CAT_OS_WIN
            ((isocket->type & CAT_SOCKET_TYPE_PIPE) == CAT_SOCKET_TYPE_PIPE) ||
            ((isocket->type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) ||
            ((isocket->type & CAT_SOCKET_TYPE_UDG) == CAT_SOCKET_TYPE_UDG) ||
#endif
            0;
}

static cat_always_inline void cat_socket_internal_on_open(cat_socket_internal_t *isocket, cat_sa_family_t af)
{
    if (unlikely(isocket->flags & CAT_SOCKET_INTERNAL_FLAG_OPENED)) {
        return;
    }
    isocket->flags |= CAT_SOCKET_INTERNAL_FLAG_OPENED;
    if ((isocket->type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
        if (!(isocket->option_flags & CAT_SOCKET_OPTION_FLAG_TCP_DELAY)) {
            /* TCP always nodelay by default */
            (void) uv_tcp_nodelay(&isocket->u.tcp, 1);
        }
        if (isocket->option_flags & CAT_SOCKET_OPTION_FLAG_TCP_KEEPALIVE) {
            (void) uv_tcp_keepalive(&isocket->u.tcp, 1, isocket->options.tcp_keepalive_delay);
        }
    } else if ((isocket->type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) {
        if (isocket->option_flags & CAT_SOCKET_OPTION_FLAG_UDP_BROADCAST) {
            (void) uv_udp_set_broadcast(&isocket->u.udp, 1);
        }
    }
    if (af != AF_UNSPEC && (isocket->type & CAT_SOCKET_TYPE_FLAG_INET)) {
        cat_socket_internal_detect_family(isocket, af);
    }
}

static CAT_COLD void cat_socket_fail_close_callback(uv_handle_t *handle)
{
    cat_socket_internal_t *isocket = cat_container_of(handle, cat_socket_internal_t, u.handle);
    cat_free(isocket);
}

CAT_API void cat_socket_init(cat_socket_t *socket)
{
    socket->id = 0;
    socket->flags = CAT_SOCKET_FLAG_NONE;
    socket->internal = NULL;
}

CAT_API cat_socket_t *cat_socket_create(cat_socket_t *socket, cat_socket_type_t type)
{
    return cat_socket_create_ex(socket, type, NULL);
}

CAT_API cat_socket_t *cat_socket_create_ex(cat_socket_t *socket, cat_socket_type_t type, const cat_socket_creation_options_t *options)
{
    cat_socket_creation_options_t ioptions;
    cat_socket_flags_t flags = CAT_SOCKET_FLAG_NONE;
    cat_socket_internal_t *isocket = NULL;
    cat_sa_family_t af = AF_UNSPEC;
    cat_bool_t check_connection = cat_true;
    size_t isocket_size;
    int error = CAT_EINVAL;

    if (options == NULL) {
        memset(&ioptions, 0, sizeof(ioptions));
    } else {
        ioptions = *options;
    }

    if (socket == NULL) {
        socket = (cat_socket_t *) cat_malloc(sizeof(*socket));
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(socket == NULL)) {
            cat_update_last_error_of_syscall("Malloc for socket failed");
            goto _malloc_error;
        }
#endif
        flags |= CAT_SOCKET_FLAG_ALLOCATED;
    }
#ifndef CAT_DONT_OPTIMIZE
    /* dynamic memory allocation so we can save some space */
    if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
        isocket_size = cat_offsize_of(cat_socket_internal_t, u.tcp);
    } else if ((type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) {
        isocket_size = cat_offsize_of(cat_socket_internal_t, u.udp);
    } else if (type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
#ifdef CAT_OS_UNIX_LIKE
        if (type & CAT_SOCKET_TYPE_FLAG_DGRAM) {
            isocket_size = cat_offsize_of(cat_socket_internal_t, u.udg);
        } else
#endif
        {
            isocket_size = cat_offsize_of(cat_socket_internal_t, u.pipe);
        }
    } else if ((type & CAT_SOCKET_TYPE_TTY) == CAT_SOCKET_TYPE_TTY) {
        isocket_size = cat_offsize_of(cat_socket_internal_t, u.tty);
    } else {
        goto _type_error;
    }
    /* u must be the last member */
    CAT_STATIC_ASSERT(cat_offsize_of(cat_socket_internal_t, u) == sizeof(*isocket));
#else
    isocket_size = sizeof(*isocket);
#endif
    isocket = (cat_socket_internal_t *) cat_malloc(isocket_size);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(isocket == NULL)) {
        cat_update_last_error_of_syscall("Malloc for socket internal failed");
        goto _malloc_internal_error;
    }
#endif
    /* Notice: dump callback may access this even if creation failed,
     * so we must set it to NULL here */
    isocket->u.socket = NULL;

    /* solve type and get af
     *  Notice: we should create socket without IPV* flag (AF_UNSPEC)
     *  to make sure that sys socket will not be created for now
     *  (it should be created when accept)) */
    if (type & CAT_SOCKET_TYPE_FLAG_IPV4) {
        af = AF_INET;
    } else if (type & CAT_SOCKET_TYPE_FLAG_IPV6) {
        af = AF_INET6;
    } else {
        af = AF_UNSPEC;
    }
    if (ioptions.flags & CAT_SOCKET_CREATION_OPEN_FLAGS) {
        /* prevent uv from creating a new socket */
        af = AF_UNSPEC;
    }

    /* init handle */
    if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
        error = uv_tcp_init_ex(&CAT_EVENT_G(loop), &isocket->u.tcp, af);
    } else if ((type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) {
        error = uv_udp_init_ex(&CAT_EVENT_G(loop), &isocket->u.udp, af);
    } else if (type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
#ifdef CAT_OS_UNIX_LIKE
        if ((type & CAT_SOCKET_TYPE_UDG) == CAT_SOCKET_TYPE_UDG) {
            if ((ioptions.flags & CAT_SOCKET_CREATION_OPEN_FLAGS) == 0) {
                ioptions.flags |= CAT_SOCKET_CREATION_FLAG_OPEN_FD;
                ioptions.o.fd = socket_create(AF_UNIX, SOCK_DGRAM, IPPROTO_IP);
                if (unlikely(ioptions.o.fd < 0)) {
                    error = cat_translate_sys_error(errno);
                    goto _init_error;
                }
            }
            if (ioptions.flags & CAT_SOCKET_CREATION_FLAG_OPEN_FD) {
                if (type & CAT_SOCKET_TYPE_FLAG_IPC) {
                    error = CAT_EINVAL;
                    goto _init_error;
                }
                check_connection = cat_false;
                isocket->u.udg.readfd = CAT_SOCKET_INVALID_FD;
                isocket->u.udg.writefd = CAT_SOCKET_INVALID_FD;
            }
        }
        else
#endif
        if (unlikely((type & CAT_SOCKET_TYPE_PIPE) != CAT_SOCKET_TYPE_PIPE)) {
            error = CAT_ENOTSUP;
            goto _init_error;
        }
        error = uv_pipe_init(&CAT_EVENT_G(loop), &isocket->u.pipe, !!(type & CAT_SOCKET_TYPE_FLAG_IPC));
    } else if ((type & CAT_SOCKET_TYPE_TTY) == CAT_SOCKET_TYPE_TTY) {
        /* convert SOCKET to int on Windows */
        cat_os_fd_t os_fd;
        if (ioptions.flags & CAT_SOCKET_CREATION_FLAG_OPEN_FD) {
            os_fd = ioptions.o.fd;
        } else {
            os_fd = CAT_OS_INVALID_FD;
        }
        if (os_fd == CAT_OS_INVALID_FD) {
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
        } else {
            os_fd = CAT_OS_INVALID_FD;
        }
        ioptions.flags &= ~CAT_SOCKET_CREATION_OPEN_FLAGS;
        ioptions.flags |= CAT_SOCKET_CREATION_FLAG_OPEN_FD;
        ioptions.o.fd = (cat_socket_fd_t) os_fd;
        error = uv_tty_init(&CAT_EVENT_G(loop), &isocket->u.tty, os_fd, 0);
    }
    if (unlikely(error != 0)) {
        goto _init_error;
    }
    if (ioptions.flags & CAT_SOCKET_CREATION_OPEN_FLAGS) {
        if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP &&
            (ioptions.flags & CAT_SOCKET_CREATION_FLAG_OPEN_SOCKET)) {
            error = uv_tcp_open(&isocket->u.tcp, ioptions.o.socket);
        } else if ((type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP &&
                   (ioptions.flags & CAT_SOCKET_CREATION_FLAG_OPEN_SOCKET)) {
            error = uv_udp_open(&isocket->u.udp, ioptions.o.socket);
        } else if ((type & CAT_SOCKET_TYPE_FLAG_LOCAL) &&
                   (ioptions.flags & CAT_SOCKET_CREATION_FLAG_OPEN_FD)) {
            /* convert SOCKET to int on Windows */
            error = uv_pipe_open(&isocket->u.pipe, ioptions.o.fd);
        } else if ((type & CAT_SOCKET_TYPE_TTY) == CAT_SOCKET_TYPE_TTY) {
            error = 0;
        } else {
            error = CAT_EINVAL;
        }
        if (unlikely(error != 0)) {
            goto _open_error;
        }
    }

    /* init properties of socket */
    socket->id = ++CAT_SOCKET_G(last_id);
    socket->flags = flags;
    socket->internal = isocket;

    /* init properties of socket internal */
    isocket->type = type;
    isocket->u.socket = socket;
    isocket->flags = CAT_SOCKET_INTERNAL_FLAG_NONE;
    isocket->io_flags = CAT_SOCKET_IO_FLAG_NONE;
    memset(&isocket->context.io.read, 0, sizeof(isocket->context.io.read));
    cat_queue_init(&isocket->context.io.write.coroutines);
    /* part of cache */
    isocket->cache.fd = CAT_SOCKET_INVALID_FD;
    isocket->cache.write_request = NULL;
    isocket->cache.ipcc_handle_info = NULL;
    isocket->cache.sockname = NULL;
    isocket->cache.peername = NULL;
    isocket->cache.recv_buffer_size = -1;
    isocket->cache.send_buffer_size = -1;
    /* options */
    isocket->option_flags = CAT_SOCKET_OPTION_FLAG_NONE;
    isocket->options.timeout = cat_socket_default_timeout_options;
    isocket->options.tcp_keepalive_delay = 0;
#ifdef CAT_SSL
    isocket->ssl = NULL;
    isocket->ssl_peer_name = NULL;
#endif

    if (ioptions.flags & CAT_SOCKET_CREATION_OPEN_FLAGS) {
        const cat_sockaddr_info_t *address_info = NULL;
        if (check_connection) {
            cat_bool_t is_established;
            if (((type & CAT_SOCKET_TYPE_TTY) == CAT_SOCKET_TYPE_TTY)) {
                is_established = cat_true;
            } else if (type & CAT_SOCKET_TYPE_FLAG_STREAM) {
                do {
                    if ((type & CAT_SOCKET_TYPE_PIPE) == CAT_SOCKET_TYPE_PIPE) {
                        unsigned int flags = isocket->u.handle.flags & (UV_HANDLE_READABLE | UV_HANDLE_WRITABLE);
                        if (flags != 0 && flags != (UV_HANDLE_READABLE | UV_HANDLE_WRITABLE)) {
                            // pipe2() pipe
                            is_established = cat_true;
                            break;
                        }
                    }
                    is_established = cat_socket_internal_getname_fast(isocket, cat_true, NULL) != NULL;
                } while (0);
            } else {
                /* non-connection types */
                is_established = cat_false;
            }
            if (is_established) {
                isocket->flags |= CAT_SOCKET_INTERNAL_FLAG_ESTABLISHED;
            }
        }
        cat_socket_internal_on_open(isocket, address_info != NULL ? address_info->address.common.sa_family : AF_UNSPEC);
    } else if (af != AF_UNSPEC) {
        cat_socket_internal_on_open(isocket, af);
    }

    return socket;

    if (0 && "isocket should be free in close callback after handle has been initialized") {
        _open_error:
        uv_close(&isocket->u.handle, cat_socket_fail_close_callback);
    } else {
        _init_error:
        cat_free(isocket);
    }
    _type_error:
    cat_update_last_error_with_reason(error, "Socket %s with type %s failed",
        !(ioptions.flags & CAT_SOCKET_CREATION_OPEN_FLAGS) ? "create" : "open",
        cat_socket_type_name(type));
#if CAT_ALLOC_HANDLE_ERRORS
    _malloc_internal_error:
#endif
    if (flags & CAT_SOCKET_FLAG_ALLOCATED) {
        cat_free(socket);
    }
#if CAT_ALLOC_HANDLE_ERRORS
    _malloc_error:
#endif

    return NULL;
}

CAT_API cat_socket_t *cat_socket_open_os_fd(cat_socket_t *socket, cat_socket_type_t type, cat_os_fd_t os_fd)
{
    cat_socket_creation_options_t options;
    options.flags = CAT_SOCKET_CREATION_FLAG_OPEN_FD;
    options.o.fd = os_fd;
    return cat_socket_create_ex(socket, type, &options);
}

CAT_API cat_socket_t *cat_socket_open_os_socket(cat_socket_t *socket, cat_socket_type_t type, cat_os_socket_t os_socket)
{
    cat_socket_creation_options_t options;
    options.flags = CAT_SOCKET_CREATION_FLAG_OPEN_SOCKET;
    options.o.socket = os_socket;
    return cat_socket_create_ex(socket, type, &options);
}

CAT_API cat_socket_type_t cat_socket_type_simplify(cat_socket_type_t type)
{
    return type &= ~(CAT_SOCKET_TYPE_FLAG_IPV4 | CAT_SOCKET_TYPE_FLAG_IPV6);
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
        if (type & CAT_SOCKET_TYPE_FLAG_IPC) {
            return "IPCC";
        } else {
            return "PIPE";
        }
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

CAT_API cat_sa_family_t cat_socket_type_to_af(cat_socket_type_t type)
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

CAT_API cat_socket_id_t cat_socket_get_id(const cat_socket_t *socket)
{
    return socket->id;
}

CAT_API cat_socket_type_t cat_socket_get_type(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER_SILENT(socket, isocket, return CAT_SOCKET_TYPE_ANY);
    return isocket->type;
}

CAT_API cat_socket_type_t cat_socket_get_simple_type(const cat_socket_t *socket)
{
    return cat_socket_type_simplify(cat_socket_get_type(socket));
}

CAT_API const char *cat_socket_get_type_name(const cat_socket_t *socket)
{
    return cat_socket_type_name(cat_socket_get_type(socket));
}

CAT_API const char *cat_socket_get_simple_type_name(const cat_socket_t *socket)
{
    return cat_socket_type_name(cat_socket_get_simple_type(socket));
}

static cat_always_inline cat_sa_family_t cat_socket_internal_get_af(const cat_socket_internal_t *isocket)
{
    return cat_socket_type_to_af(isocket->type);
}

CAT_API cat_sa_family_t cat_socket_get_af(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER_SILENT(socket, isocket, return AF_UNSPEC);
    return cat_socket_internal_get_af(isocket);
}

static cat_never_inline cat_socket_fd_t cat_socket_internal_get_fd_slow(const cat_socket_internal_t *isocket)
{
    cat_os_handle_t fd = CAT_OS_INVALID_HANDLE;

    (void) uv_fileno(&isocket->u.handle, &fd);

    return (cat_socket_fd_t) fd;
}

static cat_always_inline cat_socket_fd_t cat_socket_internal_get_fd_fast(const cat_socket_internal_t *isocket)
{
    cat_socket_fd_t fd = isocket->cache.fd;

    if (unlikely(fd == CAT_SOCKET_INVALID_FD)) {
        fd = cat_socket_internal_get_fd_slow(isocket);
        ((cat_socket_internal_t *) isocket)->cache.fd = fd;
    }

    return fd;
}

CAT_API cat_socket_fd_t cat_socket_get_fd_fast(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER_SILENT(socket, isocket, return CAT_SOCKET_INVALID_FD);

    return cat_socket_internal_get_fd_fast(isocket);
}

CAT_API cat_socket_fd_t cat_socket_get_fd(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return CAT_SOCKET_INVALID_FD);
    cat_os_handle_t fd = CAT_OS_INVALID_HANDLE;
    int error;

    error = uv_fileno(&isocket->u.handle, &fd);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket get fd failed");
    }

    return (cat_socket_fd_t) fd;
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
    CAT_SOCKET_INTERNAL_GETTER_SILENT(socket, isocket, return CAT_TIMEOUT_INVALID); \
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

static cat_bool_t cat_socket_internal_bind(
    cat_socket_internal_t *isocket,
    const cat_sockaddr_t *address, cat_socklen_t address_length,
    cat_socket_bind_flags_t flags
)
{
    CAT_SOCKET_CHECK_INPUT_ADDRESS_REQUIRED(address, address_length, return cat_false);
    cat_socket_type_t type = isocket->type;
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
            const char *name = address->sa_data;
            size_t name_length = address_length - CAT_SOCKADDR_HEADER_LENGTH;
            if (!cat_sockaddr_is_linux_abstract_name(name, name_length)) {
                name_length = cat_strnlen(name, name_length);
            }
            error = uv_pipe_bind_ex(&isocket->u.pipe, name, name_length);
        }
    }
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket bind failed");
        return cat_false;
    }
    /* bind done successfully, we can do something here before transfer data */
    cat_socket_internal_on_open(isocket, address->sa_family);
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
        ret  = cat_socket_internal_getaddrbyname(isocket, &address_info, name, name_length, port, NULL);
        isocket->io_flags = CAT_SOCKET_IO_FLAG_NONE;
        isocket->context.bind.coroutine = NULL;
        if (unlikely(!ret)) {
           cat_update_last_error_with_previous("Socket bind failed");
           return cat_false;
        }
        address = &address_info.address.common;
        address_length = address_info.length;
    } while (0);

    return cat_socket_internal_bind(isocket, address, address_length, flags);
}

CAT_API cat_bool_t cat_socket_bind_to(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    return cat_socket_bind_to_ex(socket, address, address_length, CAT_SOCKET_BIND_FLAG_NONE);
}

CAT_API cat_bool_t cat_socket_bind_to_ex(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_socket_bind_flags_t flags)
{
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(socket, isocket, CAT_SOCKET_IO_FLAG_BIND, return cat_false);

    return cat_socket_internal_bind(isocket, address, address_length, flags);
}

static void cat_socket_accept_connection_callback(uv_stream_t *stream, int status)
{
    cat_socket_internal_t *iserver = cat_container_of(stream, cat_socket_internal_t, u.stream);

    if (iserver->io_flags == CAT_SOCKET_IO_FLAG_ACCEPT) {
        cat_coroutine_t *coroutine = iserver->context.accept.coroutine;
        CAT_ASSERT(coroutine != NULL);
        iserver->context.accept.data.status = status;
        cat_coroutine_schedule(coroutine, SOCKET, "Accept");
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
    isocket->flags |= CAT_SOCKET_INTERNAL_FLAG_SERVER;

    return cat_true;
}

static cat_bool_t cat_socket_internal_accept(
    cat_socket_internal_t *iserver, cat_socket_internal_t *iconnection,
    cat_socket_inheritance_info_t *handle_info, cat_timeout_t timeout
) {
    int error;

    if (handle_info == NULL) {
        cat_socket_type_t server_type = cat_socket_type_simplify(iserver->type);
        cat_socket_type_t connection_type = iconnection->type;
        if (unlikely((server_type & connection_type) != server_type)) {
            cat_update_last_error(CAT_EINVAL, "Socket accept connection type mismatch, expect %s but got %s",
                cat_socket_type_name(server_type), cat_socket_type_name(connection_type));
            return cat_false;
        }
    }

    while (1) {
        cat_bool_t ret;
        error = uv_accept(&iserver->u.stream, &iconnection->u.stream);
        if (error == 0) {
            /* init client properties */
            iconnection->flags |= (CAT_SOCKET_INTERNAL_FLAG_ESTABLISHED | CAT_SOCKET_INTERNAL_FLAG_SERVER_CONNECTION);
            /* TODO: socket_extends() ? */
            memcpy(&iconnection->options, handle_info == NULL ? &iserver->options : &handle_info->options, sizeof(iconnection->options));
            cat_socket_internal_on_open(iconnection, cat_socket_type_to_af(handle_info == NULL ? iserver->type : handle_info->type));
            return cat_true;
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

    return cat_false;
}

static cat_bool_t cat_socket_internal_recv_handle(cat_socket_internal_t *isocket, cat_socket_internal_t *ihandle, cat_timeout_t timeout);

CAT_API cat_bool_t cat_socket_accept(cat_socket_t *server, cat_socket_t *connection)
{
    return cat_socket_accept_ex(server, connection, cat_socket_get_accept_timeout_fast(server));
}

CAT_API cat_bool_t cat_socket_accept_ex(cat_socket_t *server, cat_socket_t *connection, cat_timeout_t timeout)
{
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(server, iserver, CAT_SOCKET_IO_FLAG_ACCEPT, return cat_false);
    if (!(iserver->type & CAT_SOCKET_TYPE_FLAG_IPC)) {
        CAT_SOCKET_INTERNAL_SERVER_ONLY(iserver, return cat_false);
    }

    CAT_SOCKET_INTERNAL_GETTER_SILENT(connection, iconnection, {
        cat_update_last_error(CAT_EINVAL, "Socket accept can not act on an unavailable socket");
        return cat_false;
    });
    if (unlikely(cat_socket_is_open(connection))) {
        cat_update_last_error(CAT_EMISUSE, "Socket accept can only act on a lazy socket");
        return cat_false;
    }

    if (!(iserver->type & CAT_SOCKET_TYPE_FLAG_IPC)) {
        return cat_socket_internal_accept(iserver, iconnection, NULL, timeout);
    } else {
        return cat_socket_internal_recv_handle(iserver, iconnection, timeout);
    }
}

static cat_always_inline void cat_socket_internal_on_connect_done(cat_socket_internal_t *isocket, cat_sa_family_t af)
{
    /* connect done successfully, we can do something here before transfer data */
    isocket->flags |= (CAT_SOCKET_INTERNAL_FLAG_ESTABLISHED | CAT_SOCKET_INTERNAL_FLAG_CLIENT);
    cat_socket_internal_on_open(isocket, af);
    /* clear previous cache (maybe impossible here) */
    if (unlikely(isocket->cache.peername)) {
        cat_free(isocket->cache.peername);
        isocket->cache.peername = NULL;
    }
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
        cat_coroutine_schedule(coroutine, SOCKET, "Connect");
    }

    cat_free(request);
}

static void cat_socket_try_connect_callback(uv_connect_t* request, int status)
{
#ifndef CAT_OS_DARWIN
    cat_socket_internal_t *isocket = cat_container_of(request->handle, cat_socket_internal_t, u.stream);

    if (status == 0) {
        cat_socket_internal_on_connect_done(isocket, (cat_sa_family_t) (intptr_t) request->data);
    }
#endif

    cat_free(request);
}

static cat_bool_t cat_socket_internal_connect(
    cat_socket_internal_t *isocket,
    const cat_sockaddr_t *address, cat_socklen_t address_length,
    cat_timeout_t timeout, cat_bool_t is_try
)
{
    CAT_SOCKET_CHECK_INPUT_ADDRESS_REQUIRED(address, address_length, return cat_false);
    cat_socket_type_t type = isocket->type;
    uv_connect_t *request;
    int error = 0;

    /* only TCP and PIPE need request */
    if (((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) || (type & CAT_SOCKET_TYPE_FLAG_LOCAL)) {
        /* malloc for request (we must free it in the callback if it has been started) */
        request = (uv_connect_t *) cat_malloc(sizeof(*request));
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(request == NULL)) {
            cat_update_last_error_of_syscall("Malloc for Socket connect request failed");
            return cat_false;
        }
#endif
    } else {
        request = NULL;
    }
    if ((type & CAT_SOCKET_TYPE_TCP) == CAT_SOCKET_TYPE_TCP) {
        error = uv_tcp_connect(
            request, &isocket->u.tcp, address,
            !is_try ? cat_socket_connect_callback : cat_socket_try_connect_callback
        );
        if (unlikely(error != 0)) {
            cat_update_last_error_with_reason(error, "Tcp connect init failed");
            cat_free(request);
            return cat_false;
        }
    } else if ((type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP) {
        error = uv_udp_connect(&isocket->u.udp, address);
    } else if (type & CAT_SOCKET_TYPE_FLAG_LOCAL) {
        const char *name = address->sa_data;
        size_t name_length = address_length - CAT_SOCKADDR_HEADER_LENGTH;
        if (!cat_sockaddr_is_linux_abstract_name(name, name_length)) {
            name_length = cat_strnlen(name, name_length);
        }
        (void) uv_pipe_connect_ex(
            request, &isocket->u.pipe, name, name_length,
            !is_try ? cat_socket_connect_callback : cat_socket_try_connect_callback
        );
    } else {
        error = CAT_EAFNOSUPPORT;
    }
    if (request != NULL) {
        if (!is_try) {
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
                cat_socket_internal_unrecoverable_io_error(isocket);
                return cat_false;
            }
            error = isocket->context.connect.data.status;
            if (error == CAT_ECANCELED) {
                cat_update_last_error(CAT_ECANCELED, "Socket connect has been canceled");
                /* interrupt can not recover */
                cat_socket_internal_unrecoverable_io_error(isocket);
                return cat_false;
            }
        }
#ifndef CAT_OS_DARWIN
        else {
            request->data = (void *) (intptr_t) address->sa_family;
        }
#endif
    }
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket connect failed");
        return cat_false;
    }
#ifndef CAT_OS_DARWIN /* connect done is later than POLLOUT event on macOS */
    if (!is_try)
#endif
    {
        cat_socket_internal_on_connect_done(isocket, address->sa_family);
    }

    return cat_true;
}

static cat_bool_t cat_socket__smart_connect(cat_socket_t *socket, const char *name, size_t name_length, int port, cat_timeout_t timeout, cat_bool_t is_try)
{
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(socket, isocket, CAT_SOCKET_IO_FLAG_CONNECT, return cat_false);
    CAT_SOCKET_INTERNAL_ESTABLISHED_ONCE(isocket, return cat_false);

    /* address can be NULL if it's UDP */
    if (name_length == 0 && ((isocket->type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP)) {
        return cat_socket_internal_connect(isocket, NULL, 0, timeout, is_try);
    }

    /* otherwise we resolve address */
    cat_sa_family_t af = cat_socket_internal_get_af(isocket);
    cat_sockaddr_info_t address_info;
    cat_bool_t ret = cat_false;
    int error;
    address_info.length = sizeof(address_info.address);
    address_info.address.common.sa_family = af;
    error = cat_sockaddr__getbyname(&address_info.address.common, &address_info.length, name, name_length, port);
    if (error == 0) {
        return cat_socket_internal_connect(isocket, &address_info.address.common, address_info.length, timeout, is_try);
    }
    if (unlikely(error != CAT_EINVAL)) {
        cat_update_last_error_with_reason(error, "Socket connect failed, reason: Socket get address by name failed");
        return cat_false;
    }

    /* the input addr name is a domain name, do a ns look-up */
    struct addrinfo *responses, *response;
    struct addrinfo hints = {0};
    if (af != AF_UNSPEC) {
        // the socket is already specified or bound
        hints.ai_family = af;
    }
    if (isocket->type & CAT_SOCKET_TYPE_FLAG_STREAM) {
        hints.ai_socktype = SOCK_STREAM;
    } else if (isocket->type & CAT_SOCKET_TYPE_FLAG_DGRAM) {
        hints.ai_socktype = SOCK_DGRAM;
    } else {
        hints.ai_socktype = 0;
    }
    isocket->context.connect.coroutine = CAT_COROUTINE_G(current);
    isocket->io_flags = CAT_SOCKET_IO_FLAG_CONNECT;
    responses = cat_dns_getaddrinfo_ex(name, NULL, &hints, cat_socket_get_dns_timeout_fast(socket));
    isocket->io_flags = CAT_SOCKET_IO_FLAG_NONE;
    isocket->context.connect.coroutine = NULL;
    if (unlikely(responses == NULL)) {
        cat_update_last_error_with_previous("Socket connect failed");
        return cat_false;
    }
    /* Try to connect to all address results until successful */
    cat_sa_family_t last_af = responses->ai_addr->sa_family;
    cat_bool_t is_initialized = cat_socket_internal_get_fd_fast(isocket) != CAT_SOCKET_INVALID_FD;
    response = responses;
    do {
        CAT_ASSERT(((response->ai_addr->sa_family == AF_INET && response->ai_addrlen == sizeof(struct sockaddr_in)) ||
                   (response->ai_addr->sa_family == AF_INET6 && response->ai_addrlen == sizeof(struct sockaddr_in6))) &&
                   "GAI should only return inet and inet6 addresses");
        memcpy(&address_info.address.common, response->ai_addr, response->ai_addrlen);
        if (response->ai_addr->sa_family == AF_INET) {
            address_info.address.in.sin_port = htons((uint16_t) port);
        } else if (response->ai_addr->sa_family == AF_INET6) {
            address_info.address.in6.sin6_port = htons((uint16_t) port);
        } else {
            continue; // should never here
        }
        if (response->ai_family != last_af) {
            CAT_ASSERT(af == AF_UNSPEC);
            // af changed and socket has not been initialized yet, recreate isocket
            cat_socket_type_t type = isocket->type; // TODO: Implement socket_dup()
            cat_socket_internal_close(isocket, cat_false);
            ret = cat_socket_create(socket, type) != NULL;
            if (!ret) {
                cat_update_last_error_with_previous("Socket connect recreate failed");
                break;
            }
            isocket = socket->internal;
        }
        ret = cat_socket_internal_connect(
            isocket,
            &address_info.address.common,
            (cat_socklen_t) response->ai_addrlen,
            timeout,
            is_try
        );
        if (ret || is_initialized || !cat_socket_is_available(socket)) {
            break;
        }
    } while ((response = response->ai_next));
    cat_dns_freeaddrinfo(responses);

#ifdef CAT_SSL
    if (ret) {
        isocket->ssl_peer_name = cat_strndup(name, name_length);
    }
#endif

    return ret;
}

static cat_bool_t cat_socket__connect_to(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout, cat_bool_t is_try)
{
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(socket, isocket, CAT_SOCKET_IO_FLAG_CONNECT, return cat_false);
    CAT_SOCKET_INTERNAL_ESTABLISHED_ONCE(isocket, return cat_false);

    return cat_socket_internal_connect(isocket, address, address_length, timeout, is_try);
}

CAT_API cat_bool_t cat_socket_connect(cat_socket_t *socket, const char *name, size_t name_length, int port)
{
    return cat_socket_connect_ex(socket, name, name_length, port, cat_socket_get_connect_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_connect_ex(cat_socket_t *socket, const char *name, size_t name_length, int port, cat_timeout_t timeout)
{
    return cat_socket__smart_connect(socket, name, name_length, port, timeout, cat_false);
}

CAT_API cat_bool_t cat_socket_connect_to(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    return cat_socket_connect_to_ex(socket, address, address_length, cat_socket_get_connect_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_connect_to_ex(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout)
{
    return cat_socket__connect_to(socket, address, address_length, timeout, cat_false);
}

CAT_API cat_bool_t cat_socket_try_connect(cat_socket_t *socket, const char *name, size_t name_length, int port)
{
    return cat_socket__smart_connect(socket, name, name_length, port, CAT_TIMEOUT_INVALID, cat_true);
}

CAT_API cat_bool_t cat_socket_try_connect_to(cat_socket_t *socket, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    return cat_socket__connect_to(socket, address, address_length, CAT_TIMEOUT_INVALID, cat_true);
}

#ifdef CAT_SSL
CAT_API void cat_socket_crypto_options_init(cat_socket_crypto_options_t *options, cat_bool_t is_client)
{
    options->is_client = is_client;
    options->peer_name = NULL;
    options->ca_file = NULL;
    options->ca_path = NULL;
    options->certificate = NULL;
    options->certificate_key = NULL;
    options->passphrase = NULL;
    options->protocols = CAT_SSL_PROTOCOLS_DEFAULT;
    options->verify_depth = CAT_SSL_DEFAULT_STREAM_VERIFY_DEPTH;
    options->verify_peer = is_client;
    options->verify_peer_name = is_client;
    options->allow_self_signed = cat_false;
    options->no_ticket = cat_false;
    options->no_compression = cat_false;
    options->no_client_ca_list = cat_false;
}

/* TODO: Support non-blocking SSL handshake? (just for PHP, stupid design) */

CAT_API cat_bool_t cat_socket_enable_crypto(cat_socket_t *socket, const cat_socket_crypto_options_t *options)
{
    return cat_socket_enable_crypto_ex(socket, options, cat_socket_get_handshake_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_enable_crypto_ex(cat_socket_t *socket, const cat_socket_crypto_options_t *options, cat_timeout_t timeout)
{
    /* TODO: DTLS support */
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(socket, isocket, CAT_SOCKET_IO_FLAG_RDWR, return cat_false);
    CAT_SOCKET_INTERNAL_INET_STREAM_ONLY(isocket, return cat_false);
    if (unlikely(isocket->ssl != NULL && cat_ssl_is_established(isocket->ssl))) {
        CAT_SOCKET_INTERNAL_ESTABLISHED_ONCE(isocket, return cat_false);
    } else {
        CAT_SOCKET_INTERNAL_ESTABLISHED_ONLY(isocket, return cat_false);
    }
    cat_ssl_t *ssl;
    cat_ssl_context_t *context = NULL;
    cat_buffer_t *buffer;
    cat_socket_crypto_options_t ioptions;
    cat_bool_t use_tmp_context;
    cat_bool_t ret = cat_false;

    /* check options */
    if (options == NULL) {
        cat_bool_t is_client = !cat_socket_is_server_connection(socket);
        cat_socket_crypto_options_init(&ioptions, is_client);
    } else {
        ioptions = *options;
    }
    if (ioptions.peer_name == NULL) {
        ioptions.peer_name = isocket->ssl_peer_name;
    }
    if (ioptions.verify_peer_name && ioptions.peer_name == NULL) {
        cat_update_last_error(CAT_EINVAL, "SSL verify peer name is enabled but peer name is empty");
        goto _prepare_error;
    }

    /* check context (TODO: support context from arg?) */
    use_tmp_context = context == NULL;
    if (use_tmp_context) {
        cat_ssl_method_t method;
        if (isocket->type & CAT_SOCKET_TYPE_FLAG_STREAM) {
            method = CAT_SSL_METHOD_TLS;
        } else /* if (socket->type & CAT_SOCKET_TYPE_FLAG_DGRAM) */ {
            method = CAT_SSL_METHOD_DTLS;
        }
        context = cat_ssl_context_create(method, ioptions.protocols);
        if (unlikely(context == NULL)) {
            goto _prepare_error;
        }
    }

    if (ioptions.verify_peer) {
        if (!ioptions.is_client && !ioptions.no_client_ca_list && ioptions.ca_file != NULL) {
            if (!cat_ssl_context_set_client_ca_list(context, ioptions.ca_file)) {
                goto _setup_error;
            }
        }
        if (ioptions.ca_file  != NULL || ioptions.ca_path != NULL) {
            if (!cat_ssl_context_load_verify_locations(context, ioptions.ca_file, ioptions.ca_path)) {
                goto _setup_error;
            }
        } else {
#ifndef CAT_OS_WIN
            /* check context related options */
            if (ioptions.is_client && !cat_ssl_context_set_default_verify_paths(context)) {
                goto _setup_error;
            }
#else
            cat_ssl_context_configure_cert_verify_callback(context);
#endif
        }
        cat_ssl_context_set_verify_depth(context, ioptions.verify_depth);
        cat_ssl_context_enable_verify_peer(context);
    } else {
        cat_ssl_context_disable_verify_peer(context);
    }
    if (ioptions.passphrase != NULL) {
        if (!cat_ssl_context_set_passphrase(context, ioptions.passphrase, strlen(ioptions.passphrase))) {
            goto _setup_error;
        }
    }
    if (ioptions.certificate != NULL) {
        cat_ssl_context_set_certificate(context, ioptions.certificate, ioptions.certificate_key);
    }
    if (ioptions.no_ticket) {
        cat_ssl_context_set_no_ticket(context);
    }
    if (ioptions.no_compression) {
        cat_ssl_context_set_no_compression(context);
    }

    /* create ssl connection */
    ssl = cat_ssl_create(NULL, context);
    if (use_tmp_context) {
        /* deref/free context */
        cat_ssl_context_close(context);
    }
    if (unlikely(ssl == NULL)) {
        goto _prepare_error;
    }

    /* set state */
    if (!ioptions.is_client) {
        cat_ssl_set_accept_state(ssl);
    } else {
        cat_ssl_set_connect_state(ssl);
    }

    /* connection related options */
    if (ioptions.is_client && ioptions.peer_name != NULL) {
        cat_ssl_set_sni_server_name(ssl, ioptions.peer_name);
    }
    ssl->allow_self_signed = ioptions.allow_self_signed;

    buffer = &ssl->read_buffer;

    while (1) {
        ssize_t n;
        cat_ssl_ret_t ssl_ret;

        ssl_ret = cat_ssl_handshake(ssl);
        if (unlikely(ssl_ret == CAT_SSL_RET_ERROR)) {
            break;
        }
        /* ssl_read_encrypted_bytes() may return n > 0
         * after ssl_handshake() return OK */
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
        }
        /* Notice: if it's client and it write something to the server,
         * it means server will response something later, so, we need to recv it then returns,
         * otherwise it will lead errors on Windows */
        if (ssl_ret == CAT_SSL_RET_OK && !(n > 0 && ioptions.is_client)) {
            CAT_LOG_DEBUG(SOCKET, "Socket SSL handshake completed");
            ret = cat_true;
            break;
        }
        {
            ssize_t nread, nwrite;
            CAT_TIME_WAIT_START() {
                nread = cat_socket_recv_ex(socket, buffer->value, buffer->size, timeout);
            } CAT_TIME_WAIT_END(timeout);
            if (unlikely(nread <= 0)) {
                break;
            }
            nwrite = cat_ssl_write_encrypted_bytes(ssl, buffer->value, nread);
            if (unlikely(nwrite != nread)) {
                break;
            }
            continue;
        }
    }

    if (unlikely(!ret)) {
        /* Notice: io error can not recover */
        goto _unrecoverable_error;
    }

    if (ioptions.verify_peer) {
        if (!cat_ssl_verify_peer(ssl, ioptions.allow_self_signed)) {
            goto _unrecoverable_error;
        }
    }
    if (ioptions.verify_peer_name) {
        if (!cat_ssl_check_host(ssl, ioptions.peer_name, strlen(ioptions.peer_name))) {
            goto _unrecoverable_error;
        }
    }

    isocket->ssl = ssl;

    return cat_true;

    _unrecoverable_error:
    cat_socket_internal_unrecoverable_io_error(isocket);
    cat_ssl_close(ssl);
    if (0) {
        _setup_error:
        cat_ssl_context_close(context);
    }
    _prepare_error:
    cat_update_last_error_with_previous("Socket enable crypto failed");

    return cat_false;
}

#if 0
/* TODO: BIO SSL shutdown
 * we must cancel all IO operations and mark this socket unavailable temporarily,
 * then read or write on it to shutdown SSL connection */
CAT_API cat_bool_t cat_socket_disable_crypto(cat_socket_t *socket)
{
    if (!cat_socket_is_encrypted(socket)) {
        return cat_true;
    }
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(socket, isocket, CAT_SOCKET_IO_FLAG_RDWR, return cat_false);
    cat_ssl_t *ssl = isocket->ssl;

    while (1) {
        cat_ssl_ret_t ret = cat_ssl_shutdown(ssl);
        if (ret == 1) {
            cat_ssl_close(ssl);
            return cat_true;
        }
        // read or write...
    }
}
#endif
#endif

static int cat_socket_internal_getname(const cat_socket_internal_t *isocket, cat_sockaddr_t *address, cat_socklen_t *address_length, cat_bool_t is_peer)
{
    cat_socket_type_t type = isocket->type;
    cat_sockaddr_info_t *cache = !is_peer ? isocket->cache.sockname : isocket->cache.peername;
    int error;

    if (address_length != NULL && ((int) *address_length) < 0) {
        error = CAT_EINVAL;
    } else if (cache != NULL) {
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
            cat_sockaddr_local_t local;
            size_t length = sizeof(local.sl_path);
            if (!is_peer) {
                error = uv_pipe_getsockname(&isocket->u.pipe, local.sl_path, &length);
            } else {
                error = uv_pipe_getpeername(&isocket->u.pipe, local.sl_path, &length);
            }
            if (error == 0 || error == CAT_ENOBUFS) {
                local.sl_family = AF_LOCAL;
                /* Notice: the returned length no longer includes the terminating null byte,
                * and the buffer is not null terminated only if it's linux abstract name. */
                length = CAT_SOCKADDR_HEADER_LENGTH + length + !cat_sockaddr_is_linux_abstract_name(local.sl_path, length);
                memcpy(address, &local, CAT_MIN((size_t) *address_length, length));
                *address_length = (cat_socklen_t) length;
            }
        }
    }

    return error;
}

static const cat_sockaddr_info_t *cat_socket_internal_getname_fast(cat_socket_internal_t *isocket, cat_bool_t is_peer, int *error_ptr)
{
    cat_sockaddr_info_t *cache, **cache_ptr;
    cat_sockaddr_info_t tmp;
    size_t size;
    int error = 0;

    if (!is_peer) {
        cache_ptr = &isocket->cache.sockname;
    } else {
        cache_ptr = &isocket->cache.peername;
    }
    cache = *cache_ptr;
    if (cache != NULL) {
        goto _out;
    }
    tmp.length = sizeof(tmp.address);
    error = cat_socket_internal_getname(isocket, &tmp.address.common, &tmp.length, is_peer);
    if (unlikely(error != 0 && error != CAT_ENOSPC)) {
        goto _out;
    }
    size = offsetof(cat_sockaddr_info_t, address) + tmp.length;
    cache = (cat_sockaddr_info_t *) cat_malloc(size);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(cache == NULL)) {
        error = cat_translate_sys_error(cat_sys_errno);
        goto _out;
    }
#endif
    if (error != 0) {
        /* ENOSPC, retry now */
        error = cat_socket_internal_getname(isocket, &cache->address.common, &cache->length, is_peer);
        if (unlikely(error != 0)) {
            /* almost impossible */
            cat_free(cache);
            goto _out;
        }
    } else {
        memcpy(cache, &tmp, size);
    }
    *cache_ptr = cache;

    _out:
    if (error_ptr != NULL) {
        *error_ptr = error;
    }
    return cache;
}

CAT_API cat_bool_t cat_socket_getname(const cat_socket_t *socket, cat_sockaddr_t *address, cat_socklen_t *address_length, cat_bool_t is_peer)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);
    int error;

    error = cat_socket_internal_getname(isocket, address, address_length, is_peer);

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
    const cat_sockaddr_info_t *cache;
    int error;

    cache = cat_socket_internal_getname_fast(isocket, is_peer, &error);

    if (cache == NULL) {
        cat_update_last_error_with_reason(error, "Socket get%sname fast failed", !is_peer ? "sock" : "peer");
    }

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

typedef struct cat_socket_read_context_s {
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
    (void) suggested_size;
    cat_socket_internal_t *isocket = cat_container_of(handle, cat_socket_internal_t, u.handle);
    cat_socket_read_context_t *context = (cat_socket_read_context_t *) isocket->context.io.read.data.ptr;

    buf->base = context->buffer + context->nread;
    buf->len =  (cat_socket_vector_length_t) (context->size - context->nread);
}

static void cat_socket_read_callback(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    (void) buf;
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
        cat_coroutine_schedule(coroutine, SOCKET, "Stream read");
    }
}

static void cat_socket_udp_recv_callback(uv_udp_t *udp, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *address, unsigned flags)
{
    (void) buf;
    (void) flags;
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
        cat_coroutine_schedule(coroutine, SOCKET, "UDP recv");
    } while (0);
}

#ifdef CAT_OS_UNIX_LIKE
static int cat_socket_internal_udg_wait_readable(cat_socket_internal_t *isocket, cat_socket_fd_t fd, cat_timeout_t timeout)
{
    cat_ret_t ret;
    if (isocket->u.udg.readfd == CAT_OS_INVALID_FD) {
        isocket->u.udg.readfd = dup(fd);
        if (unlikely(isocket->u.udg.readfd == CAT_OS_INVALID_FD)) {
            return cat_translate_sys_error(cat_sys_errno);
        }
    }
    isocket->context.io.read.coroutine = CAT_COROUTINE_G(current);
    isocket->io_flags |= CAT_SOCKET_IO_FLAG_READ;
    ret = cat_poll_one(isocket->u.udg.readfd, POLLIN, NULL, timeout);
    isocket->io_flags ^= CAT_SOCKET_IO_FLAG_READ;
    isocket->context.io.read.coroutine = NULL;
    if (ret == CAT_RET_OK) {
        return 0;
    } else if (ret == CAT_RET_NONE) {
        return CAT_ETIMEDOUT;
    } else {
        return CAT_EPREV;
    }
}
#endif

static cat_always_inline cat_bool_t cat_socket_internal_support_inline_read(const cat_socket_internal_t *isocket)
{
    return isocket->u.handle.type != UV_TTY &&
           !(isocket->u.handle.type == UV_NAMED_PIPE && isocket->u.pipe.ipc);
}

static ssize_t cat_socket_internal_read_raw(
    cat_socket_internal_t *isocket,
    char *buffer, size_t size,
    cat_sockaddr_t *address, cat_socklen_t *address_length,
    cat_timeout_t timeout,
    cat_bool_t once
)
{
    cat_bool_t is_dgram = (isocket->type & CAT_SOCKET_TYPE_FLAG_DGRAM);
    cat_bool_t is_udp = ((isocket->type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP);
#ifdef CAT_OS_UNIX_LIKE
    cat_bool_t is_udg = (isocket->type & CAT_SOCKET_TYPE_UDG) == CAT_SOCKET_TYPE_UDG;
#endif
    size_t nread = 0;
    ssize_t error;

    if (unlikely(size == 0)) {
        error = CAT_ENOBUFS;
        goto _error;
    }

    if (!is_dgram) {
        if (unlikely(address_length != NULL)) {
            *address_length = 0;
        }
    } else {
        once = cat_true;
    }

#ifdef CAT_OS_UNIX_LIKE /* Do not inline read on WIN, proactor way is faster */
    /* Notice: when IO is low/slow, this is de-optimization,
     * because recv usually returns EAGAIN error,
     * and there is an additional system call overhead */
    if (likely(cat_socket_internal_support_inline_read(isocket))) {
        cat_socket_fd_t fd = cat_socket_internal_get_fd_fast(isocket);
        if (unlikely(fd == CAT_SOCKET_INVALID_FD)) {
            CAT_ASSERT(is_dgram && "only dgram fd creation is lazy");
        } else while (1) {
            while (1) {
                if (isocket->flags & CAT_SOCKET_INTERNAL_FLAG_NOT_SOCK) {
                    error = read(fd, buffer + nread, size - nread);
                } else if (!is_dgram) {
                    error = recv(fd, buffer + nread, size - nread, 0);
                } else {
                    error = recvfrom(fd, buffer, size, 0, address, address_length);
                }
                if (error < 0) {
                    if (likely(cat_sys_errno == EAGAIN)) {
                        break;
                    }
                    if (unlikely(cat_sys_errno == EINTR)) {
                        continue;
                    }
                    if (unlikely(cat_sys_errno == ENOTSOCK)) {
                        isocket->flags |= CAT_SOCKET_INTERNAL_FLAG_NOT_SOCK;
                        continue;
                    }
                    error = cat_translate_sys_error(cat_sys_errno);
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
            if (is_udg) {
                error = cat_socket_internal_udg_wait_readable(isocket, fd, timeout);
                if (unlikely(error != 0)) {
                    if (error != CAT_EPREV) {
                        goto _error;
                    } else {
                        goto _wait_error;
                    }
                } else {
                    continue;
                }
            }
            break;
        }
    }
#endif

    /* async read */
    {
        cat_socket_read_context_t context;
        cat_bool_t ret;
        /* construct context */
        context.once = once;
        context.buffer = buffer;
        context.size = size;
        context.nread = (size_t) nread;
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
            goto _wait_error;
        }
        nread = context.nread;
        error = context.error;
        if (unlikely(error != 0)) {
            goto _error;
        }
    }

    return (ssize_t) nread;

    _error:
    if (error == CAT_ECANCELED) {
        cat_update_last_error(CAT_ECANCELED, "Socket read has been canceled");
    } else if (1) {
        cat_update_last_error_with_reason((cat_errno_t) error, "Socket read %s", nread != 0 ? "uncompleted" : "failed");
    } else {
        _wait_error:
        cat_update_last_error_with_previous("Socket read wait failed");
    }
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

static ssize_t cat_socket_internal_try_recv_raw(
    cat_socket_internal_t *isocket,
    char *buffer, size_t size,
    cat_sockaddr_t *address, cat_socklen_t *address_length
)
{
    cat_socket_t *socket = isocket->u.socket; CAT_ASSERT(socket != NULL);
    cat_socket_fd_t fd;
    ssize_t nread;

    if (unlikely(!cat_socket_internal_support_inline_read(isocket))) {
        return CAT_EMISUSE;
    }
    fd = cat_socket_internal_get_fd_fast(isocket);
    if (unlikely(fd == CAT_SOCKET_INVALID_FD)) {
        return CAT_EBADF;
    }

    while (1) {
        nread = recvfrom(
            fd, buffer, (cat_socket_recv_length_t) size, 0,
            address, address_length
        );
        if (nread < 0) {
#ifdef CAT_OS_UNIX_LIKE
            if (unlikely(cat_sys_errno == EINTR)) {
                continue;
            }
#endif
            return cat_translate_sys_error(cat_sys_errno);
        }
        break;
    }

    return nread;
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
    cat_bool_t eof;

    while (1) {
        size_t out_length = size - nread;
        ssize_t n;

        if (!cat_ssl_decrypt(ssl, buffer, &out_length, &eof)) {
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
        if (eof) {
            if (once) {
                /* do not treat it as error */
                break;
            }
            goto _error;
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
    cat_socket_internal_ssl_recoverability_check(isocket);
    if (nread == 0) {
        return -1;
    }
    return (ssize_t) nread;
}

static ssize_t cat_socket_internal_try_recv_decrypted(
    cat_socket_internal_t *isocket,
    char *buffer, size_t size,
    cat_sockaddr_t *address, cat_socklen_t *address_length
)
{
    cat_ssl_t *ssl = isocket->ssl; CAT_ASSERT(ssl != NULL);
    cat_buffer_t *read_buffer = &ssl->read_buffer;

    while (1) {
        size_t out_length = size;
        ssize_t nread;
        cat_errno_t error = 0;
        cat_bool_t decrypted, eof;

        CAT_PROTECT_LAST_ERROR_START() {
            decrypted = cat_ssl_decrypt(ssl, buffer, &out_length, &eof);
            if (!decrypted) {
                error = cat_get_last_error_code();
            }
        } CAT_PROTECT_LAST_ERROR_END();
        if (!decrypted) {
            cat_socket_internal_ssl_recoverability_check(isocket);
            return error;
        }

        if (out_length > 0) {
            return out_length;
        }
        if (eof) {
            return 0;
        }

        nread = cat_socket_internal_try_recv_raw(
            isocket,
            read_buffer->value + read_buffer->length,
            read_buffer->size - read_buffer->length,
            address, address_length
        );

        if (unlikely(nread <= 0)) {
            return nread;
        }

        read_buffer->length += nread;
    }
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

static cat_always_inline ssize_t cat_socket_internal_try_recv(
    cat_socket_internal_t *isocket,
    char *buffer, size_t size,
    cat_sockaddr_t *address, cat_socklen_t *address_length
)
{
#ifdef CAT_SSL
    if (isocket->ssl != NULL) {
        return cat_socket_internal_try_recv_decrypted(isocket, buffer, size, address, address_length);
    }
#endif
    return cat_socket_internal_try_recv_raw(isocket, buffer, size, address, address_length);
}

CAT_STATIC_ASSERT(sizeof(cat_socket_write_vector_t) == sizeof(uv_buf_t));
CAT_STATIC_ASSERT(offsetof(cat_socket_write_vector_t, base) == offsetof(uv_buf_t, base));
CAT_STATIC_ASSERT(offsetof(cat_socket_write_vector_t, length) == offsetof(uv_buf_t, len));

CAT_API size_t cat_socket_write_vector_length(const cat_socket_write_vector_t *vector, unsigned int vector_count)
{
    return cat_io_vector_length((const cat_io_vector_t *) vector, vector_count);
}

/* IOCP/io_uring may not support wait writable */
static cat_always_inline void cat_socket_internal_write_callback(cat_socket_internal_t *isocket, cat_socket_write_request_t *request, int status)
{
    cat_coroutine_t *coroutine = request->u.coroutine;

    if (coroutine != NULL) {
        CAT_ASSERT(isocket->io_flags & CAT_SOCKET_IO_FLAG_WRITE);
        request->error = status;
        /* just resume and it will retry to send on while loop */
        cat_coroutine_schedule(coroutine, SOCKET, "Write");
    }

    if (request != isocket->cache.write_request) {
        cat_free(request);
    }
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
    cat_queue_t *queue = &isocket->context.io.write.coroutines;
    cat_bool_t ret = cat_false, queued = !!(isocket->io_flags & CAT_SOCKET_IO_FLAG_WRITE);
    ssize_t error;

    isocket->io_flags |= CAT_SOCKET_IO_FLAG_WRITE;
    cat_queue_push_back(queue, &CAT_COROUTINE_G(current)->waiter.node);
    if (queued) {
        cat_bool_t wait_ret;
        CAT_TIME_WAIT_START() {
            wait_ret = cat_time_wait(timeout);
        } CAT_TIME_WAIT_END(timeout);
        if (unlikely(!wait_ret)) {
            cat_update_last_error_with_previous("Socket write failed");
            return cat_false;
        }
        if (cat_queue_front_data(queue, cat_coroutine_t, waiter.node) != CAT_COROUTINE_G(current)) {
            cat_update_last_error(CAT_ECANCELED, "Socket write has been canceled");
            return cat_false;
        }
    }

    while (1) {
        struct msghdr msg;
        msg.msg_name = (struct sockaddr *) address;
        msg.msg_namelen = address_length;
        msg.msg_iov = (struct iovec *) vector;
        msg.msg_iovlen = vector_count;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = 0;
        do {
            error = sendmsg(fd, &msg, 0);
        } while (error < 0 && CAT_SOCKET_RETRY_ON_WRITE_ERROR(cat_sys_errno));
        if (unlikely(error < 0)) {
            if (CAT_SOCKET_IS_TRANSIENT_WRITE_ERROR(cat_sys_errno)) {
                if (unlikely(isocket->u.udg.writefd == CAT_OS_INVALID_FD)) {
                    isocket->u.udg.writefd = dup(fd);
                    if (unlikely(isocket->u.udg.writefd == CAT_OS_INVALID_FD)) {
                        goto _syscall_error;
                    }
                }
                cat_ret_t poll_ret = cat_poll_one(isocket->u.udg.writefd, POLLOUT, NULL, timeout);
                if (poll_ret == CAT_RET_OK) {
                    continue;
                } else if (poll_ret == CAT_RET_NONE) {
                    cat_update_last_error(CAT_ETIMEDOUT, "Socket UDG poll writable timedout");
                } else {
                    cat_update_last_error_with_previous("Socket UDG poll writable failed");
                }
            } else {
                _syscall_error:
                cat_update_last_error_of_syscall("Socket write failed");
            }
        } else {
            ret = cat_true;
        }
        break;
    }

    cat_queue_remove(&CAT_COROUTINE_G(current)->waiter.node);
    if (cat_queue_empty(queue)) {
        isocket->io_flags ^= CAT_SOCKET_IO_FLAG_WRITE;
    } else {
        /* resume queued coroutines */
        cat_coroutine_t *waiter = cat_queue_front_data(queue, cat_coroutine_t, waiter.node);
        cat_coroutine_schedule(waiter, SOCKET, "UDG write");
    }

    return ret;
}
#endif

static cat_bool_t cat_socket_internal_write_raw(
    cat_socket_internal_t *isocket,
    const cat_socket_write_vector_t *vector, unsigned int vector_count,
    const cat_sockaddr_t *address, cat_socklen_t address_length,
    cat_socket_t *send_handle,
    cat_timeout_t timeout
)
{
    CAT_SOCKET_CHECK_INPUT_ADDRESS(address, address_length, return cat_false);
    cat_bool_t is_udp = ((isocket->type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP);
    cat_bool_t is_dgram = (isocket->type & CAT_SOCKET_TYPE_FLAG_DGRAM);
    cat_bool_t ret = cat_false;
    cat_socket_write_request_t *request;
    size_t context_size;
    ssize_t error;

#ifdef CAT_OS_UNIX_LIKE
    /* libuv does not support dynamic address length */
    CAT_ASSERT(
        (!is_udp || address == NULL || address->sa_family != AF_UNIX || address_length >= sizeof(struct sockaddr_un)) &&
        "Socket does not support variable size UNIX address on UDP"
    );
    if (is_dgram && !is_udp) {
        ret = cat_socket_internal_udg_write(isocket, vector, vector_count, address, address_length, timeout);
        goto _out;
    }
#endif

    if (!(isocket->io_flags & CAT_SOCKET_IO_FLAG_WRITE)) {
        request = isocket->cache.write_request;
    } else {
        request = NULL;
    }
    if (unlikely(request == NULL)) {
        /* why we do not try write: on high-traffic scenarios, is_try_write will instead lead to performance */
        if (!is_udp) {
            context_size = cat_offsize_of(cat_socket_write_request_t, u.stream);
        } else {
            context_size = cat_offsize_of(cat_socket_write_request_t, u.udp);
        }
        request = (cat_socket_write_request_t *) cat_malloc(context_size);
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(request == NULL)) {
            cat_update_last_error_of_syscall("Malloc for write request failed");
            goto _out;
        }
#endif
        if (!(isocket->io_flags & CAT_SOCKET_IO_FLAG_WRITE)) {
            isocket->cache.write_request = request;
        }
    }
    if (!is_dgram) {
        error = uv_write2(
            &request->u.stream, &isocket->u.stream,
            (const uv_buf_t *) vector, vector_count,
            send_handle == NULL ? NULL : &send_handle->internal->u.stream,
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
        isocket->io_flags |= CAT_SOCKET_IO_FLAG_WRITE;
        cat_queue_push_back(&isocket->context.io.write.coroutines, &CAT_COROUTINE_G(current)->waiter.node);
        ret = cat_time_wait(timeout);
        cat_queue_remove(&CAT_COROUTINE_G(current)->waiter.node);
        request->u.coroutine = NULL;
        error = request->error;
        if (cat_queue_empty(&isocket->context.io.write.coroutines)) {
            isocket->io_flags ^= CAT_SOCKET_IO_FLAG_WRITE;
        }
        /* handle error */
        if (unlikely(!ret || request->error == CAT_ECANCELED)) {
            /* write request is in progress, it can not be cancelled gracefully,
             * so we must cancel it by socket_close(), it's unrecoverable.
             * TODO: should we wait for close done here?
             * Release buffers before write operation is totally over is dangerous on Windows */
            cat_socket_internal_unrecoverable_io_error(isocket);
        }
        if (unlikely(!ret)) {
            cat_update_last_error_with_previous("Socket write wait failed");
            goto _out;
        }
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

#ifdef CAT_OS_UNIX_LIKE
static cat_never_inline ssize_t cat_socket_internal_udg_try_write(
    cat_socket_internal_t *isocket,
    const cat_socket_write_vector_t *vector, unsigned int vector_count,
    const cat_sockaddr_t *address, cat_socklen_t address_length
)
{
    cat_socket_fd_t fd;
    struct msghdr msg;
    ssize_t nwrite;

    if (!!(isocket->io_flags & CAT_SOCKET_IO_FLAG_WRITE)) {
        return CAT_EAGAIN;
    }

    fd = cat_socket_internal_get_fd_fast(isocket);
    msg.msg_name = (struct sockaddr *) address;
    msg.msg_namelen = address_length;
    msg.msg_iov = (struct iovec *) vector;
    msg.msg_iovlen = vector_count;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;
    do {
        nwrite = sendmsg(fd, &msg, 0);
    } while (nwrite < 0 && CAT_SOCKET_RETRY_ON_WRITE_ERROR(cat_sys_errno));

    if (unlikely(nwrite < 0)) {
        return cat_translate_sys_error(cat_sys_errno);
    }
    return nwrite;
}
#endif

static ssize_t cat_socket_internal_try_write_raw(
    cat_socket_internal_t *isocket,
    const cat_socket_write_vector_t *vector, unsigned int vector_count,
    const cat_sockaddr_t *address, cat_socklen_t address_length
)
{
    CAT_SOCKET_CHECK_INPUT_ADDRESS_SILENT(address, address_length, return cat_false);
    cat_bool_t is_dgram = (isocket->type & CAT_SOCKET_TYPE_FLAG_DGRAM);

#ifdef CAT_OS_UNIX_LIKE
    cat_bool_t is_udp = ((isocket->type & CAT_SOCKET_TYPE_UDP) == CAT_SOCKET_TYPE_UDP);
    /* libuv does not support dynamic address length */
    CAT_ASSERT(
        (!is_udp || address == NULL || address->sa_family != AF_UNIX || address_length >= sizeof(struct sockaddr_un)) &&
        "Socket does not support variable size UNIX address on UDP"
    );
    if (is_dgram && !is_udp) {
        return cat_socket_internal_udg_try_write(isocket, vector, vector_count, address, address_length);
    }
#endif
    if (!is_dgram) {
        return uv_try_write(
            &isocket->u.stream,
            (const uv_buf_t *) vector, vector_count
        );
    } else {
        return uv_udp_try_send(
            &isocket->u.udp,
            (const uv_buf_t *) vector, vector_count,
            address
        );
    }
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
        cat_socket_internal_ssl_recoverability_check(isocket);
        return cat_false;
    }

    ret = cat_socket_internal_write_raw(
        isocket, (cat_socket_write_vector_t *) ssl_vector, ssl_vector_count,
        address, address_length, NULL, timeout
    );

    cat_ssl_encrypted_vector_free(ssl, ssl_vector, ssl_vector_count);

    return ret;
}

static void cat_socket_internal_try_write_encrypted_callback(uv_write_t *request, int status)
{
    cat_socket_internal_t *isocket = (cat_socket_internal_t *) request->data;
    if (status == 0) {
        isocket->ssl->write_buffer.length = 0;
    }
}

static ssize_t cat_socket_internal_try_write_encrypted(
    cat_socket_internal_t *isocket,
    const cat_socket_write_vector_t *vector, unsigned int vector_count,
    const cat_sockaddr_t *address, cat_socklen_t address_length
)
{
    cat_ssl_t *ssl = isocket->ssl; CAT_ASSERT(ssl != NULL);
    if (unlikely(ssl->write_buffer.length != 0)) {
        return CAT_EAGAIN;
    }
    cat_io_vector_t ssl_vector[8];
    unsigned int ssl_vector_count;
    ssize_t nwrite, nwrite_encrypted;
    cat_bool_t encrypted;
    cat_errno_t error = 0;

    /* Notice: we must encrypt all buffers at once,
     * otherwise we will not be able to support queued writes. */
    ssl_vector_count = CAT_ARRAY_SIZE(ssl_vector);
    CAT_PROTECT_LAST_ERROR_START() {
        encrypted = cat_ssl_encrypt(
            isocket->ssl,
            (const cat_io_vector_t *) vector, vector_count,
            ssl_vector, &ssl_vector_count
        );
        if (unlikely(!encrypted)) {
            error = cat_get_last_error_code();
        }
    } CAT_PROTECT_LAST_ERROR_END();
    if (unlikely(!encrypted)) {
        cat_socket_internal_ssl_recoverability_check(isocket);
        return error;
    }

    nwrite_encrypted = cat_socket_internal_try_write_raw(
        isocket,
        (cat_socket_write_vector_t *) ssl_vector, ssl_vector_count,
        address, address_length
    );

    if (nwrite_encrypted == CAT_EAGAIN) {
        nwrite_encrypted = 0;
    }
    if (unlikely(nwrite_encrypted < 0)) {
        nwrite = nwrite_encrypted;
    } else {
        cat_io_vector_t *ssl_vector_current = ssl_vector;
        cat_io_vector_t *ssl_vector_eof = ssl_vector + ssl_vector_count;
        size_t ssl_vector_base_offset = nwrite_encrypted;
        nwrite = cat_io_vector_length((const cat_io_vector_t *) vector, vector_count);
        while (ssl_vector_base_offset >= ssl_vector_current->length) {
            ssl_vector_base_offset -= ssl_vector_current->length;
            if (++ssl_vector_current == ssl_vector_eof) {
                break;
            }
        }
        /* Well, this could be confusing. if we can not send all encrypted data at once,
            * we really can not know how many bytes of raw data has been sent,
            * so the only thing we can do is to store the remaining data to buffer and
            * try again in the next call. */
#ifdef CAT_DEBUG
        size_t encrypted_bytes = cat_io_vector_length(ssl_vector, ssl_vector_count);
        CAT_LOG_DEBUG(SSL, "SSL %p expect send %zu encrypted bytes, actually %zu bytes was sent (raw data is %zu bytes)",
            ssl, encrypted_bytes, (size_t) nwrite_encrypted, (size_t) nwrite);
        CAT_ASSERT(((size_t) nwrite_encrypted == encrypted_bytes) ==
                    (ssl_vector_current == ssl_vector_eof));
#endif
        if (ssl_vector_current != ssl_vector_eof) {
            if (ssl_vector_current->base == ssl->write_buffer.value) {
                ssl->write_buffer.length = ssl_vector_current->length - ssl_vector_base_offset;
                memmove(ssl->write_buffer.value,
                        ssl_vector_current->base + ssl_vector_base_offset,
                        ssl->write_buffer.length);
            } else {
                cat_buffer_append(&ssl->write_buffer,
                    ssl_vector_current->base + ssl_vector_base_offset,
                    ssl_vector_current->length - ssl_vector_base_offset);
            }
            while (++ssl_vector_current < ssl_vector_eof) {
                cat_buffer_append(&ssl->write_buffer,
                    ssl_vector_current->base,
                    ssl_vector_current->length);
            }
            /* We tell caller all data has been sent, but actually they are in buffered,
                * it's ok, just like syscall write() did. */
            CAT_LOG_DEBUG(SSL, "SSL %p write buffer now has %zu bytes queued data", ssl, ssl->write_buffer.length);
            uv_write_t *request = (uv_write_t *) cat_malloc(sizeof(*request));
#if CAT_ALLOC_HANDLE_ERRORS
            if (unlikely(request == NULL)) {
                nwrite = cat_translate_sys_error(cat_sys_errno);
            } else
#endif
            {
                uv_buf_t buf;
                request->data = isocket;
                buf.base = ssl->write_buffer.value;
                buf.len = (cat_io_vector_length_t) ssl->write_buffer.length;
                int error = uv_write(request, &isocket->u.stream, &buf, 1, cat_socket_internal_try_write_encrypted_callback);
                if (unlikely(error != 0)) {
                    nwrite = error;
                }
            }
        }
    }

    cat_ssl_encrypted_vector_free(ssl, ssl_vector, ssl_vector_count);

    return nwrite;
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
    return cat_socket_internal_write_raw(isocket, vector, vector_count, address, address_length, NULL, timeout);
}

static cat_always_inline ssize_t cat_socket_internal_try_write(
    cat_socket_internal_t *isocket,
    const cat_socket_write_vector_t *vector, unsigned int vector_count,
    const cat_sockaddr_t *address, cat_socklen_t address_length
)
{
#ifdef CAT_SSL
    if (isocket->ssl != NULL) {
        return cat_socket_internal_try_write_encrypted(isocket, vector, vector_count, address, address_length);
    }
#endif
    return cat_socket_internal_try_write_raw(isocket, vector, vector_count, address, address_length);
}

#define CAT_SOCKET_INTERNAL_IO_ESTABLISHED_CHECK_FOR_STREAM_SILENT(_isocket, _failure) do { \
    if (!(_isocket->type & CAT_SOCKET_TYPE_FLAG_DGRAM)) { \
        CAT_SOCKET_INTERNAL_ESTABLISHED_ONLY_SILENT(_isocket, _failure); \
    } \
} while (0)

#define CAT_SOCKET_INTERNAL_IO_ESTABLISHED_CHECK_FOR_STREAM(_isocket, _failure) do { \
    if (!(_isocket->type & CAT_SOCKET_TYPE_FLAG_DGRAM)) { \
        CAT_SOCKET_INTERNAL_ESTABLISHED_ONLY(_isocket, _failure); \
    } \
} while (0)

#define CAT_SOCKET_IO_CHECK_RAW(_socket, _isocket, _io_flag, _failure) \
    CAT_SOCKET_INTERNAL_GETTER_WITH_IO(_socket, _isocket, _io_flag, _failure); \
    CAT_SOCKET_INTERNAL_IO_ESTABLISHED_CHECK_FOR_STREAM(_isocket, _failure) \

#define CAT_SOCKET_IO_CHECK(_socket, _isocket, _io_flag, _failure) \
        CAT_SOCKET_IO_CHECK_RAW(_socket, _isocket, _io_flag, _failure); \
        do { \
            if (unlikely((_isocket->type & CAT_SOCKET_TYPE_IPCC) == CAT_SOCKET_TYPE_IPCC)) { \
                cat_update_last_error(CAT_EMISUSE, "Socket IPC channel can not transfer user data"); \
                _failure; \
            } \
        } while (0)

#define CAT_SOCKET_TRY_IO_CHECK(_socket, _isocket, _io_flag, _failure) \
        CAT_SOCKET_INTERNAL_GETTER_WITH_IO_SILENT(_socket, _isocket, _io_flag, _failure); \
        CAT_SOCKET_INTERNAL_IO_ESTABLISHED_CHECK_FOR_STREAM_SILENT(_isocket, _failure) \

static cat_always_inline ssize_t cat_socket__read(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length, cat_timeout_t timeout, cat_bool_t once)
{
    CAT_SOCKET_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_READ, return -1);
    return cat_socket_internal_read(isocket, buffer, size, address, address_length, timeout, once);
}

static cat_always_inline ssize_t cat_socket__try_recv(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length)
{
    CAT_SOCKET_TRY_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_READ, return error);
    return cat_socket_internal_try_recv(isocket, buffer, size, address, address_length);
}

static cat_always_inline cat_bool_t cat_socket__write(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout)
{
    CAT_SOCKET_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_NONE, return cat_false);
    return cat_socket_internal_write(isocket, vector, vector_count, address, address_length, timeout);
}

static cat_always_inline ssize_t cat_socket__try_write(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    CAT_SOCKET_TRY_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_WRITE, return error == CAT_ELOCKED ? CAT_EAGAIN : error);
    return cat_socket_internal_try_write(isocket, vector, vector_count, address, address_length);
}

#define CAT_SOCKET_READ_ADDRESS_CONTEXT(_address, _name, _name_length, _port) \
    cat_sockaddr_info_t _address##_info; \
    cat_sockaddr_t *_address; \
    cat_socklen_t *_address##_length; do { \
    \
    if (unlikely((_name == NULL || _name_length == NULL || *_name_length == 0) && _port == NULL)) { \
        _address = NULL; \
        _address##_length = NULL; \
        _address##_info.length = 0; \
    } else { \
        _address = &_address##_info.address.common; \
        _address##_length = &_address##_info.length; \
        _address##_info.length = sizeof(_address##_info.address); \
    } \
} while (0)

#define CAT_SOCKET_READ_ADDRESS_TO_NAME(_address, _name, _name_length, _port) do { \
    if (unlikely(_address##_info.length > sizeof(_address##_info.address))) { \
        _address##_info.length = 0; /* address is imcomplete, just discard it */ \
    } \
    /* always call this (it can handle empty case internally) */ \
    (void) cat_sockaddr_to_name_silent(_address, _address##_info.length, _name, _name_length, _port); \
} while (0)

static ssize_t cat_socket__recv_from(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port, cat_timeout_t timeout)
{
    CAT_SOCKET_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_READ, return -1);
    CAT_SOCKET_READ_ADDRESS_CONTEXT(address, name, name_length, port);
    ssize_t ret;

    ret = cat_socket_internal_read(isocket, buffer, size, address, address_length, timeout, cat_true);

    CAT_SOCKET_READ_ADDRESS_TO_NAME(address, name, name_length, port);

    return ret;
}

static ssize_t cat_socket__try_recv_from(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port)
{
    CAT_SOCKET_TRY_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_READ, return error);
    CAT_SOCKET_READ_ADDRESS_CONTEXT(address, name, name_length, port);
    ssize_t ret;

    ret = cat_socket_internal_try_recv(isocket, buffer, size, address, address_length);

    CAT_SOCKET_READ_ADDRESS_TO_NAME(address, name, name_length, port);

    return ret;
}

#define CAT_SOCKET_INTERNAL_SOLVE_WRITE_TO_ADDRESS(isocket, name, name_length, port, address, address_length, _failure) \
    cat_sockaddr_info_t address_info; \
    cat_sockaddr_t *address; \
    cat_socklen_t address_length; \
    do { \
        cat_ret_t ret = cat_socket_internal_solve_write_to_address(isocket, name, name_length, port, &address_info); \
        if (ret == CAT_RET_ERROR) { \
            _failure; \
        } else if (ret == CAT_RET_NONE) { \
            address = NULL; \
            address_length = 0; \
        } else { \
            address = &address_info.address.common; \
            address_length = address_info.length; \
        } \
    } while (0)

static cat_always_inline cat_ret_t cat_socket_internal_solve_write_to_address(
    cat_socket_internal_t *isocket,
    const char *name, size_t name_length, int port,
    cat_sockaddr_info_t *address_info
) {
    /* resolve address (DNS query may be triggered) */
    if (name_length != 0) {
        cat_bool_t ret;
        isocket->io_flags |= CAT_SOCKET_IO_FLAG_WRITE;
        cat_queue_push_back(&isocket->context.io.write.coroutines, &CAT_COROUTINE_G(current)->waiter.node);
        ret  = cat_socket_internal_getaddrbyname(isocket, address_info, name, name_length, port, NULL);
        cat_queue_remove(&CAT_COROUTINE_G(current)->waiter.node);
        if (cat_queue_empty(&isocket->context.io.write.coroutines)) {
            isocket->io_flags ^= CAT_SOCKET_IO_FLAG_WRITE;
        }
        if (unlikely(!ret)) {
           cat_update_last_error_with_previous("Socket write failed");
           return CAT_RET_ERROR;
        }
        return CAT_RET_OK;
    } else {
        return CAT_RET_NONE;
    }
}

static cat_bool_t cat_socket__write_to(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const char *name, size_t name_length, int port, cat_timeout_t timeout)
{
    CAT_SOCKET_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_NONE, return cat_false);
    CAT_SOCKET_INTERNAL_SOLVE_WRITE_TO_ADDRESS(isocket, name, name_length, port, address, address_length, return cat_false);

    return cat_socket_internal_write(isocket, vector, vector_count, address, address_length, timeout);
}

static ssize_t cat_socket__try_write_to(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const char *name, size_t name_length, int port)
{
    CAT_SOCKET_TRY_IO_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_WRITE, return error == CAT_ELOCKED ? CAT_EAGAIN : error);
    CAT_SOCKET_INTERNAL_SOLVE_WRITE_TO_ADDRESS(isocket, name, name_length, port, address, address_length, return cat_false);

    return cat_socket_internal_try_write(isocket, vector, vector_count, address, address_length);
}

CAT_API ssize_t cat_socket_read(cat_socket_t *socket, char *buffer, size_t length)
{
    return cat_socket_read_ex(socket, buffer, length, cat_socket_get_read_timeout_fast(socket));
}

CAT_API ssize_t cat_socket_read_ex(cat_socket_t *socket, char *buffer, size_t length, cat_timeout_t timeout)
{
    return cat_socket__read(socket, buffer, length, 0, NULL, timeout, cat_false);
}

CAT_API cat_bool_t cat_socket_write(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count)
{
    return cat_socket_write_ex(socket, vector, vector_count, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_write_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, cat_timeout_t timeout)
{
    return cat_socket__write(socket, vector, vector_count, NULL, 0, timeout);
}

CAT_API ssize_t cat_socket_try_write(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count)
{
    return cat_socket_try_writeto(socket, vector, vector_count, NULL, 0);
}

CAT_API cat_bool_t cat_socket_writeto(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    return cat_socket_writeto_ex(socket, vector, vector_count, address, address_length, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_writeto_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout)
{
    return cat_socket__write(socket, vector, vector_count, address, address_length, timeout);
}

CAT_API ssize_t cat_socket_try_writeto(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    return cat_socket__try_write(socket, vector, vector_count, address, address_length);
}

CAT_API cat_bool_t cat_socket_write_to(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const char *name, size_t name_length, int port)
{
    return cat_socket_write_to_ex(socket, vector, vector_count, name, name_length, port, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_write_to_ex(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const char *name, size_t name_length, int port, cat_timeout_t timeout)
{
    return cat_socket__write_to(socket, vector, vector_count, name, name_length, port, timeout);
}

CAT_API ssize_t cat_socket_try_write_to(cat_socket_t *socket, const cat_socket_write_vector_t *vector, unsigned int vector_count, const char *name, size_t name_length, int port)
{
    return cat_socket__try_write_to(socket, vector, vector_count, name, name_length, port);
}

CAT_API ssize_t cat_socket_recv(cat_socket_t *socket, char *buffer, size_t size)
{
    return cat_socket_recv_ex(socket, buffer, size, cat_socket_get_read_timeout_fast(socket));
}

CAT_API ssize_t cat_socket_recv_ex(cat_socket_t *socket, char *buffer, size_t size, cat_timeout_t timeout)
{
    return cat_socket__read(socket, buffer, size, 0, NULL, timeout, cat_true);
}

CAT_API ssize_t cat_socket_try_recv(cat_socket_t *socket, char *buffer, size_t size)
{
    return cat_socket__try_recv(socket, buffer, size, NULL, NULL);
}

CAT_API ssize_t cat_socket_recvfrom(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length)
{
    return cat_socket_recvfrom_ex(socket, buffer, size, address, address_length, cat_socket_get_read_timeout_fast(socket));
}

CAT_API ssize_t cat_socket_recvfrom_ex(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length, cat_timeout_t timeout)
{
    return cat_socket__read(socket, buffer, size, address, address_length, timeout, cat_true);
}

CAT_API ssize_t cat_socket_try_recvfrom(cat_socket_t *socket, char *buffer, size_t size, cat_sockaddr_t *address, cat_socklen_t *address_length)
{
    return cat_socket__try_recv(socket, buffer, size, address, address_length);
}

CAT_API ssize_t cat_socket_recv_from(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port)
{
    return cat_socket_recv_from_ex(socket, buffer, size, name, name_length, port, cat_socket_get_read_timeout_fast(socket));
}

CAT_API ssize_t cat_socket_recv_from_ex(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port, cat_timeout_t timeout)
{
    return cat_socket__recv_from(socket, buffer, size, name, name_length, port, timeout);
}

CAT_API ssize_t cat_socket_try_recv_from(cat_socket_t *socket, char *buffer, size_t size, char *name, size_t *name_length, int *port)
{
    return cat_socket__try_recv_from(socket, buffer, size, name, name_length, port);
}

CAT_API cat_bool_t cat_socket_send(cat_socket_t *socket, const char *buffer, size_t length)
{
    return cat_socket_send_ex(socket, buffer, length, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_send_ex(cat_socket_t *socket, const char *buffer, size_t length, cat_timeout_t timeout)
{
    return cat_socket_sendto_ex(socket, buffer, length, NULL, 0, timeout);
}

CAT_API ssize_t cat_socket_try_send(cat_socket_t *socket, const char *buffer, size_t length)
{
    return cat_socket_try_sendto(socket, buffer, length, NULL, 0);
}

CAT_API cat_bool_t cat_socket_sendto(cat_socket_t *socket, const char *buffer, size_t length, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    return cat_socket_sendto_ex(socket, buffer, length, address, address_length, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_sendto_ex(cat_socket_t *socket, const char *buffer, size_t length, const cat_sockaddr_t *address, cat_socklen_t address_length, cat_timeout_t timeout)
{
    cat_socket_write_vector_t vector = cat_socket_write_vector_init(buffer, (cat_socket_vector_length_t) length);
    return cat_socket_writeto_ex(socket, &vector, 1, address, address_length, timeout);
}

CAT_API ssize_t cat_socket_try_sendto(cat_socket_t *socket, const char *buffer, size_t length, const cat_sockaddr_t *address, cat_socklen_t address_length)
{
    cat_socket_write_vector_t vector = cat_socket_write_vector_init(buffer, (cat_socket_vector_length_t) length);
    return cat_socket_try_writeto(socket, &vector, 1, address, address_length);
}

CAT_API cat_bool_t cat_socket_send_to(cat_socket_t *socket, const char *buffer, size_t length, const char *name, size_t name_length, int port)
{
    return cat_socket_send_to_ex(socket, buffer, length, name, name_length, port, cat_socket_get_write_timeout_fast(socket));
}

CAT_API cat_bool_t cat_socket_send_to_ex(cat_socket_t *socket, const char *buffer, size_t length, const char *name, size_t name_length, int port, cat_timeout_t timeout)
{
    cat_socket_write_vector_t vector = cat_socket_write_vector_init(buffer, (cat_socket_vector_length_t) length);
    return cat_socket_write_to_ex(socket, &vector, 1, name, name_length, port, timeout);
}

CAT_API ssize_t cat_socket_try_send_to(cat_socket_t *socket, const char *buffer, size_t length, const char *name, size_t name_length, int port)
{
    cat_socket_write_vector_t vector = cat_socket_write_vector_init(buffer, (cat_socket_vector_length_t) length);
    return cat_socket_try_write_to(socket, &vector, 1, name, name_length, port);
}

static ssize_t cat_socket_internal_peekfrom(
    const cat_socket_internal_t *isocket,
    char *buffer, size_t size,
    cat_sockaddr_t *address, cat_socklen_t *address_length
)
{
    CAT_SOCKET_INTERNAL_FD_GETTER(isocket, fd, return cat_false);
    ssize_t nread;

    if (!(isocket->type & CAT_SOCKET_TYPE_FLAG_DGRAM)) {
        if (address_length != NULL) {
            *address_length = 0;
        }
    }
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
        cat_errno_t error = cat_translate_sys_error(cat_sys_errno);
        if (unlikely(error != CAT_EAGAIN && error != CAT_EMSGSIZE)) {
            /* there was an unrecoverable error */
            cat_update_last_error_of_syscall("Socket peek failed");
        } else {
            /* not real error */
            nread = 0;
        }
        if (address_length != NULL) {
            *address_length = 0;
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

#define CAT_SOCKET_IPCC_CHECK(_socket, _isocket, _io_flags, _failure) \
    CAT_SOCKET_IO_CHECK_RAW(_socket, _isocket, _io_flags, _failure); \
    if (unlikely(!((_isocket->type & CAT_SOCKET_TYPE_IPCC) == CAT_SOCKET_TYPE_IPCC))) { \
        cat_update_last_error(CAT_EINVAL, "Socket must be named pipe with IPC enabled"); \
        _failure; \
    }

#ifndef CAT_OS_WIN
#define CAT_SOCKET_IPCC_SUPPORTS "TCP, PIPE, UDP and UDG"
#else
#define CAT_SOCKET_IPCC_SUPPORTS "TCP"
#endif

CAT_API cat_bool_t cat_socket_send_handle(cat_socket_t *socket, cat_socket_t *handle)
{
    return cat_socket_send_handle_ex(socket, handle, cat_socket_get_write_timeout_fast(socket));
}

// TODO: support send extra message
CAT_API cat_bool_t cat_socket_send_handle_ex(cat_socket_t *socket, cat_socket_t *handle, cat_timeout_t timeout)
{
    CAT_SOCKET_IPCC_CHECK(socket, isocket, CAT_SOCKET_IO_FLAG_NONE, return cat_false);
    cat_socket_internal_t *ihandle;

    if (unlikely(!cat_socket_is_available(handle))) {
        cat_update_last_error(CAT_EINVAL, "Socket can not send an unavailble handle");
        return cat_false;
    }
    ihandle = handle->internal;
    if (unlikely(!cat_socket_internal_can_be_transfer_by_ipc(ihandle))) {
        cat_update_last_error(CAT_EINVAL, "Socket can only send " CAT_SOCKET_IPCC_SUPPORTS " handle");
        return cat_false;
    }

    cat_socket_inheritance_info_t handle_info = {
        ihandle->type,
        ihandle->options
    };
    cat_socket_write_vector_t vector = cat_socket_write_vector_init((char *) &handle_info, (cat_socket_vector_length_t) sizeof(handle_info));
    if (!cat_socket_internal_write_raw(isocket, &vector, 1, NULL, 0, handle, timeout)) {
        cat_socket_internal_unrecoverable_io_error(isocket);
        return cat_false;
    }

    return cat_true;
}

static cat_bool_t cat_socket_internal_recv_handle(cat_socket_internal_t *isocket, cat_socket_internal_t *ihandle, cat_timeout_t timeout)
{
    cat_socket_inheritance_info_t *handle_info = isocket->cache.ipcc_handle_info;
    cat_socket_inheritance_info_t handle_info_storage;

    if (handle_info == NULL) {
        ssize_t nread = cat_socket_internal_read(
            isocket, (char *) &handle_info_storage, sizeof(handle_info_storage),
            NULL, NULL, timeout, cat_false
        );
        if (nread != sizeof(handle_info_storage)) {
            if (nread >= 0) {
                /* interrupt can not recover */
                cat_socket_internal_unrecoverable_io_error(isocket);
            }
            return cat_false;
        }
        handle_info = &handle_info_storage;
    } else {
        handle_info = isocket->cache.ipcc_handle_info;
    }

    /* check uv pending handle */
    CAT_ASSERT(uv_pipe_pending_count(&isocket->u.pipe) > 0);
#ifdef CAT_DEBUG
    do {
        uv_handle_type uv_handle_type = uv_pipe_pending_type(&isocket->u.pipe);
        cat_socket_type_t handle_type = 0xffffffff;
        if (uv_handle_type == UV_TCP) {
            handle_type = CAT_SOCKET_TYPE_TCP;
        } else if (uv_handle_type == UV_NAMED_PIPE) {
            handle_type = CAT_SOCKET_TYPE_PIPE;
        } else if (uv_handle_type == UV_UDP) {
            handle_type = CAT_SOCKET_TYPE_UDP;
        }
        CAT_ASSERT((handle_type & handle_info->type) == handle_type);
    } while (0);
#endif

    do {
        cat_socket_type_t server_type = cat_socket_type_simplify(handle_info->type);
        cat_socket_type_t handle_type = ihandle->type;
        if (unlikely(server_type != handle_type)) {
            cat_update_last_error(CAT_EINVAL, "Socket accept handle type mismatch, expect %s but got %s",
                cat_socket_type_name(server_type), cat_socket_type_name(handle_type));
            goto _recoverable_error;
        }
    } while (0);

    cat_bool_t ret = cat_socket_internal_accept(isocket, ihandle, handle_info, timeout);

    if (unlikely(!ret)) {
        cat_errno_t error = cat_get_last_error_code();
        if (error == CAT_EINVAL || error == CAT_EMISUSE) {
            goto _recoverable_error;
        } else {
            goto _unrecoverable_error;
        }
    }

    if (handle_info == isocket->cache.ipcc_handle_info) {
        cat_free(handle_info);
        isocket->cache.ipcc_handle_info = NULL;
    }

    return cat_true;

    _recoverable_error:
    if (handle_info != isocket->cache.ipcc_handle_info) {
        isocket->cache.ipcc_handle_info =
            (cat_socket_inheritance_info_t *) cat_memdup(handle_info, sizeof(*handle_info));
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(isocket->cache.ipcc_handle_info == NULL)) {
            /* dup failed, we can not re-accept it without handle info */
            goto _unrecoverable_error;
        }
#endif
    }
    if (0) {
        _unrecoverable_error:
        cat_socket_internal_unrecoverable_io_error(isocket);
    }

    return cat_false;
}

static cat_always_inline void cat_socket_io_cancel(cat_coroutine_t *coroutine, const char *type_name)
{
    if (coroutine != NULL) {
        /* interrupt the operation */
        cat_coroutine_schedule(coroutine, SOCKET, "Cancel %s", type_name);
    } /* else: under which case will coroutine be null except we are in try_connect()? */
}

static void cat_socket_close_callback(uv_handle_t *handle)
{
    cat_socket_internal_t *isocket = cat_container_of(handle, cat_socket_internal_t, u.handle);

#ifdef CAT_SSL
    if (isocket->ssl_peer_name != NULL) {
        cat_free(isocket->ssl_peer_name);
    }
    if (isocket->ssl != NULL) {
        cat_ssl_close(isocket->ssl);
    }
#endif

    if (isocket->cache.write_request != NULL) {
        cat_free(isocket->cache.write_request);
    }
    if (isocket->cache.ipcc_handle_info != NULL) {
        cat_free(isocket->cache.ipcc_handle_info);
    }
    if (isocket->cache.sockname != NULL) {
        cat_free(isocket->cache.sockname);
    }
    if (isocket->cache.peername != NULL) {
        cat_free(isocket->cache.peername);
    }

    cat_free(isocket);
}

/* Notice: socket may be freed before isocket closed, so we can not use socket anymore after IO wait failure  */
static void cat_socket_internal_close(cat_socket_internal_t *isocket, cat_bool_t unrecoverable_error)
{
    cat_socket_t *socket = isocket->u.socket;

    if (socket == NULL) {
        return;
    }

    socket->flags |= CAT_SOCKET_FLAG_CLOSED;
    if (unlikely(unrecoverable_error)) {
        socket->flags |= CAT_SOCKET_FLAG_UNRECOVERABLE_ERROR;
    }

    /* unbind */
    socket->internal = NULL;
    isocket->u.socket = NULL;

    /* unref in listen (references are idempotent) */
    if (unlikely(cat_socket_is_server(socket))) {
        uv_ref(&isocket->u.handle);
    }

#ifdef CAT_SSL
    if (isocket->ssl != NULL &&
        cat_ssl_get_shutdown(isocket->ssl) != (CAT_SSL_SENT_SHUTDOWN | CAT_SSL_RECEIVED_SHUTDOWN)) {
        cat_ssl_set_quiet_shutdown(isocket->ssl, cat_true);
    }
#endif

    /* cancel all IO operations */
    if (isocket->io_flags == CAT_SOCKET_IO_FLAG_BIND) {
        cat_socket_io_cancel(isocket->context.bind.coroutine, "bind");
    } else if (isocket->io_flags == CAT_SOCKET_IO_FLAG_ACCEPT) {
        cat_socket_io_cancel(isocket->context.accept.coroutine, "accept");
    } else if (isocket->io_flags == CAT_SOCKET_IO_FLAG_CONNECT) {
        cat_socket_io_cancel(isocket->context.connect.coroutine, "connect");
    } else {
        /* Notice: we cancel write first */
        if (isocket->io_flags & CAT_SOCKET_IO_FLAG_WRITE) {
            cat_queue_t *write_coroutines = &isocket->context.io.write.coroutines;
            cat_coroutine_t *write_coroutine;
            while ((write_coroutine = cat_queue_front_data(write_coroutines, cat_coroutine_t, waiter.node))) {
                cat_socket_io_cancel(write_coroutine, "write");
            }
            CAT_ASSERT(cat_queue_empty(write_coroutines));
        }
        if (isocket->io_flags & CAT_SOCKET_IO_FLAG_READ) {
            cat_socket_io_cancel(isocket->context.io.read.coroutine, "read");
        }
    }

#ifdef CAT_OS_UNIX_LIKE
    if ((isocket->type & CAT_SOCKET_TYPE_UDG) == CAT_SOCKET_TYPE_UDG) {
        if (isocket->u.udg.readfd != CAT_OS_INVALID_FD) {
            (void) uv__close(isocket->u.udg.readfd);
        }
        if (isocket->u.udg.writefd != CAT_OS_INVALID_FD) {
            (void) uv__close(isocket->u.udg.writefd);
        }
    }
#endif

    uv_close(&isocket->u.handle, cat_socket_close_callback);
}

CAT_API cat_bool_t cat_socket_close(cat_socket_t *socket)
{
    cat_socket_internal_t *isocket = socket->internal;
    cat_bool_t ret = cat_true;

    /* native EBADF will be reported from now on */
    socket->flags &= ~CAT_SOCKET_FLAG_UNRECOVERABLE_ERROR;

    if (isocket == NULL) {
        /* we do not update the last error here
         * because the only reason for close failure is
         * it has been closed */
#ifdef CAT_DEBUG
        cat_update_last_error(CAT_EBADF, NULL);
#endif
        ret = cat_false;
    } else {
        cat_socket_internal_close(isocket, cat_false);
    }

    if (socket->flags & CAT_SOCKET_FLAG_ALLOCATED) {
#if !defined(_MSC_VER) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif
        /* FIXME: maybe a bug of GCC-7.5.0
         * inlined from socket_get_local_free_port() */
        cat_free(socket);
#if !defined(_MSC_VER) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    }

    return ret;
}

static CAT_COLD void cat_socket_internal_unrecoverable_io_error(cat_socket_internal_t *isocket)
{
#ifdef CAT_SSL
    if (isocket->ssl != NULL) {
        cat_ssl_unrecoverable_error(isocket->ssl);
    }
#endif
    cat_socket_internal_close(isocket, cat_true);
}

#ifdef CAT_SSL
static cat_always_inline void cat_socket_internal_ssl_recoverability_check(cat_socket_internal_t *isocket)
{
    if (!cat_ssl_is_down(isocket->ssl)) {
        return;
    }
    cat_socket_internal_close(isocket, cat_true);
}
#endif

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

#ifdef CAT_SSL
CAT_API cat_bool_t cat_socket_has_crypto(const cat_socket_t *socket)
{
    cat_socket_internal_t *isocket = socket->internal;
    return isocket != NULL && isocket->ssl != NULL;
}

CAT_API cat_bool_t cat_socket_is_encrypted(const cat_socket_t *socket)
{
    cat_socket_internal_t *isocket = socket->internal;
    return isocket != NULL && cat_socket_internal_is_established(isocket) &&
           isocket->ssl != NULL && cat_ssl_is_established(isocket->ssl);
}
#endif

// TODO: internal version APIs
CAT_API cat_bool_t cat_socket_is_server(const cat_socket_t *socket)
{
    cat_socket_internal_t *isocket = socket->internal;
    return isocket != NULL && isocket->flags & CAT_SOCKET_INTERNAL_FLAG_SERVER;
}

CAT_API cat_bool_t cat_socket_is_server_connection(const cat_socket_t *socket)
{
    cat_socket_internal_t *isocket = socket->internal;
    return isocket != NULL && isocket->flags & CAT_SOCKET_INTERNAL_FLAG_SERVER_CONNECTION;
}

CAT_API cat_bool_t cat_socket_is_client(const cat_socket_t *socket)
{
    cat_socket_internal_t *isocket = socket->internal;
    return isocket != NULL && isocket->flags & CAT_SOCKET_INTERNAL_FLAG_CLIENT;
}

CAT_API const char *cat_socket_get_role_name(const cat_socket_t *socket)
{
    if (cat_socket_is_server_connection(socket)) {
        return "server-connection";
    }
    if (cat_socket_is_client(socket)) {
        return "client";
    }
    if (cat_socket_is_server(socket)) {
        return "server";
    }

    return "unknown";
}

#ifndef CAT_SSL
#define CAT_SOCKET_INTERNAL_SSL_LIVENESS_FAST_CHECK(isocket, on_success)
#else
#define CAT_SOCKET_INTERNAL_SSL_LIVENESS_FAST_CHECK(isocket, on_success) do { \
    if (cat_socket_internal_ssl_get_liveness(isocket)) { \
        on_success; \
    } \
} while (0)

static cat_always_inline cat_bool_t cat_socket_internal_ssl_get_liveness(const cat_socket_internal_t *isocket)
{
    return isocket->ssl != NULL && isocket->ssl->read_buffer.length > 0;
}
#endif

static cat_errno_t cat_socket_check_liveness_by_fd(cat_socket_fd_t fd)
{
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
        return (cat_errno_t) error;
    }

    return 0;
}

CAT_API cat_errno_t cat_socket_get_connection_error(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER_SILENT(socket, isocket, return CAT_EBADF);
    CAT_SOCKET_INTERNAL_FD_GETTER_SILENT(isocket, fd, return CAT_EBADF);
    CAT_SOCKET_INTERNAL_SSL_LIVENESS_FAST_CHECK(isocket, return 0);

    return cat_socket_check_liveness_by_fd(fd);
}

CAT_API cat_bool_t cat_socket_check_liveness(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);
    CAT_SOCKET_INTERNAL_FD_GETTER(isocket, fd, return cat_false);
    CAT_SOCKET_INTERNAL_SSL_LIVENESS_FAST_CHECK(isocket, return cat_true);
    cat_errno_t error;

    error = cat_socket_check_liveness_by_fd(fd);

    if (unlikely(error != 0)) {
        /* there was an unrecoverable error */
        cat_update_last_error_with_reason(error, "Socket connection is unavailable");
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
    int pagesize = (int) cat_getpagesize();

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
#define CAT_SOCKET_INTERNAL_SET_FLAG(_isocket, _flag, _enable) do { \
    if (_enable) { \
        _isocket->option_flags |= (CAT_SOCKET_OPTION_FLAG_##_flag); \
    } else { \
        _isocket->option_flags &= ~(CAT_SOCKET_OPTION_FLAG_##_flag); \
    } \
} while (0)

CAT_API cat_bool_t cat_socket_get_tcp_nodelay(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER_SILENT(socket, isocket, return cat_false);

    return !(isocket->option_flags & CAT_SOCKET_OPTION_FLAG_TCP_DELAY);
}

CAT_API cat_bool_t cat_socket_set_tcp_nodelay(cat_socket_t *socket, cat_bool_t enable)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);
    CAT_SOCKET_INTERNAL_TCP_ONLY(isocket, return cat_false);

    CAT_SOCKET_INTERNAL_SET_FLAG(isocket, TCP_DELAY, !enable);
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

CAT_API cat_bool_t cat_socket_get_tcp_keepalive(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER_SILENT(socket, isocket, return cat_false);

    return isocket->option_flags & CAT_SOCKET_OPTION_FLAG_TCP_KEEPALIVE;
}

CAT_API unsigned int cat_socket_get_tcp_keepalive_delay(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER_SILENT(socket, isocket, return 0);

    return isocket->options.tcp_keepalive_delay;
}

CAT_API cat_bool_t cat_socket_set_tcp_keepalive(cat_socket_t *socket, cat_bool_t enable, unsigned int delay)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);
    CAT_SOCKET_INTERNAL_TCP_ONLY(isocket, return cat_false);
    cat_bool_t changed;

    CAT_SOCKET_INTERNAL_SET_FLAG(isocket, TCP_KEEPALIVE, enable);
    if (enable) {
        if (delay == 0) {
            delay = CAT_SOCKET_G(options.tcp_keepalive_delay);
        }
    } else {
        delay = 0;
    }
    changed = delay != isocket->options.tcp_keepalive_delay;
    isocket->options.tcp_keepalive_delay = delay;
    if (!cat_socket_is_open(socket)) {
        return cat_true;
    }
    if (!!(isocket->u.tcp.flags & UV_HANDLE_TCP_KEEPALIVE) != enable || changed) {
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
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);
    CAT_SOCKET_INTERNAL_TCP_ONLY(isocket, return cat_false);
    int error;

    error = uv_tcp_simultaneous_accepts(&isocket->u.tcp, !enable);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket %s TCP balance accepts failed", enable ? "enable" : "disable");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_socket_get_udp_broadcast(const cat_socket_t *socket)
{
    CAT_SOCKET_INTERNAL_GETTER_SILENT(socket, isocket, return cat_false);

    return isocket->option_flags & CAT_SOCKET_OPTION_FLAG_UDP_BROADCAST;
}

CAT_API cat_bool_t cat_socket_set_udp_broadcast(cat_socket_t *socket, cat_bool_t enable)
{
    CAT_SOCKET_INTERNAL_GETTER(socket, isocket, return cat_false);
    CAT_SOCKET_INTERNAL_TCP_ONLY(isocket, return cat_false);
    int error;

    CAT_SOCKET_INTERNAL_SET_FLAG(isocket, UDP_BROADCAST, enable);
    if (!cat_socket_is_open(socket)) {
        return cat_true;
    }
    error = uv_udp_set_broadcast(&isocket->u.udp, enable);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Socket %s UDP broadcast failed", enable ? "enable" : "disable");
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
    (void) arg;

    cat_socket_t *socket;
    cat_socket_internal_t *isocket;
    cat_socket_fd_t fd;
    const char *type_name, *io_state_naming, *role;
    char sock_addr[CAT_SOCKADDR_MAX_PATH] = "unknown", peer_addr[CAT_SOCKADDR_MAX_PATH] = "unknown";
    size_t sock_addr_size = sizeof(sock_addr), peer_addr_size = sizeof(peer_addr);
    int sock_port, peer_port;

    // this convert makes MSVC happy (C4061)
    switch ((int) handle->type) {
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

    CAT_LOG_INFO(SOCKET, "%-4s fd: %-6d io: %-12s role: %-7s addr: %s:%d, peer: %s:%d",
                     type_name, (int) fd, io_state_naming, role, sock_addr, sock_port, peer_addr, peer_port);
}

CAT_API void cat_socket_dump_all(void)
{
    uv_walk(&CAT_EVENT_G(loop), cat_socket_dump_callback, NULL);
}

static void cat_socket_close_by_handle_callback(uv_handle_t* handle, void* arg)
{
    (void) arg;

    cat_socket_t *socket;
    cat_socket_internal_t *isocket;

    // this convert makes MSVC happy (C4061)
    switch ((int) handle->type) {
        case UV_TCP:
            isocket = cat_container_of(handle, cat_socket_internal_t, u.tcp);
            break;
        case UV_NAMED_PIPE:
            isocket = cat_container_of(handle, cat_socket_internal_t, u.pipe);
            break;
        case UV_TTY:
            isocket = cat_container_of(handle, cat_socket_internal_t, u.tty);
            break;
        case UV_UDP:
            isocket = cat_container_of(handle, cat_socket_internal_t, u.udp);
            break;
        default:
            return;
    }

    socket = isocket->u.socket;
    if (socket == NULL) {
        return;
    }

    cat_socket_close(socket);
}

CAT_API void cat_socket_close_all(void)
{
    uv_walk(&CAT_EVENT_G(loop), cat_socket_close_by_handle_callback, NULL);
}

CAT_API cat_bool_t cat_socket_move(cat_socket_t *from, cat_socket_t *to)
{
    if (from == to) {
        return cat_true;
    }
    if (from->internal != NULL && from->internal->io_flags != CAT_SOCKET_IO_FLAG_NONE) {
        // TODO: make it work
        cat_update_last_error(CAT_EMISUSE, "Socket is immovable during IO operations");
        return cat_false;
    }

    *to = *from;
    if (from->internal != NULL) {
        from->internal = NULL;
        to->internal->u.socket = to;
    }

    return cat_true;
}

/* pipe */

CAT_API cat_bool_t cat_pipe(cat_os_fd_t fds[2], cat_pipe_flags read_flags, cat_pipe_flags write_flags)
{
    int error;

    error = uv_pipe(fds, read_flags, write_flags);

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Pipe create failed");
        return cat_false;
    }

    return cat_true;
}
