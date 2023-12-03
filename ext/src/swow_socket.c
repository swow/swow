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

#include "swow_stream.h" /* for Socket->open(stream) */

SWOW_API zend_class_entry *swow_socket_ce;
SWOW_API zend_object_handlers swow_socket_handlers;

SWOW_API zend_class_entry *swow_socket_exception_ce;

#define SWOW_SOCKET_GETTER_INTERNAL(_object, _s_socket, _socket) \
    swow_socket_t *_s_socket = swow_socket_get_from_object(_object); \
    cat_socket_t *_socket = &_s_socket->socket

#define SWOW_SOCKET_GETTER(_s_socket, _socket) SWOW_SOCKET_GETTER_INTERNAL(Z_OBJ_P(ZEND_THIS), _s_socket, _socket)

static zend_object *swow_socket_create_object(zend_class_entry *ce)
{
    swow_socket_t *s_socket = swow_object_alloc(swow_socket_t, ce, swow_socket_handlers);

    cat_socket_init(&s_socket->socket);

    return &s_socket->std;
}

static void swow_socket_dtor_object(zend_object *object)
{
    /* try to call __destruct first */
    zend_objects_destroy_object(object);

    /* force close the socket */
    SWOW_SOCKET_GETTER_INTERNAL(object, s_socket, socket);

    if (cat_socket_is_available(socket)) {
        cat_socket_close(socket);
    }
}

static void swow_socket_free_object(zend_object *object)
{
    /* __destruct will not be called if fatal error occurred */

    /* force close the socket */
    SWOW_SOCKET_GETTER_INTERNAL(object, s_socket, socket);

    if (cat_socket_is_available(socket)) {
        cat_socket_close(socket);
    }

    zend_object_std_dtor(&s_socket->std);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Socket___construct, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, type, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, __construct)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    zend_long type;

    if (UNEXPECTED(cat_socket_is_available(socket))) {
        zend_throw_error(NULL, "%s can be constructed only once", ZEND_THIS_NAME);
        RETURN_THROWS();
    }

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(type)
    ZEND_PARSE_PARAMETERS_END();

    if (type == CAT_SOCKET_TYPE_ANY) {
        /* we just want an initialized socket,
         * maybe for accept_typed() */
        return;
    }

    socket = cat_socket_create(socket, type);

    if (UNEXPECTED(socket == NULL)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_open, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, socket, IS_MIXED, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, open)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    zval *z_socket;
    php_stream *stream;
    cat_os_socket_t os_sock;
    zend_result result;

    if (UNEXPECTED(cat_socket_is_open(socket))) {
        zend_throw_error(NULL, "%s can be opened only once", ZEND_THIS_NAME);
        RETURN_THROWS();
    }

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(z_socket)
    ZEND_PARSE_PARAMETERS_END();

    // TODD: support more types
    if (Z_TYPE_P(z_socket) != IS_RESOURCE) {
        zend_argument_type_error(1, "must be a resource, %s given", zend_zval_type_name(z_socket));
        RETURN_THROWS();
    }

    SWOW_THROW_ON_ERROR_START_EX(swow_socket_exception_ce) {
	    php_stream_from_zval_no_verify(stream, z_socket);
    } SWOW_THROW_ON_ERROR_END();
    if (stream == NULL) {
        RETURN_THROWS();
    }

    bool is_builtin_stream = false;
    for (size_t n = 0; n < swow_stream_builtin_ops_count; n++) {
        if (stream->ops == swow_stream_builtin_ops[n]) {
            is_builtin_stream = true;
            break;
        }
    }
    if (is_builtin_stream) {
        swow_netstream_data_t *from_swow_sock = (swow_netstream_data_t *) stream->abstract;
        cat_socket_t *origin_socket = &from_swow_sock->socket;
        if (cat_socket_get_type(socket) != cat_socket_get_simple_type(origin_socket)) {
            zend_argument_type_error(1, "must be a socket of the same type, expect %s, %s given",
                cat_socket_get_type_name(socket), cat_socket_get_simple_type_name(origin_socket));
            RETURN_THROWS();
        }
        if (!cat_socket_open_socket(socket, origin_socket)) {
            swow_throw_exception_with_last(swow_socket_exception_ce);
            RETURN_THROWS();
        }
        RETURN_THIS();
    } else {
        SWOW_THROW_ON_ERROR_START_EX(swow_socket_exception_ce) {
            result = php_stream_cast(stream, PHP_STREAM_AS_SOCKETD, (void **) &os_sock, true);
        } SWOW_THROW_ON_ERROR_END();
        if (result != SUCCESS) {
            RETURN_THROWS();
        }
    #ifndef CAT_OS_WIN
        os_sock = dup(os_sock);
    #endif
        if (os_sock == CAT_OS_INVALID_SOCKET) {
            swow_throw_exception(swow_socket_exception_ce,
                cat_translate_sys_error(cat_sys_errno),
                "Failed to dup socket: %s", cat_strerror(cat_sys_errno));
            RETURN_THROWS();
        }

        if (!cat_socket_open_os_socket(socket, os_sock)) {
    #ifndef CAT_OS_WIN
            close(os_sock);
    #endif
            swow_throw_exception_with_last(swow_socket_exception_ce);
            RETURN_THROWS();
        }
        php_stream_set_option(stream, PHP_STREAM_OPTION_READ_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_getLong, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_getId, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, getId)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_socket_get_id(socket));
}

#define arginfo_class_Swow_Socket_getType arginfo_class_Swow_Socket_getId

static PHP_METHOD(Swow_Socket, getType)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_socket_get_type(socket));
}

#define arginfo_class_Swow_Socket_getSimpleType arginfo_class_Swow_Socket_getId

static PHP_METHOD(Swow_Socket, getSimpleType)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_socket_get_simple_type(socket));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_getTypeName, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, getTypeName)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_socket_get_type_name(socket));
}

#define arginfo_class_Swow_Socket_getSimpleTypeName arginfo_class_Swow_Socket_getTypeName

static PHP_METHOD(Swow_Socket, getSimpleTypeName)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_socket_get_simple_type_name(socket));
}

#define arginfo_class_Swow_Socket_getFd arginfo_class_Swow_Socket_getId

static PHP_METHOD(Swow_Socket, getFd)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG((zend_long) cat_socket_get_fd(socket));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_typeSimplify, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, type, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, typeSimplify)
{
    zend_long type;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(type)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(cat_socket_type_simplify(type));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_typeName, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, type, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, typeName)
{
    zend_long type;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(type)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING(cat_socket_type_get_name(type));
}

#define arginfo_class_Swow_Socket_getGlobalTimeout arginfo_class_Swow_Socket_getLong

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_setGlobalTimeout, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, timeout, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, setGlobalTimeout)
{
    zend_long timeout;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    cat_socket_set_global_timeout(timeout);
}

#define arginfo_class_Swow_Socket_getTimeout arginfo_class_Swow_Socket_getLong

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_setTimeout, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, timeout, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, setTimeout)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    zend_long timeout;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    ret = cat_socket_set_timeout(socket, timeout);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

#define SWOW_SOCKET_TIMEOUT_API_GEN(Type, type) \
\
static PHP_METHOD(Swow_Socket, getGlobal##Type##Timeout) \
{ \
    ZEND_PARSE_PARAMETERS_NONE(); \
    \
    RETURN_LONG(cat_socket_get_global_##type##_timeout()); \
} \
\
static PHP_METHOD(Swow_Socket, setGlobal##Type##Timeout) \
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
static PHP_METHOD(Swow_Socket, get##Type##Timeout) \
{ \
    SWOW_SOCKET_GETTER(s_socket, socket); \
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
static PHP_METHOD(Swow_Socket, set##Type##Timeout) \
{ \
    SWOW_SOCKET_GETTER(s_socket, socket); \
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

SWOW_SOCKET_TIMEOUT_API_GEN(Dns,             dns);
SWOW_SOCKET_TIMEOUT_API_GEN(Accept,       accept);
SWOW_SOCKET_TIMEOUT_API_GEN(Connect,     connect);
SWOW_SOCKET_TIMEOUT_API_GEN(Handshake, handshake);
SWOW_SOCKET_TIMEOUT_API_GEN(Read,           read);
SWOW_SOCKET_TIMEOUT_API_GEN(Write,         write);

#undef SWOW_SOCKET_TIMEOUT_API_GEN

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_bind, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, port, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "Swow\\Socket::BIND_FLAG_NONE")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, bind)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
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

    ret = cat_socket_bind_to_ex(socket, ZSTR_VAL(name), ZSTR_LEN(name), port, flags);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_listen, 0, 0, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, backlog, IS_LONG, 0, "Swow\\Socket::DEFAULT_BACKLOG")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, listen)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_accept, 0, 0, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, accept)
{
    SWOW_SOCKET_GETTER(s_server, server);
    cat_socket_type_t server_type = cat_socket_get_simple_type(server);
    zend_long timeout;
    bool timeout_is_null = 1;
    swow_socket_t *s_connection;
    cat_socket_t *connection;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
    ZEND_PARSE_PARAMETERS_END();

    s_connection = swow_socket_get_from_object(
        swow_socket_create_object(Z_OBJCE_P(ZEND_THIS))
    );
    connection = &s_connection->socket;
    if (timeout_is_null) {
        timeout = cat_socket_get_accept_timeout(server);
    }

    if (likely(server_type != CAT_SOCKET_TYPE_ANY)) {
        ret = cat_socket_create(connection, server_type) != NULL;
        if (UNEXPECTED(!ret)) {
            goto _creation_error;
        }
    } /* else server has not been constructed, but error will be triggered later in socket_accept() */

    ret = cat_socket_accept_ex(server, connection, timeout);

    if (UNEXPECTED(!ret)) {
        if (server_type != CAT_SOCKET_TYPE_ANY) {
            cat_socket_close(connection);
        }
        _creation_error:
        zend_object_release(&s_connection->std);
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_OBJ(&s_connection->std);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_acceptTo, 0, 1, IS_STATIC, 0)
    ZEND_ARG_OBJ_INFO(0, connection, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, acceptTo)
{
    SWOW_SOCKET_GETTER(s_server, server);
    zend_long timeout;
    bool timeout_is_null = 1;
    zend_object *connection_object;
    swow_socket_t *s_connection;
    cat_socket_t *connection;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_OBJ_OF_CLASS(connection_object, swow_socket_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
    ZEND_PARSE_PARAMETERS_END();

    s_connection = swow_socket_get_from_object(connection_object);
    connection = &s_connection->socket;
    if (timeout_is_null) {
        timeout = cat_socket_get_accept_timeout(server);
    }

    ret = cat_socket_accept_ex(server, connection, timeout);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_connect, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, port, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, connect)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    zend_string *name;
    zend_long port = 0;
    zend_long timeout;
    bool timeout_is_null = 1;
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

    ret = cat_socket_connect_to_ex(socket, ZSTR_VAL(name), ZSTR_LEN(name), port, timeout);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_enableCrypto, 0, 0, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, enableCrypto)
{
#ifdef CAT_SSL
    SWOW_SOCKET_GETTER(s_socket, socket);
    HashTable *options_array = NULL;
    cat_socket_crypto_options_t options;
    cat_bool_t is_client = !cat_socket_is_server_connection(socket);
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        // TODO: Support crypto options object
        Z_PARAM_ARRAY_HT(options_array)
    ZEND_PARSE_PARAMETERS_END();

    cat_socket_crypto_options_init(&options, is_client);
    if (options_array != NULL) {
        swow_hash_str_fetch_bool(options_array, "verify_peer", &options.verify_peer);
        swow_hash_str_fetch_bool(options_array, "verify_peer_name", &options.verify_peer_name);
        swow_hash_str_fetch_bool(options_array, "allow_self_signed", &options.allow_self_signed);
        swow_hash_str_fetch_long(options_array, "verify_depth", &options.verify_depth);
        swow_hash_str_fetch_str(options_array, "ca_file", &options.ca_file);
        swow_hash_str_fetch_str(options_array, "ca_path", &options.ca_path);
        if (options.ca_file == NULL) {
            options.ca_file = zend_ini_string((char *) ZEND_STRL("openssl.cafile"), 0);
            // note: we must check if zend_ini_string returns NULL because we do not register "openssl.cafile" ini option
            options.ca_file = (options.ca_file != NULL && strlen(options.ca_file) != 0) ? options.ca_file : NULL;
            options.no_client_ca_list = cat_true;
        }
        swow_hash_str_fetch_str(options_array, "passphrase", &options.passphrase);
        swow_hash_str_fetch_str(options_array, "certificate", &options.certificate);
        swow_hash_str_fetch_str(options_array, "certificate_key", &options.certificate_key);
        swow_hash_str_fetch_bool(options_array, "no_ticket", &options.no_ticket);
        swow_hash_str_fetch_bool(options_array, "no_compression", &options.no_compression);
        swow_hash_str_fetch_str(options_array, "passphrase", &options.passphrase);
        // TODO: SNI related things
        if (is_client) {
            swow_hash_str_fetch_str(options_array, "peer_name", &options.peer_name);
        }
    }

    ret = cat_socket_enable_crypto(socket, &options);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
#else
    zend_throw_error(NULL, "SSL support is not enabled, "
        "`--enable-" SWOW_MODULE_NAME_LC "-ssl` must be configured while compiling %s extension", SWOW_MODULE_NAME);
    RETURN_THROWS();
#endif
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_getAddress, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD_EX(Swow_Socket, getAddress, bool is_peer)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    char buffer[CAT_SOCKADDR_MAX_PATH];
    size_t buffer_length = sizeof(buffer);
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = cat_socket_get_address(socket, buffer, &buffer_length, is_peer);

    if (unlikely(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_STRINGL_FAST(buffer, buffer_length);
}

#define arginfo_class_Swow_Socket_getPort arginfo_class_Swow_Socket_getLong

static PHP_METHOD_EX(Swow_Socket, getPort, bool is_peer)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    zend_long port;

    ZEND_PARSE_PARAMETERS_NONE();

    if (EXPECTED(!(cat_socket_get_type(socket) & CAT_SOCKET_TYPE_FLAG_LOCAL))) {
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

static PHP_METHOD(Swow_Socket, getSockAddress)
{
    PHP_METHOD_CALL(Swow_Socket, getAddress, cat_false);
}

static PHP_METHOD(Swow_Socket, getPeerAddress)
{
    PHP_METHOD_CALL(Swow_Socket, getAddress, cat_true);
}

static PHP_METHOD(Swow_Socket, getSockPort)
{
    PHP_METHOD_CALL(Swow_Socket, getPort, cat_false);
}

static PHP_METHOD(Swow_Socket, getPeerPort)
{
    PHP_METHOD_CALL(Swow_Socket, getPort, cat_true);
}

static PHP_METHOD_EX(Swow_Socket, _read, bool once, bool may_address, bool peek)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    uint32_t max_num_args;
    zend_object *buffer_object;
    zend_long offset = 0;
    zend_long size = -1;
    zval *z_address = NULL, *z_port = NULL;
    zend_long timeout;
    bool timeout_is_null = !peek;
    swow_buffer_t *s_buffer;
    char *ptr;
    cat_bool_t want_address;
    ssize_t ret;

    // buffer, offset, size, timeout
    max_num_args = 4;
    if (may_address) {
        // address, port
        max_num_args += 2;
    }
    if (peek) {
        timeout = 0;
    }

    ZEND_PARSE_PARAMETERS_START(1, max_num_args)
        Z_PARAM_OBJ_OF_CLASS(buffer_object, swow_buffer_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(offset)
        Z_PARAM_LONG(size)
        if (may_address) {
            Z_PARAM_ZVAL(z_address)
            Z_PARAM_ZVAL(z_port)
        }
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
    ZEND_PARSE_PARAMETERS_END();

    /* check args and initialize */
    s_buffer = swow_buffer_get_from_object(buffer_object);
    ptr = swow_buffer_get_writable_space(s_buffer, offset, &size, 1);
    if (UNEXPECTED(ptr == NULL)) {
        RETURN_THROWS();
    }
    if (timeout_is_null) {
        timeout = cat_socket_get_read_timeout(socket);
    }
    if (!may_address) {
        want_address = cat_false;
    } else {
        want_address = z_address != NULL || z_port != NULL;
    }

    SWOW_BUFFER_LOCK(s_buffer);

    /* Read on socket is the same as write on Buffer,
     * so we should call COW here */
    swow_buffer_cow(s_buffer);

    /* read */
    if (!want_address) {
        if (!peek) {
            if (!once) {
                ret = cat_socket_read_ex(socket, ptr, size, timeout);
            } else {
                ret = cat_socket_recv_ex(socket, ptr, size, timeout);
            }
        } else {
            ret = cat_socket_peek_ex(socket, ptr, size, timeout);
        }
    } else {
        char address[CAT_SOCKADDR_MAX_PATH];
        size_t address_length = sizeof(address);
        int port;
        if (!peek) {
            ZEND_ASSERT(once);
            ret = cat_socket_recv_from_ex(socket, ptr, size, address, &address_length, &port, timeout);
        } else {
            ret = cat_socket_peek_from_ex(socket, ptr, size, address, &address_length, &port, timeout);
        }
        if (z_address != NULL) {
            if (address_length == 0 || address_length > sizeof(address) /* error */) {
                ZEND_TRY_ASSIGN_REF_EMPTY_STRING(z_address);
            } else {
                ZEND_TRY_ASSIGN_REF_STRINGL(z_address, address, address_length);
            }
        }
        if (z_port != NULL) {
            ZEND_TRY_ASSIGN_REF_LONG(z_port, port);
        }
    }

    SWOW_BUFFER_UNLOCK(s_buffer);

    if (EXPECTED(ret > 0)) {
        swow_buffer_virtual_write(s_buffer, offset, ret);
    }

    /* also for socket exception getReturnValue */
    RETVAL_LONG(ret);

    /* handle error */
    if (UNEXPECTED(ret < 0 || (!once && ret != size))) {
        swow_throw_call_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_read, 0, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, read)
{
    PHP_METHOD_CALL(Swow_Socket, _read, 0, 0, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_recv, 0, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "-1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, recv)
{
    PHP_METHOD_CALL(Swow_Socket, _read, 1, 0, 0);
}

#define SWOW_SOCKET_THROW_ECONNRESET_EXCEPTION_AND_RETURN_IF(condition) do { \
    if (UNEXPECTED(EG(exception) == NULL && (condition))) { \
        swow_throw_call_exception(swow_socket_exception_ce, 0, "Connection closed normally by peer while waiting for data"); \
        RETURN_THROWS(); \
    } \
} while (0)

#define arginfo_class_Swow_Socket_recvData arginfo_class_Swow_Socket_recv

static PHP_METHOD(Swow_Socket, recvData)
{
    PHP_METHOD_CALL(Swow_Socket, recv);
    SWOW_SOCKET_THROW_ECONNRESET_EXCEPTION_AND_RETURN_IF(Z_LVAL_P(return_value) == 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_recvFrom, 0, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "-1")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, address, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, port, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, recvFrom)
{
    PHP_METHOD_CALL(Swow_Socket, _read, 1, 1, 0);
}

#define arginfo_class_Swow_Socket_recvDataFrom arginfo_class_Swow_Socket_recvFrom

static PHP_METHOD(Swow_Socket, recvDataFrom)
{
    PHP_METHOD_CALL(Swow_Socket, recvFrom);
    SWOW_SOCKET_THROW_ECONNRESET_EXCEPTION_AND_RETURN_IF(Z_LVAL_P(return_value) == 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_peek, 0, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "-1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, peek)
{
    PHP_METHOD_CALL(Swow_Socket, _read, 1, 0, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_peekFrom, 0, 1, IS_LONG, 0)
    ZEND_ARG_OBJ_INFO(0, buffer, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "-1")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, address, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, port, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, peekFrom)
{
    PHP_METHOD_CALL(Swow_Socket, _read, 1, 1, 1);
}

static PHP_METHOD_EX(Swow_Socket, _readString, bool once, bool may_address, bool peek)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    uint32_t max_num_args;
    zend_string *string;
    zend_long size = CAT_BUFFER_COMMON_SIZE;
    zval *z_address = NULL, *z_port = NULL;
    zend_long timeout;
    bool timeout_is_null = !peek;
    char *ptr;
    cat_bool_t want_address;
    ssize_t ret;

    max_num_args = 2;
    if (may_address) {
        max_num_args += 2;
    }
    if (peek) {
        timeout = 0;
    }

    ZEND_PARSE_PARAMETERS_START(0, max_num_args)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(size)
        if (may_address) {
            Z_PARAM_ZVAL(z_address)
            Z_PARAM_ZVAL(z_port)
        }
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
    ZEND_PARSE_PARAMETERS_END();

    /* check args and initialize */
    if (UNEXPECTED(size <= 0)) {
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
        want_address = z_address != NULL || z_port != NULL;
    }

    ptr = ZSTR_VAL(string);
    while (1) {
        if (!want_address) {
            if (!peek) {
                if (!once) {
                    ret = cat_socket_read_ex(socket, ptr, size, timeout);
                } else {
                    ret = cat_socket_recv_ex(socket, ptr, size, timeout);
                }
            } else {
                ret = cat_socket_peek_ex(socket, ptr, size, timeout);
            }
        } else {
            char address[CAT_SOCKADDR_MAX_PATH];
            size_t address_length = sizeof(address);
            int port;
            if (!peek) {
                ZEND_ASSERT(once);
                ret = cat_socket_recv_from_ex(socket, ptr, size, address, &address_length, &port, timeout);
            } else {
                ret = cat_socket_peek_from_ex(socket, ptr, size, address, &address_length, &port, timeout);
            }
            if (z_address != NULL) {
                if (address_length == 0 || address_length > sizeof(address)) {
                    ZEND_TRY_ASSIGN_REF_EMPTY_STRING(z_address);
                } else {
                    ZEND_TRY_ASSIGN_REF_STRINGL(z_address, address, address_length);
                }
            }
            if (z_port != NULL) {
                ZEND_TRY_ASSIGN_REF_LONG(z_port, port);
            }
        }
        break;
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_readString, 0, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "Swow\\Buffer::COMMON_SIZE")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, readString)
{
    PHP_METHOD_CALL(Swow_Socket, _readString, 0, 0, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_recvString, 0, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "Swow\\Buffer::COMMON_SIZE")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, recvString)
{
    PHP_METHOD_CALL(Swow_Socket, _readString, 1, 0, 0);
}

#define arginfo_class_Swow_Socket_recvStringData arginfo_class_Swow_Socket_recvString

static PHP_METHOD(Swow_Socket, recvStringData)
{
    PHP_METHOD_CALL(Swow_Socket, recvString);
    SWOW_SOCKET_THROW_ECONNRESET_EXCEPTION_AND_RETURN_IF(Z_STRLEN_P(return_value) == 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_recvStringFrom, 0, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "Swow\\Buffer::COMMON_SIZE")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, address, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, port, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, recvStringFrom)
{
    PHP_METHOD_CALL(Swow_Socket, _readString, 1, 1, 0);
}

#define arginfo_class_Swow_Socket_recvStringDataFrom arginfo_class_Swow_Socket_recvStringFrom

static PHP_METHOD(Swow_Socket, recvStringDataFrom)
{
    PHP_METHOD_CALL(Swow_Socket, recvStringFrom);
    SWOW_SOCKET_THROW_ECONNRESET_EXCEPTION_AND_RETURN_IF(Z_STRLEN_P(return_value) == 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_peekString, 0, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "Swow\\Buffer::COMMON_SIZE")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, peekString)
{
    PHP_METHOD_CALL(Swow_Socket, _readString, 1, 0, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_peekStringFrom, 0, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_LONG, 0, "Swow\\Buffer::COMMON_SIZE")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, address, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, port, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, peekStringFrom)
{
    PHP_METHOD_CALL(Swow_Socket, _readString, 1, 1, 1);
}

static PHP_METHOD_EX(Swow_Socket, _write, bool single, bool may_address)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    uint32_t max_num_args;
    swow_buffer_t *s_buffer = NULL;
    zend_string *string = NULL;
    zend_long start = 0;
    zend_long length = -1;
    zend_string *address = NULL;
    zend_long port = 0;
    zend_long timeout;
    bool timeout_is_null = 1;
    const char *ptr;
    cat_socket_write_vector_t *vector, *vector_list_on_heap = NULL, vector_list_on_stack[8];
    /* real non-empty vector count */
    uint32_t vector_count = 0;
    /* Use addref/release for buffer strings to make sure data is immutable (COW),
     * save strings in a C array and release them before return. */
    zend_string **strings, **strings_on_heap = NULL, *strings_on_stack[8];
    uint32_t buffer_count = 0;
    cat_bool_t ret;

    // vector, timeout / buffer or string, timeout
    max_num_args = 2;
    if (single) {
        // start, length
        max_num_args += 2;
    }
    if (may_address) {
        // address, port
        max_num_args += 2;
    }

    vector = vector_list_on_stack;
    strings = strings_on_stack;
    ZEND_PARSE_PARAMETERS_START(1, max_num_args)
        if (!single) {
            HashTable *vector_list_array;
            uint32_t vector_list_array_count;
            Z_PARAM_ARRAY_HT(vector_list_array)
            vector_list_array_count = zend_hash_num_elements(vector_list_array);
            if (UNEXPECTED(vector_list_array_count == 0)) {
                zend_argument_value_error(1, "can not be empty");
                goto _error;
            }
            if (UNEXPECTED(vector_list_array_count > CAT_ARRAY_SIZE(vector_list_on_stack))) {
                vector = vector_list_on_heap = emalloc(vector_list_array_count * sizeof(*vector));
            }
            if (UNEXPECTED(vector_list_array_count > CAT_ARRAY_SIZE(strings_on_stack))) {
                strings = strings_on_heap = emalloc(vector_list_array_count * sizeof(*strings));
            }
            do {
                zval *z_tmp;
                uint32_t vector_list_array_index = 0;
                ZEND_HASH_FOREACH_VAL(vector_list_array, z_tmp) {
                    /* the last one can be null */
                    if (ZVAL_IS_NULL(z_tmp)) {
                        break;
                    }
                    /* [array|string|Stringable|Buffer] */
                    if (Z_TYPE_P(z_tmp) == IS_ARRAY) {
                        /* array include parameters */
                        HashTable *vector_elements_array = Z_ARR_P(z_tmp);
                        uint32_t vector_elements_array_count = zend_hash_num_elements(vector_elements_array);
                        if (UNEXPECTED(vector_elements_array_count < 1 || vector_elements_array_count > 3)) {
                            zend_argument_value_error(1, "[%u] must have 1 to 3 elements, %u given", vector_list_array_index, vector_elements_array_count);
                            goto _error;
                        }
                        uint32_t index = 0;
                        /* [string|Stringable|Buffer, start, length] */
                        ZEND_HASH_FOREACH_VAL(vector_elements_array, z_tmp) {
                            ZEND_ASSERT(index == 0 || index == 1 || index == 2);
                            if (index == 0) {
                                if (UNEXPECTED(!swow_parse_arg_buffer_or_stringable_for_reading(z_tmp, &s_buffer, &string, 1))) {
                                    zend_argument_type_error(1, "[%u][0] ($data) must be of type string or %s, %s given", vector_list_array_index, ZSTR_VAL(swow_buffer_ce->name), zend_zval_type_name(z_tmp));
                                    goto _error;
                                }
                            } else if (index == 1) {
                                if (UNEXPECTED(z_tmp != NULL && !swow_parse_arg_long(z_tmp, &start, NULL, false, 1))) {
                                    zend_argument_type_error(1, "[%u][1] ($start) must be of type int, %s given", vector_list_array_index, zend_zval_type_name(z_tmp));
                                    goto _error;
                                }
                            } else if (index == 2) {
                                if (UNEXPECTED(z_tmp != NULL && !swow_parse_arg_long(z_tmp, &length, NULL, false, 1))) {
                                    zend_argument_type_error(1, "[%u][2] ($length) must be of type int, %s given", vector_list_array_index, zend_zval_type_name(z_tmp));
                                    goto _error;
                                }
                            }
                            index++;
                        } ZEND_HASH_FOREACH_END();
                    } else if (!swow_parse_arg_buffer_or_stringable_for_reading(z_tmp, &s_buffer, &string, 1)) {
                        zend_argument_type_error(1, "[%u] ($stringable) must be of type string, array or %s, %s given", vector_list_array_index, ZSTR_VAL(swow_buffer_ce->name), zend_zval_type_name(z_tmp));
                        goto _error;
                    }
                    ptr = swow_buffer_or_string_get_readable_space_v(s_buffer, string, start, &length, 1, vector_list_array_index, 1);
                    if (UNEXPECTED(ptr == NULL)) {
                        goto _error;
                    }
                    if (length == 0) {
                        goto _next;
                    }
                    if (s_buffer != NULL) {
                        zend_string *buffer_string = swow_buffer_get_string(s_buffer);
                        if (buffer_string != NULL) {
                            strings[buffer_count++] = zend_string_copy(buffer_string);
                        }
                    }
                    vector[vector_count].base = ptr;
                    vector[vector_count].length = length;
                    vector_count++;
                    _next:
                    /* reset arguments */
                    s_buffer = NULL;
                    string = NULL;
                    start = 0;
                    length = -1;
                    vector_list_array_index++;
                } ZEND_HASH_FOREACH_END();
                if (UNEXPECTED(vector_count == 0)) {
                    goto _return;
                }
            } while (0);
            Z_PARAM_OPTIONAL
        } else {
            vector_count++;
            SWOW_PARAM_BUFFER_OR_STRINGABLE_FOR_READING(s_buffer, string)
            Z_PARAM_OPTIONAL
            Z_PARAM_LONG(start)
            Z_PARAM_LONG(length)
        }
        if (may_address) {
            Z_PARAM_STR(address)
            Z_PARAM_LONG(port)
        }
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
    ZEND_PARSE_PARAMETERS_END_EX(goto _error);

    /* check args and initialize */
    if (single) {
        ptr = swow_buffer_or_string_get_readable_space(s_buffer, string, start, &length, 1);
        if (UNEXPECTED(ptr == NULL)) {
            goto _error;
        }
        if (s_buffer != NULL) {
            zend_string *buffer_string = swow_buffer_get_string(s_buffer);
            if (buffer_string != NULL) {
                strings[buffer_count++] = zend_string_copy(buffer_string);
            }
        }
        vector[0].base = ptr;
        vector[0].length = length;
    }
    if (timeout_is_null) {
        timeout = cat_socket_get_read_timeout(socket);
    }

    /* write */
    if (!may_address || address == NULL || ZSTR_LEN(address) == 0) {
        ret = cat_socket_write_ex(socket, vector, vector_count, timeout);
    } else {
        ret = cat_socket_write_to_ex(socket, vector, vector_count, ZSTR_VAL(address), ZSTR_LEN(address), port, timeout);
    }

    if (UNEXPECTED(!ret)) {
        swow_throw_call_exception_with_last(swow_socket_exception_ce);
        goto _error;
    }

    _return:
    RETVAL_THIS();

    if (0) {
        _error:
        ZEND_ASSERT_HAS_EXCEPTION();
    }
    while (buffer_count--) {
        zend_string_release(strings[buffer_count]);
    }
    if (UNEXPECTED(strings_on_heap != NULL)) {
        efree(strings_on_heap);
    }
    if (UNEXPECTED(vector_list_on_heap != NULL)) {
        efree(vector_list_on_heap);
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_write, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, vector, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, write)
{
    PHP_METHOD_CALL(Swow_Socket, _write, 0, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_writeTo, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, vector, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address, IS_STRING, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, port, IS_LONG, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, writeTo)
{
    PHP_METHOD_CALL(Swow_Socket, _write, 0, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_send, 0, 1, IS_STATIC, 0)
    ZEND_ARG_OBJ_TYPE_MASK(0, data, Stringable, MAY_BE_STRING, NULL)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, start, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, send)
{
    PHP_METHOD_CALL(Swow_Socket, _write, 1, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_sendTo, 0, 1, IS_STATIC, 0)
    ZEND_ARG_OBJ_TYPE_MASK(0, data, Stringable, MAY_BE_STRING, NULL)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, start, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address, IS_STRING, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, port, IS_LONG, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, sendTo)
{
    PHP_METHOD_CALL(Swow_Socket, _write, 1, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_sendHandle, 0, 1, IS_STATIC, 0)
    ZEND_ARG_OBJ_INFO(0, handle, Swow\\Socket, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, sendHandle)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    zend_object *handle_object;
    zend_long timeout;
    bool timeout_is_null = 1;
    swow_socket_t *s_handle;
    cat_socket_t *handle;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_OBJ_OF_CLASS(handle_object, swow_socket_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
    ZEND_PARSE_PARAMETERS_END();

    s_handle = swow_socket_get_from_object(handle_object);
    handle = &s_handle->socket;
    if (timeout_is_null) {
        timeout = cat_socket_get_write_timeout(socket);
    }

    ret = cat_socket_send_handle_ex(socket, handle, timeout);

    if (UNEXPECTED(!ret)) {
        swow_throw_call_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_sendFile, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, offset, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, sendFile)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    zend_string *filename;
    zend_long offset = 0;
    zend_long length = 0;
    zend_long timeout;
    bool timeout_is_null = 1;
    ssize_t written;

    ZEND_PARSE_PARAMETERS_START(1, 4)
        Z_PARAM_STR(filename)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(offset)
        Z_PARAM_LONG(length)
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
    ZEND_PARSE_PARAMETERS_END();

    if (length == -1) {
        // 0 means unlimited for sendfile()
        length = 0;
    } else if (UNEXPECTED(length < 0)) {
        zend_argument_error(swow_socket_exception_ce, 3, "can only be -1 to refer to unlimited when it is negative");
        RETURN_THROWS();
    }
    if (timeout_is_null) {
        timeout = cat_socket_get_write_timeout(socket);
    }

    written = cat_socket_send_file_ex(socket, ZSTR_VAL(filename), offset, length, timeout);

    if (UNEXPECTED(written < 0)) {
        swow_throw_call_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    // TODO: RETURN_LONG_SAFE() ? may SIZE_MAX be bigger than ZEND_LONG_MAX?
    RETURN_LONG(written);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_close, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, close)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = cat_socket_close(socket);

    RETURN_BOOL(ret);
}

/* status */

#define SWOW_SOCKET_IS_XXX_API_GEN(Name, name) \
static PHP_METHOD(Swow_Socket, is##Name) \
{ \
    SWOW_SOCKET_GETTER(s_socket, socket); \
    \
    ZEND_PARSE_PARAMETERS_NONE(); \
    \
    RETURN_BOOL(cat_socket_is_##name(socket)); \
}

#define arginfo_class_Swow_Socket_isAvailable arginfo_class_Swow_Socket_close

SWOW_SOCKET_IS_XXX_API_GEN(Available, available)

#define arginfo_class_Swow_Socket_isOpen arginfo_class_Swow_Socket_close

SWOW_SOCKET_IS_XXX_API_GEN(Open, open)

#define arginfo_class_Swow_Socket_isEstablished arginfo_class_Swow_Socket_close

SWOW_SOCKET_IS_XXX_API_GEN(Established, established)

#define arginfo_class_Swow_Socket_isServer arginfo_class_Swow_Socket_close

SWOW_SOCKET_IS_XXX_API_GEN(Server, server)

#define arginfo_class_Swow_Socket_isServerConnection arginfo_class_Swow_Socket_close

SWOW_SOCKET_IS_XXX_API_GEN(ServerConnection, server_connection)

#define arginfo_class_Swow_Socket_isClient arginfo_class_Swow_Socket_close

SWOW_SOCKET_IS_XXX_API_GEN(Client, client)

#define arginfo_class_Swow_Socket_getConnectionError arginfo_class_Swow_Socket_getId

static PHP_METHOD(Swow_Socket, getConnectionError)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_socket_get_connection_error(socket));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_checkLiveness, 0, 0, IS_STATIC, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, checkLiveness)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = cat_socket_check_liveness(socket);

    if (!ret) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

#define arginfo_class_Swow_Socket_getIoState arginfo_class_Swow_Socket_getId

static PHP_METHOD(Swow_Socket, getIoState)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_socket_get_io_state(socket));
}

#define arginfo_class_Swow_Socket_getIoStateName arginfo_class_Swow_Socket_getTypeName

static PHP_METHOD(Swow_Socket, getIoStateName)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_socket_get_io_state_name(socket));
}

#define arginfo_class_Swow_Socket_getIoStateNaming arginfo_class_Swow_Socket_getTypeName

static PHP_METHOD(Swow_Socket, getIoStateNaming)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_socket_get_io_state_naming(socket));
}

#define arginfo_class_Swow_Socket_getRecvBufferSize arginfo_class_Swow_Socket_getId

static PHP_METHOD(Swow_Socket, getRecvBufferSize)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_socket_get_recv_buffer_size(socket));
}

#define arginfo_class_Swow_Socket_getSendBufferSize arginfo_class_Swow_Socket_getId

static PHP_METHOD(Swow_Socket, getSendBufferSize)
{
    SWOW_SOCKET_GETTER(s_socket, socket);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_socket_get_send_buffer_size(socket));
}

/* setter */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_setRecvBufferSize, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, size, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, setRecvBufferSize)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    zend_long size;
    int error;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(size)
    ZEND_PARSE_PARAMETERS_END();

    error = cat_socket_set_recv_buffer_size(socket, size);

    if (UNEXPECTED(error < 0)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

#define arginfo_class_Swow_Socket_setSendBufferSize arginfo_class_Swow_Socket_setRecvBufferSize

static PHP_METHOD(Swow_Socket, setSendBufferSize)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    zend_long size;
    int error;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(size)
    ZEND_PARSE_PARAMETERS_END();

    error = cat_socket_set_send_buffer_size(socket, size);

    if (UNEXPECTED(error < 0)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_setTcpNodelay, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, enable, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, setTcpNodelay)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    bool enable = cat_true;
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket_setTcpKeepAlive, 0, 2, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, enable, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, delay, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, setTcpKeepAlive)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    bool enable = cat_true;
    zend_long delay = 0;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(enable)
        Z_PARAM_LONG(delay)
    ZEND_PARSE_PARAMETERS_END();

    if (delay < 0 || delay > UINT_MAX) {
        zend_argument_value_error(2, "must be greater than 0 and less than or equal to %u", UINT_MAX);
        RETURN_THROWS();
    }

    ret = cat_socket_set_tcp_keepalive(socket, enable, delay);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_socket_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Socket___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Socket, __debugInfo)
{
    SWOW_SOCKET_GETTER(s_socket, socket);
    zval z_debug_info;
    int n;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(&z_debug_info);
    add_assoc_string(&z_debug_info, "type", cat_socket_get_type_name(socket));
    add_assoc_long(&z_debug_info, "fd", (zend_long) cat_socket_get_fd_fast(socket));
    if (!cat_socket_is_available(socket)) {
        goto _return;
    }
    do {
        zval z_timeout;
        array_init(&z_timeout);
        add_assoc_long(&z_timeout, "dns", cat_socket_get_dns_timeout(socket));
        add_assoc_long(&z_timeout, "accept", cat_socket_get_accept_timeout(socket));
        add_assoc_long(&z_timeout, "connect", cat_socket_get_connect_timeout(socket));
        add_assoc_long(&z_timeout, "handshake", cat_socket_get_handshake_timeout(socket));
        add_assoc_long(&z_timeout, "read", cat_socket_get_read_timeout(socket));
        add_assoc_long(&z_timeout, "write", cat_socket_get_write_timeout(socket));
        add_assoc_zval(&z_debug_info, "timeout", &z_timeout);
    } while (0);
    add_assoc_bool(&z_debug_info, "established", cat_socket_is_established(socket));
    add_assoc_string(&z_debug_info, "role", cat_socket_get_role_name(socket));
    for (n = 2; n--;) {
        cat_bool_t is_peer = !n;
        zval z_name;
        if (cat_socket_getname_fast(socket, is_peer) != NULL) {
            char address[CAT_SOCKADDR_MAX_PATH];
            size_t address_length = sizeof(address);
            cat_bool_t ret;
            array_init(&z_name);
            ret = cat_socket_get_address(socket, address, &address_length, is_peer);
            if (unlikely(!ret || address_length == 0)) {
                add_assoc_str(&z_name, "address", zend_empty_string);
            } else {
                add_assoc_string(&z_name, "address", address);
            }
            add_assoc_long(&z_name, "port", cat_socket_get_port(socket, is_peer));
        } else {
            ZVAL_NULL(&z_name);
        }
        add_assoc_zval(&z_debug_info, !is_peer ? "sockname" : "peername", &z_name);
    }
    add_assoc_string(&z_debug_info, "io_state", cat_socket_get_io_state_naming(socket));

    _return:
    RETURN_DEBUG_INFO_WITH_PROPERTIES(&z_debug_info);
}

static const zend_function_entry swow_socket_methods[] = {
    PHP_ME(Swow_Socket, __construct,               arginfo_class_Swow_Socket___construct,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, open,                      arginfo_class_Swow_Socket_open,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getId,                     arginfo_class_Swow_Socket_getId,               ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getType,                   arginfo_class_Swow_Socket_getType,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getSimpleType,             arginfo_class_Swow_Socket_getSimpleType,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getTypeName,               arginfo_class_Swow_Socket_getTypeName,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getSimpleTypeName,         arginfo_class_Swow_Socket_getSimpleTypeName,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getFd,                     arginfo_class_Swow_Socket_getFd,               ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getDnsTimeout,             arginfo_class_Swow_Socket_getTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getAcceptTimeout,          arginfo_class_Swow_Socket_getTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getConnectTimeout,         arginfo_class_Swow_Socket_getTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getHandshakeTimeout,       arginfo_class_Swow_Socket_getTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getReadTimeout,            arginfo_class_Swow_Socket_getTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getWriteTimeout,           arginfo_class_Swow_Socket_getTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, setTimeout,                arginfo_class_Swow_Socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, setDnsTimeout,             arginfo_class_Swow_Socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, setAcceptTimeout,          arginfo_class_Swow_Socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, setConnectTimeout,         arginfo_class_Swow_Socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, setHandshakeTimeout,       arginfo_class_Swow_Socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, setReadTimeout,            arginfo_class_Swow_Socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, setWriteTimeout,           arginfo_class_Swow_Socket_setTimeout,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, bind,                      arginfo_class_Swow_Socket_bind,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, listen,                    arginfo_class_Swow_Socket_listen,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, accept,                    arginfo_class_Swow_Socket_accept,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, acceptTo,                  arginfo_class_Swow_Socket_acceptTo,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, connect,                   arginfo_class_Swow_Socket_connect,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, enableCrypto,              arginfo_class_Swow_Socket_enableCrypto,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getSockAddress,            arginfo_class_Swow_Socket_getAddress,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getSockPort,               arginfo_class_Swow_Socket_getPort,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getPeerAddress,            arginfo_class_Swow_Socket_getAddress,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getPeerPort,               arginfo_class_Swow_Socket_getPort,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, read,                      arginfo_class_Swow_Socket_read,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, recv,                      arginfo_class_Swow_Socket_recv,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, recvData,                  arginfo_class_Swow_Socket_recvData,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, recvFrom,                  arginfo_class_Swow_Socket_recvFrom,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, recvDataFrom,              arginfo_class_Swow_Socket_recvDataFrom,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, peek,                      arginfo_class_Swow_Socket_peek,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, peekFrom,                  arginfo_class_Swow_Socket_peekFrom,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, readString,                arginfo_class_Swow_Socket_readString,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, recvString,                arginfo_class_Swow_Socket_recvString,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, recvStringData,            arginfo_class_Swow_Socket_recvStringData,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, recvStringFrom,            arginfo_class_Swow_Socket_recvStringFrom,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, recvStringDataFrom,        arginfo_class_Swow_Socket_recvStringDataFrom,  ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, peekString,                arginfo_class_Swow_Socket_peekString,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, peekStringFrom,            arginfo_class_Swow_Socket_peekStringFrom,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, write,                     arginfo_class_Swow_Socket_write,               ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, writeTo,                   arginfo_class_Swow_Socket_writeTo,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, send,                      arginfo_class_Swow_Socket_send,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, sendTo,                    arginfo_class_Swow_Socket_sendTo,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, sendHandle,                arginfo_class_Swow_Socket_sendHandle,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, sendFile,                  arginfo_class_Swow_Socket_sendFile,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, close,                     arginfo_class_Swow_Socket_close,               ZEND_ACC_PUBLIC)
    /* status */
    PHP_ME(Swow_Socket, isAvailable,               arginfo_class_Swow_Socket_isAvailable,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, isOpen,                    arginfo_class_Swow_Socket_isOpen,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, isEstablished,             arginfo_class_Swow_Socket_isEstablished,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, isServer,                  arginfo_class_Swow_Socket_isServer,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, isServerConnection,        arginfo_class_Swow_Socket_isServerConnection,  ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, isClient,                  arginfo_class_Swow_Socket_isClient,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getConnectionError,        arginfo_class_Swow_Socket_getConnectionError,  ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, checkLiveness,             arginfo_class_Swow_Socket_checkLiveness,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getIoState,                arginfo_class_Swow_Socket_getIoState,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getIoStateName,            arginfo_class_Swow_Socket_getIoStateName,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getIoStateNaming,          arginfo_class_Swow_Socket_getIoStateNaming,    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getRecvBufferSize,         arginfo_class_Swow_Socket_getRecvBufferSize,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, getSendBufferSize,         arginfo_class_Swow_Socket_getSendBufferSize,   ZEND_ACC_PUBLIC)
    /* setter */
    PHP_ME(Swow_Socket, setRecvBufferSize,         arginfo_class_Swow_Socket_setRecvBufferSize,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, setSendBufferSize,         arginfo_class_Swow_Socket_setSendBufferSize,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, setTcpNodelay,             arginfo_class_Swow_Socket_setTcpNodelay,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Socket, setTcpKeepAlive,           arginfo_class_Swow_Socket_setTcpKeepAlive,     ZEND_ACC_PUBLIC)
    /* magic */
    PHP_ME(Swow_Socket, __debugInfo,               arginfo_class_Swow_Socket___debugInfo,         ZEND_ACC_PUBLIC)
    /* globals */
    PHP_ME(Swow_Socket, typeSimplify,              arginfo_class_Swow_Socket_typeSimplify,        ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, typeName,                  arginfo_class_Swow_Socket_typeName,            ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, setGlobalTimeout,          arginfo_class_Swow_Socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, getGlobalDnsTimeout,       arginfo_class_Swow_Socket_getGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, getGlobalAcceptTimeout,    arginfo_class_Swow_Socket_getGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, getGlobalConnectTimeout,   arginfo_class_Swow_Socket_getGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, getGlobalHandshakeTimeout, arginfo_class_Swow_Socket_getGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, getGlobalReadTimeout,      arginfo_class_Swow_Socket_getGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, getGlobalWriteTimeout,     arginfo_class_Swow_Socket_getGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, setGlobalDnsTimeout,       arginfo_class_Swow_Socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, setGlobalAcceptTimeout,    arginfo_class_Swow_Socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, setGlobalConnectTimeout,   arginfo_class_Swow_Socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, setGlobalHandshakeTimeout, arginfo_class_Swow_Socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, setGlobalReadTimeout,      arginfo_class_Swow_Socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Socket, setGlobalWriteTimeout,     arginfo_class_Swow_Socket_setGlobalTimeout,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

zend_result swow_socket_module_init(INIT_FUNC_ARGS)
{
    if (!cat_socket_module_init()) {
        return FAILURE;
    }
#ifdef CAT_SSL
    if (!cat_ssl_module_init()) {
        return FAILURE;
    }
#endif

    swow_socket_ce = swow_register_internal_class(
        "Swow\\Socket", NULL, swow_socket_methods,
        &swow_socket_handlers, NULL,
        cat_false, cat_false,
        swow_socket_create_object, swow_socket_free_object,
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
        "Swow\\SocketException", swow_call_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}

zend_result swow_socket_module_shutdown(INIT_FUNC_ARGS)
{
    if (!cat_socket_module_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result swow_socket_runtime_init(INIT_FUNC_ARGS)
{
    if (!cat_socket_runtime_init()) {
        return FAILURE;
    }

    return SUCCESS;
}
