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

#include "cat_ssl.h"
#ifdef CAT_SSL

/*
This diagram shows how the read and write memory BIO's (rbio & wbio) are
associated with the socket read and write respectively.  On the inbound flow
(data into the program) bytes are read from the socket and copied into the rbio
via BIO_write.  This represents the the transfer of encrypted data into the SSL
object. The unencrypted data is then obtained through calling SSL_read.  The
reverse happens on the outbound flow to convey unencrypted user data into a
socket write of encrypted data.
  +------+                                    +-----+
  |......|--> read(fd) --> BIO_write(nbio) -->|.....|--> SSL_read(ssl)  --> IN
  |......|                                    |.....|
  |.sock.|                                    |.SSL.|
  |......|                                    |.....|
  |......|<-- write(fd) <-- BIO_read(nbio) <--|.....|<-- SSL_write(ssl) <-- OUT
  +------+                                    +-----+
          |                                  |       |                     |
          |<-------------------------------->|       |<------------------->|
          |         encrypted bytes          |       |  unencrypted bytes  |
*/

static void cat_ssl_clear_error(void);
#ifdef CAT_DEBUG
static void cat_ssl_handshake_log(cat_ssl_t *ssl);
#else
#define cat_ssl_handshake_log(ssl)
#endif

int cat_ssl_index;

#define CAT_SSL_GET_CONNECTION(_ssl, _connection, failure) \
    cat_ssl_connection_t *connection = _ssl->connection; do { \
    if (unlikely(connection == NULL)) { \
        cat_update_last_error(CAT_ESSL, "SSL connection has not been opened"); \
        failure; \
    } \
} while (0)

CAT_API cat_bool_t cat_ssl_module_init(void)
{
    /* SSL library initialisation */
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();
#if OPENSSL_VERSION_NUMBER >= 0x10100003L
    if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, NULL) == 0) {
        ERR_print_errors_fp(stderr);
        cat_core_error(SSL, "OPENSSL_init_ssl() failed");
        return cat_false;
    }
#else
    OPENSSL_config(NULL);
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif
    /* initialisation may leave errors in the error queue while returning success */
    ERR_clear_error();

#ifndef SSL_OP_NO_COMPRESSION
    do {
        /* Disable gzip compression in OpenSSL prior to 1.0.0 version, this saves about 522K per connection */
        int                  n;
        STACK_OF(SSL_COMP)  *ssl_comp_methods;
        ssl_comp_methods = SSL_COMP_get_compression_methods();
        n = sk_SSL_COMP_num(ssl_comp_methods);
        while (n--) {
            (void) sk_SSL_COMP_pop(ssl_comp_methods);
        }
    } while (0);
#endif

    cat_ssl_index = SSL_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    if (cat_ssl_index == -1) {
        ERR_print_errors_fp(stderr);
        cat_core_error(SSL, "SSL_get_ex_new_index() failed");
    }

    return cat_true;
}

CAT_API cat_ssl_t *cat_ssl_alloc(cat_ssl_t *ssl)
{
    if (ssl == NULL) {
        ssl = (cat_ssl_t *) cat_malloc(sizeof(*ssl));
        if (unlikely(ssl == NULL)) {
            cat_update_last_error_of_syscall("Malloc for SSL failed");
            return NULL;
        }
        ssl->flags |= CAT_SSL_FLAG_ALLOC;
    }
    ssl->context = SSL_CTX_new(SSLv23_method());
    if (unlikely(ssl->context == NULL)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_CTX_new() failed");
        goto _failed;
    }
    /* client side options */
#ifdef SSL_OP_MICROSOFT_SESS_ID_BUG
    SSL_CTX_set_options(ssl->context, SSL_OP_MICROSOFT_SESS_ID_BUG);
#endif
#ifdef SSL_OP_NETSCAPE_CHALLENGE_BUG
    SSL_CTX_set_options(ssl->context, SSL_OP_NETSCAPE_CHALLENGE_BUG);
#endif
    /* server side options */
#ifdef SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG
    SSL_CTX_set_options(ssl->context, SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG);
#endif
#ifdef SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER
    SSL_CTX_set_options(ssl->context, SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER);
#endif
#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
    /* this option allow a potential SSL 2.0 rollback (CAN-2005-2969) */
    SSL_CTX_set_options(ssl->context, SSL_OP_MSIE_SSLV2_RSA_PADDING);
#endif
#ifdef SSL_OP_SSLEAY_080_CLIENT_DH_BUG
    SSL_CTX_set_options(ssl->context, SSL_OP_SSLEAY_080_CLIENT_DH_BUG);
#endif
#ifdef SSL_OP_TLS_D5_BUG
    SSL_CTX_set_options(ssl->context, SSL_OP_TLS_D5_BUG);
#endif
#ifdef SSL_OP_TLS_BLOCK_PADDING_BUG
    SSL_CTX_set_options(ssl->context, SSL_OP_TLS_BLOCK_PADDING_BUG);
#endif
#ifdef SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
    SSL_CTX_set_options(ssl->context, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
#endif
    SSL_CTX_set_options(ssl->context, SSL_OP_SINGLE_DH_USE);
#if OPENSSL_VERSION_NUMBER >= 0x009080dfL
    /* only in 0.9.8m+ */
    SSL_CTX_clear_options(ssl->context, SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3|SSL_OP_NO_TLSv1);
#endif
#ifdef SSL_CTX_set_min_proto_version
    SSL_CTX_set_min_proto_version(ssl->context, 0);
    SSL_CTX_set_max_proto_version(ssl->context, TLS1_2_VERSION);
#endif
#ifdef TLS1_3_VERSION
    SSL_CTX_set_min_proto_version(ssl->context, 0);
    SSL_CTX_set_max_proto_version(ssl->context, TLS1_3_VERSION);
#endif
#ifdef SSL_OP_NO_COMPRESSION
    SSL_CTX_set_options(ssl->context, SSL_OP_NO_COMPRESSION);
#endif
#ifdef SSL_OP_NO_ANTI_REPLAY
    SSL_CTX_set_options(ssl->context, SSL_OP_NO_ANTI_REPLAY);
#endif
#ifdef SSL_OP_NO_CLIENT_RENEGOTIATION
    SSL_CTX_set_options(ssl->ctx, SSL_OP_NO_CLIENT_RENEGOTIATION);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(ssl->context, SSL_MODE_RELEASE_BUFFERS);
#endif
#ifdef SSL_MODE_NO_AUTO_CHAIN
    SSL_CTX_set_mode(ssl->context, SSL_MODE_NO_AUTO_CHAIN);
#endif
    SSL_CTX_set_read_ahead(ssl->context, 1);
//    SSL_CTX_set_info_callback(ssl->context, ngx_ssl_info_callback);

    ssl->connection = NULL;

    return ssl;

    _failed:
    if (ssl->flags & CAT_SSL_FLAG_ALLOC) {
        cat_free(ssl);
    }

    return NULL;
}

CAT_API void cat_ssl_free(cat_ssl_t *ssl)
{
    SSL_CTX_free(ssl->context);
    if (ssl->flags & CAT_SSL_FLAG_ALLOC) {
        cat_free(ssl);
    }
}

CAT_API void cat_ssl_set_protocols(cat_ssl_t *ssl, cat_ssl_protocols_t protocols)
{
    if (!(protocols & CAT_SSL_PROTOCOL_SSLv2)) {
        SSL_CTX_set_options(ssl->context, SSL_OP_NO_SSLv2);
    }
    if (!(protocols & CAT_SSL_PROTOCOL_SSLv3)) {
        SSL_CTX_set_options(ssl->context, SSL_OP_NO_SSLv3);
    }
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1)) {
        SSL_CTX_set_options(ssl->context, SSL_OP_NO_TLSv1);
    }
#ifdef SSL_OP_NO_TLSv1_1
    SSL_CTX_clear_options(ssl->context, SSL_OP_NO_TLSv1_1);
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1_1)) {
        SSL_CTX_set_options(ssl->context, SSL_OP_NO_TLSv1_1);
    }
#endif
#ifdef SSL_OP_NO_TLSv1_2
    SSL_CTX_clear_options(ssl->context, SSL_OP_NO_TLSv1_2);
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1_2)) {
        SSL_CTX_set_options(ssl->context, SSL_OP_NO_TLSv1_2);
    }
#endif
#ifdef SSL_OP_NO_TLSv1_3
    SSL_CTX_clear_options(ssl->context, SSL_OP_NO_TLSv1_3);
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1_3)) {
        SSL_CTX_set_options(ssl->context, SSL_OP_NO_TLSv1_3);
    }
#endif
}

CAT_API cat_bool_t cat_ssl_open(cat_ssl_t *ssl, cat_ssl_flag_t side)
{
    if (unlikely(ssl->connection != NULL)) {
        cat_update_last_error(CAT_EISCONN, "SSL has already opened");
        goto _failed;
    }
    if (unlikely(side != CAT_SSL_TYPE_SERVER && side != CAT_SSL_TYPE_CLIENT)) {
        cat_update_last_error(CAT_EINVAL, "Unknown SSL side");
        goto _failed;
    }

    ssl->connection = SSL_new(ssl->context);
    if (unlikely(ssl->connection == NULL)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_new() failed");
        goto _failed;
    }
    if (SSL_set_ex_data(ssl->connection, cat_ssl_index, ssl) == 0) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_set_ex_data() failed");
        goto _failed;
    }

    if (unlikely(BIO_new_bio_pair(&ssl->ibio, 0, &ssl->nbio, 0) != 1)) {
        cat_ssl_update_last_error(CAT_ESSL, "BIO_new_bio_pair() failed");
        goto _new_bio_pair_failed;
    }
    SSL_set_bio(ssl->connection, ssl->ibio, ssl->ibio);

    if (unlikely(!cat_buffer_make_pair(&ssl->rbuffer, CAT_SSL_RBUFFER_SIZE, &ssl->wbuffer, CAT_SSL_WBUFFER_SIZE))) {
        cat_update_last_error_with_previous("SSL create buffer failed");
        goto _alloc_buffer_faile;
    }

    if (side == CAT_SSL_TYPE_SERVER) {
        SSL_set_accept_state(ssl->connection); /* sets ssl to work in server mode. */
        cat_debug(SSL, "SSL_set_accept_state()");
#ifdef SSL_OP_NO_RENEGOTIATION
        SSL_set_options(ssl->connection, SSL_OP_NO_RENEGOTIATION);
        cat_debug(SSL, "SSL_set_options(SSL_OP_NO_RENEGOTIATION)");
#endif
    } else if (side == CAT_SSL_TYPE_CLIENT)  {
        SSL_set_connect_state(ssl->connection); /* sets ssl to work in client mode. */
        cat_debug(SSL, "SSL_set_connect_state()");
    }
    ssl->flags |= side;

    return cat_true;

    _alloc_buffer_faile:
    BIO_free(ssl->nbio); /* ibio will be free'd by SSL_free */

    _new_bio_pair_failed:
    SSL_free(ssl->connection);
    ssl->connection = NULL;

    _failed:
    return cat_false;
}

CAT_API void cat_ssl_close(cat_ssl_t *ssl)
{
    if (unlikely(ssl->connection == NULL)) {
        return;
    }
    SSL_free(ssl->connection); /* implicitly frees internal_bio */
    ssl->connection = NULL;
    ssl->ibio = NULL;
    BIO_free(ssl->nbio);
    ssl->nbio = NULL;
    /* reserve persistent flags only */
    ssl->flags &= CAT_SSL_FLAGS_PERSISTENT;
}

CAT_API cat_bool_t cat_ssl_set_host_name(cat_ssl_t *ssl, const char *name)
{
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
    CAT_SSL_GET_CONNECTION(ssl, connection, return cat_false);
    int error = SSL_set_tlsext_host_name(connection, name);

    if (unlikely(error != 1)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_set_tlsext_host_name(\"%s\") failed", name);
        return cat_false;
    }
    return cat_true;
#else
    cat_update_last_error(CAT_ENOTSUP, "SSL version(%s) is too low", cat_ssl_version());
    return cat_false;
#endif
}

CAT_API cat_ssl_ret_t cat_ssl_handshake(cat_ssl_t *ssl)
{
    CAT_SSL_GET_CONNECTION(ssl, connection, return CAT_SSL_RET_ERROR);
    int n, ssl_error;

    cat_ssl_clear_error();

    n = SSL_do_handshake(connection);

    cat_debug(SSL, "SSL_do_handshake: %d", n);
    if (n == 1) {
        cat_ssl_handshake_log(ssl);
        ssl->flags |= CAT_SSL_HANDSHAKED;
#ifndef SSL_OP_NO_RENEGOTIATION
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#ifdef SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS
        /* initial handshake done, disable renegotiation (CVE-2009-3555) */
        if (ssl->connection->s3 && SSL_is_server(ssl->connection)) {
            ssl->connection->s3->flags |= SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS;
        }
#endif
#endif
#endif
        return CAT_SSL_RET_OK;
    }

    ssl_error = SSL_get_error(connection, n);

    cat_debug(SSL, "SSL_get_error: %d", ssl_error);

    if (ssl_error == SSL_ERROR_WANT_READ) {
        cat_debug(SSL, "SSL_ERROR_WANT_READ");
        return CAT_SSL_RET_WANT_IO;
    }
    /* memory bios should never block with SSL_ERROR_WANT_WRITE. */

    if (ssl_error == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0) {
        cat_update_last_error_of_syscall("Peer closed connection in SSL handshake");
    } else {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_do_handshake() failed");
    }

    return CAT_SSL_RET_ERROR;
}

CAT_API int cat_ssl_read_encrypted_bytes(cat_ssl_t *ssl, char *buffer, size_t size)
{
    int n;

    n = BIO_read(ssl->nbio, buffer, size);
    cat_debug(SSL, "BIO_read(%zu) = %d", size, n);
    if (unlikely(n <= 0)) {
        if (unlikely(!BIO_should_retry(ssl->nbio))) {
            cat_ssl_update_last_error(CAT_ESSL, "BIO_read(%zu) failed", size);
            return CAT_RET_ERROR;
        }
        return CAT_RET_AGAIN;
    }

    return n;
}

CAT_API int cat_ssl_write_unencrypted_bytes(cat_ssl_t *ssl, const char *buffer, size_t length)
{
    int n;

    n = BIO_write(ssl->nbio, buffer, length);
    cat_debug(SSL, "BIO_write(%zu) = %d", length, n);
    if (unlikely(n <= 0)) {
        if (unlikely(!BIO_should_retry(ssl->nbio))) {
            cat_ssl_update_last_error(CAT_ESSL, "BIO_write(%zu) failed", length);
            return CAT_RET_ERROR;
        }
        return CAT_RET_AGAIN;
    }

    return n;
}

CAT_API char *cat_ssl_get_error_reason(void)
{
    char *errstr = NULL, *errstr2;
    unsigned long n;
    const char *data;
    int flags;

    if (ERR_peek_error()) {
        while (1) {
            n = ERR_peek_error_line_data(NULL, NULL, &data, &flags);
            if (n == 0) {
                break;
            }
            if (*data || !(flags & ERR_TXT_STRING)) {
                data = "NULL";
            }
            if (errstr == NULL) {
                errstr = cat_sprintf(" (SSL: %s:%s)", ERR_error_string(n, NULL), data);
            } else {
                errstr2 = cat_sprintf("%s (SSL: %s:%s)", errstr, ERR_error_string(n, NULL), data);
                if (unlikely(errstr2 == NULL)) {
                    ERR_print_errors_fp(stderr);
                    break;
                }
                cat_free(errstr);
                errstr = errstr2;
            }
            (void) ERR_get_error();
        }
    }

    return errstr;
}

CAT_API void cat_ssl_update_last_error(cat_errno_t error, char *format, ...)
{
    va_list args;
    char *message, *reason;

    va_start(args, format);
    message = cat_vsprintf(format, args);
    va_end(args);

    if (unlikely(message == NULL)) {
        ERR_print_errors_fp(stderr);
        return;
    }

    reason = cat_ssl_get_error_reason();
    if (reason != NULL) {
        char *output;
        output = cat_sprintf("%s%s", message, reason);
        if (output != NULL) {
            cat_free(message);
            message = output;
        }
        cat_free(reason);
    }

    cat_set_last_error(error, message);
}

static void cat_ssl_clear_error(void)
{
    while (unlikely(ERR_peek_error())) {
        char *reason = cat_ssl_get_error_reason();
        cat_notice(SSL, "Ignoring stale global SSL error %s", reason != NULL ? reason : "");
        if (reason != NULL) {
            cat_free(reason);
        }
    }
    ERR_clear_error();
}

#ifdef CAT_DEBUG
static void cat_ssl_handshake_log(cat_ssl_t *ssl)
{
    char buf[129], *s, *d;
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
    const
#endif
    SSL_CIPHER *cipher;

    cipher = SSL_get_current_cipher(ssl->connection);
    if (cipher) {
        SSL_CIPHER_description(cipher, &buf[1], 128);
        for (s = &buf[1], d = buf; *s; s++) {
            if (*s == ' ' && *d == ' ') {
                continue;
            }
            if (*s == CAT_LF || *s == CAT_CR) {
                continue;
            }
            *++d = *s;
        }
        if (*d != ' ') {
            d++;
        }
        *d = '\0';
        cat_debug(SSL, "SSL: %s, cipher: \"%s\"", SSL_get_version(ssl->connection), &buf[1]);
        if (SSL_session_reused(ssl->connection)) {
            cat_debug(SSL, "SSL reused session");
        }
    } else {
        cat_debug(SSL, "SSL no shared ciphers");
    }
}
#endif

#endif /* CAT_SSL */
