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

#include "swow_spsc_queue.h"

#include "swow_shared_object.h"

#include "zend_smart_str.h"

#include "ext/standard/php_var.h"

#include "cat_atomic.h"
#include "cat_time.h"

#include <stdint.h>
#include <string.h>

#define SWOW_SPSC_MAGIC 0x53505343U /* "SPSC" */
#define SWOW_SPSC_VERSION 1

#define SWOW_SPSC_CACHE_LINE 64
#define SWOW_SPSC_SPIN_COUNT 64

#if defined(_MSC_VER)
# include <intrin.h>
# define SWOW_SPSC_ALIGNAS(size) __declspec(align(size))
# define SWOW_SPSC_CPU_RELAX() YieldProcessor()
#else
# define SWOW_SPSC_ALIGNAS(size) _Alignas(size)
# if defined(__x86_64__) || defined(__i386__)
#  define SWOW_SPSC_CPU_RELAX() __builtin_ia32_pause()
# elif defined(__aarch64__) || defined(__arm__)
#  define SWOW_SPSC_CPU_RELAX() __asm__ __volatile__("yield")
# else
#  define SWOW_SPSC_CPU_RELAX() cat_atomic_fence_explicit(CAT_ATOMIC_MEMORY_ORDER_ACQ_REL)
# endif
#endif

typedef struct swow_spsc_slot_header_s {
    uint32_t length;
    uint32_t reserved;
} swow_spsc_slot_header_t;

typedef struct swow_spsc_shared_s {
    uint32_t magic;
    uint32_t version;
    uint32_t capacity;
    uint32_t slot_size;
    uint32_t slot_stride;
    uint32_t reserved;
    uint64_t data_offset;
    uint64_t mapping_size;

    SWOW_SPSC_ALIGNAS(SWOW_SPSC_CACHE_LINE) cat_atomic_uint32_t head;
    SWOW_SPSC_ALIGNAS(SWOW_SPSC_CACHE_LINE) cat_atomic_uint32_t tail;
    SWOW_SPSC_ALIGNAS(SWOW_SPSC_CACHE_LINE) cat_atomic_uint32_t producer_event;
    SWOW_SPSC_ALIGNAS(SWOW_SPSC_CACHE_LINE) cat_atomic_uint32_t consumer_event;
    SWOW_SPSC_ALIGNAS(SWOW_SPSC_CACHE_LINE) cat_atomic_int32_t producer_sleeping;
    SWOW_SPSC_ALIGNAS(SWOW_SPSC_CACHE_LINE) cat_atomic_int32_t consumer_sleeping;
    SWOW_SPSC_ALIGNAS(SWOW_SPSC_CACHE_LINE) cat_atomic_int32_t closed;
    SWOW_SPSC_ALIGNAS(SWOW_SPSC_CACHE_LINE) cat_atomic_uint64_t wake_count;
    SWOW_SPSC_ALIGNAS(SWOW_SPSC_CACHE_LINE) cat_atomic_uint64_t park_count;
} swow_spsc_shared_t;

struct swow_spsc_queue_s {
    cat_shm_t *shm;
    swow_spsc_shared_t *shared;
    char *name;
};

static size_t swow_spsc_align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static swow_spsc_slot_header_t *swow_spsc_get_slot(const swow_spsc_shared_t *shared, uint32_t index)
{
    char *base = (char *) shared + shared->data_offset;

    return (swow_spsc_slot_header_t *) (base + (size_t) index * shared->slot_stride);
}

static void *swow_spsc_get_slot_data(swow_spsc_slot_header_t *slot)
{
    return (char *) slot + sizeof(*slot);
}

static void swow_spsc_increment_wake(swow_spsc_shared_t *shared)
{
    cat_atomic_uint64_fetch_add_explicit(
        &shared->wake_count, 1, CAT_ATOMIC_MEMORY_ORDER_RELAXED);
}

static void swow_spsc_increment_park(swow_spsc_shared_t *shared)
{
    cat_atomic_uint64_fetch_add_explicit(
        &shared->park_count, 1, CAT_ATOMIC_MEMORY_ORDER_RELAXED);
}

static cat_timeout_t swow_spsc_remaining_timeout(cat_timeout_t timeout, cat_msec_t started)
{
    cat_msec_t elapsed;

    if (timeout < 0) {
        return CAT_TIMEOUT_FOREVER;
    }

    elapsed = cat_time_msec() - started;
    if (elapsed >= (cat_msec_t) timeout) {
        return 0;
    }

    return timeout - elapsed;
}

static cat_bool_t swow_spsc_queue_validate(const swow_spsc_shared_t *shared, size_t mapping_size)
{
    if (unlikely(shared->magic != SWOW_SPSC_MAGIC ||
                 shared->version != SWOW_SPSC_VERSION ||
                 shared->capacity == 0 ||
                 shared->slot_size == 0 ||
                 shared->slot_stride < sizeof(swow_spsc_slot_header_t) ||
                 shared->data_offset < sizeof(*shared) ||
                 shared->data_offset > mapping_size)) {
        cat_update_last_error(CAT_EINVAL, "Invalid SPSC queue header");

        return cat_false;
    }

    if (unlikely((uint64_t) shared->data_offset +
                 (uint64_t) shared->capacity * shared->slot_stride > mapping_size)) {
        cat_update_last_error(CAT_EINVAL, "The SPSC queue mapping is too small");

        return cat_false;
    }

    return cat_true;
}

static swow_spsc_queue_t *swow_spsc_queue_alloc(cat_shm_t *shm, const char *name)
{
    swow_spsc_queue_t *queue = (swow_spsc_queue_t *) cat_sys_calloc(1, sizeof(*queue));

    if (unlikely(queue == NULL)) {
        cat_update_last_error(CAT_ENOMEM, "Failed to allocate SPSC queue");
        return NULL;
    }

    queue->shm = shm;
    queue->shared = (swow_spsc_shared_t *) cat_shm_get_address(shm);

    if (name != NULL) {
        size_t length = strlen(name) + 1;

        queue->name = (char *) cat_sys_malloc(length);
        if (unlikely(queue->name == NULL)) {
            cat_update_last_error(CAT_ENOMEM, "Failed to allocate SPSC queue name");
            cat_sys_free(queue);
            return NULL;
        }
        memcpy(queue->name, name, length);
    }

    return queue;
}

static swow_spsc_queue_t *swow_spsc_queue_create(const char *name, size_t capacity, size_t slot_size)
{
    swow_spsc_queue_t *queue;
    cat_shm_t *shm;
    swow_spsc_shared_t *shared;
    size_t data_offset;
    size_t slot_stride;
    size_t mapping_size;

    if (unlikely(capacity == 0 || capacity > UINT32_MAX / 2 ||
                 slot_size == 0 || slot_size > UINT32_MAX - sizeof(swow_spsc_slot_header_t))) {
        cat_update_last_error(CAT_EINVAL, "Invalid SPSC queue capacity or slot size");
        return NULL;
    }

    slot_stride = swow_spsc_align_up(sizeof(swow_spsc_slot_header_t) + slot_size, 8);
    data_offset = swow_spsc_align_up(sizeof(swow_spsc_shared_t), SWOW_SPSC_CACHE_LINE);

    if (unlikely(capacity > (SIZE_MAX - data_offset) / slot_stride)) {
        cat_update_last_error(CAT_EINVAL, "The SPSC queue is too large");
        return NULL;
    }
    mapping_size = data_offset + capacity * slot_stride;

    if (name == NULL) {
        shm = cat_shm_create_anonymous(mapping_size);
    } else {
        shm = cat_shm_create_named(name, mapping_size);
    }
    if (unlikely(shm == NULL)) {
        return NULL;
    }

    queue = swow_spsc_queue_alloc(shm, name);
    if (unlikely(queue == NULL)) {
        cat_shm_detach(shm);
        return NULL;
    }

    shared = queue->shared;
    shared->magic = SWOW_SPSC_MAGIC;
    shared->version = SWOW_SPSC_VERSION;
    shared->capacity = (uint32_t) capacity;
    shared->slot_size = (uint32_t) slot_size;
    shared->slot_stride = (uint32_t) slot_stride;
    shared->reserved = 0;
    shared->data_offset = data_offset;
    shared->mapping_size = mapping_size;

    cat_atomic_uint32_init(&shared->head, 0);
    cat_atomic_uint32_init(&shared->tail, 0);
    cat_atomic_uint32_init(&shared->producer_event, 0);
    cat_atomic_uint32_init(&shared->consumer_event, 0);
    cat_atomic_int32_init(&shared->producer_sleeping, 0);
    cat_atomic_int32_init(&shared->consumer_sleeping, 0);
    cat_atomic_int32_init(&shared->closed, 0);
    cat_atomic_uint64_init(&shared->wake_count, 0);
    cat_atomic_uint64_init(&shared->park_count, 0);

    return queue;
}

swow_spsc_queue_t *swow_spsc_queue_create_anonymous(size_t capacity, size_t slot_size)
{
    return swow_spsc_queue_create(NULL, capacity, slot_size);
}

swow_spsc_queue_t *swow_spsc_queue_create_named(const char *name, size_t capacity, size_t slot_size)
{
    if (unlikely(name == NULL || name[0] == '\0')) {
        cat_update_last_error(CAT_EINVAL, "The SPSC queue name is empty");
        return NULL;
    }

    return swow_spsc_queue_create(name, capacity, slot_size);
}

swow_spsc_queue_t *swow_spsc_queue_attach_named(const char *name)
{
    swow_spsc_queue_t *queue;
    cat_shm_t *shm;
    size_t size;

    if (unlikely(name == NULL || name[0] == '\0')) {
        cat_update_last_error(CAT_EINVAL, "The SPSC queue name is empty");
        return NULL;
    }

    shm = cat_shm_attach_named(name);
    if (unlikely(shm == NULL)) {
        return NULL;
    }

    size = cat_shm_get_size(shm);
    if (unlikely(size < sizeof(swow_spsc_shared_t) ||
                 !swow_spsc_queue_validate((swow_spsc_shared_t *) cat_shm_get_address(shm), size))) {
        cat_shm_detach(shm);
        return NULL;
    }

    queue = swow_spsc_queue_alloc(shm, name);
    if (unlikely(queue == NULL)) {
        cat_shm_detach(shm);
        return NULL;
    }

    return queue;
}

void swow_spsc_queue_detach(swow_spsc_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }

    if (queue->shm != NULL) {
        cat_shm_detach(queue->shm);
    }
    if (queue->name != NULL) {
        cat_sys_free(queue->name);
    }
    cat_sys_free(queue);
}

cat_bool_t swow_spsc_queue_unlink(const char *name)
{
    if (unlikely(name == NULL || name[0] == '\0')) {
        cat_update_last_error(CAT_EINVAL, "The SPSC queue name is empty");
        return cat_false;
    }

    return cat_shm_unlink(name);
}

const char *swow_spsc_queue_get_name(const swow_spsc_queue_t *queue)
{
    return queue != NULL ? queue->name : NULL;
}

size_t swow_spsc_queue_get_capacity(const swow_spsc_queue_t *queue)
{
    return queue != NULL ? queue->shared->capacity : 0;
}

size_t swow_spsc_queue_get_slot_size(const swow_spsc_queue_t *queue)
{
    return queue != NULL ? queue->shared->slot_size : 0;
}

size_t swow_spsc_queue_get_length(const swow_spsc_queue_t *queue)
{
    uint32_t head;
    uint32_t tail;

    if (unlikely(queue == NULL)) {
        return 0;
    }

    head = cat_atomic_uint32_load_explicit(
        &queue->shared->head, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
    tail = cat_atomic_uint32_load_explicit(
        &queue->shared->tail, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);

    return tail - head;
}

cat_bool_t swow_spsc_queue_is_empty(const swow_spsc_queue_t *queue)
{
    return swow_spsc_queue_get_length(queue) == 0;
}

cat_bool_t swow_spsc_queue_is_full(const swow_spsc_queue_t *queue)
{
    return queue != NULL && swow_spsc_queue_get_length(queue) >= queue->shared->capacity;
}

cat_bool_t swow_spsc_queue_is_closed(const swow_spsc_queue_t *queue)
{
    return queue != NULL &&
           cat_atomic_int32_load_explicit(
               &queue->shared->closed, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE) != 0;
}

static cat_bool_t swow_spsc_queue_wait_for_space(
    swow_spsc_queue_t *queue, size_t count, cat_timeout_t timeout, cat_msec_t started)
{
    swow_spsc_shared_t *shared = queue->shared;
    unsigned int spin = 0;

    for (;;) {
        uint32_t head = cat_atomic_uint32_load_explicit(
            &shared->head, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
        uint32_t tail = cat_atomic_uint32_load_explicit(
            &shared->tail, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);

        if (unlikely(swow_spsc_queue_is_closed(queue))) {
            cat_update_last_error(CAT_ECLOSED, "The SPSC queue is closed");
            return cat_false;
        }
        if (likely(tail - head + count <= shared->capacity)) {
            return cat_true;
        }

        if (spin++ < SWOW_SPSC_SPIN_COUNT) {
            SWOW_SPSC_CPU_RELAX();
            continue;
        }

        {
            cat_timeout_t remaining = swow_spsc_remaining_timeout(timeout, started);
            cat_bool_t woke;
            uint32_t event;

            if (unlikely(remaining == 0)) {
                cat_update_last_error(CAT_ETIMEDOUT, "Timed out waiting for SPSC queue space");
                return cat_false;
            }

            event = cat_atomic_uint32_load_explicit(
                &shared->producer_event, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
            cat_atomic_int32_store_explicit(
                &shared->producer_sleeping, 1, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
            swow_spsc_increment_park(shared);

            /* A consumer that made space before the flag store is seen here. */
            head = cat_atomic_uint32_load_explicit(
                &shared->head, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
            tail = cat_atomic_uint32_load_explicit(
                &shared->tail, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
            if (tail - head + count <= shared->capacity) {
                cat_atomic_int32_store_explicit(
                    &shared->producer_sleeping, 0, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
                spin = 0;
                continue;
            }
            if (unlikely(swow_spsc_queue_is_closed(queue))) {
                cat_atomic_int32_store_explicit(
                    &shared->producer_sleeping, 0, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
                cat_update_last_error(CAT_ECLOSED, "The SPSC queue is closed");
                return cat_false;
            }

            woke = cat_futex_wait((cat_futex_t *) &shared->producer_event, (int32_t) event, remaining);
            cat_atomic_int32_store_explicit(
                &shared->producer_sleeping, 0, CAT_ATOMIC_MEMORY_ORDER_RELEASE);

            if (unlikely(!woke)) {
                cat_errno_t error = cat_get_last_error_code();

                if (error == CAT_ETIMEDOUT || error == CAT_ENOTSUP) {
                    return cat_false;
                }
            }
            spin = 0;
        }
    }
}

static cat_bool_t swow_spsc_queue_wait_for_data(
    swow_spsc_queue_t *queue, cat_timeout_t timeout, cat_msec_t started)
{
    swow_spsc_shared_t *shared = queue->shared;
    unsigned int spin = 0;

    for (;;) {
        uint32_t head = cat_atomic_uint32_load_explicit(
            &shared->head, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
        uint32_t tail = cat_atomic_uint32_load_explicit(
            &shared->tail, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);

        if (likely(tail - head > 0)) {
            return cat_true;
        }
        if (unlikely(swow_spsc_queue_is_closed(queue))) {
            cat_update_last_error(CAT_ECLOSED, "The SPSC queue is closed");
            return cat_false;
        }

        if (spin++ < SWOW_SPSC_SPIN_COUNT) {
            SWOW_SPSC_CPU_RELAX();
            continue;
        }

        {
            cat_timeout_t remaining = swow_spsc_remaining_timeout(timeout, started);
            cat_bool_t woke;
            uint32_t event;

            if (unlikely(remaining == 0)) {
                cat_update_last_error(CAT_ETIMEDOUT, "Timed out waiting for SPSC queue data");
                return cat_false;
            }

            event = cat_atomic_uint32_load_explicit(
                &shared->consumer_event, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
            cat_atomic_int32_store_explicit(
                &shared->consumer_sleeping, 1, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
            swow_spsc_increment_park(shared);

            /* A producer that published before the flag store is seen here. */
            head = cat_atomic_uint32_load_explicit(
                &shared->head, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
            tail = cat_atomic_uint32_load_explicit(
                &shared->tail, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
            if (tail - head > 0) {
                cat_atomic_int32_store_explicit(
                    &shared->consumer_sleeping, 0, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
                spin = 0;
                continue;
            }
            if (unlikely(swow_spsc_queue_is_closed(queue))) {
                cat_atomic_int32_store_explicit(
                    &shared->consumer_sleeping, 0, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
                cat_update_last_error(CAT_ECLOSED, "The SPSC queue is closed");
                return cat_false;
            }

            woke = cat_futex_wait((cat_futex_t *) &shared->consumer_event, (int32_t) event, remaining);
            cat_atomic_int32_store_explicit(
                &shared->consumer_sleeping, 0, CAT_ATOMIC_MEMORY_ORDER_RELEASE);

            if (unlikely(!woke)) {
                cat_errno_t error = cat_get_last_error_code();

                if (error == CAT_ETIMEDOUT || error == CAT_ENOTSUP) {
                    return cat_false;
                }
            }
            spin = 0;
        }
    }
}

static void swow_spsc_queue_wake_consumer(swow_spsc_queue_t *queue)
{
    swow_spsc_shared_t *shared = queue->shared;

    cat_atomic_uint32_fetch_add_explicit(
        &shared->consumer_event, 1, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
    if (cat_atomic_int32_load_explicit(
            &shared->consumer_sleeping, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE) != 0) {
        swow_spsc_increment_wake(shared);
        (void) cat_futex_wake((cat_futex_t *) &shared->consumer_event, 1);
    }
}

static void swow_spsc_queue_wake_producer(swow_spsc_queue_t *queue)
{
    swow_spsc_shared_t *shared = queue->shared;

    cat_atomic_uint32_fetch_add_explicit(
        &shared->producer_event, 1, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
    if (cat_atomic_int32_load_explicit(
            &shared->producer_sleeping, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE) != 0) {
        swow_spsc_increment_wake(shared);
        (void) cat_futex_wake((cat_futex_t *) &shared->producer_event, 1);
    }
}

cat_bool_t swow_spsc_queue_push(swow_spsc_queue_t *queue, const void *data, size_t size,
                                  cat_timeout_t timeout)
{
    swow_spsc_item_t item = {
        .data = data,
        .size = size,
    };

    return swow_spsc_queue_pushv(queue, &item, 1, timeout);
}

cat_bool_t swow_spsc_queue_pushv(swow_spsc_queue_t *queue, const swow_spsc_item_t *items,
                                  size_t count, cat_timeout_t timeout)
{
    swow_spsc_shared_t *shared;
    cat_msec_t started;
    uint32_t tail;
    size_t i;

    if (unlikely(queue == NULL)) {
        cat_update_last_error(CAT_EINVAL, "The SPSC queue is not initialized");
        return cat_false;
    }
    if (unlikely(count > queue->shared->capacity)) {
        cat_update_last_error(CAT_EINVAL, "The SPSC queue batch is larger than its capacity");
        return cat_false;
    }
    if (unlikely(items == NULL && count > 0)) {
        cat_update_last_error(CAT_EINVAL, "The SPSC queue batch is null");
        return cat_false;
    }
    for (i = 0; i < count; i++) {
        if (unlikely(items[i].size > queue->shared->slot_size ||
                     (items[i].data == NULL && items[i].size != 0))) {
            cat_update_last_error(CAT_EINVAL, "The SPSC queue message is invalid");
            return cat_false;
        }
    }

    if (unlikely(count == 0)) {
        return cat_true;
    }

    shared = queue->shared;
    started = cat_time_msec();

    if (unlikely(!swow_spsc_queue_wait_for_space(queue, count, timeout, started))) {
        return cat_false;
    }

    tail = cat_atomic_uint32_load_explicit(
        &shared->tail, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);

    for (i = 0; i < count; i++) {
        swow_spsc_slot_header_t *slot =
            swow_spsc_get_slot(shared, (tail + (uint32_t) i) % shared->capacity);

        slot->length = (uint32_t) items[i].size;
        slot->reserved = 0;
        if (items[i].size > 0) {
            memcpy(swow_spsc_get_slot_data(slot), items[i].data, items[i].size);
        }
    }

    cat_atomic_uint32_store_explicit(
        &shared->tail, tail + (uint32_t) count, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
    swow_spsc_queue_wake_consumer(queue);

    return cat_true;
}

cat_bool_t swow_spsc_queue_pop(swow_spsc_queue_t *queue, void **data, size_t *size,
                                 cat_timeout_t timeout)
{
    swow_spsc_message_t message;
    size_t count = 0;
    cat_bool_t result;

    result = swow_spsc_queue_popv(queue, &message, 1, &count, timeout);
    if (unlikely(!result)) {
        return cat_false;
    }

    *data = message.data;
    *size = message.size;

    return cat_true;
}

cat_bool_t swow_spsc_queue_popv(swow_spsc_queue_t *queue, swow_spsc_message_t *messages,
                                 size_t capacity, size_t *count, cat_timeout_t timeout)
{
    swow_spsc_shared_t *shared;
    cat_msec_t started;
    uint32_t head;
    uint32_t tail;
    size_t available;
    size_t taken;
    size_t i;

    if (unlikely(queue == NULL || messages == NULL || count == NULL || capacity == 0)) {
        cat_update_last_error(CAT_EINVAL, "The SPSC queue pop arguments are invalid");
        return cat_false;
    }

    shared = queue->shared;
    started = cat_time_msec();

    if (unlikely(!swow_spsc_queue_wait_for_data(queue, timeout, started))) {
        return cat_false;
    }

    head = cat_atomic_uint32_load_explicit(
        &shared->head, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
    tail = cat_atomic_uint32_load_explicit(
        &shared->tail, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
    available = tail - head;
    taken = available < capacity ? available : capacity;

    for (i = 0; i < taken; i++) {
        swow_spsc_slot_header_t *slot =
            swow_spsc_get_slot(shared, (head + (uint32_t) i) % shared->capacity);
        size_t length = slot->length;
        void *data = cat_sys_malloc(length > 0 ? length : 1);

        if (unlikely(data == NULL)) {
            size_t j;

            for (j = 0; j < i; j++) {
                cat_sys_free(messages[j].data);
            }
            cat_update_last_error(CAT_ENOMEM, "Failed to allocate SPSC queue message");
            return cat_false;
        }

        if (length > 0) {
            memcpy(data, swow_spsc_get_slot_data(slot), length);
        }
        messages[i].data = data;
        messages[i].size = length;
    }

    cat_atomic_uint32_store_explicit(
        &shared->head, head + (uint32_t) taken, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
    swow_spsc_queue_wake_producer(queue);
    *count = taken;

    return cat_true;
}

size_t swow_spsc_queue_drain(swow_spsc_queue_t *queue, swow_spsc_message_t *messages,
                              size_t capacity)
{
    swow_spsc_shared_t *shared;
    uint32_t head;
    uint32_t tail;
    size_t available;
    size_t taken;
    size_t i;

    if (unlikely(queue == NULL || messages == NULL || capacity == 0)) {
        return 0;
    }

    shared = queue->shared;
    head = cat_atomic_uint32_load_explicit(
        &shared->head, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
    tail = cat_atomic_uint32_load_explicit(
        &shared->tail, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE);
    available = tail - head;
    taken = available < capacity ? available : capacity;

    for (i = 0; i < taken; i++) {
        swow_spsc_slot_header_t *slot =
            swow_spsc_get_slot(shared, (head + (uint32_t) i) % shared->capacity);
        size_t length = slot->length;
        void *data = cat_sys_malloc(length > 0 ? length : 1);

        if (unlikely(data == NULL)) {
            size_t j;

            for (j = 0; j < i; j++) {
                cat_sys_free(messages[j].data);
            }
            return 0;
        }

        if (length > 0) {
            memcpy(data, swow_spsc_get_slot_data(slot), length);
        }
        messages[i].data = data;
        messages[i].size = length;
    }

    if (taken > 0) {
        cat_atomic_uint32_store_explicit(
            &shared->head, head + (uint32_t) taken, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
        swow_spsc_queue_wake_producer(queue);
    }

    return taken;
}

void swow_spsc_queue_close(swow_spsc_queue_t *queue)
{
    swow_spsc_shared_t *shared;

    if (queue == NULL) {
        return;
    }

    shared = queue->shared;
    if (cat_atomic_int32_load_explicit(
            &shared->closed, CAT_ATOMIC_MEMORY_ORDER_ACQUIRE) != 0) {
        return;
    }

    cat_atomic_int32_store_explicit(
        &shared->closed, 1, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
    cat_atomic_uint32_fetch_add_explicit(
        &shared->producer_event, 1, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
    cat_atomic_uint32_fetch_add_explicit(
        &shared->consumer_event, 1, CAT_ATOMIC_MEMORY_ORDER_RELEASE);
    (void) cat_futex_wake((cat_futex_t *) &shared->producer_event, -1);
    (void) cat_futex_wake((cat_futex_t *) &shared->consumer_event, -1);
}

uint64_t swow_spsc_queue_get_wake_count(const swow_spsc_queue_t *queue)
{
    return queue != NULL ? cat_atomic_uint64_load_explicit(
               &queue->shared->wake_count, CAT_ATOMIC_MEMORY_ORDER_RELAXED) : 0;
}

uint64_t swow_spsc_queue_get_park_count(const swow_spsc_queue_t *queue)
{
    return queue != NULL ? cat_atomic_uint64_load_explicit(
               &queue->shared->park_count, CAT_ATOMIC_MEMORY_ORDER_RELAXED) : 0;
}

#ifdef ZTS

SWOW_API zend_class_entry *swow_spsc_queue_ce;
SWOW_API zend_class_entry *swow_spsc_queue_exception_ce;
SWOW_API zend_object_handlers swow_spsc_queue_handlers;

#define SWOW_SPSC_QUEUE_SHARED_MAGIC 0x53505351U /* "SPSQ" */

/* the tag of a shared queue handle, so a serialized handle of another class is
 * rejected */
static const char swow_spsc_queue_type[] = "Swow\\SpscQueue";

typedef struct swow_spsc_queue_shared_s {
    swow_shared_object_t object;
    uint32_t magic;
    swow_spsc_queue_t *queue;
} swow_spsc_queue_shared_t;

typedef struct swow_spsc_queue_object_s {
    swow_spsc_queue_shared_t *queue;
    zend_object std;
} swow_spsc_queue_object_t;

static swow_spsc_queue_object_t *swow_spsc_queue_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_spsc_queue_object_t, std);
}

static cat_bool_t swow_spsc_queue_shared_free(swow_shared_object_t *object)
{
    swow_spsc_queue_shared_t *shared = (swow_spsc_queue_shared_t *) object;

    if (shared->queue != NULL) {
        swow_spsc_queue_detach(shared->queue);
        shared->queue = NULL;
    }

    return cat_true;
}

static zend_object *swow_spsc_queue_create_object(zend_class_entry *ce)
{
    swow_spsc_queue_object_t *spsc_queue =
        swow_object_alloc(swow_spsc_queue_object_t, ce, swow_spsc_queue_handlers);

    spsc_queue->queue = NULL;

    return &spsc_queue->std;
}

static void swow_spsc_queue_free_object(zend_object *object)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(object);

    if (spsc_queue->queue != NULL) {
        swow_shared_object_release(&spsc_queue->queue->object);
        spsc_queue->queue = NULL;
    }

    zend_object_std_dtor(&spsc_queue->std);
}

static cat_bool_t swow_spsc_queue_assign(
    swow_spsc_queue_object_t *spsc_queue, swow_spsc_queue_t *queue)
{
    swow_spsc_queue_shared_t *shared =
        (swow_spsc_queue_shared_t *) swow_shared_object_create(
            sizeof(swow_spsc_queue_shared_t), swow_spsc_queue_type, swow_spsc_queue_shared_free);

    if (unlikely(shared == NULL)) {
        swow_spsc_queue_detach(queue);
        return cat_false;
    }

    shared->magic = SWOW_SPSC_QUEUE_SHARED_MAGIC;
    shared->queue = queue;
    if (spsc_queue->queue != NULL) {
        swow_shared_object_release(&spsc_queue->queue->object);
    }
    spsc_queue->queue = shared;

    return cat_true;
}

static cat_bool_t swow_spsc_queue_wrap(zval *return_value, swow_spsc_queue_t *queue)
{
    swow_spsc_queue_object_t *spsc_queue;

    object_init_ex(return_value, swow_spsc_queue_ce);
    spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(return_value));
    if (unlikely(!swow_spsc_queue_assign(spsc_queue, queue))) {
        zend_throw_exception(swow_spsc_queue_exception_ce,
                             "Failed to register the SPSC queue", 0);
        return cat_false;
    }

    return cat_true;
}

static cat_bool_t swow_spsc_queue_serialize_value(zval *value, smart_str *buffer)
{
    php_serialize_data_t var_hash;

    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(buffer, value, &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);

    if (unlikely(EG(exception) != NULL || buffer->s == NULL)) {
        smart_str_free(buffer);
        return cat_false;
    }

    return cat_true;
}

static cat_bool_t swow_spsc_queue_unserialize_value(
    zval *return_value, const void *data, size_t size)
{
    php_unserialize_data_t var_hash;
    const unsigned char *p = (const unsigned char *) data;
    cat_bool_t ok;

    ZVAL_UNDEF(return_value);
    PHP_VAR_UNSERIALIZE_INIT(var_hash);
    ok = php_var_unserialize(return_value, &p, p + size, &var_hash);
    PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

    if (unlikely(!ok || EG(exception) != NULL)) {
        if (Z_TYPE_P(return_value) != IS_UNDEF) {
            zval_ptr_dtor(return_value);
        }
        ZVAL_UNDEF(return_value);
        return cat_false;
    }

    return cat_true;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_SpscQueue___construct, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, capacity, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maxMessageSize, IS_LONG, 0, "8192")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_SpscQueue_createNamed, 0, 2, Swow\\SpscQueue, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, capacity, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maxMessageSize, IS_LONG, 0, "8192")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_SpscQueue_attachNamed, 0, 1, Swow\\SpscQueue, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_unlink, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_getName, 0, 0, IS_STRING, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_SpscQueue_push, 0, 1, Swow\\SpscQueue, 0)
    ZEND_ARG_INFO(0, value)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_pop, 0, 0, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_pushMany, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, values, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_popMany, 0, 1, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, max, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_drain, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_isClosed, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_getCapacity, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_getMaxMessageSize, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_getLength, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_isEmpty, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_isFull, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_getWakeCount, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue_getParkCount, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue___serialize, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue___unserialize, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_SpscQueue___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_SpscQueue, __construct)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long capacity;
    zend_long max_message_size = 8192;
    swow_spsc_queue_t *queue;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(capacity)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(max_message_size)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(capacity <= 0 || max_message_size <= 0)) {
        zend_value_error("The capacity and maxMessageSize must be positive");
        RETURN_THROWS();
    }
    if (unlikely(spsc_queue->queue != NULL)) {
        zend_throw_exception(swow_spsc_queue_exception_ce,
                             "SpscQueue has already been constructed", 0);
        RETURN_THROWS();
    }

    queue = swow_spsc_queue_create_anonymous(
        (size_t) capacity, (size_t) max_message_size);
    if (unlikely(queue == NULL)) {
        swow_throw_exception_with_last(swow_spsc_queue_exception_ce);
        RETURN_THROWS();
    }

    if (unlikely(!swow_spsc_queue_assign(spsc_queue, queue))) {
        zend_throw_exception(swow_spsc_queue_exception_ce,
                             "Failed to register the SPSC queue", 0);
        RETURN_THROWS();
    }
}

static PHP_METHOD(Swow_SpscQueue, createNamed)
{
    zend_string *name;
    zend_long capacity;
    zend_long max_message_size = 8192;
    swow_spsc_queue_t *queue;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_STR(name)
        Z_PARAM_LONG(capacity)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(max_message_size)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(capacity <= 0 || max_message_size <= 0)) {
        zend_value_error("The capacity and maxMessageSize must be positive");
        RETURN_THROWS();
    }

    queue = swow_spsc_queue_create_named(
        ZSTR_VAL(name), (size_t) capacity, (size_t) max_message_size);
    if (unlikely(queue == NULL)) {
        swow_throw_exception_with_last(swow_spsc_queue_exception_ce);
        RETURN_THROWS();
    }

    if (unlikely(!swow_spsc_queue_wrap(return_value, queue))) {
        RETURN_THROWS();
    }
}

static PHP_METHOD(Swow_SpscQueue, attachNamed)
{
    zend_string *name;
    swow_spsc_queue_t *queue;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END();

    queue = swow_spsc_queue_attach_named(ZSTR_VAL(name));
    if (unlikely(queue == NULL)) {
        swow_throw_exception_with_last(swow_spsc_queue_exception_ce);
        RETURN_THROWS();
    }

    if (unlikely(!swow_spsc_queue_wrap(return_value, queue))) {
        RETURN_THROWS();
    }
}

static PHP_METHOD(Swow_SpscQueue, unlink)
{
    zend_string *name;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(!swow_spsc_queue_unlink(ZSTR_VAL(name)))) {
        swow_throw_exception_with_last(swow_spsc_queue_exception_ce);
        RETURN_THROWS();
    }
}

static PHP_METHOD(Swow_SpscQueue, getName)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));
    const char *name;

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(spsc_queue->queue == NULL)) {
        RETURN_NULL();
    }

    name = swow_spsc_queue_get_name(spsc_queue->queue->queue);
    if (name == NULL) {
        RETURN_NULL();
    }

    RETURN_STRING(name);
}

static PHP_METHOD(Swow_SpscQueue, push)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *value;
    zend_long timeout = -1;
    smart_str buffer = { 0 };

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ZVAL(value)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(spsc_queue->queue == NULL)) {
        zend_throw_exception(swow_spsc_queue_exception_ce,
                             "SpscQueue has not been constructed", 0);
        RETURN_THROWS();
    }

    if (unlikely(!swow_spsc_queue_serialize_value(value, &buffer))) {
        if (EG(exception) == NULL) {
            zend_throw_exception(swow_spsc_queue_exception_ce,
                                 "Failed to serialize the message", 0);
        }
        RETURN_THROWS();
    }

    if (unlikely(ZSTR_LEN(buffer.s) > swow_spsc_queue_get_slot_size(spsc_queue->queue->queue))) {
        smart_str_free(&buffer);
        zend_value_error("The serialized message is larger than maxMessageSize");
        RETURN_THROWS();
    }

    if (unlikely(!swow_spsc_queue_push(
                     spsc_queue->queue->queue, ZSTR_VAL(buffer.s), ZSTR_LEN(buffer.s), timeout))) {
        smart_str_free(&buffer);
        swow_throw_exception_with_last(swow_spsc_queue_exception_ce);
        RETURN_THROWS();
    }

    smart_str_free(&buffer);
    RETURN_THIS();
}

static PHP_METHOD(Swow_SpscQueue, pop)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long timeout = -1;
    void *data = NULL;
    size_t size = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(spsc_queue->queue == NULL)) {
        zend_throw_exception(swow_spsc_queue_exception_ce,
                             "SpscQueue has not been constructed", 0);
        RETURN_THROWS();
    }

    if (unlikely(!swow_spsc_queue_pop(spsc_queue->queue->queue, &data, &size, timeout))) {
        swow_throw_exception_with_last(swow_spsc_queue_exception_ce);
        RETURN_THROWS();
    }

    if (unlikely(data == NULL || size == 0 ||
                 !swow_spsc_queue_unserialize_value(return_value, data, size))) {
        if (data != NULL) {
            cat_sys_free(data);
        }
        if (EG(exception) == NULL) {
            zend_throw_exception(swow_spsc_queue_exception_ce,
                                 "Failed to unserialize the message", 0);
        }
        RETURN_THROWS();
    }

    cat_sys_free(data);
}

static PHP_METHOD(Swow_SpscQueue, pushMany)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *values;
    zend_long timeout = -1;
    smart_str *buffers;
    swow_spsc_item_t *items;
    uint32_t count;
    uint32_t i = 0;
    zval *value;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ARRAY(values)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(spsc_queue->queue == NULL)) {
        zend_throw_exception(swow_spsc_queue_exception_ce,
                             "SpscQueue has not been constructed", 0);
        RETURN_THROWS();
    }

    count = zend_hash_num_elements(Z_ARRVAL_P(values));
    if (count == 0) {
        RETURN_LONG(0);
    }

    buffers = (smart_str *) ecalloc(count, sizeof(*buffers));
    items = (swow_spsc_item_t *) ecalloc(count, sizeof(*items));

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(values), value) {
        if (unlikely(!swow_spsc_queue_serialize_value(value, &buffers[i]))) {
            if (EG(exception) == NULL) {
                zend_throw_exception(swow_spsc_queue_exception_ce,
                                     "Failed to serialize the message", 0);
            }
            goto _serialize_error;
        }
        if (unlikely(ZSTR_LEN(buffers[i].s) >
                     swow_spsc_queue_get_slot_size(spsc_queue->queue->queue))) {
            zend_value_error("The serialized message is larger than maxMessageSize");
            goto _serialize_error;
        }
        items[i].data = ZSTR_VAL(buffers[i].s);
        items[i].size = ZSTR_LEN(buffers[i].s);
        i++;
    } ZEND_HASH_FOREACH_END();

    if (unlikely(!swow_spsc_queue_pushv(spsc_queue->queue->queue, items, count, timeout))) {
        swow_throw_exception_with_last(swow_spsc_queue_exception_ce);
        goto _serialize_error;
    }

    for (i = 0; i < count; i++) {
        smart_str_free(&buffers[i]);
    }
    efree(items);
    efree(buffers);
    RETURN_LONG((zend_long) count);

    _serialize_error:
    for (i = 0; i < count; i++) {
        smart_str_free(&buffers[i]);
    }
    efree(items);
    efree(buffers);
    RETURN_THROWS();
}

static PHP_METHOD(Swow_SpscQueue, popMany)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_long max;
    zend_long timeout = -1;
    swow_spsc_message_t *messages;
    size_t capacity;
    size_t limit;
    size_t count = 0;
    size_t i;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(max)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (unlikely(spsc_queue->queue == NULL)) {
        zend_throw_exception(swow_spsc_queue_exception_ce,
                             "SpscQueue has not been constructed", 0);
        RETURN_THROWS();
    }
    if (unlikely(max <= 0)) {
        zend_value_error("The max must be positive");
        RETURN_THROWS();
    }

    capacity = swow_spsc_queue_get_capacity(spsc_queue->queue->queue);
    limit = (size_t) max < capacity ? (size_t) max : capacity;
    messages = (swow_spsc_message_t *) ecalloc(limit, sizeof(*messages));
    if (unlikely(!swow_spsc_queue_popv(
                     spsc_queue->queue->queue, messages, limit, &count, timeout))) {
        efree(messages);
        swow_throw_exception_with_last(swow_spsc_queue_exception_ce);
        RETURN_THROWS();
    }

    array_init(return_value);
    for (i = 0; i < count; i++) {
        zval value;

        if (unlikely(!swow_spsc_queue_unserialize_value(
                         &value, messages[i].data, messages[i].size))) {
            size_t j;

            cat_sys_free(messages[i].data);
            for (j = i + 1; j < count; j++) {
                cat_sys_free(messages[j].data);
            }
            efree(messages);
            zval_ptr_dtor(return_value);
            if (EG(exception) == NULL) {
                zend_throw_exception(swow_spsc_queue_exception_ce,
                                     "Failed to unserialize the message", 0);
            }
            RETURN_THROWS();
        }
        add_next_index_zval(return_value, &value);
        cat_sys_free(messages[i].data);
    }

    efree(messages);
}

static PHP_METHOD(Swow_SpscQueue, drain)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));
    swow_spsc_message_t *messages;
    size_t capacity;
    size_t count;
    size_t i;

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(spsc_queue->queue == NULL)) {
        zend_throw_exception(swow_spsc_queue_exception_ce,
                             "SpscQueue has not been constructed", 0);
        RETURN_THROWS();
    }

    capacity = swow_spsc_queue_get_capacity(spsc_queue->queue->queue);
    if (capacity == 0) {
        RETURN_EMPTY_ARRAY();
    }

    messages = (swow_spsc_message_t *) ecalloc(capacity, sizeof(*messages));
    count = swow_spsc_queue_drain(spsc_queue->queue->queue, messages, capacity);

    array_init(return_value);
    for (i = 0; i < count; i++) {
        zval value;

        if (unlikely(!swow_spsc_queue_unserialize_value(
                         &value, messages[i].data, messages[i].size))) {
            size_t j;

            cat_sys_free(messages[i].data);
            for (j = i + 1; j < count; j++) {
                cat_sys_free(messages[j].data);
            }
            efree(messages);
            zval_ptr_dtor(return_value);
            if (EG(exception) == NULL) {
                zend_throw_exception(swow_spsc_queue_exception_ce,
                                     "Failed to unserialize the message", 0);
            }
            RETURN_THROWS();
        }
        add_next_index_zval(return_value, &value);
        cat_sys_free(messages[i].data);
    }

    efree(messages);
}

static PHP_METHOD(Swow_SpscQueue, close)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    if (spsc_queue->queue != NULL) {
        swow_spsc_queue_close(spsc_queue->queue->queue);
    }
}

static PHP_METHOD(Swow_SpscQueue, isClosed)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(spsc_queue->queue != NULL &&
                swow_spsc_queue_is_closed(spsc_queue->queue->queue));
}

static PHP_METHOD(Swow_SpscQueue, getCapacity)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(spsc_queue->queue != NULL ?
                (zend_long) swow_spsc_queue_get_capacity(spsc_queue->queue->queue) : 0);
}

static PHP_METHOD(Swow_SpscQueue, getMaxMessageSize)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(spsc_queue->queue != NULL ?
                (zend_long) swow_spsc_queue_get_slot_size(spsc_queue->queue->queue) : 0);
}

static PHP_METHOD(Swow_SpscQueue, getLength)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(spsc_queue->queue != NULL ?
                (zend_long) swow_spsc_queue_get_length(spsc_queue->queue->queue) : 0);
}

static PHP_METHOD(Swow_SpscQueue, isEmpty)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(spsc_queue->queue == NULL ||
                swow_spsc_queue_is_empty(spsc_queue->queue->queue));
}

static PHP_METHOD(Swow_SpscQueue, isFull)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(spsc_queue->queue != NULL &&
                swow_spsc_queue_is_full(spsc_queue->queue->queue));
}

static PHP_METHOD(Swow_SpscQueue, getWakeCount)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(spsc_queue->queue != NULL ?
                (zend_long) swow_spsc_queue_get_wake_count(spsc_queue->queue->queue) : 0);
}

static PHP_METHOD(Swow_SpscQueue, getParkCount)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(spsc_queue->queue != NULL ?
                (zend_long) swow_spsc_queue_get_park_count(spsc_queue->queue->queue) : 0);
}

static PHP_METHOD(Swow_SpscQueue, __serialize)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));
    const char *name;

    ZEND_PARSE_PARAMETERS_NONE();

    if (unlikely(spsc_queue->queue == NULL)) {
        zend_throw_exception(swow_spsc_queue_exception_ce,
                             "SpscQueue has not been constructed", 0);
        RETURN_THROWS();
    }

    name = swow_spsc_queue_get_name(spsc_queue->queue->queue);
    if (name != NULL) {
        array_init(return_value);
        add_assoc_string(return_value, "name", name);
        return;
    }

    swow_shared_object_serialize(return_value, &spsc_queue->queue->object);
}

static PHP_METHOD(Swow_SpscQueue, __unserialize)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));
    zval *data;
    zval *z_name;
    swow_spsc_queue_t *queue;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY(data)
    ZEND_PARSE_PARAMETERS_END();

    z_name = zend_hash_str_find(Z_ARRVAL_P(data), ZEND_STRL("name"));
    if (z_name != NULL) {
        if (unlikely(Z_TYPE_P(z_name) != IS_STRING)) {
            zend_throw_exception(swow_spsc_queue_exception_ce,
                                 "Invalid serialized SpscQueue", 0);
            RETURN_THROWS();
        }
        queue = swow_spsc_queue_attach_named(Z_STRVAL_P(z_name));
        if (unlikely(queue == NULL)) {
            swow_throw_exception_with_last(swow_spsc_queue_exception_ce);
            RETURN_THROWS();
        }
    } else {
        swow_shared_object_t *object;

        if (unlikely(!swow_shared_object_unserialize(Z_ARRVAL_P(data), swow_spsc_queue_type,
                                                     &object))) {
            swow_throw_exception_with_last(swow_spsc_queue_exception_ce);
            RETURN_THROWS();
        }
        {
            swow_spsc_queue_shared_t *shared = (swow_spsc_queue_shared_t *) object;

            if (unlikely(shared->magic != SWOW_SPSC_QUEUE_SHARED_MAGIC ||
                         shared->queue == NULL)) {
                swow_shared_object_release(object);
                zend_throw_exception(swow_spsc_queue_exception_ce,
                                     "Invalid serialized SpscQueue", 0);
                RETURN_THROWS();
            }
            if (spsc_queue->queue != NULL) {
                swow_shared_object_release(&spsc_queue->queue->object);
            }
            spsc_queue->queue = shared;
        }
        return;
    }

    if (unlikely(!swow_spsc_queue_assign(spsc_queue, queue))) {
        zend_throw_exception(swow_spsc_queue_exception_ce,
                             "Failed to register the SPSC queue", 0);
        RETURN_THROWS();
    }
}

static PHP_METHOD(Swow_SpscQueue, __debugInfo)
{
    swow_spsc_queue_object_t *spsc_queue = swow_spsc_queue_get_from_object(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);
    if (spsc_queue->queue != NULL) {
        const char *name = swow_spsc_queue_get_name(spsc_queue->queue->queue);

        if (name != NULL) {
            add_assoc_string(return_value, "name", name);
        }
        add_assoc_long(return_value, "capacity",
                       (zend_long) swow_spsc_queue_get_capacity(spsc_queue->queue->queue));
        add_assoc_long(return_value, "maxMessageSize",
                       (zend_long) swow_spsc_queue_get_slot_size(spsc_queue->queue->queue));
        add_assoc_long(return_value, "length",
                       (zend_long) swow_spsc_queue_get_length(spsc_queue->queue->queue));
        add_assoc_bool(return_value, "closed",
                       swow_spsc_queue_is_closed(spsc_queue->queue->queue));
    }
}

static const zend_function_entry swow_spsc_queue_methods[] = {
    PHP_ME(Swow_SpscQueue, __construct,      arginfo_class_Swow_SpscQueue___construct,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, createNamed,      arginfo_class_Swow_SpscQueue_createNamed,      ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_SpscQueue, attachNamed,      arginfo_class_Swow_SpscQueue_attachNamed,      ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_SpscQueue, unlink,           arginfo_class_Swow_SpscQueue_unlink,           ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_SpscQueue, getName,          arginfo_class_Swow_SpscQueue_getName,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, push,             arginfo_class_Swow_SpscQueue_push,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, pop,              arginfo_class_Swow_SpscQueue_pop,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, pushMany,         arginfo_class_Swow_SpscQueue_pushMany,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, popMany,          arginfo_class_Swow_SpscQueue_popMany,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, drain,            arginfo_class_Swow_SpscQueue_drain,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, close,            arginfo_class_Swow_SpscQueue_close,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, isClosed,         arginfo_class_Swow_SpscQueue_isClosed,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, getCapacity,      arginfo_class_Swow_SpscQueue_getCapacity,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, getMaxMessageSize, arginfo_class_Swow_SpscQueue_getMaxMessageSize, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, getLength,        arginfo_class_Swow_SpscQueue_getLength,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, isEmpty,          arginfo_class_Swow_SpscQueue_isEmpty,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, isFull,           arginfo_class_Swow_SpscQueue_isFull,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, getWakeCount,     arginfo_class_Swow_SpscQueue_getWakeCount,     ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, getParkCount,     arginfo_class_Swow_SpscQueue_getParkCount,     ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, __serialize,      arginfo_class_Swow_SpscQueue___serialize,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, __unserialize,    arginfo_class_Swow_SpscQueue___unserialize,    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_SpscQueue, __debugInfo,      arginfo_class_Swow_SpscQueue___debugInfo,      ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_spsc_queue_module_init(INIT_FUNC_ARGS)
{
    swow_spsc_queue_exception_ce = swow_register_internal_class(
        "Swow\\SpscQueueException", swow_exception_ce, NULL, NULL, NULL,
        cat_true, cat_true, NULL, NULL, 0
    );

    swow_spsc_queue_ce = swow_register_internal_class(
        "Swow\\SpscQueue", NULL, swow_spsc_queue_methods,
        &swow_spsc_queue_handlers, NULL,
        cat_false, cat_true,
        swow_spsc_queue_create_object, swow_spsc_queue_free_object,
        offsetof(swow_spsc_queue_object_t, std)
    );

    return SUCCESS;
}

zend_result swow_spsc_queue_module_shutdown(SHUTDOWN_FUNC_ARGS)
{
    return SUCCESS;
}

#endif /* ZTS */
