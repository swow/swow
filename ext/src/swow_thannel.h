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

#ifndef SWOW_THANNEL_H
#define SWOW_THANNEL_H

#include "swow.h"

#include "swow_shared_object.h"

#include "cat_thannel.h"

#ifdef ZTS

extern SWOW_API zend_class_entry *swow_thannel_ce;
extern SWOW_API zend_class_entry *swow_thannel_exception_ce;
extern SWOW_API zend_object_handlers swow_thannel_handlers;

typedef struct swow_thannel_s {
    swow_shared_object_t object;
    cat_thannel_t thannel;
} swow_thannel_t;

typedef struct swow_thannel_object_s {
    swow_thannel_t *thannel;
    zend_object std;
} swow_thannel_object_t;

zend_result swow_thannel_module_init(INIT_FUNC_ARGS);
zend_result swow_thannel_module_shutdown(SHUTDOWN_FUNC_ARGS);

#endif /* ZTS */

#endif /* SWOW_THANNEL_H */
