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
#include "cat_atomic.h"

/* Blocking type constants for alerter callback */
#define SWOW_WATCHDOG_BLOCKING_TYPE_CPU     "cpu"
#define SWOW_WATCHDOG_BLOCKING_TYPE_SYSCALL "syscall"

extern SWOW_API zend_class_entry *swow_watchdog_ce;

extern SWOW_API zend_class_entry *swow_watchdog_exception_ce;

typedef struct swow_watchdog_s {
    /* Underlying watchdog handle, runs in separate thread to monitor coroutine switches */
    cat_watchdog_t watchdog;
    /* VM interrupt flag: used to distinguish CPU blocking (interruptible) from syscall blocking (non-interruptible) */
    cat_atomic_bool_t vm_interrupted;
    /* Pointer to EG(vm_interrupt), used to trigger PHP VM interrupt */
    zend_atomic_bool *vm_interrupt_ptr;
    /* Pointer to executor_globals for cross-thread reading of execution location from watchdog thread
     * Note: race condition exists, read info may be inaccurate but should not crash */
    zend_executor_globals *executor_globals;
    /* Flag indicating syscall blocking was detected, will be checked when VM resumes */
    cat_atomic_bool_t syscall_blocked;
    /* Delay scheduling time (nanoseconds) on CPU blocking, 0 means schedule immediately */
    cat_timeout_t delay;
    /* User-defined alerter zval reference (for lifetime management) */
    zval z_alerter;
    /* User-defined alerter call info cache (only used in CPU blocking scenario) */
    zend_fcall_info_cache alerter;
} swow_watchdog_t;

/* loader */

zend_result swow_watchdog_module_init(INIT_FUNC_ARGS);
zend_result swow_watchdog_module_shutdown(INIT_FUNC_ARGS);
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

SWOW_API cat_bool_t swow_watchdog_run(cat_timeout_t quantum, cat_timeout_t threshold, zval *z_alerter);
SWOW_API cat_bool_t swow_watchdog_stop(void);

SWOW_API void swow_watchdog_alert_standard(cat_watchdog_t *watchdog);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_WATCH_DOG_H */
