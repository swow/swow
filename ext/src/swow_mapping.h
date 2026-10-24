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

#ifndef SWOW_MAPPING_H
#define SWOW_MAPPING_H

#include <stdint.h>

/*
 * The first two words of the user area of a named mapping identify the
 * primitive that owns it. A primitive that attaches a mapping by name checks
 * them, so one primitive can not take over the mapping of another.
 */
#define SWOW_MAPPING_FUTEX_MAGIC 0x46555458U /* "FUTX" */
#define SWOW_MAPPING_SPSC_MAGIC 0x53505343U /* "SPSC" */
#define SWOW_MAPPING_VERSION 1

#endif /* SWOW_MAPPING_H */
