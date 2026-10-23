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

#ifndef SWOW_MUTEX_H
#define SWOW_MUTEX_H

#include "swow.h"

#include "swow_shared_object.h"
#include "swow_sync.h"

#include "cat_thread_sync.h"

#ifdef ZTS

extern SWOW_API zend_class_entry *swow_mutex_ce;
extern SWOW_API zend_object_handlers swow_mutex_handlers;

typedef struct swow_mutex_s {
    swow_shared_object_t object;
    cat_mutex_t mutex;
    cat_atomic_uint64_t owner;
} swow_mutex_t;

typedef struct swow_mutex_object_s {
    swow_mutex_t *mutex;
    zend_object std;
} swow_mutex_object_t;

zend_result swow_mutex_module_init(INIT_FUNC_ARGS);
zend_result swow_mutex_module_shutdown(SHUTDOWN_FUNC_ARGS);

#endif /* ZTS */

#endif /* SWOW_MUTEX_H */
