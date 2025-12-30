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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#ifndef SWOW_SSL_H
#define SWOW_SSL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"
#include "cat_ssl.h"

#ifdef CAT_SSL
/* private: for reuse in swow stream / PHP stream */

cat_bool_t swow_load_stream_cafile(cat_ssl_context_t *context, struct cat_socket_crypto_options_s *options);

cat_bool_t swow_ssl_enable_peer_fingerprint_verify(zval *zpeer_fingerprint, cat_ssl_peer_fingerprint_t **pfingerprints, int php_warning);

typedef struct {
    HashTable fullmatch;
    HashTable wildcard;
    HashTable contexts;
} swow_ssl_server_sni_data_t;

cat_bool_t swow_ssl_enable_server_sni(const zval *zconfig, swow_ssl_server_sni_data_t *contexts, bool php_warning);

swow_ssl_server_sni_data_t *swow_ssl_server_sni_data_alloc(void);
void swow_ssl_server_sni_data_free(swow_ssl_server_sni_data_t *contexts);

cat_bool_t swow_ssl_before_handshake_callback(cat_ssl_t* ssl, void * data);
void swow_ssl_after_handshake_callback(cat_ssl_t* ssl, cat_bool_t success, void * data);

zend_result swow_ssl_module_init(INIT_FUNC_ARGS);

#endif
#ifdef __cplusplus
}
#endif
#endif /* SWOW_SSL_H */
