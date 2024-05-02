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

#ifndef SWOW_THANNEL_H
#define SWOW_THANNEL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "swow_thread.h"

#include "cat_queue.h"

extern SWOW_API zend_class_entry *swow_thannel_ce;
extern SWOW_API zend_object_handlers swow_thannel_handlers;

extern SWOW_API zend_class_entry *swow_thannel_exception_ce;

#define SWOW_THANNEL_STATE_MAP(XX) \
    XX(NONE,    0, "none") \
    XX(WAITING, 1, "waiting") \
    XX(RUNNING, 2, "running") \
    XX(DEAD,    3, "dead") \

typedef enum swow_thannel_state_e {
#define SWOW_THANNEL_STATE_GEN(name, value, unused) CAT_ENUM_GEN(SWOW_THANNEL_STATE_, name, value)
    SWOW_THANNEL_STATE_MAP(SWOW_THANNEL_STATE_GEN)
#undef SWOW_THANNEL_STATE_GEN
} swow_thannel_state_t;

typedef struct swow_thannel_s {
    swow_thannel_state_t state;
    zend_object std;
} swow_thannel_t;

typedef uint32_t swow_thannel_size_t;

typedef struct swow_thannel_message_s {
    swow_thread_context_t *from;
    swow_thread_context_t *to;
    zend_string *hash;
} swow_thannel_message_t;

typedef struct swow_thannel_internal_s {
    swow_thannel_size_t capacity;
    swow_thannel_size_t length;
    cat_queue_t producers;
    cat_queue_t consumers;
    cat_queue_t storage;
    uv_mutex_t mutex;
} swow_thannel_internal_t;

/* globals */

CAT_GLOBALS_STRUCT_BEGIN(swow_thannel)
    void *reserved;
CAT_GLOBALS_STRUCT_END(swow_thannel)

extern SWOW_API CAT_GLOBALS_DECLARE(swow_thannel)

#define SWOW_THANNEL_G(x) CAT_GLOBALS_GET(swow_thannel, x)

/* loader */

zend_result swow_thannel_module_init(INIT_FUNC_ARGS);

/* helper*/

static zend_always_inline swow_thannel_t *swow_thannel_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_thannel_t, std);
}

#ifdef __cplusplus
}
#endif
#endif /* SWOW_THANNEL_H */
