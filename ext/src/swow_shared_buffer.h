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

#ifndef SWOW_SHARED_BUFFER_H
#define SWOW_SHARED_BUFFER_H

#include "swow.h"

#include "swow_shared_object.h"

#include "cat_shm.h"

#ifdef ZTS

extern SWOW_API zend_class_entry *swow_shared_buffer_ce;
extern SWOW_API zend_object_handlers swow_shared_buffer_handlers;

zend_result swow_shared_buffer_module_init(INIT_FUNC_ARGS);
zend_result swow_shared_buffer_module_shutdown(SHUTDOWN_FUNC_ARGS);

#endif /* ZTS */

#endif /* SWOW_SHARED_BUFFER_H */
