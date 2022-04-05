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

#ifndef SWOW_WATCH_DOG_H
#define SWOW_WATCH_DOG_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_watchdog.h"

extern SWOW_API zend_class_entry *swow_watchdog_ce;

extern SWOW_API zend_class_entry *swow_watchdog_exception_ce;

typedef struct swow_watchdog_s {
    cat_watchdog_t watchdog;
    zend_bool vm_interrupted;
    zend_bool *vm_interrupt_ptr;
    cat_timeout_t delay;
    zval zalerter;
    zend_fcall_info_cache alerter;
} swow_watchdog_t;

/* loader */

zend_result swow_watchdog_module_init(INIT_FUNC_ARGS);
zend_result swow_watchdog_runtime_init(INIT_FUNC_ARGS);
zend_result swow_watchdog_runtime_shutdown(INIT_FUNC_ARGS);

/* helper */

static zend_always_inline swow_watchdog_t *swow_watchdog_get_from_handle(cat_watchdog_t *watchdog)
{
    return cat_container_of(watchdog, swow_watchdog_t, watchdog);
}

static zend_always_inline swow_watchdog_t *swow_watchdog_get_current(void)
{
    cat_watchdog_t *watchdog = CAT_WATCH_DOG_G(watchdog);
    return watchdog != NULL ? swow_watchdog_get_from_handle(watchdog) : NULL;
}

/* APIs */

SWOW_API cat_bool_t swow_watchdog_run(cat_timeout_t quantum, cat_timeout_t threshold, zval *zalerter);
SWOW_API cat_bool_t swow_watchdog_stop(void);

SWOW_API void swow_watchdog_alert_standard(cat_watchdog_t *watchdog);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_WATCH_DOG_H */
