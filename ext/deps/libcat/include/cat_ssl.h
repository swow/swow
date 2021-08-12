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
#include "cat_ref.h"

#ifdef CAT_HAVE_OPENSSL
#define CAT_SSL 1

#include "cat_buffer.h"

#include <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER < 0x10002000L
#error "Require OpenSSL >= 1.0.2"
#endif

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

#ifdef CAT_OS_WIN
#include <Wincrypt.h>
/* These are from Wincrypt.h, they conflict with OpenSSL */
#undef X509_NAME
#undef X509_CERT_PAIR
#undef X509_EXTENSIONS
#endif

#if (defined LIBRESSL_VERSION_NUMBER && OPENSSL_VERSION_NUMBER == 0x20000000L)
#undef OPENSSL_VERSION_NUMBER
#if (LIBRESSL_VERSION_NUMBER >= 0x2080000fL)
#define OPENSSL_VERSION_NUMBER  0x1010000fL
#else
#define OPENSSL_VERSION_NUMBER  0x1000107fL
#endif
#endif

#define CAT_SSL_DEFAULT_STREAM_VERIFY_DEPTH 9

#if (OPENSSL_VERSION_NUMBER >= 0x10100001L)
#define cat_ssl_version() OpenSSL_version(OPENSSL_VERSION)
#else
#define cat_ssl_version() SSLeay_version(SSLEAY_VERSION)
#endif

#define CAT_SSL_MAX_BLOCK_LENGTH  EVP_MAX_BLOCK_LENGTH
#define CAT_SSL_MAX_PLAIN_LENGTH  SSL3_RT_MAX_PLAIN_LENGTH
#define CAT_SSL_BUFFER_SIZE       SSL3_RT_MAX_PACKET_SIZE

typedef enum cat_ssl_flag_e {
    CAT_SSL_FLAG_NONE                  = 0,
    CAT_SSL_FLAG_ALLOC                 = 1 << 0,
    CAT_SSL_FLAG_ACCEPT_STATE          = 1 << 1,
    CAT_SSL_FLAG_CONNECT_STATE         = 1 << 2,
    CAT_SSL_FLAG_HANDSHAKED            = 1 << 3,
    CAT_SSL_FLAG_RENEGOTIATION         = 1 << 4,
    CAT_SSL_FLAG_HANDSHAKE_BUFFER_SET  = 1 << 5,
    CAT_SSL_FLAG_UNRECOVERABLE_ERROR   = 1 << 31,
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
#define CAT_SSL_PROTOCOL_ALL_GEN(name, value) CAT_SSL_PROTOCOL_##name |
    CAT_SSL_PROTOCOLS_ALL = CAT_SSL_PROTOCOL_MAP(CAT_SSL_PROTOCOL_ALL_GEN) 0,
#undef CAT_SSL_PROTOCOL_ALL_GEN
    CAT_SSL_PROTOCOLS_DEFAULT = CAT_SSL_PROTOCOL_TLSv1_1 |
                                CAT_SSL_PROTOCOL_TLSv1_2 |
                                CAT_SSL_PROTOCOL_TLSv1_3,
} cat_ssl_union_protocols_t;

typedef unsigned int cat_ssl_protocols_t;

typedef SSL_CTX cat_ssl_ctx_t;
typedef SSL     cat_ssl_connection_t;
typedef BIO     cat_ssl_bio_t;

typedef struct cat_ssl_context_s {
    CAT_REF_FIELD;
    cat_ssl_ctx_t *ctx;
    cat_string_t passphrase;
} cat_ssl_context_t;

typedef struct cat_ssl_s {
    cat_ssl_flags_t flags;
    cat_ssl_connection_t *connection;
    cat_ssl_bio_t *nbio;
    cat_buffer_t read_buffer;
    cat_buffer_t write_buffer;
    /* options */
    cat_bool_t allow_self_signed;
} cat_ssl_t;

typedef enum cat_ssl_ret_e {
    CAT_SSL_RET_OK         = 1,
    CAT_SSL_RET_NONE       = 0,
    CAT_SSL_RET_ERROR      = -1,
    CAT_SSL_RET_WANT_READ  = 1 << 0,
    CAT_SSL_RET_WANT_WRITE = 1 << 1,
    CAT_SSL_RET_WANT_IO = CAT_SSL_RET_WANT_READ | CAT_SSL_RET_WANT_WRITE,
} cat_ssl_ret_t;

CAT_API cat_bool_t cat_ssl_module_init(void);

/* context */
CAT_API cat_ssl_context_t *cat_ssl_context_create(cat_ssl_method_t method, cat_ssl_protocols_t protocols);
CAT_API void cat_ssl_context_close(cat_ssl_context_t *context);

CAT_API void cat_ssl_context_set_protocols(cat_ssl_context_t *context, cat_ssl_protocols_t protocols);

CAT_API cat_bool_t cat_ssl_context_set_client_ca_list(cat_ssl_context_t *context, const char *ca_file);
CAT_API cat_bool_t cat_ssl_context_set_default_verify_paths(cat_ssl_context_t *context);
CAT_API cat_bool_t cat_ssl_context_load_verify_locations(cat_ssl_context_t *context, const char *ca_file, const char *ca_path);
CAT_API cat_bool_t cat_ssl_context_set_passphrase(cat_ssl_context_t *context, const char *passphrase, size_t passphrase_length);
CAT_API cat_bool_t cat_ssl_context_set_certificate(cat_ssl_context_t *context, const char *certificate, const char *certificate_key);
CAT_API void cat_ssl_context_set_verify_depth(cat_ssl_context_t *context, int depth);
#ifdef CAT_OS_WIN
CAT_API void cat_ssl_context_configure_cert_verify_callback(cat_ssl_context_t *context);
#endif
CAT_API void cat_ssl_context_enable_verify_peer(cat_ssl_context_t *context);
CAT_API void cat_ssl_context_disable_verify_peer(cat_ssl_context_t *context);
CAT_API void cat_ssl_context_set_no_ticket(cat_ssl_context_t *context);
CAT_API void cat_ssl_context_set_no_compression(cat_ssl_context_t *context);

/* connection */

CAT_API cat_ssl_t *cat_ssl_create(cat_ssl_t *ssl, cat_ssl_context_t *context);
CAT_API void cat_ssl_close(cat_ssl_t *ssl);

CAT_API void cat_ssl_set_accept_state(cat_ssl_t *ssl);
CAT_API void cat_ssl_set_connect_state(cat_ssl_t *ssl);

CAT_API cat_bool_t cat_ssl_set_sni_server_name(cat_ssl_t *ssl, const char *name);

CAT_API cat_bool_t cat_ssl_is_established(const cat_ssl_t *ssl);

CAT_API cat_ssl_ret_t cat_ssl_handshake(cat_ssl_t *ssl);

CAT_API cat_bool_t cat_ssl_verify_peer(cat_ssl_t *ssl, cat_bool_t allow_self_signed);
CAT_API cat_bool_t cat_ssl_check_host(cat_ssl_t *ssl, const char *name, size_t name_length);

CAT_API int cat_ssl_read_encrypted_bytes(cat_ssl_t *ssl, char *buffer, size_t size);
CAT_API int cat_ssl_write_encrypted_bytes(cat_ssl_t *ssl, const char *buffer, size_t length);

CAT_API size_t cat_ssl_encrypted_size(size_t length);
CAT_API cat_bool_t cat_ssl_encrypt(cat_ssl_t *ssl, const cat_io_vector_t *vin, unsigned int vin_count, cat_io_vector_t *vout, unsigned int *vout_count);
CAT_API void cat_ssl_encrypted_vector_free(cat_ssl_t *ssl, cat_io_vector_t *vector, unsigned int vector_count);
CAT_API cat_bool_t cat_ssl_decrypt(cat_ssl_t *ssl, char *out, size_t *out_length, cat_bool_t *eof);

typedef enum cat_ssl_shutdown_mask_e {
    CAT_SSL_SENT_SHUTDOWN = SSL_SENT_SHUTDOWN,
    CAT_SSL_RECEIVED_SHUTDOWN = SSL_RECEIVED_SHUTDOWN,
} cat_ssl_shutdown_mask_t;

typedef int cat_ssl_shutdown_masks_t;

CAT_API cat_ssl_shutdown_masks_t cat_ssl_get_shutdown(const cat_ssl_t *ssl);
CAT_API void cat_ssl_set_shutdown(cat_ssl_t *ssl, cat_ssl_shutdown_masks_t mode);
CAT_API void cat_ssl_set_quiet_shutdown(cat_ssl_t *ssl, cat_bool_t enable);

#if 0 // TODO: implement it
CAT_API cat_bool_t cat_ssl_shutdown(cat_ssl_t *ssl);
#endif

/* errors */

CAT_API CAT_COLD char *cat_ssl_get_error_reason(void);
CAT_API CAT_COLD void cat_ssl_update_last_error(cat_errno_t code, const char *format, ...);
CAT_API CAT_COLD cat_bool_t cat_ssl_is_down(const cat_ssl_t *ssl);
CAT_API CAT_COLD void cat_ssl_unrecoverable_error(cat_ssl_t *ssl);

#endif /* CAT_HAVE_OPENSSL */
#ifdef __cplusplus
}
#endif
#endif /* CAT_SSL_H */
