/*
  +--------------------------------------------------------------------------+
  | libcat                                                                   |
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

#ifndef CAT_COROUTINE_H
#define CAT_COROUTINE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#include "cat_queue.h"
#include "cat_time.h"

#define CAT_COROUTINE_STACK_ALIGNED_SIZE        (4 * 1024)
#define CAT_COROUTINE_MIN_STACK_SIZE            (128 * 1024)
#define CAT_COROUTINE_RECOMMENDED_STACK_SIZE    (256 * 1024)
#define CAT_COROUTINE_MAX_STACK_SIZE            (16 * 1024 * 1024)

#define CAT_COROUTINE_MIN_ID                    0
#define CAT_COROUTINE_MAX_ID                    UINT64_MAX
#define CAT_COROUTINE_MAIN_ID                   1

typedef void cat_coroutine_stack_t;

#ifdef CAT_COROUTINE_USE_UCONTEXT
#include <ucontext.h>

typedef ucontext_t cat_coroutine_context_t;

typedef struct
{
    cat_data_t *data;
} cat_coroutine_transfer_t;

#define cat_coroutine_context_make(ucontext, function, argc, ...) \
        makecontext(ucontext, function, argc, ##__VA_ARGS__)

#define cat_coroutine_context_jump(current_ucontext, ucontext)    do { \
    if (unlikely(swapcontext(current_ucontext, ucontext) != 0)) { \
        cat_core_error(COROUTINE, "Ucontext swapcontext failed"); \
    } \
} while (0)

#else

typedef void* cat_coroutine_context_t;

typedef struct
{
    cat_coroutine_context_t from_context;
    cat_data_t *data;
} cat_coroutine_transfer_t;

typedef void (*cat_coroutine_context_function_t)(cat_coroutine_transfer_t transfer);

cat_coroutine_context_t cat_coroutine_context_make(cat_coroutine_stack_t *stack, size_t stack_size, cat_coroutine_context_function_t function);
cat_coroutine_transfer_t cat_coroutine_context_jump(cat_coroutine_context_t const target_context, cat_data_t *transfer_data);
#endif

#define CAT_COROUTINE_ID_FMT    "%" PRIu64
typedef uint64_t cat_coroutine_id_t;

typedef enum
{
    CAT_COROUTINE_FLAG_NONE = 0,
    CAT_COROUTINE_FLAG_ON_STACK = 1 << 0,
    CAT_COROUTINE_FLAG_MANUAL_CLOSE = 1 << 1,
    CAT_COROUTINE_FLAG_SCHEDULER = 1 << 2,
    /* for user */
#define CAT_COROUTINE_FLAG_USR_GEN(XX) \
    CAT_COROUTINE_FLAG_USR##XX = 1 << (XX + (32 - 1 - 8))
    CAT_COROUTINE_FLAG_USR_GEN(1),
    CAT_COROUTINE_FLAG_USR_GEN(2),
    CAT_COROUTINE_FLAG_USR_GEN(3),
    CAT_COROUTINE_FLAG_USR_GEN(4),
    CAT_COROUTINE_FLAG_USR_GEN(5),
    CAT_COROUTINE_FLAG_USR_GEN(6),
    CAT_COROUTINE_FLAG_USR_GEN(7),
    CAT_COROUTINE_FLAG_USR_GEN(8),
#undef CAT_COROUTINE_FLAG_USR_GEN
} cat_coroutine_flag_t;

typedef uint32_t cat_coroutine_flags_t;

#define CAT_COROUTINE_STATE_MAP(XX) \
    XX(INIT,     0) \
    XX(READY,    1) \
    XX(RUNNING,  2) \
    XX(WAITING,  3) \
    XX(FINISHED, 4) \
    XX(LOCKED,   5) \
    XX(DEAD,     6) \

typedef enum
{
#define CAT_COROUTINE_STATE_GEN(name, value) CAT_ENUM_GEN(CAT_COROUTINE_STATE_, name, value)
    CAT_COROUTINE_STATE_MAP(CAT_COROUTINE_STATE_GEN)
#undef CAT_COROUTINE_STATE_GEN
} cat_coroutine_state_t;

typedef enum
{
    CAT_COROUTINE_OPCODE_NONE     = 0,
    CAT_COROUTINE_OPCODE_NO_DATA  = 1 << 0, /* transfer data should be CAT_COROUTINE_NULL */
    CAT_COROUTINE_OPCODE_CHECKED  = 1 << 1, /* checked if it is resumable */
    CAT_COROUTINE_OPCODE_WAIT     = 1 << 2, /* wait for a specified coroutine to wake it up */
    /* for user */
    CAT_COROUTINE_OPCODE_USR1 = 1 << 4,
    CAT_COROUTINE_OPCODE_USR2 = 1 << 5,
    CAT_COROUTINE_OPCODE_USR3 = 1 << 6,
    CAT_COROUTINE_OPCODE_USR4 = 1 << 7
} cat_coroutine_opcode_t;

typedef uint8_t cat_coroutine_opcodes_t;

#define CAT_COROUTINE_ROUND_FMT "%" PRIu64
typedef uint64_t cat_coroutine_round_t;

#define CAT_COROUTINE_STACK_SIZE_FMT "%u"
typedef uint32_t cat_coroutine_stack_size_t;

typedef cat_data_t *(*cat_coroutine_function_t)(cat_data_t *data);
typedef void (*cat_coroutine_easy_function_t)(void);

typedef struct cat_coroutine_s
{
    /* id is always the first member (readonly, invariant) */
    cat_coroutine_id_t id;
    /* for queue (internal) */
    union {
        struct cat_coroutine_s *coroutine;
        cat_queue_node_t node;
    } waiter;
    /* invariant info (readonly) */
    cat_msec_t start_time;
    cat_coroutine_flags_t flags;
    /* runtime info (readonly) */
    cat_coroutine_state_t state;
    cat_coroutine_opcodes_t opcodes; /* (writable) will be reset before resume */
    cat_coroutine_round_t round;
    struct cat_coroutine_s *from CAT_UNSAFE;
    struct cat_coroutine_s *previous;
    /* internal properties (readonly) */
    cat_coroutine_stack_t *stack;
    cat_coroutine_stack_size_t stack_size;
    cat_coroutine_context_t context;
    cat_coroutine_function_t function;
#ifdef CAT_COROUTINE_USE_UCONTEXT
    cat_data_t *transfer_data;
#endif
    /* ext info */
#ifdef HAVE_VALGRIND
    uint32_t valgrind_stack_id;
#endif
} cat_coroutine_t;

typedef cat_data_t *(*cat_coroutine_resume_t)(cat_coroutine_t *coroutine, cat_data_t *data);

#define CAT_COROUTINE_COUNT_FMT "%u"
typedef uint32_t cat_coroutine_count_t;

CAT_GLOBALS_STRUCT_BEGIN(cat_coroutine)
    /* options */
    cat_coroutine_stack_size_t default_stack_size;
    /* data */
    cat_data_t *null;
    cat_data_t *error;
    /* coroutines */
    cat_coroutine_t *current;
    cat_coroutine_t *main;
    cat_coroutine_t _main;
    /* scheduler */
    cat_coroutine_t *scheduler;
    /* functions */
    cat_coroutine_resume_t resume;
    /* info */
    cat_coroutine_id_t last_id;
    cat_coroutine_count_t active_count;
    cat_coroutine_count_t peak_count;
    /* watch-dog */
    cat_coroutine_round_t round;
CAT_GLOBALS_STRUCT_END(cat_coroutine)

extern CAT_API CAT_GLOBALS_DECLARE(cat_coroutine)

#define CAT_COROUTINE_G(x)                   CAT_GLOBALS_GET(cat_coroutine, x)
/* Notice: type conversion make it rval */
#define CAT_COROUTINE_DATA_NULL              ((cat_data_t *) CAT_COROUTINE_G(null))
#define CAT_COROUTINE_DATA_ERROR             ((cat_data_t *) CAT_COROUTINE_G(error))

/* module initializers */
CAT_API cat_bool_t cat_coroutine_module_init(void);
CAT_API cat_bool_t cat_coroutine_runtime_init(void);

/* register/options */
CAT_API void cat_coroutine_register_common_wrappers(cat_coroutine_resume_t resume, cat_data_t *null, cat_data_t *error);
/* return the original resume function ptr */
CAT_API cat_coroutine_resume_t cat_coroutine_register_resume(cat_coroutine_resume_t resume);
/* return the original main coroutine ptr */
CAT_API cat_coroutine_t *cat_coroutine_register_main(cat_coroutine_t *coroutine);
/* return the original stack size */
CAT_API cat_coroutine_stack_size_t cat_coroutine_set_default_stack_size(size_t size);

/* globals */
CAT_API cat_coroutine_stack_size_t cat_coroutine_get_default_stack_size(void);
CAT_API cat_coroutine_t *cat_coroutine_get_current(void);
CAT_API cat_coroutine_id_t cat_coroutine_get_current_id(void);
CAT_API cat_coroutine_t *cat_coroutine_get_main(void);
CAT_API cat_coroutine_t *cat_coroutine_get_scheduler(void);
CAT_API cat_coroutine_id_t cat_coroutine_get_last_id(void);
CAT_API cat_coroutine_count_t cat_coroutine_get_active_count(void);
CAT_API cat_coroutine_count_t cat_coroutine_get_peak_count(void);
CAT_API cat_coroutine_round_t cat_coroutine_get_current_round(void);

/* ctor and dtor */
CAT_API void cat_coroutine_init(cat_coroutine_t *coroutine);
CAT_API cat_coroutine_t *cat_coroutine_create(cat_coroutine_t *coroutine, cat_coroutine_function_t function);
CAT_API cat_coroutine_t *cat_coroutine_create_ex(cat_coroutine_t *coroutine, cat_coroutine_function_t function, size_t stack_size);
/* Notice: unless you set FLAG_MANUAL_CLOSE, or you need not close coroutine by yourself */
CAT_API void cat_coroutine_close(cat_coroutine_t *coroutine);
/* switch */
CAT_API cat_bool_t cat_coroutine_jump_precheck(cat_coroutine_t *coroutine, const cat_data_t *data);
CAT_API cat_data_t *cat_coroutine_jump(cat_coroutine_t *coroutine, cat_data_t *data);
CAT_API cat_data_t *cat_coroutine_resume_standard(cat_coroutine_t *coroutine, cat_data_t *data);
#define cat_coroutine_resume CAT_COROUTINE_G(resume)
CAT_API cat_data_t *cat_coroutine_yield(cat_data_t *data);
CAT_API cat_bool_t cat_coroutine_resume_ez(cat_coroutine_t *coroutine);
CAT_API cat_bool_t cat_coroutine_yield_ez(void);

/* properties */
CAT_API cat_coroutine_id_t cat_coroutine_get_id(const cat_coroutine_t *coroutine);
CAT_API cat_coroutine_t *cat_coroutine_get_from(const cat_coroutine_t *coroutine); CAT_UNSAFE
CAT_API cat_coroutine_t *cat_coroutine_get_previous(const cat_coroutine_t *coroutine);
CAT_API cat_coroutine_stack_size_t cat_coroutine_get_stack_size(const cat_coroutine_t *coroutine);

/* status */
CAT_API cat_bool_t cat_coroutine_is_available(const cat_coroutine_t *coroutine);
CAT_API cat_bool_t cat_coroutine_is_alive(const cat_coroutine_t *coroutine);
CAT_API const char *cat_coroutine_state_name(cat_coroutine_state_t state);
CAT_API cat_coroutine_state_t cat_coroutine_get_state(const cat_coroutine_t *coroutine);
CAT_API const char *cat_coroutine_get_state_name(const cat_coroutine_t *coroutine);
CAT_API cat_coroutine_opcodes_t cat_coroutine_get_opcodes(const cat_coroutine_t *coroutine);
CAT_API void cat_coroutine_set_opcodes(cat_coroutine_t *coroutine, cat_coroutine_opcodes_t opcodes);
CAT_API cat_coroutine_round_t cat_coroutine_get_round(const cat_coroutine_t *coroutine);
CAT_API cat_msec_t cat_coroutine_get_start_time(const cat_coroutine_t *coroutine);
CAT_API cat_msec_t cat_coroutine_get_elapsed(const cat_coroutine_t *coroutine);
CAT_API char *cat_coroutine_get_elapsed_as_string(const cat_coroutine_t *coroutine);

/* scheduler */
CAT_API cat_bool_t cat_coroutine_register_scheduler(cat_coroutine_t *coroutine); CAT_INTERNAL
CAT_API cat_coroutine_t *cat_coroutine_unregister_scheduler(void);               CAT_INTERNAL
CAT_API cat_bool_t cat_coroutine_scheduler_run(cat_coroutine_t *scheduler);      CAT_INTERNAL
CAT_API cat_coroutine_t *cat_coroutine_scheduler_stop(void);                     CAT_INTERNAL

/* special */
CAT_API void cat_coroutine_disable_auto_close(cat_coroutine_t *coroutine);
CAT_API cat_coroutine_t *cat_coroutine_exchange_with_previous(void); CAT_INTERNAL
/* take a nap, wait for sb to wake it up */
CAT_API cat_bool_t cat_coroutine_wait_for(cat_coroutine_t *who); CAT_INTERNAL
/* lock */
CAT_API cat_bool_t cat_coroutine_is_locked(cat_coroutine_t *coroutine); CAT_INTERNAL
CAT_API void cat_coroutine_lock(void);                                  CAT_INTERNAL
CAT_API cat_bool_t cat_coroutine_unlock(cat_coroutine_t *coroutine);    CAT_INTERNAL

/* helper */
CAT_API cat_coroutine_t *cat_coroutine_run(cat_coroutine_t *coroutine, cat_coroutine_function_t function, cat_data_t *data);
CAT_API cat_coroutine_t *cat_coroutine_run_ez(cat_coroutine_t *coroutine, cat_coroutine_easy_function_t function);

#ifdef __cplusplus
}
#endif
#endif /* CAT_COROUTINE_H */
