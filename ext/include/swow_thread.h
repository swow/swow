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

#ifndef SWOW_THREAD_H
#define SWOW_THREAD_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"
#ifdef ZTS

#include "cat_thread.h"
#include "cat_queue.h"
#include "cat_ts_ref.h"

extern SWOW_API zend_class_entry *swow_thread_ce;
extern SWOW_API zend_object_handlers swow_thread_object_handlers;

extern SWOW_API zend_class_entry *swow_thread_exception_ce;

typedef struct swow_thread_s {
    cat_thread_t thread;
    uv_key_t object_key;
    zend_atomic_bool *vm_interrupt_ptr;
    int exit_status;
    CAT_TS_REF_FIELD;
} swow_thread_t;

typedef struct swow_thread_object_s {
    swow_thread_t *s_thread;
    zend_object std;
} swow_thread_object_t;

/* globals */

CAT_GLOBALS_STRUCT_BEGIN(swow_thread) {
    void *reserved;
} CAT_GLOBALS_STRUCT_END(swow_thread);

extern SWOW_API CAT_GLOBALS_DECLARE(swow_thread);

#define SWOW_THREAD_G(x) CAT_GLOBALS_GET(swow_thread, x)

/* loader */

zend_result swow_thread_module_init(INIT_FUNC_ARGS);
zend_result swow_thread_module_shutdown(INIT_FUNC_ARGS);
zend_result swow_thread_runtime_init(INIT_FUNC_ARGS);
zend_result swow_thread_runtime_shutdown(INIT_FUNC_ARGS);

/* helper */

static zend_always_inline swow_thread_t *swow_thread_get_from_handle(cat_thread_t *thread)
{
    return cat_container_of(thread, swow_thread_t, thread);
}

static zend_always_inline swow_thread_t *swow_thread_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_thread_object_t, std)->s_thread;
}

static zend_always_inline swow_thread_t *swow_thread_get_current(void)
{
    cat_thread_t *thread = cat_thread_get_current();
    ZEND_ASSERT(thread != NULL && "Unable to call in third party thread context");
    return swow_thread_get_from_handle(thread);
}

static zend_always_inline swow_thread_t *swow_thread_get_parent(swow_thread_t *s_thread)
{
    cat_thread_t *parent_thread = s_thread->thread.parent;
    return parent_thread ? swow_thread_get_from_handle(parent_thread) : NULL;
}

static zend_always_inline swow_thread_object_t *swow_thread_object_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_thread_object_t, std);
}

static zend_always_inline swow_thread_object_t *swow_thread_object_get_from_handle(swow_thread_t *s_thread)
{
    return (swow_thread_object_t *) uv_key_get(&s_thread->object_key);
}

#endif /* ZTS */

#ifdef __cplusplus
}
#endif
#endif /* SWOW_THREAD_H */
