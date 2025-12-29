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

#endif
