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

#ifndef SWOW_MODULES_H
#define SWOW_MODULES_H

#include "swow.h"

#ifdef ZTS

/* thread */

zend_result swow_thread_module_init(INIT_FUNC_ARGS);
zend_result swow_thread_module_shutdown(SHUTDOWN_FUNC_ARGS);
zend_result swow_thread_runtime_init(INIT_FUNC_ARGS);
zend_result swow_thread_runtime_shutdown(SHUTDOWN_FUNC_ARGS);

/* cat_true when a child thread outlived the shutdown timeout of this request */
cat_bool_t swow_thread_children_abandoned(void);

/* mutex */

zend_result swow_mutex_module_init(INIT_FUNC_ARGS);
zend_result swow_mutex_module_shutdown(SHUTDOWN_FUNC_ARGS);

/* semaphore */

zend_result swow_semaphore_module_init(INIT_FUNC_ARGS);
zend_result swow_semaphore_module_shutdown(SHUTDOWN_FUNC_ARGS);

/* thannel */

zend_result swow_thannel_module_init(INIT_FUNC_ARGS);
zend_result swow_thannel_module_shutdown(SHUTDOWN_FUNC_ARGS);

/* thread barrier */

zend_result swow_thread_barrier_module_init(INIT_FUNC_ARGS);
zend_result swow_thread_barrier_module_shutdown(SHUTDOWN_FUNC_ARGS);

#endif /* ZTS */

/* futex, available in ZTS and NTS */

zend_result swow_futex_module_init(INIT_FUNC_ARGS);
zend_result swow_futex_module_shutdown(SHUTDOWN_FUNC_ARGS);

/* shared buffer, available in ZTS and NTS */

zend_result swow_shared_buffer_module_init(INIT_FUNC_ARGS);
zend_result swow_shared_buffer_module_shutdown(SHUTDOWN_FUNC_ARGS);

#endif /* SWOW_MODULES_H */
