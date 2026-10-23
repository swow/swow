/*
  +--------------------------------------------------------------------------+
  | libcat                                                                   |
  +--------------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License");          |
  | you may not use this file except in compliance with the License.         |
  | You may obtain a copy of the License at                                  |
  | http://www.apache.org/licenses/LICENSE-2.0                               |
  +--------------------------------------------------------------------------+
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#include "cat.h"
#include "cat_thannel.h"

#ifdef CAT_THREAD_SAFE

/* How many messages a queue holds before a push waits. A rendezvous holds one
 * while its producer waits for the take. */
static cat_always_inline cat_thannel_size_t cat_thannel_limit(const cat_thannel_t *thannel)
{
    return thannel->capacity == CAT_THANNEL_CAPACITY_UNBUFFERED ? 1 : thannel->capacity;
}

static cat_always_inline cat_bool_t cat_thannel_is_rendezvous(const cat_thannel_t *thannel)
{
    return thannel->capacity == CAT_THANNEL_CAPACITY_UNBUFFERED ? cat_true : cat_false;
}

/* The caller must hold the mutex. */
static void cat_thannel_enqueue(cat_thannel_t *thannel, cat_thannel_message_t *message)
{
    cat_queue_push_back(&thannel->messages, &message->node);
    thannel->length++;
}

/* The caller must hold the mutex. */
static cat_thannel_message_t *cat_thannel_dequeue(cat_thannel_t *thannel)
{
    cat_thannel_message_t *message;

    if (cat_queue_empty(&thannel->messages)) {
        return NULL;
    }

    message = cat_queue_front_data(&thannel->messages, cat_thannel_message_t, node);
    cat_queue_remove(&message->node);
    thannel->length--;

    return message;
}

/* The caller must hold the mutex. */
static void cat_thannel_take_back(cat_thannel_t *thannel, cat_thannel_message_t *message)
{
    cat_queue_remove(&message->node);
    thannel->length--;
}

static void cat_thannel_destroy_message(cat_thannel_message_t *message)
{
    cat_sys_free(message->data);
    cat_sys_free(message);
}

/* The caller must hold the mutex. */
static cat_thannel_message_t *cat_thannel_alloc_message(cat_thannel_t *thannel)
{
    cat_thannel_message_t *message;

    if (cat_queue_empty(&thannel->free_messages)) {
        return (cat_thannel_message_t *) cat_sys_calloc(1, sizeof(cat_thannel_message_t));
    }

    message = cat_queue_front_data(&thannel->free_messages, cat_thannel_message_t, node);
    cat_queue_remove(&message->node);

    return message;
}

/* The caller must hold the mutex, and own the message. */
static void cat_thannel_recycle(cat_thannel_t *thannel, cat_thannel_message_t *message)
{
    cat_sys_free(message->data);
    message->data = NULL;
    message->size = 0;
    message->taken = cat_false;

    cat_queue_push_back(&thannel->free_messages, &message->node);
}

/* Copy one value into the queue. The caller must hold the mutex. */
static cat_bool_t cat_thannel_append(cat_thannel_t *thannel, const cat_thannel_item_t *item)
{
    cat_thannel_message_t *message = cat_thannel_alloc_message(thannel);

    if (unlikely(message == NULL)) {
        cat_update_last_error(CAT_ENOMEM, "Allocate the message failed");
        return cat_false;
    }

    if (item->size > 0) {
        message->data = cat_sys_malloc(item->size);
        if (unlikely(message->data == NULL)) {
            cat_thannel_recycle(thannel, message);
            cat_update_last_error(CAT_ENOMEM, "Allocate the message data failed");
            return cat_false;
        }
        memcpy(message->data, item->data, item->size);
        message->size = item->size;
    }

    cat_thannel_enqueue(thannel, message);

    return cat_true;
}

/* Take the last count messages back out of the queue, so a batch push that
 * failed half way leaves nothing behind. The caller must hold the mutex. */
static void cat_thannel_rollback(cat_thannel_t *thannel, size_t count)
{
    cat_thannel_message_t *message;

    while (count-- > 0) {
        message = cat_queue_back_data(&thannel->messages, cat_thannel_message_t, node);
        cat_thannel_take_back(thannel, message);
        cat_thannel_recycle(thannel, message);
    }
}

/* Take up to capacity messages and give their values to the caller. The caller
 * must hold the mutex. */
static size_t cat_thannel_take_messages(cat_thannel_t *thannel, cat_thannel_item_t *items,
                                        size_t capacity)
{
    cat_thannel_message_t *message;
    size_t count = 0;

    while (count < capacity && (message = cat_thannel_dequeue(thannel)) != NULL) {
        items[count].data = message->data;
        items[count].size = message->size;
        count++;

        /* Detach the value, because a rendezvous producer frees the message as
         * soon as it sees the take. */
        message->data = NULL;
        message->size = 0;
        message->taken = cat_true;

        if (!cat_thannel_is_rendezvous(thannel)) {
            cat_thannel_recycle(thannel, message);
        }
    }

    return count;
}

/* A wait that a wakeup ended must not restart the timeout, so the deadline is
 * fixed once for the whole call. */
static cat_always_inline cat_timeout_t cat_thannel_deadline(cat_timeout_t timeout)
{
    return timeout == CAT_TIMEOUT_FOREVER ? CAT_TIMEOUT_FOREVER
                                          : (cat_timeout_t) cat_time_msec() + timeout;
}

static cat_timeout_t cat_thannel_remaining(cat_timeout_t deadline)
{
    cat_msec_t now;

    if (deadline == CAT_TIMEOUT_FOREVER) {
        return CAT_TIMEOUT_FOREVER;
    }

    now = cat_time_msec();
    if (now >= (cat_msec_t) deadline) {
        return 0;
    }

    return (cat_timeout_t) ((cat_msec_t) deadline - now);
}

CAT_API cat_thannel_t *cat_thannel_create(cat_thannel_t *thannel, cat_thannel_size_t capacity)
{
    if (unlikely(thannel == NULL)) {
        cat_update_last_error(CAT_EINVAL, "The thannel is null");
        return NULL;
    }

    memset(thannel, 0, sizeof(*thannel));
    thannel->capacity = capacity;
    cat_queue_init(&thannel->messages);
    cat_queue_init(&thannel->free_messages);

    if (unlikely(!cat_mutex_init(&thannel->mutex))) {
        return NULL;
    }
    if (unlikely(!cat_cond_init(&thannel->readable))) {
        (void) cat_mutex_destroy(&thannel->mutex);
        return NULL;
    }
    if (unlikely(!cat_cond_init(&thannel->writable))) {
        cat_cond_destroy(&thannel->readable);
        (void) cat_mutex_destroy(&thannel->mutex);
        return NULL;
    }

    return thannel;
}

CAT_API cat_bool_t cat_thannel_pushv(cat_thannel_t *thannel, const cat_thannel_item_t *items,
                                     size_t count, cat_timeout_t timeout)
{
    cat_thannel_message_t *message;
    cat_timeout_t deadline;
    size_t index;

    if (unlikely(thannel == NULL)) {
        cat_update_last_error(CAT_EINVAL, "The thannel is null");
        return cat_false;
    }
    if (unlikely(items == NULL && count > 0)) {
        cat_update_last_error(CAT_EINVAL, "The items are null but the count is not zero");
        return cat_false;
    }
    if (unlikely(count > cat_thannel_limit(thannel))) {
        cat_update_last_error(CAT_EINVAL, "The batch is larger than the queue holds");
        return cat_false;
    }
    for (index = 0; index < count; index++) {
        if (unlikely(items[index].data == NULL && items[index].size > 0)) {
            cat_update_last_error(CAT_EINVAL, "The data is null but the size is not zero");
            return cat_false;
        }
    }
    if (count == 0) {
        return cat_true;
    }

    if (unlikely(!cat_mutex_lock(&thannel->mutex, CAT_TIMEOUT_FOREVER))) {
        return cat_false;
    }

    deadline = cat_thannel_deadline(timeout);

    while (!thannel->closed && thannel->length + count > cat_thannel_limit(thannel)) {
        thannel->park_count++;
        if (unlikely(!cat_cond_wait(&thannel->writable, &thannel->mutex,
            cat_thannel_remaining(deadline)))) {
            cat_mutex_unlock(&thannel->mutex);
            return cat_false;
        }
    }

    if (unlikely(thannel->closed)) {
        cat_mutex_unlock(&thannel->mutex);
        cat_update_last_error(CAT_ECLOSED, "The thannel is closed");
        return cat_false;
    }

    for (index = 0; index < count; index++) {
        if (unlikely(!cat_thannel_append(thannel, &items[index]))) {
            cat_thannel_rollback(thannel, index);
            cat_mutex_unlock(&thannel->mutex);
            return cat_false;
        }
    }

    if (cat_cond_signal(&thannel->readable)) {
        thannel->wake_count++;
    }

    if (!cat_thannel_is_rendezvous(thannel)) {
        cat_mutex_unlock(&thannel->mutex);
        return cat_true;
    }

    /* A rendezvous push completes only after a pop took the message. The
     * producer keeps the message object, and the pop empties it. */
    message = cat_queue_back_data(&thannel->messages, cat_thannel_message_t, node);
    while (!message->taken && !thannel->closed) {
        thannel->park_count++;
        if (unlikely(!cat_cond_wait(&thannel->writable, &thannel->mutex,
            cat_thannel_remaining(deadline)))) {
            break;
        }
    }

    if (!message->taken) {
        /* Our message is still queued, because a pop needs the mutex to take it. */
        cat_thannel_take_back(thannel, message);
        cat_thannel_recycle(thannel, message);
        cat_mutex_unlock(&thannel->mutex);

        if (thannel->closed) {
            cat_update_last_error(CAT_ECLOSED, "The thannel is closed");
        }
        return cat_false;
    }

    cat_thannel_recycle(thannel, message);
    cat_mutex_unlock(&thannel->mutex);

    return cat_true;
}

CAT_API cat_bool_t cat_thannel_push(cat_thannel_t *thannel, const void *data, size_t size,
                                    cat_timeout_t timeout)
{
    cat_thannel_item_t item;

    if (unlikely(data == NULL && size > 0)) {
        cat_update_last_error(CAT_EINVAL, "The data is null but the size is not zero");
        return cat_false;
    }

    item.data = (void *) data;
    item.size = size;

    return cat_thannel_pushv(thannel, &item, 1, timeout);
}

CAT_API cat_bool_t cat_thannel_popv(cat_thannel_t *thannel, cat_thannel_item_t *items,
                                    size_t capacity, size_t *count, cat_timeout_t timeout)
{
    cat_timeout_t deadline;
    size_t taken;

    if (unlikely(thannel == NULL)) {
        cat_update_last_error(CAT_EINVAL, "The thannel is null");
        return cat_false;
    }
    if (unlikely(items == NULL || count == NULL)) {
        cat_update_last_error(CAT_EINVAL, "The items or the count is null");
        return cat_false;
    }
    if (unlikely(capacity == 0)) {
        cat_update_last_error(CAT_EINVAL, "The capacity is zero");
        return cat_false;
    }

    if (unlikely(!cat_mutex_lock(&thannel->mutex, CAT_TIMEOUT_FOREVER))) {
        return cat_false;
    }

    deadline = cat_thannel_deadline(timeout);

    while (thannel->length == 0) {
        if (thannel->closed) {
            cat_mutex_unlock(&thannel->mutex);
            cat_update_last_error(CAT_ECLOSED, "The thannel is closed and empty");
            return cat_false;
        }

        thannel->park_count++;
        if (unlikely(!cat_cond_wait(&thannel->readable, &thannel->mutex,
            cat_thannel_remaining(deadline)))) {
            cat_mutex_unlock(&thannel->mutex);
            return cat_false;
        }
    }

    taken = cat_thannel_take_messages(thannel, items, capacity);

    /* This pop made room and, for a rendezvous, completed a take. Producers of
     * both kinds wait on the same condition, so wake them all. */
    if (cat_cond_broadcast(&thannel->writable)) {
        thannel->wake_count++;
    }

    cat_mutex_unlock(&thannel->mutex);

    *count = taken;

    return cat_true;
}

CAT_API cat_bool_t cat_thannel_pop(cat_thannel_t *thannel, void **data, size_t *size,
                                   cat_timeout_t timeout)
{
    cat_thannel_item_t item = { 0 };
    size_t count = 0;

    if (unlikely(data == NULL || size == NULL)) {
        cat_update_last_error(CAT_EINVAL, "The data or the size is null");
        return cat_false;
    }

    if (unlikely(!cat_thannel_popv(thannel, &item, 1, &count, timeout))) {
        return cat_false;
    }

    *data = item.data;
    *size = item.size;

    return cat_true;
}

CAT_API size_t cat_thannel_drain(cat_thannel_t *thannel, cat_thannel_item_t *items,
                                 size_t capacity)
{
    size_t taken;

    if (unlikely(thannel == NULL || items == NULL || capacity == 0)) {
        cat_update_last_error(CAT_EINVAL, "The thannel, the items, or the capacity is invalid");
        return 0;
    }

    if (unlikely(!cat_mutex_lock(&thannel->mutex, CAT_TIMEOUT_FOREVER))) {
        return 0;
    }

    taken = cat_thannel_take_messages(thannel, items, capacity);

    if (taken > 0 && cat_cond_broadcast(&thannel->writable)) {
        thannel->wake_count++;
    }

    cat_mutex_unlock(&thannel->mutex);

    return taken;
}

CAT_API cat_bool_t cat_thannel_close(cat_thannel_t *thannel)
{
    if (unlikely(thannel == NULL)) {
        cat_update_last_error(CAT_EINVAL, "The thannel is null");
        return cat_false;
    }
    if (unlikely(!cat_mutex_lock(&thannel->mutex, CAT_TIMEOUT_FOREVER))) {
        return cat_false;
    }

    thannel->closed = cat_true;

    cat_mutex_unlock(&thannel->mutex);

    /* Wake every waiter, on both sides, outside the lock. */
    (void) cat_cond_broadcast(&thannel->readable);
    (void) cat_cond_broadcast(&thannel->writable);

    return cat_true;
}

CAT_API void cat_thannel_cleanup(cat_thannel_t *thannel)
{
    cat_thannel_message_t *message;

    if (thannel == NULL) {
        return;
    }

    while ((message = cat_thannel_dequeue(thannel)) != NULL) {
        cat_thannel_destroy_message(message);
    }

    while (!cat_queue_empty(&thannel->free_messages)) {
        message = cat_queue_front_data(&thannel->free_messages, cat_thannel_message_t, node);
        cat_queue_remove(&message->node);
        cat_thannel_destroy_message(message);
    }

    cat_cond_destroy(&thannel->writable);
    cat_cond_destroy(&thannel->readable);
    (void) cat_mutex_destroy(&thannel->mutex);
}

CAT_API cat_thannel_size_t cat_thannel_get_capacity(const cat_thannel_t *thannel)
{
    return thannel == NULL ? 0 : thannel->capacity;
}

CAT_API cat_thannel_size_t cat_thannel_get_length(const cat_thannel_t *thannel)
{
    cat_thannel_t *self = (cat_thannel_t *) thannel;
    cat_thannel_size_t length;

    if (thannel == NULL) {
        return 0;
    }
    if (unlikely(!cat_mutex_lock(&self->mutex, CAT_TIMEOUT_FOREVER))) {
        return 0;
    }

    length = self->length;

    cat_mutex_unlock(&self->mutex);

    return length;
}

CAT_API cat_bool_t cat_thannel_is_empty(const cat_thannel_t *thannel)
{
    return cat_thannel_get_length(thannel) == 0 ? cat_true : cat_false;
}

CAT_API cat_bool_t cat_thannel_is_full(const cat_thannel_t *thannel)
{
    cat_thannel_t *self = (cat_thannel_t *) thannel;
    cat_bool_t full;

    if (thannel == NULL) {
        return cat_false;
    }
    if (unlikely(!cat_mutex_lock(&self->mutex, CAT_TIMEOUT_FOREVER))) {
        return cat_false;
    }

    full = self->length >= cat_thannel_limit(self) ? cat_true : cat_false;

    cat_mutex_unlock(&self->mutex);

    return full;
}

CAT_API cat_bool_t cat_thannel_is_closed(const cat_thannel_t *thannel)
{
    cat_thannel_t *self = (cat_thannel_t *) thannel;
    cat_bool_t closed;

    if (thannel == NULL) {
        return cat_false;
    }
    if (unlikely(!cat_mutex_lock(&self->mutex, CAT_TIMEOUT_FOREVER))) {
        return cat_true;
    }

    closed = self->closed;

    cat_mutex_unlock(&self->mutex);

    return closed;
}

CAT_API uint64_t cat_thannel_get_wake_count(const cat_thannel_t *thannel)
{
    cat_thannel_t *self = (cat_thannel_t *) thannel;
    uint64_t count;

    if (thannel == NULL) {
        return 0;
    }
    if (unlikely(!cat_mutex_lock(&self->mutex, CAT_TIMEOUT_FOREVER))) {
        return 0;
    }

    count = self->wake_count;

    cat_mutex_unlock(&self->mutex);

    return count;
}

CAT_API uint64_t cat_thannel_get_park_count(const cat_thannel_t *thannel)
{
    cat_thannel_t *self = (cat_thannel_t *) thannel;
    uint64_t count;

    if (thannel == NULL) {
        return 0;
    }
    if (unlikely(!cat_mutex_lock(&self->mutex, CAT_TIMEOUT_FOREVER))) {
        return 0;
    }

    count = self->park_count;

    cat_mutex_unlock(&self->mutex);

    return count;
}

CAT_API cat_bool_t cat_thannel_module_init(void)
{
    return cat_true;
}

CAT_API cat_bool_t cat_thannel_module_shutdown(void)
{
    return cat_true;
}

#endif /* CAT_THREAD_SAFE */
