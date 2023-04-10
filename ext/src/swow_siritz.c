#include "swow.h"
#include "SAPI.h"
#include "php_main.h"
#include "swow_siritz.h"
#include "swow_closure.h"

#define getThisSiritz(s) swow_siritz_t *s = swow_siritz_get_from_object(Z_OBJ_P(ZEND_THIS))

SWOW_API zend_class_entry *swow_siritz_ce;

SWOW_API zend_class_entry *swow_siritz_exception_ce;

static HashTable siritz;

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Siritz___construct, 0, 1, 0)
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

    smart_str str_callable = {0};
    // smart_str str_callable_dup = {0};
    php_serialize_data_t var_hash;
    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&str_callable, ZEND_CALL_ARG(execute_data, 1), &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);

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

    zend_hash_destroy(&args);

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
            &p,
            pe,
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
            &p,
            pe,
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
    memcpy(run, (const swow_siritz_run_t[]){{
        .callable = s->callable,
        .args = s->args,
        .server_context = SG(server_context),
    }}, sizeof(*run));
    
    int ret = uv_thread_create_ex(&s->thread, (const uv_thread_options_t[]) {{
        .flags = UV_THREAD_HAS_STACK_SIZE,
        .stack_size = 1024 * 1024,
    }}, swow_siritz_run, run);
    if (ret != 0) {
        zend_throw_exception_ex(swow_siritz_exception_ce, 0, "Failed to create thread: %s", uv_strerror(ret));
        return;
    }

    // printf("add thread %p\n", s->thread);
    zend_hash_str_add_ptr(&siritz, (const char *) &s->thread, sizeof(s->thread), "running");

    RETURN_THIS();
}

// returns enum for error code
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Siritz_wait, 0, 0, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, kill, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Siritz, wait)
{
    getThisSiritz(s);

    zend_long timeout = -1;
    zend_bool kill = false;

    ZEND_PARSE_PARAMETERS_START(1, -1)
        Z_PARAM_LONG(timeout)
        Z_PARAM_BOOL(kill)
    ZEND_PARSE_PARAMETERS_END();

    // todo: use uv_thread and mutex things

    DWORD ret = WaitForSingleObject((HANDLE)s->thread, timeout);

    if (kill) {
        TerminateThread((HANDLE)s->thread, 0);
    }

    if (ret == WAIT_OBJECT_0 || kill) {
        // printf("remove thread %p\n", s->thread);
        zend_hash_str_del(&siritz, (const char *) &s->thread, sizeof(s->thread));
    }

    RETURN_LONG(ret);
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
    PHP_FE_END
};

zend_result swow_siritz_module_init(INIT_FUNC_ARGS)
{
    zend_class_entry ce;

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

    zend_hash_init(&siritz, 0, NULL, NULL, 1);

    return SUCCESS;
}

zend_result swow_siritz_module_shutdown(INIT_FUNC_ARGS)
{

    ZEND_HASH_REVERSE_FOREACH_STR_KEY(&siritz, HANDLE t) {
        // printf("wait for thread %p\n", t);
        DWORD ret = WaitForSingleObject(t, 1000/* TODO: configurable */);
        if (ret == WAIT_TIMEOUT) {
            TerminateThread(t, 0);
        }
    } ZEND_HASH_FOREACH_END();

    return SUCCESS;
}

