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

#ifndef CAT_CURL_H
#define CAT_CURL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#ifdef CAT_HAVE_CURL
#define CAT_CURL 1

#include <curl/curl.h>

CAT_API cat_bool_t cat_curl_module_init(void);
CAT_API cat_bool_t cat_curl_module_shutdown(void);
CAT_API cat_bool_t cat_curl_runtime_init(void);
CAT_API cat_bool_t cat_curl_runtime_shutdown(void);

CAT_API CURLcode cat_curl_easy_perform(CURL *ch);

CAT_API CURLM *cat_curl_multi_init(void);
CAT_API CURLMcode cat_curl_multi_cleanup(CURLM *multi);
CAT_API CURLMcode cat_curl_multi_perform(CURLM *multi, int *running_handles);
CAT_API CURLMcode cat_curl_multi_wait(CURLM *multi, struct curl_waitfd *extra_fds, unsigned int extra_nfds, int timeout_ms, int *numfds);

#endif /* CAT_HAVE_CURL */

#ifdef __cplusplus
}
#endif
#endif /* CAT_CURL_H */
