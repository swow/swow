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

/* Note: ASan can not work well with mmap()/VirtualAlloc(),
 * Using sys_malloc() so we can get better memory log.
 * and on OpenBSD we must use mmap() with MAP_STACK */
#ifndef CAT_COROUTINE_USE_THREAD_CONTEXT
# if defined(CAT_COROUTINE_USE_ASAN) && !defined(__OpenBSD__)
#  define CAT_COROUTINE_USE_SYS_MALLOC 1
# elif !defined(CAT_OS_WIN)
#  define CAT_COROUTINE_USE_MMAP 1
# else
#  define CAT_COROUTINE_USE_VIRTUAL_ALLOC 1
# endif
#endif

/** Note: LeakSanitizer will access protected page
 * if memleak ocurred (e.g. fatal error),
 * so we can not protect memory if ASan is enabled */
#if defined(CAT_DEBUG) && !defined(CAT_COROUTINE_USE_ASAN) && defined(CAT_COROUTINE_USE_USER_STACK)
# define CAT_COROUTINE_USE_MEMORY_PROTECT 1
#endif

#if defined(CAT_OS_UNIX_LIKE) && (defined(CAT_COROUTINE_USE_MMAP) || defined(CAT_COROUTINE_USE_MEMORY_PROTECT))
# include <sys/mman.h> /* for mmap()/mprotect() */
#endif

#ifdef CAT_COROUTINE_USE_MMAP
# if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#  define MAP_ANONYMOUS MAP_ANON
# endif
/* FreeBSD require a first (i.e. addr) argument of mmap(2) is not NULL
* if MAP_STACK is passed.
* http://www.FreeBSD.org/cgi/query-pr.cgi?pr=158755 */
# if !defined(MAP_STACK) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#  undef MAP_STACK
#  define MAP_STACK 0
# endif
# ifndef MAP_FAILED
#  define MAP_FAILED ((void * ) -1)
# endif
# define CAT_COROUTINE_MEMORY_INVALID MAP_FAILED
#elif defined(CAT_COROUTINE_USE_SYS_MALLOC) || defined(CAT_COROUTINE_USE_VIRTUAL_ALLOC)
# define CAT_COROUTINE_MEMORY_INVALID NULL
#endif

#ifdef CAT_COROUTINE_USE_MEMORY_PROTECT
# ifdef CAT_COROUTINE_USE_SYS_MALLOC
#  define CAT_COROUTINE_STACK_PADDING_PAGE_COUNT 2
# else
#  define CAT_COROUTINE_STACK_PADDING_PAGE_COUNT 1
# endif
# else
# define CAT_COROUTINE_STACK_PADDING_PAGE_COUNT  0
#endif

#ifdef CAT_HAVE_VALGRIND
# include <valgrind/valgrind.h>
#endif

#ifdef CAT_COROUTINE_USE_ASAN
// for warning -Wstrict-prototypes/C4255
# define __sanitizer_acquire_crash_state() __sanitizer_acquire_crash_state(void)
# ifndef _MSC_VER
#  include <sanitizer/common_interface_defs.h>
# else // workaround
#  include <../crt/src/sanitizer/common_interface_defs.h>
# endif
#endif

/* context */

#if defined(CAT_COROUTINE_USE_THREAD_CONTEXT)

typedef struct cat_coroutine_transfer_s {
    cat_coroutine_t *coroutine;
} cat_coroutine_transfer_t;

#elif defined(CAT_COROUTINE_USE_UCONTEXT)

typedef struct cat_coroutine_transfer_s {
    cat_data_t *data;
} cat_coroutine_transfer_t;

#define cat_coroutine_context_make(ucontext, function, argc, ...) \
        makecontext(ucontext, function, argc, ##__VA_ARGS__)

#define cat_coroutine_context_jump(current_ucontext, ucontext)    do { \
    if (unlikely(swapcontext(current_ucontext, ucontext) != 0)) { \
        CAT_CORE_ERROR(COROUTINE, "Ucontext swapcontext failed"); \
    } \
} while (0)

#else // defined(CAT_COROUTINE_USE_BOOST_CONTEXT)

typedef struct cat_coroutine_transfer_s {
    cat_coroutine_context_t from_context;
    cat_data_t *data;
} cat_coroutine_transfer_t;

typedef void (*cat_coroutine_context_function_t)(cat_coroutine_transfer_t transfer);

cat_coroutine_context_t cat_coroutine_context_make(void *stack, size_t stack_size, cat_coroutine_context_function_t function);
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
        size_t alignment = cat_getpagesize();
        size = CAT_MEMORY_ALIGNED_SIZE_EX(size, alignment);
    }

    return (cat_coroutine_stack_size_t) size;
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
    cat_coroutine_register_jump(cat_coroutine_jump_standard);
    CAT_COROUTINE_G(switch_denied) = cat_false;

    /* init options */
    cat_coroutine_set_default_stack_size(CAT_COROUTINE_RECOMMENDED_STACK_SIZE);
    cat_coroutine_set_deadlock_log_type(CAT_LOG_TYPE_WARNING);
    cat_coroutine_set_deadlock_callback(NULL);

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
        main_coroutine->end_time = 0;
        main_coroutine->flags = CAT_COROUTINE_FLAG_NONE;
        main_coroutine->state = CAT_COROUTINE_STATE_RUNNING;
        main_coroutine->round = ++CAT_COROUTINE_G(round);
        main_coroutine->from = NULL;
        main_coroutine->previous = NULL;
        main_coroutine->next = NULL;
        main_coroutine->stack_size = 0;
        main_coroutine->function = NULL;
#ifdef CAT_COROUTINE_USE_USER_STACK
        main_coroutine->virtual_memory = NULL;
        main_coroutine->virtual_memory_size = 0;
        memset(&main_coroutine->context, 0, sizeof(cat_coroutine_context_t));
#endif
#ifdef CAT_COROUTINE_USE_USER_TRANSFER_DATA
        main_coroutine->transfer_data = NULL;
#endif
#ifdef CAT_COROUTINE_USE_THREAD_CONTEXT
        CAT_SHOULD_BE(uv_sem_init(&main_coroutine->sem, 0) == 0);
#endif
#ifdef CAT_HAVE_VALGRIND
        main_coroutine->valgrind_stack_id = UINT32_MAX;
#endif
#ifdef CAT_COROUTINE_USE_ASAN
        main_coroutine->asan_fake_stack = NULL;
        main_coroutine->asan_stack = NULL;
        main_coroutine->asan_stack_size = 0;
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
    CAT_COROUTINE_G(waiter_count) = 0;

    return cat_true;
}

CAT_API cat_bool_t cat_coroutine_runtime_shutdown(void)
{
    /* For the non-scheduler mode */
    cat_coroutine_notify_all();

    CAT_ASSERT(cat_queue_empty(&CAT_COROUTINE_G(waiters)) && CAT_COROUTINE_G(waiter_count) == 0 &&
        "Coroutine waiter should be empty");
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

CAT_API cat_log_type_t cat_coroutine_set_deadlock_log_type(cat_log_type_t type)
{
    cat_log_type_t original_type = CAT_COROUTINE_G(deadlock_log_type);
    CAT_COROUTINE_G(deadlock_log_type) = type;
    return original_type;
}

CAT_API cat_coroutine_deadlock_callback_t cat_coroutine_set_deadlock_callback(cat_coroutine_deadlock_callback_t callback)
{
    cat_coroutine_deadlock_callback_t original_callback = CAT_COROUTINE_G(deadlock_callback);
    CAT_COROUTINE_G(deadlock_callback) = callback;
    return original_callback;
}

CAT_API cat_coroutine_jump_t cat_coroutine_register_jump(cat_coroutine_jump_t jump)
{
    cat_coroutine_jump_t original_jump = cat_coroutine_jump;
    cat_coroutine_jump = jump;
    return original_jump;
}

CAT_API cat_bool_t cat_coroutine_switch_denied(void)
{
    return CAT_COROUTINE_G(switch_denied);
}

CAT_API void cat_coroutine_switch_deny(void)
{
    CAT_COROUTINE_G(switch_denied) = cat_true;
}

CAT_API void cat_coroutine_switch_allow(void)
{
    CAT_COROUTINE_G(switch_denied) = cat_false;
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

CAT_API cat_log_type_t cat_coroutine_get_deadlock_log_type(void)
{
    return CAT_COROUTINE_G(deadlock_log_type);
}

CAT_API cat_coroutine_t *cat_coroutine_get_current(void)
{
    return CAT_COROUTINE_G(current);
}

CAT_API cat_coroutine_id_t cat_coroutine_get_current_id(void)
{
    cat_coroutine_t *coroutine = CAT_COROUTINE_G(current);

    /* Notice: current coroutine is NULL before runtime_init() */
    return likely(coroutine != NULL) ? coroutine->id : CAT_COROUTINE_MAIN_ID;
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
    cat_coroutine_t *coroutine;
    cat_data_t *data;
#ifndef CAT_COROUTINE_USE_THREAD_CONTEXT
    coroutine = CAT_COROUTINE_G(current);
#else
    coroutine = transfer.coroutine;
    /* we must create the thread first,
     * because thread creation does not always succeed,
     * we should handle error before we start.
     * And we need to wait for the first resume() call here. */
    uv_sem_wait(&coroutine->sem);
    if (unlikely(coroutine->state == CAT_COROUTINE_STATE_DEAD)) {
        /* coroutine maybe closed after created without executing anything */
        return;
    }
#endif
#ifdef CAT_COROUTINE_USE_ASAN
    CAT_ASSERT(coroutine->asan_fake_stack == NULL);
    __sanitizer_finish_switch_fiber(coroutine->asan_fake_stack, &coroutine->from->asan_stack, &coroutine->from->asan_stack_size);
#endif
    if (unlikely(++CAT_COROUTINE_G(count) > CAT_COROUTINE_G(peak_count))) {
        CAT_COROUTINE_G(peak_count) = CAT_COROUTINE_G(count);
    }
    CAT_LOG_DEBUG(COROUTINE, "Start (count=" CAT_COROUTINE_COUNT_FMT ")", CAT_COROUTINE_G(count));
#if defined(CAT_COROUTINE_USE_BOOST_CONTEXT)
    /* update origin's context */
    coroutine->from->context = transfer.from_context;
    data = transfer.data;
#elif defined(CAT_COROUTINE_USE_USER_TRANSFER_DATA)
    data = coroutine->transfer_data;
#endif
    /* start time */
    coroutine->start_time = cat_time_msec();
    /* execute function */
    data = coroutine->function(data);
    /* end time */
    coroutine->end_time = cat_time_msec();
    /* finished */
    CAT_COROUTINE_G(count)--;
    CAT_LOG_DEBUG(COROUTINE, "Finished (count=" CAT_COROUTINE_COUNT_FMT ")", CAT_COROUTINE_G(count));
    /* mark as dead */
    coroutine->state = CAT_COROUTINE_STATE_DEAD;
    /* yield to previous */
    cat_coroutine_yield(data, NULL);
#ifdef CAT_COROUTINE_USE_THREAD_CONTEXT
    return;
#endif
    /* never here */
    CAT_NEVER_HERE("Coroutine is dead");
}

CAT_API cat_coroutine_t *cat_coroutine_create(cat_coroutine_t *coroutine, cat_coroutine_function_t function)
{
    return cat_coroutine_create_ex(coroutine, function, 0);
}

CAT_API cat_coroutine_t *cat_coroutine_create_ex(cat_coroutine_t *coroutine, cat_coroutine_function_t function, size_t stack_size)
{
    cat_coroutine_context_t context;
    cat_coroutine_flags_t flags = CAT_COROUTINE_FLAG_NONE;

#ifdef CAT_COROUTINE_USE_UCONTEXT
    if (unlikely(getcontext(&context) == -1)) {
        cat_update_last_error_of_syscall("Coroutine getcontext() failed");
        return NULL;
    }
#endif

    /* Malloc coroutine if necessary */
    if (coroutine == NULL) {
        coroutine = (cat_coroutine_t *) cat_malloc(sizeof(*coroutine));
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(coroutine == NULL)) {
            cat_update_last_error_of_syscall("Malloc for coroutine failed");
            return NULL;
        }
#endif
        flags |= CAT_COROUTINE_FLAG_ALLOCATED;
    }

    /* align stack size and add padding */
    stack_size = cat_coroutine_align_stack_size(stack_size);

#ifdef CAT_COROUTINE_USE_THREAD_CONTEXT
    int error = uv_sem_init(&coroutine->sem, 0);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Coroutine sem init failed");
        return NULL;
    }
    uv_thread_options_t options;
    options.flags = UV_THREAD_HAS_STACK_SIZE;
    options.stack_size = stack_size;
    error = uv_thread_create_ex(&context, &options, (uv_thread_cb) cat_coroutine_context_function, coroutine);
    if (unlikely(error != 0)) {
        uv_sem_destroy(&coroutine->sem);
        if (flags & CAT_COROUTINE_FLAG_ALLOCATED) {
            cat_free(coroutine);
        }
        cat_update_last_error_with_reason(error, "Coroutine thread create failed");
        return NULL;
    }
#endif

#ifdef  CAT_COROUTINE_USE_USER_STACK
    void *virtual_memory;
    size_t virtual_memory_size;
    size_t padding_size = cat_getpagesize() * CAT_COROUTINE_STACK_PADDING_PAGE_COUNT;
    void *stack, *stack_start;
    /* Coroutine Virtual Memory
    * - PADDING: memory-protection is on
    *   (1 page for mmap/VirtualAlloc, 2 pages for sys_malloc)
    + - - - - +-----------------------------------------------+
    : PADDING :                     STACK                     :
    + - - - - +-----------------------------------------------+
    *         ^                                               ^
    *       stack                                         stack_start
    */
    virtual_memory_size = padding_size + stack_size;
    /* alloc memory */
#if defined(CAT_COROUTINE_USE_MMAP)
    virtual_memory = mmap(NULL, virtual_memory_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
#elif defined(CAT_COROUTINE_USE_VIRTUAL_ALLOC)
    virtual_memory = VirtualAlloc(0, virtual_memory_size, MEM_COMMIT, PAGE_READWRITE);
#else // if defined(CAT_COROUTINE_USE_SYS_MALLOC)
    virtual_memory = cat_sys_malloc(virtual_memory_size);
#endif
    if (unlikely(virtual_memory == CAT_COROUTINE_MEMORY_INVALID)) {
        cat_update_last_error_of_syscall("Allocate virtual memory for coroutine stack failed with size %zu", virtual_memory_size);
        if (flags & CAT_COROUTINE_FLAG_ALLOCATED) {
            cat_free(coroutine);
        }
        return NULL;
    }
    stack = ((char *) virtual_memory) + padding_size;
    stack_start = ((char *) stack) + stack_size;

#ifdef CAT_COROUTINE_USE_MEMORY_PROTECT
    /* protect a page of memory after the stack top
     * to notify stack overflow */
    do {
        void *page = virtual_memory;
        cat_bool_t ret;
# ifdef CAT_COROUTINE_USE_SYS_MALLOC
        /* mallocated memory is not aligned with the page */
        page = cat_getpageafter(page);
# endif
# ifndef CAT_OS_WIN
        ret = mprotect(page, cat_getpagesize(), PROT_NONE) == 0;
# else
        DWORD old_protect;
        ret = VirtualProtect(page, cat_getpagesize(), PAGE_NOACCESS /* PAGE_READWRITE | PAGE_GUARD */, &old_protect) != 0;
# endif
        CAT_LOG_DEBUG_V2(COROUTINE, "Protect stack page at %p with %zu bytes %s", page, cat_getpagesize(), ret ? "successfully" : "failed");
        if (unlikely(!ret)) {
            CAT_SYSCALL_FAILURE(NOTICE, COROUTINE, "Protect stack page failed");
        }
    } while (0);
#endif /* CAT_COROUTINE_USE_MEMORY_PROTECT */
#endif /* CAT_COROUTINE_USE_USER_STACK */

    /* make context */
#if defined(CAT_COROUTINE_USE_UCONTEXT)
    context.uc_stack.ss_sp = stack;
    context.uc_stack.ss_size = stack_size;
    context.uc_stack.ss_flags = 0;
    context.uc_link = NULL;
    cat_coroutine_context_make(&context, (void (*)(void)) &cat_coroutine_context_function, 1, NULL);
#elif defined(CAT_COROUTINE_USE_BOOST_CONTEXT)
    context = cat_coroutine_context_make(stack_start, stack_size, cat_coroutine_context_function);
#endif

    /* init coroutine properties */
    coroutine->id = CAT_COROUTINE_G(last_id)++;
    coroutine->flags = flags | CAT_COROUTINE_FLAG_ACCEPT_DATA;
    coroutine->state = CAT_COROUTINE_STATE_WAITING;
    coroutine->round = 0;
    coroutine->from = NULL;
    coroutine->previous = NULL;
    coroutine->next = NULL;
    coroutine->start_time = 0;
    coroutine->end_time = 0;
    coroutine->stack_size = (cat_coroutine_stack_size_t) stack_size;
    coroutine->function = function;
#ifdef CAT_COROUTINE_USE_USER_STACK
    coroutine->virtual_memory = virtual_memory;
    coroutine->virtual_memory_size = (uint32_t) virtual_memory_size;
#endif
    coroutine->context = context;
#ifdef CAT_COROUTINE_USE_USER_TRANSFER_DATA
    coroutine->transfer_data = NULL;
#endif
#ifdef CAT_HAVE_VALGRIND
    coroutine->valgrind_stack_id = VALGRIND_STACK_REGISTER(stack_start, stack);
#endif
#ifdef CAT_COROUTINE_USE_ASAN
    coroutine->asan_fake_stack = NULL;
    coroutine->asan_stack = stack;
    coroutine->asan_stack_size = stack_size;
#endif
#ifndef CAT_COROUTINE_USE_THREAD_CONTEXT
    CAT_LOG_DEBUG(COROUTINE, "Create R" CAT_COROUTINE_ID_FMT " "
        "with stack = %p, stack_size = %zu, function = %p, "
        "virtual_memory = %p, virtual_memory_size = %zu",
        coroutine->id, stack, stack_size, function, 
        virtual_memory, virtual_memory_size);
#else
    CAT_LOG_DEBUG(COROUTINE, "Create R" CAT_COROUTINE_ID_FMT " "
        "with tid = %" PRId64 ", stack_size = %zu, function = %p, ",
        coroutine->id, (int64_t) coroutine->context, stack_size, function);
#endif

    return coroutine;
}

CAT_API void cat_coroutine_free(cat_coroutine_t *coroutine)
{
    CAT_LOG_DEBUG(COROUTINE, "Close R" CAT_COROUTINE_ID_FMT, coroutine->id);
    CAT_ASSERT(!cat_coroutine_is_alive(coroutine) && "Coroutine can not be forced to close when it is running or waiting");
#ifdef CAT_COROUTINE_USE_THREAD_CONTEXT
    if (coroutine->start_time == 0) {
        coroutine->state = CAT_COROUTINE_STATE_DEAD;
    }
    uv_sem_post(&coroutine->sem);
    uv_thread_join(&coroutine->context);
#endif
#ifdef CAT_HAVE_VALGRIND
    VALGRIND_STACK_DEREGISTER(coroutine->valgrind_stack_id);
#endif
#if defined(CAT_COROUTINE_USE_MEMORY_PROTECT) && defined(CAT_COROUTINE_USE_SYS_MALLOC)
    do {
        void *page = cat_getpageafter(coroutine->virtual_memory);
        cat_bool_t ret;
# ifndef CAT_OS_WIN
        ret = mprotect(page, cat_getpagesize(), PROT_READ | PROT_WRITE) == 0;
# else
        DWORD old_protect;
        ret = VirtualProtect(page, cat_getpagesize(), PAGE_READWRITE, &old_protect) != 0;
# endif
        CAT_LOG_DEBUG_V2(COROUTINE, "Unprotect stack page at %p with %zu bytes %s", page, cat_getpagesize(), ret ? "successfully" : "failed");
        if (unlikely(!ret)) {
            CAT_SYSCALL_FAILURE(NOTICE, COROUTINE, "Unprotect stack page failed");
        }
    } while (0);
#endif
#if defined(CAT_COROUTINE_USE_MMAP)
    munmap(coroutine->virtual_memory, coroutine->virtual_memory_size);
#elif defined(CAT_COROUTINE_USE_VIRTUAL_ALLOC)
    VirtualFree(coroutine->virtual_memory, 0, MEM_RELEASE);
#elif defined(CAT_COROUTINE_USE_SYS_MALLOC)
    cat_sys_free(coroutine->virtual_memory);
#endif
    if (coroutine->flags & CAT_COROUTINE_FLAG_ALLOCATED) {
        cat_free(coroutine);
    }
}

CAT_API cat_bool_t cat_coroutine_close(cat_coroutine_t *coroutine)
{
    if (!cat_coroutine_is_available(coroutine)) {
        cat_update_last_error(CAT_EBADF, "Coroutine is not avaliable");
        return cat_false;
    }
    if (cat_coroutine_is_alive(coroutine)) {
        cat_update_last_error(CAT_EBUSY, "Coroutine can not be forced to close when it is running or waiting");
        return cat_false;
    }

    cat_coroutine_free(coroutine);

    return cat_true;
}

CAT_API void cat_coroutine_jump_standard(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval)
{
    cat_coroutine_t *current_coroutine = CAT_COROUTINE_G(current);

    CAT_ASSERT((data == NULL || (coroutine->flags & CAT_COROUTINE_FLAG_ACCEPT_DATA)) && "Coroutine does not accept data");

    /* swap ptr */
    CAT_COROUTINE_G(current) = coroutine;
    /* update from */
    coroutine->from = current_coroutine;
    /* update current coroutine state */
    if (current_coroutine->state == CAT_COROUTINE_STATE_RUNNING) {
        /* maybe dead */
        current_coroutine->state = CAT_COROUTINE_STATE_WAITING;
    }
    /* update state */
    coroutine->state = CAT_COROUTINE_STATE_RUNNING;
    /* round++ */
    coroutine->round = ++CAT_COROUTINE_G(round);
    /* accept-data check */
    if (retval != NULL) {
        current_coroutine->flags |= CAT_COROUTINE_FLAG_ACCEPT_DATA;
    }
#ifdef CAT_COROUTINE_USE_ASAN
    __sanitizer_start_switch_fiber(
        current_coroutine->state != CAT_COROUTINE_STATE_DEAD ? &current_coroutine->asan_fake_stack : NULL,
        coroutine->asan_stack, coroutine->asan_stack_size
    );
#endif
    /* jump {{{ */
#ifdef CAT_COROUTINE_USE_USER_TRANSFER_DATA
    coroutine->transfer_data = data;
#endif
#if defined(CAT_COROUTINE_USE_THREAD_CONTEXT)
    uv_sem_post(&coroutine->sem);
    uv_sem_wait(&current_coroutine->sem);
    if (unlikely(current_coroutine->state == CAT_COROUTINE_STATE_DEAD)) {
        return;
    }
#elif defined(CAT_COROUTINE_USE_UCONTEXT)
    cat_coroutine_context_jump(&current_coroutine->context, &coroutine->context);
#elif defined(CAT_COROUTINE_USE_BOOST_CONTEXT)
    cat_coroutine_transfer_t transfer = cat_coroutine_context_jump(coroutine->context, data);
    data = transfer.data;
#endif
#ifdef CAT_COROUTINE_USE_USER_TRANSFER_DATA
    data = current_coroutine->transfer_data;
#endif
    /* }}} */
    CAT_ASSERT(current_coroutine == CAT_COROUTINE_G(current));
    /* accept-data clear */
    current_coroutine->flags &= ~CAT_COROUTINE_FLAG_ACCEPT_DATA;
    /* handle from */
    coroutine = current_coroutine->from;
    CAT_ASSERT(coroutine != NULL);
#ifdef CAT_COROUTINE_USE_ASAN
    __sanitizer_finish_switch_fiber(current_coroutine->asan_fake_stack, &coroutine->asan_stack, &coroutine->asan_stack_size);
#endif
#ifdef CAT_COROUTINE_USE_BOOST_CONTEXT
    /* update the from context */
    coroutine->context = transfer.from_context;
#endif
    /* close the coroutine if it is finished */
    if (unlikely(coroutine->state == CAT_COROUTINE_STATE_DEAD)) {
        cat_coroutine_free(coroutine);
    }

    if (retval != NULL) {
        *retval = data;
    } else {
        CAT_ASSERT(data == NULL && "Unexpected transfer return data");
    }
}

#define CAT_COROUTINE_SWITCH_PRECHECK(failure) \
    if (unlikely(CAT_COROUTINE_G(switch_denied))) { \
        cat_update_last_error(CAT_EMISUSE, "Coroutine switch denied"); \
        failure; \
    }

static cat_always_inline cat_bool_t cat_coroutine_check_resumability(const cat_coroutine_t *coroutine)
{
    switch (coroutine->state) {
        case CAT_COROUTINE_STATE_WAITING:
            break;
        case CAT_COROUTINE_STATE_RUNNING:
            cat_update_last_error(CAT_EBUSY, "Coroutine is already running");
            return cat_false;
        case CAT_COROUTINE_STATE_DEAD:
            cat_update_last_error(CAT_ESRCH, "Coroutine is dead");
            return cat_false;
        case CAT_COROUTINE_STATE_NONE:
            cat_update_last_error(CAT_EMISUSE, "Coroutine is uninitialized");
            return cat_false;
        default:
            CAT_NEVER_HERE("Unknown state");
    }

    return cat_true;
}

#define CAT_COROUTINE_SWITCH_LOG(action, to) cat_coroutine_switch_log(#action, to)

static cat_always_inline void cat_coroutine_switch_log(const char *action, const cat_coroutine_t *coroutine)
{
    CAT_LOG_DEBUG_SCOPE_START(COROUTINE) {
        const char *name = cat_coroutine_get_role_name(coroutine);
        if (name != NULL) {
            CAT_LOG_DEBUG_D(COROUTINE, "%s to %s", action, name);
        } else {
            CAT_LOG_DEBUG_D(COROUTINE, "%s to R" CAT_COROUTINE_ID_FMT, action, coroutine->id);
        }
    } CAT_LOG_DEBUG_SCOPE_END();
}

CAT_API cat_bool_t cat_coroutine_resume(cat_coroutine_t *coroutine, cat_data_t *data, cat_data_t **retval)
{
    CAT_COROUTINE_SWITCH_PRECHECK(return cat_false);
    if (unlikely(!cat_coroutine_check_resumability(coroutine))) {
        return cat_false;
    }

    CAT_COROUTINE_SWITCH_LOG(resume, coroutine);

    /* resume flow:
    * +------+  +------+       +------+
    * | co-1 +->| co-2 +-here->| co-3 |
    * +------+  +------+       +------+
    * resume previous
    * +------+  +------+       +------+
    * | co-1 +->| co-2 +------>| co-3 |
    * +------+  +--^---+       +---+--+
    *              |               |
    *              +-----here------+
    * then it becomes:
    * +------+  +------+      +------+
    * | co-1 +->| co-3 +----->| co-2 |
    * +------+  +------+      +------+
    * or cross resume flow:
    * +------+  +------+      +------+
    * | co-1 +->| co-2 +----->| co-3 |
    * +--^---+  +------+      +---+--+
    *   |                        |
    *   +----- here-we-are-------+
    * then it becomes:
    * +------+  +------+      +------+
    * | co-2 +->| co-3 +----->| co-1 |
    * +------+  +------+      +------+
    */
    do {
        cat_coroutine_t *current_coroutine = CAT_COROUTINE_G(current);
        /* remove target from linked list */
        if (coroutine->previous != NULL) {
            coroutine->previous->next = coroutine->next;
        }
        if (coroutine->next != NULL) {
            coroutine->next->previous = coroutine->previous;
        }
        /* target's previous becomes current */
        coroutine->previous = current_coroutine;
        /* target's next becomes NULL */
        coroutine->next = NULL;
        /* current's next becomes target */
        current_coroutine->next = coroutine;
    } while (0);

    cat_coroutine_jump(coroutine, data, retval);

    return cat_true;
}

CAT_API cat_bool_t cat_coroutine_yield(cat_data_t *data, cat_data_t **retval)
{
    CAT_COROUTINE_SWITCH_PRECHECK(return cat_false);
    cat_coroutine_t *current_coroutine = CAT_COROUTINE_G(current);
    cat_coroutine_t *coroutine = current_coroutine->previous;

    if (coroutine == NULL) {
        coroutine = CAT_COROUTINE_G(scheduler);
        if (unlikely(coroutine == NULL || coroutine == current_coroutine)) {
            cat_update_last_error(CAT_EMISUSE, "Coroutine has nowhere to go");
            return cat_false;
        }
    }

    CAT_COROUTINE_SWITCH_LOG(yield, coroutine);

    /* yield flow:
    * +------+  +------+       +------+
    * | co-1 +->| co-2 +<-here-| co-3 |
    * +------+  +------+       +------+
    * then it becomes:
    * +------+  +------+       +------+
    * | co-1 +->| co-2 |       | co-3 |:(waiting)
    * +------+  +------+       +------+
    */

    /* break the previous */
    current_coroutine->previous = NULL;
    /* break the next */
    coroutine->next = NULL;

    cat_coroutine_jump(coroutine, data, retval);

    return cat_true;
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

CAT_API cat_coroutine_t *cat_coroutine_get_next(const cat_coroutine_t *coroutine)
{
    return coroutine->next;
}

CAT_API cat_coroutine_stack_size_t cat_coroutine_get_stack_size(const cat_coroutine_t *coroutine)
{
    return coroutine->stack_size;
}

/* status */

CAT_API cat_bool_t cat_coroutine_is_available(const cat_coroutine_t *coroutine)
{
    return coroutine->state < CAT_COROUTINE_STATE_DEAD && coroutine->state > CAT_COROUTINE_STATE_NONE;
}

CAT_API cat_bool_t cat_coroutine_is_alive(const cat_coroutine_t *coroutine)
{
    return coroutine->end_time == 0 && coroutine->start_time > 0;
}

CAT_API cat_bool_t cat_coroutine_is_over(const cat_coroutine_t *coroutine)
{
    return coroutine->state >= CAT_COROUTINE_STATE_DEAD;
}

CAT_API cat_coroutine_flags_t cat_coroutine_get_flags(const cat_coroutine_t *coroutine)
{
    return coroutine->flags;
}

CAT_API void cat_coroutine_set_flags(cat_coroutine_t *coroutine, cat_coroutine_flags_t flags)
{
    coroutine->flags = flags;
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

CAT_API cat_coroutine_round_t cat_coroutine_get_round(const cat_coroutine_t *coroutine)
{
    return coroutine->round;
}

CAT_API cat_msec_t cat_coroutine_get_start_time(const cat_coroutine_t *coroutine)
{
    return coroutine->start_time;
}

CAT_API cat_msec_t cat_coroutine_get_end_time(const cat_coroutine_t *coroutine)
{
    return coroutine->end_time;
}

CAT_API cat_msec_t cat_coroutine_get_elapsed(const cat_coroutine_t *coroutine)
{
    if (unlikely(coroutine->start_time == 0)) {
        return 0;
    }
    if (unlikely(coroutine->end_time != 0)) {
        return coroutine->end_time - coroutine->start_time;
    }
    return cat_time_msec() - coroutine->start_time;
}

CAT_API char *cat_coroutine_get_elapsed_as_string(const cat_coroutine_t *coroutine)
{
    return cat_time_format_msec(cat_coroutine_get_elapsed(coroutine));
}

/* scheduler */

static void cat_coroutine_deadlock(cat_coroutine_deadlock_function_t deadlock)
{
    CAT_LOG_WITH_TYPE(CAT_COROUTINE_G(deadlock_log_type), COROUTINE, CAT_EDEADLK, "Deadlock: all coroutines are asleep");

    if (deadlock != NULL) {
        deadlock();
    } else while (cat_sys_usleep(999999) == 0);
}

static cat_always_inline cat_bool_t cat_coroutine_is_deadlocked(void)
{
    return CAT_COROUTINE_G(count) - CAT_COROUTINE_G(waiter_count) > 0;
}

static cat_data_t *cat_coroutine_scheduler_function(cat_data_t *data)
{
    cat_coroutine_t *coroutine = CAT_COROUTINE_G(current);
    cat_coroutine_scheduler_t scheduler = *((cat_coroutine_scheduler_t *) data);

    CAT_COROUTINE_G(scheduler) = coroutine;
    CAT_COROUTINE_G(count)--;

    cat_coroutine_yield(NULL, NULL);

    while (!(coroutine->from->flags & CAT_COROUTINE_FLAG_SCHEDULING)) {

        scheduler.schedule();

        if (cat_coroutine_is_deadlocked()) {
            if (CAT_COROUTINE_G(deadlock_callback) != NULL) {
                CAT_COROUTINE_G(deadlock_callback)();
                scheduler.schedule(); // There is only one chance
                if (!cat_coroutine_is_deadlocked()) {
                    continue;
                }
            }
            /* we expect everything is done,
             * but there are still coroutines that have not finished
             * so we try to trigger the deadlock */
            cat_coroutine_deadlock(scheduler.deadlock);
            /* deadlock failed or it was broken by some magic ways */
            continue;
        }

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

    /* let scheduler id be MAX */
    do {
        cat_coroutine_id_t last_id = CAT_COROUTINE_G(last_id);
        CAT_COROUTINE_G(last_id) = CAT_COROUTINE_SCHEDULER_ID;
        coroutine = cat_coroutine_create(coroutine, cat_coroutine_scheduler_function);
        CAT_COROUTINE_G(last_id) = last_id;
    } while (0);

    if (coroutine == NULL) {
        cat_update_last_error_with_previous("Create event scheduler failed");
        return NULL;
    }

    /* run scheduler */
    (void) cat_coroutine_resume(coroutine, (cat_data_t *) scheduler, NULL);

    CAT_ASSERT(CAT_COROUTINE_G(scheduler) != NULL);

    return coroutine;
}

CAT_API cat_coroutine_t *cat_coroutine_scheduler_close(void)
{
    cat_coroutine_t *coroutine = CAT_COROUTINE_G(scheduler);

    if (coroutine == NULL) {
        cat_update_last_error(CAT_EMISUSE, "No scheduler is available");
        return NULL;
    }

    /* wait all coroutines done before we close scheduler */
    while (1) {
        if (cat_coroutine_wait_all()) {
            break;
        }
    }

    cat_coroutine_schedule(coroutine, COROUTINE, "Close scheduler");

    CAT_ASSERT(CAT_COROUTINE_G(scheduler) == NULL);

    return coroutine;
}

CAT_API cat_bool_t cat_coroutine_wait_all(void)
{
    return cat_coroutine_wait_all_ex(CAT_TIMEOUT_FOREVER);
}

CAT_API cat_bool_t cat_coroutine_wait_all_ex(cat_timeout_t timeout)
{
    cat_coroutine_t *coroutine = CAT_COROUTINE_G(current);
    cat_bool_t done, ret;

    cat_queue_push_back(&CAT_COROUTINE_G(waiters), &coroutine->waiter.node);
    CAT_COROUTINE_G(waiter_count)++;

    /* usually, it will be unlocked by event scheduler */
    ret = cat_time_wait(timeout);
    done = !!(coroutine->from->flags & CAT_COROUTINE_FLAG_SCHEDULING);

    CAT_COROUTINE_G(waiter_count)--;
    cat_queue_remove(&coroutine->waiter.node);

    if (!ret) {
        cat_update_last_error_with_previous("Wait all failed");
        return cat_false;
    }
    if (!done) {
        cat_update_last_error(CAT_ECANCELED, "Wait all has been canceled");
        return cat_false;
    }

    return cat_true;
}

CAT_API void cat_coroutine_notify_all(void)
{
    cat_queue_t *waiters = &CAT_COROUTINE_G(waiters);
    cat_coroutine_count_t count = CAT_COROUTINE_G(waiter_count);
    /* Notice: coroutine may re-wait after resume immediately, so we must record count here,
     * otherwise it will always be resumed */
    while (count--) {
        cat_coroutine_t *coroutine = cat_queue_front_data(waiters, cat_coroutine_t, waiter.node);
        cat_coroutine_schedule(coroutine, COROUTINE, "Notify all");
    }
}

/* special */

CAT_API const char *cat_coroutine_get_role_name(const cat_coroutine_t *coroutine)
{
    switch (coroutine->id) {
        case CAT_COROUTINE_MAIN_ID:
            return "main";
        case CAT_COROUTINE_SCHEDULER_ID:
            return "scheduler";
        default:
            return NULL;
    }
}

CAT_API const char *cat_coroutine_get_current_role_name(void)
{
    const cat_coroutine_t *coroutine = CAT_COROUTINE_G(current);

    if (unlikely(coroutine == NULL)) {
        /* may be called out of runtime (usually in log) */
        return "main";
    }

    return cat_coroutine_get_role_name(coroutine);
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
        cat_coroutine_free(coroutine);
        return NULL;
    }

    return coroutine;
}
