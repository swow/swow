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
  | Author: dixyes <dixyes@gmail.com>                                        |
  +--------------------------------------------------------------------------+
 */

#include "swow_stream.h"

#include "cat.h"
#include "cat_ssl.h"
#include "cat_socket.h"

#include "swow_ssl.h"
#include "swow_socket.h"
#include "swow_utils.h"

#include "streams/php_streams_int.h"
#include "zend_hash.h"
#include "zend_portability.h"

#ifdef CAT_SSL

cat_bool_t swow_load_stream_cafile(cat_ssl_context_t *context, struct cat_socket_crypto_options_s *options)
{
    php_stream *stream;
    X509 *cert;
    BIO *buffer;
    int buffer_active = 0;
    char *line = NULL;
    size_t line_len;
    long certs_added = 0;
    X509_STORE *cert_store = SSL_CTX_get_cert_store(context->ctx);

    // printf("load file %s\n", cafile);
    cat_bool_t hooking_plain_wrapper = SWOW_STREAM_G(hooking_plain_wrapper);
    cat_bool_t hooking_stdio_ops = SWOW_STREAM_G(hooking_stdio_ops);
    SWOW_STREAM_G(hooking_plain_wrapper) = cat_false;
    SWOW_STREAM_G(hooking_stdio_ops) = cat_false;

    stream = php_stream_open_wrapper(options->ca_file, "rb", 0, NULL);

    if (stream == NULL) {
        php_error(E_WARNING, "failed loading cafile stream: `%s'", options->ca_file);
        goto end;
    } else if (stream->wrapper->is_url) {
        php_stream_close(stream);
        php_error(E_WARNING, "remote cafile streams are disabled for security purposes");
        goto end;
    }

    cert_start: {
        line = php_stream_get_line(stream, NULL, 0, &line_len);
        if (line == NULL) {
            goto stream_complete;
        } else if (!strcmp(line, "-----BEGIN CERTIFICATE-----\n") ||
            !strcmp(line, "-----BEGIN CERTIFICATE-----\r\n")
        ) {
            buffer = BIO_new(BIO_s_mem());
            buffer_active = 1;
            goto cert_line;
        } else {
            efree(line);
            goto cert_start;
        }
    }

    cert_line: {
        BIO_puts(buffer, line);
        efree(line);
        line = php_stream_get_line(stream, NULL, 0, &line_len);
        if (line == NULL) {
            goto stream_complete;
        } else if (!strcmp(line, "-----END CERTIFICATE-----") ||
            !strcmp(line, "-----END CERTIFICATE-----\n") ||
            !strcmp(line, "-----END CERTIFICATE-----\r\n")
        ) {
            goto add_cert;
        } else {
            goto cert_line;
        }
    }

    add_cert: {
        BIO_puts(buffer, line);
        efree(line);
        cert = PEM_read_bio_X509(buffer, NULL, 0, NULL);
        BIO_free(buffer);
        buffer_active = 0;
        if (cert && X509_STORE_add_cert(cert_store, cert)) {
            ++certs_added;
            X509_free(cert);
        }
        goto cert_start;
    }

    stream_complete: {
        php_stream_close(stream);
        if (buffer_active == 1) {
            BIO_free(buffer);
        }
    }

    if (certs_added == 0) {
        php_error(E_WARNING, "no valid certs found cafile stream: `%s'", options->ca_file);
    }

end:
    SWOW_STREAM_G(hooking_plain_wrapper) = hooking_plain_wrapper;
    SWOW_STREAM_G(hooking_stdio_ops) = hooking_stdio_ops;
    return certs_added > 0;
}

cat_bool_t swow_ssl_enable_peer_fingerprint_verify(zval *zpeer_fingerprint, cat_ssl_peer_fingerprint_t **pfingerprints, int php_warning) {
    cat_bool_t ret = cat_false;
    cat_ssl_peer_fingerprint_t *fingerprints = NULL;
    /*
     * according to PHP documentation:
     * When a string is used, the length will determine which hashing algorithm is applied, either "md5" (32) or "sha1" (40).
     * When an array is used, the keys indicate the hashing algorithm name and each corresponding value is the expected digest. 
     * but this error handling behavior is not confirmed to PHP
     * we error here right now, donot continue
     * (PHP will try to connect/accept then verify the fingerprint and it will fail)
     */
    if (Z_TYPE_P(zpeer_fingerprint) == IS_STRING) {
        // single kind of fingerprint
        fingerprints = (cat_ssl_peer_fingerprint_t *) cat_calloc(2 * sizeof(cat_ssl_peer_fingerprint_t) + EVP_MAX_MD_SIZE, 1);
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(fingerprints == NULL)) {
            if (php_warning) {
                cat_update_last_error(CAT_ENOMEM, "failed to allocate memory for peer fingerprints");
            } else {
                swow_throw_exception(swow_socket_exception_ce, CAT_ENOMEM, "failed to allocate memory for peer fingerprints");
            }
            goto _cleanup;
        }
#endif
        switch (Z_STRLEN_P(zpeer_fingerprint)) {
        case 32:
            fingerprints[0].algorithm = "md5";
            break;
        case 40:
            fingerprints[0].algorithm = "sha1";
            break;
        default:
            if (php_warning) {
                // php will try to get the digest with algorithm (const char *) NULL, then fail
                // so we fail here right now with "Unknown digest algorithm" error
                cat_update_last_error(CAT_EINVAL, "Unknown digest algorithm");
            } else {
                swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "invalid peer fingerprint length: %zu, only md5 (32) or sha1 (40) are supported", Z_STRLEN_P(zpeer_fingerprint));
            }
            goto _cleanup;
        }
        if (swow_utils_parse_hex_string(
            (unsigned char *) (fingerprints + 2),
            Z_STRVAL_P(zpeer_fingerprint),
            Z_STRLEN_P(zpeer_fingerprint)
        ) < 0) {
            if (php_warning) {
                // php will try to get the digest with algorithm (const char *) NULL, then fail
                // so we fail here right now with "peer_fingerprint match failure" error
                cat_update_last_error(CAT_EINVAL, "peer_fingerprint match failure");
            } else {
                swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "invalid peer fingerprint, expected hex string");
            }
            goto _cleanup;
        }
        fingerprints[0].fingerprint = (unsigned char *) (fingerprints + 2);
        ret = cat_true;
        goto _cleanup;
    } else if (Z_TYPE_P(zpeer_fingerprint) == IS_ARRAY) {
        uint32_t count = zend_hash_num_elements(Z_ARR_P(zpeer_fingerprint));
        if (count == 0) {
            if (php_warning) {
                php_error_docref(NULL, E_WARNING, "Invalid peer_fingerprint array; [algo => fingerprint] form required");
                // php will try to compare the fingerprint, so we warn here
                cat_update_last_error(CAT_EINVAL, "peer_fingerprint match failure");
            } else {
                swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "invalid peer fingerprint, expected array with at least one element");
            }
            goto _cleanup;
        }
        fingerprints = (cat_ssl_peer_fingerprint_t *) cat_calloc(
            count * (EVP_MAX_MD_SIZE + sizeof(cat_ssl_peer_fingerprint_t)) + sizeof(cat_ssl_peer_fingerprint_t),
            1
        );
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(fingerprints == NULL)) {
            if (php_warning) {
                cat_update_last_error(CAT_ENOMEM, "failed to allocate memory for peer fingerprints");
            } else {
                swow_throw_exception(swow_socket_exception_ce, CAT_ENOMEM, "failed to allocate memory for peer fingerprints");
            }
            goto _cleanup;
        }
#endif
        unsigned char *p_binary_digest = (unsigned char *) (fingerprints + count + 1);
        size_t i = 0;
        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(zpeer_fingerprint), zend_string *key, zval *value) {
            if (key == NULL || value == NULL) {
                if (php_warning) {
                    php_error_docref(NULL, E_WARNING, "Invalid peer_fingerprint array; [algo => fingerprint] form required");
                    // php will try to compare the fingerprint, so we warn here
                    cat_update_last_error(CAT_EINVAL, "peer_fingerprint match failure");
                } else {
                    swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "invalid peer fingerprint, expected array with string key and hex string value");
                }
                goto _cleanup;
            }
            // I'm not sure if ZSTR_VAL(key) is null-terminated
            // but PHP passes it to EVP_get_digestbyname/OBJ_NAME_get
            // so we assume it is null-terminated
            fingerprints[i].algorithm = ZSTR_VAL(key);

            // check if the algo is supported
            const EVP_MD *md = (const EVP_MD *) OBJ_NAME_get(fingerprints[i].algorithm, OBJ_NAME_TYPE_MD_METH);
            if (md == NULL) {
                if (php_warning) {
                    cat_update_last_error(CAT_EINVAL, "Unknown digest algorithm");
                } else {
                    swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "invalid peer fingerprint algorithm \"%s\"", ZSTR_VAL(key));
                }
                goto _cleanup;
            }
            size_t digest_length = (size_t) EVP_MD_size(md);
            if (2 * digest_length != Z_STRLEN_P(value)) {
                if (php_warning) {
                    cat_update_last_error(CAT_EINVAL, "peer_fingerprint match failure");
                } else {
                    swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "invalid peer fingerprint hex string length, expected %zu, got %zu", 2 * digest_length, Z_STRLEN_P(value));
                }
                goto _cleanup;
            }

            if (swow_utils_parse_hex_string(
                p_binary_digest + (i * EVP_MAX_MD_SIZE),
                Z_STRVAL_P(value),
                Z_STRLEN_P(value)
            ) < 0) {
                if (php_warning) {
                    cat_update_last_error(CAT_EINVAL, "peer_fingerprint match failure");
                } else {
                    swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "invalid peer fingerprint value, expected hex string");
                }
                goto _cleanup;
            }
            fingerprints[i].fingerprint = p_binary_digest + (i * EVP_MAX_MD_SIZE);
            i++;
        } ZEND_HASH_FOREACH_END();
        ret = cat_true;
        goto _cleanup;
    } else {
        if (php_warning) {
            cat_update_last_error(CAT_EINVAL, "Expected peer fingerprint must be a string or an array");
        } else {
            swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "invalid peer fingerprint, expected string or array");
        }
        goto _cleanup;
    }
_cleanup:
    if (ret == cat_false && fingerprints != NULL) {
        cat_free(fingerprints);
        fingerprints = NULL;
    }
    *pfingerprints = fingerprints;
    return ret;
}

#ifndef OPENSSL_NO_TLSEXT

// TODO: implement this (open_basedir limitation)
// // from ext/openssl/openssl.c @ 040ea4ab5f9e2390ead553104a3a569768c678a5

// /* openssl file path check error function */
// static void swow_php_openssl_check_path_error(uint32_t arg_num, int type, const char *format, ...)
// {
//     va_list va;
//     const char *arg_name;

//     va_start(va, format);

//     if (type == E_ERROR) {
//         zend_argument_error_variadic(zend_ce_value_error, arg_num, format, va);
//     } else {
//         arg_name = get_active_function_arg_name(arg_num);
//         php_verror(NULL, arg_name, type, format, va);
//     }
//     va_end(va);
// }

// /* openssl file path check extended */
// static bool swow_php_openssl_check_path_ex(
//         const char *file_path, size_t file_path_len, char *real_path, uint32_t arg_num,
//         bool contains_file_protocol, bool is_from_array, const char *option_name)
// {
//     const char *fs_file_path;
//     size_t fs_file_path_len;
//     const char *error_msg = NULL;
//     int error_type = E_WARNING;

//     if (file_path_len == 0) {
//         real_path[0] = '\0';
//         return true;
//     }

//     if (contains_file_protocol) {
//         size_t path_prefix_len = sizeof("file://") - 1;
//         if (file_path_len <= path_prefix_len) {
//             return false;
//         }
//         fs_file_path = file_path + path_prefix_len;
//         fs_file_path_len = file_path_len - path_prefix_len;
//     } else {
//         fs_file_path = file_path;
//         fs_file_path_len = file_path_len;
//     }

//     if (zend_char_has_nul_byte(fs_file_path, fs_file_path_len)) {
//         error_msg = "must not contain any null bytes";
//         error_type = E_ERROR;
//     } else if (expand_filepath(fs_file_path, real_path) == NULL) {
//         error_msg = "must be a valid file path";
//     }

//     if (error_msg != NULL) {
//         if (arg_num == 0) {
//             const char *option_title = option_name ? option_name : "unknown";
//             const char *option_label = is_from_array ? "array item" : "option";
//             php_error_docref(NULL, E_WARNING, "Path for %s %s %s",
//                     option_title, option_label, error_msg);
//         } else if (is_from_array && option_name != NULL) {
//             swow_php_openssl_check_path_error(
//                     arg_num, error_type, "option %s array item %s", option_name, error_msg);
//         } else if (is_from_array) {
//             swow_php_openssl_check_path_error(arg_num, error_type, "array item %s", error_msg);
//         } else if (option_name != NULL) {
//             swow_php_openssl_check_path_error(
//                     arg_num, error_type, "option %s %s", option_name, error_msg);
//         } else {
//             swow_php_openssl_check_path_error(arg_num, error_type, "%s", error_msg);
//         }
//     } else if (!php_check_open_basedir(real_path)) {
//         return true;
//     }

//     return false;
// }

// // from ext/openssl/php_openssl.h @ d0c0a9abfdc3d60f8e442e1ed4e13b200abd03de

// /* openssl file path extra check with zend string */
// static inline bool swow_php_openssl_check_path_str_ex(
//     zend_string *file_path, char *real_path, uint32_t arg_num,
//     bool contains_file_protocol, bool is_from_array, const char *option_name)
// {
//     return swow_php_openssl_check_path_ex(
//         ZSTR_VAL(file_path), ZSTR_LEN(file_path), real_path, arg_num, contains_file_protocol,
//         is_from_array, option_name);
// }

static void swow_ssl_server_sni_data_destructor(zval *zcontext) {
    cat_ssl_context_t *context = (cat_ssl_context_t *) Z_PTR_P(zcontext);
    cat_ssl_context_close(context);
}

swow_ssl_server_sni_data_t *swow_ssl_server_sni_data_alloc(void) {
    swow_ssl_server_sni_data_t *contexts =
        (swow_ssl_server_sni_data_t *) cat_calloc(1, sizeof(swow_ssl_server_sni_data_t));
#if CAT_ALLOC_HANDLE_ERRORS
    // let caller handle the error
    if (unlikely(contexts == NULL)) {
        return NULL;
    }
#endif
    zend_hash_init(&contexts->fullmatch, 0, NULL, NULL, true);
    zend_hash_init(&contexts->wildcard, 0, NULL, NULL, true);
    zend_hash_init(&contexts->contexts, 0, NULL, swow_ssl_server_sni_data_destructor, true);
    return contexts;
}

static inline void swow_ssl_server_sni_data_add_cert(
    swow_ssl_server_sni_data_t *contexts,
    const char *matcher, size_t matcher_len, cat_ssl_context_t *ctx
) {
    if (matcher_len > 2 && matcher[0] == '*' && matcher[1] == '.') {
        // wildcard matcher
        zend_hash_str_add_new_ptr(&contexts->wildcard, matcher + 2, matcher_len - 2, ctx);
    } else {
        // fullmatch matcher
        zend_hash_str_add_new_ptr(&contexts->fullmatch, matcher, matcher_len, ctx);
    }
}


void swow_ssl_server_sni_data_free(swow_ssl_server_sni_data_t *contexts) {
    if (contexts == NULL) {
        return;
    }
    zend_hash_destroy(&contexts->fullmatch);
    zend_hash_destroy(&contexts->wildcard);
    zend_hash_destroy(&contexts->contexts);
    cat_free(contexts);
}

static zend_always_inline cat_bool_t load_sni_server_cert_and_key_from_file(
    SSL_CTX *ctx, const char *cert_path, const char *key_path, int php_warning
) {
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_path) != 1) {
        if (php_warning) {
            php_error_docref(NULL, E_WARNING,
                "Failed setting local cert chain file `%s'; " \
                "check that your cafile/capath settings include " \
                "details of your certificate and its issuer",
                cert_path
            );
        } else {
            swow_throw_exception(swow_socket_exception_ce, CAT_ESSL, "Failed setting local cert chain file '%s'", cert_path);
        }
        return cat_false;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) != 1) {
        if (php_warning) {
            php_error_docref(NULL, E_WARNING,
                "Failed setting private key from file `%s'",
                key_path
            );
        } else {
            swow_throw_exception(swow_socket_exception_ce, CAT_ESSL, "Failed setting private key from file '%s'", key_path);
        }
        return cat_false;
    }

    return cat_true;
}

cat_bool_t swow_ssl_enable_server_sni(const zval *zconfig, swow_ssl_server_sni_data_t *contexts, bool php_warning) {
    if (Z_TYPE_P(zconfig) != IS_ARRAY) {
        // not supported
        if (php_warning) {
            cat_update_last_error(CAT_EINVAL, "SNI_server_certs requires an array mapping host names to cert paths");
        } else {
            swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "SNI_server_certs requires an array of configs");
        }
        return cat_false;
    }

    if (zend_hash_num_elements(Z_ARRVAL_P(zconfig)) == 0) {
        // empty array
        if (php_warning) {
            cat_update_last_error(CAT_EINVAL, "SNI_server_certs host cert array must not be empty");
        } else {
            swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "SNI_server_certs host cert array must not be empty");
        }
        return cat_false;
    }

    ZEND_HASH_REVERSE_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(zconfig), zend_string *key, zval *config) {
        cat_ssl_context_t *ctx = cat_ssl_context_create(CAT_SSL_METHOD_TLS, CAT_SSL_PROTOCOLS_ALL); // TODO: DTLS
        if (ctx == NULL) {
            if (php_warning) {
                cat_update_last_error(CAT_ESSL, "Failed creating SSL context");
            } else {
                swow_throw_exception(swow_socket_exception_ce, CAT_ESSL, "Failed creating SSL context");
            }
            return cat_false;
        }
        zend_hash_next_index_insert_ptr(&contexts->contexts, ctx);

        if (Z_TYPE_P(config) == IS_ARRAY) {
            // if it is an array, it is a config like { "local_cert" => "path/to/cert.pem", "local_pk" => "path/to/key.pem" }
            // we need to load the cert and key from the array
            zval *zcert, *zpkey;
            if (php_warning) {
                // for PHP stream, uses "local_cert" and "local_pk"
                zcert = zend_hash_str_find(Z_ARRVAL_P(config), CAT_STRL("local_cert"));
                zpkey = zend_hash_str_find(Z_ARRVAL_P(config), CAT_STRL("local_pk"));
            } else {
                // for Swow, uses "certificate" and "certificate_key"
                zcert = zend_hash_str_find(Z_ARRVAL_P(config), CAT_STRL("certificate"));
                zpkey = zend_hash_str_find(Z_ARRVAL_P(config), CAT_STRL("certificate_key"));
            }

            if (zcert == NULL) {
                if (php_warning) {
                    cat_update_last_error(CAT_EINVAL, "local_cert not present in the array");
                } else {
                    swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "certificate not present in SNI_server_certs");
                }
                goto _failed;
            }
            if (zpkey == NULL) {
                if (php_warning) {
                    cat_update_last_error(CAT_EINVAL, "local_pk not present in the array");
                } else {
                    swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "certificate_key not present in SNI_server_certs");
                }
                goto _failed;
            }

            if (!load_sni_server_cert_and_key_from_file(ctx->ctx, Z_STRVAL_P(zcert), Z_STRVAL_P(zpkey), php_warning)) {
                goto _failed;
            }

        } else if (Z_TYPE_P(config) == IS_STRING) {
            if (!load_sni_server_cert_and_key_from_file(ctx->ctx, Z_STRVAL_P(config), Z_STRVAL_P(config), php_warning)) {
                goto _failed;
            }
        } else {
            // not supported
            if (php_warning) {
                cat_update_last_error(CAT_ENOENT, "SNI_server_certs options values must be of type array|string");
            } else {
                swow_throw_exception(swow_socket_exception_ce, CAT_ENOENT, "SNI_server_certs value must be a string or array");
            }
            goto _failed;
        }

        // handle the key
        int matchers = 0;
        if (!key) {
            // no key specified, use the cert name (SAN only, not CN) as matcher
            X509 *cert = SSL_CTX_get0_certificate(ctx->ctx);
            if (cert != NULL) {
                STACK_OF(GENERAL_NAME) *san_names =
                    X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
                if (san_names != NULL) {
                    for (int j = 0; j < sk_GENERAL_NAME_num(san_names); j++) {
                        GENERAL_NAME *name = sk_GENERAL_NAME_value(san_names, j);
                        if (name->type == GEN_DNS && name->d.ia5 != NULL) {
                            matchers++;
                            swow_ssl_server_sni_data_add_cert(contexts, (const char *)name->d.ia5->data, name->d.ia5->length, ctx);
                        }
                    }
                    GENERAL_NAMES_free(san_names);
                }
            }
        } else {
            // key specified, use the key as matcher
            matchers++;
            swow_ssl_server_sni_data_add_cert(contexts, ZSTR_VAL(key), ZSTR_LEN(key), ctx);
        }
        if (matchers == 0) {
            // no matchers found, throw error
            if (php_warning) {
                cat_update_last_error(CAT_EINVAL, "SNI_server_certs array requires string host name keys");
            } else {
                swow_throw_exception(swow_socket_exception_ce, CAT_EINVAL, "No dns name SAN found for cert in SNI_server_certs");
            }
            goto _failed;
        }
    } ZEND_HASH_FOREACH_END();

    if (0) {
_failed:
        return cat_false;
    }

    return cat_true;
}

static int swow_openssl_server_sni_callback(SSL *ssl_handle, int *al, void *arg)
{
    size_t i;
    const char *server_name;
    size_t server_name_len;
    swow_ssl_server_sni_data_t *contexts;
    zval *zcontext;
    cat_ssl_context_t *context;

    contexts = (swow_ssl_server_sni_data_t *)arg;
    if (contexts == NULL) {
        // no contexts, use the default context
        return SSL_TLSEXT_ERR_OK;
    }

    server_name = SSL_get_servername(ssl_handle, TLSEXT_NAMETYPE_host_name);
    if (!server_name) {
        return SSL_TLSEXT_ERR_NOACK;
    }
    server_name_len = strlen(server_name);

    // try full match first
    zcontext = zend_hash_str_find(&contexts->fullmatch, server_name, server_name_len);
    if (zcontext != NULL) {
        context = (cat_ssl_context_t *)Z_PTR_P(zcontext);
        return SSL_set_SSL_CTX(ssl_handle, context->ctx) ? SSL_TLSEXT_ERR_OK : SSL_TLSEXT_ERR_NOACK;
    }

    // try wildcard match
    // remove first domain part some.example.com -> example.com
    for (i = 0; i < server_name_len; i++) {
        if (server_name[i] == '.') {
            break;
        }
    }
    if (i < server_name_len) {
        server_name = server_name + i + 1;
        server_name_len = server_name_len - i - 1;
    } else {
        // bad domain, nothing can match
        return SSL_TLSEXT_ERR_NOACK;
    }
    zcontext = zend_hash_str_find(&contexts->wildcard, server_name, server_name_len);
    if (zcontext != NULL) {
        context = (cat_ssl_context_t *)Z_PTR_P(zcontext);
        return SSL_set_SSL_CTX(ssl_handle, context->ctx) ? SSL_TLSEXT_ERR_OK : SSL_TLSEXT_ERR_NOACK;
    }

    return SSL_TLSEXT_ERR_NOACK;
}

cat_bool_t swow_ssl_before_handshake_callback(cat_ssl_t* ssl, void * data) {
    SSL_CTX *ctx = SSL_get_SSL_CTX(ssl->connection);

    SSL_CTX_set_tlsext_servername_callback(ctx, swow_openssl_server_sni_callback);
    SSL_CTX_set_tlsext_servername_arg(ctx, data);
    return cat_true;
}

# endif // OPENSSL_NO_TLSEXT

// from ext/openssl/php_openssl.h @ d0c0a9abfdc3d60f8e442e1ed4e13b200abd03de
typedef struct _php_openssl_certificate_object {
    X509 *x509;
    zend_object std;
} php_openssl_certificate_object;

static zend_class_entry *swow_php_openssl_certificate_ce = NULL;

static inline php_openssl_certificate_object *php_openssl_certificate_from_obj(zend_object *obj) {
    return (php_openssl_certificate_object *)((char *)(obj) - XtOffsetOf(php_openssl_certificate_object, std));
}

#define Z_OPENSSL_CERTIFICATE_P(zv) php_openssl_certificate_from_obj(Z_OBJ_P(zv))


void swow_ssl_after_handshake_callback(cat_ssl_t* ssl, cat_bool_t success, void * data) {
    php_stream *stream = (php_stream *)data;
    X509 *peer_cert = NULL;
    zval zcert;
    zval *val;
    php_openssl_certificate_object *cert_object;

    // we need to get the peer cert, regardless of success or not
    (void) success;

    if (swow_php_openssl_certificate_ce == NULL) {
        // we cannot initialize openssl certificate object, cannot capture peer certificate
        // TODO: throw exception ?
        php_error_docref(NULL, E_WARNING, "PHP openssl extension is not loaded or version mismatch with Swow, cannot capture peer certificate");
        return;
    }

    if (
        NULL != PHP_STREAM_CONTEXT(stream) &&
        NULL != (val = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream),
            "ssl", "capture_peer_cert")) &&
        zend_is_true(val)
    ) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        peer_cert = SSL_get1_peer_certificate(ssl->connection);
#else
        peer_cert = SSL_get_peer_certificate(ssl->connection);
#endif
        if (peer_cert != NULL) {
            object_init_ex(&zcert, swow_php_openssl_certificate_ce);
            cert_object = Z_OPENSSL_CERTIFICATE_P(&zcert);
            cert_object->x509 = peer_cert;

            php_stream_context_set_option(PHP_STREAM_CONTEXT(stream), "ssl", "peer_certificate", &zcert);
            zval_ptr_dtor(&zcert);
        }
    }
    
    if (
        NULL != PHP_STREAM_CONTEXT(stream) &&
        NULL != (val = php_stream_context_get_option(PHP_STREAM_CONTEXT(stream),
    "ssl", "capture_peer_cert_chain")) &&
        zend_is_true(val)
    ) {
        zval arr;
        STACK_OF(X509) *chain;

        chain = SSL_get_peer_cert_chain(ssl->connection);

        if (chain && sk_X509_num(chain) > 0) {
            int i;
            array_init(&arr);

            for (i = 0; i < sk_X509_num(chain); i++) {
                X509 *mycert = X509_dup(sk_X509_value(chain, i));

                object_init_ex(&zcert, swow_php_openssl_certificate_ce);
                cert_object = Z_OPENSSL_CERTIFICATE_P(&zcert);
                cert_object->x509 = mycert;
                add_next_index_zval(&arr, &zcert);
            }

        } else {
            ZVAL_NULL(&arr);
        }

        php_stream_context_set_option(PHP_STREAM_CONTEXT(stream), "ssl", "peer_certificate_chain", &arr);
        zval_ptr_dtor(&arr);
    }
}

zend_result swow_ssl_module_init(INIT_FUNC_ARGS)
{
    if (!cat_ssl_module_init()) {
        return FAILURE;
    }

    // get php_openssl_certificate_ce from php
    swow_php_openssl_certificate_ce = (zend_class_entry *) zend_hash_str_find_ptr(
        CG(class_table), ZEND_STRL("opensslcertificate")
    );
    if (swow_php_openssl_certificate_ce == NULL) {
        return SUCCESS;
    }

    // check if the offset matches our version
#if PHP_VERSION_ID < 80300
    if (
        swow_php_openssl_certificate_ce->properties_info_table == NULL ||
        swow_php_openssl_certificate_ce->properties_info_table[0]->offset != XtOffsetOf(php_openssl_certificate_object, std)
    )
#else
    if (
        swow_php_openssl_certificate_ce->default_object_handlers == NULL ||
        swow_php_openssl_certificate_ce->default_object_handlers->offset != XtOffsetOf(php_openssl_certificate_object, std)
    )
#endif
    {
        return SUCCESS;
    }

    return SUCCESS;
}

#endif
