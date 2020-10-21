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

#define CAT_ABS(value)           (((value) >= 0) ? (value) : - (value))
#define CAT_MAX(value1, value2)  (((value1) < (value2)) ? (value2) : (value1))
#define CAT_MIN(value1, value2)  (((value1) > (value2)) ? (value2) : (value1))
#define CAT_BETWEEN(value, min, max) \
        (unlikely((value) > (max)) ? (max) : (unlikely((value) < (min)) ? (min) : (value)))
