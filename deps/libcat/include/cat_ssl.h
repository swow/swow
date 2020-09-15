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

#ifndef HAVE_OPENSSL
#define CAT_SSL_ENUM_GEN(XX) CAT_ENUM_EMPTY_GEN
#else
#define CAT_SSL 1
#define CAT_SSL_ENUM_GEN(XX) XX

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
#define cat_ssl_version()       OpenSSL_version(OPENSSL_VERSION)
#else
#define cat_ssl_version()       SSLeay_version(SSLEAY_VERSION)
#endif

typedef enum
{
    CAT_SSL_FLAG_ALLOC  = 1 << 0,
    CAT_SSL_TYPE_SERVER = 1 << 1,
    CAT_SSL_TYPE_CLIENT = 1 << 2,
    CAT_SSL_HANDSHAKED  = 1 << 3,
} cat_ssl_flag_t;

typedef enum
{
    CAT_SSL_FLAGS_PERSISTENT = CAT_SSL_FLAG_ALLOC,
} cat_ssl_union_flags_t;

typedef uint32_t cat_ssl_flags_t;

typedef enum
{
    CAT_SSL_PROTOCOL_SSLv2   = 1 << 0,
    CAT_SSL_PROTOCOL_SSLv3   = 1 << 1,
    CAT_SSL_PROTOCOL_TLSv1   = 1 << 2,
    CAT_SSL_PROTOCOL_TLSv1_1 = 1 << 3,
    CAT_SSL_PROTOCOL_TLSv1_2 = 1 << 4,
    CAT_SSL_PROTOCOL_TLSv1_3 = 1 << 5,
} cat_ssl_protocol_t;

typedef enum
{
    CAT_SSL_PROTOCOLS_ALL =
        CAT_SSL_PROTOCOL_SSLv2 |
        CAT_SSL_PROTOCOL_SSLv3 |
        CAT_SSL_PROTOCOL_TLSv1 |
        CAT_SSL_PROTOCOL_TLSv1_1 |
        CAT_SSL_PROTOCOL_TLSv1_2 |
        CAT_SSL_PROTOCOL_TLSv1_3,
} cat_ssl_union_protocols_t;

typedef unsigned int cat_ssl_protocols_t;

typedef SSL_CTX cat_ssl_context_t;
typedef SSL     cat_ssl_connection_t;
typedef BIO     cat_ssl_bio_t;

typedef struct
{
    cat_ssl_flags_t flags;
    cat_ssl_context_t *context;
    cat_ssl_connection_t *connection;
    cat_ssl_bio_t *ibio;
    cat_ssl_bio_t *nbio;
    cat_buffer_t rbuffer;
    cat_buffer_t wbuffer;
} cat_ssl_t;

#define CAT_SSL_RBUFFER_SIZE 131072
#define CAT_SSL_WBUFFER_SIZE 16384

CAT_API cat_bool_t cat_ssl_module_init(void);

/* context (persistent) */
CAT_API cat_ssl_t *cat_ssl_alloc(cat_ssl_t *ssl);
CAT_API void cat_ssl_set_protocols(cat_ssl_t *ssl, cat_ssl_protocols_t protocols);
CAT_API void cat_ssl_free(cat_ssl_t *ssl);

/* connection */
CAT_API cat_bool_t cat_ssl_open(cat_ssl_t *ssl, cat_ssl_flag_t side);
CAT_API cat_bool_t cat_ssl_set_host_name(cat_ssl_t *ssl, const char *name);

typedef enum {
    CAT_SSL_RET_OK         = 0,
    CAT_SSL_RET_ERROR      = -1,
    CAT_SSL_RET_WANT_READ  = 1 << 0,
    CAT_SSL_RET_WANT_WRITE = 1 << 1,
    CAT_SSL_RET_WANT_IO = CAT_SSL_RET_WANT_READ | CAT_SSL_RET_WANT_WRITE,
} cat_ssl_ret_t;

CAT_API cat_ssl_ret_t cat_ssl_handshake(cat_ssl_t *ssl);
CAT_API int cat_ssl_read_encrypted_bytes(cat_ssl_t *ssl, char *buffer, size_t size);
CAT_API int cat_ssl_write_unencrypted_bytes(cat_ssl_t *ssl, const char *buffer, size_t length);

CAT_API void cat_ssl_close(cat_ssl_t *ssl);

/* errors */
CAT_API char *cat_ssl_get_error_reason(void);
CAT_API void cat_ssl_update_last_error(cat_errno_t code, char *format, ...);

#endif /* HAVE_OPENSSL */
#ifdef __cplusplus
}
#endif
#endif /* CAT_SSL_H */
