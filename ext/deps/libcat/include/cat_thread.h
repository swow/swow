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

#ifndef CAT_THREAD_H
#define CAT_THREAD_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#ifdef CAT_THREAD_SAFE
#define CAT_THREAD 1

#include "cat_queue.h"
#include "cat_time.h"

#define CAT_THREAD_MAIN_ID 1

typedef void (*cat_thread_routine_t)(cat_data_t *arg);

typedef struct cat_thread_s cat_thread_t;

typedef uint64_t cat_thread_id_t;

struct cat_thread_s {
    uv_thread_t tid;
/* can ony be modified by parent {{{ */
    /* properties */
    cat_thread_id_t id;
    cat_thread_routine_t routine;
    cat_data_t *transfer_data;
    cat_bool_t allocated;
    cat_bool_t started;
    cat_bool_t exited;
    cat_bool_t joined;
    /* children */
    uint32_t child_count;
    /* parent */
    cat_thread_t *parent;
    cat_queue_t parent_waiters;
    uint32_t parent_waiter_count;
/* }}} */
    /* message */
    uint32_t notifier_refcount;
    uv_async_t notifier;
    /* tasks */
    cat_queue_t tasks;
    uv_mutex_t task_mutex;
    uint32_t task_count;
};

typedef enum cat_thread_option_flag_e {
    CAT_THREAD_OPTION_FLAG_NONE = UV_THREAD_NO_FLAGS,
    CAT_THREAD_OPTION_FLAG_STACK_SIZE = UV_THREAD_HAS_STACK_SIZE,
} cat_thread_option_flag_t;

typedef int cat_thread_option_flags_t;

typedef struct cat_thread_options_s {
    cat_thread_option_flags_t flags;
    size_t stack_size;
    /* More fields may be added at any time. */
} cat_thread_options_t;

typedef struct cat_thread_task_s {
    cat_queue_node_t node;
    cat_data_callback_t callback;
    cat_data_t *data;
} cat_thread_task_t;

CAT_STATIC_ASSERT(sizeof(cat_thread_options_t) == sizeof(uv_thread_options_t));

CAT_API cat_bool_t cat_thread_module_init(void);
CAT_API cat_bool_t cat_thread_runtime_init(void);
CAT_API cat_bool_t cat_thread_runtime_shutdown(void);

CAT_API cat_thread_t *cat_thread_register_main(cat_thread_t *thread);
CAT_API uint32_t cat_thread_get_count(void);

CAT_API cat_thread_t *cat_thread_create(cat_thread_t *thread, cat_thread_routine_t routine, cat_data_t *arg);
CAT_API cat_thread_t *cat_thread_create_ex(cat_thread_t *thread, cat_thread_routine_t routine, cat_data_t *arg, const cat_thread_options_t *options);
CAT_API cat_bool_t cat_thread_close(cat_thread_t *thread);

CAT_API cat_bool_t cat_thread_join(cat_thread_t *thread);

CAT_API cat_bool_t cat_thread_equal(const cat_thread_t *thread1, const cat_thread_t *thread2);
CAT_API cat_thread_t *cat_thread_get_current(void);

CAT_API cat_bool_t cat_thread_dispatch_task(cat_thread_t *thread, cat_data_callback_t callback, cat_data_t *data);
CAT_API void cat_thread_do_tasks(cat_thread_t *thread);

#endif /* CAT_THREAD_SAFE */

#ifdef __cplusplus
}
#endif
#endif /* CAT_THREAD_H */
