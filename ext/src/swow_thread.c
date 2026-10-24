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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#include "swow_thread.h"

#include "zend_smart_str.h"

#include "TSRM.h"

#if PHP_VERSION_ID >= 80200 && PHP_VERSION_ID < 80700
# include "zend_atomic.h"
#endif


#include "main/php_main.h"
#include "main/SAPI.h"

#include "ext/standard/php_var.h"
#include "php_variables.h"

#ifdef ZTS

SWOW_API zend_class_entry *swow_thread_ce;
SWOW_API zend_class_entry *swow_thread_exception_ce;
SWOW_API zend_object_handlers swow_thread_handlers;

#ifdef CAT_OS_WIN
static int (*swow_thread_original_sapi_deactivate)(void);

static int swow_thread_sapi_deactivate(void)
{
    if (!tsrm_is_main_thread()) {
        fflush(stdout);
        return SUCCESS;
    }

    return swow_thread_original_sapi_deactivate != NULL
        ? swow_thread_original_sapi_deactivate()
        : SUCCESS;
}
#endif

static zend_long swow_thread_id_to_long(cat_thread_t *thread);

static cat_always_inline swow_thread_t *swow_thread_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_thread_t, std);
}

/* run */

static cat_bool_t swow_thread_abandoned_children = cat_false;

cat_bool_t swow_thread_children_abandoned(void)
{
    return swow_thread_abandoned_children;
}

static swow_thread_run_t *swow_thread_run_create(void)
{
    swow_thread_run_t *run = cat_sys_calloc(1, sizeof(*run));

    if (unlikely(run == NULL)) {
        return NULL;
    }

    cat_atomic_uint32_init(&run->refcount, 1);
    cat_atomic_bool_init(&run->killed, cat_false);
    cat_atomic_bool_init(&run->result_invalid, cat_false);
    cat_atomic_int32_init(&run->exit_status, 0);

    if (unlikely(uv_mutex_init(&run->lock) != 0)) {
        cat_sys_free(run);
        return NULL;
    }

    if (unlikely(!cat_sem_init(&run->started, 0))) {
        uv_mutex_destroy(&run->lock);
        cat_sys_free(run);
        return NULL;
    }

    return run;
}

static void swow_thread_run_addref(swow_thread_run_t *run)
{
    cat_atomic_uint32_fetch_add(&run->refcount, 1);
}

static void swow_thread_run_release(swow_thread_run_t *run)
{
    if (cat_atomic_uint32_fetch_sub(&run->refcount, 1) != 1) {
        return;
    }

    cat_sem_destroy(&run->started);
    uv_mutex_destroy(&run->lock);

    if (run->callable != NULL) {
        cat_sys_free((void *) run->callable);
    }
    if (run->args != NULL) {
        cat_sys_free((void *) run->args);
    }
    if (run->code != NULL) {
        cat_sys_free((void *) run->code);
    }
    if (run->result != NULL) {
        cat_sys_free((void *) run->result);
    }
    if (run->exception != NULL) {
        cat_sys_free((void *) run->exception);
    }

    cat_sys_free(run);
}

static cat_bool_t swow_thread_run_kill(swow_thread_run_t *run)
{
    zend_executor_globals *target_eg;
    zend_swow_globals *target_globals;

    uv_mutex_lock(&run->lock);

    target_eg = run->eg;
    target_globals = run->globals;

    if (target_eg == NULL) {
        uv_mutex_unlock(&run->lock);
        return cat_false;
    }

    cat_atomic_bool_store(&run->killed, cat_true);
    cat_atomic_int32_store(&run->exit_status, -1);

    if (target_globals != NULL) {
        cat_atomic_bool_store(&target_globals->thread.kill_requested, cat_true);
    }

#if PHP_VERSION_ID >= 80700
    atomic_store(&target_eg->vm_interrupt, true);
#elif PHP_VERSION_ID >= 80200
    zend_atomic_bool_store(&target_eg->vm_interrupt, true);
#elif defined(CAT_OS_WIN)
    InterlockedExchange8((volatile char *) &target_eg->vm_interrupt, 1);
#else
    __atomic_store_n(&target_eg->vm_interrupt, true, __ATOMIC_SEQ_CST);
#endif

    uv_mutex_unlock(&run->lock);

    return cat_true;
}

static void swow_thread_children_add(swow_thread_run_t *run)
{
    swow_thread_run_addref(run);
    zend_hash_index_add_ptr(&SWOW_G(thread.children), (zend_ulong) (uintptr_t) run, run);
}

static void swow_thread_children_del(swow_thread_run_t *run)
{
    if (zend_hash_index_del(&SWOW_G(thread.children), (zend_ulong) (uintptr_t) run) == SUCCESS) {
        swow_thread_run_release(run);
    }
}

static void swow_thread_children_shutdown(void)
{
    zend_long timeout = SWOW_G(ini.thread_shutdown_timeout);
    cat_bool_t has_deadline = timeout >= 0;
    uint64_t deadline = 0;
    swow_thread_run_t *run;

    if (has_deadline) {
        uint64_t timeout_ns;
        uint64_t now = uv_hrtime();

        if ((uint64_t) timeout > UINT64_MAX / UINT64_C(1000000)) {
            timeout_ns = UINT64_MAX;
        } else {
            timeout_ns = (uint64_t) timeout * UINT64_C(1000000);
        }
        deadline = UINT64_MAX - now < timeout_ns ? UINT64_MAX : now + timeout_ns;
    }

    ZEND_HASH_FOREACH_PTR(&SWOW_G(thread.children), run) {
        cat_timeout_t join_timeout = timeout < 0 ? CAT_TIMEOUT_FOREVER : (cat_timeout_t) timeout;

        if (has_deadline) {
            uint64_t now = uv_hrtime();

            if (now >= deadline) {
                join_timeout = 0;
            } else {
                join_timeout = (cat_timeout_t) ((deadline - now + UINT64_C(999999)) / UINT64_C(1000000));
            }
        }

        if (!cat_thread_join(&run->thread, join_timeout)) {
            (void) cat_thread_detach(&run->thread);
            /* The thread still runs and still uses the shared objects, so the
             * module shutdown has to keep them */
            swow_thread_abandoned_children = cat_true;
        }
        swow_thread_run_release(run);
    } ZEND_HASH_FOREACH_END();

    zend_hash_destroy(&SWOW_G(thread.children));
}

/* serialization */

static cat_bool_t swow_thread_serialize(zval *zv, const char **data, size_t *len)
{
    smart_str buf = { 0 };
    php_serialize_data_t var_hash;

    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&buf, zv, &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);

    if (EG(exception) != NULL || buf.s == NULL) {
        smart_str_free(&buf);
        return cat_false;
    }

    *len = ZSTR_LEN(buf.s);
    *data = cat_sys_malloc(*len);
    if (unlikely(*data == NULL)) {
        smart_str_free(&buf);
        return cat_false;
    }

    memcpy((void *) *data, ZSTR_VAL(buf.s), *len);
    smart_str_free(&buf);

    return cat_true;
}

static cat_bool_t swow_thread_unserialize(const char *data, size_t len, zval *zv)
{
    php_unserialize_data_t var_hash;
    const unsigned char *p = (const unsigned char *) data;
    cat_bool_t ret;

    PHP_VAR_UNSERIALIZE_INIT(var_hash);
    ret = php_var_unserialize(zv, &p, p + len, &var_hash);
    PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

    return ret;
}

/* A serialization failure leaves EG(exception) set, and php_request_shutdown()
 * would turn that into a fatal error that ends the whole process */
static void swow_thread_run_set_serialize_failure(swow_thread_run_t *run)
{
    if (EG(exception) != NULL) {
        zend_clear_exception();
    }
    cat_atomic_bool_store(&run->result_invalid, cat_true);
    cat_atomic_int32_store(&run->exit_status, 255);
}

/*
 * The thread entry runs without a user frame, so zend_throw_exception() reports
 * an exception as uncaught and ends the process with a fatal error. Keep a
 * bailout guard around the serialization, which reports such a throw.
 */
static cat_bool_t swow_thread_serialize_guarded(zval *zv, const char **data, size_t *len)
{
    volatile cat_bool_t serialized = cat_false;

    zend_first_try {
        serialized = swow_thread_serialize(zv, data, len);
    } zend_end_try();

    if (unlikely(!serialized)) {
        if (EG(exception) != NULL) {
            zend_clear_exception();
        }
        *data = NULL;
        *len = 0;
    }

    return serialized;
}

/* Report the exception that ended the code to the waiting parent */
static void swow_thread_run_set_exception(swow_thread_run_t *run)
{
    zval exception;

    /*
     * Take the exception out of the executor first, because the serializer
     * reports a pending exception as a failure of its own
     */
    ZVAL_OBJ(&exception, EG(exception));
    EG(exception) = NULL;

    if (unlikely(!swow_thread_serialize_guarded(&exception, &run->exception,
                                                &run->exception_len))) {
        zval_ptr_dtor(&exception);
        swow_thread_run_set_serialize_failure(run);
        return;
    }

    zval_ptr_dtor(&exception);
    cat_atomic_int32_store(&run->exit_status, 255);
}

/* A bailout ends the code without a return value. exit() and die() keep their
 * status in EG(exit_status), and any other exception is the code's own */
static void swow_thread_run_take_end(zend_object *exception, swow_thread_run_t *run)
{
    if (exception != NULL) {
        if (zend_is_unwind_exit(exception) || zend_is_graceful_exit(exception)) {
            zend_clear_exception();
        } else {
            swow_thread_run_set_exception(run);
            return;
        }
    }

    cat_atomic_int32_store(&run->exit_status, (int32_t) EG(exit_status));
}

/* child */

static const char swow_thread_callable_wrapper[] =
    "static function (callable $callable, array $args): array {"
    "    try {"
    "        $result = $callable(...$args);"
    "    } catch (\\Throwable $e) {"
    "        try {"
    "            return [1, serialize($e)];"
    "        } catch (\\Throwable $ignored) {"
    "            return [2, null];"
    "        }"
    "    }"
    /* The value is serialized here, because a throw from the C side of a thread
     * has no user frame and ends the process as an uncaught exception */
    "    try {"
    "        return [0, serialize($result)];"
    "    } catch (\\Throwable $ignored) {"
    "        return [2, null];"
    "    }"
    "};";

/* The status word of the wrapper result */
#define SWOW_THREAD_RESULT_OK       0
#define SWOW_THREAD_RESULT_EXCEPTION 1
#define SWOW_THREAD_RESULT_INVALID  2

static void swow_thread_run_take_payload(const char *data, size_t len,
                                         const char **payload, size_t *payload_len)
{
    *payload = cat_sys_malloc(len);
    if (unlikely(*payload == NULL)) {
        *payload_len = 0;
        return;
    }
    memcpy((void *) *payload, data, len);
    *payload_len = len;
}

static void swow_thread_run_callable(swow_thread_run_t *run)
{
    zval callable;
    zval args;
    zval wrapper;
    zval wrapped;
    zval *status;
    zval *payload;
    cat_bool_t completed = cat_false;

    ZVAL_UNDEF(&callable);
    ZVAL_UNDEF(&args);
    ZVAL_UNDEF(&wrapper);
    ZVAL_UNDEF(&wrapped);

    if (cat_atomic_bool_load(&run->killed)) {
        cat_atomic_int32_store(&run->exit_status, -1);
        return;
    }

    if (unlikely(!swow_thread_unserialize(run->callable, run->callable_len, &callable))) {
        return;
    }

    if (unlikely(!swow_thread_unserialize(run->args, run->args_len, &args) ||
                 Z_TYPE(args) != IS_ARRAY)) {
        zval_ptr_dtor(&callable);
        if (Z_TYPE(args) != IS_UNDEF) {
            zval_ptr_dtor(&args);
        }
        return;
    }

    if (unlikely(zend_eval_stringl(swow_thread_callable_wrapper,
                                   strlen(swow_thread_callable_wrapper),
                                   &wrapper, "swow_thread") != SUCCESS)) {
        if (EG(exception) != NULL) {
            zend_clear_exception();
        }
        cat_atomic_int32_store(&run->exit_status, 255);
        goto _done;
    }

    SWOW_G(thread.kill_ready) = true;
    if (cat_atomic_bool_load(&run->killed)) {
        SWOW_G(thread.kill_ready) = false;
        cat_atomic_int32_store(&run->exit_status, -1);
        goto _done;
    }

    {
        zval params[2];
        zend_fcall_info fci;

        ZVAL_COPY_VALUE(&params[0], &callable);
        ZVAL_COPY_VALUE(&params[1], &args);

        fci = (zend_fcall_info) {
            .size = sizeof(zend_fcall_info),
            .retval = &wrapped,
            .function_name = wrapper,
            .params = params,
            .param_count = 2,
        };

        zend_first_try {
            zend_call_function(&fci, NULL);
            completed = cat_true;
        } zend_end_try();
    }

    SWOW_G(thread.kill_ready) = false;

    if (cat_atomic_bool_load(&run->killed)) {
        if (EG(exception) != NULL) {
            zend_clear_exception();
        }
        cat_atomic_int32_store(&run->exit_status, -1);
        goto _done;
    }

    if (unlikely(!completed)) {
        /* exit() or die() went through the bailout path */
        swow_thread_run_take_end(EG(exception), run);
        goto _done;
    }

    if (EG(exception) != NULL) {
        swow_thread_run_take_end(EG(exception), run);
        goto _done;
    }

    if (Z_TYPE(wrapped) != IS_ARRAY) {
        cat_atomic_int32_store(&run->exit_status, 255);
        goto _done;
    }

    status = zend_hash_index_find(Z_ARRVAL(wrapped), 0);
    payload = zend_hash_index_find(Z_ARRVAL(wrapped), 1);
    if (unlikely(status == NULL || payload == NULL || Z_TYPE_P(status) != IS_LONG)) {
        cat_atomic_int32_store(&run->exit_status, 255);
        goto _done;
    }

    switch (Z_LVAL_P(status)) {
        case SWOW_THREAD_RESULT_OK:
            if (unlikely(Z_TYPE_P(payload) != IS_STRING)) {
                swow_thread_run_set_serialize_failure(run);
                break;
            }
            swow_thread_run_take_payload(Z_STRVAL_P(payload), Z_STRLEN_P(payload),
                                        &run->result, &run->result_len);
            if (unlikely(run->result == NULL)) {
                swow_thread_run_set_serialize_failure(run);
            }
            break;
        case SWOW_THREAD_RESULT_EXCEPTION:
            if (unlikely(Z_TYPE_P(payload) != IS_STRING)) {
                swow_thread_run_set_serialize_failure(run);
                break;
            }
            swow_thread_run_take_payload(Z_STRVAL_P(payload), Z_STRLEN_P(payload),
                                        &run->exception, &run->exception_len);
            if (unlikely(run->exception == NULL)) {
                swow_thread_run_set_serialize_failure(run);
                break;
            }
            cat_atomic_int32_store(&run->exit_status, 255);
            break;
        default:
            /* the result or the exception could not be serialized */
            swow_thread_run_set_serialize_failure(run);
            break;
    }

_done:
    zval_ptr_dtor(&wrapped);
    zval_ptr_dtor(&wrapper);
    zval_ptr_dtor(&args);
    zval_ptr_dtor(&callable);
}

static void swow_thread_run_code(swow_thread_run_t *run)
{
    volatile cat_bool_t completed = cat_false;

    if (cat_atomic_bool_load(&run->killed)) {
        cat_atomic_int32_store(&run->exit_status, -1);
        return;
    }

    SWOW_G(thread.kill_ready) = true;
    if (cat_atomic_bool_load(&run->killed)) {
        SWOW_G(thread.kill_ready) = false;
        cat_atomic_int32_store(&run->exit_status, -1);
        return;
    }

    /* The code runs as a script, so a return value is not taken */
    zend_first_try {
        if (zend_eval_stringl(run->code, run->code_len, NULL, "Swow\\Thread") == SUCCESS) {
            completed = cat_true;
        }
    } zend_end_try();

    SWOW_G(thread.kill_ready) = false;

    if (cat_atomic_bool_load(&run->killed)) {
        if (EG(exception) != NULL) {
            zend_clear_exception();
        }
        cat_atomic_int32_store(&run->exit_status, -1);
        return;
    }

    if (unlikely(!completed)) {
        swow_thread_run_take_end(EG(exception), run);
        return;
    }

    if (EG(exception) != NULL) {
        swow_thread_run_take_end(EG(exception), run);
    }
}

static void swow_thread_entry(cat_data_t *data)
{
    swow_thread_run_t *run = (swow_thread_run_t *) data;
    cat_bool_t request_started = cat_false;

    (void) ts_resource(0);

    ZEND_TSRMLS_CACHE_UPDATE();

    SG(server_context) = run->server_context;
    PG(expose_php) = false;
    PG(auto_globals_jit) = true;

    if (unlikely(php_request_startup() == FAILURE)) {
        goto _started;
    }
    request_started = cat_true;

    PG(during_request_startup) = false;
    SG(sapi_started) = false;
    SG(headers_sent) = true;
    SG(request_info).no_headers = true;
    php_register_variable("PHP_SELF", "-", NULL);

    if (unlikely(!cat_thread_bootstrap(&run->thread))) {
        goto _started;
    }
    SWOW_G(thread.registered) = true;

    uv_mutex_lock(&run->lock);
    run->eg = (zend_executor_globals *) ts_resource_ex(executor_globals_id, NULL);
    run->globals = (zend_swow_globals *) ts_resource_ex(swow_globals_id, NULL);
    uv_mutex_unlock(&run->lock);

    cat_atomic_int32_store(&run->start_status, SUCCESS);

_started:
    cat_sem_post(&run->started);

    if (cat_atomic_int32_load(&run->start_status) == SUCCESS) {
        if (run->code != NULL) {
            swow_thread_run_code(run);
        } else {
            swow_thread_run_callable(run);
        }
    }

    uv_mutex_lock(&run->lock);
    run->eg = NULL;
    run->globals = NULL;
    uv_mutex_unlock(&run->lock);

    if (request_started) {
        php_request_shutdown(NULL);
    }

    if (SWOW_G(thread.registered)) {
        SWOW_G(thread.registered) = false;
        (void) cat_thread_runtime_shutdown();
    }

    cat_thread_teardown(&run->thread);
    ts_free_thread();
    swow_thread_run_release(run);
}

/* object */

static zend_object *swow_thread_create_object(zend_class_entry *ce)
{
    swow_thread_t *s_thread = swow_object_alloc(swow_thread_t, ce, swow_thread_handlers);

    ZVAL_UNDEF(&s_thread->callable);
    ZVAL_UNDEF(&s_thread->args);
    ZVAL_UNDEF(&s_thread->result);
    s_thread->run = NULL;
    s_thread->thread = NULL;
    s_thread->id = 0;
    s_thread->started = cat_false;
    s_thread->waited = cat_false;
    s_thread->failed = cat_false;
    s_thread->detached = cat_false;
    s_thread->killed = cat_false;

    return &s_thread->std;
}

static void swow_thread_free_object(zend_object *object)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(object);

    if (s_thread->run != NULL) {
        swow_thread_run_release(s_thread->run);
        s_thread->run = NULL;
    }

    zval_ptr_dtor(&s_thread->result);
    zval_ptr_dtor(&s_thread->callable);
    zval_ptr_dtor(&s_thread->args);

    zend_object_std_dtor(&s_thread->std);
}

static HashTable *swow_thread_get_gc(zend_object *object, zval **gc_data, int *gc_count)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(object);
    zend_get_gc_buffer *gc_buffer;

    if (Z_TYPE(s_thread->callable) == IS_UNDEF &&
        Z_TYPE(s_thread->args) == IS_UNDEF &&
        Z_TYPE(s_thread->result) == IS_UNDEF) {
        *gc_data = NULL;
        *gc_count = 0;
        return zend_std_get_properties(object);
    }

    gc_buffer = zend_get_gc_buffer_create();

    if (Z_TYPE(s_thread->callable) != IS_UNDEF) {
        zend_get_gc_buffer_add_zval(gc_buffer, &s_thread->callable);
    }
    if (Z_TYPE(s_thread->args) != IS_UNDEF) {
        zend_get_gc_buffer_add_zval(gc_buffer, &s_thread->args);
    }
    if (Z_TYPE(s_thread->result) != IS_UNDEF) {
        zend_get_gc_buffer_add_zval(gc_buffer, &s_thread->result);
    }

    zend_get_gc_buffer_use(gc_buffer, gc_data, gc_count);

    return zend_std_get_properties(object);
}

/* methods */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Thread___construct, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, callable, IS_CALLABLE, 0)
    ZEND_ARG_VARIADIC_TYPE_INFO(0, args, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Thread_run, 0, 1, Swow\\Thread, 0)
    ZEND_ARG_INFO(0, callable)
    ZEND_ARG_VARIADIC_INFO(0, args)
ZEND_END_ARG_INFO()

static zend_result swow_thread_call_method(zval *object, const char *name, size_t name_len,
                                          zval *retval, uint32_t param_count, zval *params)
{
    zend_fcall_info fci = {
        .size = sizeof(zend_fcall_info),
        .retval = retval,
        .params = params,
        .param_count = param_count,
        .object = Z_OBJ_P(object),
    };
    zend_result result;

    ZVAL_STRINGL(&fci.function_name, name, name_len);
    result = zend_call_function(&fci, NULL);
    zval_ptr_dtor(&fci.function_name);

    return result;
}

static PHP_METHOD(Swow_Thread, run)
{
    zval *z_callable;
    zval *params;
    uint32_t param_count;
    zend_class_entry *ce;
    zval thread;
    zval retval;
    zval *args;

    ZEND_PARSE_PARAMETERS_START(1, -1)
        Z_PARAM_ZVAL(z_callable)
        Z_PARAM_VARIADIC('*', params, param_count)
    ZEND_PARSE_PARAMETERS_END();

    ce = zend_get_called_scope(execute_data);
    if (ce == NULL) {
        ce = swow_thread_ce;
    }

    object_init_ex(&thread, ce);

    args = safe_emalloc(param_count + 1, sizeof(zval), 0);
    ZVAL_COPY_VALUE(&args[0], z_callable);
    for (uint32_t i = 0; i < param_count; i++) {
        ZVAL_COPY_VALUE(&args[i + 1], &params[i]);
    }

    ZVAL_UNDEF(&retval);
    if (swow_thread_call_method(&thread, ZEND_STRL("__construct"),
                                &retval, param_count + 1, args) != SUCCESS ||
        EG(exception) != NULL) {
        efree(args);
        zval_ptr_dtor(&thread);
        zval_ptr_dtor(&retval);
        if (EG(exception) == NULL) {
            zend_throw_exception(swow_thread_exception_ce, "Failed to construct the thread", 0);
        }
        RETURN_THROWS();
    }
    efree(args);
    zval_ptr_dtor(&retval);
    ZVAL_UNDEF(&retval);

    if (swow_thread_call_method(&thread, ZEND_STRL("start"),
                                &retval, 0, NULL) != SUCCESS ||
        EG(exception) != NULL) {
        zval_ptr_dtor(&thread);
        zval_ptr_dtor(&retval);
        if (EG(exception) == NULL) {
            zend_throw_exception(swow_thread_exception_ce, "Failed to start the thread", 0);
        }
        RETURN_THROWS();
    }

    zval_ptr_dtor(&retval);

    RETVAL_ZVAL(&thread, 1, 0);
    zval_ptr_dtor(&thread);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Thread_runCode, 0, 1, Swow\\Thread, 0)
    ZEND_ARG_TYPE_INFO(0, code, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thread, runCode)
{
    zend_string *code;
    zend_class_entry *ce;
    zval thread;
    swow_thread_t *s_thread;
    swow_thread_run_t *run;
    cat_thread_options_t options = {
        .name = NULL,
        .stack_size = 0,
        .flags = CAT_THREAD_CREATE_MANUAL_RUNTIME,
    };

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(code)
    ZEND_PARSE_PARAMETERS_END();

    ce = zend_get_called_scope(execute_data);
    if (ce == NULL) {
        ce = swow_thread_ce;
    }

    object_init_ex(&thread, ce);
    s_thread = swow_thread_get_from_object(Z_OBJ(thread));

    run = swow_thread_run_create();
    if (unlikely(run == NULL)) {
        zval_ptr_dtor(&thread);
        zend_throw_exception(swow_thread_exception_ce, "Failed to allocate the thread", 0);
        RETURN_THROWS();
    }

    run->code = cat_sys_malloc(ZSTR_LEN(code) + 1);
    if (unlikely(run->code == NULL)) {
        swow_thread_run_release(run);
        zval_ptr_dtor(&thread);
        zend_throw_exception(swow_thread_exception_ce, "Failed to allocate the thread code", 0);
        RETURN_THROWS();
    }
    memcpy((void *) run->code, ZSTR_VAL(code), ZSTR_LEN(code));
    ((char *) run->code)[ZSTR_LEN(code)] = '\0';
    run->code_len = ZSTR_LEN(code);
    run->server_context = SG(server_context);
    cat_atomic_int32_init(&run->start_status, FAILURE);

    if (unlikely(!cat_thread_create_ex(&run->thread, swow_thread_entry, run, &options))) {
        swow_thread_run_release(run);
        zval_ptr_dtor(&thread);
        swow_throw_exception_with_last(swow_thread_exception_ce);
        RETURN_THROWS();
    }

    swow_thread_run_addref(run);
    swow_thread_children_add(run);

    s_thread->run = run;
    s_thread->thread = &run->thread;
    s_thread->id = swow_thread_id_to_long(&run->thread);
    s_thread->started = cat_true;

    if (unlikely(!cat_sem_acquire(&run->started, -1))) {
        zval_ptr_dtor(&thread);
        swow_throw_exception_with_last(swow_thread_exception_ce);
        RETURN_THROWS();
    }

    if (unlikely(cat_atomic_int32_load(&run->start_status) != SUCCESS)) {
        zval_ptr_dtor(&thread);
        zend_throw_exception(swow_thread_exception_ce, "Thread failed to start", 0);
        RETURN_THROWS();
    }

    RETVAL_ZVAL(&thread, 1, 0);
    zval_ptr_dtor(&thread);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Thread_start, 0, 0, Swow\\Thread, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thread_wait, 0, 0, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thread, __construct)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *z_callable;
    zval *params;
    uint32_t param_count;

    ZEND_PARSE_PARAMETERS_START(1, -1)
        Z_PARAM_ZVAL(z_callable)
        Z_PARAM_VARIADIC('*', params, param_count)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(Z_TYPE(s_thread->callable) != IS_UNDEF)) {
        zend_throw_error(NULL, "%s can be constructed only once", ZEND_THIS_NAME);
        RETURN_THROWS();
    }

    if (unlikely(!zend_is_callable(z_callable, 0, NULL))) {
        zend_argument_type_error(1, "must be a valid callback");
        RETURN_THROWS();
    }

    ZVAL_COPY(&s_thread->callable, z_callable);

    array_init_size(&s_thread->args, param_count);
    for (uint32_t i = 0; i < param_count; i++) {
        Z_TRY_ADDREF(params[i]);
        zend_hash_next_index_insert_new(Z_ARRVAL(s_thread->args), &params[i]);
    }
}

static PHP_METHOD(Swow_Thread, start)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long timeout = -1;
    swow_thread_run_t *run;
    const char *callable = NULL;
    size_t callable_len = 0;
    const char *args = NULL;
    size_t args_len = 0;
    cat_thread_options_t options = {
        .name = NULL,
        .stack_size = 0,
        .flags = CAT_THREAD_CREATE_MANUAL_RUNTIME,
    };

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(s_thread->started)) {
        zend_throw_exception(swow_thread_exception_ce, "Thread has already been started", 0);
        RETURN_THROWS();
    }

    if (unlikely(Z_TYPE(s_thread->callable) == IS_UNDEF)) {
        zend_throw_exception(swow_thread_exception_ce, "Thread has not been constructed", 0);
        RETURN_THROWS();
    }

    if (unlikely(!swow_thread_serialize(&s_thread->callable, &callable, &callable_len))) {
        if (EG(exception) == NULL) {
            zend_throw_exception(swow_thread_exception_ce, "Failed to serialize the callable", 0);
        }
        RETURN_THROWS();
    }

    if (unlikely(!swow_thread_serialize(&s_thread->args, &args, &args_len))) {
        cat_sys_free((void *) callable);
        if (EG(exception) == NULL) {
            zend_throw_exception(swow_thread_exception_ce, "Failed to serialize the arguments", 0);
        }
        RETURN_THROWS();
    }

    run = swow_thread_run_create();
    if (unlikely(run == NULL)) {
        cat_sys_free((void *) callable);
        cat_sys_free((void *) args);
        zend_throw_exception(swow_thread_exception_ce, "Failed to allocate the thread", 0);
        RETURN_THROWS();
    }

    run->callable = callable;
    run->callable_len = callable_len;
    run->args = args;
    run->args_len = args_len;
    run->server_context = SG(server_context);
    cat_atomic_int32_init(&run->start_status, FAILURE);

    if (unlikely(!cat_thread_create_ex(&run->thread, swow_thread_entry, run, &options))) {
        swow_thread_run_release(run);
        swow_throw_exception_with_last(swow_thread_exception_ce);
        RETURN_THROWS();
    }

    swow_thread_run_addref(run);
    swow_thread_children_add(run);

    s_thread->run = run;
    s_thread->thread = &run->thread;
    s_thread->id = swow_thread_id_to_long(&run->thread);
    s_thread->started = cat_true;

    if (unlikely(!cat_sem_acquire(&run->started, timeout))) {
        swow_throw_exception_with_last(swow_thread_exception_ce);
        RETURN_THROWS();
    }

    if (unlikely(cat_atomic_int32_load(&run->start_status) != SUCCESS)) {
        zend_throw_exception(swow_thread_exception_ce, "Thread failed to start", 0);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

/* Latch a failure that came after the join, so a second wait() reports it again
 * instead of returning an undefined result */
static void swow_thread_wait_failure(swow_thread_t *s_thread, const char *message)
{
    zval thrown;

    zval_ptr_dtor(&s_thread->result);
    ZVAL_UNDEF(&s_thread->result);

    zend_throw_exception(swow_thread_exception_ce, message, 0);
    ZVAL_OBJ(&s_thread->result, EG(exception));
    EG(exception) = NULL;

    s_thread->waited = cat_true;
    s_thread->failed = cat_true;

    ZVAL_COPY(&thrown, &s_thread->result);
    zend_throw_exception_object(&thrown);
}

static PHP_METHOD(Swow_Thread, wait)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long timeout = -1;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (likely(s_thread->waited)) {
        if (unlikely(s_thread->killed)) {
            zend_throw_exception(swow_thread_exception_ce, "Thread was killed", 0);
            RETURN_THROWS();
        }
        if (unlikely(s_thread->failed)) {
            zval exception;

            ZVAL_COPY(&exception, &s_thread->result);
            zend_throw_exception_object(&exception);
            RETURN_THROWS();
        }
        RETURN_ZVAL(&s_thread->result, 1, 0);
    }

    if (unlikely(s_thread->run == NULL)) {
        zend_throw_exception(swow_thread_exception_ce,
                             s_thread->started ? "Cannot wait for the current thread" :
                                                 "Thread has not been started", 0);
        RETURN_THROWS();
    }

    if (unlikely(s_thread->detached)) {
        zend_throw_exception(swow_thread_exception_ce, "Thread has been detached", 0);
        RETURN_THROWS();
    }

    if (unlikely(!cat_thread_join(&s_thread->run->thread, timeout))) {
        swow_throw_exception_with_last(swow_thread_exception_ce);
        RETURN_THROWS();
    }

    swow_thread_children_del(s_thread->run);

    if (cat_atomic_bool_load(&s_thread->run->killed)) {
        s_thread->waited = cat_true;
        s_thread->killed = cat_true;
        zend_throw_exception(swow_thread_exception_ce, "Thread was killed", 0);
        RETURN_THROWS();
    }

    if (unlikely(cat_atomic_bool_load(&s_thread->run->result_invalid))) {
        swow_thread_wait_failure(s_thread, "Failed to serialize the thread result or exception");
        RETURN_THROWS();
    }

    if (s_thread->run->exception != NULL) {
        zval exception;

        ZVAL_UNDEF(&exception);
        if (unlikely(!swow_thread_unserialize(s_thread->run->exception,
                                              s_thread->run->exception_len, &exception))) {
            if (EG(exception) != NULL) {
                zend_clear_exception();
            }
            if (Z_TYPE(exception) != IS_UNDEF) {
                zval_ptr_dtor(&exception);
            }
            swow_thread_wait_failure(s_thread, "Failed to unserialize the thread exception");
            RETURN_THROWS();
        }
        if (unlikely(Z_TYPE(exception) != IS_OBJECT ||
                     !instanceof_function(Z_OBJCE(exception), zend_ce_throwable))) {
            zval_ptr_dtor(&exception);
            swow_thread_wait_failure(s_thread, "Thread returned a non-throwable exception");
            RETURN_THROWS();
        }

        zval_ptr_dtor(&s_thread->result);
        s_thread->result = exception;
        s_thread->waited = cat_true;
        s_thread->failed = cat_true;

        {
            zval thrown;

            ZVAL_COPY(&thrown, &s_thread->result);
            zend_throw_exception_object(&thrown);
        }
        RETURN_THROWS();
    }

    if (s_thread->run->result == NULL) {
        /* the thread ended with exit() or without a return value */
        zval_ptr_dtor(&s_thread->result);
        ZVAL_NULL(&s_thread->result);
        s_thread->waited = cat_true;
        RETURN_NULL();
    }

    ZVAL_UNDEF(&s_thread->result);
    if (unlikely(!swow_thread_unserialize(s_thread->run->result, s_thread->run->result_len,
                                          &s_thread->result))) {
        if (EG(exception) != NULL) {
            zend_clear_exception();
        }
        if (Z_TYPE(s_thread->result) != IS_UNDEF) {
            zval_ptr_dtor(&s_thread->result);
            ZVAL_UNDEF(&s_thread->result);
        }
        swow_thread_wait_failure(s_thread, "Failed to unserialize the thread result");
        RETURN_THROWS();
    }

    s_thread->waited = cat_true;

    RETURN_ZVAL(&s_thread->result, 1, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Thread_detach, 0, 0, Swow\\Thread, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Thread_kill, 0, 0, Swow\\Thread, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thread_isStarted, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thread_isRunning, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thread_isDetached, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thread_getId, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thread, detach)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(s_thread->run == NULL)) {
        zend_throw_exception(swow_thread_exception_ce,
                             s_thread->started ? "Cannot detach the current thread" :
                                                 "Thread has not been started", 0);
        RETURN_THROWS();
    }

    if (unlikely(s_thread->waited)) {
        zend_throw_exception(swow_thread_exception_ce, "Thread has already been waited", 0);
        RETURN_THROWS();
    }

    if (s_thread->detached) {
        RETURN_THIS();
    }

    /* Keep the run in the children table so request shutdown can join it
     * before the extension is unloaded */
    s_thread->detached = cat_true;

    RETURN_THIS();
}

static PHP_METHOD(Swow_Thread, kill)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS));
    swow_thread_run_t *run = s_thread->run;

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(run == NULL)) {
        zend_throw_exception(swow_thread_exception_ce,
                             s_thread->started ? "Cannot kill the current thread" :
                                                 "Thread has not been started", 0);
        RETURN_THROWS();
    }

    if (unlikely(!cat_thread_is_running(&run->thread))) {
        zend_throw_exception(swow_thread_exception_ce, "Thread is not running", 0);
        RETURN_THROWS();
    }

    if (unlikely(!swow_thread_run_kill(run))) {
        zend_throw_exception(swow_thread_exception_ce, "Thread is not running", 0);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

static PHP_METHOD(Swow_Thread, isStarted)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(s_thread->started);
}

static zend_long swow_thread_id_to_long(cat_thread_t *thread)
{
#ifdef CAT_OS_WIN
    return (zend_long) GetThreadId((HANDLE) cat_thread_get_id(thread));
#else
    return (zend_long) cat_thread_get_id(thread);
#endif
}

static PHP_METHOD(Swow_Thread, isRunning)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(s_thread->thread != NULL && cat_thread_is_running(s_thread->thread));
}

static PHP_METHOD(Swow_Thread, isDetached)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(s_thread->detached);
}

static PHP_METHOD(Swow_Thread, getId)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(s_thread->id);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thread_killAll, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thread, killAll)
{
    zend_long count = 0;
    swow_thread_run_t *run;

    ZEND_PARSE_PARAMETERS_NONE();

    ZEND_HASH_FOREACH_PTR(&SWOW_G(thread.children), run) {
        if (swow_thread_run_kill(run)) {
            count++;
        }
    } ZEND_HASH_FOREACH_END();

    RETURN_LONG(count);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thread___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thread, __debugInfo)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);
    add_assoc_long(return_value, "id", s_thread->id);
    add_assoc_bool(return_value, "started", s_thread->started != cat_false);
    add_assoc_bool(return_value, "running",
                   s_thread->thread != NULL && cat_thread_is_running(s_thread->thread));
    add_assoc_bool(return_value, "detached", s_thread->detached != cat_false);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Thread_getCurrent, 0, 0, Swow\\Thread, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thread_getCurrentId, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thread, getCurrent)
{
    cat_thread_t *thread;

    ZEND_PARSE_PARAMETERS_NONE();

    if (Z_TYPE(SWOW_G(thread.current)) == IS_UNDEF) {
        zval current;
        swow_thread_t *s_current;

        thread = cat_thread_get_current();
        if (unlikely(thread == NULL)) {
            zend_throw_exception(swow_thread_exception_ce, "Current thread is not registered", 0);
            RETURN_THROWS();
        }

        object_init_ex(&current, swow_thread_ce);
        s_current = swow_thread_get_from_object(Z_OBJ(current));
        s_current->thread = thread;
        s_current->id = swow_thread_id_to_long(thread);
        s_current->started = cat_true;
        ZVAL_COPY_VALUE(&SWOW_G(thread.current), &current);
    }

    RETURN_ZVAL(&SWOW_G(thread.current), 1, 0);
}

static PHP_METHOD(Swow_Thread, getCurrentId)
{
    cat_thread_t *thread;

    ZEND_PARSE_PARAMETERS_NONE();

    thread = cat_thread_get_current();
    if (unlikely(thread == NULL)) {
        RETURN_LONG(0);
    }

    RETURN_LONG(swow_thread_id_to_long(thread));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thread_getExitStatus, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thread, getExitStatus)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(s_thread->run != NULL ? cat_atomic_int32_load(&s_thread->run->exit_status) : 0);
}

static const zend_function_entry swow_thread_methods[] = {
    PHP_ME(Swow_Thread, __construct,   arginfo_class_Swow_Thread___construct,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, run,           arginfo_class_Swow_Thread_run,           ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Thread, runCode,       arginfo_class_Swow_Thread_runCode,       ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Thread, start,         arginfo_class_Swow_Thread_start,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, wait,          arginfo_class_Swow_Thread_wait,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, detach,        arginfo_class_Swow_Thread_detach,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, kill,          arginfo_class_Swow_Thread_kill,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, killAll,       arginfo_class_Swow_Thread_killAll,       ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Thread, isStarted,     arginfo_class_Swow_Thread_isStarted,     ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, isRunning,     arginfo_class_Swow_Thread_isRunning,     ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, isDetached,    arginfo_class_Swow_Thread_isDetached,    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, getId,         arginfo_class_Swow_Thread_getId,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, __debugInfo,   arginfo_class_Swow_Thread___debugInfo,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, getCurrent,    arginfo_class_Swow_Thread_getCurrent,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Thread, getCurrentId,  arginfo_class_Swow_Thread_getCurrentId,  ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Thread, getExitStatus, arginfo_class_Swow_Thread_getExitStatus, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* module */

static swow_interrupt_function_t original_zend_interrupt_function = NULL;

static void swow_thread_interrupt_function(zend_execute_data *execute_data)
{
    if (cat_atomic_bool_load(&SWOW_G(thread.kill_requested))) {
        cat_atomic_bool_store(&SWOW_G(thread.kill_requested), cat_false);
        if (SWOW_G(thread.kill_ready)) {
            zend_throw_unwind_exit();
        }
        return;
    }

    if (original_zend_interrupt_function != NULL) {
        original_zend_interrupt_function(execute_data);
    }
}

zend_result swow_thread_module_init(INIT_FUNC_ARGS)
{
    swow_thread_ce = swow_register_internal_class(
        "Swow\\Thread", NULL, swow_thread_methods,
        &swow_thread_handlers, NULL,
        cat_false, cat_false,
        swow_thread_create_object, swow_thread_free_object,
        offsetof(swow_thread_t, std)
    );
    swow_thread_handlers.get_gc = swow_thread_get_gc;

    swow_thread_exception_ce = swow_register_internal_class(
        "Swow\\ThreadException", swow_exception_ce, NULL, NULL, NULL,
        cat_true, cat_true, NULL, NULL, 0
    );

    original_zend_interrupt_function = zend_interrupt_function;
    zend_interrupt_function = swow_thread_interrupt_function;

#ifdef CAT_OS_WIN
    /* The CLI deactivate callback lives in php.exe and uses a TSRM cache that
     * a Swow thread never initializes */
    if (sapi_module.name != NULL &&
        (strcmp(sapi_module.name, "cli") == 0 || strcmp(sapi_module.name, "phpdbg") == 0) &&
        sapi_module.deactivate != swow_thread_sapi_deactivate) {
        swow_thread_original_sapi_deactivate = sapi_module.deactivate;
        sapi_module.deactivate = swow_thread_sapi_deactivate;
    }
#endif

    return SUCCESS;
}

zend_result swow_thread_module_shutdown(SHUTDOWN_FUNC_ARGS)
{
    if (zend_interrupt_function == swow_thread_interrupt_function) {
        zend_interrupt_function = original_zend_interrupt_function;
    }

#ifdef CAT_OS_WIN
    if (sapi_module.deactivate == swow_thread_sapi_deactivate) {
        sapi_module.deactivate = swow_thread_original_sapi_deactivate;
    }
#endif

    return SUCCESS;
}

zend_result swow_thread_runtime_init(INIT_FUNC_ARGS)
{
    SWOW_G(thread.registered) = false;
    cat_atomic_bool_init(&SWOW_G(thread.kill_requested), cat_false);
    SWOW_G(thread.kill_ready) = false;
    swow_thread_abandoned_children = cat_false;
    ZVAL_UNDEF(&SWOW_G(thread.current));
    zend_hash_init(&SWOW_G(thread.children), 8, NULL, NULL, 0);
    SWOW_G(thread.children_initialized) = true;

    if (tsrm_is_main_thread()) {
        if (unlikely(!cat_thread_ensure_root())) {
            return FAILURE;
        }
        SWOW_G(thread.registered) = true;
    }

    return SUCCESS;
}

zend_result swow_thread_runtime_shutdown(SHUTDOWN_FUNC_ARGS)
{
    if (SWOW_G(thread.children_initialized)) {
        swow_thread_children_shutdown();
        SWOW_G(thread.children_initialized) = false;
    }

    zval_ptr_dtor(&SWOW_G(thread.current));
    ZVAL_UNDEF(&SWOW_G(thread.current));

    if (SWOW_G(thread.registered)) {
        SWOW_G(thread.registered) = false;
        (void) cat_thread_runtime_shutdown();
    }

    return SUCCESS;
}

#endif /* ZTS */
