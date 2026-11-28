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

#include "swow_watchdog.h"

#include "swow_debug.h"    /* for trace functions */
#include "cat_time.h"      /* for time_wait() */
#include "cat_thread.h"    /* for the thread id of the status */

#include "zend_generators.h" /* for zend_generator_check_placeholder_frame() */
#include "zend_builtin_functions.h" /* for DEBUG_BACKTRACE_IGNORE_ARGS */

#ifndef PHP_WIN32
#include <sys/time.h>
#else
#include "win32/time.h"
#endif

SWOW_API zend_class_entry *swow_watchdog_ce;

SWOW_API zend_class_entry *swow_watchdog_exception_ce;

static swow_interrupt_function_t original_zend_interrupt_function = (swow_interrupt_function_t) -1;

#define SWOW_WATCHDOG_DEFAULT_BACKOFF_MAX   500
#define SWOW_WATCHDOG_DEFAULT_EPISODE_GAP   2000
#define SWOW_WATCHDOG_DEFAULT_RECENT_EVENTS 32
#define SWOW_WATCHDOG_DEFAULT_TRACE_DEPTH   64

/* consecutive alerts of one coroutine and one blocking type */
typedef struct swow_watchdog_episode_s {
    const char *type;
    zend_long id;
    zend_long count;
    /* wall clock (ns since the epoch) of the first alert, for reports */
    uint64_t started_at;
    /* uv_hrtime() of the latest alert, for the gap */
    uint64_t last_at;
} swow_watchdog_episode_t;

/* thread status registry */

static uv_mutex_t swow_watchdog_registry_lock;
static cat_queue_t swow_watchdog_registry;

static uint64_t swow_watchdog_wall_time_ns(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) tv.tv_sec * 1000000000 + (uint64_t) tv.tv_usec * 1000;
}

static void swow_watchdog_publish_text(cat_atomic_uint32_t *seq, char *storage, size_t size, const char *text)
{
    size_t length = strlen(text);
    if (length >= size) {
        length = size - 1;
    }
    cat_atomic_uint32_fetch_add(seq, 1);
    memcpy(storage, text, length);
    storage[length] = '\0';
    cat_atomic_uint32_fetch_add(seq, 1);
}

static cat_bool_t swow_watchdog_read_text(cat_atomic_uint32_t *seq, const char *storage, char *buffer, size_t size)
{
    for (int attempt = 0; attempt < 4; attempt++) {
        uint32_t before = cat_atomic_uint32_load(seq);
        if ((before & 1) != 0) {
            continue;
        }
        memcpy(buffer, storage, size);
        buffer[size - 1] = '\0';
        if (cat_atomic_uint32_load(seq) == before) {
            return cat_true;
        }
    }

    return cat_false;
}

static zend_long swow_watchdog_current_thread_id(void)
{
#ifdef ZTS
# ifdef CAT_OS_WIN
    return (zend_long) GetCurrentThreadId();
# else
    return (zend_long) cat_thread_get_current_id();
# endif
#else
    return 0;
#endif
}

static void swow_watchdog_status_register(void)
{
    swow_watchdog_thread_status_t *status = cat_sys_calloc(1, sizeof(*status));

    if (status == NULL) {
        SWOW_G(observe.thread_status) = NULL;
        return;
    }
    status->thread_id = swow_watchdog_current_thread_id();
    status->pid = (zend_long) uv_os_getpid();
#ifdef ZTS
    status->is_main = tsrm_is_main_thread();
#else
    status->is_main = cat_true;
#endif
    cat_atomic_bool_init(&status->running, cat_false);
    cat_atomic_uint64_init(&status->quantum, 0);
    cat_atomic_uint64_init(&status->threshold, 0);
    cat_atomic_uint64_init(&status->block_start, 0);
    cat_atomic_uint64_init(&status->last_alert_at, 0);
    cat_atomic_uint64_init(&status->coroutine_id, 0);
    cat_atomic_uint64_init(&status->alert_count, 0);
    cat_atomic_bool_init(&status->syscall, cat_false);
    cat_atomic_uint32_init(&status->location_seq, 0);
    status->location[0] = '\0';

    uv_mutex_lock(&swow_watchdog_registry_lock);
    cat_queue_push_back(&swow_watchdog_registry, &status->node);
    uv_mutex_unlock(&swow_watchdog_registry_lock);
    SWOW_G(observe.thread_status) = status;
}

static void swow_watchdog_status_unregister(void)
{
    swow_watchdog_thread_status_t *status = SWOW_G(observe.thread_status);

    if (status == NULL) {
        return;
    }
    uv_mutex_lock(&swow_watchdog_registry_lock);
    cat_queue_remove(&status->node);
    uv_mutex_unlock(&swow_watchdog_registry_lock);
    SWOW_G(observe.thread_status) = NULL;
    cat_sys_free(status);
}

#ifndef CAT_OS_WIN
/* The child of a fork has one thread; the lock may have been held by another
 * thread, and the other entries describe threads the child does not have. */
static void swow_watchdog_registry_atfork_child(void)
{
    (void) uv_mutex_init(&swow_watchdog_registry_lock);
    cat_queue_init(&swow_watchdog_registry);
    if (cat_globals_available() && SWOW_G(observe.thread_status) != NULL) {
        swow_watchdog_thread_status_t *status = SWOW_G(observe.thread_status);
        status->pid = (zend_long) uv_os_getpid();
        cat_queue_push_back(&swow_watchdog_registry, &status->node);
    }
}
#endif

/* A status is blocking while alerts keep coming: the watchdog alerts every
 * quantum during a block, and says nothing when the block ends. */
static void swow_watchdog_fill_status(zval *z_status, swow_watchdog_thread_status_t *status, uint64_t now)
{
    zval z_watchdog, z_blocking;
    cat_bool_t running = cat_atomic_bool_load(&status->running);
    uint64_t quantum = cat_atomic_uint64_load(&status->quantum);
    uint64_t block_start = cat_atomic_uint64_load(&status->block_start);
    uint64_t last_alert_at = cat_atomic_uint64_load(&status->last_alert_at);

    array_init(z_status);
    add_assoc_long(z_status, "threadId", status->thread_id);
    add_assoc_bool(z_status, "isMain", status->is_main);
    add_assoc_long(z_status, "pid", status->pid);
    add_assoc_bool(z_status, "isCurrent", status == SWOW_G(observe.thread_status));
    array_init(&z_watchdog);
    add_assoc_bool(&z_watchdog, "running", running);
    add_assoc_long(&z_watchdog, "quantumNs", (zend_long) quantum);
    add_assoc_long(&z_watchdog, "thresholdNs", (zend_long) cat_atomic_uint64_load(&status->threshold));
    add_assoc_zval(z_status, "watchdog", &z_watchdog);
    if (running && block_start != 0 && last_alert_at != 0 && now - last_alert_at <= quantum * 2) {
        char location[sizeof(status->location)];
        array_init(&z_blocking);
        add_assoc_string(&z_blocking, "type", cat_atomic_bool_load(&status->syscall) ? SWOW_WATCHDOG_BLOCKING_TYPE_SYSCALL : SWOW_WATCHDOG_BLOCKING_TYPE_CPU);
        add_assoc_long(&z_blocking, "coroutineId", (zend_long) cat_atomic_uint64_load(&status->coroutine_id));
        add_assoc_long(&z_blocking, "durationNs", (zend_long) (now > block_start ? now - block_start : 0));
        add_assoc_long(&z_blocking, "alertCount", (zend_long) cat_atomic_uint64_load(&status->alert_count));
        if (swow_watchdog_read_text(&status->location_seq, status->location, location, sizeof(location)) && location[0] != '\0') {
            add_assoc_string(&z_blocking, "lastKnownLocation", location);
        } else {
            add_assoc_null(&z_blocking, "lastKnownLocation");
        }
        add_assoc_zval(z_status, "blocking", &z_blocking);
    } else {
        add_assoc_null(z_status, "blocking");
    }
}

SWOW_API void swow_watchdog_get_thread_statuses(zval *z_statuses)
{
    uint64_t now = uv_hrtime();

    array_init(z_statuses);
    uv_mutex_lock(&swow_watchdog_registry_lock);
    CAT_QUEUE_FOREACH_DATA_START(&swow_watchdog_registry, swow_watchdog_thread_status_t, node, status) {
        zval z_status;
        swow_watchdog_fill_status(&z_status, status, now);
        add_next_index_zval(z_statuses, &z_status);
    } CAT_QUEUE_FOREACH_DATA_END();
    uv_mutex_unlock(&swow_watchdog_registry_lock);
}

/* Capture the current execution location. This runs on the watched thread, so
 * walking the execute stack is safe here; the watchdog thread only reads the
 * resulting text. */
static void swow_watchdog_capture_execute_location(swow_watchdog_t *s_watchdog)
{
    zend_execute_data *execute_data = EG(current_execute_data);
    const char *filename = NULL;
    uint32_t lineno = 0;
    const char *function_name = NULL;
    const char *class_name = NULL;
    char line[sizeof(s_watchdog->location)];
    int length;

    for (zend_execute_data *frame = execute_data; frame != NULL; frame = frame->prev_execute_data) {
        zend_function *func = frame->func;
        if (func == NULL || !ZEND_USER_CODE(func->common.type)) {
            continue;
        }
        const zend_op *opline = frame->opline;
        if (opline == NULL || func->op_array.filename == NULL) {
            continue;
        }
        filename = ZSTR_VAL(func->op_array.filename);
        lineno = opline->lineno;
        if (func->common.function_name != NULL) {
            function_name = ZSTR_VAL(func->common.function_name);
            if (func->common.scope != NULL && func->common.scope->name != NULL) {
                class_name = ZSTR_VAL(func->common.scope->name);
            }
        }
        break;
    }

    if (filename == NULL) {
        length = snprintf(line, sizeof(line), "unknown location");
    } else if (function_name == NULL) {
        length = snprintf(line, sizeof(line), "%s:%u", filename, lineno);
    } else if (class_name == NULL) {
        length = snprintf(line, sizeof(line), "%s() at %s:%u", function_name, filename, lineno);
    } else {
        length = snprintf(line, sizeof(line), "%s::%s() at %s:%u", class_name, function_name, filename, lineno);
    }
    if (length < 0) {
        return;
    }
    if ((size_t) length >= sizeof(line)) {
        length = (int) sizeof(line) - 1;
    }

    /* Publish the text with a version counter, so a reader never sees a
     * half-written snapshot */
    cat_atomic_uint32_fetch_add(&s_watchdog->location_seq, 1);
    memcpy(s_watchdog->location, line, (size_t) length);
    s_watchdog->location[length] = '\0';
    cat_atomic_uint32_fetch_add(&s_watchdog->location_seq, 1);
}

static cat_bool_t swow_watchdog_read_execute_location(swow_watchdog_t *s_watchdog, char *buffer, size_t size)
{
    return swow_watchdog_read_text(&s_watchdog->location_seq, s_watchdog->location, buffer, size);
}

SWOW_API void swow_watchdog_alert_standard(cat_watchdog_t *watchdog)
{
    swow_watchdog_t *s_watchdog = swow_watchdog_get_from_handle(watchdog);
    cat_bool_t vm_interrupted;

    // CPU starvation (and we should try to schedule the coroutine)

    vm_interrupted = cat_atomic_bool_exchange(&s_watchdog->vm_interrupted, cat_false);
    swow_atomic_bool_store(s_watchdog->vm_interrupt_ptr, 1);

    /* publish the block for the other threads; the coroutine is read the
     * same unsynchronized way as the counters of the watchdog loop */
    if (s_watchdog->status != NULL) {
        swow_watchdog_thread_status_t *status = s_watchdog->status;
        cat_coroutine_t *blocked_coroutine = watchdog->globals->current;
        if (watchdog->alert_count == 1) {
            cat_atomic_bool_store(&status->syscall, cat_false);
        }
        cat_atomic_uint64_store(&status->block_start, cat_atomic_uint64_load(&watchdog->block_start));
        cat_atomic_uint64_store(&status->coroutine_id, blocked_coroutine != NULL ? blocked_coroutine->id : 0);
        cat_atomic_uint64_store(&status->alert_count, watchdog->alert_count);
        cat_atomic_uint64_store(&status->last_alert_at, uv_hrtime());
    }

    if (
        watchdog->alert_count > 1 &&
        vm_interrupted == 0 /* interrupt maybe failed */
    ) {
        if (
            watchdog->threshold > 0 && /* blocking time is greater than syscall threshold */
            ((cat_timeout_t) (watchdog->quantum * watchdog->alert_count)) > watchdog->threshold
        ) {
            /* Syscall blocking
             * CPU starvation is also possible,
             * the machine performance is too bad (such as mine),
             * VM has not interrupted yet */

            /* The watched thread captured its location; this thread only reads
             * the captured text, never the execute stack itself */
            char location[sizeof(s_watchdog->location)];
            cat_bool_t has_location = swow_watchdog_read_execute_location(
                s_watchdog,
                location,
                sizeof(location)
            );
            cat_coroutine_t *current_coroutine = watchdog->globals->current;

            /* Output standard warning first */
            cat_watchdog_alert_standard(watchdog);

            /* Output detailed location info for debugging */
            if (current_coroutine != NULL) {
                fprintf(stderr,
                    "         Coroutine: #" CAT_COROUTINE_ID_FMT " (total: " CAT_COROUTINE_COUNT_FMT ")\n",
                    current_coroutine->id, watchdog->globals->count);
            }

            if (has_location && location[0] != '\0') {
                fprintf(stderr, "         Location: %s\n", location);
            } else {
                fprintf(stderr, "         Unable to read execute location (coroutine may be idle or in C extension)\n");
            }

            if (s_watchdog->status != NULL) {
                cat_atomic_bool_store(&s_watchdog->status->syscall, cat_true);
                swow_watchdog_publish_text(&s_watchdog->status->location_seq, s_watchdog->status->location,
                    sizeof(s_watchdog->status->location), has_location ? location : "");
            }

            /* Set flag to report the event when syscall returns and VM resumes;
             * keep the start of the block, the watchdog forgets it once the
             * coroutine switches again */
            if (!cat_atomic_bool_exchange(&s_watchdog->syscall_blocked, cat_true)) {
                cat_atomic_uint64_store(&s_watchdog->syscall_block_start, cat_atomic_uint64_load(&watchdog->block_start));
            }
        }
    }
}

/* Check if execution context is safe for functions that access call stack (like debug_backtrace)
 * Returns true if safe, false if execution context is incomplete (e.g., just returned from FFI call) */
static cat_bool_t swow_watchdog_is_backtrace_safe(void)
{
    zend_execute_data *call = EG(current_execute_data);

    if (!call) {
        return cat_false;
    }

    /* Check a few frames to ensure func pointers are valid
     * This prevents assertion failure in zend_fetch_debug_backtrace when
     * returning from FFI calls where execute_data->func may be NULL */
    int check_depth = 0;
    while (call && check_depth < 3) {
        if (!call->func) {
            /* Check if this is a generator placeholder frame */
            zend_execute_data *checked = zend_generator_check_placeholder_frame(call);
            if (!checked->func) {
                /* Not a generator frame and func is still NULL - unsafe for backtrace */
                return cat_false;
            }
        }

        call = call->prev_execute_data;
        check_depth++;
    }

    return cat_true;
}

static void swow_watchdog_episode_dtor(zval *z_episode)
{
    efree(Z_PTR_P(z_episode));
}

/* Count the alert in the episode of the current coroutine, and drop the
 * episodes that ended (no alert within the gap). */
static swow_watchdog_episode_t *swow_watchdog_update_episode(swow_watchdog_t *s_watchdog, const char *type)
{
    cat_coroutine_id_t id = CAT_COROUTINE_G(current)->id;
    uint64_t now = uv_hrtime();
    swow_watchdog_episode_t *episode;

    zend_ulong episode_key;
    /* deleting the current element during the iteration is allowed */
    ZEND_HASH_FOREACH_NUM_KEY_PTR(&s_watchdog->episodes, episode_key, episode) {
        if (now - episode->last_at > s_watchdog->episode_gap) {
            zend_hash_index_del(&s_watchdog->episodes, episode_key);
        }
    } ZEND_HASH_FOREACH_END();

    episode = zend_hash_index_find_ptr(&s_watchdog->episodes, id);
    if (episode == NULL || episode->type != type) {
        if (episode == NULL) {
            episode = emalloc(sizeof(*episode));
            zend_hash_index_add_new_ptr(&s_watchdog->episodes, id, episode);
        }
        episode->type = type;
        episode->id = ++s_watchdog->last_episode_id;
        episode->count = 0;
        episode->started_at = swow_watchdog_wall_time_ns();
    }
    episode->count++;
    episode->last_at = now;

    return episode;
}

static cat_timeout_t swow_watchdog_backoff(const swow_watchdog_t *s_watchdog, zend_long count)
{
    cat_timeout_t delay = s_watchdog->backoff_base;

    for (zend_long i = 1; i < count && delay < s_watchdog->backoff_max; i++) {
        delay *= 2;
    }
    return delay < s_watchdog->backoff_max ? delay : s_watchdog->backoff_max;
}

/*
 * Build the event of an alert on the watched thread, keep it in the ring of
 * recent events and return it (one reference owned by the caller).
 *
 * | type    | delivery  | trace taken                                       |
 * | cpu     | immediate | at the blocking code                              |
 * | syscall | deferred  | right after the blocking call returned, its frame |
 * |         |           | is still on top                                   |
 */
static void swow_watchdog_build_event(swow_watchdog_t *s_watchdog, zval *z_event, const char *type, cat_bool_t deferred, uint64_t block_start)
{
    swow_watchdog_episode_t *episode = swow_watchdog_update_episode(s_watchdog, type);
    uint64_t now = uv_hrtime();
    zval z_episode, z_trace;
    char location[sizeof(s_watchdog->location)];

    array_init(z_event);
    add_assoc_long(z_event, "seq", ++s_watchdog->last_event_seq);
    add_assoc_string(z_event, "type", type);
    add_assoc_string(z_event, "delivery", deferred ? "deferred" : "immediate");
    add_assoc_double(z_event, "at", (double) swow_watchdog_wall_time_ns() / 1e9);
    add_assoc_long(z_event, "pid", (zend_long) s_watchdog->watchdog.pid);
    add_assoc_long(z_event, "threadId", swow_watchdog_current_thread_id());
    add_assoc_long(z_event, "coroutineId", (zend_long) CAT_COROUTINE_G(current)->id);
    add_assoc_long(z_event, "blockedNs", (zend_long) (block_start != 0 && now > block_start ? now - block_start : 0));
    array_init(&z_episode);
    add_assoc_long(&z_episode, "id", episode->id);
    add_assoc_long(&z_episode, "count", episode->count);
    add_assoc_long(&z_episode, "startedAt", (zend_long) episode->started_at);
    add_assoc_zval(z_event, "episode", &z_episode);
    add_assoc_long(z_event, "quantumNs", (zend_long) s_watchdog->watchdog.quantum);
    add_assoc_long(z_event, "thresholdNs", (zend_long) s_watchdog->watchdog.threshold);
    add_assoc_long(z_event, "backoffNs", (zend_long) swow_watchdog_backoff(s_watchdog, episode->count) * 1000 * 1000);
    if (deferred && swow_watchdog_read_execute_location(s_watchdog, location, sizeof(location)) && location[0] != '\0') {
        add_assoc_string(z_event, "lastKnownLocation", location);
    } else {
        add_assoc_null(z_event, "lastKnownLocation");
    }
    ZVAL_ARR(&z_trace, swow_debug_get_trace(DEBUG_BACKTRACE_IGNORE_ARGS, s_watchdog->trace_depth));
    add_assoc_zval(z_event, "trace", &z_trace);

    if (s_watchdog->recent_limit > 0) {
        HashTable *recent = Z_ARRVAL(s_watchdog->recent);
        while (zend_hash_num_elements(recent) >= (uint32_t) s_watchdog->recent_limit) {
            zend_hash_internal_pointer_reset(recent);
            zend_ulong index;
            if (zend_hash_get_current_key(recent, NULL, &index) != HASH_KEY_IS_LONG) {
                break;
            }
            zend_hash_index_del(recent, index);
        }
        Z_ADDREF_P(z_event);
        zend_hash_next_index_insert(recent, z_event);
    }
}

/* Call user alerter with the blocking type and the event */
static void swow_watchdog_call_alerter(swow_watchdog_t *s_watchdog, const char *blocking_type, zval *z_event)
{
    if (s_watchdog->alerter.function_handler == NULL) {
        return;
    }

    zend_fcall_info fci = empty_fcall_info;
    zval retval, z_params[2];

    ZVAL_UNDEF(&retval);
    fci.size = sizeof(fci);
    fci.param_count = 2;
    fci.params = z_params;
    fci.retval = &retval;

    ZVAL_STRING(&z_params[0], blocking_type);
    ZVAL_COPY(&z_params[1], z_event);

    /* Save and clear vm_interrupt to prevent being triggered again in PHP alerter function */
    bool original_vm_interrupt = swow_atomic_bool_exchange(s_watchdog->vm_interrupt_ptr, 0);

    (void) zend_call_function(&fci, &s_watchdog->alerter);

    /* Restore original vm_interrupt value in case it was set by other reasons */
    swow_atomic_bool_store(s_watchdog->vm_interrupt_ptr, original_vm_interrupt);

    zval_ptr_dtor(&retval);
    zval_ptr_dtor(&z_params[0]);
    zval_ptr_dtor(&z_params[1]);
}

/* Back off after an alert, unless the alerter threw or killed the coroutine. */
static void swow_watchdog_apply_backoff(swow_watchdog_t *s_watchdog, zval *z_event)
{
    zval *z_backoff;

    if (s_watchdog->alerter.function_handler != NULL && !s_watchdog->backoff_with_alerter) {
        return;
    }
    if (EG(exception) != NULL) {
        return;
    }
    z_backoff = zend_hash_str_find(Z_ARRVAL_P(z_event), ZEND_STRL("backoffNs"));
    if (!cat_time_wait((cat_timeout_t) (Z_LVAL_P(z_backoff) / (1000 * 1000))) &&
        cat_get_last_error_code() != CAT_ETIMEDOUT) {
        CAT_CORE_ERROR_WITH_LAST(WATCH_DOG, "Watchdog interrupt schedule failed");
    }
}

static void swow_watchdog_interrupt_function(zend_execute_data *execute_data)
{
    if (cat_watchdog_is_running()) {
        swow_watchdog_t *s_watchdog = swow_watchdog_get_current();
        cat_watchdog_t *watchdog = &s_watchdog->watchdog;

        /* Check if syscall blocking was detected and VM just resumed */
        if (cat_atomic_bool_load(&s_watchdog->syscall_blocked)) {
            /* Syscall blocking detected, check if execution context is safe for alerter callback */
            if (swow_watchdog_is_backtrace_safe()) {
                /* Execution context is now safe, clear flag and report */
                zval z_event;
                cat_atomic_bool_store(&s_watchdog->syscall_blocked, cat_false);
                swow_watchdog_build_event(s_watchdog, &z_event, SWOW_WATCHDOG_BLOCKING_TYPE_SYSCALL, cat_true,
                    cat_atomic_uint64_load(&s_watchdog->syscall_block_start));
                swow_watchdog_call_alerter(s_watchdog, SWOW_WATCHDOG_BLOCKING_TYPE_SYSCALL, &z_event);
                swow_watchdog_apply_backoff(s_watchdog, &z_event);
                zval_ptr_dtor(&z_event);
                goto _end;
            } else {
                /* Execution context still incomplete (e.g., just returned from FFI),
                 * keep flag set and trigger another interrupt to retry later */
                swow_atomic_bool_store(s_watchdog->vm_interrupt_ptr, 1);
                goto _end;
            }
        }

        cat_atomic_bool_store(&s_watchdog->vm_interrupted, cat_true);
        swow_watchdog_capture_execute_location(s_watchdog);
        /* re-check if current switches still equal to last_switches  */
        if (CAT_COROUTINE_G(switches) == watchdog->last_switches) {
            /* CPU blocking detected: report, then back off (always without an
             * alerter, only when asked for with one) */
            zval z_event;
            swow_watchdog_build_event(s_watchdog, &z_event, SWOW_WATCHDOG_BLOCKING_TYPE_CPU, cat_false,
                cat_atomic_uint64_load(&watchdog->block_start));
            swow_watchdog_call_alerter(s_watchdog, SWOW_WATCHDOG_BLOCKING_TYPE_CPU, &z_event);
            swow_watchdog_apply_backoff(s_watchdog, &z_event);
            zval_ptr_dtor(&z_event);
        }
    }

_end:
    if (original_zend_interrupt_function != NULL) {
        original_zend_interrupt_function(execute_data);
    }
}

/* Reads a non-negative integer option, returns false with an error otherwise */
static cat_bool_t swow_watchdog_option_long(HashTable *options, const char *name, zend_long *value)
{
    zval *z_value = zend_hash_str_find(options, name, strlen(name));

    if (z_value == NULL) {
        return cat_true;
    }
    if (Z_TYPE_P(z_value) != IS_LONG || Z_LVAL_P(z_value) < 0) {
        cat_update_last_error(CAT_EINVAL, "Watchdog option '%s' must be a non-negative integer", name);
        return cat_false;
    }
    *value = Z_LVAL_P(z_value);
    return cat_true;
}

/*
 * | alerter  | after an alert                                               |
 * | null     | back off from the next event loop round (base 0)             |
 * | numeric  | back off exponentially from that many milliseconds           |
 * | callable | call it with (type, event); back off only with backoffBase   |
 *
 * options (milliseconds): backoffBase, backoffMax (500), episodeGap (2000);
 * recentEvents (32) events are kept for getRecentEvents(); traceDepth (64).
 */
SWOW_API cat_bool_t swow_watchdog_run(cat_timeout_t quantum, cat_timeout_t threshold, zval *z_alerter, HashTable *options)
{
    swow_watchdog_t *s_watchdog;
    zend_fcall_info_cache fcc = empty_fcall_info_cache;
    zend_long backoff_base = 0;
    zend_long backoff_max = SWOW_WATCHDOG_DEFAULT_BACKOFF_MAX;
    zend_long episode_gap = SWOW_WATCHDOG_DEFAULT_EPISODE_GAP;
    zend_long recent_limit = SWOW_WATCHDOG_DEFAULT_RECENT_EVENTS;
    zend_long trace_depth = SWOW_WATCHDOG_DEFAULT_TRACE_DEPTH;
    cat_bool_t backoff_with_alerter = cat_false;
    cat_bool_t ret;

    if (z_alerter != NULL) {
        switch (Z_TYPE_P(z_alerter)) {
            case IS_NULL:
            case IS_LONG:
            case IS_DOUBLE:
                backoff_base = zval_get_long(z_alerter);
                z_alerter = NULL;
                break;
            default: {
                char *error;
                if (!zend_is_callable_ex(z_alerter, NULL, 0, NULL, &fcc, &error)) {
                    cat_update_last_error(CAT_EMISUSE, "Watchdog alerter must be numeric or callable, %s", error);
                    efree(error);
                    return cat_false;
                }
                efree(error);
            }
        }
    }
    if (options != NULL) {
        zend_string *key;
        ZEND_HASH_FOREACH_STR_KEY(options, key) {
            if (key == NULL || (
                !zend_string_equals_literal(key, "backoffBase") && !zend_string_equals_literal(key, "backoffMax") &&
                !zend_string_equals_literal(key, "episodeGap") && !zend_string_equals_literal(key, "recentEvents") &&
                !zend_string_equals_literal(key, "traceDepth"))) {
                cat_update_last_error(CAT_EINVAL, "Unknown watchdog option '%s'", key != NULL ? ZSTR_VAL(key) : "(int)");
                return cat_false;
            }
        } ZEND_HASH_FOREACH_END();
        if (zend_hash_str_exists(options, ZEND_STRL("backoffBase"))) {
            backoff_with_alerter = cat_true;
        }
        if (!swow_watchdog_option_long(options, "backoffBase", &backoff_base) ||
            !swow_watchdog_option_long(options, "backoffMax", &backoff_max) ||
            !swow_watchdog_option_long(options, "episodeGap", &episode_gap) ||
            !swow_watchdog_option_long(options, "recentEvents", &recent_limit) ||
            !swow_watchdog_option_long(options, "traceDepth", &trace_depth)) {
            return cat_false;
        }
    }
    if (backoff_base < 0) {
        backoff_base = 0;
    }

    s_watchdog = (swow_watchdog_t *) emalloc(sizeof(*s_watchdog));
    cat_atomic_bool_init(&s_watchdog->vm_interrupted, cat_false);
    s_watchdog->vm_interrupt_ptr = &EG(vm_interrupt);
    cat_atomic_uint32_store(&s_watchdog->location_seq, 0);
    s_watchdog->location[0] = '\0';
    cat_atomic_bool_init(&s_watchdog->syscall_blocked, cat_false);
    cat_atomic_uint64_init(&s_watchdog->syscall_block_start, 0);
    s_watchdog->backoff_base = (cat_timeout_t) backoff_base;
    s_watchdog->backoff_max = (cat_timeout_t) backoff_max;
    s_watchdog->backoff_with_alerter = backoff_with_alerter;
    s_watchdog->episode_gap = (uint64_t) episode_gap * 1000 * 1000;
    zend_hash_init(&s_watchdog->episodes, 8, NULL, swow_watchdog_episode_dtor, 0);
    s_watchdog->last_episode_id = 0;
    s_watchdog->last_event_seq = 0;
    array_init(&s_watchdog->recent);
    s_watchdog->recent_limit = recent_limit;
    s_watchdog->trace_depth = trace_depth;
    s_watchdog->status = SWOW_G(observe.thread_status);
    s_watchdog->alerter = fcc;
    if (z_alerter != NULL) {
        ZVAL_COPY(&s_watchdog->z_alerter, z_alerter);
    } else {
        ZVAL_NULL(&s_watchdog->z_alerter);
    }

    ret = cat_watchdog_run(&s_watchdog->watchdog, quantum, threshold, swow_watchdog_alert_standard);

    if (!ret) {
        Z_TRY_DELREF(s_watchdog->z_alerter);
        zend_hash_destroy(&s_watchdog->episodes);
        zval_ptr_dtor(&s_watchdog->recent);
        efree(s_watchdog);
        return cat_false;
    }

    if (s_watchdog->status != NULL) {
        cat_atomic_uint64_store(&s_watchdog->status->quantum, (uint64_t) s_watchdog->watchdog.quantum);
        cat_atomic_uint64_store(&s_watchdog->status->threshold, (uint64_t) (s_watchdog->watchdog.threshold > 0 ? s_watchdog->watchdog.threshold : 0));
        cat_atomic_uint64_store(&s_watchdog->status->block_start, 0);
        cat_atomic_bool_store(&s_watchdog->status->running, cat_true);
    }

    return cat_true;
}

SWOW_API cat_bool_t swow_watchdog_stop(void)
{
    swow_watchdog_t *s_watchdog = swow_watchdog_get_current();
    cat_bool_t ret;

    ret = cat_watchdog_stop();

    if (!ret) {
        return cat_false;
    }

    if (s_watchdog->status != NULL) {
        cat_atomic_bool_store(&s_watchdog->status->running, cat_false);
    }
    zval_ptr_dtor(&s_watchdog->z_alerter);
    zend_hash_destroy(&s_watchdog->episodes);
    zval_ptr_dtor(&s_watchdog->recent);
    efree(s_watchdog);

    return cat_true;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Watchdog_run, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, quantum, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, threshold, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_MASK(0, alerter, MAY_BE_CALLABLE | MAY_BE_LONG | MAY_BE_DOUBLE | MAY_BE_NULL, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Watchdog, run)
{
    zend_long quantum = 0;
    zend_long threshold = 0;
    zval *z_alerter = NULL;
    HashTable *options = NULL;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 4)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(quantum)
        Z_PARAM_LONG(threshold)
        Z_PARAM_ZVAL(z_alerter)
        Z_PARAM_ARRAY_HT(options)
    ZEND_PARSE_PARAMETERS_END();

    if (original_zend_interrupt_function == (swow_interrupt_function_t) -1) {
        original_zend_interrupt_function = zend_interrupt_function;
        zend_interrupt_function = swow_watchdog_interrupt_function;
    }

    ret = swow_watchdog_run(quantum, threshold, z_alerter, options);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_watchdog_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Watchdog_stop, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Watchdog, stop)
{
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_NONE();

    ret = swow_watchdog_stop();

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_watchdog_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Watchdog_isRunning, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Watchdog, isRunning)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(cat_watchdog_is_running());
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Watchdog_getRecentEvents, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

/* The latest events of the running watchdog, oldest first, [] when it is not running */
static PHP_METHOD(Swow_Watchdog, getRecentEvents)
{
    swow_watchdog_t *s_watchdog = swow_watchdog_get_current();

    ZEND_PARSE_PARAMETERS_NONE();

    if (s_watchdog == NULL) {
        RETURN_EMPTY_ARRAY();
    }
    zval *z_event;
    array_init_size(return_value, zend_hash_num_elements(Z_ARRVAL(s_watchdog->recent)));
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL(s_watchdog->recent), z_event) {
        Z_ADDREF_P(z_event);
        add_next_index_zval(return_value, z_event);
    } ZEND_HASH_FOREACH_END();
}

static const zend_function_entry swow_watchdog_methods[] = {
    PHP_ME(Swow_Watchdog, run,       arginfo_class_Swow_Watchdog_run,       ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Watchdog, stop,      arginfo_class_Swow_Watchdog_stop,      ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Watchdog, isRunning, arginfo_class_Swow_Watchdog_isRunning, ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Watchdog, getRecentEvents, arginfo_class_Swow_Watchdog_getRecentEvents, ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_watchdog_module_init(INIT_FUNC_ARGS)
{
    if (!cat_watchdog_module_init()) {
        return FAILURE;
    }

    swow_watchdog_ce = swow_register_internal_class(
        "Swow\\Watchdog", NULL, swow_watchdog_methods,
        NULL, NULL, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );

    swow_watchdog_exception_ce = swow_register_internal_class(
        "Swow\\WatchdogException", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    if (uv_mutex_init(&swow_watchdog_registry_lock) != 0) {
        return FAILURE;
    }
    cat_queue_init(&swow_watchdog_registry);
#ifndef CAT_OS_WIN
    if (pthread_atfork(NULL, NULL, swow_watchdog_registry_atfork_child) != 0) {
        return FAILURE;
    }
#endif

    return SUCCESS;
}

zend_result swow_watchdog_module_shutdown(INIT_FUNC_ARGS)
{
    if (!cat_watchdog_module_shutdown()) {
        return FAILURE;
    }
    uv_mutex_destroy(&swow_watchdog_registry_lock);

    return SUCCESS;
}

zend_result swow_watchdog_runtime_init(INIT_FUNC_ARGS)
{
    if (!cat_watchdog_runtime_init()) {
        return FAILURE;
    }
    swow_watchdog_status_register();

    return SUCCESS;
}

zend_result swow_watchdog_runtime_shutdown(INIT_FUNC_ARGS)
{
    if (cat_watchdog_is_running() && !swow_watchdog_stop()) {
        CAT_CORE_ERROR_WITH_LAST(WATCH_DOG, "Watchdog stop failed");
    }
    /* after the stop, so the watchdog thread no longer writes the entry */
    swow_watchdog_status_unregister();

    if (!cat_watchdog_runtime_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}
