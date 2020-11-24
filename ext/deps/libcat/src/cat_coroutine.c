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

#include "cat_coroutine.h"
#include "cat_time.h"

#if defined(CAT_OS_UNIX_LIKE) && defined(CAT_DEBUG)
#include <sys/mman.h>
#ifdef PROT_NONE
#define CAT_COROUTINE_USE_MPEOTECT
#endif
#endif

#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#endif

/* context */

#ifdef CAT_COROUTINE_USE_UCONTEXT
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

typedef struct
{
    cat_coroutine_context_t from_context;
    cat_data_t *data;
} cat_coroutine_transfer_t;

typedef void (*cat_coroutine_context_function_t)(cat_coroutine_transfer_t transfer);

cat_coroutine_context_t cat_coroutine_context_make(cat_coroutine_stack_t *stack, size_t stack_size, cat_coroutine_context_function_t function);
cat_coroutine_transfer_t cat_coroutine_context_jump(cat_coroutine_context_t const target_context, cat_data_t *transfer_data);
#endif

/* coroutine */

static cat_coroutine_stack_size_t cat_coroutine_align_stack_size(size_t size)
{
    if (size == 0) {
        size = CAT_COROUTINE_G(default_stack_size);
    } else if (unlikely(size < CAT_COROUTINE_MIN_STACK_SIZE)) {
        size = CAT_COROUTINE_MIN_STACK_SIZE;
    } else if (unlikely(size > CAT_COROUTINE_MAX_STACK_SIZE)) {
        size = CAT_COROUTINE_MAX_STACK_SIZE;
    } else {
        size = CAT_MEMORY_ALIGNED_SIZE_EX(size, CAT_COROUTINE_STACK_ALIGNED_SIZE);
    }

    return size;
}

CAT_API CAT_GLOBALS_DECLARE(cat_coroutine)

CAT_GLOBALS_CTOR_DECLARE_SZ(cat_coroutine)

CAT_API cat_bool_t cat_coroutine_module_init(void)
{
    CAT_GLOBALS_REGISTER(cat_coroutine, CAT_GLOBALS_CTOR(cat_coroutine), NULL);
    return cat_true;
}

CAT_API cat_bool_t cat_coroutine_runtime_init(void)
{
    /* register coroutine resume */
    cat_coroutine_register_resume(cat_coroutine_resume_standard);

    /* init options */
    cat_coroutine_set_default_stack_size(CAT_COROUTINE_RECOMMENDED_STACK_SIZE);
    cat_coroutine_set_dead_lock_log_type(CAT_LOG_TYPE_WARNING);

    /* init info */
    CAT_COROUTINE_G(last_id) = 0;
    CAT_COROUTINE_G(count) = 0;
    CAT_COROUTINE_G(peak_count) = 0;
    CAT_COROUTINE_G(round) = 0;

    /* init main coroutine properties */
    do {
        cat_coroutine_t *main_coroutine = &CAT_COROUTINE_G(_main);
        main_coroutine->id = CAT_COROUTINE_MAIN_ID;
        main_coroutine->start_time = cat_time_msec();
        main_coroutine->flags = CAT_COROUTINE_FLAG_NONE;
        main_coroutine->state = CAT_COROUTINE_STATE_RUNNING;
        main_coroutine->opcodes = CAT_COROUTINE_OPCODE_NONE;
        main_coroutine->round = ++CAT_COROUTINE_G(round);
        main_coroutine->from = NULL;
        main_coroutine->previous = NULL;
        main_coroutine->stack = NULL;
        main_coroutine->stack_size = 0;
        memset(&main_coroutine->context, ~0, sizeof(cat_coroutine_context_t));
        main_coroutine->function = NULL;
#ifdef CAT_COROUTINE_USE_UCONTEXT
        main_coroutine->transfer_data = NULL;
#endif
#ifdef HAVE_VALGRIND
        main_coroutine->valgrind_stack_id = UINT32_MAX;
#endif
        CAT_COROUTINE_G(main) = main_coroutine;
        CAT_COROUTINE_G(current) = main_coroutine;

        CAT_COROUTINE_G(last_id) = main_coroutine->id + 1;
        CAT_COROUTINE_G(count)++;
        CAT_COROUTINE_G(peak_count)++;
    } while (0);

    /* scheduler */
    CAT_COROUTINE_G(scheduler) = NULL;
    cat_queue_init(&CAT_COROUTINE_G(waiters));

    return cat_true;
}

CAT_API cat_bool_t cat_coroutine_runtime_shutdown(void)
{
    CAT_ASSERT(cat_queue_empty(&CAT_COROUTINE_G(waiters)) && "Coroutine waiter should be empty");
    CAT_ASSERT(cat_coroutine_get_scheduler() == NULL && "Coroutine scheduler should have been stopped");
    CAT_ASSERT(CAT_COROUTINE_G(count) == 1 && "Coroutine count should be 1");

    return cat_true;
}

CAT_API cat_coroutine_stack_size_t cat_coroutine_set_default_stack_size(size_t size)
{
    cat_coroutine_stack_size_t original_size = CAT_COROUTINE_G(default_stack_size);
    CAT_COROUTINE_G(default_stack_size) = cat_coroutine_align_stack_size(size);
    return original_size;
}

CAT_API cat_bool_t cat_coroutine_set_dead_lock_log_type(cat_log_type_t type)
{
    CAT_COROUTINE_G(dead_lock_log_type) = type;
    return cat_true;
}

CAT_API cat_coroutine_resume_t cat_coroutine_register_resume(cat_coroutine_resume_t resume)
{
    cat_coroutine_resume_t origin_resume = cat_coroutine_resume;
    cat_coroutine_resume = resume;
    return origin_resume;
}

CAT_API cat_coroutine_t *cat_coroutine_register_main(cat_coroutine_t *coroutine)
{
    cat_coroutine_t *original_main = CAT_COROUTINE_G(main);

    if (original_main != coroutine) {
        if (original_main != NULL) {
            memcpy(coroutine, original_main, sizeof(*coroutine));
        }
        CAT_COROUTINE_G(main) = coroutine;
        if (original_main == CAT_COROUTINE_G(current)) {
            CAT_COROUTINE_G(current) = coroutine;
        }
    }

    return original_main;
}

/* globals */
CAT_API cat_coroutine_stack_size_t cat_coroutine_get_default_stack_size(void)
{
    return CAT_COROUTINE_G(default_stack_size);
}

CAT_API cat_log_type_t cat_coroutine_get_dead_lock_log_type(void)
{
    return CAT_COROUTINE_G(dead_lock_log_type);
}

CAT_API cat_coroutine_t *cat_coroutine_get_current(void)
{
    return CAT_COROUTINE_G(current);
}

CAT_API cat_coroutine_id_t cat_coroutine_get_current_id(void)
{
    return CAT_COROUTINE_G(current)->id;
}

CAT_API cat_coroutine_t *cat_coroutine_get_by_index(cat_coroutine_count_t index)
{
    cat_coroutine_t *coroutine = CAT_COROUTINE_G(current);
    cat_coroutine_count_t count = 0;

    while (coroutine->previous != NULL) {
        count++;
        coroutine = coroutine->previous;
    }

    if (index != 0) {
        /* overflow */
        if (index > count) {
            return NULL;
        }
        /* re-loop */
        coroutine = CAT_COROUTINE_G(current);
        count -= index;
        while (count--) {
            coroutine = coroutine->previous;
        }
    }

    return coroutine;
}

CAT_API cat_coroutine_t *cat_coroutine_get_root(void)
{
    return cat_coroutine_get_by_index(0);
}

CAT_API cat_coroutine_t *cat_coroutine_get_main(void)
{
    return CAT_COROUTINE_G(main);
}

CAT_API cat_coroutine_t *cat_coroutine_get_scheduler(void)
{
    return CAT_COROUTINE_G(scheduler);
}

CAT_API cat_coroutine_id_t cat_coroutine_get_last_id(void)
{
    return CAT_COROUTINE_G(last_id);
}

CAT_API cat_coroutine_count_t cat_coroutine_get_count(void)
{
    return CAT_COROUTINE_G(count);
}

CAT_API cat_coroutine_count_t cat_coroutine_get_real_count(void)
{
    cat_coroutine_count_t count = CAT_COROUTINE_G(count);

    if (CAT_COROUTINE_G(scheduler) != NULL) {
        count++;
    }

    return count;
}

CAT_API cat_coroutine_count_t cat_coroutine_get_peak_count(void)
{
    return CAT_COROUTINE_G(peak_count);
}

CAT_API cat_coroutine_round_t cat_coroutine_get_current_round(void)
{
    return CAT_COROUTINE_G(round);
}

static void cat_coroutine_context_function(cat_coroutine_transfer_t transfer)
{
    cat_coroutine_t *coroutine = CAT_COROUTINE_G(current);
    if (unlikely(++CAT_COROUTINE_G(count) > CAT_COROUTINE_G(peak_count))) {
        CAT_COROUTINE_G(peak_count) = CAT_COROUTINE_G(count);
    }
    cat_debug(COROUTINE, "Start (count=" CAT_COROUTINE_COUNT_FMT ")", CAT_COROUTINE_G(count));
#ifdef CAT_COROUTINE_USE_UCONTEXT
    CAT_ASSERT(transfer.data == NULL);
    transfer.data = coroutine->transfer_data;
#else
    /* update origin's context */
    coroutine->from->context = transfer.from_context;
#endif
    /* start time */
    coroutine->start_time = cat_time_msec();
    /* execute function */
    transfer.data = coroutine->function(transfer.data);
    /* is finished */
    coroutine->state = CAT_COROUTINE_STATE_FINISHED;
    /* finished */
    CAT_COROUTINE_G(count)--;
    cat_debug(COROUTINE, "Finished (count=" CAT_COROUTINE_COUNT_FMT ")", CAT_COROUTINE_G(count));
    /* yield to previous */
    cat_coroutine_yield(transfer.data, NULL);
    /* never here */
    CAT_NEVER_HERE("Coroutine is dead");
}

CAT_API void cat_coroutine_init(cat_coroutine_t *coroutine)
{
    coroutine->state = CAT_COROUTINE_STATE_INIT;
}

CAT_API cat_coroutine_t *cat_coroutine_create(cat_coroutine_t *coroutine, cat_coroutine_function_t function)
{
    return cat_coroutine_create_ex(coroutine, function, 0);
}

CAT_API cat_coroutine_t *cat_coroutine_create_ex(cat_coroutine_t *coroutine, cat_coroutine_function_t function, size_t stack_size)
{
    cat_coroutine_stack_t *stack, *stack_end;
    size_t real_stack_size;
    cat_coroutine_context_t context;

    /* align stack size */
    stack_size = cat_coroutine_align_stack_size(stack_size);
    /* alloc memory */
    stack = (cat_coroutine_stack_t *) cat_sys_malloc(stack_size);
    if (unlikely(!stack)) {
        cat_update_last_error_of_syscall("Malloc for stack failed with size %zu", stack_size);
        return NULL;
    }
    /* calculations */
    real_stack_size = coroutine ? stack_size : (stack_size - sizeof(cat_coroutine_t));
    stack_end = (cat_coroutine_stack_t *) (((char *) stack) + real_stack_size);
    /* determine the position of the coroutine */
    if (coroutine == NULL) {
        coroutine = (cat_coroutine_t *) stack_end;
    }
    /* make context */
#ifdef CAT_COROUTINE_USE_UCONTEXT
    if (unlikely(getcontext(&context) == -1)) {
        cat_sys_free(stack);
        cat_update_last_error_ez("Ucontext getcontext failed");
        return NULL;
    }
    context.uc_stack.ss_sp = stack;
    context.uc_stack.ss_size = available_size;
    context.uc_stack.ss_flags = 0;
    context.uc_link = NULL;
    cat_coroutine_context_make(&context, (void (*)(void)) &cat_coroutine_context_function, 1, NULL);
#else
    context = cat_coroutine_context_make((void *) stack_end, real_stack_size, cat_coroutine_context_function);
#endif
    /* protect stack */
#ifdef CAT_COROUTINE_USE_MPEOTECT
    if (unlikely(mprotect(cat_getpageof(stack) + cat_getpagesize(), cat_getpagesize(), PROT_NONE) != 0)) {
        cat_syscall_failure(NOTICE, COROUTINE, "Protect stack failed");
    }
#endif
    /* init coroutine properties */
    coroutine->id = CAT_COROUTINE_G(last_id)++;
    coroutine->flags = CAT_COROUTINE_FLAG_NONE;
    coroutine->state = CAT_COROUTINE_STATE_READY;
    coroutine->opcodes = CAT_COROUTINE_OPCODE_NONE;
    coroutine->round = 0;
    coroutine->from = NULL;
    coroutine->previous = NULL;
    coroutine->start_time = 0;
    coroutine->stack = stack;
    coroutine->stack_size = stack_size;
    coroutine->context = context;
    coroutine->function = function;
#ifdef CAT_COROUTINE_USE_UCONTEXT
    coroutine->transfer_data = NULL;
#endif
#ifdef HAVE_VALGRIND
    coroutine->valgrind_stack_id = VALGRIND_STACK_REGISTER(stack_end, stack);
#endif
    cat_debug(COROUTINE, "Create R" CAT_COROUTINE_ID_FMT " with stack = %p, stack_size = %zu, function = %p on the %s",
                        coroutine->id, stack, stack_size, function, ((void *) coroutine) == ((void*) stack_end) ? "stack" : "heap");

    return coroutine;
}

CAT_API void cat_coroutine_close(cat_coroutine_t *coroutine)
{
    cat_coroutine_stack_t *stack = coroutine->stack;

    CAT_ASSERT(stack != NULL && "Coroutine is unready or closed");
    CAT_ASSERT(!cat_coroutine_is_alive(coroutine) && "Coroutine should not be active");

#ifdef HAVE_VALGRIND
    VALGRIND_STACK_DEREGISTER(coroutine->valgrind_stack_id);
#endif
#ifdef CAT_COROUTINE_USE_MPEOTECT
    if (unlikely(mprotect(cat_getpageof(stack) + cat_getpagesize(), cat_getpagesize(), PROT_READ | PROT_WRITE) != 0)) {
        cat_syscall_failure(NOTICE, COROUTINE, "Unprotect stack failed");
    }
#endif
    /* fast free */
    coroutine->state = CAT_COROUTINE_STATE_DEAD;
    coroutine->stack = NULL;
    cat_sys_free(stack);
}

CAT_API cat_data_t *cat_coroutine_jump(cat_coroutine_t *coroutine, cat_data_t *data)
{
    cat_coroutine_t *current_coroutine = CAT_COROUTINE_G(current);

    cat_debug(COROUTINE, "Jump to R" CAT_COROUTINE_ID_FMT, coroutine->id);
    /* update from */
    coroutine->from = current_coroutine;
    /* solve origin */
    if (current_coroutine->previous == coroutine) {
        /* if it is yield, update current state to waiting */
        if (current_coroutine->state == CAT_COROUTINE_STATE_RUNNING) {
            /* maybe locked or finished */
            current_coroutine->state = CAT_COROUTINE_STATE_WAITING;
        }
        /* break the origin */
        current_coroutine->previous = NULL;
    } else {
        /* it is not yield */
        CAT_ASSERT(coroutine->previous == NULL);
        /* current becomes target's origin */
        coroutine->previous = current_coroutine;
    }
    /* swap ptr */
    CAT_COROUTINE_G(current) = coroutine;
    /* update state */
    coroutine->state = CAT_COROUTINE_STATE_RUNNING;
    /* reset the opcode */
    coroutine->opcodes = CAT_COROUTINE_OPCODE_NONE;
    /* round++ */
    coroutine->round = ++CAT_COROUTINE_G(round);
    /* jump */
#ifdef CAT_COROUTINE_USE_UCONTEXT
    coroutine->transfer_data = data;
    cat_coroutine_context_jump(&current_coroutine->context, &coroutine->context);
    data = current_coroutine->transfer_data;
#else
    cat_coroutine_transfer_t transfer = cat_coroutine_context_jump(coroutine->context, data);
    data = transfer.data;
#endif
    /* handle from */
    coroutine = current_coroutine->from;
    CAT_ASSERT(coroutine != NULL);
#ifndef CAT_COROUTINE_USE_UCONTEXT
    /* update the from context */
    coroutine->context = transfer.from_context;
#endif
    /* close the coroutine if it is finished */
    if (unlikely(coroutine->state == CAT_COROUTINE_STATE_FINISHED)) {
        cat_coroutine_close(coroutine);
    }

    return data;
}

CAT_API cat_bool_t cat_coroutine_is_resumable(const cat_coroutine_t *coroutine)
{
    cat_coroutine_t *current_coroutine = CAT_COROUTINE_G(current);

    if (coroutine == current_coroutine->previous) {
        return cat_true;
    }

    switch (coroutine->state) {
        case CAT_COROUTINE_STATE_WAITING:
        case CAT_COROUTINE_STATE_READY:
            break;
        case CAT_COROUTINE_STATE_RUNNING:
            cat_update_last_error(CAT_EBUSY, "Coroutine is running");
            return cat_false;
        case CAT_COROUTINE_STATE_LOCKED:
            cat_update_last_error(CAT_ELOCKED, "Coroutine is locked");
            return cat_false;
        default:
            cat_update_last_error(CAT_ESRCH, "Coroutine is not available");
            return cat_false;
    }

    if (unlikely((coroutine->opcodes & CAT_COROUTINE_OPCODE_WAIT) &&
                 (current_coroutine != coroutine->waiter.coroutine))) {
        cat_update_last_error(CAT_EAGAIN, "Coroutine is waiting for someone else");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_coroutine_resume_standard(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval)
{
    if (likely(!(coroutine->opcodes & CAT_COROUTINE_OPCODE_CHECKED))) {
        if (unlikely(!cat_coroutine_is_resumable(coroutine))) {
            return cat_false;
        }
    }

    data = cat_coroutine_jump(coroutine, data);

    if (retval != NULL) {
        *retval = data;
    } else {
        CAT_ASSERT(data == NULL && "Unexpected non-empty data, resource may leak");
    }

    return cat_true;
}

CAT_API cat_bool_t cat_coroutine_yield(cat_data_t *data, cat_data_t **retval)
{
    cat_coroutine_t *coroutine = CAT_COROUTINE_G(current)->previous;
    cat_bool_t ret;

    if (unlikely(coroutine == NULL)) {
        cat_update_last_error(CAT_EMISUSE, "Coroutine has nowhere to go");
        return cat_false;
    }

    ret = cat_coroutine_resume(coroutine, data, retval);

    CAT_ASSERT(ret && "Yield never fail");

    return ret;
}

/* properties */
CAT_API cat_coroutine_id_t cat_coroutine_get_id(const cat_coroutine_t *coroutine)
{
    return coroutine->id;
}

CAT_API cat_coroutine_t *cat_coroutine_get_from(const cat_coroutine_t *coroutine)
{
    return coroutine->from;
}

CAT_API cat_coroutine_t *cat_coroutine_get_previous(const cat_coroutine_t *coroutine)
{
    return coroutine->previous;
}

CAT_API cat_coroutine_stack_size_t cat_coroutine_get_stack_size(const cat_coroutine_t *coroutine)
{
    return coroutine->stack_size;
}

/* status */

CAT_API cat_bool_t cat_coroutine_is_available(const cat_coroutine_t *coroutine)
{
    return coroutine->state < CAT_COROUTINE_STATE_LOCKED && coroutine->state > CAT_COROUTINE_STATE_INIT;
}

CAT_API cat_bool_t cat_coroutine_is_alive(const cat_coroutine_t *coroutine)
{
    return coroutine->state < CAT_COROUTINE_STATE_LOCKED && coroutine->state > CAT_COROUTINE_STATE_READY;
}

CAT_API cat_bool_t cat_coroutine_is_over(const cat_coroutine_t *coroutine)
{
    return coroutine->state >= CAT_COROUTINE_STATE_FINISHED;
}

CAT_API const char *cat_coroutine_state_name(cat_coroutine_state_t state)
{
    switch (state) {
#define CAT_COROUTINE_STATE_NAME_GEN(name, unused, value) case CAT_COROUTINE_STATE_##name: return value;
    CAT_COROUTINE_STATE_MAP(CAT_COROUTINE_STATE_NAME_GEN)
#undef CAT_COROUTINE_STATE_NAME_GEN
    }
    CAT_NEVER_HERE("Unknown state");
}

CAT_API cat_coroutine_state_t cat_coroutine_get_state(const cat_coroutine_t *coroutine)
{
    return coroutine->state;
}

CAT_API const char *cat_coroutine_get_state_name(const cat_coroutine_t *coroutine)
{
    return cat_coroutine_state_name(coroutine->state);
}

CAT_API cat_coroutine_opcodes_t cat_coroutine_get_opcodes(const cat_coroutine_t *coroutine)
{
    return coroutine->opcodes;
}

CAT_API void cat_coroutine_set_opcodes(cat_coroutine_t *coroutine, cat_coroutine_opcodes_t opcodes)
{
    coroutine->opcodes = opcodes;
}

CAT_API cat_coroutine_round_t cat_coroutine_get_round(const cat_coroutine_t *coroutine)
{
    return coroutine->round;
}

CAT_API cat_msec_t cat_coroutine_get_start_time(const cat_coroutine_t *coroutine)
{
    return coroutine->start_time;
}

CAT_API cat_msec_t cat_coroutine_get_elapsed(const cat_coroutine_t *coroutine)
{
    if (unlikely(coroutine->state < CAT_COROUTINE_STATE_RUNNING)) {
        return 0;
    }
    return cat_time_msec() - coroutine->start_time;
}

CAT_API char *cat_coroutine_get_elapsed_as_string(const cat_coroutine_t *coroutine)
{
    return cat_time_format_msec(cat_coroutine_get_elapsed(coroutine));
}

/* scheduler */

static void cat_coroutine_dead_lock(cat_coroutine_dead_lock_function_t dead_lock)
{
    cat_log_type_t type = CAT_COROUTINE_G(dead_lock_log_type);

    cat_log_with_type(type, COROUTINE, CAT_EDEADLK, "Dead lock: all coroutines are asleep");

    if (dead_lock != NULL) {
        dead_lock();
    } else while (usleep(60 * 1000 * 1000) == 0);
}

static cat_data_t *cat_coroutine_scheduler_function(cat_data_t *data)
{
    cat_coroutine_t *coroutine = CAT_COROUTINE_G(current);
    cat_coroutine_scheduler_t scheduler = *((cat_coroutine_scheduler_t *) data);

    CAT_COROUTINE_G(scheduler) = coroutine;
    CAT_COROUTINE_G(count)--;

    cat_coroutine_yield(NULL, NULL);

    while (coroutine == cat_coroutine_get_root()) {

        scheduler.schedule();

        if (CAT_COROUTINE_G(count) > 0) {
            /* we expect everything is done,
             * but there are still coroutines that have not finished
             * so we try to trigger the dead lock */
            cat_coroutine_dead_lock(scheduler.dead_lock);
            /* dead lock failed or it was broken by some magic ways */
            continue;
        }

        /* notify waiters */
        cat_coroutine_notify_all();
    }

    CAT_COROUTINE_G(count)++;
    CAT_COROUTINE_G(scheduler) = NULL;

    return NULL;
}

CAT_API cat_coroutine_t *cat_coroutine_scheduler_run(cat_coroutine_t *coroutine, const cat_coroutine_scheduler_t *scheduler)
{
    if (CAT_COROUTINE_G(scheduler) != NULL) {
        cat_update_last_error(CAT_EMISUSE, "Only one scheduler coroutine is allowed in the same thread");
        return NULL;
    }

    /* let scheduler id be 0 */
    do {
        cat_coroutine_id_t last_id = CAT_COROUTINE_G(last_id);
        CAT_COROUTINE_G(last_id) = 0;
        coroutine = cat_coroutine_create(coroutine, (cat_data_t *) scheduler);
        CAT_COROUTINE_G(last_id) = last_id;
    } while (0);

    if (coroutine == NULL) {
        cat_update_last_error_with_previous("Create event scheduler failed");
        return NULL;
    }

    /* hook scheduler function */
    do {
        cat_coroutine_function_t function = coroutine->function;
        coroutine->function = cat_coroutine_scheduler_function;
        (void) cat_coroutine_resume(coroutine, function, NULL);
        CAT_ASSERT(cat_coroutine_is_alive(coroutine));
    } while (0);

    CAT_ASSERT(CAT_COROUTINE_G(scheduler) != NULL);

    /* let it be in running state */
    coroutine->state = CAT_COROUTINE_STATE_RUNNING;
    /* let it be the new root */
    cat_coroutine_get_root()->previous = coroutine;

    return coroutine;
}

CAT_API cat_coroutine_t *cat_coroutine_scheduler_close(void)
{
    cat_coroutine_t *coroutine = CAT_COROUTINE_G(scheduler);

    if (coroutine == NULL) {
        cat_update_last_error(CAT_EMISUSE, "No scheduler is available");
        return NULL;
    }

    cat_coroutine_wait();

    /* remove it from the root */
    cat_coroutine_get_by_index(1)->previous = NULL;
    /* let it be in waiting state
     * (TODO: remove this line if we do not do check before internal resume ?) */
    coroutine->state = CAT_COROUTINE_STATE_WAITING;

    (void) cat_coroutine_resume(coroutine, NULL, NULL);

    CAT_ASSERT(CAT_COROUTINE_G(scheduler) == NULL);

    return coroutine;
}

CAT_API cat_bool_t cat_coroutine_wait(void)
{
    cat_bool_t ret;

    cat_queue_push_back(&CAT_COROUTINE_G(waiters), &CAT_COROUTINE_G(current)->waiter.node);

    /* usually, it will be unlocked by event scheduler */
    ret = cat_coroutine_lock();

    cat_queue_remove(&CAT_COROUTINE_G(current)->waiter.node);

    if (!ret) {
        cat_update_last_error_with_previous("Wait lock failed");
        return cat_false;
    }

    return cat_true;
}

CAT_API void cat_coroutine_notify_all(void)
{
    cat_queue_t *waiters = &CAT_COROUTINE_G(waiters);
    cat_coroutine_t *coroutine;

    while ((coroutine = cat_queue_front_data(waiters, cat_coroutine_t, waiter.node))) {
        cat_bool_t ret;

        ret = cat_coroutine_unlock(coroutine);

        if (!ret) {
            cat_core_error_with_last(COROUTINE, "Notify failed");
        }
    }
}

/* special */

CAT_API cat_bool_t cat_coroutine_wait_for(cat_coroutine_t *who)
{
    cat_coroutine_t *current_coroutine = CAT_COROUTINE_G(current);
    cat_bool_t ret;

    current_coroutine->opcodes |= CAT_COROUTINE_OPCODE_WAIT;
    current_coroutine->waiter.coroutine = who;

    ret = cat_coroutine_yield(NULL, NULL);

    if (unlikely(!ret)) {
        current_coroutine->opcodes ^= CAT_COROUTINE_OPCODE_WAIT;
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_coroutine_lock(void)
{
    cat_coroutine_t *coroutine = CAT_COROUTINE_G(current);
    cat_bool_t ret;

    coroutine->state = CAT_COROUTINE_STATE_LOCKED;
    CAT_COROUTINE_G(count)--;

    ret = cat_coroutine_yield(NULL, NULL);

    CAT_COROUTINE_G(count)++;
    coroutine->state = CAT_COROUTINE_STATE_WAITING;

    if (!ret) {
        cat_update_last_error_with_previous("Lock coroutine failed");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_coroutine_unlock(cat_coroutine_t *coroutine)
{
    cat_bool_t ret;

    if (coroutine->state != CAT_COROUTINE_STATE_LOCKED) {
        cat_update_last_error(CAT_EINVAL, "Unlock an unlocked coroutine");
        return cat_false;
    }

    coroutine->opcodes |= CAT_COROUTINE_OPCODE_CHECKED;

    ret = cat_coroutine_resume(coroutine, NULL, NULL);

    CAT_ASSERT(ret && "Unlock never fail");

    return cat_true;
}

/* helper */

CAT_API cat_coroutine_t *cat_coroutine_run(cat_coroutine_t *coroutine, cat_coroutine_function_t function, cat_data_t *data)
{
    cat_bool_t ret;

    coroutine = cat_coroutine_create(NULL, function);

    if (unlikely(coroutine == NULL)) {
        return NULL;
    }

    ret = cat_coroutine_resume(coroutine, data, NULL);

    if (unlikely(!ret)) {
        cat_coroutine_close(coroutine);
        return NULL;
    }

    return coroutine;
}
