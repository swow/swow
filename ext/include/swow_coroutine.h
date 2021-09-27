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

#ifndef SWOW_COROUTINE_H
#define SWOW_COROUTINE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_coroutine.h"

#define SWOW_COROUTINE_STACK_PAGE_ALIGNED_SIZE (4 * 1024)
#define SWOW_COROUTINE_MIN_STACK_PAGE_SIZE     (4 * 1024)
#define SWOW_COROUTINE_DEFAULT_STACK_PAGE_SIZE (4 * 1024)
#define SWOW_COROUTINE_MAX_STACK_PAGE_SIZE     (256 * 1024)

#if PHP_VERSION_ID >= 80000
#define SWOW_COROUTINE_SWAP_JIT_GLOBALS     1
#endif
#define SWOW_COROUTINE_SWAP_ERROR_HANDING   1
#if PHP_VERSION_ID < 80100
#define SWOW_COROUTINE_SWAP_BASIC_GLOBALS   1
#endif
#define SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS  1
#define SWOW_COROUTINE_SWAP_SILENCE_CONTEXT 1

extern SWOW_API zend_class_entry *swow_coroutine_ce;
extern SWOW_API zend_object_handlers swow_coroutine_handlers;

extern SWOW_API zend_class_entry *swow_coroutine_exception_ce;

typedef enum
{
    SWOW_COROUTINE_FLAG_DEYING = CAT_COROUTINE_FLAG_USR1,
    SWOW_COROUTINE_FLAG_DEBUGGING = CAT_COROUTINE_FLAG_USR2,
} swow_coroutine_flag_t;

typedef enum
{
    SWOW_COROUTINE_OPCODE_ACCEPT_ZDATA = CAT_COROUTINE_OPCODE_USR1,
} swow_coroutine_opcode_t;

/*
 * these things will no longer be used
 * after the coroutine is finished
 */
typedef struct
{
    /* coroutine info */
    zval zcallable;
    zend_fcall_info_cache fcc;
    /* zend things */
    JMP_BUF *bailout;
    zval *vm_stack_top;
    zval *vm_stack_end;
    zend_vm_stack vm_stack;
    size_t vm_stack_page_size;
    zend_execute_data *current_execute_data;
    zend_object *exception;
#ifdef SWOW_COROUTINE_SWAP_ERROR_HANDING
    /* for hack practice of php-kernel (convert warning to exception) */
    zend_error_handling_t error_handling;
    zend_class_entry *exception_class;
#endif
#ifdef SWOW_COROUTINE_SWAP_BASIC_GLOBALS
    /* for array_walk non-reentrancy */
    swow_fcall_t *array_walk_context;
#endif
#ifdef SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS
    zend_output_globals *output_globals;
#endif
#ifdef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
    int error_reporting;
#endif
#ifdef SWOW_COROUTINE_SWAP_JIT_GLOBALS
    uint32_t jit_trace_num;
#endif
} swow_coroutine_executor_t;

typedef struct
{
    cat_coroutine_t coroutine;
    /* php things... */
    int exit_status;
    swow_coroutine_executor_t *executor;
    zend_object std;
} swow_coroutine_t;

typedef enum
{
    SWOW_COROUTINE_RUNTIME_STATE_NONE,
    SWOW_COROUTINE_RUNTIME_STATE_RUNNING,
    SWOW_COROUTINE_RUNTIME_STATE_IN_SHUTDOWN
} swow_coroutine_runtime_state_t;

#ifdef SWOW_COROUTINE_USE_RATED
typedef struct
{
    swow_coroutine_t *killer;
    swow_coroutine_t *dead;
} swow_coroutine_rated_t;
#endif

CAT_GLOBALS_STRUCT_BEGIN(swow_coroutine)
    /* ini */
    size_t default_stack_page_size;
    cat_bool_t classic_error_handler;
    zend_long exception_error_severity;
    /* runtime */
    swow_coroutine_runtime_state_t runtime_state;
    HashTable *map;
#ifdef SWOW_COROUTINE_ENABLE_CUSTOM_ENTRY
    zend_class_entry *custom_entry; /* we can resolve resume now */
#endif
    /* internal special */
    cat_coroutine_resume_t original_resume;
    cat_coroutine_t *original_main;
#ifdef SWOW_COROUTINE_USE_RATED
    swow_coroutine_rated_t rated;
#endif
    zval ztransfer_data;
    zend_object *exception;
CAT_GLOBALS_STRUCT_END(swow_coroutine)

typedef zval *(*swow_coroutine_resume_t)(swow_coroutine_t *scoroutine, zval *zdata);
typedef zval *(*swow_coroutine_yield_t)(zval *zdata);

extern SWOW_API CAT_GLOBALS_DECLARE(swow_coroutine)

#define SWOW_COROUTINE_G(x) CAT_GLOBALS_GET(swow_coroutine, x)

/* loaders */

int swow_coroutine_module_init(INIT_FUNC_ARGS);
int swow_coroutine_runtime_init(INIT_FUNC_ARGS);
int swow_coroutine_runtime_shutdown(SHUTDOWN_FUNC_ARGS);
zend_result swow_coroutine_delay_runtime_shutdown(void);

/* helper */

static zend_always_inline swow_coroutine_t *swow_coroutine_get_from_handle(cat_coroutine_t *coroutine)
{
    return cat_container_of(coroutine, swow_coroutine_t, coroutine);
}

static zend_always_inline swow_coroutine_t *swow_coroutine_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_coroutine_t, std);
}

/* create */
SWOW_API swow_coroutine_t *swow_coroutine_create(zval *zcallable);
SWOW_API swow_coroutine_t *swow_coroutine_create_ex(zend_class_entry *ce, zval *zcallable, size_t stack_page_size, size_t c_stack_size);
SWOW_API void swow_coroutine_close(swow_coroutine_t *scoroutine);

/* switch */
SWOW_API void swow_coroutine_executor_switch(swow_coroutine_executor_t *current_executor, swow_coroutine_executor_t *target_executor); SWOW_INTERNAL
SWOW_API void swow_coroutine_executor_save(swow_coroutine_executor_t *executor);    SWOW_INTERNAL
SWOW_API void swow_coroutine_executor_recover(swow_coroutine_executor_t *executor); SWOW_INTERNAL
SWOW_API zval *swow_coroutine_jump(swow_coroutine_t *scoroutine, zval *zdata);
SWOW_API cat_bool_t swow_coroutine_is_resumable(const swow_coroutine_t *scoroutine);
SWOW_API cat_bool_t swow_coroutine_resume_standard(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval); SWOW_INTERNAL
SWOW_API cat_bool_t swow_coroutine_resume(swow_coroutine_t *scoroutine, zval *zdata, zval *retval);
SWOW_API cat_bool_t swow_coroutine_yield(zval *zdata, zval *retval);

/* basic info */
SWOW_API cat_bool_t swow_coroutine_is_available(const swow_coroutine_t *scoroutine);
SWOW_API cat_bool_t swow_coroutine_is_alive(const swow_coroutine_t *scoroutine);
SWOW_API cat_bool_t swow_coroutine_is_executing(const swow_coroutine_t *scoroutine);
SWOW_API swow_coroutine_t *swow_coroutine_get_from(const swow_coroutine_t *scoroutine); SWOW_UNSAFE
SWOW_API swow_coroutine_t *swow_coroutine_get_previous(const swow_coroutine_t *scoroutine);

/* globals (options) */
SWOW_API size_t swow_coroutine_set_default_stack_page_size(size_t size);
SWOW_API size_t swow_coroutine_set_default_c_stack_size(size_t size);

/* globals (getter) */
SWOW_API swow_coroutine_t *swow_coroutine_get_current(void);
SWOW_API swow_coroutine_t *swow_coroutine_get_main(void);
SWOW_API swow_coroutine_t *swow_coroutine_get_scheduler(void);

/* debug */
SWOW_API zend_string *swow_coroutine_get_executed_filename(const swow_coroutine_t *scoroutine, zend_long level);
SWOW_API uint32_t swow_coroutine_get_executed_lineno(const swow_coroutine_t *scoroutine, zend_long level);
SWOW_API zend_string *swow_coroutine_get_executed_function_name(const swow_coroutine_t *scoroutine, zend_long level);

SWOW_API HashTable *swow_coroutine_get_trace(const swow_coroutine_t *scoroutine, zend_long level, zend_long limit, zend_long options);
SWOW_API smart_str *swow_coroutine_get_trace_as_smart_str(swow_coroutine_t *scoroutine, smart_str *str, zend_long level, zend_long limit, zend_long options);
SWOW_API zend_string *swow_coroutine_get_trace_as_string(const swow_coroutine_t *scoroutine, zend_long level, zend_long limit, zend_long options);
SWOW_API HashTable *swow_coroutine_get_trace_as_list(const swow_coroutine_t *scoroutine, zend_long level, zend_long limit, zend_long options);

SWOW_API HashTable *swow_coroutine_get_defined_vars(swow_coroutine_t *scoroutine, zend_ulong level);
SWOW_API cat_bool_t swow_coroutine_set_local_var(swow_coroutine_t *scoroutine, zend_string *name, zval *value, zend_long level, zend_bool force);

SWOW_API cat_bool_t swow_coroutine_eval(swow_coroutine_t *scoroutine, zend_string *string, zend_long level, zval *return_value); SWOW_MAY_EXCEPTION
SWOW_API cat_bool_t swow_coroutine_call(swow_coroutine_t *scoroutine, zval *zcallable, zval *return_value);                      SWOW_MAY_EXCEPTION

SWOW_API swow_coroutine_t *swow_coroutine_get_by_id(cat_coroutine_id_t id);
SWOW_API zval *swow_coroutine_get_zval_by_id(cat_coroutine_id_t id);

SWOW_API void swow_coroutine_dump(swow_coroutine_t *scoroutine);
SWOW_API void swow_coroutine_dump_by_id(cat_coroutine_id_t id);
SWOW_API void swow_coroutine_dump_all(void);

/* exceptions */
SWOW_API cat_bool_t swow_coroutine_throw(swow_coroutine_t *scoroutine, zend_object *exception, zval *retval);
SWOW_API cat_bool_t swow_coroutine_kill(swow_coroutine_t *scoroutine);
SWOW_API cat_bool_t swow_coroutine_kill_all(void);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_COROUTINE_H */
