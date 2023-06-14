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

#ifndef CAT_DNS_H
#define CAT_DNS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#include "cat_socket.h"

/* Notice: this module is a part of Socket */

/*
typedef struct cat_dns_cache_s {

} cat_dns_cache_t;
*/

CAT_API struct addrinfo *cat_dns_getaddrinfo(const char *hostname, const char *service, const struct addrinfo *hints);
CAT_API struct addrinfo *cat_dns_getaddrinfo_ex(const char *hostname, const char *service, const struct addrinfo *hints, cat_timeout_t timeout);
CAT_API void cat_dns_freeaddrinfo(struct addrinfo *response);

CAT_API cat_bool_t cat_dns_get_ip(char *buffer, size_t buffer_size, const char *name, int af);
CAT_API cat_bool_t cat_dns_get_ip_ex(char *buffer, size_t buffer_size, const char *name, int af, cat_timeout_t timeout);

#ifdef __cplusplus
}
#endif
#endif /* CAT_DNS_H */
