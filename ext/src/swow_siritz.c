#include "swow.h"
#include "SAPI.h"
#include "php_main.h"
#include "php_variables.h"
#include "swow_siritz.h"
#include "swow_closure.h"
#include "swow_hook.h"

#ifdef ZTS

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
    if (SWOW_SIRITZ_G(parent_thread_exiting)) {
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

    ZEND_PARSE_PARAMETERS_START(1, -1)
        Z_PARAM_FUNC_EX(fci, fcc, 0, 0)
        Z_PARAM_VARIADIC('*', fci.params, fci.param_count)
    ZEND_PARSE_PARAMETERS_END();

    if (!fcc.function_handler) {
        zend_throw_exception(swow_siritz_exception_ce, "Invalid callable", 0);
        return;
    }

    php_serialize_data_t var_hash;

    smart_str str_callable = {0};
    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&str_callable, ZEND_CALL_ARG(execute_data, 1), &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);
    if (EG(exception)) {
        return;
    }

    if (str_callable.s == NULL) {
        // serialize failed
        zend_throw_exception(swow_siritz_exception_ce, "Invalid callable", 0);
        return;
    }

    zval z_args;
    HashTable args;
    zend_hash_init(&args, fci.param_count, NULL, ZVAL_PTR_DTOR, 0);
    for (uint32_t i = 0; i < fci.param_count; i++) {
        zend_hash_next_index_insert(&args, &fci.params[i]);
    }
    ZVAL_ARR(&z_args, &args);

    smart_str str_args = {0};
    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&str_args, &z_args, &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);
    if (EG(exception)) {
        return;
    }

    zend_hash_destroy(&args);

    if (str_args.s == NULL) {
        // serialize failed
        zend_throw_exception(swow_siritz_exception_ce, "Invalid args", 0);
        return;
    }

    s->callable.s = NULL;
    smart_str_appendl_ex(&s->callable, str_callable.s->val, str_callable.s->len, 1);
    smart_str_free(&str_callable);

    s->args.s = NULL;
    smart_str_appendl_ex(&s->args, str_args.s->val, str_args.s->len, 1);
    smart_str_free(&str_args);
}

SWOW_API void swow_siritz_run(swow_siritz_run_t *call)
{
    ts_resource(0);
#ifdef PHP_WIN32
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

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
    // printf("%p %d %.*s\n", call->callable.s->val, call->callable.s->len, call->callable.s->len, call->callable.s->val);
    zend_first_try {

        php_unserialize_data_t var_hash;
        PHP_VAR_UNSERIALIZE_INIT(var_hash);
        const char *p = call->callable.s->val;
        const char *pe =  call->callable.s->val + call->callable.s->len;
        int ret = php_var_unserialize(
            &z_code,
            (const unsigned char **)&p,
            (const unsigned char *)pe,
            &var_hash
        );
        PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
        // php_var_dump(&z_code, 0);
        // printf("%d\n", ret);

        if (!ret || !swow_zval_is_closure(&z_code)) {
            php_error_docref(NULL, E_ERROR, "Failed to unserialize callable: offset " ZEND_LONG_FMT " of %zd bytes",
                (zend_long)((char*)p - call->callable.s->val), call->callable.s->len);
            return;
        }

        PHP_VAR_UNSERIALIZE_INIT(var_hash);
        p = call->args.s->val;
        pe =  call->args.s->val + call->args.s->len;
        ret = php_var_unserialize(
            &z_args,
            (const unsigned char **)&p,
            (const unsigned char *)pe,
            &var_hash
        );
        PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
        if (!ret) {
            php_error_docref(NULL, E_ERROR, "Failed to unserialize args: offset " ZEND_LONG_FMT " of %zd bytes",
                (zend_long)((char*)p - call->args.s->val), call->args.s->len);
            return;
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

        zval_ptr_dtor(&z_code);
    } zend_end_try();

    // fuck cli flaw
    void *fuck = sapi_module.deactivate;
    sapi_module.deactivate = NULL;
    php_request_shutdown(NULL);
    sapi_module.deactivate = fuck;

    ts_free_thread();

    free(call);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Siritz_run, 0, 0, IS_STATIC, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Siritz, run)
{
    getThisSiritz(s);

    // printf("%p %d %.*s\n", s->callable.s->val, s->callable.s->len, s->callable.s->len, s->callable.s->val);
    const swow_siritz_run_t *run = malloc(sizeof(*run));
    memcpy((void *)run, (const swow_siritz_run_t[]){{
        .callable = s->callable,
        .args = s->args,
        .server_context = SG(server_context),
    }}, sizeof(*run));

    int ret = uv_thread_create_ex(&s->thread, (const uv_thread_options_t[]) {{
        .flags = UV_THREAD_HAS_STACK_SIZE,
        .stack_size = 1024 * 1024,
    }}, (void *)swow_siritz_run, (void *)run);
    if (ret != 0) {
        swow_throw_exception(swow_siritz_exception_ce, 0, "Failed to create thread: %s", uv_strerror(ret));
        return;
    }

    // printf("add thread %p\n", s->thread);
    zend_hash_str_add_ptr(&SWOW_SIRITZ_G(threads), (const char *) &s->thread, sizeof(s->thread), "running");

    RETURN_THIS();
}

// returns enum for error code
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Siritz_wait, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, kill, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Siritz, wait)
{
    getThisSiritz(s);

    zend_long timeout = 0;
    zend_bool kill = false;
    zend_bool timeout_is_null = true;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(timeout, timeout_is_null)
        Z_PARAM_BOOL(kill)
    ZEND_PARSE_PARAMETERS_END();

    bool done = false;

    if (!s->thread) {
        zend_throw_exception(swow_siritz_exception_ce, "Thread not started", 0);
        RETURN_THROWS();
    }

    // maybetodo: use uv_thread and mutex things
#ifdef CAT_OS_WIN
    if (timeout_is_null) {
        timeout = INFINITE;
    }
    DWORD ret = WaitForSingleObject((HANDLE)s->thread, timeout);
    if (ret == WAIT_OBJECT_0) {
        done = true;
    }

    if (kill) {
        TerminateThread((HANDLE)s->thread, 0);
        done = true;
    }
#elif defined(CAT_OS_UNIX_LIKE)

    if (timeout_is_null) {
        pthread_join(s->thread, NULL);
        done = true;
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout / 1000;
        ts.tv_nsec += (timeout % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }

        int ret = pthread_timedjoin_np(s->thread, NULL, &ts);
        if (ret == 0) {
            done = true;
        }
    }

    if (kill) {
        pthread_cancel(s->thread);
        done = true;
    }
#else
# error "Unsupported OS"
#endif

    if (done) {
        // printf("remove thread %p\n", s->thread);
        zend_hash_str_del(&SWOW_SIRITZ_G(threads), (const char *) &s->thread, sizeof(s->thread));
    } else {
        zend_throw_exception(swow_siritz_exception_ce, "Wait for thread timed out", 0);
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
        swow_siritz_create_object, NULL,
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
    SWOW_SIRITZ_G(parent_thread_exiting) = 0;

    return SUCCESS;
}

zend_result swow_siritz_runtime_shutdown(INIT_FUNC_ARGS)
{
    ZEND_HASH_REVERSE_FOREACH_STR_KEY(&SWOW_SIRITZ_G(threads), zend_string *strkey) {

#ifdef CAT_OS_WIN
        HANDLE t = *(HANDLE *)strkey->val;
        // printf("rshutdown: wait for thread %d\n", GetThreadId(t));

        // interrupt threads using vm_interrupt
        THREAD_T phpThread = GetThreadId(t); // for Windows, php use thread id as thread handle
        zend_executor_globals *child_executor_global =
            (zend_executor_globals *)ts_resource_ex(executor_globals_id, &phpThread);
        zend_atomic_bool_store(&child_executor_global->vm_interrupt, true);

        if (SWOW_G(ini.thread_exit_join_ms) < 0) {
            WaitForSingleObject(t, INFINITE);
        } else {
            WaitForSingleObject(t, (DWORD)SWOW_G(ini.thread_exit_join_ms));
        }
#elif defined(CAT_OS_UNIX_LIKE)
        pthread_t t = *(pthread_t *)strkey->val;

        // interrupt threads using vm_interrupt
        // at pthread OS, php use pthread_t as thread handle
        zend_executor_globals *child_executor_global =
            (zend_executor_globals *)ts_resource_ex(executor_globals_id, &t);
        zend_atomic_bool_store(&child_executor_global->vm_interrupt, true);

        if (SWOW_G(ini.thread_exit_join_ms) < 0) {
            pthread_join(t, NULL);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            ts.tv_sec += SWOW_G(ini.thread_exit_join_ms) / 1000;
            ts.tv_nsec += (SWOW_G(ini.thread_exit_join_ms) % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000;
            }
            pthread_timedjoin_np(t, NULL, &ts);
        }
#else
# error "Unsupported OS"
#endif
    } ZEND_HASH_FOREACH_END();

    return SUCCESS;
}

zend_result swow_siritz_module_shutdown(INIT_FUNC_ARGS)
{
    CAT_GLOBALS_UNREGISTER(swow_siritz);

    return SUCCESS;
}

#endif // ZTS
