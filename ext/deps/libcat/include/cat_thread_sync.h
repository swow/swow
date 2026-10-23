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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#ifndef CAT_THREAD_SYNC_H_
#define CAT_THREAD_SYNC_H_

#include "cat.h"
#include "cat_atomic.h"
#include "cat_coroutine.h"
#include "cat_queue.h"
#include "cat_time.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CAT_THREAD_SAFE

/*
 * Cross-thread primitives that a coroutine can wait on.
 *
 * The lock and the count come from libuv, so their behavior stays libuv's. The
 * wait is what libcat adds. A coroutine that cannot take the lock gives the
 * thread back to the event loop, and continues when the release arrives.
 */

/*
 * One node for each waiting coroutine. The node is on the stack of the coroutine
 * that waits, and the object keeps the queue of nodes.
 */
typedef struct cat_thread_waiter_s {
    cat_coroutine_t *coroutine;
    /* cat_true after a waker removes this node. The waiter uses it to tell a
     * wakeup from a timeout */
    cat_atomic_bool_t woken;
    cat_queue_node_t node;
} cat_thread_waiter_t;

/*
 * The wait side of an object. It holds the queue of waiters and the lock that
 * guards the queue.
 *
 * A waker on another thread removes nodes under the lock. The queue and the
 * count therefore outlive each waiter.
 */
typedef struct cat_thread_wait_s {
    uv_mutex_t lock;
    cat_queue_t queue;
    cat_atomic_int64_t count;
} cat_thread_wait_t;

CAT_API cat_bool_t cat_thread_wait_init(cat_thread_wait_t *wait);

/*
 * Do not destroy a queue that still has waiters. Their nodes are on their
 * stacks, and no progress is safe after that. The call leaves the queue as it
 * is in this case.
 */
CAT_API void cat_thread_wait_destroy(cat_thread_wait_t *wait);

/*
 * Suspend the current coroutine on the queue until a waker takes it or the
 * timeout expires.
 *
 * cat_true: a waker took it. cat_false: the timeout expired (CAT_ETIMEDOUT), or
 * the wait failed (CAT_EMISUSE without a runtime or without a coroutine).
 */
CAT_API cat_bool_t cat_thread_wait_park(cat_thread_wait_t *wait, cat_timeout_t timeout);

/* Wake one waiter. Returns cat_true when it woke one. Callable from any thread. */
CAT_API cat_bool_t cat_thread_wait_wake(cat_thread_wait_t *wait);

/* Wake all waiters. Returns cat_true when it woke at least one. */
CAT_API cat_bool_t cat_thread_wait_wake_all(cat_thread_wait_t *wait);

/* cat_true while at least one coroutine waits on the object. */
CAT_API cat_bool_t cat_thread_wait_is_parked(const cat_thread_wait_t *wait);

/*
 * Suspend until ready() returns cat_true or a waker arrives.
 *
 * ready() runs under the queue lock. A state change therefore cannot happen
 * between the check and the suspend.
 */
CAT_API cat_bool_t cat_thread_wait_park_until(cat_thread_wait_t *wait, cat_timeout_t timeout,
                                              cat_bool_t (*ready)(const void *data), const void *data); CAT_INTERNAL

/*
 * How a primitive waits when its fast path fails.
 *
 * CAT_SYNC_WAIT_PARK suspends the coroutine and gives the thread to the event
 * loop. CAT_SYNC_WAIT_BLOCK blocks the calling thread in the kernel, so it needs
 * no runtime, and it does not touch the scheduler. Use BLOCK only on a thread
 * that runs one coroutine, because the blocked thread runs no other coroutine.
 */
typedef enum cat_sync_wait_e {
    CAT_SYNC_WAIT_PARK = 0,
    CAT_SYNC_WAIT_BLOCK = 1,
} cat_sync_wait_t;

typedef struct cat_mutex_s {
    uv_mutex_t lock;
    cat_thread_wait_t wait;
    /*
     * cat_true while the lock is held. On glibc, uv uses a normal pthread mutex,
     * and that mutex accepts a release from a thread that does not hold it. The
     * state word fails later, at destroy. Track the holder here to report the
     * mistake at the release.
     */
    cat_atomic_bool_t held;
    /*
     * The thread that holds the lock. A condition variable has to know whether
     * the caller is the holder, and the lock itself can not answer that on
     * Windows, where it is a critical section that the owning thread re-enters.
     */
    cat_atomic_uint64_t owner;
    /* a BLOCK waiter sleeps here, and the release signals it */
    uv_cond_t wakeup;
    cat_sync_wait_t wait_mode;
} cat_mutex_t;

typedef struct cat_sem_s {
    uv_sem_t sem;
    cat_thread_wait_t wait;
    /* libuv has no getter for the count, so the library keeps a copy of it */
    cat_atomic_int64_t value;
} cat_sem_t;

CAT_API cat_bool_t cat_mutex_init(cat_mutex_t *mutex);
/* The wait mode stays fixed for the whole life of the mutex.
 * cat_false when the mode is not one of the two values (CAT_EINVAL). */
CAT_API cat_bool_t cat_mutex_init_ex(cat_mutex_t *mutex, cat_sync_wait_t wait);

/*
 * cat_false when a coroutine still holds the lock (CAT_EMISUSE). The mutex stays
 * usable in that case.
 *
 * Do not destroy a mutex that another coroutine can take. That case has no
 * defined behavior, as with pthread_mutex_destroy.
 */
CAT_API cat_bool_t cat_mutex_destroy(cat_mutex_t *mutex);

/*
 * Take the lock, or wait for the holder to release it.
 *
 * The mutex is not recursive. A second lock from the same thread waits like any
 * other lock, even when it comes from another coroutine of that thread.
 *
 * A BLOCK mutex blocks the calling thread instead of parking a coroutine, so the
 * call needs no runtime.
 *
 * cat_false: nothing was taken. The last error gives the reason.
 * CAT_ETIMEDOUT on a timeout, CAT_EBUSY when the timeout is 0, and CAT_EMISUSE
 * without a runtime or without a coroutine to suspend.
 */
CAT_API cat_bool_t cat_mutex_lock(cat_mutex_t *mutex, cat_timeout_t timeout);
/* cat_false when the caller does not hold the lock (CAT_EMISUSE). */
CAT_API cat_bool_t cat_mutex_unlock(cat_mutex_t *mutex);

CAT_API cat_bool_t cat_sem_init(cat_sem_t *sem, unsigned int value);

/*
 * Do not destroy a semaphore while a coroutine waits on it. That case has no
 * defined behavior, as with sem_destroy. The call does not report it, and it
 * does not crash on account of it.
 */
CAT_API void cat_sem_destroy(cat_sem_t *sem);

/* cat_false on a timeout (CAT_ETIMEDOUT) or on the reasons above. */
CAT_API cat_bool_t cat_sem_acquire(cat_sem_t *sem, cat_timeout_t timeout);
CAT_API void       cat_sem_post(cat_sem_t *sem);

/* Add count counts and wake up to count waiters. A count of 0 posts nothing. */
CAT_API void cat_sem_release(cat_sem_t *sem, size_t count);

/*
 * Take a count without a wait. cat_false with CAT_EBUSY when no count is left.
 */
CAT_API cat_bool_t cat_sem_try_acquire(cat_sem_t *sem);

/*
 * The count that a post adds and an acquire takes. The copy is one step behind
 * while a post and a take run at the same time, so do not treat it as a lock.
 */
CAT_API int64_t cat_sem_get_value(const cat_sem_t *sem);

/* The number of coroutines that wait on the semaphore. */
CAT_API size_t cat_sem_get_waiter_count(const cat_sem_t *sem);

/*
 * A condition variable reports that a predicate became true. The caller owns the
 * predicate and reads it under the mutex. A wait takes the mutex and returns
 * with it held.
 *
 * A signal with no waiter is lost. Keep the predicate and the wait under the
 * same mutex for that reason.
 */
typedef struct cat_cond_s {
    cat_thread_wait_t wait;
    /* a waiter of a BLOCK mutex sleeps here */
    uv_cond_t wakeup;
} cat_cond_t;

CAT_API cat_bool_t cat_cond_init(cat_cond_t *cond);

/*
 * Do not destroy a condition variable while a coroutine waits on it. That case
 * has no defined behavior, as with pthread_cond_destroy. The call does not
 * report it, and it does not crash on account of it.
 */
CAT_API void cat_cond_destroy(cat_cond_t *cond);

/*
 * Release the mutex, wait for a signal, and take the mutex again.
 *
 * The caller must hold the mutex. The release and the queue insert are one
 * step, so a signal that arrives in between is not lost.
 *
 * The wait takes the mode of the mutex. A BLOCK mutex blocks the calling thread
 * and a PARK mutex parks the coroutine.
 *
 * cat_false: the timeout expired (CAT_ETIMEDOUT, the mutex is held again), or
 * the caller does not hold the mutex (CAT_EMISUSE).
 *
 * A wait with no timeout and no signal is a deadlock. The scheduler reports
 * "Deadlock: all coroutines are asleep", and the thread cannot shut down until
 * a deadlock callback breaks the wait. See cat_coroutine_set_deadlock_callback.
 */
CAT_API cat_bool_t cat_cond_wait(cat_cond_t *cond, cat_mutex_t *mutex, cat_timeout_t timeout);

/*
 * Wake one waiter, whether it is a parked coroutine or a blocked thread.
 *
 * cat_true when it woke a coroutine. A blocked thread wakes as well, but no
 * platform reports that case.
 */
CAT_API cat_bool_t cat_cond_signal(cat_cond_t *cond);

/* Wake all waiters. cat_true when it woke at least one coroutine. */
CAT_API cat_bool_t cat_cond_broadcast(cat_cond_t *cond);

CAT_API cat_bool_t cat_thread_sync_module_init(void);
CAT_API cat_bool_t cat_thread_sync_module_shutdown(void);

#endif /* CAT_THREAD_SAFE */

#ifdef __cplusplus
}
#endif

#endif /* CAT_THREAD_SYNC_H_ */
