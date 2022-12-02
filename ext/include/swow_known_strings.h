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
  | Author: Twosee <twosee@php.net>                                          |
  +--------------------------------------------------------------------------+
 */

#ifndef SWOW_KNOWN_STRINGS_H
#define SWOW_KNOWN_STRINGS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#define SWOW_KNOWN_STRING(name) swow_known_string_##name

#define SWOW_KNOWN_STRING_INIT(name)           SWOW_KNOWN_STRING_INIT_STRL(name, #name)
#define SWOW_KNOWN_STRING_INIT_STRL(name, str) do { \
    SWOW_KNOWN_STRING(name) = zend_new_interned_string(zend_string_init(ZEND_STRL(str), true)); \
} while (0)

#define SWOW_KNOWN_STRING_STORAGE_GEN(name, ...)        SWOW_API zend_string *SWOW_KNOWN_STRING(name);
#define SWOW_KNOWN_STRING_DECLARATION_GEN(name, ...)    extern SWOW_KNOWN_STRING_STORAGE_GEN(name)
#define SWOW_KNOWN_STRING_INIT_GEN(name)                SWOW_KNOWN_STRING_INIT(name);
#define SWOW_KNOWN_STRING_INIT_STRL_GEN(name, str)      SWOW_KNOWN_STRING_INIT_STRL(name, str);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_KNOWN_STRINGS_H */
