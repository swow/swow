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

static int cat_ssl_get_error(const cat_ssl_t *ssl, int ret_code);
static void cat_ssl_clear_error(void);
static void cat_ssl_info_callback(const cat_ssl_connection_t *connection, int where, int ret);
#ifdef CAT_DEBUG
static void cat_ssl_handshake_log(cat_ssl_t *ssl);
#else
#define cat_ssl_handshake_log(ssl)
#endif

static int cat_ssl_index;
static int cat_ssl_context_index;

static cat_always_inline cat_ssl_t *cat_ssl_get_from_connection(const cat_ssl_connection_t *connection)
{
    return (cat_ssl_t *) SSL_get_ex_data(connection, cat_ssl_index);
}

#ifdef CAT_DEBUG
static cat_always_inline cat_ssl_context_t *cat_ssl_context_get_from_ctx(const cat_ssl_ctx_t *ctx)
{
    return (cat_ssl_context_t *) SSL_CTX_get_ex_data(ctx, cat_ssl_context_index);
}
#endif

CAT_API cat_bool_t cat_ssl_module_init(void)
{
    /* SSL library initialisation */
#if OPENSSL_VERSION_NUMBER >= 0x10100003L
    if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG | OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_ADD_ALL_CIPHERS, NULL) == 0) {
        ERR_print_errors_fp(CAT_G(error_log));
        CAT_CORE_ERROR(SSL, "OPENSSL_init_ssl() failed");
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
        int n;
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
        ERR_print_errors_fp(CAT_G(error_log));
        CAT_CORE_ERROR(SSL, "SSL_get_ex_new_index() failed");
    }
    cat_ssl_context_index = SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    if (cat_ssl_context_index == -1) {
        ERR_print_errors_fp(CAT_G(error_log));
        CAT_CORE_ERROR(SSL, "SSL_CTX_get_ex_new_index() failed");
    }

    return cat_true;
}

CAT_API cat_ssl_context_t *cat_ssl_context_create(cat_ssl_method_t method, cat_ssl_protocols_t protocols)
{
    cat_ssl_context_t *context;
    cat_ssl_ctx_t *ctx;
    unsigned long options = 0;

    /* new SSL CTX */
    ctx = SSL_CTX_new(method == CAT_SSL_METHOD_TLS ? SSLv23_method() : DTLS_method());
    if (unlikely(ctx == NULL)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_CTX_new() failed");
        goto _new_failed;
    }

    /* malloc for SSL context */
    context = (cat_ssl_context_t *) cat_malloc(sizeof(*context));
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(context == NULL)) {
        cat_update_last_error_of_syscall("Malloc for SSL context failed");
        goto _malloc_failed;
    }
#endif
    CAT_REF_INIT(context);
    context->ctx = ctx;

#ifdef CAT_DEBUG
    /* link context to SSL_CTX */
    if (unlikely(SSL_CTX_set_ex_data(ctx, cat_ssl_context_index, context) == 0)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_CTX_set_ex_data() failed");
        goto _set_ex_data_failed;
    }
    CAT_ASSERT(cat_ssl_context_get_from_ctx(ctx) == context);
#endif

    /* set protocols */
    cat_ssl_context_set_protocols(context, protocols);
    /* client side options */
#ifdef SSL_OP_MICROSOFT_SESS_ID_BUG
    options |= SSL_OP_MICROSOFT_SESS_ID_BUG;
#endif
#ifdef SSL_OP_NETSCAPE_CHALLENGE_BUG
    options |= SSL_OP_NETSCAPE_CHALLENGE_BUG;
#endif
    /* server side options */
#ifdef SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG
    options |= SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG;
#endif
#ifdef SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER
    options |= SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER;
#endif
#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
    options |= SSL_OP_MSIE_SSLV2_RSA_PADDING;
#endif
#ifdef SSL_OP_SSLEAY_080_CLIENT_DH_BUG
    options |= SSL_OP_SSLEAY_080_CLIENT_DH_BUG;
#endif
#ifdef SSL_OP_TLS_D5_BUG
    options |= SSL_OP_TLS_D5_BUG;
#endif
#ifdef SSL_OP_TLS_BLOCK_PADDING_BUG
    options |= SSL_OP_TLS_BLOCK_PADDING_BUG;
#endif
#ifdef SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
    options |= SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS;
#endif
#ifdef SSL_OP_NO_COMPRESSION
    options |= SSL_OP_NO_COMPRESSION;
#endif
#ifdef SSL_OP_NO_ANTI_REPLAY
    options |= SSL_OP_NO_ANTI_REPLAY;
#endif
#ifdef SSL_OP_NO_CLIENT_RENEGOTIATION
    options |= SSL_OP_NO_CLIENT_RENEGOTIATION;
#endif
    SSL_CTX_set_options(ctx, options);
    /* set min/max proto version */
#ifdef SSL_CTX_set_min_proto_version
    SSL_CTX_set_min_proto_version(ctx, 0);
#endif
#ifdef SSL_CTX_set_max_proto_version
#ifndef TLS1_3_VERSION
    SSL_CTX_set_max_proto_version(ctx, TLS1_2_VERSION);
#else
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
#endif
#endif
    /* set mode */
#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS);
#endif
#ifdef SSL_MODE_NO_AUTO_CHAIN
    SSL_CTX_set_mode(ctx, SSL_MODE_NO_AUTO_CHAIN);
#endif
    /* set read_ahead/info_callback */
    SSL_CTX_set_read_ahead(ctx, 1);
    SSL_CTX_set_info_callback(ctx, cat_ssl_info_callback);

    /* init extra info */
    cat_string_init(&context->passphrase);

    return context;

#ifdef CAT_DEBUG
    _set_ex_data_failed:
#endif
    cat_free(context);
#if CAT_ALLOC_HANDLE_ERRORS
    _malloc_failed:
#endif
    SSL_CTX_free(ctx);
    _new_failed:
    return NULL;
}

CAT_API void cat_ssl_context_close(cat_ssl_context_t *context)
{
    SSL_CTX_free(context->ctx);
    if (CAT_REF_DEL(context) != 0) {
        return;
    }
    cat_string_close(&context->passphrase);
    cat_free(context);
}

CAT_API void cat_ssl_context_set_protocols(cat_ssl_context_t *context, cat_ssl_protocols_t protocols)
{
    cat_ssl_ctx_t *ctx = context->ctx;
    unsigned long options = 0;

#ifdef SSL_OP_NO_SSL_MASK
    SSL_CTX_clear_options(ctx, SSL_OP_NO_SSL_MASK);
#else
    SSL_CTX_clear_options(ctx,
        SSL_OP_NO_SSLv2 |
        SSL_OP_NO_SSLv3 |
        SSL_OP_NO_TLSv1 |
#ifdef SSL_OP_NO_TLSv1_1
        SSL_OP_NO_TLSv1_1|
#endif
#ifdef SSL_OP_NO_TLSv1_2
        SSL_OP_NO_TLSv1_2|
#endif
#ifdef SSL_OP_NO_TLSv1_3
        SSL_OP_NO_TLSv1_3|
#endif
        0
    );
#endif
    if (!(protocols & CAT_SSL_PROTOCOL_SSLv2)) {
        options |= SSL_OP_NO_SSLv2;
    }
    if (!(protocols & CAT_SSL_PROTOCOL_SSLv3)) {
        options |= SSL_OP_NO_SSLv3;
    }
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1)) {
        options |= SSL_OP_NO_TLSv1;
    }
#ifdef SSL_OP_NO_TLSv1_1
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1_1)) {
        options |= SSL_OP_NO_TLSv1_1;
    }
#endif
#ifdef SSL_OP_NO_TLSv1_2
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1_2)) {
        options |= SSL_OP_NO_TLSv1_2;
    }
#endif
#ifdef SSL_OP_NO_TLSv1_3
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1_3)) {
        options |= SSL_OP_NO_TLSv1_3;
    }
#endif
    SSL_CTX_set_options(ctx, options);
}

CAT_API cat_bool_t cat_ssl_context_set_client_ca_list(cat_ssl_context_t *context, const char *ca_file)
{
    /* Servers need to load and assign CA names from the cafile */
    CAT_LOG_DEBUG(SSL, "SSL_load_client_CA_file(%p, \"%s\")", context, ca_file);
    STACK_OF(X509_NAME) *cert_names = SSL_load_client_CA_file(ca_file);
    if (cert_names == NULL) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_load_client_CA_file(\"%s\") failed", context, ca_file);
        return cat_false;
    }
    CAT_LOG_DEBUG(SSL, "SSL_CTX_set_client_CA_list(%p)", context);
    SSL_CTX_set_client_CA_list(context->ctx, cert_names);
    return cat_true;
}

CAT_API cat_bool_t cat_ssl_context_set_default_verify_paths(cat_ssl_context_t *context)
{
    CAT_LOG_DEBUG(SSL, "SSL_CTX_set_default_verify_paths(%p)", context);
    if (SSL_CTX_set_default_verify_paths(context->ctx) != 1) {
        cat_ssl_update_last_error(CAT_EINVAL, "SSL_CTX_set_default_verify_paths() failed");
        return cat_false;
    }
    return cat_true;
}

CAT_API cat_bool_t cat_ssl_context_load_verify_locations(cat_ssl_context_t *context, const char *ca_file, const char *ca_path)
{
    CAT_LOG_DEBUG(SSL, "SSL_CTX_load_verify_locations(%p, ca_file: " CAT_LOG_STRING_OR_NULL_FMT ", ca_path: " CAT_LOG_STRING_OR_NULL_FMT ")",
        context, CAT_LOG_STRING_OR_NULL_PARAM(ca_file), CAT_LOG_STRING_OR_NULL_PARAM(ca_path));
    if (SSL_CTX_load_verify_locations(context->ctx, ca_file, NULL) != 1) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_CTX_load_verify_locations() failed");
        return cat_false;
    }
    return cat_true;
}

static int cat_ssl_context_password_callback(char *buf, int length, int verify, void *data)
{
    (void) verify;
    cat_ssl_context_t *context = (cat_ssl_context_t *) data;

    if (context->passphrase.length < (size_t) length - 1) {
        memcpy(buf, context->passphrase.value, context->passphrase.length);
        return (int) context->passphrase.length;
    }

    return 0;
}

CAT_API cat_bool_t cat_ssl_context_set_passphrase(cat_ssl_context_t *context, const char *passphrase, size_t passphrase_length)
{
    cat_ssl_ctx_t *ctx = context->ctx;
    cat_bool_t ret;

    cat_string_close(&context->passphrase);
    ret = cat_string_create(&context->passphrase, passphrase, passphrase_length);
    if (unlikely(!ret)) {
        cat_update_last_error_of_syscall("Malloc for SSL passphrase failed");
        return cat_false;
    }
    CAT_LOG_DEBUG(SSL, "SSL_set_default_passwd(%p, passphrase=\"%.*s\")", context, (int) passphrase_length, passphrase);
    SSL_CTX_set_default_passwd_cb_userdata(ctx, context);
    SSL_CTX_set_default_passwd_cb(ctx, cat_ssl_context_password_callback);

    return cat_true;
}

CAT_API cat_bool_t cat_ssl_context_set_certificate(cat_ssl_context_t *context, const char *certificate, const char *certificate_key)
{
    cat_ssl_ctx_t *ctx = context->ctx;

    /* a certificate to use for authentication */
    CAT_LOG_DEBUG(SSL, "SSL_CTX_use_certificate_chain_file(%p, \"%s\")", context, certificate);
    if (SSL_CTX_use_certificate_chain_file(ctx, certificate) != 1) {
        cat_ssl_update_last_error(CAT_ESSL,  "SSL_CTX_use_certificate_chain_file(\"%s\") failed, "
            "check that your ca_file/ca_path settings include details of your certificate and its issuer", certificate);
        return cat_false;
    }
    if (certificate_key == NULL) {
        certificate_key = certificate;
    }
    CAT_LOG_DEBUG(SSL, "SSL_CTX_use_PrivateKey_file(%p, \"%s\")", context, certificate_key);
    if (SSL_CTX_use_PrivateKey_file(ctx, certificate_key, SSL_FILETYPE_PEM) != 1) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_CTX_use_PrivateKey_file(\"%s\") failed", certificate_key);
        return cat_false;
    }
    if (!SSL_CTX_check_private_key(ctx)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL private key does not match certificate");
        return cat_false;
    }
    return cat_true;
}

CAT_API void cat_ssl_context_set_verify_depth(cat_ssl_context_t *context, int depth)
{
    CAT_LOG_DEBUG(SSL, "SSL_CTX_set_verify_depth(%p, %d)", context, depth);
    SSL_CTX_set_verify_depth(context->ctx, depth);
}

#ifdef CAT_OS_WIN
static int cat_ssl_win_cert_verify_callback(X509_STORE_CTX *x509_store_ctx, void *data) /* {{{ */
{
    (void) data;
    PCCERT_CONTEXT cert_ctx = NULL;
    PCCERT_CHAIN_CONTEXT cert_chain_ctx = NULL;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    X509 *cert = x509_store_ctx->cert;
#else
    X509 *cert = X509_STORE_CTX_get0_cert(x509_store_ctx);
#endif
    cat_ssl_connection_t *connection = X509_STORE_CTX_get_ex_data(x509_store_ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
    cat_ssl_t *ssl = cat_ssl_get_from_connection(connection);
    cat_bool_t is_self_signed = 0;

    CAT_LOG_DEBUG(SSL, "SSL_cert_verify_callback(%p)", ssl);
    { /* First convert the x509 struct back to a DER encoded buffer and let Windows decode it into a form it can work with */
        unsigned char *der_buf = NULL;
        int der_len;

        der_len = i2d_X509(cert, &der_buf);
        if (der_len < 0) {
            unsigned long err_code = 0, e;
            char err_buf[512];

            while ((e = ERR_get_error()) != 0) {
                err_code = e;
            }

            cat_update_last_error(CAT_ECERT, "Error encoding X509 certificate: %d: %s", err_code, ERR_error_string(err_code, err_buf));
            X509_STORE_CTX_set_error(x509_store_ctx, SSL_R_CERTIFICATE_VERIFY_FAILED);
            return cat_false;
        }

        cert_ctx = CertCreateCertificateContext(X509_ASN_ENCODING, der_buf, der_len);
        OPENSSL_free(der_buf);

        if (cert_ctx == NULL) {
            cat_update_last_error_of_syscall("Error creating certificate context");
            X509_STORE_CTX_set_error(x509_store_ctx, SSL_R_CERTIFICATE_VERIFY_FAILED);
            return cat_false;
        }
    }

    { /* Next fetch the relevant cert chain from the store */
        CERT_ENHKEY_USAGE enhkey_usage = {0};
        CERT_USAGE_MATCH cert_usage = {0};
        CERT_CHAIN_PARA chain_params = {sizeof(CERT_CHAIN_PARA)};
        LPSTR usages[] = {szOID_PKIX_KP_SERVER_AUTH, szOID_SERVER_GATED_CRYPTO, szOID_SGC_NETSCAPE};
        DWORD chain_flags = 0;
        int allowed_depth;
        unsigned int i;

        enhkey_usage.cUsageIdentifier = 3;
        enhkey_usage.rgpszUsageIdentifier = usages;
        cert_usage.dwType = USAGE_MATCH_TYPE_OR;
        cert_usage.Usage = enhkey_usage;
        chain_params.RequestedUsage = cert_usage;
        chain_flags = CERT_CHAIN_CACHE_END_CERT | CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;

        if (!CertGetCertificateChain(NULL, cert_ctx, NULL, NULL, &chain_params, chain_flags, NULL, &cert_chain_ctx)) {
            cat_update_last_error_of_syscall("Error getting certificate chain");
            CertFreeCertificateContext(cert_ctx);
            X509_STORE_CTX_set_error(x509_store_ctx, SSL_R_CERTIFICATE_VERIFY_FAILED);
            return cat_false;
        }

        /* check if the cert is self-signed */
        if (cert_chain_ctx->cChain > 0 && cert_chain_ctx->rgpChain[0]->cElement > 0
            && (cert_chain_ctx->rgpChain[0]->rgpElement[0]->TrustStatus.dwInfoStatus & CERT_TRUST_IS_SELF_SIGNED) != 0) {
            is_self_signed = 1;
        }

        /* check the depth */
        allowed_depth = SSL_get_verify_depth(connection);
        if (allowed_depth < 0) {
            allowed_depth = CAT_SSL_DEFAULT_STREAM_VERIFY_DEPTH;
        }
        CAT_LOG_DEBUG(SSL, "SSL allowed depth is %d", allowed_depth);
        for (i = 0; i < cert_chain_ctx->cChain; i++) {
            int depth = (int) cert_chain_ctx->rgpChain[i]->cElement;
            if (depth > allowed_depth) {
                CAT_LOG_DEBUG(SSL, "SSL cert depth is %d, exceeded allowed_depth, abort", depth);
                CertFreeCertificateChain(cert_chain_ctx);
                CertFreeCertificateContext(cert_ctx);
                X509_STORE_CTX_set_error(x509_store_ctx, X509_V_ERR_CERT_CHAIN_TOO_LONG);
                return cat_false;
            }
        }
    }

    { /* Then verify it against a policy */
        SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl_policy_params = {sizeof(SSL_EXTRA_CERT_CHAIN_POLICY_PARA)};
        CERT_CHAIN_POLICY_PARA chain_policy_params = {sizeof(CERT_CHAIN_POLICY_PARA)};
        CERT_CHAIN_POLICY_STATUS chain_policy_status = {sizeof(CERT_CHAIN_POLICY_STATUS)};
        BOOL verify_result;

        ssl_policy_params.dwAuthType = (ssl->flags & CAT_SSL_FLAG_ACCEPT_STATE) ? AUTHTYPE_SERVER : AUTHTYPE_CLIENT;
        /* we validate the name ourselves using the peer_name
           ctx option, so no need to use a server name here */
        ssl_policy_params.pwszServerName = NULL;
        chain_policy_params.pvExtraPolicyPara = &ssl_policy_params;

        verify_result = CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, cert_chain_ctx, &chain_policy_params, &chain_policy_status);

        CertFreeCertificateChain(cert_chain_ctx);
        CertFreeCertificateContext(cert_ctx);

        if (!verify_result) {
            cat_update_last_error_of_syscall("Error verifying certificate chain policy");
            X509_STORE_CTX_set_error(x509_store_ctx, SSL_R_CERTIFICATE_VERIFY_FAILED);
            return cat_false;
        }

        if (chain_policy_status.dwError != 0) {
            /* The chain does not match the policy */
            if (is_self_signed && chain_policy_status.dwError == CERT_E_UNTRUSTEDROOT
                && ssl->allow_self_signed) {
                /* allow self-signed certs */
                CAT_LOG_DEBUG(SSL, "SSL connection use self-signed cert but we allowed");
                X509_STORE_CTX_set_error(x509_store_ctx, X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT);
            } else {
                X509_STORE_CTX_set_error(x509_store_ctx, SSL_R_CERTIFICATE_VERIFY_FAILED);
                return cat_false;
            }
        }
    }

    return cat_true;
}

CAT_API void cat_ssl_context_configure_cert_verify_callback(cat_ssl_context_t *context)
{
    CAT_LOG_DEBUG(SSL, "SSL_CTX_set_cert_verify_callback(%p)", context);
    SSL_CTX_set_cert_verify_callback(context->ctx, cat_ssl_win_cert_verify_callback, NULL);
}
#endif

static int cat_ssl_verify_callback(int preverify_ok, X509_STORE_CTX *ctx) /* {{{ */
{
    /* conjure the stream & context to use */
    cat_ssl_connection_t *connection = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
    cat_ssl_t *ssl = cat_ssl_get_from_connection(connection);
    int depth, allowed_depth;
    int err;
    int ret = preverify_ok;

    CAT_LOG_DEBUG(SSL, "SSL_cert_verify_callback(%p)", ssl);

    /* determine the status for the current cert */
    err = X509_STORE_CTX_get_error(ctx);
    depth = X509_STORE_CTX_get_error_depth(ctx);

    /* if allow_self_signed is set, make sure that verification succeeds */
    if (err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT && ssl->allow_self_signed) {
        CAT_LOG_DEBUG(SSL, "SSL connection use self-signed cert but we allowed");
        ret = 1;
    }

    /* check the depth */
    allowed_depth = SSL_get_verify_depth(connection);
    if (allowed_depth < 0) {
        allowed_depth = CAT_SSL_DEFAULT_STREAM_VERIFY_DEPTH;
    }
    if (depth > allowed_depth) {
        ret = 0;
        X509_STORE_CTX_set_error(ctx, X509_V_ERR_CERT_CHAIN_TOO_LONG);
    }

    CAT_LOG_DEBUG(SSL, "SSL allowed depth is %d, actual depth is %d, ret = %d", allowed_depth, depth, ret);

    return ret;
}

CAT_API void cat_ssl_context_enable_verify_peer(cat_ssl_context_t *context)
{
    CAT_LOG_DEBUG(SSL, "SSL_CTX_set_verify(%p, SSL_VERIFY_PEER, ssl_verify_callback)", context);
    SSL_CTX_set_verify(context->ctx, SSL_VERIFY_PEER, cat_ssl_verify_callback);
}

CAT_API void cat_ssl_context_disable_verify_peer(cat_ssl_context_t *context)
{
    CAT_LOG_DEBUG(SSL, "SSL_CTX_set_verify(%p, SSL_VERIFY_NONE, NULL)", context);
    SSL_CTX_set_verify(context->ctx, SSL_VERIFY_NONE, NULL);
}

CAT_API void cat_ssl_context_set_no_ticket(cat_ssl_context_t *context)
{
    CAT_LOG_DEBUG(SSL, "SSL_CTX_set_options(%p, SSL_OP_NO_TICKET)", context);
    SSL_CTX_set_options(context->ctx, SSL_OP_NO_TICKET);
}

CAT_API void cat_ssl_context_set_no_compression(cat_ssl_context_t *context)
{
    CAT_LOG_DEBUG(SSL, "SSL_CTX_set_options(%p, SSL_OP_NO_COMPRESSION)", context);
    SSL_CTX_set_options(context->ctx, SSL_OP_NO_COMPRESSION);
}

CAT_API cat_ssl_t *cat_ssl_create(cat_ssl_t *ssl, cat_ssl_context_t *context)
{
    cat_ssl_connection_t *connection;

    /* new connection */
    connection = SSL_new(context->ctx);
    if (unlikely(connection == NULL)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_new() failed");
        goto _new_failed;
    }

    /* malloc for SSL handle */
    if (ssl == NULL) {
        ssl = (cat_ssl_t *) cat_malloc(sizeof(*ssl));
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(ssl == NULL)) {
            cat_update_last_error_of_syscall("Malloc for SSL failed");
            goto _malloc_failed;
        }
#endif
        ssl->flags = CAT_SSL_FLAG_ALLOC;
    } else {
        ssl->flags = CAT_SSL_FLAG_NONE;
    }

    /* link ssl to connection */
    if (unlikely(SSL_set_ex_data(connection, cat_ssl_index, ssl) == 0)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_set_ex_data() failed");
        goto _set_ex_data_failed;
    }

    /* new BIO */
    do {
        cat_ssl_bio_t *ibio;
        if (unlikely(!BIO_new_bio_pair(&ibio, 0, &ssl->nbio, 0))) {
            cat_ssl_update_last_error(CAT_ESSL, "BIO_new_bio_pair() failed");
            goto _new_bio_pair_failed;
        }
        SSL_set_bio(connection, ibio, ibio);
    } while (0);

    /* new buffers */
    if (unlikely(!cat_buffer_make_pair(&ssl->read_buffer, CAT_SSL_BUFFER_SIZE, &ssl->write_buffer, CAT_SSL_BUFFER_SIZE))) {
        cat_update_last_error_with_previous("SSL create buffer failed");
        goto _alloc_buffer_failed;
    }

    /* init ssl fields */
    ssl->connection = connection;
    ssl->allow_self_signed = cat_false;

    return ssl;

    _alloc_buffer_failed:
    /* ibio will be free'd by SSL_free */
    BIO_free(ssl->nbio);
    _set_ex_data_failed:
    _new_bio_pair_failed:
    SSL_free(connection);
    ssl->connection = NULL;
#if CAT_ALLOC_HANDLE_ERRORS
    _malloc_failed:
#endif
    if (ssl->flags & CAT_SSL_FLAG_ALLOC) {
        cat_free(ssl);
    }
    _new_failed:
    return NULL;
}

CAT_API void cat_ssl_close(cat_ssl_t *ssl)
{
    cat_buffer_close(&ssl->write_buffer);
    cat_buffer_close(&ssl->read_buffer);
    /* ibio will be free'd by SSL_free */
    BIO_free(ssl->nbio);
    /* implicitly frees internal_bio */
    SSL_free(ssl->connection);
    /* free */
    if (ssl->flags & CAT_SSL_FLAG_ALLOC) {
        cat_free(ssl);
    } else {
        ssl->flags = CAT_SSL_FLAG_NONE;
    }
}

CAT_API void cat_ssl_set_accept_state(cat_ssl_t *ssl)
{
    cat_ssl_connection_t *connection = ssl->connection;

    if (unlikely(ssl->flags & CAT_SSL_FLAGS_STATE)) {
        if (ssl->flags & CAT_SSL_FLAG_CONNECT_STATE) {
            ssl->flags ^= CAT_SSL_FLAG_CONNECT_STATE;
        } else /* if (ssl->flags & CAT_SSL_FLAG_ACCEPT_STATE) */ {
            return;
        }
    }

    CAT_LOG_DEBUG(SSL, "SSL_set_accept_state(%p)", ssl);
    SSL_set_accept_state(connection); /* sets ssl to work in server mode. */
#ifdef SSL_OP_NO_RENEGOTIATION
    CAT_LOG_DEBUG(SSL, "SSL_set_options(%p, SSL_OP_NO_RENEGOTIATION)", ssl);
    SSL_set_options(connection, SSL_OP_NO_RENEGOTIATION);
#endif
    ssl->flags |= CAT_SSL_FLAG_ACCEPT_STATE;
}

CAT_API void cat_ssl_set_connect_state(cat_ssl_t *ssl)
{
    cat_ssl_connection_t *connection = ssl->connection;

    if (unlikely(ssl->flags & CAT_SSL_FLAGS_STATE)) {
        if (ssl->flags & CAT_SSL_FLAG_ACCEPT_STATE) {
#ifdef SSL_OP_NO_RENEGOTIATION
            SSL_clear_options(connection, SSL_OP_NO_RENEGOTIATION);
#endif
            ssl->flags ^= CAT_SSL_FLAG_ACCEPT_STATE;
        } else /* if (ssl->flags & CAT_SSL_FLAG_CONNECT_STATE) */ {
            return;
        }
    }

    CAT_LOG_DEBUG(SSL, "SSL_set_connect_state(%p)", ssl);
    SSL_set_connect_state(connection); /* sets ssl to work in client mode. */
    ssl->flags |= CAT_SSL_FLAG_CONNECT_STATE;
}

CAT_API cat_bool_t cat_ssl_set_sni_server_name(cat_ssl_t *ssl, const char *name)
{
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
    int error;

    CAT_LOG_DEBUG(SSL, "SSL_set_tlsext_host_name(%p, \"%s\")", ssl, name);

    error = SSL_set_tlsext_host_name(ssl->connection, name);

    if (unlikely(error != 1)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_set_tlsext_host_name(\"%s\") failed", name);
        return cat_false;
    }
#else
#warning "SSL library version is too low to support sni server name"
#endif

    return cat_true;
}

CAT_API cat_bool_t cat_ssl_is_established(const cat_ssl_t *ssl)
{
    return ssl->flags & CAT_SSL_FLAG_HANDSHAKED;
}

CAT_API cat_ssl_ret_t cat_ssl_handshake(cat_ssl_t *ssl)
{
    cat_ssl_connection_t *connection = ssl->connection;

    if (ssl->flags & CAT_SSL_FLAG_HANDSHAKED) {
        return CAT_SSL_RET_OK;
    }

    cat_ssl_clear_error();

    int n = SSL_do_handshake(connection);

    CAT_LOG_DEBUG(SSL, "SSL_do_handshake(%p): %d", ssl, n);
    if (n == 1) {
        ssl->flags |= CAT_SSL_FLAG_HANDSHAKED;
        cat_ssl_handshake_log(ssl);
#ifndef SSL_OP_NO_RENEGOTIATION
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#ifdef SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS
        /* initial handshake done, disable renegotiation (CVE-2009-3555) */
        if (connection->s3 && SSL_is_server((SSL *) connection)) {
            connection->s3->flags |= SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS;
        }
#endif
#endif
#endif
        CAT_LOG_DEBUG(SSL, "SSL handshake succeeded");
        return CAT_SSL_RET_OK;
    }

    int error = cat_ssl_get_error(ssl, n);

    CAT_SHOULD_BE(error != SSL_ERROR_WANT_WRITE &&
        "SSL handshake should never return SSL_ERROR_WANT_WRITE with BIO mode.");
    if (error == SSL_ERROR_WANT_READ) {
        CAT_LOG_DEBUG(SSL, "SSL_ERROR_WANT_READ");
        return CAT_SSL_RET_WANT_IO;
    } else if (error == SSL_ERROR_SYSCALL) {
        cat_update_last_error_of_syscall("SSL_do_handshake() failed");
    } else if (error == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0) {
        cat_update_last_error_with_reason(CAT_ECONNRESET, "SSL_do_handshake() failed");
    } else {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_do_handshake() failed");
    }

    return CAT_SSL_RET_ERROR;
}

CAT_API cat_bool_t cat_ssl_verify_peer(cat_ssl_t *ssl, cat_bool_t allow_self_signed)
{
    cat_ssl_connection_t *connection = ssl->connection;
    X509 *cert;
    long err;
    const char *errmsg;

    cert = SSL_get_peer_certificate(connection);

    if (cert == NULL) {
        cat_update_last_error(CAT_ENOCERT, "SSL certificate not found");
        return cat_false;
    }

    X509_free(cert);

    err = SSL_get_verify_result(connection);

    switch (err) {
        case X509_V_OK:
            return cat_true;
        case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
        case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        case X509_V_ERR_CERT_UNTRUSTED:
        case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
            if (allow_self_signed) {
                return cat_true;
            }
            errmsg = "self-signed certificate is not allowed";
            break;
        default:
            errmsg = "certificate verify error";
    }
    cat_update_last_error(CAT_ECERT, "SSL %s: (%ld, %s)", errmsg, err, X509_verify_cert_error_string(err));

    return cat_false;
}

#ifndef X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT
static cat_bool_t cat_ssl_check_name(const char *name, size_t name_length, ASN1_STRING *pattern)
{
    const unsigned char *s, *p, *end;
    size_t slen, plen;

    s = name;
    slen = name_length;
    p = ASN1_STRING_data(pattern);
    plen = ASN1_STRING_length(pattern);
    if (slen == plen && cat_strncasecmp(s, p, plen) == 0) {
        return cat_true;
    }
    if (plen > 2 && p[0] == '*' && p[1] == '.') {
        plen -= 1;
        p += 1;
        end = s + slen;
        s = cat_strlchr(s, end, '.');
        if (s == NULL) {
            return cat_false;
        }
        slen = end - s;
        if (plen == slen && cat_strncasecmp(s, p, plen) == 0) {
            return cat_true;
        }
    }

    return cat_false;
}
#endif

CAT_API cat_bool_t cat_ssl_check_host(cat_ssl_t *ssl, const char *name, size_t name_length)
{
    X509 *cert;
    cat_bool_t ret = cat_false;

    cert = SSL_get_peer_certificate(ssl->connection);

    if (cert == NULL) {
        return cat_false;
    }

#ifdef X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT
    /* X509_check_host() is only available in OpenSSL 1.0.2+ */
    if (name_length == 0) {
        goto _out;
    }
    if (X509_check_host(cert, (char *) name, name_length, 0, NULL) != 1) {
        CAT_LOG_DEBUG(SSL, "X509_check_host(): no match");
        goto _out;
    }
    CAT_LOG_DEBUG(SSL, "X509_check_host(): match");
    ret = cat_true;
    goto _out;
#else
    {
        int n, i;
        X509_NAME *sname;
        ASN1_STRING *str;
        X509_NAME_ENTRY *entry;
        GENERAL_NAME *altname;
        STACK_OF(GENERAL_NAME) *altnames;
        /*
         * As per RFC6125 and RFC2818, we check subjectAltName extension,
         * and if it's not present - commonName in Subject is checked.
         */
        altnames = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
        if (altnames) {
            n = sk_GENERAL_NAME_num(altnames);
            for (i = 0; i < n; i++) {
                altname = sk_GENERAL_NAME_value(altnames, i);
                if (altname->type != GEN_DNS) {
                    continue;
                }
                str = altname->d.dNSName;
                CAT_LOG_DEBUG(SSL, "Subject alt name: \"%*s\"", ASN1_STRING_length(str), ASN1_STRING_data(str));
                if (cat_ssl_check_name(name, name_length, str)) {
                    CAT_LOG_DEBUG(SSL, "Subject alt name: match");
                    GENERAL_NAMES_free(altnames);
                    ret = cat_true;
                    goto _out;
                }
            }
            CAT_LOG_DEBUG(SSL, "Subject alt name: no match");
            GENERAL_NAMES_free(altnames);
            goto _out;
        }
        /*
         * If there is no subjectAltName extension, check commonName
         * in Subject.  While RFC2818 requires to only check "most specific"
         * CN, both Apache and OpenSSL check all CNs, and so do we.
         */
        sname = X509_get_subject_name(cert);
        if (sname == NULL) {
            goto _out;
        }
        i = -1;
        while (1) {
            i = X509_NAME_get_index_by_NID(sname, NID_commonName, i);
            if (i < 0) {
                break;
            }
            entry = X509_NAME_get_entry(sname, i);
            str = X509_NAME_ENTRY_get_data(entry);
            CAT_LOG_DEBUG(SSL, "Common name: \"%*s\"", ASN1_STRING_length(str), ASN1_STRING_data(str));
            if (cat_ssl_check_name(name, name_length, str)) {
                CAT_LOG_DEBUG(SSL, "Common name: match");
                ret = cat_true;
                goto _out;
            }
        }
        CAT_LOG_DEBUG(SSL, "Common name: no match");
    }
#endif

    _out:
    X509_free(cert);
    return ret;
}

CAT_API int cat_ssl_read_encrypted_bytes(cat_ssl_t *ssl, char *buffer, size_t size)
{
    int n;

    cat_ssl_clear_error();

    n = BIO_read(ssl->nbio, buffer, (int) size);
    CAT_LOG_DEBUG(SSL, "BIO_read(%p, %zu) = %d", ssl, size, n);
    if (unlikely(n <= 0)) {
        if (unlikely(!BIO_should_retry(ssl->nbio))) {
            cat_ssl_update_last_error(CAT_ESSL, "BIO_read(%zu) failed", size);
            cat_ssl_unrecoverable_error(ssl);
            return CAT_RET_ERROR;
        }
        CAT_LOG_DEBUG(SSL, "BIO_read(%p) should retry", ssl);
        return CAT_RET_NONE;
    }

    return n;
}

CAT_API int cat_ssl_write_encrypted_bytes(cat_ssl_t *ssl, const char *buffer, size_t length)
{
    int n;

    cat_ssl_clear_error();

    n = BIO_write(ssl->nbio, buffer, (int) length);
    CAT_LOG_DEBUG(SSL, "BIO_write(%p, %zu) = %d", ssl, length, n);
    if (unlikely(n <= 0)) {
        if (unlikely(!BIO_should_retry(ssl->nbio))) {
            cat_ssl_update_last_error(CAT_ESSL, "BIO_write(%zu) failed", length);
            cat_ssl_unrecoverable_error(ssl);
            return CAT_RET_ERROR;
        }
        CAT_LOG_DEBUG(SSL, "BIO_write(%p) should retry", ssl);
        return CAT_RET_NONE;
    }

    return n;
}

CAT_API size_t cat_ssl_encrypted_size(size_t length)
{
    return CAT_MEMORY_ALIGNED_SIZE_EX(length, CAT_SSL_MAX_BLOCK_LENGTH) + (CAT_SSL_BUFFER_SIZE - CAT_SSL_MAX_PLAIN_LENGTH);
}

static cat_bool_t cat_ssl_encrypt_buffered(cat_ssl_t *ssl, const char *in, size_t *in_length, char *out, size_t *out_length)
{
    size_t nread = 0, nwrite = 0;
    size_t in_size = *in_length;
    size_t out_size = *out_length;
    cat_bool_t ret = cat_false;

    *in_length = 0;
    *out_length = 0;

    if (unlikely(in_size == 0)) {
        return cat_false;
    }

    while (1) {
        int n;

        if (unlikely(nread == out_size)) {
            cat_update_last_error(CAT_ENOBUFS, "SSL_encrypt() no out buffer space available");
            break;
        }

        n = cat_ssl_read_encrypted_bytes(ssl, out + nread, out_size - nread);

        if (n > 0) {
            nread += n;
        } else if (n == CAT_RET_NONE) {
            // continue to SSL_write()
        } else {
            cat_update_last_error_with_previous("SSL_write() error");
            break;
        }

        if (nwrite == in_size) {
            /* done */
            ret = cat_true;
            break;
        }

        cat_ssl_clear_error();

        n = SSL_write(ssl->connection, in + nwrite, (int) (in_size - nwrite));

        CAT_LOG_DEBUG_SCOPE_START_EX(SSL, char *tmp) {
            CAT_LOG_DEBUG_D(SSL, "SSL_write(%p, %s, %zu) = %d",
                ssl, cat_log_buffer_quote(in + nwrite, n, &tmp), in_size - nwrite, n);
        } CAT_LOG_DEBUG_SCOPE_END_EX(cat_free(tmp));

        if (unlikely(n <= 0)) {
            int error = cat_ssl_get_error(ssl, n);

            if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
                CAT_LOG_DEBUG(SSL, "SSL_write(%p) want %s", ssl, error == SSL_ERROR_WANT_READ ? "read" : "write");
                // continue to  SSL_read_encrypted_bytes()
            } else if (error == SSL_ERROR_SYSCALL) {
                cat_update_last_error_of_syscall("SSL_write() error");
                break;
            } else {
                if (error != SSL_ERROR_ZERO_RETURN) {
                    cat_ssl_update_last_error(CAT_ESSL, "SSL_write() error");
                } else {
                    /* TODO: try to confirm: is that possible? */
                    cat_update_last_error_with_reason(CAT_ECONNRESET, "SSL_write() error");
                }
                cat_ssl_unrecoverable_error(ssl);
                break;
            }
        } else {
            nwrite += n;
        }
    }

    *in_length = nwrite;
    *out_length = nread;

    return ret;
}

CAT_API cat_bool_t cat_ssl_encrypt(
    cat_ssl_t *ssl,
    const cat_io_vector_t *vin, unsigned int vin_count,
    cat_io_vector_t *vout, unsigned int *vout_count
)
{
    const cat_io_vector_t *v = vin, *ve = v + vin_count;
    size_t vin_length = cat_io_vector_length(vin, vin_count);
    unsigned int vout_counted = 0, vout_size = *vout_count;
    char *buffer;
    size_t length = 0;
    size_t size;

    CAT_ASSERT(vout_size > 0);

    *vout_count = 0;

    size = cat_ssl_encrypted_size(vin_length);
    if (unlikely(size >  ssl->write_buffer.size)) {
        buffer = (char *) cat_malloc(size);
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(buffer == NULL)) {
            cat_update_last_error_of_syscall("Malloc for SSL write buffer failed");
            return cat_false;
        }
#endif
    } else {
        buffer = ssl->write_buffer.value;
        size = ssl->write_buffer.size;
    }

    while (1) {
        size_t in_length = v->length;
        size_t out_length = size - length;
        cat_bool_t ret = cat_ssl_encrypt_buffered(
            ssl, v->base, &in_length, buffer + length, &out_length
        );
        length += out_length;
        if (unlikely(!ret)) {
            /* save current */
            vout->base = buffer;
            vout->length = (cat_io_vector_length_t) length;
            vout_counted++;
            if (cat_get_last_error_code() == CAT_ENOBUFS) {
                CAT_ASSERT(length == size);
                CAT_LOG_DEBUG(SSL, "SSL encrypt buffer extend");
                if (vout_counted == vout_size) {
                    cat_update_last_error(CAT_ENOBUFS, "Unexpected vector count (too many)");
                    goto _unrecoverable_error;
                }
                buffer = (char *) cat_malloc(size = CAT_SSL_BUFFER_SIZE);
#if CAT_ALLOC_HANDLE_ERRORS
                if (unlikely(buffer == NULL)) {
                    cat_update_last_error_of_syscall("Realloc for SSL write buffer failed");
                    goto _unrecoverable_error;
                }
#endif
                /* switch to the next */
                vout++;
                continue;
            }
            goto _error;
        }
        if (++v == ve) {
            break;
        }
    }

    vout->base = buffer;
    vout->length = (cat_io_vector_length_t) length;
    *vout_count = vout_counted + 1;

    return cat_true;

    _unrecoverable_error:
    cat_ssl_unrecoverable_error(ssl);
    _error:
    cat_ssl_encrypted_vector_free(ssl, vout, vout_counted);
    return cat_false;
}

CAT_API void cat_ssl_encrypted_vector_free(cat_ssl_t *ssl, cat_io_vector_t *vector, unsigned int vector_count)
{
    while (vector_count > 0) {
        if (vector->base != ssl->write_buffer.value) {
            cat_free(vector->base);
        }
        vector++;
        vector_count--;
    }
}

CAT_API cat_bool_t cat_ssl_decrypt(cat_ssl_t *ssl, char *out, size_t *out_length, cat_bool_t *eof)
{
    cat_buffer_t *buffer = &ssl->read_buffer;
    size_t nread = 0, nwrite = 0;
    size_t out_size = *out_length;
    cat_bool_t have_encrypted_data = buffer->length != 0;
    cat_bool_t ret = cat_false;

    *out_length = 0;
    *eof = cat_false;

    while (1) {
        int n;

        if (have_encrypted_data) {
            n = cat_ssl_write_encrypted_bytes(
                ssl, buffer->value + nwrite, buffer->length - nwrite
            );
            if (n > 0) {
                nwrite += n;
                have_encrypted_data = buffer->length - nwrite > 0;
            } else if (n == CAT_RET_NONE) {
                // continue to SSL_read()
            } else {
                cat_update_last_error_with_previous("SSL_write() error");
                break;
            }
        }

        cat_ssl_clear_error();

        n = SSL_read(ssl->connection, out + nread, (int) (out_size - nread));

        CAT_LOG_DEBUG_SCOPE_START_EX(SSL, char *tmp) {
            CAT_LOG_DEBUG_D(SSL, "SSL_read(%p, %s, %zu) = %d",
                ssl, cat_log_buffer_quote(out + nread, n, &tmp), out_size - nread, n);
        } CAT_LOG_DEBUG_SCOPE_END_EX(cat_free(tmp));

        if (unlikely(n <= 0)) {
            int error = cat_ssl_get_error(ssl, n);

            if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
                CAT_LOG_DEBUG(SSL, "SSL_read(%p) want %s", ssl, error == SSL_ERROR_WANT_READ ? "read" : "write");
                // continue to SSL_write_encrypted_bytes()
            } else if (error == SSL_ERROR_ZERO_RETURN) {
                // Connection closed normally
                CAT_LOG_DEBUG(SSL, "SSL(%p) connection closed by peer", ssl);
                *eof = cat_true;
                ret = cat_true;
                break;
            } else if (error == SSL_ERROR_SYSCALL) {
                cat_update_last_error_of_syscall("SSL_read() error");
                break;
            } else {
                cat_ssl_update_last_error(CAT_ESSL, "SSL_read() error");
                cat_ssl_unrecoverable_error(ssl);
                break;
            }
        } else {
            nread += n;
            if (nread == out_size) {
                /* out buffer is full */
                ret = cat_true;
                break;
            }
        }

        if (!have_encrypted_data) {
            /* need more encrypted data */
            ret = cat_true;
            break;
        }
    }

    cat_buffer_truncate_from(buffer, nwrite, SIZE_MAX);

    *out_length = nread;

    return ret;
}

CAT_API cat_ssl_shutdown_masks_t cat_ssl_get_shutdown(const cat_ssl_t *ssl)
{
    return SSL_get_shutdown(ssl->connection);
}

CAT_API void cat_ssl_set_shutdown(cat_ssl_t *ssl, cat_ssl_shutdown_masks_t mode)
{
    CAT_ASSERT((mode &~ (CAT_SSL_SENT_SHUTDOWN | CAT_SSL_RECEIVED_SHUTDOWN)) == 0);
    SSL_set_shutdown(ssl->connection, mode);
}

CAT_API void cat_ssl_set_quiet_shutdown(cat_ssl_t *ssl, cat_bool_t enable)
{
    SSL_set_quiet_shutdown(ssl->connection, enable);
}

#if 0 /* Enable it when we implement it in socket */
CAT_API cat_ssl_ret_t cat_ssl_shutdown(cat_ssl_t *ssl)
{
    int error, ret;

    if (!cat_ssl_is_established(ssl)) {
        return CAT_SSL_RET_OK;
    }

    ret = SSL_shutdown(ssl->connection);
    CAT_LOG_DEBUG(SSL, "SSL_shutdown(%p) = %d", ret);
    if (ret == 1) {
        return CAT_SSL_RET_OK;
    } else if (ret == 0) {
        return CAT_SSL_RET_NONE;
    }

    error = cat_ssl_get_error(ssl, n);
    if (error == SSL_ERROR_WANT_READ) {
        CAT_LOG_DEBUG(SSL, "SSL_shutdown(%p) want read", ssl);
        return CAT_SSL_RET_WANT_READ;
    } else if (error == SSL_ERROR_WANT_WRITE) {
        CAT_LOG_DEBUG(SSL, "SSL_shutdown(%p) want write", ssl);
        return CAT_SSL_RET_WANT_WRITE;
    } else if (error == SSL_ERROR_SYSCALL) {
        cat_update_last_error_of_syscall("SSL_shutdown() failed");
    } else if (ssl_error == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0) {
        cat_update_last_error_with_reason(CAT_ECONNRESET, "SSL_shutdown() failed");
    } else {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_shutdown() error");
    }
    return CAT_RET_ERROR;
}
#endif

#ifdef CAT_ENABLE_DEBUG_LOG
static const char *cat_ssl_error_to_str(int error)
{
    switch (error)
    {
    case SSL_ERROR_NONE:
        return "SSL_ERROR_NONE";
    case SSL_ERROR_SSL:
        return "SSL_ERROR_SSL";
    case SSL_ERROR_WANT_READ:
        return "SSL_ERROR_WANT_READ";
    case SSL_ERROR_WANT_WRITE:
        return "SSL_ERROR_WANT_WRITE";
    case SSL_ERROR_WANT_X509_LOOKUP:
        return "SSL_ERROR_WANT_X509_LOOKUP";
    case SSL_ERROR_SYSCALL:
        return "SSL_ERROR_SYSCALL";
    case SSL_ERROR_ZERO_RETURN:
        return "SSL_ERROR_ZERO_RETURN";
    case SSL_ERROR_WANT_CONNECT:
        return "SSL_ERROR_WANT_CONNECT";
    case SSL_ERROR_WANT_ACCEPT:
        return "SSL_ERROR_WANT_ACCEPT";
#if defined(SSL_ERROR_WANT_ASYNC)
    case SSL_ERROR_WANT_ASYNC:
        return "SSL_ERROR_WANT_ASYNC";
#endif
#if defined(SSL_ERROR_WANT_ASYNC_JOB)
    case SSL_ERROR_WANT_ASYNC_JOB:
        return "SSL_ERROR_WANT_ASYNC_JOB";
#endif
#if defined(SSL_ERROR_WANT_EARLY)
    case SSL_ERROR_WANT_EARLY:
        return "SSL_ERROR_WANT_EARLY";
#endif
#if defined(SSL_ERROR_WANT_CLIENT_HELLO_CB)
    case SSL_ERROR_WANT_CLIENT_HELLO_CB:
        return "SSL_ERROR_WANT_CLIENT_HELLO_CB";
#endif
    default:
        return "SSL_ERROR unknown";
    }
}
#endif

static int cat_ssl_get_error(const cat_ssl_t *ssl, int ret_code)
{
    int error = SSL_get_error(ssl->connection, ret_code);
    CAT_LOG_DEBUG(SSL, "SSL_get_error(%p, %d) = %d [%s]", ssl, ret_code, error, cat_ssl_error_to_str(error));
    return error;
}

CAT_API CAT_COLD char *cat_ssl_get_error_reason(void)
{
    char *errstr = NULL, *errstr2;
    const char *data;
    int flags;

    if (ERR_peek_error()) {
        while (1) {
            unsigned long n = ERR_peek_error_line_data(NULL, NULL, &data, &flags);
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
                    ERR_print_errors_fp(CAT_G(error_log));
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

CAT_API CAT_COLD void cat_ssl_update_last_error(cat_errno_t error, const char *format, ...)
{
    va_list args;
    char *message, *reason;

    va_start(args, format);
    message = cat_vsprintf(format, args);
    va_end(args);

    if (unlikely(message == NULL)) {
        ERR_print_errors_fp(CAT_G(error_log));
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
        CAT_NOTICE(SSL, "Ignoring stale global SSL error %s", reason != NULL ? reason : "");
        if (reason != NULL) {
            cat_free(reason);
        }
    }
    ERR_clear_error();
}

CAT_API CAT_COLD cat_bool_t cat_ssl_is_down(const cat_ssl_t *ssl)
{
    return !!(ssl->flags & CAT_SSL_FLAG_UNRECOVERABLE_ERROR);
}

CAT_API CAT_COLD void cat_ssl_unrecoverable_error(cat_ssl_t *ssl)
{
    cat_ssl_set_shutdown(ssl, CAT_SSL_SENT_SHUTDOWN | CAT_SSL_RECEIVED_SHUTDOWN);
}

static void cat_ssl_info_callback(const cat_ssl_connection_t *connection, int where, int ret)
{
    (void) ret;

#ifndef SSL_OP_NO_RENEGOTIATION
    if ((where & SSL_CB_HANDSHAKE_START) && SSL_is_server((SSL *) connection)) {
        cat_ssl_t *ssl = cat_ssl_get_from_connection(connection);
        if (ssl->flags & CAT_SSL_FLAG_HANDSHAKED) {
            ssl->flags |= CAT_SSL_FLAG_RENEGOTIATION;
            CAT_LOG_DEBUG(SSL, "SSL#(%p) renegotiation", ssl);
        }
    }
#endif

    if ((where & SSL_CB_ACCEPT_LOOP) == SSL_CB_ACCEPT_LOOP) {
        cat_ssl_t *ssl = cat_ssl_get_from_connection(connection);
        if (!(ssl->flags & CAT_SSL_FLAG_HANDSHAKE_BUFFER_SET)) {
            /*
             * By default OpenSSL uses 4k buffer during a handshake,
             * which is too low for long certificate chains and might
             * result in extra round-trips.
             *
             * To adjust a buffer size we detect that buffering was added
             * to write side of the connection by comparing rbio and wbio.
             * If they are different, we assume that it's due to buffering
             * added to wbio, and set buffer size.
             */
            BIO *rbio, *wbio;

            rbio = SSL_get_rbio(connection);
            wbio = SSL_get_wbio(connection);

            if (rbio != wbio) {
                (void) BIO_set_write_buffer_size(wbio, CAT_SSL_BUFFER_SIZE);
                ssl->flags |= CAT_SSL_FLAG_HANDSHAKE_BUFFER_SET;
            }
        }
    }
}

#ifdef CAT_DEBUG
static void cat_ssl_handshake_log(cat_ssl_t *ssl)
{
    const char *cipher_str = NULL;
    char buf[129], *s, *d;
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
    const
#endif
    SSL_CIPHER *cipher = SSL_get_current_cipher(ssl->connection);
    if (cipher != NULL) {
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
        cipher_str = &buf[1];
    }
    CAT_LOG_DEBUG(SSL, "SSL(%p) version: %s, cipher: " CAT_LOG_STRING_OR_NULL_FMT ", reused: %u",
        ssl, SSL_get_version(ssl->connection), CAT_LOG_STRING_OR_NULL_PARAM(cipher_str), !!SSL_session_reused(ssl->connection));
}
#endif

#endif /* CAT_SSL */
