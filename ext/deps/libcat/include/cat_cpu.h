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

#ifndef CAT_CPU_H
#define CAT_CPU_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

typedef uv_cpu_info_t cat_cpu_info_t;

CAT_API unsigned int cat_cpu_get_available_parallelism(void);
CAT_API unsigned int cat_cpu_count(void);

#ifdef __cplusplus
}
#endif
#endif /* CAT_CPU_H */
