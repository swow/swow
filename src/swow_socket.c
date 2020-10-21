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

#include "swow_socket.h"
#include "swow_buffer.h"

SWOW_API zend_class_entry *swow_socket_ce;
SWOW_API zend_object_handlers swow_socket_handlers;

SWOW_API zend_class_entry *swow_socket_exception_ce;

#define SWOW_SOCKET_GETTER_INTERNAL(_object, _ssocket, _socket) \
    swow_socket_t *_ssocket = swow_socket_get_from_object(_object); \
    cat_socket_t *_socket = &_ssocket->socket

#define SWOW_SOCKET_GETTER(_ssocket, _socket) \
        SWOW_SOCKET_GETTER_INTERNAL(Z_OBJ_P(ZEND_THIS), _ssocket, _socket)

static zend_object *swow_socket_create_object(zend_class_entry *ce)
{
    swow_socket_t *ssocket = swow_object_alloc(swow_socket_t, ce, swow_socket_handlers);

    cat_socket_init(&ssocket->socket);

    return &ssocket->std;
}

static void swow_socket_dtor_object(zend_object *object)
{
    /* try to call __destruct first */
    zend_objects_destroy_object(object);

    /* force close the socket */
    SWOW_SOCKET_GETTER_INTERNAL(object, ssocket, socket);

    if (cat_socket_is_available(socket)) {
        cat_socket_close(socket);
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_swow_socket___construct, 0, ZEND_RETURN_VALUE, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, type, IS_LONG, 0, "Swow\\Socket::TYPE_TCP")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, fd, IS_LONG, 0, "Swow\\Socket::INVALID_FD")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, __construct)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zend_long type = CAT_SOCKET_TYPE_TCP;
    zend_long fd = CAT_SOCKET_INVALID_FD;

    if (UNEXPECTED(cat_socket_is_available(socket))) {
        zend_throw_error(NULL, "%s can only construct once", ZEND_THIS_NAME);
        RETURN_THROWS();
    }

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(type)
        Z_PARAM_LONG(fd)
    ZEND_PARSE_PARAMETERS_END();

    socket = cat_socket_create_ex(socket, type, fd);

    if (UNEXPECTED(socket == NULL)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_getLong, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_getString, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_socket_getType arginfo_swow_socket_getLong

static PHP_METHOD(swow_socket, getType)
{
    SWOW_SOCKET_GETTER(ssocket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(socket->type);
}

#define arginfo_swow_socket_getTypeName arginfo_swow_socket_getString

static PHP_METHOD(swow_socket, getTypeName)
{
    SWOW_SOCKET_GETTER(ssocket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_socket_get_type_name(socket));
}

#define arginfo_swow_socket_getFd arginfo_swow_socket_getLong

static PHP_METHOD(swow_socket, getFd)
{
    SWOW_SOCKET_GETTER(ssocket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_socket_get_fd(socket));
}

#define arginfo_swow_socket_getGlobalTimeout arginfo_swow_socket_getLong

ZEND_BEGIN_ARG_INFO_EX(arginfo_swow_socket_setGlobalTimeout, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_TYPE_INFO(0, timeout, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_socket_getTimeout arginfo_swow_socket_getLong

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_setTimeout, ZEND_RETURN_VALUE, 1, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO(0, timeout, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define SWOW_SOCKET_TIMEOUT_API_GEN(Type, type) \
\
static PHP_METHOD(swow_socket, getGlobal##Type##Timeout) \
{ \
    ZEND_PARSE_PARAMETERS_NONE(); \
    \
    RETURN_LONG(cat_socket_get_global_##type##_timeout()); \
} \
\
static PHP_METHOD(swow_socket, setGlobal##Type##Timeout) \
{ \
    zend_long timeout; \
    \
    ZEND_PARSE_PARAMETERS_START(1, 1) \
        Z_PARAM_LONG(timeout) \
    ZEND_PARSE_PARAMETERS_END(); \
    \
    cat_socket_set_global_##type##_timeout(timeout); \
} \
\
static PHP_METHOD(swow_socket, get##Type##Timeout) \
{ \
    SWOW_SOCKET_GETTER(ssocket, socket); \
    cat_timeout_t timeout; \
    \
    ZEND_PARSE_PARAMETERS_NONE(); \
    \
    timeout = cat_socket_get_##type##_timeout(socket); \
    \
    if (UNEXPECTED(timeout == CAT_TIMEOUT_INVALID)) { \
        swow_throw_exception_with_last(swow_socket_exception_ce); \
        RETURN_THROWS(); \
    } \
    \
    RETURN_LONG(timeout); \
} \
\
static PHP_METHOD(swow_socket, set##Type##Timeout) \
{ \
    SWOW_SOCKET_GETTER(ssocket, socket); \
    zend_long timeout; \
    cat_bool_t ret; \
    \
    ZEND_PARSE_PARAMETERS_START(1, 1) \
        Z_PARAM_LONG(timeout) \
    ZEND_PARSE_PARAMETERS_END(); \
    \
    ret = cat_socket_set_##type##_timeout(socket, timeout); \
    \
    if (UNEXPECTED(!ret)) { \
        swow_throw_exception_with_last(swow_socket_exception_ce); \
        RETURN_THROWS(); \
    } \
    \
    RETURN_THIS(); \
}

SWOW_SOCKET_TIMEOUT_API_GEN(Dns,         dns);
SWOW_SOCKET_TIMEOUT_API_GEN(Accept,   accept);
SWOW_SOCKET_TIMEOUT_API_GEN(Connect, connect);
SWOW_SOCKET_TIMEOUT_API_GEN(Read,       read);
SWOW_SOCKET_TIMEOUT_API_GEN(Write,     write);

#undef SWOW_SOCKET_TIMEOUT_API_GEN

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_bind, ZEND_RETURN_VALUE, 1, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, port, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "Swow\\Socket::BIND_FLAG_NONE")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, bind)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zend_string *name;
    zend_long port = 0;
    zend_long flags = CAT_SOCKET_BIND_FLAG_NONE;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_STR(name)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(port)
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    ret = cat_socket_bind_ex(socket, ZSTR_VAL(name), ZSTR_LEN(name), port, flags);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_listen, ZEND_RETURN_VALUE, 0, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, backlog, IS_LONG, 0, "Swow\\Socket::DEFAULT_BACKLOG")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, listen)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zend_long backlog = CAT_SOCKET_DEFAULT_BACKLOG;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(backlog)
    ZEND_PARSE_PARAMETERS_END();

    ret = cat_socket_listen(socket, backlog);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_accept, ZEND_RETURN_VALUE, 0, Swow\\Socket, 0)
    ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE(0, object, Swow\\Socket, 1, "\'$this\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getAcceptTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, accept)
{
    SWOW_SOCKET_GETTER(sserver, server);
    zval *zclient = NULL;
    zend_long timeout;
    zend_bool timeout_is_null = 1;
    swow_socket_t *sclient;
    cat_socket_t *client;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_OBJECT_OF_CLASS_EX(zclient, swow_socket_ce, 1, 0)
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
    ZEND_PARSE_PARAMETERS_END();

    if (zclient == NULL) {
        sclient = swow_socket_get_from_object(
            swow_socket_create_object(Z_OBJCE_P(ZEND_THIS))
        );
    } else {
        sclient = swow_socket_get_from_object(Z_OBJ_P(zclient));
        GC_ADDREF(&sclient->std);
    }
    client = &sclient->socket;
    if (timeout_is_null) {
        timeout = cat_socket_get_accept_timeout(server);
    }

    client = cat_socket_accept_ex(server, client, timeout);

    if (UNEXPECTED(client == NULL)) {
        zend_object_release(&sclient->std);
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_OBJ(&sclient->std);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_connect, ZEND_RETURN_VALUE, 1, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, port, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getConnectTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, connect)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zend_string *name;
    zend_long port = 0;
    zend_long timeout;
    zend_bool timeout_is_null = 1;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_STR(name)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(port)
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
    ZEND_PARSE_PARAMETERS_END();

    if (timeout_is_null) {
        timeout = cat_socket_get_connect_timeout(socket);
    }

    ret = cat_socket_connect_ex(socket, ZSTR_VAL(name), ZSTR_LEN(name), port, timeout);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

#define arginfo_swow_socket_getAddress arginfo_swow_socket_getString

static PHP_METHOD_EX(swow_socket, getAddress, zend_bool is_peer)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    char buffer[CAT_SOCKADDR_MAX_PATH];
    size_t buffer_length = sizeof(buffer);
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = cat_socket_get_address(socket, buffer, &buffer_length, is_peer);

    if (unlikely(!ret)){
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_STRINGL_FAST(buffer, buffer_length);
}

#define arginfo_swow_socket_getPort arginfo_swow_socket_getLong

static PHP_METHOD_EX(swow_socket, getPort, zend_bool is_peer)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zend_long port;

    ZEND_PARSE_PARAMETERS_NONE();

    if (EXPECTED(!(socket->type & CAT_SOCKET_TYPE_FLAG_LOCAL))) {
        port = cat_socket_get_port(socket, is_peer);
        if (UNEXPECTED(port <= 0)) {
            swow_throw_exception_with_last(swow_socket_exception_ce);
            RETURN_THROWS();
        }
    } else {
        zend_throw_error(NULL, "Local socket has no port");
        RETURN_THROWS();
    }

    RETURN_LONG(port);
}

static PHP_METHOD(swow_socket, getSockAddress)
{
    PHP_METHOD_CALL(swow_socket, getAddress, cat_false);
}

static PHP_METHOD(swow_socket, getPeerAddress)
{
    PHP_METHOD_CALL(swow_socket, getAddress, cat_true);
}

static PHP_METHOD(swow_socket, getSockPort)
{
    PHP_METHOD_CALL(swow_socket, getPort, cat_false);
}

static PHP_METHOD(swow_socket, getPeerPort)
{
    PHP_METHOD_CALL(swow_socket, getPort, cat_true);
}

static PHP_METHOD_EX(swow_socket, _read, zend_bool once, zend_bool may_address, zend_bool peek)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    uint32_t max_num_args;
    zval *zbuffer;
    zend_long size;
    zend_bool size_is_null = 1;
    zval *zaddress = NULL, *zport = NULL;
    zend_long timeout;
    zend_bool timeout_is_null = 1;
    swow_buffer_t *sbuffer;
    char *ptr;
    size_t writable_size;
    cat_bool_t want_address;
    ssize_t ret;

    max_num_args = 2;
    if (!peek) {
        max_num_args += 1;
    }
    if (may_address) {
        max_num_args += 2;
    }

    ZEND_PARSE_PARAMETERS_START(1, max_num_args)
        Z_PARAM_OBJECT_OF_CLASS(zbuffer, swow_buffer_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(size, size_is_null)
        if (may_address) {
            Z_PARAM_ZVAL_EX(zaddress, 0, 1)
            Z_PARAM_ZVAL_EX(zport, 0, 1)
        }
        if (!peek) {
            Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
        }
    ZEND_PARSE_PARAMETERS_END();

    /* check args and initialize */
    sbuffer = swow_buffer_get_from_object(Z_OBJ_P(zbuffer));
    writable_size = swow_buffer_get_writable_space(sbuffer, &ptr);
    if (size_is_null) {
        size = writable_size;
    } else if (UNEXPECTED(size <= 0)) {
        zend_argument_value_error(2, "must be greater than 0");
        RETURN_THROWS();
    }
    if (UNEXPECTED(size == 0 || (size_t) size > writable_size)) {
        swow_throw_exception(swow_socket_exception_ce, CAT_ENOBUFS, "No enough writable buffer space");
        RETURN_THROWS();
    }
    if (timeout_is_null) {
        timeout = cat_socket_get_read_timeout(socket);
    }
    if (!may_address) {
        want_address = cat_false;
    } else {
        want_address = zaddress != NULL || zport != NULL;
    }

    SWOW_BUFFER_LOCK(sbuffer);

    /* read */
    if (!want_address) {
        if (!peek) {
            if (!once) {
                ret = cat_socket_read_ex(socket, ptr, size, timeout);
            } else {
                ret = cat_socket_recv_ex(socket, ptr, size, timeout);
            }
        } else {
            ret = cat_socket_peek(socket, ptr, size);
        }
    } else {
        char address[CAT_SOCKADDR_MAX_PATH];
        size_t address_length = sizeof(address);
        int port;
        if (!peek) {
            CAT_ASSERT(once);
            ret = cat_socket_read_from_ex(socket, address, size, address, &address_length, &port, timeout);
        } else {
            ret = cat_socket_peek_from(socket, ptr, size, address, &address_length, &port);
        }
        if (zaddress != NULL) {
            zval_ptr_dtor(zaddress);
            if (address_length == 0 || address_length > sizeof(address) /* error */) {
                ZVAL_EMPTY_STRING(zaddress);
            } else {
                ZVAL_STRINGL(zaddress, address, address_length);
            }
        }
        if (zport != NULL) {
            zval_ptr_dtor(zport);
            ZVAL_LONG(zport, port);
        }
    }

    SWOW_BUFFER_UNLOCK(sbuffer);

    if (EXPECTED(ret > 0)) {
        swow_buffer_virtual_write_no_seek(sbuffer, ret);
    }

    /* also for socket exception getReturnValue */
    RETVAL_LONG(ret);

    /* handle error */
    if (UNEXPECTED(ret < 0 || (!once && ret != size))) {
        swow_throw_call_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_read, ZEND_RETURN_VALUE, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 1, "\'$this->getWritableSize()\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getReadTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, read)
{
    PHP_METHOD_CALL(swow_socket, _read, 0, 0, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_recv, ZEND_RETURN_VALUE, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 1, "\'$this->getWritableSize()\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getReadTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, recv)
{
    PHP_METHOD_CALL(swow_socket, _read, 1, 0, 0);
}

#define SWOW_SOCKET_THROW_CONNRESET_EXCEPTION_AND_RETURN_IF(condition) do { \
    if (UNEXPECTED(condition)) { \
        swow_throw_call_exception(swow_socket_exception_ce, 0, "Connection closed normally by peer while waiting for data"); \
        RETURN_THROWS(); \
    } \
} while (0)

#define arginfo_swow_socket_recvData arginfo_swow_socket_recv

static PHP_METHOD(swow_socket, recvData)
{
    PHP_METHOD_CALL(swow_socket, recv);
    SWOW_SOCKET_THROW_CONNRESET_EXCEPTION_AND_RETURN_IF(Z_LVAL_P(return_value) == 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_recvFrom, ZEND_RETURN_VALUE, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 1, "\'$this->getWritableSize()\'")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, address, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, port, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getReadTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, recvFrom)
{
    PHP_METHOD_CALL(swow_socket, _read, 1, 1, 0);
}

#define arginfo_swow_socket_recvDataFrom arginfo_swow_socket_recvFrom

static PHP_METHOD(swow_socket, recvDataFrom)
{
    PHP_METHOD_CALL(swow_socket, recvFrom);
    SWOW_SOCKET_THROW_CONNRESET_EXCEPTION_AND_RETURN_IF(Z_LVAL_P(return_value) == 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_peek, ZEND_RETURN_VALUE, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 1, "\'$this->getWritableSize()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, peek)
{
    PHP_METHOD_CALL(swow_socket, _read, 0, 0, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_peekFrom, ZEND_RETURN_VALUE, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 1, "\'$this->getWritableSize()\'")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, address, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, port, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, peekFrom)
{
    PHP_METHOD_CALL(swow_socket, _read, 1, 1, 1);
}

static PHP_METHOD_EX(swow_socket, _readString, zend_bool once, zend_bool may_address, zend_bool peek)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    uint32_t max_num_args;
    zend_string *string;
    zend_long size;
    zend_bool size_is_null = 1;
    zval *zaddress = NULL, *zport = NULL;
    zend_long timeout;
    zend_bool timeout_is_null = 1;
    char *ptr;
    cat_bool_t want_address;
    ssize_t ret;

    if (peek) {
        max_num_args = 1;
    } else {
        max_num_args = 2;
    }
    if (may_address) {
        max_num_args += 2;
    }

    ZEND_PARSE_PARAMETERS_START(0, max_num_args)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(size, size_is_null)
        if (may_address) {
            Z_PARAM_ZVAL_EX(zaddress, 0, 1)
            Z_PARAM_ZVAL_EX(zport, 0, 1)
        }
        if (!peek) {
            Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
        }
    ZEND_PARSE_PARAMETERS_END();

    /* check args and initialize */
    if (size_is_null) {
        size = CAT_BUFFER_DEFAULT_SIZE;
    } else if (UNEXPECTED(size <= 0)) {
        zend_argument_value_error(1, "must be greater than 0");
        RETURN_THROWS();
    }
    if (timeout_is_null) {
        timeout = cat_socket_get_read_timeout(socket);
    }
    string = zend_string_alloc(size, 0);
    if (!may_address) {
        want_address = cat_false;
    } else {
        want_address = zaddress != NULL || zport != NULL;
    }

    ptr = ZSTR_VAL(string);
    if (!want_address) {
        if (!peek) {
            if (!once) {
                ret = cat_socket_read_ex(socket, ptr, size, timeout);
            } else {
                ret = cat_socket_recv_ex(socket, ptr, size, timeout);
            }
        } else {
            ret = cat_socket_peek(socket, ptr, size);
        }
    } else {
        char address[CAT_SOCKADDR_MAX_PATH];
        size_t address_length = sizeof(address);
        int port;
        if (!peek) {
            CAT_ASSERT(once);
            ret = cat_socket_read_from_ex(socket, ptr, size, address, &address_length, &port, timeout);
        } else {
            ret = cat_socket_peek_from(socket, ptr, size, address, &address_length, &port);
        }
        if (zaddress != NULL) {
            zval_ptr_dtor(zaddress);
            if (address_length == 0 || address_length > sizeof(address)) {
                ZVAL_EMPTY_STRING(zaddress);
            } else {
                ZVAL_STRINGL(zaddress, address, address_length);
            }
        }
        if (zport != NULL) {
            zval_ptr_dtor(zport);
            ZVAL_LONG(zport, port);
        }
    }

    /* solve string return value */
    if (EXPECTED(ret > 0)) {
        ZSTR_VAL(string)[ZSTR_LEN(string) = ret] = '\0';
    } else {
        zend_string_release_ex(string, 0);
        string = zend_empty_string;
    }

    /* also for socket exception getReturnValue */
    RETVAL_STR(string);

    /* handle error */
    if (UNEXPECTED(ret < 0 || (!once && ret != size))) {
        swow_throw_call_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_readString, ZEND_RETURN_VALUE, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 1, "\'$this->getWritableSize()\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getReadTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, readString)
{
    PHP_METHOD_CALL(swow_socket, _readString, 0, 0, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_recvString, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 1, "\'$this->getWritableSize()\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getReadTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, recvString)
{
    PHP_METHOD_CALL(swow_socket, _readString, 1, 0, 0);
}

#define arginfo_swow_socket_recvStringData arginfo_swow_socket_recvString

static PHP_METHOD(swow_socket, recvStringData)
{
    PHP_METHOD_CALL(swow_socket, recvString);
    SWOW_SOCKET_THROW_CONNRESET_EXCEPTION_AND_RETURN_IF(Z_STRLEN_P(return_value) == 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_recvStringFrom, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 1, "\'$this->getWritableSize()\'")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, address, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, port, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getReadTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, recvStringFrom)
{
    PHP_METHOD_CALL(swow_socket, _readString, 1, 1, 0);
}

#define arginfo_swow_socket_recvStringDataFrom arginfo_swow_socket_recvStringFrom

static PHP_METHOD(swow_socket, recvStringDataFrom)
{
    PHP_METHOD_CALL(swow_socket, recvStringFrom);
    SWOW_SOCKET_THROW_CONNRESET_EXCEPTION_AND_RETURN_IF(Z_STRLEN_P(return_value) == 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_peekString, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 1, "\'$this->getWritableSize()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, peekString)
{
    PHP_METHOD_CALL(swow_socket, _readString, 1, 0, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_peekStringFrom, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 1, "\'$this->getWritableSize()\'")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, address, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, port, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, peekStringFrom)
{
    PHP_METHOD_CALL(swow_socket, _readString, 1, 1, 1);
}

static PHP_METHOD_EX(swow_socket, _write, zend_bool single, zend_bool may_address)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    uint32_t max_num_args;
    zval *zbuffer = NULL;
    zend_string *string = NULL;
    zend_long offset = 0;
    zend_long length = 0;
    zend_bool length_is_null = 1;
    zend_string *address = NULL;
    zend_long port = 0;
    zend_long timeout;
    zend_bool timeout_is_null = 1;
    swow_buffer_t *sbuffer;
    const char *ptr;
    size_t readable_length;
    cat_socket_write_vector_t *vectors, *hvectors = NULL, svectors[8];
    uint32_t vector_count = 0;
    swow_buffer_t **sbuffers, **hsbuffers = NULL, *ssbuffers[8]; /* lock/unlock buffers */
    uint32_t buffer_count = 0;
    cat_bool_t ret;

    max_num_args = 2;
    if (single) {
        max_num_args += 1;
    }
    if (may_address) {
        max_num_args += 2;
    }

    vectors = svectors;
    sbuffers = ssbuffers;
    ZEND_PARSE_PARAMETERS_START(1, max_num_args)
        if (!single) {
            HashTable *vectors_array;
            uint32_t vectors_array_count;
            Z_PARAM_ARRAY_HT(vectors_array)
            vectors_array_count = zend_hash_num_elements(vectors_array);
            if (UNEXPECTED(vectors_array_count == 0)) {
                zend_argument_value_error(1, "can not be empty");
                goto _error;
            }
            if (UNEXPECTED(vectors_array_count > CAT_ARRAY_SIZE(svectors))) {
                vectors = hvectors = emalloc(vectors_array_count * sizeof(*vectors));
            }
            if (UNEXPECTED(vectors_array_count > CAT_ARRAY_SIZE(ssbuffers))) {
                sbuffers = hsbuffers = emalloc(vectors_array_count * sizeof(*sbuffers));
            }
            do {
                zval *ztmp;
                ZEND_HASH_FOREACH_VAL(vectors_array, ztmp) {
                    /* the last one can be null */
                    if (ZVAL_IS_NULL(ztmp)) {
                        break;
                    }
                    if (EXPECTED(Z_TYPE_P(ztmp) == IS_STRING)) {
                        /* just a string */
                        string = Z_STR_P(ztmp);
                    } else if (Z_TYPE_P(ztmp) == IS_ARRAY) {
                        /* array include paramaters */
                        HashTable *vector_array = Z_ARR_P(ztmp);
                        uint32_t vector_array_count = zend_hash_num_elements(vector_array);
                        Bucket *bucket = vector_array->arData;
                        Bucket *bucket_end = bucket + vector_array->nNumUsed;
                        if (UNEXPECTED(vector_array_count < 1 || vector_array_count > 2)) {
                            zend_argument_value_error(1, "[%u] must have 1 or 2 elements as paramaters", vector_count);
                            goto _error;
                        }
/* TODO: let it be API */
#define _CURRENT_ZVAL_(bucket, bucket_end, ztmp) do { \
        while (1) { \
            ztmp = &bucket->val; \
            if (EXPECTED(Z_TYPE_P(ztmp) != IS_UNDEF)) { \
                break; \
            } \
            if (bucket != bucket_end) { \
                bucket++; \
            } else { \
                ztmp = NULL; \
                break; \
            } \
        } \
} while (0)
#define _NEXT_ZVAL_(bucket, bucket_end, ztmp) do { \
        if (bucket != bucket_end) { \
            bucket++; \
            _CURRENT_ZVAL_(bucket, bucket_end, ztmp); \
        } else { \
            ztmp = NULL; \
        } \
} while (0)
                        _CURRENT_ZVAL_(bucket, bucket_end, ztmp);
                        CAT_ASSERT(ztmp != NULL);
                        if (EXPECTED(Z_TYPE_P(ztmp) == IS_STRING)) {
                            /* [string, offset, length] */
                            if (1) {
                                string = Z_STR_P(ztmp);
                            } else {
                                _maybe_stringable_object:
                                if (UNEXPECTED(!zend_parse_arg_str(ztmp, &string, 0))) {
                                    zend_argument_value_error(1, "[%u][string] must be type of string, %s given", vector_count, zend_zval_type_name(ztmp));
                                    goto _error;
                                }
                            }
                            _NEXT_ZVAL_(bucket, bucket_end, ztmp);
                            if (UNEXPECTED(ztmp!= NULL && !zend_parse_arg_long(ztmp, &offset, NULL, 0))) {
                                zend_argument_value_error(1, "[%u][offset] must be type of long", vector_count);
                                goto _error;
                            }
                            _NEXT_ZVAL_(bucket, bucket_end, ztmp);
                            if (UNEXPECTED(ztmp != NULL && !zend_parse_arg_long(ztmp, &length, NULL, 0))) {
                                zend_argument_value_error(1, "[%u][length] must be type of long", vector_count);
                                goto _error;
                            }
                        } else {
                            /* [buffer, length] */
                            if (!zend_parse_arg_object(ztmp, &zbuffer, swow_buffer_ce, 0)) {
                                if (Z_TYPE_P(ztmp) == IS_OBJECT) {
                                    goto _maybe_stringable_object;
                                }
                                zend_argument_value_error(1, "[%u][buffer] must be type of %s, %s given", vector_count, ZSTR_VAL(swow_buffer_ce->name), zend_zval_type_name(ztmp));
                                goto _error;
                            }
                            _NEXT_ZVAL_(bucket, bucket_end, ztmp);
                            if (UNEXPECTED(ztmp != NULL && !zend_parse_arg_long(ztmp, &length, &length_is_null, 1))) {
                                zend_argument_value_error(1, "[%u][length] must be type of long or null, %s given", vector_count, zend_zval_type_name(ztmp));
                                goto _error;
                            }
                        }
#undef _CURRENT_ZVAL_
#undef _NEXT_ZVAL_
                    } else if (zend_parse_arg_object(ztmp, &zbuffer, swow_buffer_ce, 0)) {
                        /* buffer object (do othing) */
                    } else {
                        /* stringable object  */
                        if (UNEXPECTED(!zend_parse_arg_str_slow(ztmp, &string))) {
                            zend_argument_value_error(1, "[%u] must be type of string, array or %s, %s given", vector_count, ZSTR_VAL(swow_buffer_ce->name), zend_zval_type_name(ztmp));
                            goto _error;
                        }
                    }
                    if (EXPECTED(string != NULL)) {
                        SWOW_BUFFER_CHECK_STRING_SCOPE_EX(string, offset, length, goto _error);
                        if (length == 0) {
                            continue;
                        }
                        ptr = ZSTR_VAL(string) + offset;
                    } else {
                        CAT_ASSERT(zbuffer != NULL);
                        sbuffer = swow_buffer_get_from_object(Z_OBJ_P(zbuffer));
                        readable_length = swow_buffer_get_readable_space(sbuffer, &ptr);
                        if (length_is_null) {
                            length = readable_length;
                        } else if (UNEXPECTED(length < 0)) {
                            zend_argument_value_error(1, "[%u] length can not be negative", vector_count);
                            goto _error;
                        }
                        if (length == 0) {
                            continue;
                        }
                        if (UNEXPECTED((size_t) length > readable_length)) {
                            swow_throw_exception(swow_socket_exception_ce, CAT_ENOBUFS, "No enough readable buffer space on vectors[%u]", vector_count);
                            goto _error;
                        }
                        SWOW_BUFFER_LOCK_EX(sbuffer, goto _error);
                        sbuffers[buffer_count++] = sbuffer;
                    }
                    vectors[vector_count].base = ptr;
                    vectors[vector_count].length = length;
                    vector_count++;
                    /* reset arguments */
                    zbuffer = NULL;
                    string = NULL;
                    offset = 0;
                    length = 0;
                    length_is_null = 1;
                } ZEND_HASH_FOREACH_END();
            } while (0);
            Z_PARAM_OPTIONAL
        } else {
            vector_count++;
            Z_PARAM_OBJECT_OF_CLASS(zbuffer, swow_buffer_ce)
            Z_PARAM_OPTIONAL
            Z_PARAM_LONG_OR_NULL(length, length_is_null)
        }
        if (may_address) {
            Z_PARAM_STR(address)
            Z_PARAM_LONG(port)
        }
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
    ZEND_PARSE_PARAMETERS_END_EX(goto _error);

    /* check args and initialize */
    if (single) {
        sbuffer = swow_buffer_get_from_object(Z_OBJ_P(zbuffer));
        readable_length = swow_buffer_get_readable_space(sbuffer, &ptr);
        if (length_is_null) {
            length = readable_length;
        } else if (UNEXPECTED(length < 0)) {
            zend_argument_value_error(2, "can not be negative");
            goto _error;
        }
        if (UNEXPECTED((size_t) length > readable_length)) {
            swow_throw_exception(swow_socket_exception_ce, CAT_ENOBUFS, "No enough readable buffer space");
            goto _error;
        }
        SWOW_BUFFER_LOCK_EX(sbuffer, goto _error);
        sbuffers[buffer_count++] = sbuffer;
        vectors[0].base = ptr;
        vectors[0].length = length;
    } else if (UNEXPECTED(vector_count == 0)) {
        zend_argument_value_error(1, "can not be empty");
        goto _error;
    }
    if (timeout_is_null) {
        timeout = cat_socket_get_read_timeout(socket);
    }

    /* write */
    if (!may_address || address == NULL || ZSTR_LEN(address) == 0) {
        ret = cat_socket_write_ex(socket, vectors, vector_count, timeout);
    } else {
        ret = cat_socket_write_to_ex(socket, vectors, vector_count, ZSTR_VAL(address), ZSTR_LEN(address), port, timeout);
    }

    /* we won't update buffer offset */
    if (UNEXPECTED(!ret)) {
        swow_throw_call_exception_with_last(swow_socket_exception_ce);
        goto _error;
    }

    RETVAL_THIS();

    if (0) {
        _error:
        RETURN_THROWS_ASSERTION();
    }
    while (buffer_count--) {
        SWOW_BUFFER_UNLOCK(sbuffers[buffer_count]);
    }
    if (UNEXPECTED(hsbuffers != NULL)) {
        efree(hsbuffers);
    }
    if (UNEXPECTED(hvectors != NULL)) {
        efree(hvectors);
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_write, ZEND_RETURN_VALUE, 1, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO(0, vectors, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getWriteTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, write)
{
    PHP_METHOD_CALL(swow_socket, _write, 0, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_writeTo, ZEND_RETURN_VALUE, 1, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO(0, vectors, IS_ARRAY, 0)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, address, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, port, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getWriteTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, writeTo)
{
    PHP_METHOD_CALL(swow_socket, _write, 0, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_send, ZEND_RETURN_VALUE, 1, Swow\\Socket, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 1, "\'$this->getReadableLength()\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getWriteTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, send)
{
    PHP_METHOD_CALL(swow_socket, _write, 1, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_sendTo, ZEND_RETURN_VALUE, 1, Swow\\Socket, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 1, "\'$this->getReadableLength()\'")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, address, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, port, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getWriteTimeout()\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, sendTo)
{
    PHP_METHOD_CALL(swow_socket, _write, 1, 1);
}

static PHP_METHOD_EX(swow_socket, _sendString, zend_bool may_address)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zend_string *string;
    zend_string *address = NULL;
    zend_long port = 0;
    zend_long timeout;
    zend_bool timeout_is_null = 1;
    zend_long offset = 0;
    zend_long length = 0;
    const char *ptr;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, !may_address ? 4: 6)
        Z_PARAM_STR(string)
        Z_PARAM_OPTIONAL
        if (may_address) {
            Z_PARAM_STR(address)
            Z_PARAM_LONG(port)
        }
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
        Z_PARAM_LONG(offset)
        Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END();

    /* check args and initialize */
    SWOW_BUFFER_CHECK_STRING_SCOPE(string, offset, length);
    ptr = ZSTR_VAL(string) + offset;
    if (timeout_is_null) {
        timeout =  cat_socket_get_write_timeout(socket);
    }

    if (!may_address || address == NULL || ZSTR_LEN(address) == 0) {
        ret = cat_socket_send_ex(socket, ptr, length, timeout);
    } else {
        ret = cat_socket_send_to_ex(socket, ptr, length, ZSTR_VAL(address), ZSTR_LEN(address), port, timeout);
    }

    if (UNEXPECTED(!ret)) {
        swow_throw_call_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_sendString, ZEND_RETURN_VALUE, 1, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO(0, string, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getWriteTimeout()\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, sendString)
{
    PHP_METHOD_CALL(swow_socket, _sendString, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_sendStringTo, ZEND_RETURN_VALUE, 1, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO(0, string, IS_STRING, 0)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, address, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, port, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "\'$this->getWriteTimeout()\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, sendStringTo)
{
    PHP_METHOD_CALL(swow_socket, _sendString, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_close, ZEND_RETURN_VALUE, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, close)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = cat_socket_close(socket);

    RETURN_BOOL(ret);
}

/* status */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket_getBool, ZEND_RETURN_VALUE, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_socket_isAvailable arginfo_swow_socket_getBool

static PHP_METHOD(swow_socket, isAvailable)
{
    SWOW_SOCKET_GETTER(ssocket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_socket_is_available(socket));
}

#define arginfo_swow_socket_isEstablished arginfo_swow_socket_getBool

static PHP_METHOD(swow_socket, isEstablished)
{
    SWOW_SOCKET_GETTER(ssocket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_socket_is_established(socket));
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_checkLiveness, ZEND_RETURN_VALUE, 0, Swow\\Socket, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, checkLiveness)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = cat_socket_check_liveness(socket);

    if (!ret) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }
}

#define arginfo_swow_socket_getIoState arginfo_swow_socket_getLong

static PHP_METHOD(swow_socket, getIoState)
{
    SWOW_SOCKET_GETTER(ssocket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_socket_get_io_state(socket));
}

#define arginfo_swow_socket_getIoStateName arginfo_swow_socket_getString

static PHP_METHOD(swow_socket, getIoStateName)
{
    SWOW_SOCKET_GETTER(ssocket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_socket_get_io_state_name(socket));
}

#define arginfo_swow_socket_getIoStateNaming arginfo_swow_socket_getString

static PHP_METHOD(swow_socket, getIoStateNaming)
{
    SWOW_SOCKET_GETTER(ssocket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_socket_get_io_state_naming(socket));
}

#define arginfo_swow_socket_getRecvBufferSize arginfo_swow_socket_getLong

static PHP_METHOD(swow_socket, getRecvBufferSize)
{
    SWOW_SOCKET_GETTER(ssocket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_socket_get_recv_buffer_size(socket));
}

#define arginfo_swow_socket_getSendBufferSize arginfo_swow_socket_getLong

static PHP_METHOD(swow_socket, getSendBufferSize)
{
    SWOW_SOCKET_GETTER(ssocket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_socket_get_send_buffer_size(socket));
}

/* setter */

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_setSize, ZEND_RETURN_VALUE, 1, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO(0, size, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_socket_setRecvBufferSize arginfo_swow_socket_setSize

static PHP_METHOD(swow_socket, setRecvBufferSize)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zend_long size;
    int error;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_LONG(size)
    ZEND_PARSE_PARAMETERS_END();

    error = cat_socket_set_recv_buffer_size(socket, size);

    if (UNEXPECTED(error < 0)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

#define arginfo_swow_socket_setSendBufferSize arginfo_swow_socket_setSize

static PHP_METHOD(swow_socket, setSendBufferSize)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zend_long size;
    int error;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_LONG(size)
    ZEND_PARSE_PARAMETERS_END();

    error = cat_socket_set_send_buffer_size(socket, size);

    if (UNEXPECTED(error < 0)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_setBool, ZEND_RETURN_VALUE, 0, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO(0, enable, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

#define arginfo_swow_socket_setTcpNodelay arginfo_swow_socket_setBool

static PHP_METHOD(swow_socket, setTcpNodelay)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zend_bool enable = cat_true;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    ret = cat_socket_set_tcp_nodelay(socket, enable);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_swow_socket_setTcpKeepAlive, ZEND_RETURN_VALUE, 0, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO(0, enable, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, delay, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, setTcpKeepAlive)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zend_bool enable = cat_true;
    zend_long delay = 0;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(enable)
        Z_PARAM_LONG(delay)
    ZEND_PARSE_PARAMETERS_END();

    if (delay < 0 || delay > UINT_MAX) {
        zend_argument_value_error(2, "can not be negative or be greater than %u", UINT_MAX);
        RETURN_THROWS();
    }

    ret = cat_socket_set_tcp_keepalive(socket, enable, delay);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

#define arginfo_swow_socket_setTcpAcceptBalance arginfo_swow_socket_setBool

static PHP_METHOD(swow_socket, setTcpAcceptBalance)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zend_bool enable = cat_true;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    ret = cat_socket_set_tcp_accept_balance(socket, enable);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_socket___debugInfo, ZEND_RETURN_VALUE, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(swow_socket, __debugInfo)
{
    SWOW_SOCKET_GETTER(ssocket, socket);
    zval zdebug_info;
    int n;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(&zdebug_info);
    add_assoc_string(&zdebug_info, "type", cat_socket_get_type_name(socket));
    add_assoc_long(&zdebug_info, "fd", cat_socket_get_fd_fast(socket));
    if (!cat_socket_is_available(socket)) {
        goto _return;
    }
    do {
        zval ztimeout;
        array_init(&ztimeout);
        add_assoc_long(&ztimeout, "dns", cat_socket_get_dns_timeout(socket));
        add_assoc_long(&ztimeout, "accept", cat_socket_get_accept_timeout(socket));
        add_assoc_long(&ztimeout, "connect", cat_socket_get_connect_timeout(socket));
        add_assoc_long(&ztimeout, "read", cat_socket_get_read_timeout(socket));
        add_assoc_long(&ztimeout, "write", cat_socket_get_write_timeout(socket));
        add_assoc_zval(&zdebug_info, "timeout", &ztimeout);
    } while (0);
    add_assoc_bool(&zdebug_info, "established", cat_socket_is_established(socket));
    add_assoc_string(&zdebug_info, "side", cat_socket_is_server(socket) ? "server" : (cat_socket_is_client(socket) ? "client" : "none"));
    for (n = 2; n --;)  {
        cat_bool_t is_peer = !n;
        zval zname;
        if (cat_socket_getname_fast(socket, is_peer) != NULL) {
            char address[CAT_SOCKADDR_MAX_PATH];
            size_t address_length = sizeof(address);
            cat_bool_t ret;
            array_init(&zname);
            ret = cat_socket_get_address(socket, address, &address_length, is_peer);
            if (unlikely(!ret || address_length == 0)) {
                add_assoc_str(&zname, "address", zend_empty_string);
            } else {
                add_assoc_string(&zname, "address", address);
            }
            add_assoc_long(&zname, "port", cat_socket_get_port(socket, is_peer));
        } else {
            ZVAL_NULL(&zname);
        }
        add_assoc_zval(&zdebug_info, !is_peer ? "sockname" : "peername", &zname);
    }
    add_assoc_string(&zdebug_info, "io_state", cat_socket_get_io_state_naming(socket));

    _return:
    RETURN_DEBUG_INFO_WITH_PROPERTIES(&zdebug_info);
}

static const zend_function_entry swow_socket_methods[] = {
    PHP_ME(swow_socket, __construct,             arginfo_swow_socket___construct,         ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getType,                 arginfo_swow_socket_getType,             ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getTypeName,             arginfo_swow_socket_getTypeName,         ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getFd,                   arginfo_swow_socket_getFd,               ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getDnsTimeout,           arginfo_swow_socket_getTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getAcceptTimeout,        arginfo_swow_socket_getTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getConnectTimeout,       arginfo_swow_socket_getTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getReadTimeout,          arginfo_swow_socket_getTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getWriteTimeout,         arginfo_swow_socket_getTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, setDnsTimeout,           arginfo_swow_socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, setAcceptTimeout,        arginfo_swow_socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, setConnectTimeout,       arginfo_swow_socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, setReadTimeout,          arginfo_swow_socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, setWriteTimeout,         arginfo_swow_socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, bind,                    arginfo_swow_socket_bind,                ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, listen,                  arginfo_swow_socket_listen,              ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, accept,                  arginfo_swow_socket_accept,              ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, connect,                 arginfo_swow_socket_connect,             ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getSockAddress,          arginfo_swow_socket_getAddress,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getSockPort,             arginfo_swow_socket_getPort,             ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getPeerAddress,          arginfo_swow_socket_getAddress,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getPeerPort,             arginfo_swow_socket_getPort,             ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, read,                    arginfo_swow_socket_read,                ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, recv,                    arginfo_swow_socket_recv,                ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, recvData,                arginfo_swow_socket_recvData,            ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, recvFrom,                arginfo_swow_socket_recvFrom,            ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, recvDataFrom,            arginfo_swow_socket_recvDataFrom,        ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, peek,                    arginfo_swow_socket_peek,                ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, peekFrom,                arginfo_swow_socket_peekFrom,            ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, readString,              arginfo_swow_socket_readString,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, recvString,              arginfo_swow_socket_recvString,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, recvStringData,          arginfo_swow_socket_recvStringData,      ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, recvStringFrom,          arginfo_swow_socket_recvStringFrom,      ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, recvStringDataFrom,      arginfo_swow_socket_recvStringDataFrom,  ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, peekString,              arginfo_swow_socket_peekString,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, peekStringFrom,          arginfo_swow_socket_peekStringFrom,      ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, write,                   arginfo_swow_socket_write,               ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, writeTo,                 arginfo_swow_socket_writeTo,             ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, send,                    arginfo_swow_socket_send,                ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, sendTo,                  arginfo_swow_socket_sendTo,              ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, sendString,              arginfo_swow_socket_sendString,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, sendStringTo,            arginfo_swow_socket_sendStringTo,        ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, close,                   arginfo_swow_socket_close,               ZEND_ACC_PUBLIC)
    /* status */
    PHP_ME(swow_socket, isAvailable,             arginfo_swow_socket_isAvailable,         ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, isEstablished,           arginfo_swow_socket_isEstablished,       ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, checkLiveness,           arginfo_swow_socket_checkLiveness,       ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getIoState,              arginfo_swow_socket_getIoState,          ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getIoStateName,          arginfo_swow_socket_getIoStateName,      ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getIoStateNaming,        arginfo_swow_socket_getIoStateNaming,    ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getRecvBufferSize,       arginfo_swow_socket_getRecvBufferSize,   ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, getSendBufferSize,       arginfo_swow_socket_getSendBufferSize,   ZEND_ACC_PUBLIC)
    /* setter */
    PHP_ME(swow_socket, setRecvBufferSize,       arginfo_swow_socket_setRecvBufferSize,   ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, setSendBufferSize,       arginfo_swow_socket_setSendBufferSize,   ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, setTcpNodelay,           arginfo_swow_socket_setTcpNodelay,       ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, setTcpKeepAlive,         arginfo_swow_socket_setTcpKeepAlive,     ZEND_ACC_PUBLIC)
    PHP_ME(swow_socket, setTcpAcceptBalance,     arginfo_swow_socket_setTcpAcceptBalance, ZEND_ACC_PUBLIC)
    /* magic */
    PHP_ME(swow_socket, __debugInfo,             arginfo_swow_socket___debugInfo,         ZEND_ACC_PUBLIC)
    /* globals */
    PHP_ME(swow_socket, getGlobalDnsTimeout,     arginfo_swow_socket_getGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swow_socket, getGlobalAcceptTimeout,  arginfo_swow_socket_getGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swow_socket, getGlobalConnectTimeout, arginfo_swow_socket_getGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swow_socket, getGlobalReadTimeout,    arginfo_swow_socket_getGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swow_socket, getGlobalWriteTimeout,   arginfo_swow_socket_getGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swow_socket, setGlobalDnsTimeout,     arginfo_swow_socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swow_socket, setGlobalAcceptTimeout,  arginfo_swow_socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swow_socket, setGlobalConnectTimeout, arginfo_swow_socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swow_socket, setGlobalReadTimeout,    arginfo_swow_socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swow_socket, setGlobalWriteTimeout,   arginfo_swow_socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

int swow_socket_module_init(INIT_FUNC_ARGS)
{
    if (!cat_socket_module_init()) {
        return FAILURE;
    }

    swow_socket_ce = swow_register_internal_class(
        "Swow\\Socket", NULL, swow_socket_methods,
        &swow_socket_handlers, NULL,
        cat_false, cat_false, cat_false,
        swow_socket_create_object, NULL,
        XtOffsetOf(swow_socket_t, std)
    );
    swow_socket_handlers.dtor_obj = swow_socket_dtor_object;
    /* constants */
    zend_declare_class_constant_long(swow_socket_ce, ZEND_STRL("INVALID_FD"), CAT_SOCKET_INVALID_FD);
    zend_declare_class_constant_long(swow_socket_ce, ZEND_STRL("DEFAULT_BACKLOG"), CAT_SOCKET_DEFAULT_BACKLOG);
#define SWOW_SOCKET_TYPE_FLAG_GEN(name, value) \
    zend_declare_class_constant_long(swow_socket_ce, ZEND_STRL("TYPE_FLAG_" #name), (value));
    CAT_SOCKET_TYPE_FLAG_MAP(SWOW_SOCKET_TYPE_FLAG_GEN)
#undef SWOW_SOCKET_TYPE_FLAG_GEN
#define SWOW_SOCKET_TYPE_GEN(name, value) \
    zend_declare_class_constant_long(swow_socket_ce, ZEND_STRL("TYPE_" #name), (value));
    CAT_SOCKET_TYPE_MAP(SWOW_SOCKET_TYPE_GEN)
#undef SWOW_SOCKET_TYPE_GEN
#define SWOW_SOCKET_IO_FLAG_GEN(name, value) \
    zend_declare_class_constant_long(swow_socket_ce, ZEND_STRL("IO_FLAG_" #name), (value));
    CAT_SOCKET_IO_FLAG_MAP(SWOW_SOCKET_IO_FLAG_GEN)
#undef SWOW_SOCKET_IO_FLAG_GEN
#define SWOW_SOCKET_BIND_FLAG_GEN(name, value) \
    zend_declare_class_constant_long(swow_socket_ce, ZEND_STRL("BIND_FLAG_" #name), (value));
    CAT_SOCKET_BIND_FLAG_MAP(SWOW_SOCKET_BIND_FLAG_GEN)
#undef SWOW_SOCKET_BIND_FLAG_GEN

    swow_socket_exception_ce = swow_register_internal_class(
        "Swow\\Socket\\Exception", swow_call_exception_ce, NULL, NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}

int swow_socket_runtime_init(INIT_FUNC_ARGS)
{
    if (!cat_socket_runtime_init()) {
        return FAILURE;
    }

    return SUCCESS;
}
