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
  | Author: Twosee <twose@qq.com>                                            |
  +--------------------------------------------------------------------------+
 */

#ifndef SWOW_COROUTINE_H
#define SWOW_COROUTINE_H

#include "swow.h"

#include "cat_coroutine.h"

#define SWOW_COROUTINE_STACK_PAGE_ALIGNED_SIZE (4 * 1024)
#define SWOW_COROUTINE_MIN_STACK_PAGE_SIZE     (4 * 1024)
#define SWOW_COROUTINE_DEFAULT_STACK_PAGE_SIZE (4 * 1024)
#define SWOW_COROUTINE_MAX_STACK_PAGE_SIZE     (256 * 1024)

#ifndef SWOW_COROUTINE_SWAP_INTERNAL_CONTEXT
#define SWOW_COROUTINE_SWAP_INTERNAL_CONTEXT   1
#endif
#ifndef SWOW_COROUTINE_SWAP_BASIC_GLOBALS
#define SWOW_COROUTINE_SWAP_BASIC_GLOBALS      1
#endif
#ifndef SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS
#define SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS     1
#endif
#ifndef SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
#define SWOW_COROUTINE_SWAP_SILENCE_CONTEXT     1
#endif

extern SWOW_API zend_class_entry *swow_coroutine_ce;
extern SWOW_API zend_object_handlers swow_coroutine_handlers;

extern SWOW_API zend_class_entry *swow_coroutine_exception_ce;

extern SWOW_API zend_class_entry *swow_coroutine_cross_exception_ce;
extern SWOW_API zend_class_entry *swow_coroutine_term_exception_ce;
extern SWOW_API zend_class_entry *swow_coroutine_kill_exception_ce;

typedef enum
{
    SWOW_COROUTINE_FLAG_NO_STACK      = CAT_COROUTINE_FLAG_USR1, /* no PHP stack */
    SWOW_COROUTINE_FLAG_MAIN_FINISHED = CAT_COROUTINE_FLAG_USR2,
    SWOW_COROUTINE_FLAG_ALL_FINISHED  = CAT_COROUTINE_FLAG_USR3,
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
    zval zdata;
    zend_object *cross_exception;
    /* zend things */
    JMP_BUF *bailout;
    zval *vm_stack_top;
    zval *vm_stack_end;
    zend_vm_stack vm_stack;
    size_t vm_stack_page_size;
    zend_execute_data *current_execute_data;
    zend_object *exception;
#if SWOW_COROUTINE_SWAP_INTERNAL_CONTEXT
    /* for PDO non-reentrancy */
    zend_error_handling_t error_handling;
#endif
#if SWOW_COROUTINE_SWAP_BASIC_GLOBALS
    /* for array_walk non-reentrancy */
    swow_fcall_t *array_walk_context;
#endif
#if SWOW_COROUTINE_SWAP_OUTPUT_GLOBALS
    zend_output_globals *output_globals;
#endif
#if SWOW_COROUTINE_SWAP_SILENCE_CONTEXT
    int error_reporting_for_silence;
#endif
} swow_coroutine_exector_t;

typedef struct
{
    cat_coroutine_t coroutine;
    /* php things... */
    swow_coroutine_exector_t *executor;
    zend_object std;
} swow_coroutine_t;

typedef enum
{
    SWOW_COROUTINE_RUNTIME_STATE_NONE,
    SWOW_COROUTINE_RUNTIME_STATE_RUNNING,
    SWOW_COROUTINE_RUNTIME_STATE_IN_SHUTDOWN
} swow_coroutine_runtime_state_t;

typedef struct
{
    swow_object_create_t original_create_object;
    cat_coroutine_resume_t original_resume;
} swow_coroutine_readonly_t;

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
    cat_coroutine_t *original_main;
    cat_bool_t kill_main; /* used to ignore kill exception in main */
    swow_coroutine_readonly_t readonly;
#ifdef SWOW_COROUTINE_USE_RATED
    swow_coroutine_rated_t rated;
#endif
#ifdef SWOW_COROUTINE_HOOK_ZEND_EXUECTE_EX
    void (*)(zend_execute_data *) original_zend_execute_ex;
#endif
CAT_GLOBALS_STRUCT_END(swow_coroutine)

typedef zval *(*swow_coroutine_resume_t)(swow_coroutine_t *scoroutine, zval *zdata);
typedef zval *(*swow_coroutine_yield_t)(zval *zdata);

extern SWOW_API CAT_GLOBALS_DECLARE(swow_coroutine)

#define SWOW_COROUTINE_G(x)       CAT_GLOBALS_GET(swow_coroutine, x)
#define SWOW_COROUTINE_DATA_NULL  ((zval *) CAT_COROUTINE_DATA_NULL)
#define SWOW_COROUTINE_DATA_ERROR ((zval *) CAT_COROUTINE_DATA_ERROR)

/* loaders */

int swow_coroutine_module_init(INIT_FUNC_ARGS);
int swow_coroutine_runtime_init(INIT_FUNC_ARGS);
int swow_coroutine_runtime_shutdown(SHUTDOWN_FUNC_ARGS);

/* helper */

static cat_always_inline swow_coroutine_t *swow_coroutine_get_from_handle(cat_coroutine_t *coroutine)
{
    return cat_container_of(coroutine, swow_coroutine_t, coroutine);
}

static cat_always_inline swow_coroutine_t *swow_coroutine_get_from_object(zend_object *object)
{
    return (swow_coroutine_t *) ((char *) object - swow_coroutine_handlers.offset);
}

/* create */
SWOW_API swow_coroutine_t *swow_coroutine_create(zval *zcallable);
SWOW_API swow_coroutine_t *swow_coroutine_create_ex(zval *zcallable, size_t stack_page_size, size_t c_stack_size);

/* switch */
SWOW_API void swow_coroutine_executor_switch(swow_coroutine_t *scoroutine);
SWOW_API void swow_coroutine_executor_save(swow_coroutine_exector_t *executor);    SWOW_INTERNAL
SWOW_API void swow_coroutine_executor_recover(swow_coroutine_exector_t *executor); SWOW_INTERNAL
SWOW_API cat_bool_t swow_coroutine_jump_precheck(swow_coroutine_t *scoroutine, const zval *zdata);
SWOW_API zval *swow_coroutine_jump(swow_coroutine_t *scoroutine, zval *zdata);
SWOW_API cat_data_t *swow_coroutine_resume_standard(cat_coroutine_t *coroutine, cat_data_t *data); SWOW_INTERNAL
SWOW_API cat_bool_t swow_coroutine_resume(swow_coroutine_t *scoroutine, zval *zdata, zval *retval);
SWOW_API cat_bool_t swow_coroutine_yield(zval *zdata, zval *retval);
SWOW_API cat_bool_t swow_coroutine_resume_ez(swow_coroutine_t *scoroutine);
SWOW_API cat_bool_t swow_coroutine_yield_ez(void);

SWOW_UNSAFE
#define SWOW_COROUTINE_DO_SOMETHING_START(scoroutine) do { \
    zend_execute_data *current_execute_data = NULL; \
    if (EXPECTED(scoroutine != swow_coroutine_get_current())) { \
        current_execute_data = EG(current_execute_data); \
        EG(current_execute_data) = (scoroutine)->executor->current_execute_data; \
    }
SWOW_UNSAFE
#define SWOW_COROUTINE_DO_SOMETHING_END() \
    if (current_execute_data) { \
        EG(current_execute_data) = current_execute_data; \
    } \
} while (0)

/* basic info */
SWOW_API cat_bool_t swow_coroutine_is_available(const swow_coroutine_t *scoroutine);
SWOW_API cat_bool_t swow_coroutine_is_alive(const swow_coroutine_t *scoroutine);
SWOW_API swow_coroutine_t *swow_coroutine_get_from(const swow_coroutine_t *scoroutine); SWOW_UNSAFE
SWOW_API swow_coroutine_t *swow_coroutine_get_previous(const swow_coroutine_t *scoroutine);

/* globals (options) */
SWOW_API size_t swow_coroutine_set_default_stack_page_size(size_t size);
SWOW_API size_t swow_coroutine_set_default_c_stack_size(size_t size);
SWOW_API void swow_coroutine_set_readonly(cat_bool_t enable);

/* globals (getter) */
SWOW_API swow_coroutine_t *swow_coroutine_get_current(void);
SWOW_API swow_coroutine_t *swow_coroutine_get_main(void);
SWOW_API swow_coroutine_t *swow_coroutine_get_scheduler(void);

/* scheduler */
SWOW_API cat_bool_t swow_coroutine_scheduler_run(swow_coroutine_t *scheduler); SWOW_INTERNAL
SWOW_API swow_coroutine_t *swow_coroutine_scheduler_stop(void);                SWOW_INTERNAL
SWOW_API cat_bool_t swow_coroutine_is_scheduler(swow_coroutine_t *scoroutine);

/* executor switcher */
SWOW_API void swow_coroutine_set_executor_switcher(cat_bool_t enable); SWOW_INTERNAL

/* debug */
SWOW_API HashTable *swow_coroutine_get_trace(const swow_coroutine_t *scoroutine, zend_long options, zend_long limit);
SWOW_API smart_str *swow_coroutine_get_trace_to_string(swow_coroutine_t *scoroutine, smart_str *str, zend_long options, zend_long limit);
SWOW_API zend_string *swow_coroutine_get_trace_as_string(const swow_coroutine_t *scoroutine, zend_long options, zend_long limit);
SWOW_API HashTable *swow_coroutine_get_trace_as_list(const swow_coroutine_t *scoroutine, zend_long options, zend_long limit);

SWOW_API void swow_coroutine_dump(swow_coroutine_t *scoroutine);
SWOW_API void swow_coroutine_dump_by_id(cat_coroutine_id_t id);
SWOW_API void swow_coroutine_dump_all(void);

/* exceptions */
SWOW_API cat_bool_t swow_coroutine_throw(swow_coroutine_t *scoroutine, zend_object *exception, zval *retval);
SWOW_API cat_bool_t swow_coroutine_term(swow_coroutine_t *scoroutine, const char *message, zend_long code, zval *retval);
SWOW_API cat_bool_t swow_coroutine_kill(swow_coroutine_t *scoroutine, const char *message, zend_long code);

#endif	/* SWOW_COROUTINE_H */
