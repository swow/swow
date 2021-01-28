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

#include "swow_coroutine.h"

#include "swow_debug.h"

#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
#define E_MAGIC (1 << 31)
#endif

SWOW_API zend_class_entry *swow_coroutine_ce;
SWOW_API zend_object_handlers swow_coroutine_handlers;

SWOW_API zend_class_entry *swow_coroutine_exception_ce;
SWOW_API zend_class_entry *swow_coroutine_cross_exception_ce;
SWOW_API zend_class_entry *swow_coroutine_term_exception_ce;
SWOW_API zend_class_entry *swow_coroutine_kill_exception_ce;

SWOW_API CAT_GLOBALS_DECLARE(swow_coroutine)

CAT_GLOBALS_CTOR_DECLARE_SZ(swow_coroutine)

#define SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, update_last_error, failure) do { \
    if (UNEXPECTED(!swow_coroutine_is_executing(scoroutine))) { \
        if (update_last_error) { \
            cat_update_last_error(CAT_ESRCH, "Coroutine is not in executing"); \
        } \
        failure; \
    } \
} while (0)

/* pre declare */
static cat_bool_t swow_coroutine_construct(swow_coroutine_t *scoroutine, zval *zcallable, size_t stack_page_size, size_t c_stack_size);
static void swow_coroutine_shutdown(swow_coroutine_t *scoroutine);
static void swow_coroutine_handle_cross_exception(zend_object *cross_exception);
static cat_bool_t swow_coroutine_resume_deny(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval);

static cat_always_inline size_t swow_coroutine_align_stack_page_size(size_t size)
{
    if (size == 0) {
        size = SWOW_COROUTINE_G(default_stack_page_size);
    } else if (UNEXPECTED(size < CAT_COROUTINE_MIN_STACK_SIZE)) {
        size = SWOW_COROUTINE_MIN_STACK_PAGE_SIZE;
    } else if (UNEXPECTED(size > CAT_COROUTINE_MAX_STACK_SIZE)) {
        size = SWOW_COROUTINE_MAX_STACK_PAGE_SIZE;
    } else {
        size = CAT_MEMORY_ALIGNED_SIZE_EX(size, SWOW_COROUTINE_STACK_PAGE_ALIGNED_SIZE);
    }

    return size;
}

static zend_object *swow_coroutine_create_object(zend_class_entry *ce)
{
    swow_coroutine_t *scoroutine = swow_object_alloc(swow_coroutine_t, ce, swow_coroutine_handlers);

    cat_coroutine_init(&scoroutine->coroutine);

    scoroutine->executor = NULL;
    scoroutine->exit_status = 0;

    return &scoroutine->std;
}

static void swow_coroutine_dtor_object(zend_object *object)
{
    /* force kill the coroutine if it is still alive */
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_object(object);

    while (UNEXPECTED(swow_coroutine_is_alive(scoroutine))) {
        /* not finished, should be discard */
        if (UNEXPECTED(!swow_coroutine_kill(scoroutine, NULL, ~0))) {
            cat_core_error(COROUTINE, "Kill coroutine failed when destruct object, reason: %s", cat_get_last_error_message());
        }
    }
}

static void swow_coroutine_free_object(zend_object *object)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_object(object);

    if (UNEXPECTED(swow_coroutine_is_available(scoroutine))) {
        /* created but never run (or it is main coroutine) */
        swow_coroutine_shutdown(scoroutine);
    }

    zend_object_std_dtor(&scoroutine->std);
}

static CAT_COLD cat_bool_t swow_coroutine_exception_should_be_silent(zend_object *exception)
{
    zval *zprevious_exception, ztmp;

    while (1) {
        ZVAL7_ALLOC_OBJECT(exception);
        if (instanceof_function(exception->ce, swow_coroutine_term_exception_ce)) {
            zval *zcode, ztmp;
            zcode = zend_read_property_ex(exception->ce, ZVAL7_OBJECT(exception), ZSTR_KNOWN(ZEND_STR_CODE), 1, &ztmp);
            if (zval_get_long(zcode) == 0) {
                return cat_true;
            }
            return cat_false;
        } else if (instanceof_function(exception->ce, swow_coroutine_kill_exception_ce)) {
            return cat_true;
        }
        zprevious_exception = zend_read_property_ex(zend_get_exception_base(ZVAL7_OBJECT(exception)), ZVAL7_OBJECT(exception), ZSTR_KNOWN(ZEND_STR_PREVIOUS), 1, &ztmp);
        if (Z_TYPE_P(zprevious_exception) == IS_OBJECT) {
            exception = Z_OBJ_P(zprevious_exception);
            continue;
        }
        break;
    }

    return cat_false;
}

static CAT_COLD void swow_coroutine_function_handle_exception(void)
{
    CAT_ASSERT(EG(exception) != NULL);

    zend_exception_restore();

    /* keep silent for killer and term(0) */
    if (swow_coroutine_exception_should_be_silent(EG(exception))) {
        OBJ_RELEASE(EG(exception));
        EG(exception) = NULL;
        return;
    }

    if (Z_TYPE(EG(user_exception_handler)) != IS_UNDEF) {
        zval origin_user_exception_handler;
        zval param, retval;
        zend_object *old_exception;
        old_exception = EG(exception);
        EG(exception) = NULL;
        ZVAL_OBJ(&param, old_exception);
        ZVAL_COPY_VALUE(&origin_user_exception_handler, &EG(user_exception_handler));
        if (call_user_function(CG(function_table), NULL, &origin_user_exception_handler, &retval, 1, &param) == SUCCESS) {
            zval_ptr_dtor(&retval);
            if (EG(exception)) {
                if (EG(exception) == old_exception) {
                    GC_DELREF(old_exception);
                } else {
                    zend_exception_set_previous(EG(exception), old_exception);
                }
            }
        }
        if (EG(exception) == NULL) {
            EG(exception) = old_exception;
        }
    }

    if (EG(exception)) {
        zend_long severity = SWOW_COROUTINE_G(exception_error_severity);
        if (severity > E_NONE) {
            zend_exception_error(EG(exception), severity);
        } else {
            OBJ_RELEASE(EG(exception));
            EG(exception) = NULL;
        }
    }
}

static zval *swow_coroutine_function(zval *zdata)
{
    static const zend_execute_data dummy_execute_data;
    swow_coroutine_t *scoroutine = swow_coroutine_get_current();
    swow_coroutine_executor_t *executor = scoroutine->executor;
    zval zcallable = executor->zcallable;
    zend_fcall_info fci;
    zval retval;

    CAT_ASSERT(executor != NULL);

    /* add to scoroutines map (we can not add beofre run otherwise refcount would never be 0) */
    do {
        zval ztmp;
        ZVAL_OBJ(&ztmp, &scoroutine->std);
        zend_hash_index_update(SWOW_COROUTINE_G(map), scoroutine->coroutine.id, &ztmp);
        GC_ADDREF(&scoroutine->std);
    } while (0);

    /* prepare function call info */
    fci.size = sizeof(fci);
    ZVAL_UNDEF(&fci.function_name);
    fci.object = NULL;
    /* params will be copied by zend_call_function */
    if (likely(zdata == NULL)) {
        fci.param_count = 0;
    } else if (Z_TYPE_P(zdata) != IS_PTR) {
        Z_TRY_DELREF_P(zdata);
        fci.param_count = 1;
        fci.params = zdata;
    } else {
        zend_fcall_info *fci_ptr = (zend_fcall_info *) Z_PTR_P(zdata);
        fci.param_count = fci_ptr->param_count;
        fci.params = fci_ptr->params;
    }
#if PHP_VERSION_ID >= 80000
    fci.named_params = NULL;
#else
    fci.no_separation = 0;
#endif
    fci.retval = &retval;

    /* call function */
    EG(current_execute_data) = (zend_execute_data *) &dummy_execute_data;
    (void) zend_call_function(&fci, &executor->fcc);
    EG(current_execute_data) = NULL;
    if (UNEXPECTED(EG(exception) != NULL)) {
        swow_coroutine_function_handle_exception();
    }

    /* call __destruct() first here (prevent destructing in scheduler) */
    if (scoroutine->std.ce->destructor != NULL) {
        EG(current_execute_data) = (zend_execute_data *) &dummy_execute_data;
        zend_objects_destroy_object(&scoroutine->std);
        EG(current_execute_data) = NULL;
        if (UNEXPECTED(EG(exception) != NULL)) {
            swow_coroutine_function_handle_exception();
        }
    }
    /* do not call __destruct() anymore  */
    GC_ADD_FLAGS(&scoroutine->std, IS_OBJ_DESTRUCTOR_CALLED);

    /* discard all possible resources (varibles by "use" in zend_closure) */
    EG(current_execute_data) = (zend_execute_data *) &dummy_execute_data;
    ZVAL_NULL(&executor->zcallable);
    zval_ptr_dtor(&zcallable);
    EG(current_execute_data) = NULL;
    if (UNEXPECTED(EG(exception) != NULL)) {
        swow_coroutine_function_handle_exception();
    }

    /* ob end clean */
#ifdef SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS
    swow_output_globals_fast_end();
#endif

    /* solve retval */
    if (UNEXPECTED(Z_TYPE_P(fci.retval) != IS_UNDEF && Z_TYPE_P(fci.retval) != IS_NULL)) {
        scoroutine->coroutine.opcodes |= SWOW_COROUTINE_OPCODE_ACCEPT_ZDATA;
        /* make sure the memory space of zdata is safe */
        zval *ztransfer_data = &SWOW_COROUTINE_G(ztransfer_data);
        ZVAL_COPY_VALUE(ztransfer_data, fci.retval);
        return ztransfer_data;
    }

    return NULL;
}

#ifdef SWOW_COROUTINE_ENABLE_CUSTOM_ENTRY
static swow_coroutine_t *swow_coroutine_create_custom_object(zval *zcallable)
{
    zend_class_entry *custom_entry = SWOW_COROUTINE_G(custom_entry);
    swow_coroutine_t *scoroutine;
    zval zscoroutine;

    scoroutine = swow_coroutine_get_from_object(swow_object_create(custom_entry));
    ZVAL_OBJ(&zscoroutine, &scoroutine->std);
    swow_call_method_with_1_params(
        &zscoroutine,
        custom_entry,
        &custom_entry->constructor,
        "__construct",
        NULL,
        zcallable
    );
    if (UNEXPECTED(EG(exception))) {
        cat_update_last_error_ez("Exception occurred during construction");
        swow_coroutine_close(scoroutine);
        return NULL;
    }

    return scoroutine;
}
#endif

static cat_bool_t swow_coroutine_construct(swow_coroutine_t *scoroutine, zval *zcallable, size_t stack_page_size, size_t c_stack_size)
{
    swow_coroutine_executor_t *executor;
    zend_fcall_info_cache fcc;
    cat_coroutine_t *coroutine;

    /* check arguments */
    do {
        char *error;
        if (!zend_is_callable_ex(zcallable, NULL, 0, NULL, &fcc, &error)) {
            cat_update_last_error(CAT_EMISUSE, "Coroutine function must be callable, %s", error);
            efree(error);
            return cat_false;
        }
        efree(error);
    } while (0);

    /* create C coroutine only if function is not NULL
     * (e.g. main coroutine is running so we do not need to re-create it,
     * or we want to create/run it by ourself later) */
    coroutine = cat_coroutine_create_ex(
        &scoroutine->coroutine,
        (cat_coroutine_function_t) swow_coroutine_function,
        c_stack_size
    );
    if (UNEXPECTED(coroutine == NULL)) {
        return cat_false;
    }

    /* init executor */
    do {
        zend_vm_stack vm_stack;
        coroutine->opcodes |= SWOW_COROUTINE_OPCODE_ACCEPT_ZDATA;
        /* align stack page size */
        stack_page_size = swow_coroutine_align_stack_page_size(stack_page_size);
        /* alloc vm stack memory */
        vm_stack = (zend_vm_stack) emalloc(stack_page_size);
        /* assign the end to executor */
        executor = (swow_coroutine_executor_t *) ZEND_VM_STACK_ELEMENTS(vm_stack);
        /* init executor */
        executor->bailout = NULL;
        executor->vm_stack = vm_stack;
        executor->vm_stack->top = (zval *) (((char *) executor) + CAT_MEMORY_ALIGNED_SIZE_EX(sizeof(*executor), sizeof(zval)));
        executor->vm_stack->end = (zval *) (((char *) vm_stack) + stack_page_size);
        executor->vm_stack->prev = NULL;
        executor->vm_stack_top = executor->vm_stack->top;
        executor->vm_stack_end = executor->vm_stack->end;
#if PHP_VERSION_ID >= 70300
        executor->vm_stack_page_size = stack_page_size;
#endif
        executor->current_execute_data = NULL;
        executor->exception = NULL;
#ifdef SWOW_COROUTINE_SWAP_ERROR_HANDING
        executor->error_handling = EH_NORMAL;
        executor->exception_class = NULL;
#endif
#ifdef SWOW_COROUTINE_SWAP_JIT_GLOBALS
        executor->jit_trace_num = 0;
#endif
#ifdef SWOW_COROUTINE_SWAP_BASIC_GLOBALS
        executor->array_walk_context = NULL;
#endif
#ifdef SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS
        executor->output_globals = NULL;
#endif
#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
        executor->error_reporting = 0;
#endif
        /* save function cache */
        ZVAL_COPY(&executor->zcallable, zcallable);
        executor->fcc = fcc;
        /* it's unnecessary to init the zdata */
        /* ZVAL_UNDEF(&executor->zdata); */
        executor->cross_exception = NULL;
    } while (0);

    scoroutine->executor = executor;
    scoroutine->exit_status = 0;

    return cat_true;
}

static void swow_coroutine_shutdown(swow_coroutine_t *scoroutine)
{
    swow_coroutine_executor_t *executor = scoroutine->executor;

    if (executor == NULL) {
        return;
    }

    /* we release the context resources here which are created during the runtime  */
#ifdef SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS
    if (UNEXPECTED(executor->output_globals != NULL)) {
        efree(executor->output_globals);
    }
#endif
#ifdef SWOW_COROUTINE_SWAP_BASIC_GLOBALS
    if (UNEXPECTED(executor->array_walk_context != NULL)) {
        efree(executor->array_walk_context);
    }
#endif

    /* discard function (usually cleaned up before the coroutine finished, unless the coroutine never run) */
    if (UNEXPECTED(!ZVAL_IS_NULL(&executor->zcallable))) {
        zval_ptr_dtor(&executor->zcallable);
    }

    /* free zend vm stack */
    if (EXPECTED(executor->vm_stack != NULL)) {
        zend_vm_stack stack = executor->vm_stack;
        do {
            zend_vm_stack prev = stack->prev;
            efree(stack);
            stack = prev;
        } while (stack);
    } else {
        efree(executor);
    }

    scoroutine->executor = NULL;
}

static void swow_coroutine_main_create(void)
{
    swow_coroutine_t *scoroutine;

    scoroutine = swow_coroutine_get_from_object(
        swow_object_create(swow_coroutine_ce)
    );

    /* register first (sync coroutine info) */
    SWOW_COROUTINE_G(original_main) = cat_coroutine_register_main(&scoroutine->coroutine);

    scoroutine->executor = ecalloc(1, sizeof(*scoroutine->executor));
    ZVAL_NULL(&scoroutine->executor->zcallable);
    scoroutine->exit_status = 0;

    /* add main scoroutine to the map */
    do {
        zval zscoroutine;
        ZVAL_OBJ(&zscoroutine, &scoroutine->std);
        zend_hash_index_update(SWOW_COROUTINE_G(map), scoroutine->coroutine.id, &zscoroutine);
        /* Notice: we have 1 ref by create */
        GC_ADDREF(&scoroutine->std);
    } while (0);

    /* do not call __destruct() on main scoroutine */
    GC_ADD_FLAGS(&scoroutine->std, IS_OBJ_DESTRUCTOR_CALLED);
}

static void swow_coroutine_main_close(void)
{
    swow_coroutine_t *scoroutine;

    scoroutine = swow_coroutine_get_main();

    /* revert globals main */
    cat_coroutine_register_main(SWOW_COROUTINE_G(original_main));

    /* hack way to close the main */
    scoroutine->coroutine.state = CAT_COROUTINE_STATE_READY;
    scoroutine->executor->vm_stack = NULL;

    /* release main scoroutine */
    zend_object_release(&scoroutine->std);
}

SWOW_API swow_coroutine_t *swow_coroutine_create(zval *zcallable)
{
    return swow_coroutine_create_ex(zcallable, 0, 0);
}

SWOW_API swow_coroutine_t *swow_coroutine_create_ex(zval *zcallable, size_t stack_page_size, size_t c_stack_size)
{
    swow_coroutine_t *scoroutine;

    scoroutine = swow_coroutine_get_from_object(
        swow_object_create(swow_coroutine_ce)
    );

    if (UNEXPECTED(!swow_coroutine_construct(scoroutine, zcallable, stack_page_size, c_stack_size))) {
        swow_coroutine_close(scoroutine);
        return NULL;
    }

    return scoroutine;
}

SWOW_API void swow_coroutine_close(swow_coroutine_t *scoroutine)
{
    zend_object_release(&scoroutine->std);
}

SWOW_API void swow_coroutine_executor_switch(swow_coroutine_executor_t *current_executor, swow_coroutine_executor_t *target_executor)
{
    // TODO: it's not optimal
    if (current_executor != NULL) {
        swow_coroutine_executor_save(current_executor);
    }
    if (target_executor != NULL) {
        swow_coroutine_executor_recover(target_executor);
    } else {
        EG(current_execute_data) = NULL; /* make the log stack trace empty */
    }
}

SWOW_API void swow_coroutine_executor_save(swow_coroutine_executor_t *executor)
{
    zend_executor_globals *eg = SWOW_GLOBALS_FAST_PTR(executor_globals);
    executor->bailout = eg->bailout;
    executor->vm_stack_top = eg->vm_stack_top;
    executor->vm_stack_end = eg->vm_stack_end;
    executor->vm_stack = eg->vm_stack;
#if PHP_VERSION_ID >= 70300
    executor->vm_stack_page_size = eg->vm_stack_page_size;
#endif
    executor->current_execute_data = eg->current_execute_data;
    executor->exception = eg->exception;
#ifdef SWOW_COROUTINE_SWAP_ERROR_HANDING
    executor->error_handling = eg->error_handling;
    executor->exception_class = eg->exception_class;
#endif
#ifdef SWOW_COROUTINE_SWAP_JIT_GLOBALS
    executor->jit_trace_num = eg->jit_trace_num;
#endif
#ifdef SWOW_COROUTINE_SWAP_BASIC_GLOBALS
    do {
        swow_fcall_t *fcall = (swow_fcall_t *) &BG(array_walk_fci);
        if (UNEXPECTED(fcall->info.size != 0)) {
            if (UNEXPECTED(executor->array_walk_context == NULL)) {
                executor->array_walk_context = (swow_fcall_t *) emalloc(sizeof(*fcall));
            }
            memcpy(executor->array_walk_context, fcall, sizeof(*fcall));
            memset(fcall, 0, sizeof(*fcall));
        }
    } while (0);
#endif
#ifdef SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS
    do {
        zend_output_globals *og = SWOW_GLOBALS_PTR(output_globals);
        if (UNEXPECTED(og->handlers.elements != NULL)) {
            if (UNEXPECTED(executor->output_globals == NULL)) {
                executor->output_globals = (zend_output_globals *) emalloc(sizeof(zend_output_globals));
            }
            memcpy(executor->output_globals, og, sizeof(zend_output_globals));
            php_output_activate();
        }
    } while (0);
#endif
#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
    do {
        if (UNEXPECTED(executor->error_reporting != 0)) {
            CAT_ASSERT(executor->error_reporting & E_MAGIC);
            int error_reporting = executor->error_reporting;
            executor->error_reporting = eg->error_reporting | E_MAGIC;
            eg->error_reporting = error_reporting ^ E_MAGIC;
        }
    } while (0);
#endif
}

SWOW_API void swow_coroutine_executor_recover(swow_coroutine_executor_t *executor)
{
    zend_executor_globals *eg = SWOW_GLOBALS_FAST_PTR(executor_globals);
    eg->bailout = executor->bailout;
    eg->vm_stack_top = executor->vm_stack_top;
    eg->vm_stack_end = executor->vm_stack_end;
    eg->vm_stack = executor->vm_stack;
#if PHP_VERSION_ID >= 70300
    eg->vm_stack_page_size = executor->vm_stack_page_size;
#endif
    eg->current_execute_data = executor->current_execute_data;
    eg->exception = executor->exception;
#ifdef SWOW_COROUTINE_SWAP_ERROR_HANDING
    eg->error_handling = executor->error_handling;
    eg->exception_class = executor->exception_class;
#endif
#ifdef SWOW_COROUTINE_SWAP_JIT_GLOBALS
    eg->jit_trace_num = executor->jit_trace_num;
#endif
#ifdef SWOW_COROUTINE_SWAP_BASIC_GLOBALS
    do {
        swow_fcall_t *fcall = executor->array_walk_context;
        if (UNEXPECTED(fcall != NULL && fcall->info.size != 0)) {
            memcpy(&BG(array_walk_fci), fcall, sizeof(*fcall));
            fcall->info.size = 0;
        }
    } while (0);
#endif
#ifdef SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS
    do {
        zend_output_globals *og = executor->output_globals;
        if (UNEXPECTED(og != NULL)) {
            if (UNEXPECTED(og->handlers.elements != NULL)) {
                memcpy(SWOW_GLOBALS_PTR(output_globals), og, sizeof(zend_output_globals));
                og->handlers.elements = NULL;
            }
        }
    } while (0);
#endif
#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
    do {
        if (UNEXPECTED(executor->error_reporting != 0)) {
            CAT_ASSERT(executor->error_reporting & E_MAGIC);
            int error_reporting = eg->error_reporting;
            eg->error_reporting = executor->error_reporting ^ E_MAGIC;
            executor->error_reporting = error_reporting | E_MAGIC;
        }
    } while (0);
#endif
}

static void swow_coroutine_handle_not_null_zdata(swow_coroutine_t *scoroutine, swow_coroutine_t *current_scoroutine, zval **zdata_ptr)
{
    if (!(current_scoroutine->coroutine.opcodes & SWOW_COROUTINE_OPCODE_ACCEPT_ZDATA)) {
        if (UNEXPECTED(scoroutine->coroutine.opcodes & SWOW_COROUTINE_OPCODE_ACCEPT_ZDATA)) {
            cat_core_error(COROUTINE, "Internal logic error: sent unrecognized data to PHP layer");
        } else {
            /* internal raw data to internal operation coroutine */
        }
    } else {
        zval *zdata = *zdata_ptr;
        zend_bool handle_ref = current_scoroutine->coroutine.state == CAT_COROUTINE_STATE_FINISHED;
        if (!(scoroutine->coroutine.opcodes & SWOW_COROUTINE_OPCODE_ACCEPT_ZDATA)) {
            CAT_ASSERT(Z_TYPE_P(zdata) != IS_PTR);
            /* the PHP layer can not send data to the internal-controlled coroutine */
            if (handle_ref) {
                zval_ptr_dtor(zdata);
            }
            *zdata_ptr = NULL;
        } else {
            if (UNEXPECTED(Z_TYPE_P(zdata) == IS_PTR)) {
                /* params will be copied by zend_call_function */
                return;
            }
            if (!handle_ref) {
                /* send without copy value */
                Z_TRY_ADDREF_P(zdata);
            }
        }
    }
}

SWOW_API zval *swow_coroutine_jump(swow_coroutine_t *scoroutine, zval *zdata)
{
    swow_coroutine_t *current_scoroutine = swow_coroutine_get_current();

    /* solve origin's refcount */
    do {
        swow_coroutine_t *current_previous_scoroutine = swow_coroutine_get_previous(current_scoroutine);
        if (current_previous_scoroutine == scoroutine) {
            /* if it is yield, break the origin */
            GC_DELREF(&current_previous_scoroutine->std);
        } else {
            /* it is not yield */
            CAT_ASSERT(swow_coroutine_get_previous(scoroutine) == NULL);
            /* current becomes target's origin */
            GC_ADDREF(&current_scoroutine->std);
        }
    } while (0);

    /* switch executor */
    swow_coroutine_executor_switch(current_scoroutine->executor, scoroutine->executor);

    if (UNEXPECTED(zdata != NULL)) {
        swow_coroutine_handle_not_null_zdata(scoroutine, swow_coroutine_get_current(), &zdata);
    }

    /* resume C coroutine */
    zdata = (zval *) cat_coroutine_jump(&scoroutine->coroutine, zdata);

    /* get from scoroutine */
    scoroutine = swow_coroutine_get_from(current_scoroutine);

    if (UNEXPECTED(scoroutine->coroutine.state == CAT_COROUTINE_STATE_DEAD)) {
        swow_coroutine_shutdown(scoroutine);
        /* delete it from global map
         * (we can not delete it in coroutine_function, object maybe released during deletion) */
        zend_hash_index_del(SWOW_COROUTINE_G(map), scoroutine->coroutine.id);
    } else {
        swow_coroutine_executor_t *executor = current_scoroutine->executor;
        if (executor != NULL) {
            /* handle cross exception */
            if (UNEXPECTED(executor->cross_exception != NULL)) {
                swow_coroutine_handle_cross_exception(executor->cross_exception);
                executor->cross_exception = NULL;
            }
        }
    }

    return zdata;
}

SWOW_API cat_bool_t swow_coroutine_is_resumable(const swow_coroutine_t *scoroutine)
{
    return cat_coroutine_is_resumable(&scoroutine->coroutine);
}

SWOW_API cat_bool_t swow_coroutine_resume_standard(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_handle(coroutine);

    if (EXPECTED(!(scoroutine->coroutine.opcodes & CAT_COROUTINE_OPCODE_CHECKED))) {
        if (UNEXPECTED(!swow_coroutine_is_resumable(scoroutine))) {
            return cat_false;
        }
    }

    data = (cat_data_t *) swow_coroutine_jump(scoroutine, (zval *) data);

    if (retval != NULL) {
        *retval = data;
    } else {
        CAT_ASSERT(data == NULL && "Unexpected non-empty data, resource may leak");
    }

    return cat_true;
}

#define SWOW_COROUTINE_JUMP_INIT(retval) \
    cat_coroutine_t *current_coroutine = CAT_COROUTINE_G(current); do { \
    \
    if (retval != NULL) { \
        ZVAL_NULL(retval); \
    } \
    \
    current_coroutine->opcodes |= SWOW_COROUTINE_OPCODE_ACCEPT_ZDATA; \
} while (0)

#define SWOW_COROUTINE_JUMP_HANDLE_FAILURE() do { \
    current_coroutine->opcodes ^= SWOW_COROUTINE_OPCODE_ACCEPT_ZDATA; \
} while (0);

#define SWOW_COROUTINE_JUMP_HANDLE_RETVAL(zdata, retval) do { \
    if (UNEXPECTED(zdata != NULL)) { \
        if (retval == NULL) { \
            zval_ptr_dtor(zdata); \
        } else { \
            ZVAL_COPY_VALUE(retval, zdata); \
        } \
    } \
} while (0)

SWOW_API cat_bool_t swow_coroutine_resume(swow_coroutine_t *scoroutine, zval *zdata, zval *retval)
{
    SWOW_COROUTINE_JUMP_INIT(retval);
    cat_bool_t ret;

    ret = cat_coroutine_resume(&scoroutine->coroutine, zdata, (cat_data_t **) &zdata);

    if (UNEXPECTED(!ret)) {
        SWOW_COROUTINE_JUMP_HANDLE_FAILURE();
        return cat_false;
    }

    SWOW_COROUTINE_JUMP_HANDLE_RETVAL(zdata, retval);

    return cat_true;
}

SWOW_API cat_bool_t swow_coroutine_yield(zval *zdata, zval *retval)
{
    SWOW_COROUTINE_JUMP_INIT(retval);
    cat_bool_t ret;

    ret = cat_coroutine_yield(zdata, (cat_data_t **) &zdata);

    if (UNEXPECTED(!ret)) {
        SWOW_COROUTINE_JUMP_HANDLE_FAILURE();
        return cat_false;
    }

    SWOW_COROUTINE_JUMP_HANDLE_RETVAL(zdata, retval);

    return cat_true;
}

#undef SWOW_COROUTINE_JUMP_HANDLE_FAILURE
#undef SWOW_COROUTINE_JUMP_HANDLE_RETVAL
#undef SWOW_COROUTINE_JUMP_INIT

/* basic info */

SWOW_API cat_bool_t swow_coroutine_is_available(const swow_coroutine_t *scoroutine)
{
    return cat_coroutine_is_available(&scoroutine->coroutine);
}

SWOW_API cat_bool_t swow_coroutine_is_alive(const swow_coroutine_t *scoroutine)
{
     return cat_coroutine_is_alive(&scoroutine->coroutine);
}

SWOW_API cat_bool_t swow_coroutine_is_executing(const swow_coroutine_t *scoroutine)
{
    if (scoroutine == swow_coroutine_get_current()) {
        return EG(current_execute_data) != NULL;
    }

    return swow_coroutine_is_alive(scoroutine) &&
            scoroutine->executor != NULL &&
            scoroutine->executor->current_execute_data != NULL;
}

SWOW_API swow_coroutine_t *swow_coroutine_get_from(const swow_coroutine_t *scoroutine)
{
    return swow_coroutine_get_from_handle(scoroutine->coroutine.from);
}

SWOW_API swow_coroutine_t *swow_coroutine_get_previous(const swow_coroutine_t *scoroutine)
{
    return swow_coroutine_get_from_handle(scoroutine->coroutine.previous);
}

/* globals (options) */

SWOW_API size_t swow_coroutine_set_default_stack_page_size(size_t size)
{
    size_t original_size = SWOW_COROUTINE_G(default_stack_page_size);
    SWOW_COROUTINE_G(default_stack_page_size) = swow_coroutine_align_stack_page_size(size);
    return original_size;
}

SWOW_API size_t swow_coroutine_set_default_c_stack_size(size_t size)
{
    return cat_coroutine_set_default_stack_size(size);
}

static cat_bool_t swow_coroutine_resume_deny(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval)
{
    cat_update_last_error(CAT_EMISUSE, "Unexpected coroutine switching");

    return cat_false;
}

SWOW_API void swow_coroutine_set_readonly(cat_bool_t enable)
{
    swow_coroutine_readonly_t *readonly = &SWOW_COROUTINE_G(readonly);
    if (enable) {
        readonly->original_create_object = swow_coroutine_ce->create_object;
        readonly->original_resume = cat_coroutine_register_resume(swow_coroutine_resume_deny);
        swow_coroutine_ce->create_object = swow_create_object_deny;
    } else {
        if (
            swow_coroutine_ce->create_object == swow_create_object_deny &&
            readonly->original_create_object != NULL
        ) {
            swow_coroutine_ce->create_object = readonly->original_create_object;
        }
        if (
            cat_coroutine_resume == swow_coroutine_resume_deny &&
            readonly->original_resume != NULL
        ) {
            cat_coroutine_register_resume(readonly->original_resume);
        }
    }
}

/* globals (getter) */

SWOW_API swow_coroutine_t *swow_coroutine_get_current(void)
{
    return swow_coroutine_get_from_handle(CAT_COROUTINE_G(current));
}

SWOW_API swow_coroutine_t *swow_coroutine_get_main(void)
{
    return swow_coroutine_get_from_handle(CAT_COROUTINE_G(main));
}

SWOW_API swow_coroutine_t *swow_coroutine_get_scheduler(void)
{
    return swow_coroutine_get_from_handle(CAT_COROUTINE_G(scheduler));
}

/* debug */

SWOW_API zend_string *swow_coroutine_get_executed_filename(const swow_coroutine_t *scoroutine, zend_long level)
{
    zend_string *filename;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_false, return zend_empty_string);

    SWOW_COROUTINE_EXECUTE_START(scoroutine, level) {
        filename = zend_get_executed_filename_ex();
    } SWOW_COROUTINE_EXECUTE_END();

    if (UNEXPECTED(filename == NULL)) {
        return zend_empty_string;
    }

    return zend_string_copy(filename);
}

SWOW_API uint32_t swow_coroutine_get_executed_lineno(const swow_coroutine_t *scoroutine, zend_long level)
{
    uint32_t lineno;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_false, return 0);

    SWOW_COROUTINE_EXECUTE_START(scoroutine, level) {
        lineno = zend_get_executed_lineno();
    } SWOW_COROUTINE_EXECUTE_END();

    return lineno;
}

SWOW_API zend_string *swow_coroutine_get_executed_function_name(const swow_coroutine_t *scoroutine, zend_long level)
{
    zend_string *function_name;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_false, return zend_empty_string);

    SWOW_COROUTINE_EXECUTE_START(scoroutine, level) {
        if (EXPECTED(zend_is_executing())) {
            function_name = get_active_function_or_method_name();
        } else {
            function_name = zend_empty_string;
        }
    } SWOW_COROUTINE_EXECUTE_END();

    return function_name;
}

SWOW_API HashTable *swow_coroutine_get_trace(const swow_coroutine_t *scoroutine, zend_long level, zend_long limit, zend_long options)
{
    HashTable *trace;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_false, return NULL);

    SWOW_COROUTINE_EXECUTE_START(scoroutine, level) {
        trace = swow_debug_get_trace(options, limit);
    } SWOW_COROUTINE_EXECUTE_END();

    return trace;
}

SWOW_API smart_str *swow_coroutine_get_trace_as_smart_str(swow_coroutine_t *scoroutine, smart_str *str, zend_long level, zend_long limit, zend_long options)
{
    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_false, return NULL);

    SWOW_COROUTINE_EXECUTE_START(scoroutine, level) {
        str = swow_debug_get_trace_as_smart_str(str, options, limit);
    } SWOW_COROUTINE_EXECUTE_END();

    return str;
}

SWOW_API zend_string *swow_coroutine_get_trace_as_string(const swow_coroutine_t *scoroutine, zend_long level, zend_long limit, zend_long options)
{
    zend_string *trace;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_false, return NULL);

    SWOW_COROUTINE_EXECUTE_START(scoroutine, level) {
        trace = swow_debug_get_trace_as_string(options, limit);
    } SWOW_COROUTINE_EXECUTE_END();

    return trace;
}

SWOW_API HashTable *swow_coroutine_get_trace_as_list(const swow_coroutine_t *scoroutine, zend_long level, zend_long limit, zend_long options)
{
    HashTable *trace;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_false, return NULL);

    SWOW_COROUTINE_EXECUTE_START(scoroutine, level) {
        trace = swow_debug_get_trace_as_list(options, limit);
    } SWOW_COROUTINE_EXECUTE_END();

    return trace;
}

#define SWOW_COROUTINE_CHECK_CALL_INFO(failure) do { \
    if (UNEXPECTED(ZEND_CALL_INFO(EG(current_execute_data)) & ZEND_CALL_DYNAMIC)) { \
        cat_update_last_error(CAT_EPERM, "Coroutine executor is dynamic"); \
        failure; \
    } \
} while (0)

SWOW_API HashTable *swow_coroutine_get_defined_vars(swow_coroutine_t *scoroutine, zend_ulong level)
{
    HashTable *symbol_table;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_true, return NULL);

    SWOW_COROUTINE_USER_EXECUTE_START(scoroutine, level) {
        SWOW_COROUTINE_CHECK_CALL_INFO(goto _error);

        symbol_table = zend_rebuild_symbol_table();

        if (EXPECTED(symbol_table != NULL)) {
            symbol_table = zend_array_dup(symbol_table);
        }

        if (0) {
            _error:
            symbol_table = NULL;
        }
    } SWOW_COROUTINE_USER_EXECUTE_END();

    return symbol_table;
}

SWOW_API cat_bool_t swow_coroutine_set_local_var(swow_coroutine_t *scoroutine, zend_string *name, zval *value, zend_long level, zend_bool force)
{
    cat_bool_t ret;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_true, return cat_false);

    SWOW_COROUTINE_USER_EXECUTE_START(scoroutine, level) {
        int error;

        SWOW_COROUTINE_CHECK_CALL_INFO(goto _error);

        error = zend_set_local_var(name, value, force);

        if (UNEXPECTED(error != SUCCESS)) {
            cat_update_last_error(CAT_EINVAL, "Set var '%.*s' failed by unknown reason", (int) ZSTR_LEN(name), ZSTR_VAL(name));
            _error:
            ret = cat_false;
        } else {
            ret = cat_true;
        }
    } SWOW_COROUTINE_USER_EXECUTE_END();

    return ret;
}

#undef SWOW_COROUTINE_CHECK_CALL_INFO

SWOW_API cat_bool_t swow_coroutine_eval(swow_coroutine_t *scoroutine, zend_string *string, zend_long level, zval *return_value)
{
    int error;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_true, return cat_false);

    if (scoroutine != swow_coroutine_get_current() || level != 0) {
        swow_coroutine_set_readonly(cat_true);
    }

    SWOW_COROUTINE_USER_EXECUTE_START(scoroutine, level) {
        error = zend_eval_stringl(ZSTR_VAL(string), ZSTR_LEN(string), return_value, (char *) "Coroutine::eval()");
    } SWOW_COROUTINE_USER_EXECUTE_END();

    swow_coroutine_set_readonly(cat_false);

    if (UNEXPECTED(error != SUCCESS)) {
        cat_update_last_error(CAT_UNKNOWN, "Eval failed by unknown reason");
        return cat_false;
    }

    return cat_true;
}

SWOW_API cat_bool_t swow_coroutine_call(swow_coroutine_t *scoroutine, zval *zcallable, zval *return_value)
{
    zend_fcall_info fci;
    zend_fcall_info_cache fcc;
    int error;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_true, return cat_false);

    do {
        char *error;
        if (!zend_is_callable_ex(zcallable, NULL, 0, NULL, &fcc, &error)) {
            cat_update_last_error(CAT_EMISUSE, "Coroutine::call() only accept callable parameter, %s", error);
            efree(error);
            return cat_false;
        }
        efree(error);
    } while (0);

    fci.size = sizeof(fci);
    ZVAL_UNDEF(&fci.function_name);
    fci.object = NULL;
    fci.param_count = 0;
#if PHP_VERSION_ID >= 80000
    fci.named_params = NULL;
#else
    fci.no_separation = 0;
#endif
    fci.retval = return_value;

    if (scoroutine != swow_coroutine_get_current()) {
        swow_coroutine_set_readonly(cat_true);
    }

    SWOW_COROUTINE_EXECUTE_START(scoroutine, 0) {
        error = zend_call_function(&fci, &fcc);
    } SWOW_COROUTINE_EXECUTE_END();

    swow_coroutine_set_readonly(cat_false);

    if (UNEXPECTED(error != SUCCESS)) {
        cat_update_last_error(CAT_UNKNOWN, "Call function failed by unknown reason");
        return cat_false;
    }

    return cat_true;
}

SWOW_API swow_coroutine_t *swow_coroutine_get_by_id(cat_coroutine_id_t id)
{
    zval *zscoroutine = zend_hash_index_find(SWOW_COROUTINE_G(map), id);
    swow_coroutine_t *scoroutine;

    if (zscoroutine != NULL) {
        scoroutine = swow_coroutine_get_from_object(Z_OBJ_P(zscoroutine));
    } else {
        scoroutine = NULL;
    }

    return scoroutine;
}

SWOW_API zval *swow_coroutine_get_zval_by_id(cat_coroutine_id_t id)
{
    zval *zscoroutine = zend_hash_index_find(SWOW_COROUTINE_G(map), id);
    static zval ztmp;

    if (zscoroutine == NULL) {
        ZVAL_NULL(&ztmp);
        zscoroutine = &ztmp;
    }

    return zscoroutine;
}

SWOW_API void swow_coroutine_dump(swow_coroutine_t *scoroutine)
{
    zval zscoroutine;

    ZVAL_OBJ(&zscoroutine, &scoroutine->std);
    php_var_dump(&zscoroutine, 0);
}

SWOW_API void swow_coroutine_dump_by_id(cat_coroutine_id_t id)
{
    php_var_dump(swow_coroutine_get_zval_by_id(id), 0);
}

SWOW_API void swow_coroutine_dump_all(void)
{
    zval zmap;

    ZVAL_ARR(&zmap, SWOW_COROUTINE_G(map));
    php_var_dump(&zmap, 0);
}

/* exception */

static void swow_coroutine_handle_cross_exception(zend_object *cross_exception)
{
    zend_object *exception;
    zend_class_entry *ce = cross_exception->ce;
    zval ztmp;

    /* for throw method success */
    GC_ADDREF(cross_exception);

    if (UNEXPECTED(EG(exception) != NULL)) {
        // FIXME: why?
        if (instanceof_function(ce, swow_coroutine_kill_exception_ce)) {
            OBJ_RELEASE(EG(exception));
            EG(exception) = NULL;
        }
    }
    exception = swow_object_create(ce);
    ZVAL7_ALLOC_OBJECT(exception);
    ZVAL7_ALLOC_OBJECT(cross_exception);
    zend_update_property_ex(
        ce, ZVAL7_OBJECT(exception), ZSTR_KNOWN(ZEND_STR_MESSAGE),
        zend_read_property_ex(ce, ZVAL7_OBJECT(cross_exception), ZSTR_KNOWN(ZEND_STR_MESSAGE), 1, &ztmp)
    );
    zend_update_property_ex(
        ce, ZVAL7_OBJECT(exception), ZSTR_KNOWN(ZEND_STR_CODE),
        zend_read_property_ex(ce, ZVAL7_OBJECT(cross_exception), ZSTR_KNOWN(ZEND_STR_CODE), 1, &ztmp)
    );
    if (UNEXPECTED(EG(exception) != NULL)) {
        zend_throw_exception_internal(ZVAL7_OBJECT(cross_exception));
        zend_throw_exception_internal(ZVAL7_OBJECT(exception));
    } else {
        zend_exception_set_previous(exception, cross_exception);
        if (EG(exception) == NULL) {
            zend_throw_exception_internal(ZVAL7_OBJECT(exception));
        }
    }
}

SWOW_API cat_bool_t swow_coroutine_throw(swow_coroutine_t *scoroutine, zend_object *exception, zval *retval)
{
    if (UNEXPECTED(!instanceof_function(exception->ce, zend_ce_throwable))) {
        cat_update_last_error(CAT_EMISUSE, "Instance of %s is not throwable", ZSTR_VAL(exception->ce->name));
        return cat_false;
    }

    if (UNEXPECTED(scoroutine == swow_coroutine_get_current())) {
        ZVAL7_ALLOC_OBJECT(exception);
        GC_ADDREF(exception);
        zend_throw_exception_internal(ZVAL7_OBJECT(exception));
        ZVAL_NULL(retval);
    } else {
        SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_true, return cat_false);
        // TODO: split check & resume
        scoroutine->executor->cross_exception = exception;
        if (UNEXPECTED(!swow_coroutine_resume(scoroutine, NULL, retval))) {
            scoroutine->executor->cross_exception = NULL;
            return cat_false;
        }
    }

    return cat_true;
}

SWOW_API cat_bool_t swow_coroutine_term(swow_coroutine_t *scoroutine, const char *message, zend_long code, zval *retval)
{
    zend_object *exception;
    cat_bool_t success;

    exception = swow_object_create(swow_coroutine_term_exception_ce);
    swow_exception_set_properties(exception, message, code);
    success = swow_coroutine_throw(scoroutine, exception, retval);
    OBJ_RELEASE(exception);

    return success;
}

#ifdef SWOW_COROUTINE_USE_RATED
static cat_data_t swow_coroutine_resume_rated(cat_coroutine_t *coroutine, cat_data_t data)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_handle(coroutine);
    swow_coroutine_t *current_scoroutine = swow_coroutine_get_current();
    swow_coroutine_rated_t *rated = &SWOW_COROUTINE_G(rated);

    /* target + current */
    if (UNEXPECTED((
        scoroutine != rated->dead &&
        scoroutine != rated->killer
    ) || (
        current_scoroutine != rated->dead &&
        current_scoroutine != rated->killer
    ))) {
        return swow_coroutine_resume_deny(coroutine, data);
    }

    return swow_coroutine_resume_standard(coroutine, data);
}
#endif

SWOW_API cat_bool_t swow_coroutine_kill(swow_coroutine_t *scoroutine, const char *message, zend_long code)
{
    zend_object *exception;
    cat_bool_t success;
    zval retval;

    exception = swow_object_create(swow_coroutine_kill_exception_ce);
    swow_exception_set_properties(exception, message, code);
#ifndef SWOW_COROUTINE_USE_RATED
    success = swow_coroutine_throw(scoroutine, exception, &retval);
    OBJ_RELEASE(exception);
    if (!success) {
        return cat_false;
    }
    zval_ptr_dtor(&retval);

    return cat_true;
#else
    do {
        swow_coroutine_rated_t *rated = &SWOW_COROUTINE_G(rated);
        /* prevent coroutines from escaping */
        cat_coroutine_resume_t original_resume = cat_coroutine_register_resume(swow_coroutine_resume_rated);
        rated->killer = swow_coroutine_get_current();
        rated->dead = scoroutine;
        success = swow_coroutine_throw(scoroutine, exception, &retval);
        CAT_ASSERT(!SWOW_COROUTINE_G(kill_main));
        /* revert */
        cat_coroutine_register_resume(original_resume);
    } while (0);
    OBJ_RELEASE(exception);
    if (UNEXPECTED(!success)) {
        return cat_false;
    }
    if (UNEXPECTED(swow_coroutine_is_running(scoroutine))) {
        cat_core_error(COROUTINE, "Kill coroutine failed by unknown reason");
    }
    if (UNEXPECTED(!ZVAL_IS_NULL(&retval))) {
        cat_core_error(COROUTINE, "Unexpected return value");
    }

    return cat_true;
#endif
}

#define getThisCoroutine() (swow_coroutine_get_from_object(Z_OBJ_P(ZEND_THIS)))

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Coroutine___construct, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_CALLABLE_INFO(0, callable, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, stackPageSize, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, stackSize, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, __construct)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();
    zval *zcallable;
    zend_long stack_page_size = 0;
    zend_long c_stack_size = 0;

    if (UNEXPECTED(scoroutine->coroutine.state != CAT_COROUTINE_STATE_INIT)) {
        zend_throw_error(NULL, "%s can only construct once", ZEND_THIS_NAME);
        RETURN_THROWS();
    }

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_ZVAL(zcallable)
        Z_PARAM_OPTIONAL
        /* TODO: options & ulong */
        Z_PARAM_LONG(stack_page_size)
        Z_PARAM_LONG(c_stack_size)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(!swow_coroutine_construct(scoroutine, zcallable, stack_page_size, c_stack_size))) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

#define SWOW_COROUTINE_DECLARE_RESUME_TRANSFER(zdata) \
    zend_fcall_info fci = empty_fcall_info; \
    zval *zdata, _##zdata

#define SWOW_COROUTINE_HANDLE_RESUME_TRANSFER(zdata) \
    if (EXPECTED(fci.param_count == 0)) { \
        zdata = NULL; \
    } else if (EXPECTED(fci.param_count == 1)) { \
        zdata = fci.params; \
    } else /* if (fci.param_count > 1) */ { \
        if (UNEXPECTED(scoroutine->coroutine.state != CAT_COROUTINE_STATE_READY)) { \
            zend_throw_error(NULL, "Only one argument allowed when resuming an coroutine which is alive"); \
            RETURN_THROWS(); \
        } \
        zdata = &_##zdata; \
        ZVAL_PTR(zdata, &fci); \
    }

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_class_Swow_Coroutine_run, 1)
    ZEND_ARG_CALLABLE_INFO(0, callable, 0)
    ZEND_ARG_VARIADIC_INFO(0, data)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, run)
{
    zval *zcallable;
    SWOW_COROUTINE_DECLARE_RESUME_TRANSFER(zdata);

    ZEND_PARSE_PARAMETERS_START(1, -1)
        Z_PARAM_ZVAL(zcallable)
        Z_PARAM_VARIADIC('*', fci.params, fci.param_count)
    ZEND_PARSE_PARAMETERS_END();

    swow_coroutine_t *scoroutine = swow_coroutine_create(zcallable);
    if (UNEXPECTED(scoroutine == NULL)) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }

    SWOW_COROUTINE_HANDLE_RESUME_TRANSFER(zdata);
    if (UNEXPECTED(!swow_coroutine_resume(scoroutine, zdata, NULL))) {
        /* impossible? */
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        swow_coroutine_close(scoroutine);
        RETURN_THROWS();
    }
    RETURN_OBJ(&scoroutine->std);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Coroutine_resume, 0, ZEND_RETURN_VALUE, 0)
    ZEND_ARG_VARIADIC_INFO(0, data)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, resume)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();
    SWOW_COROUTINE_DECLARE_RESUME_TRANSFER(zdata);
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, -1)
        Z_PARAM_VARIADIC('*', fci.params, fci.param_count)
    ZEND_PARSE_PARAMETERS_END();

    SWOW_COROUTINE_HANDLE_RESUME_TRANSFER(zdata);

    ret = swow_coroutine_resume(scoroutine, zdata, return_value);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

#undef SWOW_COROUTINE_DECLARE_RESUME_TRANSFER
#undef SWOW_COROUTINE_HANDLE_RESUME_TRANSFER

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Coroutine_yield, 0, ZEND_RETURN_VALUE, 0)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, data, "null")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, yield)
{
    zval *zdata = NULL;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL(zdata)
    ZEND_PARSE_PARAMETERS_END();

    ret = swow_coroutine_yield(zdata, return_value);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getId, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getId)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(getThisCoroutine()->coroutine.id);
}

#define SWOW_COROUTINE_GETTER(getter) do { \
    ZEND_PARSE_PARAMETERS_NONE(); \
    swow_coroutine_t *scoroutine = getter; \
    if (UNEXPECTED(scoroutine == NULL)) { \
        RETURN_NULL(); \
    } \
    GC_ADDREF(&scoroutine->std); \
    RETURN_OBJ(&scoroutine->std); \
} while (0)

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_class_Swow_Coroutine_getCoroutine, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD_EX(Swow_Coroutine, getCoroutine, swow_coroutine_t *scoroutine)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(scoroutine == NULL)) {
        RETURN_NULL();
    }

    GC_ADDREF(&scoroutine->std);
    RETURN_OBJ(&scoroutine->std);
}

#define arginfo_class_Swow_Coroutine_getCurrent arginfo_class_Swow_Coroutine_getCoroutine

static PHP_METHOD(Swow_Coroutine, getCurrent)
{
    PHP_METHOD_CALL(Swow_Coroutine, getCoroutine, swow_coroutine_get_current());
}

#define arginfo_class_Swow_Coroutine_getMain arginfo_class_Swow_Coroutine_getCoroutine

static PHP_METHOD(Swow_Coroutine, getMain)
{
    PHP_METHOD_CALL(Swow_Coroutine, getCoroutine, swow_coroutine_get_main());
}

#define arginfo_class_Swow_Coroutine_getPrevious arginfo_class_Swow_Coroutine_getCoroutine

static PHP_METHOD(Swow_Coroutine, getPrevious)
{
    PHP_METHOD_CALL(Swow_Coroutine, getCoroutine, swow_coroutine_get_previous(getThisCoroutine()));
}

#undef SWOW_COROUTINE_GETTER

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getLong, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Swow_Coroutine_getState arginfo_class_Swow_Coroutine_getLong

static PHP_METHOD(Swow_Coroutine, getState)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(getThisCoroutine()->coroutine.state);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getString, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Swow_Coroutine_getStateName arginfo_class_Swow_Coroutine_getString

static PHP_METHOD(Swow_Coroutine, getStateName)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_coroutine_get_state_name(&getThisCoroutine()->coroutine));
}

#define arginfo_class_Swow_Coroutine_getRound arginfo_class_Swow_Coroutine_getLong

static PHP_METHOD(Swow_Coroutine, getRound)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(getThisCoroutine()->coroutine.round);
}

#define arginfo_class_Swow_Coroutine_getCurrentRound arginfo_class_Swow_Coroutine_getLong

static PHP_METHOD(Swow_Coroutine, getCurrentRound)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(CAT_COROUTINE_G(round));
}

#define arginfo_class_Swow_Coroutine_getElapsed arginfo_class_Swow_Coroutine_getLong

static PHP_METHOD(Swow_Coroutine, getElapsed)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_coroutine_get_elapsed(&getThisCoroutine()->coroutine));
}

#define arginfo_class_Swow_Coroutine_getElapsedAsString arginfo_class_Swow_Coroutine_getString

static PHP_METHOD(Swow_Coroutine, getElapsedAsString)
{
    char *elapsed;

    ZEND_PARSE_PARAMETERS_NONE();

    elapsed = cat_coroutine_get_elapsed_as_string(&getThisCoroutine()->coroutine);

    RETVAL_STRING(elapsed);

    cat_free(elapsed);
}

#define arginfo_class_Swow_Coroutine_getExitStatus arginfo_class_Swow_Coroutine_getLong

static PHP_METHOD(Swow_Coroutine, getExitStatus)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(scoroutine == swow_coroutine_get_main())) {
        RETURN_LONG(EG(exit_status));
    }

    RETURN_LONG(scoroutine->exit_status);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getBool, ZEND_RETURN_VALUE, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Swow_Coroutine_isAvailable arginfo_class_Swow_Coroutine_getBool

static PHP_METHOD(Swow_Coroutine, isAvailable)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(swow_coroutine_is_available(getThisCoroutine()));
}

#define arginfo_class_Swow_Coroutine_isAlive arginfo_class_Swow_Coroutine_getBool

static PHP_METHOD(Swow_Coroutine, isAlive)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(swow_coroutine_is_alive(getThisCoroutine()));
}

#define SWOW_COROUTINE_GET_EXECUTED_INFO_PARAMETERS_PARSER() \
    zend_long level = 0; \
    ZEND_PARSE_PARAMETERS_START(0, 1) \
        Z_PARAM_OPTIONAL \
        Z_PARAM_LONG(level) \
    ZEND_PARSE_PARAMETERS_END();

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getExecutedFilename, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, level, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getExecutedFilename)
{
    zend_string *filename;

    SWOW_COROUTINE_GET_EXECUTED_INFO_PARAMETERS_PARSER();

    filename = swow_coroutine_get_executed_filename(getThisCoroutine(), level);

    RETURN_STR(filename);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getExecutedLineno, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, level, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getExecutedLineno)
{
    zend_long lineno;

    SWOW_COROUTINE_GET_EXECUTED_INFO_PARAMETERS_PARSER();

    lineno = swow_coroutine_get_executed_lineno(getThisCoroutine(), level);

    RETURN_LONG(lineno);
}

#define arginfo_class_Swow_Coroutine_getExecutedFunctionName arginfo_class_Swow_Coroutine_getExecutedFilename

static PHP_METHOD(Swow_Coroutine, getExecutedFunctionName)
{
    zend_string *function_name;

    SWOW_COROUTINE_GET_EXECUTED_INFO_PARAMETERS_PARSER();

    function_name = swow_coroutine_get_executed_function_name(getThisCoroutine(), level);

    RETURN_STR(function_name);
}

#define SWOW_COROUTINE_GET_TRACE_PARAMETERS_PARSER() \
    zend_long level = 0; \
    zend_long limit = 0; \
    zend_long options = DEBUG_BACKTRACE_PROVIDE_OBJECT; \
    ZEND_PARSE_PARAMETERS_START(0, 3) \
        Z_PARAM_OPTIONAL \
        Z_PARAM_LONG(level) \
        Z_PARAM_LONG(limit) \
        Z_PARAM_LONG(options) \
    ZEND_PARSE_PARAMETERS_END()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getTrace, ZEND_RETURN_VALUE, 0, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, level, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, limit, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_LONG, 0, "DEBUG_BACKTRACE_PROVIDE_OBJECT")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getTrace)
{
    HashTable *trace;

    SWOW_COROUTINE_GET_TRACE_PARAMETERS_PARSER();

    trace = swow_coroutine_get_trace(getThisCoroutine(), level, limit, options);

    if (UNEXPECTED(trace == NULL)) {
        RETURN_EMPTY_ARRAY();
    }

    RETURN_ARR(trace);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getTraceAsString, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, level, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, limit, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_LONG, 0, "DEBUG_BACKTRACE_PROVIDE_OBJECT")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getTraceAsString)
{
    zend_string *trace;

    SWOW_COROUTINE_GET_TRACE_PARAMETERS_PARSER();

    trace = swow_coroutine_get_trace_as_string(getThisCoroutine(), level, limit, options);

    if (UNEXPECTED(trace == NULL)) {
        RETURN_EMPTY_STRING();
    }

    RETURN_STR(trace);
}

#define arginfo_class_Swow_Coroutine_getTraceAsList arginfo_class_Swow_Coroutine_getTrace

static PHP_METHOD(Swow_Coroutine, getTraceAsList)
{
    HashTable *trace;

    SWOW_COROUTINE_GET_TRACE_PARAMETERS_PARSER();

    trace = swow_coroutine_get_trace_as_list(getThisCoroutine(), level, limit, options);

    if (UNEXPECTED(trace == NULL)) {
        RETURN_EMPTY_ARRAY();
    }

    RETURN_ARR(trace);
}

#undef SWOW_COROUTINE_GET_TRACE_PARAMETERS_PARSER

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getDefinedVars, ZEND_RETURN_VALUE, 0, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, level, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getDefinedVars)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();
    zend_array *symbol_table;
    zend_long level = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(level)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(level < 0)) {
        zend_argument_value_error(1, "can not be negative");
        RETURN_THROWS();
    }

    symbol_table = swow_coroutine_get_defined_vars(scoroutine, level);

    if (UNEXPECTED(symbol_table == NULL)) {
        RETURN_EMPTY_ARRAY();
    }

    RETURN_ARR(symbol_table);
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(arginfo_class_Swow_Coroutine_setLocalVar, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, level, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, force, _IS_BOOL, 0, "true")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, setLocalVar)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();
    zend_string *name;
    zval *value;
    zend_long level = 0;
    zend_bool force = 1;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(2, 4)
        Z_PARAM_STR(name)
        Z_PARAM_ZVAL(value)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(level)
        Z_PARAM_BOOL(force)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(level < 0)) {
        zend_argument_value_error(1, "can not be negative");
        RETURN_THROWS();
    }

    ret = swow_coroutine_set_local_var(scoroutine, name, value, level, force);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_eval, ZEND_RETURN_VALUE, 0, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO(0, string, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, level, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, eval)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();
    zend_string *string;
    zend_long level = 0;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(string)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(level)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(!swow_coroutine_is_alive(scoroutine))) {
        swow_throw_exception(swow_coroutine_exception_ce, CAT_ESRCH, "Coroutine is not alive");
        RETURN_THROWS();
    }

    ret = swow_coroutine_eval(scoroutine, string, level, return_value);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_call, ZEND_RETURN_VALUE, 0, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO(0, callable, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, call)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();
    zval *zcallable;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(zcallable)
    ZEND_PARSE_PARAMETERS_END();

    ret = swow_coroutine_call(scoroutine, zcallable, return_value);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Coroutine_throw, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_OBJ_INFO(0, throwable, Throwable, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, throw)
{
    zval *zexception;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(zexception, zend_ce_throwable)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(!swow_coroutine_throw(getThisCoroutine(), Z_OBJ_P(zexception), return_value)))  {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

#define SWOW_COROUTINE_MESSAGE_AND_CODE_PARAMETERS_PARSER() \
    char *message = NULL; \
    size_t message_length = 0; \
    zend_long code = ~0; \
    ZEND_PARSE_PARAMETERS_START(0, 2) \
        Z_PARAM_OPTIONAL \
        Z_PARAM_STRING(message, message_length) \
        Z_PARAM_LONG(code) \
    ZEND_PARSE_PARAMETERS_END(); \

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Coroutine_throwCrossException, 0, ZEND_RETURN_VALUE, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, message, IS_STRING, 0, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, code, IS_LONG, 0, "null")
ZEND_END_ARG_INFO()

#define arginfo_class_Swow_Coroutine_term arginfo_class_Swow_Coroutine_throwCrossException

static PHP_METHOD(Swow_Coroutine, term)
{
    SWOW_COROUTINE_MESSAGE_AND_CODE_PARAMETERS_PARSER();

    if (UNEXPECTED(!swow_coroutine_term(getThisCoroutine(), message, code, return_value)))  {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

#define arginfo_class_Swow_Coroutine_kill arginfo_class_Swow_Coroutine_throwCrossException

static PHP_METHOD(Swow_Coroutine, kill)
{
    SWOW_COROUTINE_MESSAGE_AND_CODE_PARAMETERS_PARSER();

    if (UNEXPECTED(!swow_coroutine_kill(getThisCoroutine(), message, code))) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

#undef SWOW_COROUTINE_MESSAGE_AND_CODE_PARAMETERS_PARSER

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_count, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, count)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(zend_hash_num_elements(SWOW_COROUTINE_G(map)));
}

ZEND_BEGIN_ARG_WITH_RETURN_THIS_OR_NULL_INFO_EX(arginfo_class_Swow_Coroutine_get, 0)
    ZEND_ARG_TYPE_INFO(0, id, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, get)
{
    zval *zcoroutine;
    zend_long id;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(id)
    ZEND_PARSE_PARAMETERS_END();

    zcoroutine = zend_hash_index_find(SWOW_COROUTINE_G(map), id);

    if (UNEXPECTED(zcoroutine == NULL)) {
        RETURN_NULL();
    }

    RETURN_ZVAL(zcoroutine, 1, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getAll, ZEND_RETURN_VALUE, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getAll)
{
    HashTable *map = SWOW_COROUTINE_G(map);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_ARR(zend_array_dup(map));
}

#ifdef SWOW_COROUTINE_ENABLE_CUSTOM_ENTRY
ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Coroutine_extends, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_TYPE_INFO(0, class, IS_STRING, 0)
ZEND_END_ARG_INFO()

static cat_data_t swow_coroutine_custom_resume(cat_coroutine_t *coroutine, cat_data_t data)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_handle(coroutine);
    zval *retval = scoroutine->executor ? &scoroutine->executor->zdata : NULL;
    zval zscoroutine;

    // TOOD: if PHP and zdata == IS_PTR

    ZVAL_OBJ(&zscoroutine, &scoroutine->std);
    swow_call_method_with_1_params(&zscoroutine, scoroutine->std.ce, NULL, "resume", retval, (zval *) data);

    return retval;
}

static PHP_METHOD(Swow_Coroutine, extends)
{
    zend_string *name;
    zend_class_entry *ce;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END();

    ce = zend_lookup_class(name);
    if (UNEXPECTED(ce == NULL)) {
        swow_throw_error(swow_coroutine_error_ce, "Class %s dose not exist", ZSTR_VAL(name));
        RETURN_THROWS();
    }
    if (ce == swow_coroutine_ce) {
        SWOW_COROUTINE_G(custom_entry) = NULL;
        cat_coroutine_register_resume(swow_coroutine_resume_standard);
        return;
    }
    if (UNEXPECTED(!instanceof_function(ce, swow_coroutine_ce))) {
        swow_throw_error(swow_coroutine_error_ce, "Class %s must extends %s", ZSTR_VAL(name), ZSTR_VAL(swow_coroutine_error_ce->name));
        RETURN_THROWS();
    }
    SWOW_COROUTINE_G(custom_entry) = ce;
    cat_coroutine_register_resume(swow_coroutine_custom_resume);
    // TODO: replace self::resume function_handler
}
#endif

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine___debugInfo, ZEND_RETURN_VALUE, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, __debugInfo)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();
    cat_coroutine_t *coroutine = &scoroutine->coroutine;
    zval zdebug_info;
    char *tmp;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(&zdebug_info);
    add_assoc_long(&zdebug_info, "id", coroutine->id);
    add_assoc_string(&zdebug_info, "state", cat_coroutine_get_state_name(coroutine));
    tmp = cat_coroutine_get_elapsed_as_string(coroutine);
    add_assoc_long(&zdebug_info, "round", coroutine->round);
    add_assoc_string(&zdebug_info, "elapsed", tmp);
    cat_free(tmp);
    if (swow_coroutine_is_alive(scoroutine)) {
        const zend_long level = 0;
        const zend_long limit = 0;
        const zend_long options = DEBUG_BACKTRACE_PROVIDE_OBJECT;
        smart_str str = { 0 };
        smart_str_appendc(&str, '\n');
        swow_coroutine_get_trace_as_smart_str(scoroutine, &str, level, limit, options);
        smart_str_appendc(&str, '\n');
        smart_str_0(&str);
        add_assoc_str(&zdebug_info, "trace", str.s);
    }

    RETURN_DEBUG_INFO_WITH_PROPERTIES(&zdebug_info);
}

static const zend_function_entry swow_coroutine_methods[] = {
    PHP_ME(Swow_Coroutine, __construct,             arginfo_class_Swow_Coroutine___construct,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, run,                     arginfo_class_Swow_Coroutine_run,                     ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Coroutine, resume,                  arginfo_class_Swow_Coroutine_resume,                  ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, yield,                   arginfo_class_Swow_Coroutine_yield,                   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Coroutine, getId,                   arginfo_class_Swow_Coroutine_getId,                   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getCurrent,              arginfo_class_Swow_Coroutine_getCurrent,              ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Coroutine, getMain,                 arginfo_class_Swow_Coroutine_getMain,                 ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Coroutine, getPrevious,             arginfo_class_Swow_Coroutine_getPrevious,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getState,                arginfo_class_Swow_Coroutine_getState,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getStateName,            arginfo_class_Swow_Coroutine_getStateName,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getRound,                arginfo_class_Swow_Coroutine_getRound,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getCurrentRound,         arginfo_class_Swow_Coroutine_getCurrentRound,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getElapsed,              arginfo_class_Swow_Coroutine_getElapsed,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getElapsedAsString,      arginfo_class_Swow_Coroutine_getElapsedAsString,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getExitStatus,           arginfo_class_Swow_Coroutine_getExitStatus,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, isAvailable,             arginfo_class_Swow_Coroutine_isAvailable,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, isAlive,                 arginfo_class_Swow_Coroutine_isAlive,                 ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getExecutedFilename,     arginfo_class_Swow_Coroutine_getExecutedFilename,     ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getExecutedLineno,       arginfo_class_Swow_Coroutine_getExecutedLineno,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getExecutedFunctionName, arginfo_class_Swow_Coroutine_getExecutedFunctionName, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getTrace,                arginfo_class_Swow_Coroutine_getTrace,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getTraceAsString,        arginfo_class_Swow_Coroutine_getTraceAsString,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getTraceAsList,          arginfo_class_Swow_Coroutine_getTraceAsList,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getDefinedVars,          arginfo_class_Swow_Coroutine_getDefinedVars,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, setLocalVar,             arginfo_class_Swow_Coroutine_setLocalVar,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, eval,                    arginfo_class_Swow_Coroutine_eval,                    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, call,                    arginfo_class_Swow_Coroutine_call,                    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, throw,                   arginfo_class_Swow_Coroutine_throw,                   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, term,                    arginfo_class_Swow_Coroutine_term,                    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, kill,                    arginfo_class_Swow_Coroutine_kill,                    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, count,                   arginfo_class_Swow_Coroutine_count,                   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Coroutine, get,                     arginfo_class_Swow_Coroutine_get,                     ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Coroutine, getAll,                  arginfo_class_Swow_Coroutine_getAll,                  ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
#ifdef SWOW_COROUTINE_ENABLE_CUSTOM_ENTRY
    PHP_ME(Swow_Coroutine, extends,                 arginfo_class_Swow_Coroutine_extends,                 ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
#endif
    /* magic */
    PHP_ME(Swow_Coroutine, __debugInfo,             arginfo_class_Swow_Coroutine___debugInfo,             ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* handlers */

static HashTable *swow_coroutine_get_gc(ZEND_GET_GC_PARAMATERS)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_object(Z7_OBJ_P(object));
    zval *zcallable = scoroutine->executor ? &scoroutine->executor->zcallable : NULL;

    if (zcallable == NULL || ZVAL_IS_NULL(zcallable)) {
        ZEND_GET_GC_RETURN_EMPTY();
    }

    ZEND_GET_GC_RETURN_ZVAL(zcallable);
}

/* exception/error */

static zend_object *swow_coroutine_cross_exception_create_object(zend_class_entry *ce)
{
    zend_object *object;
    zval zcoroutine;

    object = swow_exception_create_object(ce);
    ZVAL7_ALLOC_OBJECT(object);
    ZVAL_OBJ(&zcoroutine, &swow_coroutine_get_current()->std);
    zend_update_property(ce, ZVAL7_OBJECT(object), ZEND_STRL("coroutine"), &zcoroutine);

    return object;
}

#define arginfo_class_Swow_Coroutine_Cross_Exception_getCoroutine arginfo_class_Swow_Coroutine_getCoroutine

static PHP_METHOD(Swow_Coroutine_Cross_Exception, getCoroutine)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_THIS_PROPERTY("coroutine");
}

static const zend_function_entry swow_coroutine_cross_exception_methods[] = {
    PHP_ME(Swow_Coroutine_Cross_Exception, getCoroutine, arginfo_class_Swow_Coroutine_Cross_Exception_getCoroutine, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* hack error hook */

#if PHP_VERSION_ID < 80000
#define ZEND_ERROR_CB_LAST_ARG_D const char *format, va_list args
#define ZEND_ERROR_CB_LAST_ARG_RELAY format, args
#else
#define ZEND_ERROR_CB_LAST_ARG_D     zend_string *message
#define ZEND_ERROR_CB_LAST_ARG_RELAY message
#endif

static void (*original_zend_error_cb)(int type, const char *error_filename, const uint32_t error_lineno, ZEND_ERROR_CB_LAST_ARG_D);

static void swow_call_original_zend_error_cb(int type, const char *error_filename, const uint32_t error_lineno, ZEND_ERROR_CB_LAST_ARG_D)
{
    if (EXPECTED(original_zend_error_cb != NULL)) {
        zend_try {
            original_zend_error_cb(type, error_filename, error_lineno, ZEND_ERROR_CB_LAST_ARG_RELAY);
        } zend_catch {
            exit(EG(exit_status));
        } zend_end_try();
    }
}

static void swow_coroutine_error_cb(int type, const char *error_filename, const uint32_t error_lineno, ZEND_ERROR_CB_LAST_ARG_D)
{
#if PHP_VERSION_ID >= 80000
    const char *format = ZSTR_VAL(message);
#endif
    zend_string *new_message = NULL;

    if (!SWOW_COROUTINE_G(classic_error_handler)) {
        const char *original_type_string = swow_strerrortype(type);
        zend_string *trace = NULL;
        /* strncmp maybe macro if compiler is Virtual Pascal? */
        if (strncmp(format, "Uncaught ", sizeof("Uncaught ") - 1) == 0) {
            /* hack hook for error in main */
            if (swow_coroutine_get_current() == swow_coroutine_get_main()) {
                /* keep silent for kill/term(0) */
                if (SWOW_COROUTINE_G(silent_exception_in_main)) {
                    SWOW_COROUTINE_G(silent_exception_in_main) = cat_false;
                    return;
                } else {
                    zend_long severity = SWOW_COROUTINE_G(exception_error_severity);
                    if (severity == E_NONE) {
                        return;
                    }
                    type = severity;
                    original_type_string = swow_strerrortype(type);
                }
            }
            /* the exception of the coroutines will never cause the process to exit */
            if (type & E_FATAL_ERRORS) {
                type = E_WARNING;
            }
        } else {
            if (EG(current_execute_data) != NULL) {
                trace = swow_debug_get_trace_as_string(DEBUG_BACKTRACE_PROVIDE_OBJECT, 0);
            }
        }
        do {
            /* Notice: current coroutine is NULL before RINIT */
            swow_coroutine_t *scoroutine = swow_coroutine_get_current();
            cat_coroutine_id_t id = scoroutine != NULL ? scoroutine->coroutine.id : CAT_COROUTINE_MAIN_ID;

            new_message = zend_strpprintf(0,
                "[%s in R" CAT_COROUTINE_ID_FMT "] %s%s%s%s",
                original_type_string,
                id,
                format,
                trace != NULL ? "\nStack trace:\n" : "",
                trace != NULL ? ZSTR_VAL(trace) : "",
                trace != NULL ? "\n  triggered" : ""
            );
#if PHP_VERSION_ID < 80000
            format = ZSTR_VAL(new_message);
#else
            message = new_message;
#endif
        } while (0);
        if (trace != NULL) {
            zend_string_release(trace);
        }
    }
    if (UNEXPECTED(type & E_FATAL_ERRORS)) {
        /* update executor for backtrace */
        if (EG(current_execute_data) != NULL) {
            swow_coroutine_executor_save(swow_coroutine_get_current()->executor);
        }
    }
    swow_call_original_zend_error_cb(type, error_filename, error_lineno, ZEND_ERROR_CB_LAST_ARG_RELAY);
    if (new_message != NULL) {
        zend_string_release(new_message);
    }
}

/* hook exception */

static void (*original_zend_throw_exception_hook)(zend7_object *exception);

static void swow_zend_throw_exception_hook(zend7_object *exception)
{
    if (swow_coroutine_get_current() == swow_coroutine_get_main()) {
        if (swow_coroutine_exception_should_be_silent(Z7_OBJ_P(exception))) {
            SWOW_COROUTINE_G(silent_exception_in_main) = cat_true;
        }
    }
    if (original_zend_throw_exception_hook != NULL) {
        original_zend_throw_exception_hook(exception);
    }
}

/* hook exit */

static user_opcode_handler_t original_zend_exit_handler;

static int swow_coroutine_exit_handler(zend_execute_data *execute_data)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_current();
    const zend_op *opline = EX(opline);
    zval *zstatus = NULL;
    zend_long status;

    if (opline->op1_type != IS_UNUSED) {
        if (opline->op1_type == IS_CONST) {
            // see: https://github.com/php/php-src/commit/e70618aff6f447a298605d07648f2ce9e5a284f5
#ifdef EX_CONSTANT
            zstatus = EX_CONSTANT(opline->op1);
#else
            zstatus = RT_CONSTANT(opline, opline->op1);
#endif
        } else {
            zstatus = EX_VAR(opline->op1.var);
        }
        if (Z_ISREF_P(zstatus)) {
            zstatus = Z_REFVAL_P(zstatus);
        }
        if (zstatus != NULL && Z_TYPE_P(zstatus) == IS_LONG && Z_LVAL_P(zstatus) != 0) {
            /* exit abnormally */
            status = Z_LVAL_P(zstatus);
        } else {
            /* exit normally */
            if (zstatus != NULL && Z_TYPE_P(zstatus) != IS_LONG) {
                zend_print_zval(zstatus, 0);
            }
            status = 0;
        }
        if ((opline->op1_type) & (IS_TMP_VAR | IS_VAR)) {
            zval_ptr_dtor_nogc(zstatus);
        }
    } else {
        status = 0;
    }
    if (EG(exception) == NULL) {
#if PHP_VERSION_ID < 80000
        zend_throw_exception(swow_coroutine_kill_exception_ce, NULL, 0);
#else
        zend_throw_unwind_exit();
#endif
    }
    if (scoroutine != swow_coroutine_get_main()) {
        scoroutine->exit_status = status;
    } else {
        EG(exit_status) = status;
    }

    return ZEND_USER_OPCODE_CONTINUE;
}

/* hook silence */

#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
static user_opcode_handler_t original_zend_begin_silence_handler;
static user_opcode_handler_t original_zend_end_silence_handler;

static int swow_coroutine_begin_silence_handler(zend_execute_data *execute_data)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_current();
    scoroutine->executor->error_reporting = EG(error_reporting) | E_MAGIC;
    return ZEND_USER_OPCODE_DISPATCH;
}

static int swow_coroutine_end_silence_handler(zend_execute_data *execute_data)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_current();
    scoroutine->executor->error_reporting = 0;
    return ZEND_USER_OPCODE_DISPATCH;
}
#endif

int swow_coroutine_module_init(INIT_FUNC_ARGS)
{
    if (!cat_coroutine_module_init()) {
        return FAILURE;
    }

    CAT_GLOBALS_REGISTER(swow_coroutine, CAT_GLOBALS_CTOR(swow_coroutine), NULL);

    SWOW_COROUTINE_G(runtime_state) = SWOW_COROUTINE_RUNTIME_STATE_NONE;

    swow_coroutine_ce = swow_register_internal_class(
        "Swow\\Coroutine", NULL, swow_coroutine_methods,
        &swow_coroutine_handlers, NULL,
        cat_false, cat_false, cat_false,
        swow_coroutine_create_object,
        swow_coroutine_free_object,
        XtOffsetOf(swow_coroutine_t, std)
    );
    swow_coroutine_handlers.get_gc = swow_coroutine_get_gc;
    swow_coroutine_handlers.dtor_obj = swow_coroutine_dtor_object;
    /* constants */
#define SWOW_COROUTINE_STATE_GEN(name, value, unused) \
    zend_declare_class_constant_long(swow_coroutine_ce, ZEND_STRL("STATE_" #name), (value));
    CAT_COROUTINE_STATE_MAP(SWOW_COROUTINE_STATE_GEN)
#undef SWOW_COROUTINE_STATE_GEN

    /* Exception for common errors */
    swow_coroutine_exception_ce = swow_register_internal_class(
        "Swow\\Coroutine\\Exception", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );

    /* Exceptions for cross throw */
    swow_coroutine_cross_exception_ce = swow_register_internal_class(
        "Swow\\Coroutine\\CrossException", swow_coroutine_exception_ce, swow_coroutine_cross_exception_methods, NULL, NULL, cat_true, cat_true, cat_true,
        swow_coroutine_cross_exception_create_object, NULL, 0
    );
    zend_declare_property_null(swow_coroutine_cross_exception_ce, ZEND_STRL("coroutine"), ZEND_ACC_PROTECTED);
    /* TODO: zend_declare_property_long(swow_coroutine_error_ce, ZEND_STRL("severity"), 0, ZEND_ACC_PROTECTED); */

    swow_coroutine_term_exception_ce = swow_register_internal_class(
        "Swow\\Coroutine\\TermException", swow_coroutine_cross_exception_ce, NULL, NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );
    swow_coroutine_kill_exception_ce = swow_register_internal_class(
        "Swow\\Coroutine\\KillException", swow_coroutine_cross_exception_ce, NULL, NULL, NULL, cat_true, cat_true, cat_true, NULL, NULL, 0
    );
    zend_class_implements(swow_coroutine_kill_exception_ce, 1, swow_uncatchable_ce);

    /* hook zend_error_cb */
    do {
        original_zend_error_cb = zend_error_cb;
        zend_error_cb = swow_coroutine_error_cb;
    } while (0);

    /* hook zend_throw_exception_hook */
    do {
        original_zend_throw_exception_hook = zend_throw_exception_hook;
        zend_throw_exception_hook = swow_zend_throw_exception_hook;
    } while (0);

    /* hook opcode exit (pre) */
    do {
        original_zend_exit_handler = (user_opcode_handler_t) -1;
    } while (0);

#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
    /* hook opcode silence (pre) */
    do {
        original_zend_begin_silence_handler = (user_opcode_handler_t) -1;
        original_zend_end_silence_handler = (user_opcode_handler_t) -1;
    } while (0);
#endif

    return SUCCESS;
}

int swow_coroutine_runtime_init(INIT_FUNC_ARGS)
{
    if (!cat_coroutine_runtime_init()) {
        return FAILURE;
    }

    /* hook opcode exit */
    if (original_zend_exit_handler == (user_opcode_handler_t) -1) {
        original_zend_exit_handler = zend_get_user_opcode_handler(ZEND_EXIT);
        zend_set_user_opcode_handler(ZEND_EXIT, swow_coroutine_exit_handler);
    }

#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
    /* hook opcode silence */
    if (original_zend_begin_silence_handler == (user_opcode_handler_t) -1) {
        original_zend_begin_silence_handler = zend_get_user_opcode_handler(ZEND_BEGIN_SILENCE);
        zend_set_user_opcode_handler(ZEND_BEGIN_SILENCE, swow_coroutine_begin_silence_handler);
    }
    if (original_zend_end_silence_handler == (user_opcode_handler_t) -1) {
        original_zend_end_silence_handler = zend_get_user_opcode_handler(ZEND_END_SILENCE);
        zend_set_user_opcode_handler(ZEND_END_SILENCE, swow_coroutine_end_silence_handler);
    }
#endif

    SWOW_COROUTINE_G(original_resume) = cat_coroutine_register_resume(
        swow_coroutine_resume_standard
    );

    SWOW_COROUTINE_G(default_stack_page_size) = SWOW_COROUTINE_DEFAULT_STACK_PAGE_SIZE; /* TODO: get php.ini */
    SWOW_COROUTINE_G(classic_error_handler) = cat_false; /* TODO: get php.ini */
    SWOW_COROUTINE_G(exception_error_severity) = E_ERROR; /* TODO: get php.ini */

    SWOW_COROUTINE_G(runtime_state) = SWOW_COROUTINE_RUNTIME_STATE_RUNNING;

    SWOW_COROUTINE_G(readonly.original_create_object) = NULL;
    SWOW_COROUTINE_G(readonly.original_resume) = NULL;

    SWOW_COROUTINE_G(silent_exception_in_main) = cat_false;

    /* create scoroutine map */
    do {
        zval ztmp;
        array_init(&ztmp);
        SWOW_COROUTINE_G(map) = Z_ARRVAL(ztmp);
    } while (0);

    /* create main scoroutine */
    swow_coroutine_main_create();

    return SUCCESS;
}

// TODO: killall API
#ifdef CAT_DO_NOT_OPTIMIZE
static void swow_coroutines_kill_destructor(zval *zscoroutine)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_object(Z_OBJ_P(zscoroutine));
    CAT_ASSERT(swow_coroutine_is_alive(scoroutine));
    if (UNEXPECTED(!swow_coroutine_kill(scoroutine, "Coroutine is forced to kill when the runtime shutdown", ~0))) {
        cat_core_error(COROUTINE, "Execute kill destructor failed, reason: %s", cat_get_last_error_message());
    }
    swow_coroutine_close(scoroutine);
}
#endif

int swow_coroutine_runtime_shutdown(SHUTDOWN_FUNC_ARGS)
{
    SWOW_COROUTINE_G(runtime_state) = SWOW_COROUTINE_RUNTIME_STATE_IN_SHUTDOWN;

#ifdef CAT_DO_NOT_OPTIMIZE /* the optimization deps on event scheduler */
    /* destruct active scoroutines */
    HashTable *map = SWOW_COROUTINE_G(map);
    do {
        /* kill first (for memory safety) */
        HashTable *map_copy = zend_array_dup(map);
        /* kill all coroutines */
        zend_hash_index_del(map_copy, swow_coroutine_get_main()->coroutine.id);
        map_copy->pDestructor = swow_coroutines_kill_destructor;
        zend_array_destroy(map_copy);
    } while (zend_hash_num_elements(map) != 1);
#endif

    /* close main scoroutine */
    swow_coroutine_main_close();

    /* destroy map */
    zend_array_destroy(SWOW_COROUTINE_G(map));
    SWOW_COROUTINE_G(map) = NULL;

    /* recover resume */
    cat_coroutine_register_resume(
        SWOW_COROUTINE_G(original_resume)
    );

    if (!cat_coroutine_runtime_shutdown()) {
        return FAILURE;
    }

    SWOW_COROUTINE_G(runtime_state) = SWOW_COROUTINE_RUNTIME_STATE_NONE;

    return SUCCESS;
}
