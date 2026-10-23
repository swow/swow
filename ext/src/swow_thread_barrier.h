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

#ifndef SWOW_THREAD_BARRIER_H
#define SWOW_THREAD_BARRIER_H

#include "swow.h"

#include "swow_shared_object.h"

#include "cat_thread_sync.h"

#ifdef ZTS

extern SWOW_API zend_class_entry *swow_thread_barrier_ce;
extern SWOW_API zend_object_handlers swow_thread_barrier_handlers;

typedef struct swow_thread_barrier_s {
    swow_shared_object_t object;
    uint32_t magic;
    cat_mutex_t mutex;
    cat_cond_t cond;
    uint32_t count;
    uint32_t waiting;
    uint64_t generation;
} swow_thread_barrier_t;

typedef struct swow_thread_barrier_object_s {
    swow_thread_barrier_t *barrier;
    zend_object std;
} swow_thread_barrier_object_t;

zend_result swow_thread_barrier_module_init(INIT_FUNC_ARGS);
zend_result swow_thread_barrier_module_shutdown(SHUTDOWN_FUNC_ARGS);

#endif /* ZTS */

#endif /* SWOW_THREAD_BARRIER_H */
