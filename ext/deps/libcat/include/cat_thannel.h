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

#ifndef CAT_THANNEL_H_
#define CAT_THANNEL_H_

#include "cat.h"
#include "cat_thread_sync.h"

#include "cat_queue.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CAT_THREAD_SAFE

/* The number of messages a thannel can hold. */
typedef uint32_t cat_thannel_size_t;

/*
 * A queue that passes values between threads.
 *
 * The value is copied into the queue, and a copy comes out. This is the model a
 * thread can carry, because the memory of one thread does not belong to another.
 * The copy of a message that pop returns belongs to the caller and is freeable
 * with cat_sys_free from any thread.
 *
 * A capacity of CAT_THANNEL_CAPACITY_UNBUFFERED makes the queue a rendezvous:
 * push returns only after a pop took the message. A larger capacity counts
 * messages, and push waits while the queue is full.
 *
 * select over several thannels is not provided yet. The wait queues stay fields
 * of this struct, so a select engine can wait on them later. The flags field is
 * reserved for the full modes of a bounded queue and is always 0.
 */

#define CAT_THANNEL_CAPACITY_UNBUFFERED 0

typedef struct cat_thannel_message_s {
    cat_queue_node_t node;
    size_t size;
    /* the copy of the value, allocated with cat_sys_malloc */
    void *data;
    /* cat_true after a pop took this message, which a rendezvous push waits for */
    cat_bool_t taken;
} cat_thannel_message_t;

/* Reserved for the full modes of a bounded queue. No flag is defined yet. */
typedef uint32_t cat_thannel_flags_t;

typedef struct cat_thannel_s {
    cat_mutex_t mutex;
    /* a consumer waits here while the queue is empty */
    cat_cond_t readable;
    /* a producer waits here while the queue is full, or for a rendezvous take */
    cat_cond_t writable;
    cat_queue_t messages;
    /* messages that a pop released, so the next push reuses them */
    cat_queue_t free_messages;
    cat_thannel_size_t capacity;
    cat_thannel_size_t length;
    /* reserved, always 0 */
    cat_thannel_flags_t flags;
    cat_bool_t closed;
    /* how often a wake reached a waiter, and how often a side parked */
    uint64_t wake_count;
    uint64_t park_count;
} cat_thannel_t;

/* One value of a batch. */
typedef struct cat_thannel_item_s {
    /* push reads the value, and pop writes a copy that the caller owns */
    void *data;
    size_t size;
} cat_thannel_item_t;

/*
 * Prepare a queue with the given capacity, 0 for a rendezvous.
 *
 * Returns the queue, or NULL with the last error set. The caller owns the
 * storage, and cat_thannel_cleanup releases what the queue allocated.
 */
CAT_API cat_thannel_t *cat_thannel_create(cat_thannel_t *thannel, cat_thannel_size_t capacity);

/*
 * Copy size bytes into the queue.
 *
 * The caller keeps its buffer. A rendezvous queue returns only after a pop took
 * the message. cat_false: CAT_ETIMEDOUT on a timeout, CAT_ECLOSED when the queue
 * was closed, CAT_EINVAL on a null data with a size above 0, and CAT_EMISUSE
 * without a runtime or a coroutine to suspend.
 */
CAT_API cat_bool_t cat_thannel_push(cat_thannel_t *thannel, const void *data, size_t size,
                                    cat_timeout_t timeout);

/*
 * Take one message out of the queue.
 *
 * The caller owns the copy that pop writes to *data, and frees it with
 * cat_sys_free. A message of size 0 gives a null pointer and a size of 0.
 * cat_false: CAT_ETIMEDOUT on a timeout, CAT_ECLOSED when the queue is closed
 * and empty, CAT_EINVAL on a null output pointer.
 */
CAT_API cat_bool_t cat_thannel_pop(cat_thannel_t *thannel, void **data, size_t *size,
                                   cat_timeout_t timeout);

/*
 * Copy a batch of values into the queue, or none of them.
 *
 * The whole batch enters under one lock, and a consumer wakes once. The batch
 * must fit the queue, and a rendezvous queue takes one value at a time.
 *
 * cat_false: CAT_EINVAL on a null item or a batch that is too large,
 * CAT_ETIMEDOUT on a timeout, CAT_ECLOSED when the queue is closed.
 */
CAT_API cat_bool_t cat_thannel_pushv(cat_thannel_t *thannel, const cat_thannel_item_t *items,
                                     size_t count, cat_timeout_t timeout);

/*
 * Take between one and capacity values out of the queue.
 *
 * The caller owns the copies that the *count items point to, and frees them with
 * cat_sys_free.
 *
 * cat_false: CAT_EINVAL on a null pointer or a capacity of 0, CAT_ETIMEDOUT on
 * a timeout, CAT_ECLOSED when the queue is closed and empty.
 */
CAT_API cat_bool_t cat_thannel_popv(cat_thannel_t *thannel, cat_thannel_item_t *items,
                                    size_t capacity, size_t *count, cat_timeout_t timeout);

/*
 * Take what the queue holds, up to capacity, and return how many values came
 * out. The call does not wait, and it reports no close.
 */
CAT_API size_t cat_thannel_drain(cat_thannel_t *thannel, cat_thannel_item_t *items,
                                 size_t capacity);

/*
 * Refuse further pushes and wake every waiter. A pop still returns the messages
 * that remain, and reports CAT_ECLOSED after that.
 */
CAT_API cat_bool_t cat_thannel_close(cat_thannel_t *thannel);

/*
 * Release the messages that remain and the objects of the queue.
 *
 * Do not call it while another thread or coroutine uses the queue.
 */
CAT_API void cat_thannel_cleanup(cat_thannel_t *thannel);

CAT_API cat_thannel_size_t cat_thannel_get_capacity(const cat_thannel_t *thannel);
CAT_API cat_thannel_size_t cat_thannel_get_length(const cat_thannel_t *thannel);
CAT_API cat_bool_t cat_thannel_is_empty(const cat_thannel_t *thannel);
CAT_API cat_bool_t cat_thannel_is_full(const cat_thannel_t *thannel);
CAT_API cat_bool_t cat_thannel_is_closed(const cat_thannel_t *thannel);

/* How often a wake reached a waiter, and how often a side parked. A batch that is
 * too small shows as many parks for few values. */
CAT_API uint64_t cat_thannel_get_wake_count(const cat_thannel_t *thannel);
CAT_API uint64_t cat_thannel_get_park_count(const cat_thannel_t *thannel);

CAT_API cat_bool_t cat_thannel_module_init(void);
CAT_API cat_bool_t cat_thannel_module_shutdown(void);

#endif /* CAT_THREAD_SAFE */

#ifdef __cplusplus
}
#endif

#endif /* CAT_THANNEL_H_ */
