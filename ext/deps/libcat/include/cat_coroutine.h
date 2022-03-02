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

#if defined(CAT_COROUTINE_USE_THREAD_CONTEXT)
# if defined(CAT_THREAD_SAFE)
# error "Coroutine thread-context can not work on thread-safe mode"
# endif
typedef uv_thread_t cat_coroutine_context_t;
#elif defined(CAT_COROUTINE_USE_UCONTEXT)
# define _XOPEN_SOURCE 700 /* for APPLE */
# include <ucontext.h>
typedef ucontext_t cat_coroutine_context_t;
#else
#define CAT_COROUTINE_USE_BOOST_CONTEXT
typedef void *cat_coroutine_context_t;
#endif

#if defined(CAT_COROUTINE_USE_BOOST_CONTEXT) || defined(CAT_COROUTINE_USE_UCONTEXT)
#define CAT_COROUTINE_USE_USER_STACK 1
#endif

#ifndef CAT_COROUTINE_USE_BOOST_CONTEXT
#define CAT_COROUTINE_USE_USER_TRANSFER_DATA
#endif

#if defined(CAT_HAVE_ASAN) && !defined(CAT_COROUTINE_USE_THREAD_CONTEXT)
#define CAT_COROUTINE_USE_ASAN
#endif

typedef uint64_t cat_coroutine_id_t;
#define CAT_COROUTINE_ID_FMT "%" PRIu64
#define CAT_COROUTINE_ID_FMT_SPEC PRIu64

#define CAT_COROUTINE_FLAG_MAP(XX) \
    /* built-in persistent (0 ~ 7) */ \
    XX(NONE,        0) \
    XX(ALLOCATED,   1 << 0) \
    /* built-in runtime (8 ~ 15) */ \
    XX(SCHEDULING,  1 << 8) \
    XX(ACCEPT_DATA, 1 << 9) \
    /* for user (16 ~ 31) */ \
    XX(USR1,  1 << 16) XX(USR2,  1 << 17) XX(USR3,  1 << 18) XX(USR4,  1 << 19) \
    XX(USR5,  1 << 20) XX(USR6,  1 << 21) XX(USR7,  1 << 22) XX(USR8,  1 << 23) \
    XX(USR9,  1 << 24) XX(USR10, 1 << 25) XX(USR11, 1 << 26) XX(USR12, 1 << 27) \
    XX(USR13, 1 << 28) XX(USR14, 1 << 29) XX(USR15, 1 << 30) XX(USR16, 1 << 31) \

typedef enum cat_coroutine_flag_e {
#define CAT_COROUTINE_FLAG_GEN(name, value) CAT_ENUM_GEN(CAT_COROUTINE_FLAG_, name, value)
    CAT_COROUTINE_FLAG_MAP(CAT_COROUTINE_FLAG_GEN)
#undef CAT_COROUTINE_FLAG_GEN
} cat_coroutine_flag_t;

typedef uint32_t cat_coroutine_flags_t;

#define CAT_COROUTINE_STATE_MAP(XX) \
    XX(NONE,    0, "none") \
    XX(WAITING, 1, "waiting") \
    XX(RUNNING, 2, "running") \
    XX(DEAD,    3, "dead") \

typedef enum cat_coroutine_state_e {
#define CAT_COROUTINE_STATE_GEN(name, value, unused) CAT_ENUM_GEN(CAT_COROUTINE_STATE_, name, value)
    CAT_COROUTINE_STATE_MAP(CAT_COROUTINE_STATE_GEN)
#undef CAT_COROUTINE_STATE_GEN
} cat_coroutine_state_t;

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
    /* persistent info (readonly) */
    cat_coroutine_id_t id;
    cat_msec_t start_time;
    cat_msec_t end_time;
    /* persistent/runtime flags */
    cat_coroutine_flags_t flags;
    /* runtime info (readonly) */
    cat_coroutine_state_t state;
    cat_coroutine_round_t round;
    cat_coroutine_t *from CAT_UNSAFE;
    cat_coroutine_t *previous;
    cat_coroutine_t *next;
    /* internal properties (readonly) */
    cat_coroutine_function_t function;
    cat_coroutine_stack_size_t stack_size;
    /* internal properties (inaccessible) */
#ifdef CAT_COROUTINE_USE_USER_STACK
    uint32_t virtual_memory_size;
    void *virtual_memory;
#endif
    cat_coroutine_context_t context;
#ifdef CAT_COROUTINE_USE_USER_TRANSFER_DATA
    cat_data_t *transfer_data;
#endif
#ifdef CAT_COROUTINE_USE_THREAD_CONTEXT
    uv_sem_t sem;
#endif
    /* ext info */
#ifdef CAT_HAVE_VALGRIND
    uint32_t valgrind_stack_id;
#endif
#ifdef CAT_COROUTINE_USE_ASAN
    void *asan_fake_stack;
    const void *asan_stack;
    size_t asan_stack_size;
#endif
};

typedef void (*cat_coroutine_jump_t)(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval);

typedef uint32_t cat_coroutine_count_t;
#define CAT_COROUTINE_COUNT_FMT "%u"
#define CAT_COROUTINE_COUNT_FMT_SPEC "u"

typedef void (*cat_coroutine_deadlock_callback_t)(void);

CAT_GLOBALS_STRUCT_BEGIN(cat_coroutine)
    /* options */
    cat_coroutine_stack_size_t default_stack_size;
    cat_log_type_t deadlock_log_type;
    cat_coroutine_deadlock_callback_t deadlock_callback;
    /* coroutines */
    cat_coroutine_t *current;
    cat_coroutine_t *main;
    cat_coroutine_t _main;
    /* scheduler */
    cat_coroutine_t *scheduler;
    cat_queue_t waiters;
    cat_coroutine_count_t waiter_count;
    /* functions */
    cat_coroutine_jump_t jump;
    cat_bool_t switch_denied;
    /* info */
    cat_coroutine_id_t last_id;
    cat_coroutine_count_t count;
    cat_coroutine_count_t peak_count;
    /* for watchdog */
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
CAT_API cat_coroutine_jump_t cat_coroutine_register_jump(cat_coroutine_jump_t jump);
CAT_API cat_bool_t cat_coroutine_switch_denied(void);
CAT_API void cat_coroutine_switch_deny(void);
CAT_API void cat_coroutine_switch_allow(void);
/* return the original main coroutine ptr */
CAT_API cat_coroutine_t *cat_coroutine_register_main(cat_coroutine_t *coroutine);
/* return the original stack size */
CAT_API cat_coroutine_stack_size_t cat_coroutine_set_default_stack_size(size_t size);
/* It is recommended to set to error or warning */
CAT_API cat_log_type_t cat_coroutine_set_deadlock_log_type(cat_log_type_t type);
/* callback will be called before deadlock() */
CAT_API cat_coroutine_deadlock_callback_t cat_coroutine_set_deadlock_callback(cat_coroutine_deadlock_callback_t callback);

/* globals */
CAT_API cat_coroutine_stack_size_t cat_coroutine_get_default_stack_size(void);
CAT_API cat_log_type_t cat_coroutine_get_deadlock_log_type(void);
CAT_API cat_coroutine_t *cat_coroutine_get_current(void);
CAT_API cat_coroutine_id_t cat_coroutine_get_current_id(void);
CAT_API cat_coroutine_t *cat_coroutine_get_main(void);
CAT_API cat_coroutine_t *cat_coroutine_get_scheduler(void);
CAT_API cat_coroutine_id_t cat_coroutine_get_last_id(void);
CAT_API cat_coroutine_count_t cat_coroutine_get_count(void);
CAT_API cat_coroutine_count_t cat_coroutine_get_real_count(void);
CAT_API cat_coroutine_count_t cat_coroutine_get_peak_count(void);
CAT_API cat_coroutine_round_t cat_coroutine_get_current_round(void);

/* ctor and dtor */
CAT_API cat_coroutine_t *cat_coroutine_create(cat_coroutine_t *coroutine, cat_coroutine_function_t function);
CAT_API cat_coroutine_t *cat_coroutine_create_ex(cat_coroutine_t *coroutine, cat_coroutine_function_t function, size_t stack_size);
CAT_API void cat_coroutine_free(cat_coroutine_t *coroutine);
/* Notice: unless you create a coroutine and never ran it, or you need not close coroutine by yourself */
CAT_API cat_bool_t cat_coroutine_close(cat_coroutine_t *coroutine);
/* switch (internal) */
#define cat_coroutine_jump CAT_COROUTINE_G(jump)
CAT_API void cat_coroutine_jump_standard(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval);
/* switch (external) */
CAT_API cat_bool_t cat_coroutine_resume(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval);
CAT_API cat_bool_t cat_coroutine_yield(cat_data_t *data, cat_data_t **retval);

/* properties */
CAT_API cat_coroutine_id_t cat_coroutine_get_id(const cat_coroutine_t *coroutine);
CAT_API cat_coroutine_t *cat_coroutine_get_from(const cat_coroutine_t *coroutine); CAT_UNSAFE
CAT_API cat_coroutine_t *cat_coroutine_get_previous(const cat_coroutine_t *coroutine);
CAT_API cat_coroutine_t *cat_coroutine_get_next(const cat_coroutine_t *coroutine);
CAT_API cat_coroutine_stack_size_t cat_coroutine_get_stack_size(const cat_coroutine_t *coroutine);

/* status */
CAT_API cat_bool_t cat_coroutine_is_available(const cat_coroutine_t *coroutine);
CAT_API cat_bool_t cat_coroutine_is_alive(const cat_coroutine_t *coroutine);
CAT_API cat_bool_t cat_coroutine_is_over(const cat_coroutine_t *coroutine);
CAT_API const char *cat_coroutine_state_name(cat_coroutine_state_t state);
CAT_API cat_coroutine_flags_t cat_coroutine_get_flags(const cat_coroutine_t *coroutine);
CAT_API void cat_coroutine_set_flags(cat_coroutine_t *coroutine, cat_coroutine_flags_t flags);
CAT_API cat_coroutine_state_t cat_coroutine_get_state(const cat_coroutine_t *coroutine);
CAT_API const char *cat_coroutine_get_state_name(const cat_coroutine_t *coroutine);
CAT_API cat_coroutine_round_t cat_coroutine_get_round(const cat_coroutine_t *coroutine);
CAT_API cat_msec_t cat_coroutine_get_start_time(const cat_coroutine_t *coroutine);
CAT_API cat_msec_t cat_coroutine_get_end_time(const cat_coroutine_t *coroutine);
CAT_API cat_msec_t cat_coroutine_get_elapsed(const cat_coroutine_t *coroutine);
CAT_API char *cat_coroutine_get_elapsed_as_string(const cat_coroutine_t *coroutine);

/* scheduler */
typedef void (*cat_coroutine_schedule_function_t)(void);
typedef void (*cat_coroutine_deadlock_function_t)(void);
typedef struct cat_coroutine_scheduler_s {
    cat_coroutine_schedule_function_t schedule;
    cat_coroutine_deadlock_function_t deadlock;
} cat_coroutine_scheduler_t;
CAT_API cat_coroutine_t *cat_coroutine_scheduler_run(cat_coroutine_t *coroutine, const cat_coroutine_scheduler_t *scheduler); CAT_INTERNAL
CAT_API cat_coroutine_t *cat_coroutine_scheduler_close(void); CAT_INTERNAL

static cat_always_inline cat_bool_t cat_coroutine__schedule(cat_coroutine_t *coroutine)
{
    cat_coroutine_t *current_coroutine = CAT_COROUTINE_G(current);
    cat_bool_t ret;
    current_coroutine->flags |= CAT_COROUTINE_FLAG_SCHEDULING;
    ret = cat_coroutine_resume(coroutine, NULL, NULL);
    current_coroutine->flags ^= CAT_COROUTINE_FLAG_SCHEDULING;
    return ret;
}

#define cat_coroutine_schedule(coroutine, module_type, name, ...) do { \
    if (unlikely(!cat_coroutine__schedule(coroutine))) { \
        CAT_CORE_ERROR_WITH_LAST(module_type, name " schedule failed", ##__VA_ARGS__); \
    } \
} while (0)

/* sync */
CAT_API cat_bool_t cat_coroutine_wait_all(void);
CAT_API cat_bool_t cat_coroutine_wait_all_ex(cat_timeout_t timeout);
CAT_API void cat_coroutine_notify_all(void); CAT_INTERNAL

/* special */
/* main/scheduler/none */
CAT_API const char *cat_coroutine_get_role_name(const cat_coroutine_t *coroutine);
CAT_API const char *cat_coroutine_get_current_role_name(void);

/* helper */
CAT_API cat_coroutine_t *cat_coroutine_run(cat_coroutine_t *coroutine, cat_coroutine_function_t function, cat_data_t *data);

#ifdef __cplusplus
}
#endif
#endif /* CAT_COROUTINE_H */
