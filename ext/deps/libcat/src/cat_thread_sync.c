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
#include "cat_api.h"
#include "cat_thread_sync.h"

#ifdef CAT_THREAD_SAFE

CAT_API cat_bool_t cat_thread_sync_module_init(void)
{
    return cat_true;
}

CAT_API cat_bool_t cat_thread_sync_module_shutdown(void)
{
    return cat_true;
}

CAT_API cat_bool_t cat_thread_wait_init(cat_thread_wait_t *wait)
{
    do {
        int error = uv_mutex_init(&wait->lock);

        if (unlikely(error != 0)) {
            cat_update_last_error_with_reason(error, "Wait init mutex failed");
            return cat_false;
        }
    } while (0);

    cat_queue_init(&wait->queue);
    cat_atomic_int64_store(&wait->count, 0);

    return cat_true;
}

CAT_API void cat_thread_wait_destroy(cat_thread_wait_t *wait)
{
    /*
     * A destroy is only safe while nobody holds the lock. uv_mutex_destroy
     * turns a failure into abort(), so a lock that still has waiters keeps its
     * mutex instead: a leaked mutex is better than a dying process.
     */
    if (cat_atomic_int64_load(&wait->count) != 0) {
        return;
    }

    uv_mutex_destroy(&wait->lock);
}

CAT_API cat_bool_t cat_thread_wait_is_parked(const cat_thread_wait_t *wait)
{
    return cat_atomic_int64_load(&((cat_thread_wait_t *) wait)->count) != 0;
}

/* Take the first waiter out of the queue and mark it woken. The caller must hold
 * the lock. The node stays valid because its owner is still suspended. */
static cat_coroutine_t *cat_thread_wait_take_first(cat_thread_wait_t *wait)
{
    cat_thread_waiter_t *waiter;

    if (cat_queue_empty(&wait->queue)) {
        return NULL;
    }

    waiter = cat_queue_front_data(&wait->queue, cat_thread_waiter_t, node);
    cat_queue_remove(&waiter->node);

    /* Mark before the resume. The resumed waiter reads this mark. */
    cat_atomic_bool_store(&waiter->woken, cat_true);

    return waiter->coroutine;
}

#define CAT_THREAD_WAIT_BATCH 32

static cat_bool_t cat_thread_wait_wake_n(cat_thread_wait_t *wait, cat_bool_t all)
{
    cat_coroutine_t *woken[CAT_THREAD_WAIT_BATCH];
    int count = 0;
    int index;

    uv_mutex_lock(&wait->lock);
    while (count < CAT_THREAD_WAIT_BATCH) {
        cat_coroutine_t *coroutine = cat_thread_wait_take_first(wait);

        if (coroutine == NULL) {
            break;
        }
        woken[count++] = coroutine;
        if (!all) {
            break;
        }
    }
    uv_mutex_unlock(&wait->lock);

    if (count == 0) {
        return cat_false;
    }

    cat_atomic_int64_fetch_sub(&wait->count, count);

    /* Each resume runs the waiter, and the waiter takes this lock again. Resume
     * only after the unlock. */
    for (index = 0; index < count; index++) {
        cat_coroutine_wake(woken[index]);
    }

    return cat_true;
}

CAT_API cat_bool_t cat_thread_wait_wake(cat_thread_wait_t *wait)
{
    return cat_thread_wait_wake_n(wait, cat_false);
}

CAT_API cat_bool_t cat_thread_wait_wake_all(cat_thread_wait_t *wait)
{
    cat_bool_t woken = cat_false;

    while (cat_thread_wait_wake_n(wait, cat_true)) {
        woken = cat_true;
    }

    return woken;
}

struct cat_thread_wait_park_s {
    cat_thread_wait_t *wait;
    cat_thread_waiter_t waiter;
    cat_time_timeout_t *timer;
};

static void cat_thread_wait_timeout(cat_time_timeout_t *timer, cat_data_t *data)
{
    struct cat_thread_wait_park_s *park = (struct cat_thread_wait_park_s *) data;
    cat_coroutine_t *coroutine = NULL;

    (void) timer;

    /* The timer runs on the thread of the waiter, so the queue access here is
     * safe even when the object is gone. */
    uv_mutex_lock(&park->wait->lock);
    if (cat_atomic_bool_load(&park->waiter.woken) != cat_true) {
        /* Nobody took this waiter, so the timeout is what ends its wait. */
        cat_queue_remove(&park->waiter.node);
        coroutine = park->waiter.coroutine;
    }
    uv_mutex_unlock(&park->wait->lock);

    if (coroutine != NULL) {
        cat_atomic_int64_fetch_sub(&park->wait->count, 1);
        cat_coroutine_wake(coroutine);
    }
}

/* Put the waiter in the queue. The caller must hold the queue lock, because some
 * callers do this in the same step as another change. */
static void cat_thread_wait_enqueue(struct cat_thread_wait_park_s *park, cat_coroutine_t *coroutine)
{
    park->waiter.coroutine = coroutine;
    cat_atomic_bool_store(&park->waiter.woken, cat_false);
    cat_queue_push_back(&park->wait->queue, &park->waiter.node);
    cat_atomic_int64_fetch_add(&park->wait->count, 1);
}

/* Suspend until a waker takes the waiter out or the timeout expires. Leave the
 * queue in both cases and report which one happened. */
static cat_bool_t cat_thread_wait_suspend(struct cat_thread_wait_park_s *park, cat_timeout_t timeout)
{
    cat_bool_t woken;
    uv_async_t *handle = CAT_COROUTINE_G(wakeup_handle);

    park->timer = cat_time_timeout_init(cat_thread_wait_timeout, park, timeout);

    /*
     * A parked coroutine may be woken from another thread, and a loop with no
     * other work would not poll for that wakeup. Holding this handle keeps the
     * loop of this thread polling until the coroutine runs again.
     */
    if (handle != NULL && CAT_COROUTINE_G(parked)++ == 0) {
        uv_ref((uv_handle_t *) handle);
    }

    (void) cat_coroutine_yield(NULL, NULL);

    if (handle != NULL && --CAT_COROUTINE_G(parked) == 0) {
        uv_unref((uv_handle_t *) handle);
    }

    cat_time_timeout_close(park->timer);

    woken = cat_atomic_bool_load(&park->waiter.woken);

    /*
     * Every path that takes the node out of the queue also drops the count, so
     * the two stay in step: a waker does it, and the timeout callback does it.
     * Only a failed yield leaves the node queued, and then this does both.
     */
    if (!woken) {
        uv_mutex_lock(&park->wait->lock);
        /* the callback may have taken it between the test and the lock */
        if (!cat_queue_empty(&park->wait->queue)) {
            cat_queue_remove(&park->waiter.node);
            cat_atomic_int64_fetch_sub(&park->wait->count, 1);
        }
        uv_mutex_unlock(&park->wait->lock);
    }

    return woken;
}

CAT_API cat_bool_t cat_thread_wait_park(cat_thread_wait_t *wait, cat_timeout_t timeout)
{
    struct cat_thread_wait_park_s park;
    cat_coroutine_t *current;

    if (unlikely(!cat_globals_available())) {
        cat_update_last_error(CAT_EMISUSE, "This thread has no runtime");
        return cat_false;
    }

    current = cat_coroutine_get_current();
    if (unlikely(current == NULL)) {
        cat_update_last_error(CAT_EMISUSE, "No coroutine to suspend");
        return cat_false;
    }

    park.wait = wait;
    park.timer = NULL;

    uv_mutex_lock(&wait->lock);
    cat_thread_wait_enqueue(&park, current);
    uv_mutex_unlock(&wait->lock);

    if (!cat_thread_wait_suspend(&park, timeout)) {
        cat_update_last_error(CAT_ETIMEDOUT, "Timed out waiting for the object");
        return cat_false;
    }

    return cat_true;
}

/*
 * Wait until ready() returns cat_true.
 *
 * ready() runs under the queue lock, and the primitives below publish their
 * resource under the same lock. The release is therefore never lost between the
 * check and the suspend.
 *
 * Returns 1 when ready() returned cat_true, 0 when the wait ended without it, and
 * -1 on a timeout or a failure, which also sets the last error.
 */
static int cat_thread_wait_wait_for(cat_thread_wait_t *wait, cat_timeout_t timeout,
                                    cat_bool_t (*ready)(const void *data), const void *data)
{
    struct cat_thread_wait_park_s park;
    cat_coroutine_t *current;

    if (unlikely(!cat_globals_available())) {
        cat_update_last_error(CAT_EMISUSE, "This thread has no runtime");
        return -1;
    }

    current = cat_coroutine_get_current();
    if (unlikely(current == NULL)) {
        cat_update_last_error(CAT_EMISUSE, "No coroutine to suspend");
        return -1;
    }

    park.wait = wait;
    park.timer = NULL;

    uv_mutex_lock(&wait->lock);
    if (ready(data)) {
        uv_mutex_unlock(&wait->lock);
        return 1;
    }
    uv_mutex_unlock(&wait->lock);

    /* A thread that never ran cat_run has no scheduler coroutine to switch to.
     * Start it here, so the first wait of that thread works. */
    if (cat_coroutine_get_scheduler() == NULL && !cat_run(CAT_RUN_EASY)) {
        return -1;
    }

    uv_mutex_lock(&wait->lock);
    if (ready(data)) {
        /* it became available while the scheduler started */
        uv_mutex_unlock(&wait->lock);
        return 1;
    }
    cat_thread_wait_enqueue(&park, current);
    uv_mutex_unlock(&wait->lock);

    if (!cat_thread_wait_suspend(&park, timeout)) {
        cat_update_last_error(CAT_ETIMEDOUT, "Timed out waiting for the object");
        return -1;
    }

    return 0;
}

CAT_API cat_bool_t cat_thread_wait_park_until(cat_thread_wait_t *wait, cat_timeout_t timeout,
                                              cat_bool_t (*ready)(const void *data), const void *data)
{
    int result = cat_thread_wait_wait_for(wait, timeout, ready, data);

    /* A wakeup that did not find the resource still ends the wait, and the
     * caller reads its own state again. Only a timeout or a failure is an
     * error. */
    return result < 0 ? cat_false : cat_true;
}

CAT_API cat_bool_t cat_cond_init(cat_cond_t *cond)
{
    return cat_thread_wait_init(&cond->wait);
}

CAT_API void cat_cond_destroy(cat_cond_t *cond)
{
    cat_thread_wait_destroy(&cond->wait);
}

CAT_API cat_bool_t cat_cond_signal(cat_cond_t *cond)
{
    return cat_thread_wait_wake(&cond->wait);
}

CAT_API cat_bool_t cat_cond_broadcast(cat_cond_t *cond)
{
    return cat_thread_wait_wake_all(&cond->wait);
}

/* ready() for the mutex: taking it is the check, so a lock that is free is taken
 * while the queue is locked and cannot be taken by anybody else in between. */
static cat_bool_t cat_mutex_try_take(const void *data)
{
    cat_mutex_t *mutex = (cat_mutex_t *) data;
    cat_bool_t free = cat_false;

    /*
     * The flag decides, not the lock. On Windows the lock is a critical
     * section, and a critical section lets the thread that owns it take it
     * again. Every coroutine of a thread would then take a lock that another
     * coroutine of the same thread holds.
     */
    if (!cat_atomic_bool_compare_exchange_strong(&mutex->held, &free, cat_true)) {
        return cat_false;
    }

    if (uv_mutex_trylock(&mutex->lock) != 0) {
        /* The holder cleared the flag but has not released the lock yet. */
        cat_atomic_bool_store(&mutex->held, cat_false);
        return cat_false;
    }

    cat_atomic_uint64_store(&mutex->owner, (uint64_t) (uintptr_t) uv_thread_self());

    return cat_true;
}

/* ready() for the semaphore, which consumes the count as its check. */
static cat_bool_t cat_sem_try_take(const void *data)
{
    cat_sem_t *sem = (cat_sem_t *) data;

    return uv_sem_trywait(&sem->sem) == 0 ? cat_true : cat_false;
}

/* The queue lock is held, so the resource has just become available and no
 * waiter can be between its check and its park. The waiter leaves the queue here
 * and is resumed by the caller, outside the lock. */
static cat_coroutine_t *cat_thread_wait_publish(cat_thread_wait_t *wait)
{
    return cat_thread_wait_take_first(wait);
}

static void cat_thread_wait_resume(cat_thread_wait_t *wait, cat_coroutine_t *coroutine)
{
    if (coroutine == NULL) {
        return;
    }

    cat_atomic_int64_fetch_sub(&wait->count, 1);
    cat_coroutine_wake(coroutine);
}

CAT_API cat_bool_t cat_cond_wait(cat_cond_t *cond, cat_mutex_t *mutex, cat_timeout_t timeout)
{
    struct cat_thread_wait_park_s park;
    cat_coroutine_t *current;
    cat_coroutine_t *waiting;
    cat_bool_t woken;

    if (unlikely(!cat_globals_available())) {
        cat_update_last_error(CAT_EMISUSE, "This thread has no runtime");
        return cat_false;
    }

    current = cat_coroutine_get_current();
    if (unlikely(current == NULL)) {
        cat_update_last_error(CAT_EMISUSE, "No coroutine to suspend");
        return cat_false;
    }

    /* The caller must hold the mutex, and the owner is how that is known. A
     * trylock here would answer for the thread, not for the caller. */
    if (cat_atomic_uint64_load(&mutex->owner) != (uint64_t) (uintptr_t) uv_thread_self()) {
        cat_update_last_error(CAT_EMISUSE, "The caller does not hold the mutex");
        return cat_false;
    }

    park.wait = &cond->wait;
    park.timer = NULL;

    /* Insert the waiter and release the mutex as one step. A signal needs the
     * mutex, so no signal can arrive between them and get lost. */
    uv_mutex_lock(&cond->wait.lock);
    cat_thread_wait_enqueue(&park, current);
    cat_atomic_bool_store(&mutex->held, cat_false);

    /* The release has to wake a coroutine that waits for the mutex, the same way
     * an unlock does. */
    uv_mutex_lock(&mutex->wait.lock);
    uv_mutex_unlock(&mutex->lock);
    waiting = cat_thread_wait_publish(&mutex->wait);
    uv_mutex_unlock(&mutex->wait.lock);
    uv_mutex_unlock(&cond->wait.lock);

    cat_thread_wait_resume(&mutex->wait, waiting);

    woken = cat_thread_wait_suspend(&park, timeout);

    /* Both paths take the mutex again, so the caller keeps what it held. */
    if (unlikely(!cat_mutex_lock(mutex, CAT_TIMEOUT_FOREVER))) {
        return cat_false;
    }

    if (!woken) {
        cat_update_last_error(CAT_ETIMEDOUT, "Timed out waiting for the condition");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_mutex_init(cat_mutex_t *mutex)
{
    do {
        int error = uv_mutex_init(&mutex->lock);

        if (unlikely(error != 0)) {
            cat_update_last_error_with_reason(error, "Mutex init failed");
            return cat_false;
        }
    } while (0);

    cat_atomic_bool_init(&mutex->held, cat_false);
    cat_atomic_uint64_init(&mutex->owner, 0);

    return cat_thread_wait_init(&mutex->wait);
}

CAT_API cat_bool_t cat_mutex_destroy(cat_mutex_t *mutex)
{
    if (unlikely(cat_atomic_bool_load(&mutex->held) == cat_true)) {
        /* libuv turns a destroy of a held mutex into abort() */
        cat_update_last_error(CAT_EMISUSE, "The mutex is still held");
        return cat_false;
    }

    cat_thread_wait_destroy(&mutex->wait);
    uv_mutex_destroy(&mutex->lock);

    return cat_true;
}

CAT_API cat_bool_t cat_mutex_lock(cat_mutex_t *mutex, cat_timeout_t timeout)
{
    for (;;) {
        int result;

        if (cat_mutex_try_take(mutex)) {
            return cat_true;
        }

        if (timeout == 0) {
            cat_update_last_error(CAT_EBUSY, "The mutex is held by somebody else");
            return cat_false;
        }

        result = cat_thread_wait_wait_for(&mutex->wait, timeout, cat_mutex_try_take, mutex);

        if (result < 0) {
            return cat_false;
        }
        if (result == 1) {
            return cat_true;
        }
        /* woken, but somebody else took the lock first, so look again */
    }
}

CAT_API cat_bool_t cat_mutex_unlock(cat_mutex_t *mutex)
{
    cat_bool_t held = cat_true;
    cat_coroutine_t *coroutine;

    /* A release with no holder is a mistake. Report it here, because the pthread
     * release accepts it and fails later at destroy. */
    if (unlikely(!cat_atomic_bool_compare_exchange_strong(&mutex->held, &held, cat_false))) {
        cat_update_last_error(CAT_EMISUSE, "The caller does not hold the mutex");
        return cat_false;
    }

    /* Release the lock and take a waiter while the queue lock is held. A waiter
     * cannot miss this release between its check and its suspend. */
    uv_mutex_lock(&mutex->wait.lock);
    uv_mutex_unlock(&mutex->lock);
    coroutine = cat_thread_wait_publish(&mutex->wait);
    uv_mutex_unlock(&mutex->wait.lock);

    cat_thread_wait_resume(&mutex->wait, coroutine);

    return cat_true;
}

CAT_API cat_bool_t cat_sem_init(cat_sem_t *sem, unsigned int value)
{
    do {
        int error = uv_sem_init(&sem->sem, value);

        if (unlikely(error != 0)) {
            cat_update_last_error_with_reason(error, "Semaphore init failed");
            return cat_false;
        }
    } while (0);

    return cat_thread_wait_init(&sem->wait);
}

CAT_API void cat_sem_destroy(cat_sem_t *sem)
{
    cat_thread_wait_destroy(&sem->wait);
    uv_sem_destroy(&sem->sem);
}

CAT_API cat_bool_t cat_sem_acquire(cat_sem_t *sem, cat_timeout_t timeout)
{
    for (;;) {
        int result;

        if (cat_sem_try_take(sem)) {
            return cat_true;
        }

        if (timeout == 0) {
            cat_update_last_error(CAT_EBUSY, "The semaphore is empty");
            return cat_false;
        }

        result = cat_thread_wait_wait_for(&sem->wait, timeout, cat_sem_try_take, sem);

        if (result < 0) {
            return cat_false;
        }
        if (result == 1) {
            return cat_true;
        }
        /* woken, but another waiter took the count first, so look again */
    }
}

CAT_API void cat_sem_post(cat_sem_t *sem)
{
    cat_coroutine_t *coroutine;

    /* Add the count and take a waiter while the queue lock is held. A waiter
     * cannot miss this count between its check and its suspend. */
    uv_mutex_lock(&sem->wait.lock);
    uv_sem_post(&sem->sem);
    coroutine = cat_thread_wait_publish(&sem->wait);
    uv_mutex_unlock(&sem->wait.lock);

    cat_thread_wait_resume(&sem->wait, coroutine);
}

#endif /* CAT_THREAD_SAFE */
