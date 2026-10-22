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

CAT_API cat_bool_t cat_thannel_push(cat_thannel_t *thannel, const void *data, size_t size,
                                    cat_timeout_t timeout)
{
    cat_thannel_message_t *message;
    cat_timeout_t deadline;

    if (unlikely(thannel == NULL)) {
        cat_update_last_error(CAT_EINVAL, "The thannel is null");
        return cat_false;
    }
    if (unlikely(data == NULL && size > 0)) {
        cat_update_last_error(CAT_EINVAL, "The data is null but the size is not zero");
        return cat_false;
    }

    message = (cat_thannel_message_t *) cat_sys_calloc(1, sizeof(*message));
    if (unlikely(message == NULL)) {
        cat_update_last_error(CAT_ENOMEM, "Allocate the message failed");
        return cat_false;
    }
    if (size > 0) {
        message->data = cat_sys_malloc(size);
        if (unlikely(message->data == NULL)) {
            cat_sys_free(message);
            cat_update_last_error(CAT_ENOMEM, "Allocate the message data failed");
            return cat_false;
        }
        memcpy(message->data, data, size);
        message->size = size;
    }

    if (unlikely(!cat_mutex_lock(&thannel->mutex, CAT_TIMEOUT_FOREVER))) {
        cat_thannel_destroy_message(message);
        return cat_false;
    }

    deadline = cat_thannel_deadline(timeout);

    while (!thannel->closed && thannel->length >= cat_thannel_limit(thannel)) {
        if (unlikely(!cat_cond_wait(&thannel->writable, &thannel->mutex,
            cat_thannel_remaining(deadline)))) {
            cat_mutex_unlock(&thannel->mutex);
            cat_thannel_destroy_message(message);
            return cat_false;
        }
    }

    if (unlikely(thannel->closed)) {
        cat_mutex_unlock(&thannel->mutex);
        cat_thannel_destroy_message(message);
        cat_update_last_error(CAT_ECLOSED, "The thannel is closed");
        return cat_false;
    }

    cat_thannel_enqueue(thannel, message);
    (void) cat_cond_signal(&thannel->readable);

    if (!cat_thannel_is_rendezvous(thannel)) {
        /* The queue owns the message from here, and a pop releases it. */
        cat_mutex_unlock(&thannel->mutex);
        return cat_true;
    }

    /* A rendezvous push completes only after a pop took the message. The
     * producer keeps the message object, and the pop empties it. */
    while (!message->taken && !thannel->closed) {
        if (unlikely(!cat_cond_wait(&thannel->writable, &thannel->mutex,
            cat_thannel_remaining(deadline)))) {
            break;
        }
    }

    if (!message->taken) {
        /* Our message is still queued, because a pop needs the mutex to take it. */
        cat_thannel_take_back(thannel, message);
        cat_mutex_unlock(&thannel->mutex);
        cat_thannel_destroy_message(message);

        if (thannel->closed) {
            cat_update_last_error(CAT_ECLOSED, "The thannel is closed");
        }
        return cat_false;
    }

    cat_mutex_unlock(&thannel->mutex);
    cat_thannel_destroy_message(message);

    return cat_true;
}

CAT_API cat_bool_t cat_thannel_pop(cat_thannel_t *thannel, void **data, size_t *size,
                                   cat_timeout_t timeout)
{
    cat_thannel_message_t *message;
    cat_timeout_t deadline;
    void *value;
    size_t value_size;

    if (unlikely(thannel == NULL)) {
        cat_update_last_error(CAT_EINVAL, "The thannel is null");
        return cat_false;
    }
    if (unlikely(data == NULL || size == NULL)) {
        cat_update_last_error(CAT_EINVAL, "The data or the size is null");
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

        if (unlikely(!cat_cond_wait(&thannel->readable, &thannel->mutex,
            cat_thannel_remaining(deadline)))) {
            cat_mutex_unlock(&thannel->mutex);
            return cat_false;
        }
    }

    message = cat_thannel_dequeue(thannel);
    value = message->data;
    value_size = message->size;

    /* Detach the value before the wakeup, because a rendezvous producer frees
     * the message as soon as it sees the take. */
    message->data = NULL;
    message->size = 0;
    message->taken = cat_true;

    /* This pop made room and, for a rendezvous, completed a take. Producers of
     * both kinds wait on the same condition, so wake them all. */
    (void) cat_cond_broadcast(&thannel->writable);

    cat_mutex_unlock(&thannel->mutex);

    if (!cat_thannel_is_rendezvous(thannel)) {
        cat_sys_free(message);
    }

    *data = value;
    *size = value_size;

    return cat_true;
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

CAT_API cat_bool_t cat_thannel_module_init(void)
{
    return cat_true;
}

CAT_API cat_bool_t cat_thannel_module_shutdown(void)
{
    return cat_true;
}

#endif /* CAT_THREAD_SAFE */
