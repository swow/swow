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

#define CAT_COROUTINE_MIN_STACK_SIZE            (128UL * 1024UL)
#define CAT_COROUTINE_RECOMMENDED_STACK_SIZE    (256UL * 1024UL)
#define CAT_COROUTINE_MAX_STACK_SIZE            (16UL * 1024UL * 1024UL)

#define CAT_COROUTINE_MIN_ID                    0ULL
#define CAT_COROUTINE_MAX_ID                    UINT64_MAX

#define CAT_COROUTINE_SCHEDULER_ID              CAT_COROUTINE_MIN_ID
#define CAT_COROUTINE_MAIN_ID                   1ULL

#ifndef CAT_COROUTINE_USE_UCONTEXT
typedef void *cat_coroutine_context_t;
#else
#define _XOPEN_SOURCE 700 /* for APPLE */
#include <ucontext.h>
typedef ucontext_t cat_coroutine_context_t;
#endif

typedef uint64_t cat_coroutine_id_t;
#define CAT_COROUTINE_ID_FMT "%" PRIu64
#define CAT_COROUTINE_ID_FMT_SPEC PRIu64

typedef enum cat_coroutine_flag_e {
    CAT_COROUTINE_FLAG_NONE = 0,
    CAT_COROUTINE_FLAG_ALLOCATED = 1 << 0,
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
    XX(INIT,     0, "init") \
    XX(READY,    1, "ready") \
    XX(RUNNING,  2, "running") \
    XX(WAITING,  3, "waiting") \
    XX(LOCKED,   4, "locked") \
    XX(FINISHED, 5, "finished") \
    XX(DEAD,     6, "dead") \

typedef enum cat_coroutine_state_e {
#define CAT_COROUTINE_STATE_GEN(name, value, unused) CAT_ENUM_GEN(CAT_COROUTINE_STATE_, name, value)
    CAT_COROUTINE_STATE_MAP(CAT_COROUTINE_STATE_GEN)
#undef CAT_COROUTINE_STATE_GEN
} cat_coroutine_state_t;

typedef enum cat_coroutine_opcode_e {
    /* built-in (0 ~ 7) */
    CAT_COROUTINE_OPCODE_NONE     = 0,
    CAT_COROUTINE_OPCODE_CHECKED  = 1 << 0, /* we have already checked if it is resumable */
    CAT_COROUTINE_OPCODE_WAIT     = 1 << 1, /* wait for a specified coroutine to wake it up */
    /* for user (8 ~ 15) */
#define CAT_COROUTINE_OPCODE_USR_GEN(XX) \
    CAT_COROUTINE_OPCODE_USR##XX = 1 << (XX + (16 - 1 - 8))
    CAT_COROUTINE_OPCODE_USR_GEN(1),
    CAT_COROUTINE_OPCODE_USR_GEN(2),
    CAT_COROUTINE_OPCODE_USR_GEN(3),
    CAT_COROUTINE_OPCODE_USR_GEN(4),
    CAT_COROUTINE_OPCODE_USR_GEN(5),
    CAT_COROUTINE_OPCODE_USR_GEN(6),
    CAT_COROUTINE_OPCODE_USR_GEN(7),
    CAT_COROUTINE_OPCODE_USR_GEN(8),
#undef CAT_COROUTINE_OPCODE_USR_GEN
} cat_coroutine_opcode_t;

typedef uint16_t cat_coroutine_opcodes_t;

typedef uint64_t cat_coroutine_round_t;
#define CAT_COROUTINE_ROUND_FMT "%" PRIu64
#define CAT_COROUTINE_ROUND_FMT_SPEC PRIu64

typedef uint32_t cat_coroutine_stack_size_t;
#define CAT_COROUTINE_STACK_SIZE_FMT "%u"
#define CAT_COROUTINE_STACK_SIZE_FMT_SPEC "u"

typedef cat_data_t *(*cat_coroutine_function_t)(cat_data_t *data);

typedef struct cat_coroutine_s cat_coroutine_t;

struct cat_coroutine_s
{
    union {
        cat_coroutine_t *coroutine;
        cat_queue_node_t node;
    } waiter;
    /* invariant info (readonly) */
    cat_coroutine_id_t id;
    cat_msec_t start_time;
    cat_coroutine_flags_t flags;
    /* runtime info (readonly) */
    cat_coroutine_state_t state;
    cat_coroutine_opcodes_t opcodes; /* (writable) will be reset before resume */
    cat_coroutine_round_t round;
    cat_coroutine_t *from CAT_UNSAFE;
    cat_coroutine_t *previous;
    /* internal properties (readonly) */
    cat_coroutine_function_t function;
    cat_coroutine_stack_size_t stack_size;
    /* internal properties (inaccessible) */
    uint32_t virtual_memory_size;
    void *virtual_memory;
    cat_coroutine_context_t context;
#ifdef CAT_COROUTINE_USE_UCONTEXT
    cat_data_t *transfer_data;
#endif
    /* ext info */
#ifdef CAT_HAVE_VALGRIND
    uint32_t valgrind_stack_id;
#endif
#ifdef CAT_HAVE_ASAN
    void *asan_fake_stack;
    const void *asan_stack;
    size_t asan_stack_size;
#endif
};

typedef cat_bool_t (*cat_coroutine_resume_t)(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval);

typedef uint32_t cat_coroutine_count_t;
#define CAT_COROUTINE_COUNT_FMT "%u"
#define CAT_COROUTINE_COUNT_FMT_SPEC "u"

CAT_GLOBALS_STRUCT_BEGIN(cat_coroutine)
    /* options */
    cat_coroutine_stack_size_t default_stack_size;
    cat_log_type_t dead_lock_log_type;
    /* coroutines */
    cat_coroutine_t *current;
    cat_coroutine_t *main;
    cat_coroutine_t _main;
    /* scheduler */
    cat_coroutine_t *scheduler;
    cat_queue_t waiters;
    cat_coroutine_count_t waiter_count;
    /* functions */
    cat_coroutine_resume_t resume;
    cat_coroutine_resume_t original_resume;
    /* info */
    cat_coroutine_id_t last_id;
    cat_coroutine_count_t count;
    cat_coroutine_count_t peak_count;
    /* for watch-dog */
    cat_coroutine_round_t round;
CAT_GLOBALS_STRUCT_END(cat_coroutine)

extern CAT_API CAT_GLOBALS_DECLARE(cat_coroutine)

#define CAT_COROUTINE_G(x) CAT_GLOBALS_GET(cat_coroutine, x)

/* module initializers */
CAT_API cat_bool_t cat_coroutine_module_init(void);
CAT_API cat_bool_t cat_coroutine_runtime_init(void);
CAT_API cat_bool_t cat_coroutine_runtime_shutdown(void);

/* register/options */
/* return the original resume function ptr */
CAT_API cat_coroutine_resume_t cat_coroutine_register_resume(cat_coroutine_resume_t resume);
CAT_API cat_bool_t cat_coroutine_switch_blocked(void);
CAT_API void cat_coroutine_switch_block(void);
CAT_API void cat_coroutine_switch_unblock(void);
/* return the original main coroutine ptr */
CAT_API cat_coroutine_t *cat_coroutine_register_main(cat_coroutine_t *coroutine);
/* return the original stack size */
CAT_API cat_coroutine_stack_size_t cat_coroutine_set_default_stack_size(size_t size);
/* It is recommended to set to error or warning */
CAT_API cat_bool_t cat_coroutine_set_dead_lock_log_type(cat_log_type_t type);

/* globals */
CAT_API cat_coroutine_stack_size_t cat_coroutine_get_default_stack_size(void);
CAT_API cat_log_type_t cat_coroutine_get_dead_lock_log_type(void);
CAT_API cat_coroutine_t *cat_coroutine_get_current(void);
CAT_API cat_coroutine_id_t cat_coroutine_get_current_id(void);
CAT_API cat_coroutine_t *cat_coroutine_get_by_index(cat_coroutine_count_t index);
CAT_API cat_coroutine_t *cat_coroutine_get_root(void);
CAT_API cat_coroutine_t *cat_coroutine_get_main(void);
CAT_API cat_coroutine_t *cat_coroutine_get_scheduler(void);
CAT_API cat_coroutine_id_t cat_coroutine_get_last_id(void);
CAT_API cat_coroutine_count_t cat_coroutine_get_count(void);
CAT_API cat_coroutine_count_t cat_coroutine_get_real_count(void);
CAT_API cat_coroutine_count_t cat_coroutine_get_peak_count(void);
CAT_API cat_coroutine_round_t cat_coroutine_get_current_round(void);

/* ctor and dtor */
CAT_API void cat_coroutine_init(cat_coroutine_t *coroutine);
CAT_API cat_coroutine_t *cat_coroutine_create(cat_coroutine_t *coroutine, cat_coroutine_function_t function);
CAT_API cat_coroutine_t *cat_coroutine_create_ex(cat_coroutine_t *coroutine, cat_coroutine_function_t function, size_t stack_size);
/* Notice: unless you set FLAG_MANUAL_CLOSE, or you need not close coroutine by yourself */
CAT_API void cat_coroutine_close(cat_coroutine_t *coroutine);
/* switch (internal) */
CAT_API cat_data_t *cat_coroutine_jump(cat_coroutine_t *coroutine, cat_data_t *data);
/* switch (external) */
CAT_API cat_bool_t cat_coroutine_is_resumable(const cat_coroutine_t *coroutine);
CAT_API cat_bool_t cat_coroutine_resume_standard(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval);
#define cat_coroutine_resume CAT_COROUTINE_G(resume)
CAT_API cat_bool_t cat_coroutine_yield(cat_data_t *data, cat_data_t **retval);

/* properties */
CAT_API cat_coroutine_id_t cat_coroutine_get_id(const cat_coroutine_t *coroutine);
CAT_API cat_coroutine_t *cat_coroutine_get_from(const cat_coroutine_t *coroutine); CAT_UNSAFE
CAT_API cat_coroutine_t *cat_coroutine_get_previous(const cat_coroutine_t *coroutine);
CAT_API cat_coroutine_stack_size_t cat_coroutine_get_stack_size(const cat_coroutine_t *coroutine);

/* status */
CAT_API cat_bool_t cat_coroutine_is_available(const cat_coroutine_t *coroutine);
CAT_API cat_bool_t cat_coroutine_is_alive(const cat_coroutine_t *coroutine);
CAT_API cat_bool_t cat_coroutine_is_over(const cat_coroutine_t *coroutine);
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
typedef void (*cat_coroutine_schedule_function_t)(void);
typedef void (*cat_coroutine_dead_lock_function_t)(void);
typedef struct cat_coroutine_scheduler_s {
    cat_coroutine_schedule_function_t schedule;
    cat_coroutine_dead_lock_function_t dead_lock;
} cat_coroutine_scheduler_t;
CAT_API cat_coroutine_t *cat_coroutine_scheduler_run(cat_coroutine_t *coroutine, const cat_coroutine_scheduler_t *scheduler); CAT_INTERNAL
CAT_API cat_coroutine_t *cat_coroutine_scheduler_close(void); CAT_INTERNAL

/* sync */
CAT_API cat_bool_t cat_coroutine_wait(void);
CAT_API void cat_coroutine_notify_all(void); CAT_INTERNAL

/* special */
/* take a nap, wait for sb to wake it up */
CAT_API cat_bool_t cat_coroutine_wait_for(cat_coroutine_t *who); CAT_INTERNAL
/* lock */
CAT_API cat_bool_t cat_coroutine_lock(void);                         CAT_INTERNAL
CAT_API cat_bool_t cat_coroutine_unlock(cat_coroutine_t *coroutine); CAT_INTERNAL
/* main/scheduler/none */
CAT_API const char *cat_coroutine_get_role_name(const cat_coroutine_t *coroutine);
CAT_API const char *cat_coroutine_get_current_role_name(void);

/* helper */
CAT_API cat_coroutine_t *cat_coroutine_run(cat_coroutine_t *coroutine, cat_coroutine_function_t function, cat_data_t *data);

#define cat_coroutine_schedule(coroutine, module_type, name, ...) do { \
    if (unlikely(!cat_coroutine_resume(coroutine, NULL, NULL))) { \
        CAT_CORE_ERROR_WITH_LAST(module_type, name " schedule failed", ##__VA_ARGS__); \
    } \
} while (0)

#ifdef __cplusplus
}
#endif
#endif /* CAT_COROUTINE_H */
