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

#ifndef SWOW_FUTEX_H
#define SWOW_FUTEX_H

#include "swow.h"

#include "swow_shared_object.h"

#include "cat_futex.h"
#include "cat_shm.h"

#ifdef ZTS

extern SWOW_API zend_class_entry *swow_futex_ce;
extern SWOW_API zend_object_handlers swow_futex_handlers;

zend_result swow_futex_module_init(INIT_FUNC_ARGS);
zend_result swow_futex_module_shutdown(SHUTDOWN_FUNC_ARGS);

#endif /* ZTS */

#endif /* SWOW_FUTEX_H */
