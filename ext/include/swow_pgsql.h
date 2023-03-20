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
  | Author: Codinghuang <codinghuang@qq.com>                                 |
  | Author: dixyes <dixyes@gmail.com>                                        |
  +--------------------------------------------------------------------------+
*/

#ifndef SWOW_PGSQL_H
#define SWOW_PGSQL_H

#include "swow.h"

// at swow_pgsql_version.c
extern int swow_building_libpq_version;

// at swow_pgsql_driver.c
extern int swow_libpq_version;
extern cat_bool_t swow_pgsql_hooked;

zend_result swow_pgsql_module_init(INIT_FUNC_ARGS);
zend_result swow_pgsql_module_shutdown(INIT_FUNC_ARGS);

#endif // SWOW_PGSQL_H
