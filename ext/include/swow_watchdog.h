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

/*
 * The status of a thread runtime, readable from any thread through
 * Swow\Debug\getThreadStatuses(): when a thread is stuck, its own debugger
 * endpoint can not answer, but another thread can still report what the
 * watchdog of the stuck thread saw. Only atomics and a seqlock-published text
 * cross threads; the registry lock keeps an entry alive while it is read.
 */
typedef struct swow_watchdog_thread_status_s {
    cat_queue_node_t node;
    zend_long thread_id;
    zend_long pid;
    cat_bool_t is_main;
    /* written by the owner thread */
    cat_atomic_bool_t running;
    cat_atomic_uint64_t quantum;
    cat_atomic_uint64_t threshold;
    /* written by the watchdog thread on every alert */
    cat_atomic_uint64_t block_start;
    cat_atomic_uint64_t last_alert_at;
    cat_atomic_uint64_t coroutine_id;
    cat_atomic_uint64_t alert_count;
    cat_atomic_bool_t syscall;
    cat_atomic_uint32_t location_seq;
    char location[512];
} swow_watchdog_thread_status_t;

typedef struct swow_watchdog_s {
    /* Underlying watchdog handle, runs in separate thread to monitor coroutine switches */
    cat_watchdog_t watchdog;
    /* VM interrupt flag: used to distinguish CPU blocking (interruptible) from syscall blocking (non-interruptible) */
    cat_atomic_bool_t vm_interrupted;
    /* Pointer to EG(vm_interrupt), used to trigger PHP VM interrupt */
    swow_atomic_bool *vm_interrupt_ptr;
    /* Snapshot of the execution location, written by the watched thread and
     * read by the watchdog thread: walking the execute stack from another
     * thread is not safe, so the location is captured where it is stable. */
    cat_atomic_uint32_t location_seq;
    char location[512];
    /* Flag indicating syscall blocking was detected, will be checked when VM resumes */
    cat_atomic_bool_t syscall_blocked;
    /* uv_hrtime() of the start of that syscall block, kept for the deferred
     * event because the watchdog clears its own record once switches resume */
    cat_atomic_uint64_t syscall_block_start;
    /* backoff after an alert: min(backoff_max, backoff_base * 2^(count - 1)) ms,
     * where count is the alert count of the episode; base 0 means the next
     * round of the event loop */
    cat_timeout_t backoff_base;
    cat_timeout_t backoff_max;
    /* whether a callable alerter backs off too (only when asked for) */
    cat_bool_t backoff_with_alerter;
    /* alerts of the same coroutine and type closer than this (ns) form one episode */
    uint64_t episode_gap;
    /* coroutine id -> swow_watchdog_episode_t */
    HashTable episodes;
    zend_long last_episode_id;
    zend_long last_event_seq;
    /* the latest events, at most recent_limit, oldest first */
    zval recent;
    zend_long recent_limit;
    zend_long trace_depth;
    /* this thread's entry in the status registry */
    swow_watchdog_thread_status_t *status;
    /* User-defined alerter zval reference (for lifetime management) */
    zval z_alerter;
    /* User-defined alerter call info cache */
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

SWOW_API cat_bool_t swow_watchdog_run(cat_timeout_t quantum, cat_timeout_t threshold, zval *z_alerter, HashTable *options);
SWOW_API cat_bool_t swow_watchdog_stop(void);

SWOW_API void swow_watchdog_alert_standard(cat_watchdog_t *watchdog);

/* fills z_statuses with the status of every thread runtime */
SWOW_API void swow_watchdog_get_thread_statuses(zval *z_statuses);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_WATCH_DOG_H */
