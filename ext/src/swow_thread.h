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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#ifndef SWOW_THREAD_H
#define SWOW_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"
#include "cat_thread.h"

#ifdef ZTS

extern SWOW_API zend_class_entry *swow_thread_ce;
extern SWOW_API zend_class_entry *swow_thread_exception_ce;
extern SWOW_API zend_object_handlers swow_thread_handlers;

typedef struct swow_thread_run_s {
    cat_atomic_uint32_t refcount;
    cat_atomic_bool_t killed;
    cat_thread_t thread;
    cat_sem_t started;
    /* serialized callable and args, allocated with cat_sys_malloc */
    const char *callable;
    size_t callable_len;
    const char *args;
    size_t args_len;
    /* PHP code for runCode, allocated with cat_sys_malloc */
    const char *code;
    size_t code_len;
    /* serialized return value, allocated with cat_sys_malloc */
    const char *result;
    size_t result_len;
    /* serialized exception, allocated with cat_sys_malloc */
    const char *exception;
    size_t exception_len;
    cat_atomic_int32_t exit_status;
    /* cat_true when the result or the exception did not serialize */
    cat_atomic_bool_t result_invalid;
    void *server_context;
    cat_atomic_int32_t start_status;
    uv_mutex_t lock;
    zend_executor_globals *eg;
    zend_swow_globals *globals;
} swow_thread_run_t;

typedef struct swow_thread_s {
    zval callable;
    zval args;
    zval result;
    swow_thread_run_t *run;
    cat_thread_t *thread;
    zend_long id;
    cat_bool_t started;
    cat_bool_t waited;
    cat_bool_t failed;
    cat_bool_t detached;
    cat_bool_t killed;
    zend_object std;
} swow_thread_t;

zend_result swow_thread_module_init(INIT_FUNC_ARGS);
zend_result swow_thread_module_shutdown(SHUTDOWN_FUNC_ARGS);
zend_result swow_thread_runtime_init(INIT_FUNC_ARGS);
zend_result swow_thread_runtime_shutdown(SHUTDOWN_FUNC_ARGS);

#endif /* ZTS */

#ifdef __cplusplus
}
#endif

#endif /* SWOW_THREAD_H */
