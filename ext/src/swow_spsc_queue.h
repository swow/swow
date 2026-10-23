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

#ifndef SWOW_SPSC_QUEUE_H
#define SWOW_SPSC_QUEUE_H

#include "swow.h"

#include "cat_futex.h"
#include "cat_shm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct swow_spsc_queue_s swow_spsc_queue_t;

typedef struct swow_spsc_item_s {
    const void *data;
    size_t size;
} swow_spsc_item_t;

typedef struct swow_spsc_message_s {
    void *data;
    size_t size;
} swow_spsc_message_t;

/*
 * The queue is a fixed-size ring of fixed-size slots. One thread produces and
 * one thread consumes. The calls block the calling thread when they wait, so
 * they are for a worker thread that runs one coroutine.
 *
 * Both the anonymous and the named mapping store the queue in shared memory.
 * An anonymous queue is visible to threads of this process and to a forked
 * child. A named queue is visible to another process that attaches the same
 * name.
 */

/* capacity and slot_size must be greater than zero. */
swow_spsc_queue_t *swow_spsc_queue_create_anonymous(size_t capacity, size_t slot_size);
swow_spsc_queue_t *swow_spsc_queue_create_named(const char *name, size_t capacity, size_t slot_size);
swow_spsc_queue_t *swow_spsc_queue_attach_named(const char *name);

void swow_spsc_queue_detach(swow_spsc_queue_t *queue);
cat_bool_t swow_spsc_queue_unlink(const char *name);

/* NULL for an anonymous queue. */
const char *swow_spsc_queue_get_name(const swow_spsc_queue_t *queue);
size_t swow_spsc_queue_get_capacity(const swow_spsc_queue_t *queue);
size_t swow_spsc_queue_get_slot_size(const swow_spsc_queue_t *queue);
size_t swow_spsc_queue_get_length(const swow_spsc_queue_t *queue);
cat_bool_t swow_spsc_queue_is_empty(const swow_spsc_queue_t *queue);
cat_bool_t swow_spsc_queue_is_full(const swow_spsc_queue_t *queue);
cat_bool_t swow_spsc_queue_is_closed(const swow_spsc_queue_t *queue);

/* A push of a zero-sized item is valid. */
cat_bool_t swow_spsc_queue_push(swow_spsc_queue_t *queue, const void *data, size_t size,
                                  cat_timeout_t timeout);
cat_bool_t swow_spsc_queue_pop(swow_spsc_queue_t *queue, void **data, size_t *size,
                                 cat_timeout_t timeout);

/* The batch enters as a whole, or not at all. */
cat_bool_t swow_spsc_queue_pushv(swow_spsc_queue_t *queue, const swow_spsc_item_t *items,
                                  size_t count, cat_timeout_t timeout);
/* It takes at least one message when the queue is not empty. The caller frees
 * each returned data pointer with cat_sys_free. */
cat_bool_t swow_spsc_queue_popv(swow_spsc_queue_t *queue, swow_spsc_message_t *messages,
                                 size_t capacity, size_t *count, cat_timeout_t timeout);
/* It never waits and returns zero when the queue is empty. */
size_t swow_spsc_queue_drain(swow_spsc_queue_t *queue, swow_spsc_message_t *messages,
                              size_t capacity);

/* Stop further pushes and wake both sides. */
void swow_spsc_queue_close(swow_spsc_queue_t *queue);

uint64_t swow_spsc_queue_get_wake_count(const swow_spsc_queue_t *queue);
uint64_t swow_spsc_queue_get_park_count(const swow_spsc_queue_t *queue);

#ifdef ZTS

extern SWOW_API zend_class_entry *swow_spsc_queue_ce;
extern SWOW_API zend_class_entry *swow_spsc_queue_exception_ce;
extern SWOW_API zend_object_handlers swow_spsc_queue_handlers;

zend_result swow_spsc_queue_module_init(INIT_FUNC_ARGS);
zend_result swow_spsc_queue_module_shutdown(SHUTDOWN_FUNC_ARGS);

#endif /* ZTS */

#ifdef __cplusplus
}
#endif

#endif /* SWOW_SPSC_QUEUE_H */
