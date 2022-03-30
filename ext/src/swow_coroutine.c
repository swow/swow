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

#ifdef SWOW_COROUTINE_MOCK_FIBER_CONTEXT
# include "zend_observer.h"
#endif

#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
# define E_MAGIC (1 << 31)
#endif

SWOW_API zend_class_entry *swow_coroutine_ce;
SWOW_API zend_object_handlers swow_coroutine_handlers;
SWOW_API zend_object_handlers swow_coroutine_main_handlers;

SWOW_API zend_class_entry *swow_coroutine_exception_ce;

#if PHP_VERSION_ID <= 80007
/* See: https://github.com/php/php-src/pull/6640
   and: https://github.com/php/php-src/commit/d4a206b27679779b698c4cbc9f6b17e082203073 */
# warning "JIT support with Swow require PHP 8.0.7 or later, see: https://github.com/php/php-src/pull/6640"
#endif

/* Internal pseudo-exception that is not exposed to userland. */
#if PHP_VERSION_ID > 80012
/* See: https://github.com/php/php-src/pull/7459 */
# define SWOW_NATIVE_UNWIND_EXIT_SUPPORT 1
#else
# warning #warning "Native unwind exit support with Swow require PHP 8.0.12 or later, see: https://github.com/php/php-src/pull/7459"
#endif

#ifndef SWOW_NATIVE_UNWIND_EXIT_SUPPORT
static zend_class_entry *swow_coroutine_unwind_exit_ce;
#endif
#define SWOW_COROUTINE_UNWIND_EXIT_MAGIC               ((zend_object *) -1)
#define SWOW_COROUTINE_IS_UNWIND_EXIT_MAGIC(exception) (exception == SWOW_COROUTINE_UNWIND_EXIT_MAGIC)

static zend_function swow_coroutine_internal_function;

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
static ZEND_COLD void swow_coroutine_handle_cross_exception(zend_object *cross_exception);
static ZEND_COLD void swow_coroutine_throw_unwind_exit(void);
static zend_always_inline zend_bool swow_coroutine_has_unwind_exit(zend_object *exception);

static zend_always_inline size_t swow_coroutine_align_stack_page_size(size_t size)
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

#ifdef SWOW_COROUTINE_MOCK_FIBER_CONTEXT
static zend_never_inline void swow_coroutine_fiber_context_init(swow_coroutine_t *scoroutine)
{
    zend_fiber_context *fiber_context = (zend_fiber_context *) emalloc(sizeof(*fiber_context));
    fiber_context->handle = (void *) -1;
    fiber_context->kind = swow_coroutine_ce;
    fiber_context->function = (zend_fiber_coroutine) -1;
    fiber_context->stack = (zend_fiber_stack *) -1;
    scoroutine->fiber_context = fiber_context;
}

static zend_always_inline void swow_coroutine_fiber_context_try_init(swow_coroutine_t *scoroutine)
{
    if (UNEXPECTED(SWOW_NTS_G(has_debug_extension))) {
        swow_coroutine_fiber_context_init(scoroutine);
    } else {
        scoroutine->fiber_context = NULL;
    }
}
#endif

static zend_object *swow_coroutine_create_object(zend_class_entry *ce)
{
    swow_coroutine_t *scoroutine = swow_object_alloc(swow_coroutine_t, ce, swow_coroutine_handlers);

    scoroutine->coroutine.state = CAT_COROUTINE_STATE_NONE;
    scoroutine->executor = NULL;
    scoroutine->exit_status = 0;
#ifdef SWOW_COROUTINE_MOCK_FIBER_CONTEXT
    swow_coroutine_fiber_context_try_init(scoroutine);
#endif

    return &scoroutine->std;
}

static void swow_coroutine_dtor_object(zend_object *object)
{
    /* force kill the coroutine if it is still alive */
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_object(object);

    if (UNEXPECTED(swow_coroutine_is_alive(scoroutine))) {
        /* not finished, should be discard */
        if (UNEXPECTED(!swow_coroutine_kill(scoroutine))) {
            CAT_CORE_ERROR(COROUTINE, "Kill coroutine failed when destruct object, reason: %s", cat_get_last_error_message());
        }
    }
}

static void swow_coroutine_free_object(zend_object *object)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_object(object);

    if (UNEXPECTED(swow_coroutine_is_available(scoroutine))) {
        /* created but never run */
        swow_coroutine_shutdown(scoroutine);
        cat_coroutine_free(&scoroutine->coroutine);
    }

#ifdef SWOW_COROUTINE_MOCK_FIBER_CONTEXT
    if (scoroutine->fiber_context != NULL) {
        efree(scoroutine->fiber_context);
    }
#endif

    zend_object_std_dtor(&scoroutine->std);
}

static zend_always_inline void swow_coroutine_add_to_map(swow_coroutine_t *scoroutine, HashTable *map)
{
    zval ztmp;
    ZVAL_OBJ(&ztmp, &scoroutine->std);
    zend_hash_index_add_new(map, scoroutine->coroutine.id, &ztmp);
    GC_ADDREF(&scoroutine->std);
}

static zend_always_inline void swow_coroutine_remove_from_map(swow_coroutine_t *scoroutine, HashTable *map)
{
    zend_hash_index_del(map, scoroutine->coroutine.id);
}

static CAT_COLD void swow_coroutine_function_handle_exception(void)
{
    ZEND_ASSERT(EG(exception) != NULL);

    zend_exception_restore();

    if (swow_coroutine_has_unwind_exit(EG(exception))) {
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
            if (EG(exception) != NULL) {
                OBJ_RELEASE(EG(exception));
                EG(exception) = NULL;
            }
            OBJ_RELEASE(old_exception);
        } else {
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
    swow_coroutine_t *scoroutine = swow_coroutine_get_current();
    swow_coroutine_executor_t *executor = scoroutine->executor;
    zend_fcall_info fci;
    zval retval;

    ZEND_ASSERT(executor != NULL);

    /* add to scoroutines map (we can not add before run otherwise refcount would never be 0) */
    swow_coroutine_add_to_map(scoroutine, SWOW_COROUTINE_G(map));

    /* clear accept zval data flag (it was set in constructor) */
    scoroutine->coroutine.flags ^= SWOW_COROUTINE_FLAG_ACCEPT_ZDATA;
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
    fci.named_params = NULL;
    fci.retval = &retval;

    /* call function */
    (void) zend_call_function(&fci, &executor->fcall.fcc);
    if (UNEXPECTED(EG(exception) != NULL)) {
        swow_coroutine_function_handle_exception();
    }

    /* discard all possible resources (e.g. variable by "use" in zend_closure) */
    swow_fcall_storage_release(&executor->fcall);
    if (UNEXPECTED(EG(exception) != NULL)) {
        swow_coroutine_function_handle_exception();
    }

    /* call __destruct() first here (prevent destructing in scheduler) */
    if (scoroutine->std.ce->destructor != NULL) {
        zend_objects_destroy_object(&scoroutine->std);
        if (UNEXPECTED(EG(exception) != NULL)) {
            swow_coroutine_function_handle_exception();
        }
    }
    /* do not call __destruct() anymore  */
    GC_ADD_FLAGS(&scoroutine->std, IS_OBJ_DESTRUCTOR_CALLED);

    /* ob end clean */
#ifdef SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS
    swow_output_globals_fast_shutdown();
#endif

    /* solve retval */
    if (UNEXPECTED(Z_TYPE_P(fci.retval) != IS_UNDEF && Z_TYPE_P(fci.retval) != IS_NULL)) {
        /* let data handler know we can solve zval data */
        scoroutine->coroutine.flags |= SWOW_COROUTINE_FLAG_ACCEPT_ZDATA;
        /* make sure the memory space of zdata is safe */
        zval *ztransfer_data = &SWOW_COROUTINE_G(ztransfer_data);
        ZVAL_COPY_VALUE(ztransfer_data, fci.retval);
        return ztransfer_data;
    }

    return NULL;
}

static cat_bool_t swow_coroutine_construct(swow_coroutine_t *scoroutine, zval *zcallable, size_t stack_page_size, size_t c_stack_size)
{
    swow_coroutine_executor_t *executor;
    swow_fcall_storage_t fcall;
    cat_coroutine_t *coroutine;

    if (!swow_fcall_storage_create(&fcall, zcallable)) {
        cat_update_last_error_with_previous("Coroutine construct failed");
        return cat_false;
    }

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
        coroutine->flags |= SWOW_COROUTINE_FLAG_ACCEPT_ZDATA;
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
        executor->vm_stack_page_size = stack_page_size;
        do {
            /* add const to make sure it would not be modified */
            executor->root_execute_data = (zend_execute_data *) executor->vm_stack_top;
            memset(executor->root_execute_data, 0, sizeof(*executor->root_execute_data));
            executor->root_execute_data->func = (zend_function *) &swow_coroutine_internal_function;
            executor->vm_stack_top += ZEND_CALL_FRAME_SLOT;
        } while (0);
        executor->current_execute_data = executor->root_execute_data;
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
        /* save function info */
        executor->fcall = fcall;
        /* it's unnecessary to init the zdata */
        /* ZVAL_UNDEF(&executor->zdata); */
    } while (0);

    scoroutine->executor = executor;

    return cat_true;
}

static void swow_coroutine_shutdown(swow_coroutine_t *scoroutine)
{
    swow_coroutine_executor_t *executor = scoroutine->executor;

    if (UNEXPECTED(executor == NULL)) {
        // Pure C stacked coroutine (e.g. scheduler)
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
    if (UNEXPECTED(swow_fcall_storage_is_available(&executor->fcall))) {
        swow_fcall_storage_release(&executor->fcall);
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
    scoroutine->std.handlers = &swow_coroutine_main_handlers;

    /* register first (sync coroutine info) */
    SWOW_COROUTINE_G(original_main) = cat_coroutine_register_main(&scoroutine->coroutine);

    scoroutine->executor = ecalloc(1, sizeof(*scoroutine->executor));
    scoroutine->executor->root_execute_data = (zend_execute_data *) EG(vm_stack)->top;
    // memset(&scoroutine->executor->fcall, 0, sizeof(scoroutine->executor->fcall));
#ifdef SWOW_COROUTINE_MOCK_FIBER_CONTEXT
    efree(scoroutine->fiber_context);
    scoroutine->fiber_context = EG(main_fiber_context);
#endif

    /* add main scoroutine to the map */
    do {
        zval zscoroutine;
        ZVAL_OBJ(&zscoroutine, &scoroutine->std);
        zend_hash_index_update(SWOW_COROUTINE_G(map), scoroutine->coroutine.id, &zscoroutine);
        /* Notice: we have 1 ref by create */
        GC_ADDREF(&scoroutine->std);
    } while (0);
}

static void swow_coroutine_main_close(void)
{
    swow_coroutine_t *scoroutine;

    scoroutine = swow_coroutine_get_main();

    /* revert globals main */
    cat_coroutine_register_main(SWOW_COROUTINE_G(original_main));

    /* hack way to shutdown the main */
    scoroutine->coroutine.state = CAT_COROUTINE_STATE_DEAD;
    scoroutine->executor->vm_stack = NULL;
#ifdef SWOW_COROUTINE_MOCK_FIBER_CONTEXT
    scoroutine->fiber_context = NULL;
#endif
    swow_coroutine_shutdown(scoroutine);

    /* release main scoroutine */
    swow_coroutine_close(scoroutine);
    EG(objects_store).object_buckets[0] = NULL;
}

static void swow_coroutine_main_dtor_object(zend_object *object)
{
    HashTable *map = SWOW_COROUTINE_G(map);

    ZEND_ASSERT(EG(flags) & EG_FLAGS_OBJECT_STORE_NO_REUSE);
    ZEND_ASSERT(EG(objects_store).top > 1);

    ZEND_HASH_FOREACH_VAL(map, zval *zscoroutine) {
        zend_object *object = Z_OBJ_P(zscoroutine);
        if (IS_OBJ_VALID(object)) {
            if (!(OBJ_FLAGS(object) & IS_OBJ_DESTRUCTOR_CALLED)) {
                GC_ADD_FLAGS(object, IS_OBJ_DESTRUCTOR_CALLED);
                if (object->handlers->dtor_obj != zend_objects_destroy_object ||
                    object->ce->destructor) {
                    GC_ADDREF(object);
                    object->handlers->dtor_obj(object);
                    GC_DELREF(object);
                }
            }
        }
    } ZEND_HASH_FOREACH_END();
}

SWOW_API swow_coroutine_t *swow_coroutine_create(zval *zcallable)
{
    return swow_coroutine_create_ex(NULL, zcallable, 0, 0);
}

SWOW_API swow_coroutine_t *swow_coroutine_create_ex(zend_class_entry *ce, zval *zcallable, size_t stack_page_size, size_t c_stack_size)
{
    swow_coroutine_t *scoroutine;

    scoroutine = swow_coroutine_get_from_object(
        swow_object_create(ce == NULL ? swow_coroutine_ce : ce)
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

static zend_always_inline void swow_coroutine_relink_executor_linkedlist_node(swow_coroutine_t *scoroutine, swow_coroutine_executor_t *executor)
{
    swow_coroutine_t *previous_scoroutine = swow_coroutine_get_previous(scoroutine);
    if (previous_scoroutine != NULL && previous_scoroutine->executor != NULL) {
        executor->root_execute_data->prev_execute_data = previous_scoroutine->executor->current_execute_data;
    } else {
        executor->root_execute_data->prev_execute_data = NULL;
    }
}

SWOW_API void swow_coroutine_switch_executor(swow_coroutine_t *current_scoroutine, swow_coroutine_t *target_scoroutine)
{
    swow_coroutine_executor_t *current_executor = current_scoroutine->executor;
    swow_coroutine_executor_t *target_executor = target_scoroutine->executor;
    // TODO: it's not optimal
    if (current_executor != NULL) {
        swow_coroutine_executor_save(current_executor);
        swow_coroutine_relink_executor_linkedlist_node(current_scoroutine, current_executor);
    }
    if (target_executor != NULL) {
        swow_coroutine_executor_recover(target_executor);
        swow_coroutine_relink_executor_linkedlist_node(target_scoroutine, target_executor);
    } else {
        zend_executor_globals *eg = SWOW_GLOBALS_FAST_PTR(executor_globals);
        eg->current_execute_data = NULL; /* make the log stack trace empty */
        eg->exception = NULL;            /* or maybe thrown in zend_error() (TODO: confirm it) */
    }
}

SWOW_API void swow_coroutine_executor_save(swow_coroutine_executor_t *executor)
{
    zend_executor_globals *eg = SWOW_GLOBALS_FAST_PTR(executor_globals);
    executor->bailout = eg->bailout;
    executor->vm_stack_top = eg->vm_stack_top;
    executor->vm_stack_end = eg->vm_stack_end;
    executor->vm_stack = eg->vm_stack;
    executor->vm_stack_page_size = eg->vm_stack_page_size;
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
        swow_fcall_info_t *fcall = (swow_fcall_info_t *) &BG(array_walk_fci);
        if (UNEXPECTED(fcall->info.size != 0)) {
            if (UNEXPECTED(executor->array_walk_context == NULL)) {
                executor->array_walk_context = (swow_fcall_info_t *) emalloc(sizeof(*fcall));
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
            swow_output_globals_init();
        }
    } while (0);
#endif
#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
    do {
        if (UNEXPECTED(executor->error_reporting != 0)) {
            ZEND_ASSERT(executor->error_reporting & E_MAGIC);
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
    eg->vm_stack_page_size = executor->vm_stack_page_size;
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
        swow_fcall_info_t *fcall = executor->array_walk_context;
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
                SWOW_OUTPUT_GLOBALS_MODIFY_START() {
                    memcpy(SWOW_GLOBALS_PTR(output_globals), og, sizeof(zend_output_globals));
                    og->handlers.elements = NULL; /* clear state */
                } SWOW_OUTPUT_GLOBALS_MODIFY_END();
            }
        }
    } while (0);
#endif
#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
    do {
        if (UNEXPECTED(executor->error_reporting != 0)) {
            ZEND_ASSERT(executor->error_reporting & E_MAGIC);
            int error_reporting = eg->error_reporting;
            eg->error_reporting = executor->error_reporting ^ E_MAGIC;
            executor->error_reporting = error_reporting | E_MAGIC;
        }
    } while (0);
#endif
}

static void swow_coroutine_handle_not_null_zdata(
    swow_coroutine_t *scoroutine, swow_coroutine_t *current_scoroutine, zval **zdata_ptr)
{
    if (!(current_scoroutine->coroutine.flags & SWOW_COROUTINE_FLAG_ACCEPT_ZDATA)) {
        if (UNEXPECTED(scoroutine->coroutine.flags & SWOW_COROUTINE_FLAG_ACCEPT_ZDATA)) {
            CAT_CORE_ERROR(COROUTINE, "Internal logic error: sent unrecognized data to PHP layer");
        } else {
            /* internal raw data to internal operation coroutine */
        }
    } else {
        zval *zdata = *zdata_ptr;
        zend_bool handle_ref = current_scoroutine->coroutine.state == CAT_COROUTINE_STATE_DEAD;
        if (!(scoroutine->coroutine.flags & SWOW_COROUTINE_FLAG_ACCEPT_ZDATA)) {
            ZEND_ASSERT(Z_TYPE_P(zdata) != IS_PTR);
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

#ifdef SWOW_COROUTINE_MOCK_FIBER_CONTEXT

static zend_always_inline zend_fiber_status swow_coroutine_get_fiber_status(const swow_coroutine_t *scoroutine)
{
    switch (scoroutine->coroutine.state) {
        case CAT_COROUTINE_STATE_WAITING:
            if (EXPECTED(scoroutine->coroutine.start_time > 0)) {
                return ZEND_FIBER_STATUS_SUSPENDED;
            } else {
                return ZEND_FIBER_STATUS_INIT;
            }
        case CAT_COROUTINE_STATE_RUNNING:
            return ZEND_FIBER_STATUS_RUNNING;
        case CAT_COROUTINE_STATE_DEAD:
            return ZEND_FIBER_STATUS_DEAD;
        case CAT_COROUTINE_STATE_NONE:
        default:
            CAT_NEVER_HERE("Unexpected state");
            // make compiler happy
            return ZEND_FIBER_STATUS_DEAD;
    }
}

static zend_never_inline void swow_coroutine_fiber_context_switch_notify(swow_coroutine_t *from, swow_coroutine_t *to)
{
    zend_fiber_context *from_context = from->fiber_context;
    zend_fiber_context *to_context = to->fiber_context;

    from_context->status = swow_coroutine_get_fiber_status(from);
    to_context->status = swow_coroutine_get_fiber_status(to);

    if (to_context->status == ZEND_FIBER_STATUS_INIT) {
        zend_observer_fiber_init_notify(to_context);
    } else if (from_context->status == ZEND_FIBER_STATUS_DEAD) {
        zend_observer_fiber_destroy_notify(from_context);
    }
    zend_observer_fiber_switch_notify(from_context, to_context);
}

static zend_always_inline void swow_coroutine_fiber_context_switch_try_notify(swow_coroutine_t *from, swow_coroutine_t *to)
{
    if (EXPECTED(!SWOW_NTS_G(has_debug_extension))) {
        return;
    }
    if (UNEXPECTED(SWOW_G(runtime_state) != SWOW_RUNTIME_STATE_RUNNING)) {
        return;
    }

    swow_coroutine_fiber_context_switch_notify(from, to);
}
#endif /* SWOW_COROUTINE_MOCK_FIBER_CONTEXT */

static void swow_coroutine_jump_standard(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval)
{
    swow_coroutine_t *current_scoroutine = swow_coroutine_get_current();
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_handle(coroutine);
    zval *zdata = (zval *) data;

#ifdef SWOW_COROUTINE_MOCK_FIBER_CONTEXT
    swow_coroutine_fiber_context_switch_try_notify(current_scoroutine, scoroutine);
#endif

    /* switch executor */
    swow_coroutine_switch_executor(current_scoroutine, scoroutine);

    /* solve transfer zval data */
    if (UNEXPECTED(zdata != NULL)) {
        swow_coroutine_handle_not_null_zdata(scoroutine, swow_coroutine_get_current(), &zdata);
    }

    /* resume C coroutine */
    cat_coroutine_jump_standard(&scoroutine->coroutine, zdata, retval);

    /* get from scoroutine */
    scoroutine = swow_coroutine_get_from(current_scoroutine);

    if (UNEXPECTED(scoroutine->coroutine.state == CAT_COROUTINE_STATE_DEAD)) {
        swow_coroutine_shutdown(scoroutine);
        /* delete it from global map
         * (we can not delete it in coroutine_function, object maybe released during deletion) */
        swow_coroutine_remove_from_map(scoroutine, SWOW_COROUTINE_G(map));
    } else {
        swow_coroutine_executor_t *executor = current_scoroutine->executor;
        if (executor != NULL) {
            /* handle cross exception */
            if (UNEXPECTED(SWOW_COROUTINE_G(exception) != NULL)) {
                swow_coroutine_handle_cross_exception(SWOW_COROUTINE_G(exception));
                SWOW_COROUTINE_G(exception) = NULL;
            }
        }
    }
}

static zend_always_inline void swow_coroutine_jump_handle_retval(zval *zdata, zval *retval)
{
    if (UNEXPECTED(zdata != NULL)) {
        if (retval == NULL) {
            zval_ptr_dtor(zdata);
        } else {
            ZVAL_COPY_VALUE(retval, zdata);
        }
    }
}

SWOW_API cat_bool_t swow_coroutine_resume(swow_coroutine_t *scoroutine, zval *zdata, zval *retval)
{
    cat_coroutine_t *current_coroutine = CAT_COROUTINE_G(current);
    cat_bool_t ret;

    if (retval != NULL) {
        ZVAL_NULL(retval);
    }

    current_coroutine->flags |= SWOW_COROUTINE_FLAG_ACCEPT_ZDATA;

    ret = cat_coroutine_resume(&scoroutine->coroutine, zdata, (cat_data_t **) &zdata);

    current_coroutine->flags ^= SWOW_COROUTINE_FLAG_ACCEPT_ZDATA;

    if (UNEXPECTED(!ret)) {
        return cat_false;
    }

    swow_coroutine_jump_handle_retval(zdata, retval);

    return cat_true;
}

SWOW_API cat_bool_t swow_coroutine_yield(zval *zdata, zval *retval)
{
    cat_coroutine_t *current_coroutine = CAT_COROUTINE_G(current);
    cat_bool_t ret;

    if (retval != NULL) {
        ZVAL_NULL(retval);
    }

    current_coroutine->flags |= SWOW_COROUTINE_FLAG_ACCEPT_ZDATA;

    ret = cat_coroutine_yield(zdata, (cat_data_t **) &zdata);

    current_coroutine->flags ^= SWOW_COROUTINE_FLAG_ACCEPT_ZDATA;

    if (UNEXPECTED(!ret)) {
        return cat_false;
    }

    swow_coroutine_jump_handle_retval(zdata, retval);

    return cat_true;
}

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

static zend_always_inline zend_execute_data *swow_coroutine_execute_scope_start(const swow_coroutine_t *scoroutine, zend_long level)
{
    zend_execute_data *current_execute_data_backup = EG(current_execute_data);

    if (EXPECTED(scoroutine != swow_coroutine_get_current())) {
        EG(current_execute_data) = scoroutine->executor->current_execute_data;
    }
    EG(current_execute_data) = swow_debug_backtrace_resolve(EG(current_execute_data), level);

    return current_execute_data_backup;
}

static zend_always_inline void swow_coroutine_execute_scope_end(zend_execute_data *current_execute_data_backup)
{
    EG(current_execute_data) = current_execute_data_backup;
}

#define SWOW_COROUTINE_EXECUTE_START(scoroutine, level) do { \
    zend_execute_data *current_execute_data_backup; \
    current_execute_data_backup = swow_coroutine_execute_scope_start(scoroutine, level); \

#define SWOW_COROUTINE_EXECUTE_END() \
    swow_coroutine_execute_scope_end(current_execute_data_backup); \
} while (0)

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
        zend_execute_data *ex = EG(current_execute_data);
        if (ex->func->common.function_name == NULL) {
            zend_execute_data *prev = ex->prev_execute_data;
            uint32_t include_kind;
            _prev_frame:
            include_kind = 0;
            if (prev && prev->func && ZEND_USER_CODE(prev->func->common.type) && prev->opline->opcode == ZEND_INCLUDE_OR_EVAL) {
                include_kind = prev->opline->extended_value;
            }
            switch (include_kind) {
                case ZEND_EVAL:
                    function_name = ZSTR_KNOWN(ZEND_STR_EVAL);
                    break;
                case ZEND_INCLUDE:
                    function_name = ZSTR_KNOWN(ZEND_STR_INCLUDE);
                    break;
                case ZEND_REQUIRE:
                    function_name = ZSTR_KNOWN(ZEND_STR_REQUIRE);
                    break;
                case ZEND_INCLUDE_ONCE:
                    function_name = ZSTR_KNOWN(ZEND_STR_INCLUDE_ONCE);
                    break;
                case ZEND_REQUIRE_ONCE:
                    function_name = ZSTR_KNOWN(ZEND_STR_REQUIRE_ONCE);
                    break;
                default:
                    /* Skip dummy frame unless it is needed to preserve filename/lineno info. */
                    if (prev && !swow_debug_is_user_call(prev)) {
                        prev = prev->prev_execute_data;
                        goto _prev_frame;
                    }
                    function_name = ZSTR_KNOWN(ZEND_STR_UNKNOWN);
                    break;
            }
        } else {
           function_name = get_function_or_method_name(ex->func);
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

SWOW_API zend_long swow_coroutine_get_trace_depth(const swow_coroutine_t *scoroutine, zend_long limit)
{
    zend_long depth;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_false, return -1);

    SWOW_COROUTINE_EXECUTE_START(scoroutine, 0) {
        zend_execute_data *root_execute_data = scoroutine->executor->root_execute_data;
        zend_execute_data *root_previous_execute_data = root_execute_data->prev_execute_data;
        root_execute_data->prev_execute_data = NULL;
        depth = swow_debug_backtrace_depth(EG(current_execute_data), limit);
        root_execute_data->prev_execute_data = root_previous_execute_data;
    } SWOW_COROUTINE_EXECUTE_END();

    return depth;
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

    SWOW_COROUTINE_EXECUTE_START(scoroutine, level) {
        SWOW_COROUTINE_CHECK_CALL_INFO(goto _error);

        symbol_table = zend_rebuild_symbol_table();

        if (EXPECTED(symbol_table != NULL)) {
            symbol_table = zend_array_dup(symbol_table);
        }

        if (0) {
            _error:
            symbol_table = NULL;
        }
    } SWOW_COROUTINE_EXECUTE_END();

    return symbol_table;
}

SWOW_API cat_bool_t swow_coroutine_set_local_var(swow_coroutine_t *scoroutine, zend_string *name, zval *value, zend_long level, zend_bool force)
{
    cat_bool_t ret;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_true, return cat_false);

    SWOW_COROUTINE_EXECUTE_START(scoroutine, level) {
        int error;

        SWOW_COROUTINE_CHECK_CALL_INFO(goto _error);
        Z_TRY_ADDREF_P(value);

        error = zend_set_local_var(name, value, force);

        if (UNEXPECTED(error != SUCCESS)) {
            Z_TRY_DELREF_P(value);
            cat_update_last_error(CAT_EINVAL, "Set var '%.*s' failed by unknown reason", (int) ZSTR_LEN(name), ZSTR_VAL(name));
            _error:
            ret = cat_false;
        } else {
            ret = cat_true;
        }
    } SWOW_COROUTINE_EXECUTE_END();

    return ret;
}

#undef SWOW_COROUTINE_CHECK_CALL_INFO

SWOW_API cat_bool_t swow_coroutine_eval(swow_coroutine_t *scoroutine, zend_string *string, zend_long level, zval *return_value)
{
    int error;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_true, return cat_false);

    if (scoroutine != swow_coroutine_get_current() || level != 0) {
        cat_coroutine_switch_deny();
    }

    SWOW_COROUTINE_EXECUTE_START(scoroutine, level) {
        error = zend_eval_stringl(ZSTR_VAL(string), ZSTR_LEN(string), return_value, (char *) "Coroutine::eval()");
    } SWOW_COROUTINE_EXECUTE_END();

    cat_coroutine_switch_allow();

    if (UNEXPECTED(error != SUCCESS)) {
        cat_update_last_error(CAT_UNKNOWN, "Eval failed by unknown reason");
        return cat_false;
    }

    return cat_true;
}

SWOW_API cat_bool_t swow_coroutine_call(swow_coroutine_t *scoroutine, zval *zcallable, zend_long level, zval *return_value)
{
    zend_fcall_info fci;
    swow_fcall_storage_t fcall;
    int error;

    SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_true, return cat_false);

    if (!swow_fcall_storage_create(&fcall, zcallable)) {
        cat_update_last_error_with_previous("Coroutine::call() failed");
        return cat_false;
    }

    fci.size = sizeof(fci);
    ZVAL_UNDEF(&fci.function_name);
    fci.object = NULL;
    fci.param_count = 0;
    fci.named_params = NULL;
    fci.retval = return_value;

    if (scoroutine != swow_coroutine_get_current()) {
        cat_coroutine_switch_deny();
    }

    SWOW_COROUTINE_EXECUTE_START(scoroutine, level) {
        error = zend_call_function(&fci, &fcall.fcc);
    } SWOW_COROUTINE_EXECUTE_END();

    cat_coroutine_switch_allow();

    swow_fcall_storage_release(&fcall);

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

SWOW_API void swow_coroutine_dump_current(void)
{
    zval zscoroutine;
    ZVAL_OBJ(&zscoroutine, &swow_coroutine_get_current()->std);
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

SWOW_API void swow_coroutine_dump_all_to_file(const char *filename)
{
    zend_string *output;

    SWOW_OB_START(output) {
        swow_coroutine_dump_all();
    } SWOW_OB_END();

    if (output == NULL) {
        return;
    }

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        return;
    }
    (void) fwrite(ZSTR_VAL(output), sizeof(char), ZSTR_LEN(output), file);
    (void) fclose(file);
}

/* exception */

static ZEND_COLD void swow_coroutine_handle_cross_exception(zend_object *exception)
{
    if (SWOW_COROUTINE_IS_UNWIND_EXIT_MAGIC(exception)) {
        swow_coroutine_throw_unwind_exit();
    } else {
        /* for throw method success */
        GC_ADDREF(exception);
        zend_throw_exception_internal(exception);
    }
}

SWOW_API cat_bool_t swow_coroutine_throw(swow_coroutine_t *scoroutine, zend_object *exception, zval *retval)
{
    if (!SWOW_COROUTINE_IS_UNWIND_EXIT_MAGIC(exception) &&
        UNEXPECTED(!instanceof_function(exception->ce, zend_ce_throwable))) {
        cat_update_last_error(CAT_EMISUSE, "Instance of %s is not throwable", ZSTR_VAL(exception->ce->name));
        return cat_false;
    }

    if (UNEXPECTED(scoroutine == swow_coroutine_get_current())) {
        if (SWOW_COROUTINE_IS_UNWIND_EXIT_MAGIC(exception)) {
            swow_coroutine_throw_unwind_exit();
        } else {
            GC_ADDREF(exception);
            zend_throw_exception_internal(exception);
        }
        ZVAL_NULL(retval);
    } else {
        SWOW_COROUTINE_SHOULD_BE_IN_EXECUTING(scoroutine, cat_true, return cat_false);
        // TODO: split check & resume
        SWOW_COROUTINE_G(exception) = exception;
        if (UNEXPECTED(!swow_coroutine_resume(scoroutine, NULL, retval))) {
            SWOW_COROUTINE_G(exception) = NULL;
            return cat_false;
        }
    }

    return cat_true;
}

static ZEND_COLD void swow_coroutine_throw_unwind_exit(void)
{
    swow_coroutine_get_current()->coroutine.flags |= SWOW_COROUTINE_FLAG_KILLED;
#ifndef SWOW_NATIVE_UNWIND_EXIT_SUPPORT
    zend_throw_exception(swow_coroutine_unwind_exit_ce, NULL, 0);
#else
    zend_throw_unwind_exit();
#endif
}

SWOW_API cat_bool_t swow_coroutine_kill(swow_coroutine_t *scoroutine)
{
    while (1) {
        zval retval;
        cat_bool_t success = swow_coroutine_throw(scoroutine, SWOW_COROUTINE_UNWIND_EXIT_MAGIC, &retval);
        if (!success) {
            cat_update_last_error_with_previous("Kill coroutine " CAT_COROUTINE_ID_FMT " failed", scoroutine->coroutine.id);
            return cat_false;
        }
        zval_ptr_dtor(&retval); // TODO: __destruct may lead coroutine switch
        if (UNEXPECTED(swow_coroutine_is_alive(scoroutine))) {
            if (scoroutine == swow_coroutine_get_current()) {
                break;
            }
            continue;
        }
        break;
    }

    return cat_true;
}

SWOW_API cat_bool_t swow_coroutine_kill_all(void)
{
    HashTable *map = SWOW_COROUTINE_G(map);
    cat_bool_t try_again, ret = cat_true;

    do {
        try_again = cat_false;
        ZEND_HASH_FOREACH_VAL(map, zval *zscoroutine) {
            swow_coroutine_t *scoroutine = swow_coroutine_get_from_object(Z_OBJ_P(zscoroutine));
            if (scoroutine == swow_coroutine_get_current()) {
                continue;
            }
            if (scoroutine == swow_coroutine_get_main()) {
                continue;
            }
            if (unlikely(!swow_coroutine_kill(scoroutine))) {
                CAT_WARN_WITH_LAST(COROUTINE, "Error occurred while killing all coroutines");
                ret = cat_false;
            }
            try_again = ret;
        } ZEND_HASH_FOREACH_END();
    } while (try_again);

    return ret;
}

#define getThisCoroutine() (swow_coroutine_get_from_object(Z_OBJ_P(ZEND_THIS)))

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Coroutine___construct, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, callable, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, __construct)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();
    swow_fcall_storage_t fcall;
    zval zfcall;

    /* create_object() may fail */
    if (UNEXPECTED(EG(exception))) {
        RETURN_THROWS();
    }

    if (UNEXPECTED(scoroutine->coroutine.state != CAT_COROUTINE_STATE_NONE)) {
        zend_throw_error(NULL, "%s can be constructed only once", ZEND_THIS_NAME);
        RETURN_THROWS();
    }

    ZEND_PARSE_PARAMETERS_START(1, 1)
        SWOW_PARAM_FCALL(fcall)
    ZEND_PARSE_PARAMETERS_END();

    ZVAL_PTR(&zfcall, &fcall);
    if (UNEXPECTED(!swow_coroutine_construct(scoroutine, &zfcall, 0, 0))) {
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
        if (UNEXPECTED(scoroutine->coroutine.start_time > 0)) { \
            zend_throw_error(NULL, "Only one argument allowed when resuming an coroutine that has been run"); \
            RETURN_THROWS(); \
        } \
        zdata = &_##zdata; \
        ZVAL_PTR(zdata, &fci); \
    }

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_run, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, callable, IS_CALLABLE, 0)
    ZEND_ARG_VARIADIC_TYPE_INFO(0, data, IS_MIXED, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, run)
{
    swow_fcall_storage_t fcall;
    zval zfcall;
    SWOW_COROUTINE_DECLARE_RESUME_TRANSFER(zdata);

    ZEND_PARSE_PARAMETERS_START(1, -1)
        SWOW_PARAM_FCALL(fcall)
        Z_PARAM_VARIADIC('*', fci.params, fci.param_count)
    ZEND_PARSE_PARAMETERS_END();

    ZVAL_PTR(&zfcall, &fcall);
    swow_coroutine_t *scoroutine = swow_coroutine_create_ex(zend_get_called_scope(execute_data), &zfcall, 0, 0);
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_resume, 0, 0, IS_MIXED, 0)
    ZEND_ARG_VARIADIC_TYPE_INFO(0, data, IS_MIXED, 0)
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_yield, 0, 0, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, data, IS_MIXED, 0, "null")
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getId, 0, 0, IS_LONG, 0)
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

static PHP_METHOD_EX(Swow_Coroutine, getCoroutine, swow_coroutine_t *scoroutine)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(scoroutine == NULL)) {
        RETURN_NULL();
    }

    GC_ADDREF(&scoroutine->std);
    RETURN_OBJ(&scoroutine->std);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_TYPE_MASK_EX(arginfo_class_Swow_Coroutine_getCurrent, 0, 0, Swow\\Coroutine, MAY_BE_STATIC)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getCurrent)
{
    PHP_METHOD_CALL(Swow_Coroutine, getCoroutine, swow_coroutine_get_current());
}

#define arginfo_class_Swow_Coroutine_getMain arginfo_class_Swow_Coroutine_getCurrent

static PHP_METHOD(Swow_Coroutine, getMain)
{
    PHP_METHOD_CALL(Swow_Coroutine, getCoroutine, swow_coroutine_get_main());
}

#define arginfo_class_Swow_Coroutine_getPrevious arginfo_class_Swow_Coroutine_getCurrent

static PHP_METHOD(Swow_Coroutine, getPrevious)
{
    PHP_METHOD_CALL(Swow_Coroutine, getCoroutine, swow_coroutine_get_previous(getThisCoroutine()));
}

#undef SWOW_COROUTINE_GETTER

#define arginfo_class_Swow_Coroutine_getState arginfo_class_Swow_Coroutine_getId

static PHP_METHOD(Swow_Coroutine, getState)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(getThisCoroutine()->coroutine.state);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getStateName, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getStateName)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(cat_coroutine_get_state_name(&getThisCoroutine()->coroutine));
}

#define arginfo_class_Swow_Coroutine_getRound arginfo_class_Swow_Coroutine_getId

static PHP_METHOD(Swow_Coroutine, getRound)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(getThisCoroutine()->coroutine.round);
}

#define arginfo_class_Swow_Coroutine_getCurrentRound arginfo_class_Swow_Coroutine_getId

static PHP_METHOD(Swow_Coroutine, getCurrentRound)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(CAT_COROUTINE_G(round));
}

#define arginfo_class_Swow_Coroutine_getElapsed arginfo_class_Swow_Coroutine_getId

static PHP_METHOD(Swow_Coroutine, getElapsed)
{
    cat_msec_t elapsed;

    ZEND_PARSE_PARAMETERS_NONE();

    elapsed = cat_coroutine_get_elapsed(&getThisCoroutine()->coroutine);

    if (elapsed > ZEND_LONG_MAX) {
        RETURN_STR(zend_ulong_to_str(elapsed));
    }

    RETURN_LONG(elapsed);
}

#define arginfo_class_Swow_Coroutine_getElapsedAsString arginfo_class_Swow_Coroutine_getStateName

static PHP_METHOD(Swow_Coroutine, getElapsedAsString)
{
    char *elapsed;

    ZEND_PARSE_PARAMETERS_NONE();

    elapsed = cat_coroutine_get_elapsed_as_string(&getThisCoroutine()->coroutine);

    RETVAL_STRING(elapsed);

    cat_free(elapsed);
}

#define arginfo_class_Swow_Coroutine_getExitStatus arginfo_class_Swow_Coroutine_getId

static PHP_METHOD(Swow_Coroutine, getExitStatus)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(scoroutine == swow_coroutine_get_main())) {
        RETURN_LONG(EG(exit_status));
    }

    RETURN_LONG(scoroutine->exit_status);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_isAvailable, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, isAvailable)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(swow_coroutine_is_available(getThisCoroutine()));
}

#define arginfo_class_Swow_Coroutine_isAlive arginfo_class_Swow_Coroutine_isAvailable

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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getExecutedFilename, 0, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, level, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getExecutedFilename)
{
    zend_string *filename;

    SWOW_COROUTINE_GET_EXECUTED_INFO_PARAMETERS_PARSER();

    filename = swow_coroutine_get_executed_filename(getThisCoroutine(), level);

    RETURN_STR(filename);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getExecutedLineno, 0, 0, IS_LONG, 0)
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getTrace, 0, 0, IS_ARRAY, 0)
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getTraceAsString, 0, 0, IS_STRING, 0)
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getTraceDepth, 0, 0, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, limit, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getTraceDepth)
{
    zend_long level = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(level)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(swow_coroutine_get_trace_depth(getThisCoroutine(), level));
}

#undef SWOW_COROUTINE_GET_TRACE_PARAMETERS_PARSER

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getDefinedVars, 0, 0, IS_ARRAY, 0)
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

    symbol_table = swow_coroutine_get_defined_vars(scoroutine, level);

    if (UNEXPECTED(symbol_table == NULL)) {
        RETURN_EMPTY_ARRAY();
    }

    RETURN_ARR(symbol_table);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_setLocalVar, 0, 2, IS_STATIC, 0)
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

    ret = swow_coroutine_set_local_var(scoroutine, name, value, level, force);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_eval, 0, 1, IS_MIXED, 0)
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_call, 0, 1, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO(0, callable, IS_CALLABLE, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, level, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, call)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();
    swow_fcall_storage_t fcall;
    zval zfcall;
    zend_long level = 0;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        SWOW_PARAM_FCALL(fcall)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(level)
    ZEND_PARSE_PARAMETERS_END();

    ZVAL_PTR(&zfcall, &fcall);
    ret = swow_coroutine_call(scoroutine, &zfcall, level, return_value);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_throw, 0, 1, IS_MIXED, 0)
    ZEND_ARG_OBJ_INFO(0, throwable, Throwable, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, throw)
{
    zval *zexception;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(zexception, zend_ce_throwable)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(!swow_coroutine_throw(getThisCoroutine(), Z_OBJ_P(zexception), return_value))) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_kill, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, kill)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(!swow_coroutine_kill(getThisCoroutine()))) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

#define arginfo_class_Swow_Coroutine_killAll arginfo_class_Swow_Coroutine_kill

static PHP_METHOD(Swow_Coroutine, killAll)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(!swow_coroutine_kill_all())) {
        swow_throw_exception_with_last(swow_coroutine_exception_ce);
        RETURN_THROWS();
    }
}

#undef SWOW_COROUTINE_MESSAGE_AND_CODE_PARAMETERS_PARSER

#define arginfo_class_Swow_Coroutine_count arginfo_class_Swow_Coroutine_getId

static PHP_METHOD(Swow_Coroutine, count)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(zend_hash_num_elements(SWOW_COROUTINE_G(map)));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_get, 0, 1, IS_STATIC, 1)
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Coroutine_getAll, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, getAll)
{
    HashTable *map = SWOW_COROUTINE_G(map);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_ARR(zend_array_dup(map));
}

#define arginfo_class_Swow_Coroutine___debugInfo arginfo_class_Swow_Coroutine_getAll

static PHP_METHOD(Swow_Coroutine, __debugInfo)
{
    swow_coroutine_t *scoroutine = getThisCoroutine();
    cat_coroutine_t *coroutine = &scoroutine->coroutine;
    zval zdebug_info;
    char *tmp;
    const char *const_tmp;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(&zdebug_info);
    add_assoc_long(&zdebug_info, "id", coroutine->id);
    const_tmp = cat_coroutine_get_role_name(coroutine);
    if (const_tmp != NULL) {
        add_assoc_string(&zdebug_info, "role", const_tmp);
    }
    add_assoc_string(&zdebug_info, "state", cat_coroutine_get_state_name(coroutine));
    tmp = cat_coroutine_get_elapsed_as_string(coroutine);
    add_assoc_long(&zdebug_info, "round", coroutine->round);
    add_assoc_string(&zdebug_info, "elapsed", tmp);
    cat_free(tmp);
    if (swow_coroutine_is_executing(scoroutine)) {
        const zend_long level = 0;
        const zend_long limit = 0;
        const zend_long options = DEBUG_BACKTRACE_PROVIDE_OBJECT;
        smart_str str = { 0 };
        smart_str_appendc(&str, '\n');
        swow_coroutine_get_trace_as_smart_str(scoroutine, &str, level, limit, options);
        if (ZSTR_LEN(str.s) == CAT_STRLEN("\n")) {
            smart_str_free(&str);
        } else {
            smart_str_appendc(&str, '\n');
            smart_str_0(&str);
            add_assoc_str(&zdebug_info, "trace", str.s);
        }
    }

    RETURN_DEBUG_INFO_WITH_PROPERTIES(&zdebug_info);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Coroutine_registerDeadlockHandler, 0, 1, Swow\x5cUtil\\Handler, 0)
    ZEND_ARG_TYPE_INFO(0, callable, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Coroutine, registerDeadlockHandler)
{
    swow_util_handler_t *handler;
    swow_fcall_storage_t fcall;
    zval zfcall;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        SWOW_PARAM_FCALL(fcall)
    ZEND_PARSE_PARAMETERS_END();

    ZVAL_PTR(&zfcall, &fcall);
    handler = swow_util_handler_create(&zfcall);
    ZEND_ASSERT(handler != NULL);

    swow_util_handler_push_back_to(handler, &SWOW_COROUTINE_G(deadlock_handlers));

    RETURN_OBJ_COPY(&handler->std);
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
    PHP_ME(Swow_Coroutine, getCurrentRound,         arginfo_class_Swow_Coroutine_getCurrentRound,         ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
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
    PHP_ME(Swow_Coroutine, getTraceDepth,           arginfo_class_Swow_Coroutine_getTraceDepth,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, getDefinedVars,          arginfo_class_Swow_Coroutine_getDefinedVars,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, setLocalVar,             arginfo_class_Swow_Coroutine_setLocalVar,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, eval,                    arginfo_class_Swow_Coroutine_eval,                    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, call,                    arginfo_class_Swow_Coroutine_call,                    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, throw,                   arginfo_class_Swow_Coroutine_throw,                   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, kill,                    arginfo_class_Swow_Coroutine_kill,                    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Coroutine, killAll,                 arginfo_class_Swow_Coroutine_killAll,                 ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Coroutine, count,                   arginfo_class_Swow_Coroutine_count,                   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Coroutine, get,                     arginfo_class_Swow_Coroutine_get,                     ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Coroutine, getAll,                  arginfo_class_Swow_Coroutine_getAll,                  ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    /* magic */
    PHP_ME(Swow_Coroutine, __debugInfo,             arginfo_class_Swow_Coroutine___debugInfo,             ZEND_ACC_PUBLIC)
    /* debug */
    PHP_ME(Swow_Coroutine, registerDeadlockHandler, arginfo_class_Swow_Coroutine_registerDeadlockHandler, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

/* handlers */

static HashTable *swow_coroutine_get_gc(zend_object *object, zval **gc_data, int *gc_count)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_object(object);
    zval *zcallable = scoroutine->executor ? &scoroutine->executor->fcall.zcallable : NULL;

    if (zcallable == NULL || ZVAL_IS_NULL(zcallable)) {
        *gc_data = NULL;
        *gc_count = 0;
    } else {
        *gc_data = zcallable;
        *gc_count = 1;
    }

    return zend_std_get_properties(object);
}

/* hack error hook */

#if PHP_VERSION_ID < 80100
# define ZEND_ERROR_CB_FILENAME_T const char
#else
# define ZEND_ERROR_CB_FILENAME_T zend_string
#endif

/* we should call original error_cb when we are not in runtime */
#define SWOW_COROUTINE_ERROR_CB_CHECK() \
    if (SWOW_COROUTINE_G(runtime_state) == SWOW_COROUTINE_RUNTIME_STATE_NONE) { \
        swow_call_original_zend_error_cb(type, error_filename, error_lineno, message); \
        return; \
    }

typedef void (*swow_error_cb_t)(int type, ZEND_ERROR_CB_FILENAME_T *
, const uint32_t error_lineno, zend_string *message);

static swow_error_cb_t original_zend_error_cb;

static void swow_call_original_zend_error_cb(int type, ZEND_ERROR_CB_FILENAME_T *error_filename, const uint32_t error_lineno, zend_string *message)
{
    original_zend_error_cb(type, error_filename, error_lineno, message);
}

static void swow_call_original_zend_error_cb_safe(int type, ZEND_ERROR_CB_FILENAME_T *error_filename, const uint32_t error_lineno, zend_string *message)
{
    zend_try {
        original_zend_error_cb(type, error_filename, error_lineno, message);
    } zend_catch {
        // TODO: kill all coroutines?
        exit(EG(exit_status));
    } zend_end_try();
}

static void swow_coroutine_error_cb(int type, ZEND_ERROR_CB_FILENAME_T *error_filename, const uint32_t error_lineno, zend_string *message)
{
    SWOW_COROUTINE_ERROR_CB_CHECK();
    swow_coroutine_t *current_scoroutine = swow_coroutine_get_current();
    swow_coroutine_t *main_scoroutine = swow_coroutine_get_main();
    const char *format = ZSTR_VAL(message);
    zend_bool is_uncaught_exception = !!(type & E_DONT_BAIL) && !(type & (E_PARSE | E_COMPILE_ERROR));
    zend_string *new_message = NULL;

    /* keep silent for kill */
    if (is_uncaught_exception && current_scoroutine->coroutine.flags & SWOW_COROUTINE_FLAG_KILLED) {
        return;
    }

    if (current_scoroutine == main_scoroutine) {
        /* only main coroutine, keep error behaviour as normal */
        if (CAT_COROUTINE_G(count) == 1) {
            swow_call_original_zend_error_cb(type, error_filename, error_lineno, message);
            return;
        }
    }

    if (!SWOW_COROUTINE_G(classic_error_handler)) {
        const char *original_type_string = swow_strerrortype(type);
        zend_string *trace = NULL;
        if (is_uncaught_exception) {
            /* hack hook for error in main */
            if (current_scoroutine == main_scoroutine) {
                zend_long severity = SWOW_COROUTINE_G(exception_error_severity);
                if (severity == E_NONE) {
                    return;
                }
                type = severity;
                original_type_string = swow_strerrortype(type);
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
            const char *trace1 = trace != NULL ? "\nStack trace:\n" : "";
            const char *trace2 = trace != NULL ? ZSTR_VAL(trace) : "";
            const char *trace3 = trace != NULL ? "\n  triggered" : "";
            const char *name = cat_coroutine_get_current_role_name();
            if (name != NULL) {
                new_message = zend_strpprintf(0,
                    "[%s in %s] %s%s%s%s",
                    original_type_string, name, format,
                    trace1, trace2, trace3
                );
            } else {
                new_message = zend_strpprintf(0,
                    "[%s in R" CAT_COROUTINE_ID_FMT "] %s%s%s%s",
                    original_type_string, cat_coroutine_get_current_id(), format,
                    trace1, trace2, trace3
                );
            }
            message = new_message;
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
    swow_call_original_zend_error_cb_safe(type, error_filename, error_lineno, message);
    if (new_message != NULL) {
        zend_string_release(new_message);
    }
}

/* user opcode handler maybe called after runtime shutdown */
#define SWOW_COROUTINE_OPCODE_HANDLER_CHECK() \
    if (SWOW_COROUTINE_G(runtime_state) == SWOW_COROUTINE_RUNTIME_STATE_NONE) { \
        return ZEND_USER_OPCODE_DISPATCH; \
    }

/* hook catch */

static zend_always_inline zend_bool swow_coroutine_has_unwind_exit(zend_object *exception)
{
    if (zend_is_unwind_exit(exception)) {
        return true;
    }

#ifndef SWOW_NATIVE_UNWIND_EXIT_SUPPORT
    while (1) {
        zval *zprevious_exception, ztmp;
        if (exception->ce == swow_coroutine_unwind_exit_ce) {
            return 1;
        }
        zprevious_exception = zend_read_property_ex(
            zend_get_exception_base(exception),
            exception,
            ZSTR_KNOWN(ZEND_STR_PREVIOUS),
            1, &ztmp
        );
        if (Z_TYPE_P(zprevious_exception) == IS_OBJECT) {
            exception = Z_OBJ_P(zprevious_exception);
            continue;
        }
        break;
    }
#endif

    return false;
}

#ifndef SWOW_NATIVE_UNWIND_EXIT_SUPPORT
static int swow_coroutine_catch_handler(zend_execute_data *execute_data)
{
    SWOW_COROUTINE_OPCODE_HANDLER_CHECK();
    zend_exception_restore();
    if (UNEXPECTED(EG(exception) != NULL)) {
        if (swow_coroutine_has_unwind_exit(EG(exception))) {
            return ZEND_USER_OPCODE_RETURN;
        }
    }

    return ZEND_USER_OPCODE_DISPATCH;
}
#endif

/* hook exit */

static int swow_coroutine_exit_handler(zend_execute_data *execute_data)
{
    SWOW_COROUTINE_OPCODE_HANDLER_CHECK();
    swow_coroutine_t *scoroutine = swow_coroutine_get_current();

    if (scoroutine == swow_coroutine_get_main()) {
        return ZEND_USER_OPCODE_DISPATCH;
    }

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
    scoroutine->exit_status = status;
    if (EG(exception) == NULL) {
        zend_throw_unwind_exit();
    }

    return ZEND_USER_OPCODE_CONTINUE;
}

/* hook silence */

#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
static int swow_coroutine_begin_silence_handler(zend_execute_data *execute_data)
{
    SWOW_COROUTINE_OPCODE_HANDLER_CHECK();
    swow_coroutine_t *scoroutine = swow_coroutine_get_current();
    scoroutine->executor->error_reporting = EG(error_reporting) | E_MAGIC;
    return ZEND_USER_OPCODE_DISPATCH;
}

static int swow_coroutine_end_silence_handler(zend_execute_data *execute_data)
{
    SWOW_COROUTINE_OPCODE_HANDLER_CHECK();
    swow_coroutine_t *scoroutine = swow_coroutine_get_current();
    scoroutine->executor->error_reporting = 0;
    return ZEND_USER_OPCODE_DISPATCH;
}
#endif

/* deadlock callback */

static void swow_coroutine_deadlock_callback(void)
{
    CAT_QUEUE_FOREACH_DATA_START(&SWOW_COROUTINE_G(deadlock_handlers), swow_util_handler_t, node, handler) {
        swow_coroutine_t *scoroutine;
        zval zfcall;
        ZVAL_PTR(&zfcall, &handler->fcall);
        scoroutine = swow_coroutine_create(&zfcall);
        if (scoroutine == NULL) {
            goto _error;
        }
        if (!swow_coroutine_resume(scoroutine, NULL, NULL)) {
            goto _error;
        }
        swow_coroutine_close(scoroutine);
    } CAT_QUEUE_FOREACH_DATA_END();

    return;

    _error:
    CAT_WARN_WITH_LAST(COROUTINE, "Deadlock handler call failed");
}

zend_result swow_coroutine_module_init(INIT_FUNC_ARGS)
{
    if (!cat_coroutine_module_init()) {
        return FAILURE;
    }

    CAT_GLOBALS_REGISTER(swow_coroutine, CAT_GLOBALS_CTOR(swow_coroutine), NULL);

    SWOW_COROUTINE_G(runtime_state) = SWOW_COROUTINE_RUNTIME_STATE_NONE;

    swow_coroutine_ce = swow_register_internal_class(
        "Swow\\Coroutine", NULL, swow_coroutine_methods,
        &swow_coroutine_handlers, NULL,
        cat_false, cat_false,
        swow_coroutine_create_object,
        swow_coroutine_free_object,
        XtOffsetOf(swow_coroutine_t, std)
    );
    swow_coroutine_handlers.get_gc = swow_coroutine_get_gc;
    swow_coroutine_handlers.dtor_obj = swow_coroutine_dtor_object;
    swow_coroutine_main_handlers = swow_coroutine_handlers;
    swow_coroutine_main_handlers.dtor_obj = swow_coroutine_main_dtor_object;
    /* constants */
#define SWOW_COROUTINE_STATE_GEN(name, value, unused) \
    zend_declare_class_constant_long(swow_coroutine_ce, ZEND_STRL("STATE_" #name), (value));
    CAT_COROUTINE_STATE_MAP(SWOW_COROUTINE_STATE_GEN)
#undef SWOW_COROUTINE_STATE_GEN

    /* Exception for common errors */
    swow_coroutine_exception_ce = swow_register_internal_class(
        "Swow\\CoroutineException", swow_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

#ifndef SWOW_NATIVE_UNWIND_EXIT_SUPPORT
    /* implement UnwindExit by ourself (temporarily) */
    swow_coroutine_unwind_exit_ce = swow_register_internal_class(
        "Swow\\Coroutine\\UnwindExit", swow_coroutine_exception_ce, NULL, NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );
    swow_coroutine_unwind_exit_ce->ce_flags |= ZEND_ACC_FINAL;
#endif

    /* hook zend_error_cb (we should only do it in runtime) */
    original_zend_error_cb = zend_error_cb;
    zend_error_cb = swow_coroutine_error_cb;

    /* construct coroutine php internal function */
    memset(&swow_coroutine_internal_function, 0, sizeof(swow_coroutine_internal_function));
    swow_coroutine_internal_function.common.type = ZEND_INTERNAL_FUNCTION;

    /* hook opcode exit */
    zend_set_user_opcode_handler(ZEND_EXIT, swow_coroutine_exit_handler);
# ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
    /* hook opcode silence */
    zend_set_user_opcode_handler(ZEND_BEGIN_SILENCE, swow_coroutine_begin_silence_handler);
    zend_set_user_opcode_handler(ZEND_END_SILENCE, swow_coroutine_end_silence_handler);
# endif
# ifndef SWOW_NATIVE_UNWIND_EXIT_SUPPORT
    /* hook opcode catch */
    zend_set_user_opcode_handler(ZEND_CATCH, swow_coroutine_catch_handler);
# endif

    return SUCCESS;
}

zend_result swow_coroutine_runtime_init(INIT_FUNC_ARGS)
{
    if (!cat_coroutine_runtime_init()) {
        return FAILURE;
    }

    SWOW_COROUTINE_G(original_jump) = cat_coroutine_register_jump(
        swow_coroutine_jump_standard
    );

    SWOW_COROUTINE_G(default_stack_page_size) = SWOW_COROUTINE_DEFAULT_STACK_PAGE_SIZE; /* TODO: get php.ini */
    SWOW_COROUTINE_G(classic_error_handler) = cat_false; /* TODO: get php.ini */
    SWOW_COROUTINE_G(exception_error_severity) = E_ERROR; /* TODO: get php.ini */

    SWOW_COROUTINE_G(runtime_state) = SWOW_COROUTINE_RUNTIME_STATE_RUNNING;

    SWOW_COROUTINE_G(exception) = NULL;

    /* create scoroutine map */
    do {
        zval ztmp;
        array_init(&ztmp);
        SWOW_COROUTINE_G(map) = Z_ARRVAL(ztmp);
    } while (0);

    /* create main scoroutine */
    swow_coroutine_main_create();

    /* set deadlock callback */
    cat_queue_init(&SWOW_COROUTINE_G(deadlock_handlers));
    cat_coroutine_set_deadlock_callback(swow_coroutine_deadlock_callback);

    return SUCCESS;
}

// TODO: killall API
#ifdef CAT_DONT_OPTIMIZE
static void swow_coroutines_kill_destructor(zval *zscoroutine)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_from_object(Z_OBJ_P(zscoroutine));
    ZEND_ASSERT(swow_coroutine_is_alive(scoroutine));
    if (UNEXPECTED(!swow_coroutine_kill(scoroutine))) {
        CAT_CORE_ERROR(COROUTINE, "Execute kill destructor failed, reason: %s", cat_get_last_error_message());
    }
    swow_coroutine_close(scoroutine);
}
#endif

zend_result swow_coroutine_runtime_shutdown(SHUTDOWN_FUNC_ARGS)
{
    SWOW_COROUTINE_G(runtime_state) = SWOW_COROUTINE_RUNTIME_STATE_IN_SHUTDOWN;

    swow_util_handlers_release(&SWOW_COROUTINE_G(deadlock_handlers));

#ifdef CAT_DONT_OPTIMIZE /* the optimization deps on event scheduler */
    /* destruct active scoroutines */
    HashTable *map = SWOW_COROUTINE_G(map);
    do {
        /* kill first (for memory safety) */
        HashTable *map_copy = zend_array_dup(map);
        /* kill all coroutines */
        swow_coroutine_remove_from_map(swow_coroutine_get_main(), map_copy);
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
    cat_coroutine_register_jump(
        SWOW_COROUTINE_G(original_jump)
    );

    if (!cat_coroutine_runtime_shutdown()) {
        return FAILURE;
    }

    SWOW_COROUTINE_G(runtime_state) = SWOW_COROUTINE_RUNTIME_STATE_NONE;

    return SUCCESS;
}
