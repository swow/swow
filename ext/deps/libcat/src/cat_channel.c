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

#include "cat_channel.h"
#include "cat_coroutine.h"
#include "cat_time.h"

#define CAT_CHANNEL_CHECK_STATE_EX(channel, update_last_error, failure) \
    if (unlikely(!cat_channel__is_available(channel))) { \
        if (update_last_error) { \
            cat_channel__update_last_error(channel); \
        } \
        failure; \
    }

#define CAT_CHANNEL_CHECK_STATE(channel, failure) \
        CAT_CHANNEL_CHECK_STATE_EX(channel, cat_true, failure)

#define CAT_CHANNEL_CHECK_STATE_WITHOUT_ERROR(channel, failure) \
        CAT_CHANNEL_CHECK_STATE_EX(channel, cat_false, failure)

/* for select()
 * head must be consistent with coroutine */
typedef struct {
    union { cat_queue_node_t node; } waiter;
    cat_coroutine_id_t id;
    cat_coroutine_t *coroutine;
} cat_channel_dummy_coroutine_t;

CAT_STATIC_ASSERT(cat_offsize_of(cat_channel_dummy_coroutine_t, id) == cat_offsize_of(cat_coroutine_t, id));
CAT_STATIC_ASSERT(cat_offsize_of(cat_channel_dummy_coroutine_t, waiter) == cat_offsize_of(cat_coroutine_t, waiter));

static cat_always_inline cat_bool_t cat_channel__is_available(const cat_channel_t *channel)
{
    return !(channel->flags & (CAT_CHANNEL_FLAG_CLOSING | CAT_CHANNEL_FLAG_CLOSED));
}

static cat_never_inline void cat_channel__update_last_error(const cat_channel_t *channel)
{
    CAT_ASSERT((channel->flags & (CAT_CHANNEL_FLAG_CLOSING | CAT_CHANNEL_FLAG_CLOSED)) && "Unexpected state");
    if (channel->flags & CAT_CHANNEL_FLAG_CLOSING) {
        cat_update_last_error(CAT_ECLOSING, "Channel is closing");
    } else if (channel->flags & CAT_CHANNEL_FLAG_CLOSED) {
        cat_update_last_error(CAT_ECLOSED, "Channel has been closed");
    }
}

static cat_always_inline cat_bool_t cat_channel__is_unbuffered(const cat_channel_t *channel)
{
    return channel->capacity == 0;
}

static cat_always_inline cat_bool_t cat_channel__has_producers(const cat_channel_t *channel)
{
    return !cat_queue_empty(&channel->producers);
}

static cat_always_inline cat_bool_t cat_channel__has_consumers(const cat_channel_t *channel)
{
    return !cat_queue_empty(&channel->consumers);
}

static cat_always_inline cat_bool_t cat_channel__is_empty(const cat_channel_t *channel)
{
    return channel->length == 0;
}

static cat_always_inline cat_bool_t cat_channel__is_full(const cat_channel_t *channel)
{
    return channel->length == channel->capacity;
}

static cat_always_inline cat_bool_t cat_channel__is_readable(const cat_channel_t *channel)
{
    return likely(cat_channel__is_available(channel)) && (
           /* buffered */
           !cat_channel__is_empty(channel) ||
           /* unbuffered */
           cat_channel__has_producers(channel)
    );
}

static cat_always_inline cat_bool_t cat_channel__is_writable(const cat_channel_t *channel)
{
    return likely(cat_channel__is_available(channel)) && (
           /* buffered */
           !cat_channel__is_full(channel) ||
           /* unbuffered */
           cat_channel__has_consumers(channel)
    );
}

static cat_always_inline cat_bool_t cat_channel_waiter_is_dummy(const cat_coroutine_t *coroutine)
{
    return coroutine->id == CAT_COROUTINE_MAX_ID;
}

static cat_always_inline void cat_channel_resume_waiter(cat_coroutine_t *coroutine, const char *name)
{
    if (unlikely(cat_channel_waiter_is_dummy(coroutine))) {
        cat_channel_dummy_coroutine_t *dummy_coroutine = (cat_channel_dummy_coroutine_t *) coroutine;
        coroutine = dummy_coroutine->coroutine;
        dummy_coroutine->coroutine = NULL;
        CAT_ASSERT(coroutine != NULL);
    }

    cat_coroutine_schedule(coroutine, CHANNEL, "%s", name);
}

static cat_always_inline cat_bool_t cat_channel_wait(cat_queue_t *queue, cat_timeout_t timeout)
{
    cat_queue_node_t *waiter = &CAT_COROUTINE_G(current)->waiter.node;
    cat_bool_t ret;

    cat_queue_push_back(queue, waiter);
    ret = cat_time_wait(timeout);
    cat_queue_remove(waiter);

    return ret;
}

static cat_always_inline cat_bool_t cat_channel_unbuffered_is_pushable(const cat_channel_t *channel)
{
    return channel->u.unbuffered.able.push;
}

static cat_always_inline cat_bool_t cat_channel_unbuffered_is_popable(const cat_channel_t *channel)
{
    return channel->u.unbuffered.able.pop;
}

static cat_always_inline void cat_channel_unbuffered_push_data(cat_channel_t *channel, const cat_data_t *in)
{
    cat_data_t *out = channel->u.unbuffered.data.out;
    CAT_ASSERT(cat_channel_unbuffered_is_pushable(channel));
    CAT_ASSERT(in != NULL);
    /* copy data to the pop side and make it NULL (let it know that we are done) */
    if (out != NULL) {
        memcpy(out, in, channel->data_size);
    } else if (channel->dtor != NULL) {
        channel->dtor(in);
    }
    channel->u.unbuffered.data.out = NULL;
    channel->u.unbuffered.able.push = cat_false;
}

static cat_always_inline void cat_channel_unbuffered_pop_data(cat_channel_t *channel, cat_data_t *out)
{
    const cat_data_t *in = channel->u.unbuffered.data.in;
    CAT_ASSERT(cat_channel_unbuffered_is_popable(channel));
    CAT_ASSERT(in != NULL);
    /* copy data to the pop side and make it NULL (let it know that we are done) */
    if (out != NULL) {
        memcpy(out, in, channel->data_size);
    } else if (channel->dtor != NULL) {
        channel->dtor(in);
    }
    channel->u.unbuffered.data.in = NULL;
    channel->u.unbuffered.able.pop = cat_false;
}

static cat_always_inline void cat_channel_unbuffered_notify_consumer(cat_channel_t *channel, const cat_data_t *data)
{
    cat_coroutine_t *consumer = cat_queue_front_data(&channel->consumers, cat_coroutine_t, waiter.node);

    CAT_ASSERT(consumer != NULL);
    CAT_ASSERT(!cat_channel_unbuffered_is_pushable(channel));
    CAT_ASSERT(!cat_channel_unbuffered_is_popable(channel));

    channel->u.unbuffered.data.in = data;
    channel->u.unbuffered.able.pop = cat_true;

    cat_channel_resume_waiter(consumer, "Consumer");
}

static cat_always_inline void cat_channel_unbuffered_notify_producer(cat_channel_t *channel, cat_data_t *data)
{
    cat_coroutine_t *producer = cat_queue_front_data(&channel->producers, cat_coroutine_t, waiter.node);

    CAT_ASSERT(producer != NULL);
    CAT_ASSERT(!cat_channel_unbuffered_is_pushable(channel));
    CAT_ASSERT(!cat_channel_unbuffered_is_popable(channel));

    channel->u.unbuffered.data.out = data;
    channel->u.unbuffered.able.push = cat_true;

    cat_channel_resume_waiter(producer, "Producer");
}

static cat_bool_t cat_channel_unbuffered_push(cat_channel_t *channel, const cat_data_t *data, cat_timeout_t timeout)
{
    /* if it is unwritable, just wait */
    if (!cat_channel__has_consumers(channel)) {
        if (unlikely(!cat_channel_wait(&channel->producers, timeout))) {
            /* sleep failed or timedout */
            cat_update_last_error_with_previous("Channel wait consumer failed");
            return cat_false;
        }
        if (unlikely(!cat_channel_unbuffered_is_pushable(channel))) {
            /* still no consumer, must be canceled */
            cat_update_last_error(CAT_ECANCELED, "Channel push has been canceled");
            return cat_false;
        }
        CAT_ASSERT(!cat_channel__has_consumers(channel));
        /* push data and continue to run */
        cat_channel_unbuffered_push_data(channel, data);
    } else {
        CAT_ASSERT(!cat_channel__has_producers(channel));
        /* notify the consumer to consume the data
         * after it yield and go back to here, data has been consumed */
        cat_channel_unbuffered_notify_consumer(channel, data);
    }

    return cat_true;
}

static cat_bool_t cat_channel_unbuffered_pop(cat_channel_t *channel, cat_data_t *data, cat_timeout_t timeout)
{
    /* if it is unreadable, just wait */
    if (!cat_channel__has_producers(channel)) {
        if (unlikely(!cat_channel_wait(&channel->consumers, timeout))) {
            /* sleep failed or timedout */
            cat_update_last_error_with_previous("Channel wait producer failed");
            return cat_false;
        }
        if (unlikely(!cat_channel_unbuffered_is_popable(channel))) {
            /* still no producer, must be canceled */
            cat_update_last_error(CAT_ECANCELED, "Channel pop has been canceled");
            return cat_false;
        }
        CAT_ASSERT(!cat_channel__has_producers(channel));
        /* pop data and continue to run */
        cat_channel_unbuffered_pop_data(channel, data);
    } else {
        CAT_ASSERT(!cat_channel__has_consumers(channel));
        /* notify the producer to produce the data,
         * when it yield and go back to here, data is ready */
        cat_channel_unbuffered_notify_producer(channel, data);
    }

    return cat_true;
}

static cat_always_inline cat_channel_bucket_t *cat_channel_buffered_bucket_create(const cat_data_t *data, size_t data_size)
{
    cat_channel_bucket_t *bucket;

    bucket = (cat_channel_bucket_t *) cat_malloc(offsetof(cat_channel_bucket_t, data) + data_size);

#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(bucket == NULL)) {
        cat_update_last_error_of_syscall("Malloc for channel bucket failed");
        return NULL;
    }
#endif

    memcpy(bucket->data, data, data_size);

    return bucket;
}

static cat_always_inline cat_bool_t cat_channel_buffered_push_data(cat_channel_t *channel, const cat_data_t *data)
{
    cat_channel_bucket_t *bucket;

    bucket = cat_channel_buffered_bucket_create(data, channel->data_size);

#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(bucket == NULL)) {
        return cat_false;
    }
#endif

    cat_queue_push_back(&channel->u.buffered.storage, &bucket->node);
    channel->length++;

    return cat_true;
}

static cat_always_inline void cat_channel_buffered_pop_data(cat_channel_t *channel, cat_data_t *data)
{
    cat_channel_bucket_t *bucket;

    bucket = cat_queue_front_data(&channel->u.buffered.storage, cat_channel_bucket_t, node);
    cat_queue_remove(&bucket->node);
    if (data != NULL) {
        memcpy(data, bucket->data, channel->data_size);
    } else if (channel->dtor != NULL) {
        channel->dtor(bucket->data);
    }
    cat_free(bucket);
    channel->length--;
}

static cat_always_inline void cat_channel_notify_possible_consumer(cat_channel_t *channel)
{
    cat_coroutine_t *consumer = cat_queue_front_data(&channel->consumers, cat_coroutine_t, waiter.node);

    if (consumer != NULL) {
        /* notify a possible consume to consume */
        cat_channel_resume_waiter(consumer, "Consumer");
    }
}

static cat_always_inline void cat_channel_notify_possible_producer(cat_channel_t *channel)
{
    cat_coroutine_t *producer = cat_queue_front_data(&channel->producers, cat_coroutine_t, waiter.node);

    if (producer != NULL) {
        /* notify a possible producer to produce */
        cat_channel_resume_waiter(producer, "Producer");
    }
}

static cat_bool_t cat_channel_buffered_push(cat_channel_t *channel, const cat_data_t *data, cat_timeout_t timeout)
{
    /* if it is full, just wait */
    if (cat_channel__is_full(channel)) {
        if (unlikely(!cat_channel_wait(&channel->producers, timeout))) {
            /* sleep failed or timedout */
            cat_update_last_error_with_previous("Channel wait consumer failed");
            return cat_false;
        }
        if (unlikely(cat_channel__is_full(channel))) {
            /* still full, must be canceled */
            cat_update_last_error(CAT_ECANCELED, "Channel push has been canceled");
            return cat_false;
        }
        CAT_ASSERT(!cat_channel__has_consumers(channel));
        /* push data to the storage queue and return */
        return cat_channel_buffered_push_data(channel, data);
    } else {
        CAT_ASSERT(!cat_channel__has_producers(channel));
        /* push data to the storage queue */
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(!cat_channel_buffered_push_data(channel, data))) {
            return cat_false;
        }
#else
        (void) cat_channel_buffered_push_data(channel, data);
#endif
        /* try to notify one for balance */
        cat_channel_notify_possible_consumer(channel);
        return cat_true;
    }
}

static cat_bool_t cat_channel_buffered_pop(cat_channel_t *channel, cat_data_t *data, cat_timeout_t timeout)
{
    /* if it is empty, just wait */
    if (cat_channel__is_empty(channel)) {
        if (unlikely(!cat_channel_wait(&channel->consumers, timeout))) {
            /* sleep failed or timedout */
            cat_update_last_error_with_previous("Channel wait producer failed");
            return cat_false;
        }
        if (unlikely(cat_channel__is_empty(channel))) {
            /* still empty, must be canceled */
            cat_update_last_error(CAT_ECANCELED, "Channel pop has been canceled");
            return cat_false;
        }
        CAT_ASSERT(!cat_channel__has_producers(channel));
        /* pop data from the storage queue and return */
        cat_channel_buffered_pop_data(channel, data);
    } else {
        CAT_ASSERT(!cat_channel__has_consumers(channel));
        /* pop data from the storage queue */
        cat_channel_buffered_pop_data(channel, data);
        /* try to notify one for balance */
        cat_channel_notify_possible_producer(channel);
    }

    return cat_true;
}

/* common */

CAT_API cat_channel_t *cat_channel_create(cat_channel_t *channel, cat_channel_size_t capacity, cat_channel_data_size_t data_size, cat_channel_data_dtor_t dtor)
{
    channel->capacity = capacity;
    channel->data_size = data_size;
    channel->length = 0;
    channel->flags = CAT_CHANNEL_FLAG_NONE;
    channel->dtor = dtor;
    cat_queue_init(&channel->producers);
    cat_queue_init(&channel->consumers);
    if (cat_channel__is_unbuffered(channel)) {
        memset(&channel->u.unbuffered, 0, sizeof(channel->u.unbuffered));
    } else {
        cat_queue_init(&channel->u.buffered.storage);
    }

    return channel;
}

CAT_API cat_bool_t cat_channel_push(cat_channel_t *channel, const cat_data_t *data, cat_timeout_t timeout)
{
    CAT_CHANNEL_CHECK_STATE(channel, return cat_false);
    CAT_ASSERT(data != NULL);

    if (cat_channel__is_unbuffered(channel)) {
        return cat_channel_unbuffered_push(channel, data, timeout);
    } else {
        return cat_channel_buffered_push(channel, data, timeout);
    }
}

CAT_API cat_bool_t cat_channel_pop(cat_channel_t *channel, cat_data_t *data, cat_timeout_t timeout)
{
    CAT_CHANNEL_CHECK_STATE(channel, return cat_false);

    if (cat_channel__is_unbuffered(channel)) {
        return cat_channel_unbuffered_pop(channel, data, timeout);
    } else {
        return cat_channel_buffered_pop(channel, data, timeout);
    }
}

CAT_API void cat_channel_close(cat_channel_t *channel)
{
    CAT_CHANNEL_CHECK_STATE_WITHOUT_ERROR(channel, return);

    /* prevent from channel operations during closing */
    channel->flags |= CAT_CHANNEL_FLAG_CLOSING;

    /* notify all waiters */
    cat_coroutine_t *waiter;
    while ((waiter = cat_queue_front_data(&channel->producers, cat_coroutine_t, waiter.node))) {
        cat_channel_resume_waiter(waiter, "Producer");
    }
    while ((waiter = cat_queue_front_data(&channel->consumers, cat_coroutine_t, waiter.node))) {
        cat_channel_resume_waiter(waiter, "Consumer");
    }

    CAT_ASSERT(!cat_channel__has_producers(channel));
    CAT_ASSERT(!cat_channel__has_consumers(channel));

    /* clean up the data bucket (no more consumers) */
    if (!cat_channel__is_unbuffered(channel)) {
        cat_queue_t *storage = &channel->u.buffered.storage;
        cat_channel_data_dtor_t dtor = channel->dtor;
        cat_channel_bucket_t *bucket;
        while ((bucket = cat_queue_front_data(storage, cat_channel_bucket_t, node))) {
            cat_queue_remove(&bucket->node);
            if (dtor != NULL) {
                dtor(bucket->data);
            }
            cat_free(bucket);
            channel->length--;
        }
    }

    /* everything will be reset after close */
    CAT_ASSERT(cat_channel__is_empty(channel));

    /* revert and we can reuse this channel (if necessary...) */
    channel->flags ^= CAT_CHANNEL_FLAG_CLOSING;
    if (!(channel->flags & CAT_CHANNEL_FLAG_REUSE)) {
        channel->flags |= CAT_CHANNEL_FLAG_CLOSED;
    }
}

/* select */

static cat_always_inline void cat_channel_queue_dummy_coroutine(cat_queue_t *queue, cat_channel_dummy_coroutine_t *dummy_coroutine)
{
    dummy_coroutine->id = CAT_COROUTINE_MAX_ID; /* it shows that it's a dummy coroutine */
    dummy_coroutine->coroutine = CAT_COROUTINE_G(current); /* real one */
    cat_queue_push_back(queue, &dummy_coroutine->waiter.node);
}

CAT_API cat_channel_select_response_t *cat_channel_select(cat_channel_select_request_t *requests, size_t count, cat_timeout_t timeout)
{
    cat_channel_dummy_coroutine_t *dummy_coroutines;
    cat_channel_select_request_t *request;
    cat_channel_select_response_t *response;
    cat_channel_t *channel;
    cat_bool_t ret;
    size_t i;

    for (i = 0, request = requests; i < count; i++, request++) {
        channel = request->channel;
        CAT_CHANNEL_CHECK_STATE(
            channel,
            request->error = cat_true;
            return request;
        );
        if (request->opcode == CAT_CHANNEL_OPCODE_PUSH) {
            if (cat_channel__is_writable(channel)) {
                request->error = !cat_channel_push(channel, request->data.in, -1);
                CAT_ASSERT(!request->error || cat_get_last_error_code() == CAT_ENOMEM);
                return request;
            }
        } else /* if (request->opcode == CAT_CHANNEL_OPCODE_POP) */ {
            if (cat_channel__is_readable(channel)) {
                request->error = !cat_channel_pop(channel, request->data.out, -1);
                CAT_ASSERT(!request->error);
                return request;
            }
        }
    }

    /* dummy coroutines */
    dummy_coroutines = (cat_channel_dummy_coroutine_t *) cat_malloc(sizeof(cat_channel_dummy_coroutine_t) * count);

#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(dummy_coroutines == NULL)) {
        cat_update_last_error_of_syscall("Malloc for dummy coroutines failed");
        return NULL;
    }
#endif

    for (i = 0, request = requests; i < count; i++, request++) {
        channel = request->channel;
        if (request->opcode == CAT_CHANNEL_OPCODE_PUSH) {
            cat_channel_queue_dummy_coroutine(&channel->producers, &dummy_coroutines[i]);
        } else /* if (request->opcode == CAT_CHANNEL_OPCODE_POP) */ {
            cat_channel_queue_dummy_coroutine(&channel->consumers, &dummy_coroutines[i]);
        }
    }

    ret = cat_time_wait(timeout);

    response = NULL;
    for (i = 0; i < count; i++) {
        cat_channel_dummy_coroutine_t *dummy_coroutine = &dummy_coroutines[i];
        cat_queue_remove(&dummy_coroutine->waiter.node);
        if (dummy_coroutine->coroutine == NULL) {
            response = &requests[i];
            channel = response->channel;
            CAT_CHANNEL_CHECK_STATE(
                channel,
                cat_set_last_error_code(CAT_ECANCELED);
                response->error = cat_true;
                continue;
            );
            response->error = cat_false;
            if (response->opcode == CAT_CHANNEL_OPCODE_PUSH) {
                if (cat_channel__is_unbuffered(channel)) {
                    cat_channel_unbuffered_push_data(channel, response->data.in);
                } else {
                    response->error =
                    !cat_channel_buffered_push_data(channel, response->data.in);
                }
            } else /* if (request->opcode == CAT_CHANNEL_OPCODE_POP) */ {
                if (cat_channel__is_unbuffered(channel)) {
                    cat_channel_unbuffered_pop_data(channel, response->data.out);
                } else {
                    cat_channel_buffered_pop_data(channel, response->data.out);
                }
            }
        }
    }

    cat_free(dummy_coroutines);

    if (unlikely(!ret)) {
        /* sleep failed or timedout */
        cat_update_last_error_with_previous("Channel select wait failed");
        return NULL;
    }

    if (unlikely(response == NULL)) {
        cat_update_last_error(CAT_ECANCELED, "Channel select has been canceled");
        return NULL;
    }

    return response;
}

/* status */

CAT_API cat_channel_size_t cat_channel_get_capacity(const cat_channel_t *channel)
{
    return channel->capacity;
}

CAT_API cat_channel_size_t cat_channel_get_length(const cat_channel_t *channel)
{
    return channel->length;
}

CAT_API cat_bool_t cat_channel_is_available(const cat_channel_t *channel)
{
    return cat_channel__is_available(channel);
}

CAT_API cat_bool_t cat_channel_has_producers(const cat_channel_t *channel)
{
    return cat_channel__has_producers(channel);
}

CAT_API cat_bool_t cat_channel_has_consumers(const cat_channel_t *channel)
{
    return cat_channel__has_consumers(channel);
}

CAT_API cat_bool_t cat_channel_is_empty(const cat_channel_t *channel)
{
    return cat_channel__is_empty(channel);
}

CAT_API cat_bool_t cat_channel_is_full(const cat_channel_t *channel)
{
    return cat_channel__is_full(channel);
}

CAT_API cat_bool_t cat_channel_is_readable(const cat_channel_t *channel)
{
    return cat_channel__is_readable(channel);
}

CAT_API cat_bool_t cat_channel_is_writable(const cat_channel_t *channel)
{
    return cat_channel__is_writable(channel);
}

/* special */

CAT_API cat_channel_flags_t cat_channel_get_flags(const cat_channel_t *channel)
{
    return channel->flags;
}

CAT_API void cat_channel_enable_reuse(cat_channel_t *channel)
{
    channel->flags |= CAT_CHANNEL_FLAG_REUSE;
}

CAT_API cat_channel_data_dtor_t cat_channel_get_dtor(const cat_channel_t *channel)
{
    return channel->dtor;
}

CAT_API cat_channel_data_dtor_t cat_channel_set_dtor(cat_channel_t *channel, cat_channel_data_dtor_t dtor)
{
    cat_channel_data_dtor_t old_dtor = channel->dtor;

    channel->dtor = dtor;

    return old_dtor;
}

/* ext */

CAT_API cat_queue_t *cat_channel_get_storage(cat_channel_t *channel)
{
    if (unlikely(cat_channel__is_unbuffered(channel))) {
        return NULL;
    }
    return &channel->u.buffered.storage;
}
