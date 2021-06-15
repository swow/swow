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

#ifndef CAT_SSL_H
#define CAT_SSL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#ifdef CAT_HAVE_OPENSSL
#define CAT_SSL 1

#include "cat_buffer.h"

#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/dh.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif
#include <openssl/evp.h>
#include <openssl/hmac.h>
#ifndef OPENSSL_NO_OCSP
#include <openssl/ocsp.h>
#endif
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/ossl_typ.h>

#if (defined LIBRESSL_VERSION_NUMBER && OPENSSL_VERSION_NUMBER == 0x20000000L)
#undef OPENSSL_VERSION_NUMBER
#if (LIBRESSL_VERSION_NUMBER >= 0x2080000fL)
#define OPENSSL_VERSION_NUMBER  0x1010000fL
#else
#define OPENSSL_VERSION_NUMBER  0x1000107fL
#endif
#endif

#if (OPENSSL_VERSION_NUMBER >= 0x10100001L)
#define cat_ssl_version() OpenSSL_version(OPENSSL_VERSION)
#else
#define cat_ssl_version() SSLeay_version(SSLEAY_VERSION)
#endif

#define OPENSSL_DEFAULT_STREAM_VERIFY_DEPTH 9

typedef enum cat_ssl_flag_e {
    CAT_SSL_FLAG_NONE                  = 0,
    CAT_SSL_FLAG_ALLOC                 = 1 << 0,
    CAT_SSL_FLAG_ACCEPT_STATE          = 1 << 1,
    CAT_SSL_FLAG_CONNECT_STATE         = 1 << 2,
    CAT_SSL_FLAG_HANDSHAKED            = 1 << 3,
    CAT_SSL_FLAG_RENEGOTIATION         = 1 << 4,
    CAT_SSL_FLAG_HANDSHAKE_BUFFER_SET  = 1 << 5,
} cat_ssl_flag_t;

typedef enum cat_ssl_union_flags_e {
    CAT_SSL_FLAGS_STATE = CAT_SSL_FLAG_ACCEPT_STATE | CAT_SSL_FLAG_CONNECT_STATE,
} cat_ssl_union_flags_t;

typedef uint32_t cat_ssl_flags_t;

#define CAT_SSL_METHOD_MAP(XX) \
    XX(TLS,  1 << 0) \
    XX(DTLS, 1 << 1) \

typedef enum cat_ssl_method_e {
#define CAT_SSL_METHOD_GEN(name, value) CAT_ENUM_GEN(CAT_SSL_METHOD_, name, value)
    CAT_SSL_METHOD_MAP(CAT_SSL_METHOD_GEN)
#undef CAT_SSL_METHOD_GEN
} cat_ssl_method_t;

#define CAT_SSL_PROTOCOL_MAP(XX) \
    XX(SSLv2,   1 << 0) \
    XX(SSLv3,   1 << 1) \
    XX(TLSv1,   1 << 2) \
    XX(TLSv1_1, 1 << 3) \
    XX(TLSv1_2, 1 << 4) \
    XX(TLSv1_3, 1 << 5) \

typedef enum cat_ssl_protocol_e {
#define CAT_SSL_PROTOCOL_GEN(name, value) CAT_ENUM_GEN(CAT_SSL_PROTOCOL_, name, value)
    CAT_SSL_PROTOCOL_MAP(CAT_SSL_PROTOCOL_GEN)
#undef CAT_SSL_PROTOCOL_GEN
} cat_ssl_protocol_t;

typedef enum cat_ssl_union_protocols_e {
#define CAT_SSL_PROTOCO_ALLL_GEN(name, value) CAT_SSL_PROTOCOL_##name |
    CAT_SSL_PROTOCOLS_ALL = CAT_SSL_PROTOCOL_MAP(CAT_SSL_PROTOCO_ALLL_GEN) 0,
#undef CAT_SSL_PROTOCO_ALLL_GEN
} cat_ssl_union_protocols_t;

typedef unsigned int cat_ssl_protocols_t;

typedef SSL_CTX cat_ssl_context_t;
typedef SSL     cat_ssl_connection_t;
typedef BIO     cat_ssl_bio_t;

typedef struct cat_ssl_s {
    cat_ssl_flags_t flags;
    cat_ssl_context_t *context;
    cat_ssl_connection_t *connection;
    cat_ssl_bio_t *nbio;
    cat_buffer_t read_buffer;
    cat_buffer_t write_buffer;
    /* options */
    cat_bool_t allow_self_signed;
    cat_string_t passphrase;
} cat_ssl_t;

#define CAT_SSL_MAX_BLOCK_LENGTH  EVP_MAX_BLOCK_LENGTH
#define CAT_SSL_MAX_PLAIN_LENGTH  SSL3_RT_MAX_PLAIN_LENGTH
#define CAT_SSL_BUFFER_SIZE       SSL3_RT_MAX_PACKET_SIZE

CAT_API cat_bool_t cat_ssl_module_init(void);

/* context */
CAT_API cat_ssl_context_t *cat_ssl_context_create(cat_ssl_method_t method);
CAT_API void cat_ssl_context_close(cat_ssl_context_t *context);

CAT_API void cat_ssl_context_set_protocols(cat_ssl_context_t *context, cat_ssl_protocols_t protocols);
CAT_API cat_bool_t cat_ssl_context_set_default_verify_paths(cat_ssl_context_t *context);
CAT_API cat_bool_t cat_ssl_context_set_ca_file(cat_ssl_context_t *context, const char *ca_file);
CAT_API cat_bool_t cat_ssl_context_set_ca_path(cat_ssl_context_t *context, const char *ca_path);
CAT_API void cat_ssl_context_set_verify_depth(cat_ssl_context_t *context, int depth);
#ifdef CAT_OS_WIN
CAT_API void cat_ssl_context_configure_verify(cat_ssl_context_t *context);
#endif

/* connection */
CAT_API cat_ssl_t *cat_ssl_create(cat_ssl_t *ssl, cat_ssl_context_t *context);
CAT_API void cat_ssl_close(cat_ssl_t *ssl);

CAT_API cat_buffer_t *cat_ssl_get_read_buffer(cat_ssl_t *ssl, size_t size);

CAT_API void cat_ssl_set_accept_state(cat_ssl_t *ssl);
CAT_API void cat_ssl_set_connect_state(cat_ssl_t *ssl);

CAT_API cat_bool_t cat_ssl_set_sni_server_name(cat_ssl_t *ssl, const char *name);
CAT_API cat_bool_t cat_ssl_set_passphrase(cat_ssl_t *ssl, const char *passphrase, size_t passphrase_length);

typedef enum cat_ssl_ret_e {
    CAT_SSL_RET_OK         = 0,
    CAT_SSL_RET_ERROR      = -1,
    CAT_SSL_RET_WANT_READ  = 1 << 0,
    CAT_SSL_RET_WANT_WRITE = 1 << 1,
    CAT_SSL_RET_WANT_IO = CAT_SSL_RET_WANT_READ | CAT_SSL_RET_WANT_WRITE,
} cat_ssl_ret_t;

CAT_API cat_bool_t cat_ssl_is_established(const cat_ssl_t *ssl);

CAT_API cat_ssl_ret_t cat_ssl_handshake(cat_ssl_t *ssl);

CAT_API cat_bool_t cat_ssl_verify_peer(cat_ssl_t *ssl, cat_bool_t allow_self_signed);
CAT_API cat_bool_t cat_ssl_check_host(cat_ssl_t *ssl, const char *name, size_t name_length);

CAT_API int cat_ssl_read_encrypted_bytes(cat_ssl_t *ssl, char *buffer, size_t size);
CAT_API int cat_ssl_write_encrypted_bytes(cat_ssl_t *ssl, const char *buffer, size_t length);

CAT_API size_t cat_ssl_encrypted_size(size_t length);
CAT_API cat_bool_t cat_ssl_encrypt(cat_ssl_t *ssl, const cat_io_vector_t *vin, unsigned int vin_count, cat_io_vector_t *vout, unsigned int *vout_count);
CAT_API void cat_ssl_encrypted_vector_free(cat_ssl_t *ssl, cat_io_vector_t *vector, unsigned int vector_count);
CAT_API cat_bool_t cat_ssl_decrypt(cat_ssl_t *ssl, char *out, size_t *out_length);

/* errors */
CAT_API char *cat_ssl_get_error_reason(void);
CAT_API void cat_ssl_update_last_error(cat_errno_t code, const char *format, ...);

#endif /* CAT_HAVE_OPENSSL */
#ifdef __cplusplus
}
#endif
#endif /* CAT_SSL_H */
