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

#ifndef SWOW_PROCESS_H
#define SWOW_PROCESS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_os_wait.h"

#ifdef CAT_OS_WAIT

extern SWOW_API zend_class_entry *swow_process_interface_ce;

extern SWOW_API zend_class_entry *swow_fork_process_ce;
extern SWOW_API zend_object_handlers swow_fork_process_handlers;

extern SWOW_API zend_class_entry *swow_process_exit_status_ce;
extern SWOW_API zend_object_handlers swow_process_exit_status_handlers;

extern SWOW_API zend_class_entry *swow_process_exception_ce;

typedef struct swow_fork_process_s {
    cat_pid_t pid;
    int status;           /* waitpid 返回的 raw status */
    cat_bool_t exited;    /* 是否已回收 */
    zend_object std;
} swow_fork_process_t;

typedef struct swow_process_exit_status_s {
    int status;           /* waitpid 返回的 raw status */
    zend_object std;
} swow_process_exit_status_t;

/* loader */

zend_result swow_process_module_init(INIT_FUNC_ARGS);

/* helper */

static zend_always_inline swow_fork_process_t *swow_fork_process_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_fork_process_t, std);
}

static zend_always_inline swow_process_exit_status_t *swow_process_exit_status_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_process_exit_status_t, std);
}

#endif /* CAT_OS_WAIT */

#ifdef __cplusplus
}
#endif
#endif /* SWOW_PROCESS_H */
