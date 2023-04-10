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

#include "swow_thread.h"
#ifdef ZTS

#include "swow_closure.h"
#include "cat_coroutine.h"

#include "zend_closures.h"
#include "php_main.h"
#include "SAPI.h"

SWOW_API zend_class_entry *swow_thread_ce;
SWOW_API zend_object_handlers swow_thread_object_handlers;

SWOW_API zend_class_entry *swow_thread_exception_ce;

static HashTable swow_thread_map;
static uv_mutex_t swow_thread_map_mutex;

SWOW_API CAT_GLOBALS_DECLARE(swow_thread);

static swow_thread_t *swow_thread_create(void)
{
    return NULL;
}

static void swow_thread_addref(swow_thread_t *s_thread)
{
    CAT_TS_REF_ADD(s_thread);
}

static void swow_thread_release(swow_thread_t *s_thread)
{
    uv_mutex_lock(&swow_thread_map_mutex);
    if (CAT_TS_REF_DEL(s_thread) != 0) {
        goto _out;
    }
    zend_hash_index_del(&swow_thread_map, s_thread->thread.id);
    pefree(s_thread, true);
    _out:
    uv_mutex_unlock(&swow_thread_map_mutex);
}

static zend_object *swow_thread_create_object(zend_class_entry *ce)
{
    swow_thread_object_t *s_thread_object = swow_object_alloc(swow_thread_object_t, ce, swow_thread_object_handlers);
    s_thread_object->s_thread = NULL;
    return &s_thread_object->std;
}

static void swow_thread_free_object(zend_object *object)
{
    swow_thread_object_t *s_thread_object = swow_thread_object_get_from_object(object);
    swow_thread_t *s_thread = s_thread_object->s_thread;
    if (s_thread != NULL) {
        swow_thread_release(s_thread);
    }
    zend_object_std_dtor(&s_thread_object->std);
}

static zend_always_inline void swow_thread_internal_register(swow_thread_t *s_thread)
{
    uv_mutex_lock(&swow_thread_map_mutex);
    zend_hash_index_add_new_ptr(&swow_thread_map, s_thread->thread.id, s_thread);
    uv_mutex_unlock(&swow_thread_map_mutex);
}

static zend_always_inline void swow_thread_internal_unregister(swow_thread_t *s_thread)
{
    uv_mutex_lock(&swow_thread_map_mutex);
    zend_hash_index_del(&swow_thread_map, s_thread->thread.id);
    uv_mutex_unlock(&swow_thread_map_mutex);
}

/* cross thread resource manager things */

typedef struct swow_thread_creation_context_s {
    zend_string *closure_hash;
    void *server_context;
} swow_thread_creation_context_t;

static void swow_thread_creation_context_release_task(cat_data_t *data)
{
    swow_thread_creation_context_t *context = (swow_thread_creation_context_t *) data;
    zend_string_release(context->closure_hash);
    efree(context);
}

static void swow_thread_creation_context_release(swow_thread_t *s_thread, swow_thread_creation_context_t *context)
{
    ZEND_ASSERT(s_thread != swow_thread_get_current());
    (void) cat_thread_dispatch_task(&s_thread->thread, swow_thread_creation_context_release_task, context);
}

/* runtime */

static void swow_thread_routine(cat_data_t *arg)
{
    ts_resource(0);
    TSRMLS_CACHE_UPDATE();

    swow_thread_creation_context_t *creation_context = (swow_thread_creation_context_t *) arg;
    swow_thread_t *s_thread = swow_thread_get_current();
    swow_thread_t *parent_s_thread = swow_thread_get_parent(s_thread);

    SG(server_context) = creation_context->server_context;
    PG(expose_php) = false;
    PG(auto_globals_jit) = true;

    // FIXME: hook these functions and disable them in sub-threads
    // zend_disable_functions("setlocale, dl");

    if (php_request_startup() != SUCCESS) {
	    EG(exit_status) = 1;
        goto _startup_error;
    }

    PG(during_request_startup)  = false;
    SG(sapi_started) = false;
    SG(headers_sent) = true;
    SG(request_info).no_headers = true;

    s_thread->vm_interrupt_ptr = &EG(vm_interrupt);

    zend_first_try {
        php_unserialize_data_t var_hash;
        PHP_VAR_UNSERIALIZE_INIT(var_hash);
        zval z_closure;
        const char *p = ZSTR_VAL(creation_context->closure_hash);
        const char *pe = p + ZSTR_LEN(creation_context->closure_hash);
        int result = php_var_unserialize(&z_closure, (const unsigned char **) &p, (const unsigned char *) pe, &var_hash);
        PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
        if (!result || !swow_zval_is_closure(&z_closure)) {
            // TODO: report error
            goto _unserialize_error;
        }

        swow_thread_creation_context_release(parent_s_thread, creation_context);

        zend_fcall_info fci;
        zval retval;
        fci.size = sizeof(fci);
        ZVAL_COPY_VALUE(&fci.function_name, &z_closure);
        fci.object = NULL;
        fci.param_count = 0;
        fci.named_params = NULL;
        fci.retval = &retval;

        zend_call_function(&fci, NULL);

        zval_ptr_dtor(&z_closure);
    } zend_end_try();

    php_request_shutdown(NULL);

    _unserialize_error:
    _startup_error:
    s_thread->exit_status = EG(exit_status);
    ts_free_thread();
}

#define getThisThread() (swow_thread_get_from_object(Z_OBJ_P(ZEND_THIS)))
#define getThisThreadObject() (swow_thread_object_get_from_object(Z_OBJ_P(ZEND_THIS)))

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Thread___construct, 0, 0, 1)
    ZEND_ARG_OBJ_TYPE_MASK(0, closure, Closure, MAY_BE_STRING, NULL)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thread, __construct)
{
    swow_thread_object_t *s_thread_object = getThisThreadObject();
    swow_thread_t *s_thread = s_thread_object->s_thread;
    cat_thread_t *thread;
    zend_object *closure;

    if (UNEXPECTED(s_thread != NULL)) {
        zend_throw_error(NULL, "%s can be constructed only once", ZEND_THIS_NAME);
        RETURN_THROWS();
    }

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJ_OF_CLASS(closure, zend_ce_closure)
    ZEND_PARSE_PARAMETERS_END();

    zend_string *closure_hash = swow_serialize(ZEND_CALL_ARG(execute_data, 1));
    if (closure_hash == NULL) {
        RETURN_THROWS();
    }

    swow_thread_creation_context_t *creation_context;
    creation_context = (swow_thread_creation_context_t *) emalloc(sizeof(*creation_context));
    creation_context->server_context = SG(server_context);
    creation_context->closure_hash = closure_hash;

    s_thread = (swow_thread_t *) pemalloc(sizeof(swow_thread_t), true);
    thread = cat_thread_create(&s_thread->thread, swow_thread_routine, creation_context);
    if (UNEXPECTED(thread == NULL)) {
        swow_throw_call_exception_with_last(swow_thread_exception_ce);
        goto _thread_create_error;
    }
    s_thread->exit_status = 233; // TODO: use constant
    uv_mutex_lock(&swow_thread_map_mutex);
    zend_hash_index_add_new_ptr(&swow_thread_map, s_thread->thread.id, s_thread);
    uv_mutex_unlock(&swow_thread_map_mutex);

    return;

    _thread_create_error:
    efree(creation_context);
    RETURN_THROWS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Thread_join, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thread, join)
{
    swow_thread_t *s_thread = getThisThread();

    ZEND_PARSE_PARAMETERS_NONE();

    cat_thread_join(&s_thread->thread);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_TYPE_MASK_EX(arginfo_class_Swow_Thread_getCurrent, 0, 0, Swow\\Thread, MAY_BE_STATIC)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Thread, getCurrent)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_OBJ(&swow_thread_object_get_from_handle(swow_thread_get_current())->std);
}

#define arginfo_class_Swow_Thread_getParent arginfo_class_Swow_Thread_getCurrent

static PHP_METHOD(Swow_Thread, getParent)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_OBJ(&swow_thread_object_get_from_handle(swow_thread_get_parent(swow_thread_get_current()))->std);
}

static const zend_function_entry swow_thread_methods[] = {
    PHP_ME(Swow_Thread, __construct, arginfo_class_Swow_Thread___construct, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, join,        arginfo_class_Swow_Thread_join,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Thread, getCurrent,  arginfo_class_Swow_Thread_getCurrent,  ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Thread, getParent,   arginfo_class_Swow_Thread_getParent,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

static cat_thread_t *original_main_thread;

static void swow_thread_main_create(void)
{
    swow_thread_t *s_thread = swow_thread_get_from_object(
        swow_object_create(swow_thread_ce)
    );
    original_main_thread = cat_thread_register_main(&s_thread->thread);
    uv_mutex_lock(&swow_thread_map_mutex);
    zend_hash_index_add_new_ptr(&swow_thread_map, s_thread->thread.id, s_thread);
    uv_mutex_unlock(&swow_thread_map_mutex);
}

static void swow_thread_main_close(void)
{
    swow_thread_t *s_thread =  swow_thread_get_from_handle(
        cat_thread_register_main(original_main_thread)
    );

    // it should be called in thread_join_all()
    // swow_thread_internal_unregister(s_thread);
    zend_object_release(&s_thread->std);
}

static void swow_thread_join_all(void)
{
    cat_thread_t *current_thread = cat_thread_get_current();
    HashTable *dup;
    void *ptr;

    while (current_thread->child_count != 0) {
        uv_mutex_lock(&swow_thread_map_mutex);
        dup = zend_array_dup(&swow_thread_map);
        uv_mutex_unlock(&swow_thread_map_mutex);
        ZEND_HASH_FOREACH_PTR(dup, ptr) {
            swow_thread_t *sub_s_thread = (swow_thread_t *) ptr;
            if (sub_s_thread->thread.parent == current_thread) {
                continue;
            }
            cat_thread_join(&sub_s_thread->thread);
        } ZEND_HASH_FOREACH_END();
        zend_array_release_gc(dup);
    }
}

static void swow_thread_map_destroy(zval *z_thread)
{

}

zend_result swow_thread_module_init(INIT_FUNC_ARGS)
{
    CAT_GLOBALS_REGISTER(swow_thread);

    if (!cat_thread_module_init()) {
        return FAILURE;
    }

    swow_thread_ce = swow_register_internal_class(
        "Swow\\Thread", NULL, swow_thread_methods,
        &swow_thread_object_handlers, NULL,
        cat_false, cat_false,
        swow_thread_create_object,
        swow_thread_free_object,
        XtOffsetOf(swow_thread_t, std)
    );

    swow_thread_exception_ce = swow_register_internal_class(
        "Swow\\ThreadException", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    zend_hash_init(&swow_thread_map, 8, NULL, swow_thread_map_destroy, true);

    return SUCCESS;
}

zend_result swow_thread_module_shutdown(INIT_FUNC_ARGS)
{
    CAT_GLOBALS_UNREGISTER(swow_thread);

    return SUCCESS;
}

zend_result swow_thread_runtime_init(INIT_FUNC_ARGS)
{
    swow_thread_t *s_thread = swow_thread_get_current();

    if (!cat_thread_runtime_init()) {
        return FAILURE;
    }
        /* create s_coroutine map */
    do {
        zval z_tmp;
        array_init(&z_tmp);
        SWOW_THREAD_G(map) = Z_ARRVAL(z_tmp);
    } while (0);

    if (s_thread->thread.id == CAT_THREAD_MAIN_ID) {
        swow_thread_main_create();
    }

    return SUCCESS;
}

zend_result swow_thread_runtime_shutdown(INIT_FUNC_ARGS)
{
    swow_thread_t *s_thread = swow_thread_get_current();

    swow_thread_join_all();
    swow_thread_internal_unregister(s_thread, SWOW_THREAD_G(map));
    zend_array_release_gc(SWOW_THREAD_G(map));
    SWOW_THREAD_G(map) = NULL;

    if (!cat_thread_runtime_shutdown()) {
        return FAILURE;
    }

    /* something like async handle have not been closed totally without this,
     * wait for event loop to clear all closed handles. */
    cat_coroutine_wait_all();

    if (s_thread->thread.id == CAT_THREAD_MAIN_ID) {
        swow_thread_main_close();
    } else {
        cat_thread_dispatch_task(s_thread->thread.parent, swow_thread_release_object_task, s_thread);
    }

    return SUCCESS;
}

#endif /* ZTS */
