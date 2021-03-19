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

#ifdef CAT_OS_WIN
#include <Wincrypt.h>
/* These are from Wincrypt.h, they conflict with OpenSSL */
#undef X509_NAME
#undef X509_CERT_PAIR
#undef X509_EXTENSIONS
#endif

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

CAT_API cat_bool_t cat_ssl_module_init(void)
{
    /* SSL library initialisation */
#if OPENSSL_VERSION_NUMBER >= 0x10100003L
    if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG | OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_ADD_ALL_CIPHERS, NULL) == 0) {
        ERR_print_errors_fp(stderr);
        cat_core_error(SSL, "OPENSSL_init_ssl() failed");
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
        ERR_print_errors_fp(stderr);
        cat_core_error(SSL, "SSL_get_ex_new_index() failed");
    }
    cat_ssl_context_index = SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    if (cat_ssl_context_index == -1) {
        ERR_print_errors_fp(stderr);
        cat_core_error(SSL, "SSL_CTX_get_ex_new_index() failed");
    }

    return cat_true;
}

CAT_API cat_ssl_context_t *cat_ssl_context_create(cat_ssl_method_t method)
{
    cat_ssl_context_t *context;
    const SSL_METHOD *ssl_method;

    if (method & CAT_SSL_METHOD_TLS) {
        ssl_method = SSLv23_method();
    } else if (method & CAT_SSL_METHOD_DTLS) {
        ssl_method = DTLS_method();
    } else {
        CAT_NEVER_HERE("Unknown method");
    }

    context = SSL_CTX_new(ssl_method);

    if (unlikely(context == NULL)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_CTX_new() failed");
        return NULL;
    }

    /* client side options */
#ifdef SSL_OP_MICROSOFT_SESS_ID_BUG
    SSL_CTX_set_options(context, SSL_OP_MICROSOFT_SESS_ID_BUG);
#endif
#ifdef SSL_OP_NETSCAPE_CHALLENGE_BUG
    SSL_CTX_set_options(context, SSL_OP_NETSCAPE_CHALLENGE_BUG);
#endif
    /* server side options */
#ifdef SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG
    SSL_CTX_set_options(context, SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG);
#endif
#ifdef SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER
    SSL_CTX_set_options(context, SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER);
#endif
#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
    /* this option allow a potential SSL 2.0 rollback (CAN-2005-2969) */
    SSL_CTX_set_options(context, SSL_OP_MSIE_SSLV2_RSA_PADDING);
#endif
#ifdef SSL_OP_SSLEAY_080_CLIENT_DH_BUG
    SSL_CTX_set_options(context, SSL_OP_SSLEAY_080_CLIENT_DH_BUG);
#endif
#ifdef SSL_OP_TLS_D5_BUG
    SSL_CTX_set_options(context, SSL_OP_TLS_D5_BUG);
#endif
#ifdef SSL_OP_TLS_BLOCK_PADDING_BUG
    SSL_CTX_set_options(context, SSL_OP_TLS_BLOCK_PADDING_BUG);
#endif
#ifdef SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
    SSL_CTX_set_options(context, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
#endif
    SSL_CTX_set_options(context, SSL_OP_SINGLE_DH_USE);
#if OPENSSL_VERSION_NUMBER >= 0x009080dfL
    /* only in 0.9.8m+ */
    SSL_CTX_clear_options(context, SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3|SSL_OP_NO_TLSv1);
#endif
#ifdef SSL_CTX_set_min_proto_version
    SSL_CTX_set_min_proto_version(context, 0);
    SSL_CTX_set_max_proto_version(context, TLS1_2_VERSION);
#endif
#ifdef TLS1_3_VERSION
    SSL_CTX_set_min_proto_version(context, 0);
    SSL_CTX_set_max_proto_version(context, TLS1_3_VERSION);
#endif
#ifdef SSL_OP_NO_COMPRESSION
    SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION);
#endif
#ifdef SSL_OP_NO_ANTI_REPLAY
    SSL_CTX_set_options(context, SSL_OP_NO_ANTI_REPLAY);
#endif
#ifdef SSL_OP_NO_CLIENT_RENEGOTIATION
    SSL_CTX_set_options(ssl->ctx, SSL_OP_NO_CLIENT_RENEGOTIATION);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(context, SSL_MODE_RELEASE_BUFFERS);
#endif
#ifdef SSL_MODE_NO_AUTO_CHAIN
    SSL_CTX_set_mode(context, SSL_MODE_NO_AUTO_CHAIN);
#endif
    SSL_CTX_set_read_ahead(context, 1);
    SSL_CTX_set_info_callback(context, cat_ssl_info_callback);

    return context;
}

CAT_API void cat_ssl_context_close(cat_ssl_context_t *context)
{
    SSL_CTX_free(context);
}

CAT_API void cat_ssl_context_set_protocols(cat_ssl_context_t *context, cat_ssl_protocols_t protocols)
{
    if (!(protocols & CAT_SSL_PROTOCOL_SSLv2)) {
        SSL_CTX_set_options(context, SSL_OP_NO_SSLv2);
    }
    if (!(protocols & CAT_SSL_PROTOCOL_SSLv3)) {
        SSL_CTX_set_options(context, SSL_OP_NO_SSLv3);
    }
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1)) {
        SSL_CTX_set_options(context, SSL_OP_NO_TLSv1);
    }
#ifdef SSL_OP_NO_TLSv1_1
    SSL_CTX_clear_options(context, SSL_OP_NO_TLSv1_1);
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1_1)) {
        SSL_CTX_set_options(context, SSL_OP_NO_TLSv1_1);
    }
#endif
#ifdef SSL_OP_NO_TLSv1_2
    SSL_CTX_clear_options(context, SSL_OP_NO_TLSv1_2);
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1_2)) {
        SSL_CTX_set_options(context, SSL_OP_NO_TLSv1_2);
    }
#endif
#ifdef SSL_OP_NO_TLSv1_3
    SSL_CTX_clear_options(context, SSL_OP_NO_TLSv1_3);
    if (!(protocols & CAT_SSL_PROTOCOL_TLSv1_3)) {
        SSL_CTX_set_options(context, SSL_OP_NO_TLSv1_3);
    }
#endif
}

CAT_API cat_bool_t cat_ssl_context_set_default_verify_paths(cat_ssl_context_t *context)
{
    cat_debug(SSL, "SSL_CTX_set_default_verify_paths()");
    if (SSL_CTX_set_default_verify_paths(context) != 1) {
        cat_ssl_update_last_error(CAT_EINVAL, "SSL_CTX_set_default_verify_paths() failed");
        return cat_false;
    }
    return cat_true;
}

CAT_API cat_bool_t cat_ssl_context_set_ca_file(cat_ssl_context_t *context, const char *ca_file)
{
    if (unlikely(ca_file == NULL)) {
        cat_update_last_error(CAT_EINVAL, "SSL ca file can not be empty ");
        return cat_false;
    }
    cat_debug(SSL, "SSL_CTX_load_verify_locations(ca_file: \"%s\")", ca_file);
    if (SSL_CTX_load_verify_locations(context, ca_file, NULL) != 1) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_CTX_load_verify_locations(ca_file) failed");
        return cat_false;
    }
    return cat_true;
}

CAT_API cat_bool_t cat_ssl_context_set_ca_path(cat_ssl_context_t *context, const char *ca_path)
{
    if (unlikely(ca_path == NULL)) {
        cat_update_last_error(CAT_EINVAL, "SSL ca file can not be empty ");
        return cat_false;
    }
    cat_debug(SSL, "SSL_CTX_load_verify_locations(ca_path: \"%s\")", ca_path);
    if (SSL_CTX_load_verify_locations(context, NULL, ca_path) != 1) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_CTX_load_verify_locations(ca_path) failed");
        return cat_false;
    }
    return cat_true;
}

CAT_API void cat_ssl_context_set_verify_depth(cat_ssl_context_t *context, int depth)
{
    cat_debug(SSL, "SSL_CTX_set_verify_depth(%d)", depth);
    SSL_CTX_set_verify_depth(context, depth);
}

#ifdef CAT_OS_WIN
static cat_bool_t cat_ssl_win_cert_verify_callback(X509_STORE_CTX *x509_store_ctx, void *data) /* {{{ */
{
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

	{ /* First convert the x509 struct back to a DER encoded buffer and let Windows decode it into a form it can work with */
		unsigned char *der_buf = NULL;
		int der_len;

		der_len = i2d_X509(cert, &der_buf);
		if (der_len < 0) {
			unsigned long err_code, e;
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
            allowed_depth = OPENSSL_DEFAULT_STREAM_VERIFY_DEPTH;
        }
		for (i = 0; i < cert_chain_ctx->cChain; i++) {
			if ((int) cert_chain_ctx->rgpChain[i]->cElement > allowed_depth) {
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
		LPWSTR server_name = NULL;
		BOOL verify_result;

		{ /* This looks ridiculous and it is - but we validate the name ourselves using the peer_name
		     ctx option, so just use the CN from the cert here */

			X509_NAME *cert_name;
			unsigned char *cert_name_utf8;
			int index, cert_name_utf8_len;
			DWORD num_wchars;

			cert_name = X509_get_subject_name(cert);
			index = X509_NAME_get_index_by_NID(cert_name, NID_commonName, -1);
			if (index < 0) {
				cat_update_last_error(CAT_ECERT, "Unable to locate certificate CN");
				CertFreeCertificateChain(cert_chain_ctx);
				CertFreeCertificateContext(cert_ctx);
                X509_STORE_CTX_set_error(x509_store_ctx, SSL_R_CERTIFICATE_VERIFY_FAILED);
                return cat_false;
			}

			cert_name_utf8_len = ASN1_STRING_to_UTF8(&cert_name_utf8, X509_NAME_ENTRY_get_data(X509_NAME_get_entry(cert_name, index)));

			num_wchars = MultiByteToWideChar(CP_UTF8, 0, (char*)cert_name_utf8, -1, NULL, 0);
			if (num_wchars == 0) {
				cat_update_last_error(CAT_ECERT, "Unable to convert %s to wide character string", cert_name_utf8);
				OPENSSL_free(cert_name_utf8);
				CertFreeCertificateChain(cert_chain_ctx);
				CertFreeCertificateContext(cert_ctx);
                X509_STORE_CTX_set_error(x509_store_ctx, SSL_R_CERTIFICATE_VERIFY_FAILED);
                return cat_false;
			}

			server_name = (LPWSTR) cat_malloc((num_wchars * sizeof(WCHAR)) + sizeof(WCHAR));

			num_wchars = MultiByteToWideChar(CP_UTF8, 0, (char*)cert_name_utf8, -1, server_name, num_wchars);
			if (num_wchars == 0) {
				cat_update_last_error(CAT_ECERT, "Unable to convert %s to wide character string", cert_name_utf8);
				cat_free(server_name);
				OPENSSL_free(cert_name_utf8);
				CertFreeCertificateChain(cert_chain_ctx);
				CertFreeCertificateContext(cert_ctx);
                X509_STORE_CTX_set_error(x509_store_ctx, SSL_R_CERTIFICATE_VERIFY_FAILED);
                return cat_false;
			}

			OPENSSL_free(cert_name_utf8);
		}

		ssl_policy_params.dwAuthType = (ssl->flags & CAT_SSL_FLAG_ACCEPT_STATE) ? AUTHTYPE_SERVER : AUTHTYPE_CLIENT;
		ssl_policy_params.pwszServerName = server_name;
		chain_policy_params.pvExtraPolicyPara = &ssl_policy_params;

		verify_result = CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, cert_chain_ctx, &chain_policy_params, &chain_policy_status);

		cat_free(server_name);
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
				X509_STORE_CTX_set_error(x509_store_ctx, X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT);
			} else {
                X509_STORE_CTX_set_error(x509_store_ctx, SSL_R_CERTIFICATE_VERIFY_FAILED);
                return cat_false;
			}
		}
	}

	return 1;
}

static int cat_ssl_verify_callback(int preverify_ok, X509_STORE_CTX *ctx) /* {{{ */
{
	/* conjure the stream & context to use */
	cat_ssl_connection_t *connection = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	cat_ssl_t *ssl = cat_ssl_get_from_connection(connection);
	int err, depth, ret, allowed_depth;

	ret = preverify_ok;

	/* determine the status for the current cert */
	err = X509_STORE_CTX_get_error(ctx);
	depth = X509_STORE_CTX_get_error_depth(ctx);

	/* if allow_self_signed is set, make sure that verification succeeds */
	if (err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT && ssl->allow_self_signed) {
		ret = 1;
	}

	/* check the depth */
    allowed_depth = SSL_get_verify_depth(connection);
    if (allowed_depth < 0) {
        allowed_depth = OPENSSL_DEFAULT_STREAM_VERIFY_DEPTH;
    }
	if (depth > allowed_depth) {
		ret = 0;
		X509_STORE_CTX_set_error(ctx, X509_V_ERR_CERT_CHAIN_TOO_LONG);
	}

	return ret;
}

CAT_API void cat_ssl_context_configure_verify(cat_ssl_context_t *context)
{
    cat_debug(SSL, "SSL_CTX_set_cert_verify_callback()");
    SSL_CTX_set_cert_verify_callback(context, cat_ssl_win_cert_verify_callback, NULL);
    cat_debug(SSL, "SSL_CTX_set_verify(SSL_VERIFY_PEER, NULL)");
    SSL_CTX_set_verify(context, SSL_VERIFY_PEER, NULL);
}
#endif

CAT_API cat_ssl_t *cat_ssl_create(cat_ssl_t *ssl, cat_ssl_context_t *context)
{
    if (ssl == NULL) {
        ssl = (cat_ssl_t *) cat_malloc(sizeof(*ssl));
        if (unlikely(ssl == NULL)) {
            cat_update_last_error_of_syscall("Malloc for SSL failed");
            goto _malloc_failed;
        }
        ssl->flags = CAT_SSL_FLAG_ALLOC;
    } else {
        ssl->flags = CAT_SSL_FLAG_NONE;
    }

    /* new connection */
    ssl->context = context;
    ssl->connection = SSL_new(context);
    if (unlikely(ssl->connection == NULL)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_new() failed");
        goto _new_failed;
    }
    if (unlikely(SSL_set_ex_data(ssl->connection, cat_ssl_index, ssl) == 0)) {
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
        SSL_set_bio(ssl->connection, ibio, ibio);
    } while (0);

    /* new buffers */
    if (unlikely(!cat_buffer_make_pair(&ssl->read_buffer, CAT_SSL_BUFFER_SIZE, &ssl->write_buffer, CAT_SSL_BUFFER_SIZE))) {
        cat_update_last_error_with_previous("SSL create buffer failed");
        goto _alloc_buffer_failed;
    }

    cat_string_init(&ssl->passphrase);
    ssl->allow_self_signed = cat_false;

    return ssl;

    _alloc_buffer_failed:
    /* ibio will be free'd by SSL_free */
    BIO_free(ssl->nbio);
    _set_ex_data_failed:
    _new_bio_pair_failed:
    SSL_free(ssl->connection);
    ssl->connection = NULL;
    _new_failed:
    if (ssl->flags & CAT_SSL_FLAG_ALLOC) {
        cat_free(ssl);
    }
    _malloc_failed:
    return NULL;
}

CAT_API void cat_ssl_close(cat_ssl_t *ssl)
{
    cat_string_close(&ssl->passphrase);
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

CAT_API cat_bool_t cat_ssl_shutdown(cat_ssl_t *ssl)
{
    // FIXME: BIO shutdown
    // if (ssl->flags & CAT_SSL_FLAG_HANDSHAKED) {
        // SSL_shutdown(ssl->connection);
    // }

    return cat_false;
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

    cat_debug(SSL, "SSL_set_accept_state()");
    SSL_set_accept_state(connection); /* sets ssl to work in server mode. */
#ifdef SSL_OP_NO_RENEGOTIATION
    cat_debug(SSL, "SSL_set_options(SSL_OP_NO_RENEGOTIATION)");
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

    cat_debug(SSL, "SSL_set_connect_state()");
    SSL_set_connect_state(connection); /* sets ssl to work in client mode. */
    ssl->flags |= CAT_SSL_FLAG_CONNECT_STATE;
}

CAT_API cat_bool_t cat_ssl_set_sni_server_name(cat_ssl_t *ssl, const char *name)
{
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
    cat_ssl_connection_t *connection = ssl->connection;
    int error;

    cat_debug(SSL, "SSL_set_tlsext_host_name(\"%s\")", name);

    error = SSL_set_tlsext_host_name(connection, name);

    if (unlikely(error != 1)) {
        cat_ssl_update_last_error(CAT_ESSL, "SSL_set_tlsext_host_name(\"%s\") failed", name);
        return cat_false;
    }
#else
#warning "SSL library version is too low to support sni server name"
#endif

    return cat_true;
}

static int cat_ssl_password_callback(char *buf, int length, int verify, void *data)
{
    cat_ssl_t *ssl = (cat_ssl_t *) data;

    if (ssl->passphrase.length < (size_t) length - 1) {
        memcpy(buf, ssl->passphrase.value, ssl->passphrase.length);
        return (int) ssl->passphrase.length;
    }

    return 0;
}

CAT_API cat_bool_t cat_ssl_set_passphrase(cat_ssl_t *ssl, const char *passphrase, size_t passphrase_length)
{
    cat_ssl_connection_t *connection = ssl->connection;
    cat_bool_t ret;

    cat_string_close(&ssl->passphrase);
    ret = cat_string_create(&ssl->passphrase, passphrase, passphrase_length);
    if (unlikely(!ret)) {
        cat_update_last_error_of_syscall("Malloc for SSL passphrase failed");
        return cat_false;
    }
    SSL_set_default_passwd_cb_userdata(connection, ssl);
    SSL_set_default_passwd_cb(connection, cat_ssl_password_callback);

    return cat_true;
}

CAT_API cat_bool_t cat_ssl_is_established(const cat_ssl_t *ssl)
{
    return ssl->flags & CAT_SSL_FLAG_HANDSHAKED;
}

CAT_API cat_ssl_ret_t cat_ssl_handshake(cat_ssl_t *ssl)
{
    cat_ssl_connection_t *connection = ssl->connection;
    int n, ssl_error;

    cat_ssl_clear_error();

    n = SSL_do_handshake(connection);

    cat_debug(SSL, "SSL_do_handshake(): %d", n);
    if (n == 1) {
        cat_ssl_handshake_log(ssl);
        ssl->flags |= CAT_SSL_FLAG_HANDSHAKED;
#ifndef SSL_OP_NO_RENEGOTIATION
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#ifdef SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS
        /* initial handshake done, disable renegotiation (CVE-2009-3555) */
        if (connection->s3 && SSL_is_server(connection)) {
            connection->s3->flags |= SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS;
        }
#endif
#endif
#endif
        cat_debug(SSL, "SSL handshake succeeded");
        return CAT_SSL_RET_OK;
    }

    ssl_error = SSL_get_error(connection, n);

    cat_debug(SSL, "SSL_get_error(): %d", ssl_error);

    if (ssl_error == SSL_ERROR_WANT_READ) {
        cat_debug(SSL, "SSL_ERROR_WANT_READ");
        return CAT_SSL_RET_WANT_IO;
    }
    /* memory bios should never block with SSL_ERROR_WANT_WRITE. */

    cat_debug(SSL, "SSL handshake failed");

    if (ssl_error == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0) {
        cat_update_last_error_of_syscall("Peer closed connection in SSL handshake");
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
    if (slen == plen && strncasecmp(s, p, plen) == 0) {
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
        if (plen == slen && strncasecmp(s, p, plen) == 0) {
            return cat_true;
        }
    }

    return cat_false;
}
#endif

CAT_API cat_bool_t cat_ssl_check_host(cat_ssl_t *ssl, const char *name, size_t name_length)
{
    cat_ssl_connection_t *connection = ssl->connection;
    X509 *cert;
    cat_bool_t ret = cat_false;

    cert = SSL_get_peer_certificate(connection);

    if (cert == NULL) {
        return cat_false;
    }

#ifdef X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT
    /* X509_check_host() is only available in OpenSSL 1.0.2+ */
    if (name_length == 0) {
        goto _out;
    }
    if (X509_check_host(cert, (char *) name, name_length, 0, NULL) != 1) {
        cat_debug(SSL, "X509_check_host(): no match");
        goto _out;
    }
    cat_debug(SSL, "X509_check_host(): match");
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
                cat_debug(SSL, "Subject alt name: \"%*s\"", ASN1_STRING_length(str), ASN1_STRING_data(str));
                if (cat_ssl_check_name(name, name_length, str)) {
                    cat_debug(SSL, "Subject alt name: match");
                    GENERAL_NAMES_free(altnames);
                    ret = cat_true;
                    goto _out;
                }
            }
            cat_debug(SSL, "Subject alt name: no match");
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
            cat_debug(SSL, "Common name: \"%*s\"", ASN1_STRING_length(str), ASN1_STRING_data(str));
            if (cat_ssl_check_name(name, name_length, str)) {
                cat_debug(SSL, "Common name: match");
                ret = cat_true;
                goto _out;
            }
        }
        cat_debug(SSL, "Common name: no match");
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

    n = BIO_read(ssl->nbio, buffer, size);
    cat_debug(SSL, "BIO_read(%zu) = %d", size, n);
    if (unlikely(n <= 0)) {
        if (unlikely(!BIO_should_retry(ssl->nbio))) {
            cat_ssl_update_last_error(CAT_ESSL, "BIO_read(%zu) failed", size);
            return CAT_RET_ERROR;
        }
        cat_debug(SSL, "BIO_read() should retry");
        return CAT_RET_AGAIN;
    }

    return n;
}

CAT_API int cat_ssl_write_encrypted_bytes(cat_ssl_t *ssl, const char *buffer, size_t length)
{
    int n;

    cat_ssl_clear_error();

    n = BIO_write(ssl->nbio, buffer, length);
    cat_debug(SSL, "BIO_write(%zu) = %d", length, n);
    if (unlikely(n <= 0)) {
        if (unlikely(!BIO_should_retry(ssl->nbio))) {
            cat_ssl_update_last_error(CAT_ESSL, "BIO_write(%zu) failed", length);
            return CAT_RET_ERROR;
        }
        cat_debug(SSL, "BIO_write() should retry");
        return CAT_RET_AGAIN;
    }

    return n;
}

CAT_API size_t cat_ssl_encrypted_size(size_t length)
{
    return CAT_MEMORY_ALIGNED_SIZE_EX(length, CAT_SSL_MAX_BLOCK_LENGTH) + (CAT_SSL_BUFFER_SIZE - CAT_SSL_MAX_PLAIN_LENGTH);
}

static size_t cat_ssl_encrypt_buffered(cat_ssl_t *ssl, const char *in, size_t *in_length, char *out, size_t *out_length)
{
    cat_ssl_connection_t *connection = ssl->connection;
    size_t nread = 0, nwritten = 0;
    size_t in_size = *in_length;
    size_t out_size = *out_length;
    cat_bool_t ret = cat_false;

    *in_length = 0;
    *out_length = 0;

    if (unlikely(in_size == 0)) {
        return 0;
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
        } else if (n == CAT_RET_AGAIN) {
            // continue to SSL_write()
        } else {
            cat_update_last_error_with_previous("SSL_write() error");
            break;
        }

        if (nwritten == in_size) {
            /* done */
            ret = cat_true;
            break;
        }

        cat_ssl_clear_error();

        n = SSL_write(connection, in + nwritten, in_size - nwritten);

        cat_debug(SSL, "SSL_write(%zu) = %d", in_size - nwritten, n);

        if (unlikely(n <= 0)) {
            int error = SSL_get_error(connection, n);

            if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
                cat_debug(SSL, "SSL_write() want %s", error == SSL_ERROR_WANT_READ ? "read" : "write");
                // continue to  SSL_read_encrypted_bytes()
            } else if (error == SSL_ERROR_SYSCALL) {
                cat_update_last_error_of_syscall("SSL_write() error");
                break;
            } else {
                cat_ssl_update_last_error(CAT_ESSL, "SSL_write() error");
                break;
            }
        } else {
            nwritten += n;
        }
    }

    *in_length = nwritten;
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

    buffer = ssl->write_buffer.value;
    size = cat_ssl_encrypted_size(vin_length);
    if (unlikely(size >  ssl->write_buffer.size)) {
        buffer = (char *) cat_malloc(size);
        if (unlikely(buffer == NULL)) {
            cat_update_last_error_of_syscall("Malloc for SSL write buffer failed");
            return cat_false;
        }
    } else {
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
            vout->length = length;
            vout_counted++;
            if (cat_get_last_error_code() == CAT_ENOBUFS) {
                CAT_ASSERT(length == size);
                cat_debug(SSL, "SSL encrypt buffer extend");
                if (vout_counted == vout_size) {
                    cat_update_last_error(CAT_ENOBUFS, "Unexpected vector count (too many)");
                    goto _error;
                }
                buffer = (char *) cat_malloc(size = CAT_SSL_BUFFER_SIZE);
                if (unlikely(buffer == NULL)) {
                    cat_update_last_error_of_syscall("Realloc for SSL write buffer failed");
                    goto _error;
                }
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
    vout->length = length;
    *vout_count = vout_counted + 1;

    return cat_true;

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

CAT_API cat_bool_t cat_ssl_decrypt(cat_ssl_t *ssl, char *out, size_t *out_length)
{
    cat_ssl_connection_t *connection = ssl->connection;
    cat_buffer_t *buffer = &ssl->read_buffer;
    size_t nread = 0, nwritten = 0;
    size_t out_size = *out_length;

    *out_length = 0;

    if (unlikely(buffer->length == 0)) {
        return cat_true;
    }

    while (1) {
        int n;

        if (unlikely(nread == out_size)) {
            // full
            break;
        }

        cat_ssl_clear_error();

        n = SSL_read(connection, out + nread, out_size - nread);

        cat_debug(SSL, "SSL_read(%zu) = %d", out_size - nread, n);

        if (unlikely(n <= 0)) {
            int error = SSL_get_error(connection, n);

            if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
                cat_debug(SSL, "SSL_read() want %s", error == SSL_ERROR_WANT_READ ? "read" : "write");
                // continue to SSL_write_encrypted_bytes()
            } else if (error == SSL_ERROR_SYSCALL) {
                cat_update_last_error_of_syscall("SSL_read() error");
                break;
            } else {
                cat_ssl_update_last_error(CAT_ESSL, "SSL_read() error");
                break;
            }
        } else {
            nread += n;
        }

        if (unlikely(nwritten == buffer->length)) {
            // ENOBUFS
            break;
        }

        n = cat_ssl_write_encrypted_bytes(
            ssl, buffer->value + nwritten, buffer->length - nwritten
        );

        if (n > 0) {
            nwritten += n;
        } else if (n == CAT_RET_AGAIN) {
            // continue to SSL_read()
        } else {
            cat_update_last_error_with_previous("SSL_write() error");
            break;
        }
    }

    cat_buffer_truncate(buffer, nwritten, 0);

    *out_length = nread;

    return cat_true;
}

CAT_API char *cat_ssl_get_error_reason(void)
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

static void cat_ssl_info_callback(const cat_ssl_connection_t *connection, int where, int ret)
{

#ifndef SSL_OP_NO_RENEGOTIATION
    if ((where & SSL_CB_HANDSHAKE_START) && SSL_is_server(connection)) {
        cat_ssl_t *ssl = cat_ssl_get_from_connection(connection);
        if (ssl->flags & CAT_SSL_FLAG_HANDSHAKED) {
            ssl->flags |= CAT_SSL_FLAG_RENEGOTIATION;
            cat_debug(SSL, "SSL renegotiation");
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
