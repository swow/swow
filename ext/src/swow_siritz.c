#include "cat.h"
#include "swow.h"
#include "SAPI.h"
#include "php_main.h"
#include "php_variables.h"
#include "swow_siritz.h"
#include "swow_closure.h"
#include "swow_hook.h"
#include "swow_wrapper.h"
#include "zend_atomic.h"
#include "zend_exceptions.h"
#include "zend_hash.h"
#include "zend_smart_str.h"
#include "zend_types.h"

#ifdef ZTS

typedef enum swow_siritz_thread_status_e {
    SWOW_SIRITZ_THREAD_STATUS_NONE = 0,
    SWOW_SIRITZ_THREAD_STATUS_RUNNING = 1,
    SWOW_SIRITZ_THREAD_STATUS_FINISHED = 2,
} swow_siritz_thread_status_t;

typedef enum swow_siritz_wait_result_e {
    SWOW_SIRITZ_WAIT_RESULT_SUCCESS = 0,
    SWOW_SIRITZ_WAIT_RESULT_TIMEOUT = 1,
    SWOW_SIRITZ_WAIT_RESULT_KILLED = 2,
    SWOW_SIRITZ_WAIT_RESULT_ERROR = 3,
} swow_siritz_wait_result_t;

#ifdef CAT_OS_WIN
# define CAT_LOG_THREAD_FMT "%p"
#elif defined(CAT_OS_UNIX_LIKE)
# define CAT_LOG_THREAD_FMT "%lu"
#else
# error "Unsupported OS"
#endif

#if !defined(HAVE_PTHREAD_TIMEDJOIN_NP) && !defined(CAT_OS_WIN)
// from https://stackoverflow.com/a/11552244
struct pthread_timedjoin_np_args {
    int joined;
    pthread_t td;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    void **res;
};

static void *swow_pthread_timedjoin_np_waiter(void *ap)
{
    struct pthread_timedjoin_np_args *args = ap;
    pthread_join(args->td, args->res);
    pthread_mutex_lock(&args->mtx);
    args->joined = 1;
    pthread_mutex_unlock(&args->mtx);
    pthread_cond_signal(&args->cond);
    return 0;
}

static int swow_pthread_timedjoin_np(pthread_t td, void **res, struct timespec *ts)
{
    pthread_t tmp;
    int ret;
    struct pthread_timedjoin_np_args args = { .td = td, .res = res };

    pthread_mutex_init(&args.mtx, 0);
    pthread_cond_init(&args.cond, 0);
    pthread_mutex_lock(&args.mtx);

    ret = pthread_create(&tmp, 0, swow_pthread_timedjoin_np_waiter, &args);
    if (!ret)
            do ret = pthread_cond_timedwait(&args.cond, &args.mtx, ts);
        while (!args.joined && ret != ETIMEDOUT);

    pthread_mutex_unlock(&args.mtx);

    pthread_cancel(tmp);
    pthread_join(tmp, 0);

    pthread_cond_destroy(&args.cond);
    pthread_mutex_destroy(&args.mtx);

    return args.joined ? 0 : ret;
}
#define pthread_timedjoin_np swow_pthread_timedjoin_np
#endif

CAT_GLOBALS_DECLARE(swow_siritz);

static swow_interrupt_function_t original_zend_interrupt_function = (swow_interrupt_function_t) -1;

static void swow_siritz_interrupt_function(zend_execute_data *execute_data)
{
    if (EG(current_execute_data)) {
#if PHP_VERSION_ID <= 80012
# error "Unsupported PHP version"
#else
        zend_throw_unwind_exit();
#endif
    }

    if (original_zend_interrupt_function != NULL) {
        original_zend_interrupt_function(execute_data);
    }
}

static void swow_siritz_thread_interrupt(uv_thread_t thread)
{
    THREAD_T php_thread;
#ifdef CAT_OS_WIN
    php_thread = GetThreadId((HANDLE)thread);
#elif defined(CAT_OS_UNIX_LIKE)
    php_thread = (THREAD_T)thread;
#else
# error "Unsupported OS"
#endif
    // interrupt thread using vm_interrupt
    zend_executor_globals *child_executor_global =
        (zend_executor_globals *)ts_resource_ex(executor_globals_id, &php_thread);
    zend_atomic_bool_store(&child_executor_global->vm_interrupt, true);
}

static swow_siritz_wait_result_t swow_siritz_thread_wait(uv_thread_t thread, swow_siritz_run_t *run, int32_t timeout_ms, bool kill_after_timeout)
{
    CAT_LOG_DEBUG(THREADS, "wait for thread " CAT_LOG_THREAD_FMT ", timeout %d, kill_after_timeout %d", thread, timeout_ms, kill_after_timeout);

    swow_siritz_wait_result_t ret = SWOW_SIRITZ_WAIT_RESULT_SUCCESS;

#ifdef CAT_OS_WIN
    DWORD dword_ret;
    if (timeout_ms < 0) {
        // wait until thread is finished
        dword_ret = WaitForSingleObject((HANDLE)thread, INFINITE);
    } else {
        // wait for timeout_ms
        dword_ret = WaitForSingleObject((HANDLE)thread, (DWORD)timeout_ms);
    }
    // fprintf(stderr, "WaitForSingleObject: %d\n", dword_ret);
    switch (dword_ret) {
        case WAIT_OBJECT_0:
            ret = SWOW_SIRITZ_WAIT_RESULT_SUCCESS;
            break;
        case WAIT_TIMEOUT:
            ret = SWOW_SIRITZ_WAIT_RESULT_TIMEOUT;
            break;
        case WAIT_FAILED:
            /* fall through */
        default:
            ret = SWOW_SIRITZ_WAIT_RESULT_ERROR;
            break;
    }
    if (ret == SWOW_SIRITZ_WAIT_RESULT_TIMEOUT && kill_after_timeout) {
        // fprintf(stderr, "swow_siritz_thread_wait: thread %p timed out, killing\n", thread);
        CAT_LOG_DEBUG(THREADS, "swow_siritz_thread_wait: thread " CAT_LOG_THREAD_FMT " timed out, killing", thread);
        ret = SWOW_SIRITZ_WAIT_RESULT_KILLED;
        TerminateThread((HANDLE)thread, 0);
    }
#elif defined(CAT_OS_UNIX_LIKE)
    int int_ret;
    if (timeout_ms < 0) {
        // wait until thread is finished
        int_ret = pthread_join(thread, NULL);
    } else {
        // wait for timeout_ms then kill thread
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
        int_ret = pthread_timedjoin_np(thread, NULL, &ts);
    }
    // fprintf(stderr, "pthread_join/pthread_timedjoin_np: %d\n", int_ret);
    switch (int_ret) {
        case 0:
            ret = SWOW_SIRITZ_WAIT_RESULT_SUCCESS;
            break;
        case ETIMEDOUT:
            ret = SWOW_SIRITZ_WAIT_RESULT_TIMEOUT;
            break;
        default:
            ret = SWOW_SIRITZ_WAIT_RESULT_ERROR;
            errno = int_ret;
            break;
    }
    if (ret == SWOW_SIRITZ_WAIT_RESULT_TIMEOUT && kill_after_timeout) {
        CAT_LOG_DEBUG(THREADS, "swow_siritz_thread_wait: thread " CAT_LOG_THREAD_FMT " timed out, killing", thread);
        pthread_cancel(thread);
        pthread_join(thread, NULL);
        ret = SWOW_SIRITZ_WAIT_RESULT_KILLED;
    }
#else
    # error "Unsupported OS"
#endif

    CAT_LOG_DEBUG(THREADS, "swow_siritz_thread_wait: thread " CAT_LOG_THREAD_FMT " finished with result %d", thread, ret);
    return ret;
}

#define getThisSiritz(s) swow_siritz_t *s = swow_siritz_get_from_object(Z_OBJ_P(ZEND_THIS))

SWOW_API zend_class_entry *swow_siritz_ce;

SWOW_API zend_class_entry *swow_siritz_exception_ce;

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Siritz___construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, callable, IS_CALLABLE, 0)
    ZEND_ARG_VARIADIC_TYPE_INFO(0, data, IS_MIXED, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Siritz, __construct)
{
    getThisSiritz(s);
    zend_fcall_info fci = empty_fcall_info;
    zend_fcall_info_cache fcc = empty_fcall_info_cache;
    zval z_args;
    HashTable args;
    php_serialize_data_t var_hash;
    smart_str str_callable = {0};
    smart_str str_args = {0};

    ZEND_PARSE_PARAMETERS_START(1, -1)
        Z_PARAM_FUNC_EX(fci, fcc, 0, 0)
        Z_PARAM_VARIADIC('*', fci.params, fci.param_count)
    ZEND_PARSE_PARAMETERS_END();

    if (!fcc.function_handler) {
        zend_throw_exception(swow_siritz_exception_ce, "Invalid callable", 0);
        RETURN_THROWS();
    }

    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&str_callable, ZEND_CALL_ARG(execute_data, 1), &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);
    if (EG(exception)) {
        goto _cleanup;
    }

    if (str_callable.s == NULL) {
        // serialize failed
        zend_throw_exception(swow_siritz_exception_ce, "Invalid callable", 0);
        goto _cleanup;
    }

    zend_hash_init(&args, fci.param_count, NULL, NULL, 0);
    for (uint32_t i = 0; i < fci.param_count; i++) {
        zend_hash_next_index_insert_new(&args, &fci.params[i]);
    }
    ZVAL_ARR(&z_args, &args);

    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&str_args, &z_args, &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);
    zend_hash_destroy(&args);
    if (EG(exception)) {
        goto _cleanup;
    }

    if (str_args.s == NULL) {
        // serialize failed
        zend_throw_exception(swow_siritz_exception_ce, "Invalid args", 0);
        goto _cleanup;
    }

    // duplicate strings
    s->callable = malloc(str_callable.s->len);
    memcpy((unsigned char *)s->callable, str_callable.s->val, str_callable.s->len);
    s->callable_len = str_callable.s->len;
    s->args = malloc(str_args.s->len);
    memcpy((unsigned char *)s->args, str_args.s->val, str_args.s->len);
    s->args_len = str_args.s->len;

_cleanup:
    smart_str_free(&str_callable);
    smart_str_free(&str_args);
}

static void swow_siritz_run(swow_siritz_run_t *call)
{
    (void) ts_resource(0);

    ZEND_TSRMLS_CACHE_UPDATE();

    SG(server_context) = call->server_context;
    PG(expose_php)       = false;
    PG(auto_globals_jit) = true;

    if (php_request_startup() == FAILURE) {
        php_error_docref(NULL, E_ERROR, "Failed to startup request");
        return;
    }

    PG(during_request_startup) = false;
    SG(sapi_started) = false;
    SG(headers_sent) = true;
    SG(request_info).no_headers = true;
    php_register_variable("PHP_SELF", "-", NULL);

    zval z_code, z_args;
    const unsigned char *p;
    ZVAL_UNDEF(&z_code);
    ZVAL_UNDEF(&z_args);
    CAT_LOG_DEBUG_V3(THREADS, "run child thread with callable %.*s and args %.*s", (int)call->callable_len, (const char *)call->callable, (int)call->args_len, (const char *)call->args);
    zend_first_try {
        uv_mutex_lock(&call->mutex);
        uv_sem_post(&call->sem);
        call->status = SWOW_SIRITZ_THREAD_STATUS_RUNNING;
        uv_mutex_unlock(&call->mutex);

        php_unserialize_data_t var_hash;
        PHP_VAR_UNSERIALIZE_INIT(var_hash);
        p = call->callable;
        int ret = php_var_unserialize(
            &z_code,
            &p,
            (const unsigned char *)(p + call->callable_len),
            &var_hash
        );
        PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
        free((void *)call->callable);
        // php_var_dump(&z_code, 0);
        // fprintf(stderr, "unserialize callable: %d\n", ret);
        if (!ret || !swow_zval_is_closure(&z_code)) {
            zend_throw_exception_ex(swow_siritz_exception_ce,
                0, "Failed to unserialize callable: offset " ZEND_LONG_FMT " of %zd bytes",
                (zend_long)(p - call->callable), call->callable_len);
            zend_bailout();
        }

        PHP_VAR_UNSERIALIZE_INIT(var_hash);
        p = call->args;
        ret = php_var_unserialize(
            &z_args,
            &p,
            (const unsigned char *)(p + call->args_len),
            &var_hash
        );
        PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
        free((void *)call->args);
        if (!ret) {
            zend_throw_exception_ex(swow_siritz_exception_ce,
                0, "Failed to unserialize args: offset " ZEND_LONG_FMT " of %zd bytes",
                (zend_long)(p - call->args), call->args_len);
            zend_bailout();
        }

        uint32_t param_count = zend_hash_num_elements(Z_ARRVAL(z_args));

        if (param_count) {
            zend_hash_to_packed(Z_ARRVAL(z_args));
        }

        zval retval;
        // ZVAL_STRING(&z_code, "var_dump");
        zend_call_function(&(zend_fcall_info){
            .size = sizeof(zend_fcall_info),
            .retval = &retval,
            .function_name = z_code,
            .params = param_count ? Z_ARRVAL(z_args)->arPacked: 0,
            .param_count = param_count,
        }, NULL);

    } zend_end_try();

    uv_mutex_lock(&call->mutex);
    call->status = SWOW_SIRITZ_THREAD_STATUS_FINISHED;
    uv_mutex_unlock(&call->mutex);

    zval_ptr_dtor(&z_args);
    zval_ptr_dtor(&z_code);

    // fuck cli flaw
    void *fuck = sapi_module.deactivate;
    sapi_module.deactivate = NULL;
    php_request_shutdown(NULL);
    sapi_module.deactivate = fuck;

    ts_free_thread();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Siritz_run, 0, 0, IS_STATIC, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Siritz, run)
{
    getThisSiritz(s);
    int ret;

    if (s->thread) {
        zend_throw_exception(swow_siritz_exception_ce, "Thread already started", 0);
        RETURN_THROWS();
    }

    // fprintf(stderr, "%p %d %.*s\n", s->callable.s->val, s->callable.s->len, s->callable.s->len, s->callable.s->val);
    // initialize run struct
    swow_siritz_run_t *run = malloc(sizeof(*run));
    run->callable = s->callable;
    run->callable_len = s->callable_len;
    run->args = s->args;
    run->args_len = s->args_len;
    run->server_context = SG(server_context);
    run->status = SWOW_SIRITZ_THREAD_STATUS_NONE;
    ret = uv_mutex_init(&run->mutex);
    if (ret != 0) {
        // almost impossible
        swow_throw_exception(swow_siritz_exception_ce, 0, "Failed to init mutex: %s", uv_strerror(ret));
        RETURN_THROWS();
    }
    ret = uv_sem_init(&run->sem, 0);
    if (ret != 0) {
        // almost impossible
        swow_throw_exception(swow_siritz_exception_ce, 0, "Failed to init semaphore: %s", uv_strerror(ret));
        RETURN_THROWS();
    }

    ret = uv_thread_create_ex(&s->thread, (const uv_thread_options_t[]) {{
        .flags = UV_THREAD_HAS_STACK_SIZE,
        .stack_size = 1024 * 1024,
    }}, (void *)swow_siritz_run, (void *)run);
    if (ret != 0) {
        // almost impossible
        swow_throw_exception(swow_siritz_exception_ce, 0, "Failed to create thread: %s", uv_strerror(ret));
        RETURN_THROWS();
    }

    zend_hash_str_add_ptr(&SWOW_SIRITZ_G(threads), (const char *) &s->thread, sizeof(s->thread), run);
    CAT_LOG_DEBUG(THREADS, "add thread " CAT_LOG_THREAD_FMT " run %p, start waiting for semaphore", s->thread, run);
    uv_sem_wait(&run->sem);

    RETURN_THIS();
}

// returns enum for error code
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Siritz_wait, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, kill_after_timeout, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Siritz, wait)
{
    getThisSiritz(s);

    zend_long timeout = -1;
    zend_bool kill_after_timeout = false;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
        Z_PARAM_BOOL(kill_after_timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (!s->thread) {
        zend_throw_exception(swow_siritz_exception_ce, "Thread not started", 0);
        RETURN_THROWS();
    }

    swow_siritz_run_t *run = zend_hash_str_find_ptr(&SWOW_SIRITZ_G(threads), (const char *) &s->thread, sizeof(s->thread));
    if (!run) {
        // impossible
        zend_throw_exception(swow_siritz_exception_ce, "Thread not found", 0);
        RETURN_THROWS();
    }

    swow_siritz_wait_result_t ret = swow_siritz_thread_wait(s->thread, run, timeout, kill_after_timeout);
    switch (ret) {
        case SWOW_SIRITZ_WAIT_RESULT_TIMEOUT:
            zend_throw_exception(swow_siritz_exception_ce, "Wait for thread timed out", 0);
            RETURN_THROWS();
        case SWOW_SIRITZ_WAIT_RESULT_ERROR:
            zend_throw_exception(swow_siritz_exception_ce, "Failed to wait for thread", 0);
            RETURN_THROWS();
        default:
            // otherwise, success
            break;
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Siritz_getTid, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Siritz, getTid)
{
    getThisSiritz(s);

    if (!s->thread) {
        zend_throw_exception(swow_siritz_exception_ce, "Thread not started", 0);
        RETURN_THROWS();
    }

#ifdef CAT_OS_WIN
    RETURN_LONG(GetThreadId((HANDLE)s->thread));
#elif defined(CAT_OS_DARWIN)
    ino_t tid;
    pthread_threadid_np(s->thread, &tid);
    RETURN_LONG(tid);
#elif defined(CAT_OS_LINUX)
    RETURN_LONG(s->thread);
#else
# error "Unsupported OS"
#endif
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_getmytid, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(swow_getmytid) {
#ifdef CAT_OS_WIN
    RETURN_LONG(GetCurrentThreadId());
#elif defined(CAT_OS_DARWIN)
    ino_t tid;
    pthread_threadid_np(pthread_self(), &tid);
    RETURN_LONG(tid);
#elif defined(CAT_OS_LINUX)
    RETURN_LONG(gettid());
#else
# error "Unsupported OS"
#endif
}

SWOW_API zend_object_handlers swow_siritz_handlers;

static zend_object *swow_siritz_create_object(zend_class_entry *ce)
{
    swow_siritz_t *s = swow_object_alloc(swow_siritz_t, ce, swow_siritz_handlers);
    memset(&s->callable, 0, sizeof(s->callable));
    memset(&s->args, 0, sizeof(s->args));
    memset(&s->thread, 0, sizeof(s->thread));

    return &s->std;
}

static void swow_siritz_free_object(zend_object *object)
{
    swow_siritz_t *s = swow_siritz_get_from_object(object);
    if (!s->thread) {
        // if the thread is not started, free the callable and args
        free((void *)s->callable);
        free((void *)s->args);
    }
    // otherwise, let child thread free it
}

static const zend_function_entry swow_siritz_methods[] = {
    PHP_ME(Swow_Siritz, __construct, arginfo_class_Swow_Siritz___construct, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Siritz, run, arginfo_class_Swow_Siritz_run, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Siritz, wait, arginfo_class_Swow_Siritz_wait, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Siritz, getTid, arginfo_class_Swow_Siritz_getTid, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry swow_siritz_functions[] = {
    PHP_FENTRY(getmytid, PHP_FN(swow_getmytid), arginfo_swow_getmytid, 0)
    PHP_FE_END
};

zend_result swow_siritz_module_init(INIT_FUNC_ARGS)
{
    // zend_class_entry ce;
    CAT_GLOBALS_REGISTER(swow_siritz);

    swow_siritz_ce = swow_register_internal_class(
        "Swow\\Siritz", NULL, swow_siritz_methods,
        &swow_siritz_handlers, NULL,
        cat_false, cat_false,
        swow_siritz_create_object, swow_siritz_free_object,
        XtOffsetOf(swow_siritz_t, std)
    );

    swow_siritz_exception_ce = swow_register_internal_class(
        "Swow\\SiritzException", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    if (!swow_hook_internal_functions(swow_siritz_functions)) {
        return FAILURE;
    }

    if (original_zend_interrupt_function == (swow_interrupt_function_t) -1) {
        original_zend_interrupt_function = zend_interrupt_function;
        zend_interrupt_function = swow_siritz_interrupt_function;
    }

    return SUCCESS;
}

zend_result swow_siritz_runtime_init(INIT_FUNC_ARGS)
{
    zend_hash_init(&SWOW_SIRITZ_G(threads), 0, NULL, NULL, 1);

    return SUCCESS;
}

zend_result swow_siritz_runtime_shutdown(INIT_FUNC_ARGS)
{
    ZEND_HASH_REVERSE_FOREACH_STR_KEY_VAL(&SWOW_SIRITZ_G(threads), zend_string *strkey, zval *zv) {
        swow_siritz_run_t *run = (swow_siritz_run_t *)Z_PTR_P(zv);
        uv_thread_t thread = *(uv_thread_t *)strkey->val;
        bool needs_wait = false;

        CAT_LOG_DEBUG(THREADS, "siritz runtime shutdown: wait for thread " CAT_LOG_THREAD_FMT " run %p", thread, run);

        uv_mutex_lock(&run->mutex);
        if (run->status == SWOW_SIRITZ_THREAD_STATUS_RUNNING) {
            swow_siritz_thread_interrupt(thread);
            needs_wait = true;
        }
        uv_mutex_unlock(&run->mutex);

        if (needs_wait) {
            if (SWOW_G(ini.thread_exit_join_ms) >= 0) {
                swow_siritz_thread_wait(thread, run, SWOW_G(ini.thread_exit_join_ms), true);
            } else {
                swow_siritz_thread_wait(thread, run, -1, false);
            }
        }

        uv_mutex_destroy(&run->mutex);
        uv_sem_destroy(&run->sem);
        free(run);
    } ZEND_HASH_FOREACH_END();

    zend_hash_destroy(&SWOW_SIRITZ_G(threads));

    return SUCCESS;
}

zend_result swow_siritz_module_shutdown(INIT_FUNC_ARGS)
{
    CAT_GLOBALS_UNREGISTER(swow_siritz);

    return SUCCESS;
}

#endif // ZTS
