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
  | Author: Twosee <twose@qq.com>                                            |
  +--------------------------------------------------------------------------+
 */

#ifndef SWOW_EVENT_H
#define SWOW_EVENT_H

#include "swow.h"
#include "swow_coroutine.h"

#include "cat_event.h"

CAT_GLOBALS_STRUCT_BEGIN(swow_event)
    zval reserve;
CAT_GLOBALS_STRUCT_END(swow_event)

extern SWOW_API CAT_GLOBALS_DECLARE(swow_event)

#define SWOW_EVENT_G(x) CAT_GLOBALS_GET(swow_event, x)

extern SWOW_API zend_class_entry *swow_event_ce;
extern SWOW_API zend_object_handlers swow_event_handlers;

extern SWOW_API zend_class_entry *swow_event_scheduler_ce;
extern SWOW_API zend_object_handlers swow_event_scheduler_handlers;

int swow_event_module_init(INIT_FUNC_ARGS);
int swow_event_runtime_init(INIT_FUNC_ARGS);
int swow_event_runtime_shutdown(SHUTDOWN_FUNC_ARGS);
int swow_event_delay_runtime_shutdown(void);

#endif    /* SWOW_EVENT_H */
