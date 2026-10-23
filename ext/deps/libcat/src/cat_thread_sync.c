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
    do {
        int error = uv_cond_init(&cond->wakeup);

        if (unlikely(error != 0)) {
            cat_update_last_error_with_reason(error, "Condition init failed");
            return cat_false;
        }
    } while (0);

    if (unlikely(!cat_thread_wait_init(&cond->wait))) {
        uv_cond_destroy(&cond->wakeup);
        return cat_false;
    }

    return cat_true;
}

CAT_API void cat_cond_destroy(cat_cond_t *cond)
{
    cat_thread_wait_destroy(&cond->wait);
    uv_cond_destroy(&cond->wakeup);
}

CAT_API cat_bool_t cat_cond_signal(cat_cond_t *cond)
{
    uv_mutex_lock(&cond->wait.lock);
    uv_cond_signal(&cond->wakeup);
    uv_mutex_unlock(&cond->wait.lock);

    return cat_thread_wait_wake(&cond->wait);
}

CAT_API cat_bool_t cat_cond_broadcast(cat_cond_t *cond)
{
    uv_mutex_lock(&cond->wait.lock);
    uv_cond_broadcast(&cond->wakeup);
    uv_mutex_unlock(&cond->wait.lock);

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

    if (uv_sem_trywait(&sem->sem) != 0) {
        return cat_false;
    }

    cat_atomic_int64_fetch_sub(&sem->value, 1);

    return cat_true;
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

/* The BLOCK form of cat_cond_wait below, which needs the mutex calls that come
 * after this function. */
static cat_bool_t cat_cond_block_wait(cat_cond_t *cond, cat_mutex_t *mutex, cat_timeout_t timeout);

CAT_API cat_bool_t cat_cond_wait(cat_cond_t *cond, cat_mutex_t *mutex, cat_timeout_t timeout)
{
    struct cat_thread_wait_park_s park;
    cat_coroutine_t *current;
    cat_coroutine_t *waiting;
    cat_bool_t woken;

    if (mutex->wait_mode == CAT_SYNC_WAIT_BLOCK) {
        return cat_cond_block_wait(cond, mutex, timeout);
    }

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

CAT_API cat_bool_t cat_mutex_init_ex(cat_mutex_t *mutex, cat_sync_wait_t wait)
{
    if (unlikely(wait != CAT_SYNC_WAIT_PARK && wait != CAT_SYNC_WAIT_BLOCK)) {
        cat_update_last_error(CAT_EINVAL, "Unknown wait mode");
        return cat_false;
    }

    do {
        int error = uv_mutex_init(&mutex->lock);

        if (unlikely(error != 0)) {
            cat_update_last_error_with_reason(error, "Mutex init failed");
            return cat_false;
        }
    } while (0);

    do {
        int error = uv_cond_init(&mutex->wakeup);

        if (unlikely(error != 0)) {
            cat_update_last_error_with_reason(error, "Mutex init cond failed");
            uv_mutex_destroy(&mutex->lock);
            return cat_false;
        }
    } while (0);

    cat_atomic_bool_init(&mutex->held, cat_false);
    cat_atomic_uint64_init(&mutex->owner, 0);
    mutex->wait_mode = wait;

    if (unlikely(!cat_thread_wait_init(&mutex->wait))) {
        uv_cond_destroy(&mutex->wakeup);
        uv_mutex_destroy(&mutex->lock);
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_mutex_init(cat_mutex_t *mutex)
{
    return cat_mutex_init_ex(mutex, CAT_SYNC_WAIT_PARK);
}

CAT_API cat_bool_t cat_mutex_destroy(cat_mutex_t *mutex)
{
    if (unlikely(cat_atomic_bool_load(&mutex->held) == cat_true)) {
        /* libuv turns a destroy of a held mutex into abort() */
        cat_update_last_error(CAT_EMISUSE, "The mutex is still held");
        return cat_false;
    }

    cat_thread_wait_destroy(&mutex->wait);
    uv_cond_destroy(&mutex->wakeup);
    uv_mutex_destroy(&mutex->lock);

    return cat_true;
}

/* Block the calling thread until a release signals the condition, or the
 * timeout runs out. cat_false with CAT_ETIMEDOUT when the timeout runs out. */
static cat_bool_t cat_mutex_block(cat_mutex_t *mutex, cat_timeout_t timeout)
{
    int error;

    uv_mutex_lock(&mutex->wait.lock);

    if (!cat_atomic_bool_load(&mutex->held)) {
        /* the holder released between the failed take and this lock */
        uv_mutex_unlock(&mutex->wait.lock);
        return cat_true;
    }

    if (timeout == CAT_TIMEOUT_FOREVER) {
        uv_cond_wait(&mutex->wakeup, &mutex->wait.lock);
        error = 0;
    } else {
        error = uv_cond_timedwait(&mutex->wakeup, &mutex->wait.lock,
            (uint64_t) timeout * 1000000ULL);
    }

    uv_mutex_unlock(&mutex->wait.lock);

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Timed out waiting for the mutex");
        return cat_false;
    }

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

        if (mutex->wait_mode == CAT_SYNC_WAIT_BLOCK) {
            if (unlikely(!cat_mutex_block(mutex, timeout))) {
                return cat_false;
            }
            /* a release woke the thread, so look again */
            continue;
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

    /* Release the lock and wake a waiter while the queue lock is held. A waiter
     * cannot miss this release between its check and its wait. */
    uv_mutex_lock(&mutex->wait.lock);

    /* A release with no holder is a mistake. Report it here, because the pthread
     * release accepts it and fails later at destroy. */
    if (unlikely(!cat_atomic_bool_compare_exchange_strong(&mutex->held, &held, cat_false))) {
        uv_mutex_unlock(&mutex->wait.lock);
        cat_update_last_error(CAT_EMISUSE, "The caller does not hold the mutex");
        return cat_false;
    }

    uv_mutex_unlock(&mutex->lock);

    if (mutex->wait_mode == CAT_SYNC_WAIT_BLOCK) {
        uv_cond_signal(&mutex->wakeup);
        uv_mutex_unlock(&mutex->wait.lock);
        return cat_true;
    }

    coroutine = cat_thread_wait_publish(&mutex->wait);
    uv_mutex_unlock(&mutex->wait.lock);

    cat_thread_wait_resume(&mutex->wait, coroutine);

    return cat_true;
}

/* The BLOCK form of cat_cond_wait: release the mutex, sleep in the condition,
 * and take the mutex again. */
static cat_bool_t cat_cond_block_wait(cat_cond_t *cond, cat_mutex_t *mutex, cat_timeout_t timeout)
{
    int error;

    /* The caller must hold the mutex, and the owner is how that is known. */
    if (unlikely(cat_atomic_uint64_load(&mutex->owner) != (uint64_t) (uintptr_t) uv_thread_self())) {
        cat_update_last_error(CAT_EMISUSE, "The caller does not hold the mutex");
        return cat_false;
    }

    uv_mutex_lock(&cond->wait.lock);

    /* Release the mutex and wake a thread that waits for it as one step, so a
     * release is not lost between the two. */
    uv_mutex_lock(&mutex->wait.lock);
    cat_atomic_bool_store(&mutex->held, cat_false);
    uv_mutex_unlock(&mutex->lock);
    uv_cond_signal(&mutex->wakeup);
    uv_mutex_unlock(&mutex->wait.lock);

    if (timeout == CAT_TIMEOUT_FOREVER) {
        uv_cond_wait(&cond->wakeup, &cond->wait.lock);
        error = 0;
    } else {
        error = uv_cond_timedwait(&cond->wakeup, &cond->wait.lock,
            (uint64_t) timeout * 1000000ULL);
    }

    uv_mutex_unlock(&cond->wait.lock);

    /* Both paths take the mutex again, so the caller keeps what it held. */
    if (unlikely(!cat_mutex_lock(mutex, CAT_TIMEOUT_FOREVER))) {
        return cat_false;
    }

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Timed out waiting for the condition");
        return cat_false;
    }

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

    cat_atomic_int64_init(&sem->value, (int64_t) value);

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

CAT_API void cat_sem_release(cat_sem_t *sem, size_t count)
{
    cat_coroutine_t *coroutine;
    size_t index;

    if (count == 0) {
        return;
    }

    /* Add the counts and take a waiter while the queue lock is held. A waiter
     * cannot miss a count between its check and its suspend. */
    uv_mutex_lock(&sem->wait.lock);
    for (index = 0; index < count; index++) {
        uv_sem_post(&sem->sem);
    }
    cat_atomic_int64_fetch_add(&sem->value, (int64_t) count);
    coroutine = cat_thread_wait_publish(&sem->wait);
    uv_mutex_unlock(&sem->wait.lock);

    cat_thread_wait_resume(&sem->wait, coroutine);

    /* Each count serves one coroutine, so no waiter beyond the count leaves. */
    for (index = 1; index < count; index++) {
        if (!cat_thread_wait_wake(&sem->wait)) {
            break;
        }
    }
}

CAT_API void cat_sem_post(cat_sem_t *sem)
{
    cat_sem_release(sem, 1);
}

CAT_API cat_bool_t cat_sem_try_acquire(cat_sem_t *sem)
{
    if (cat_sem_try_take(sem)) {
        return cat_true;
    }

    cat_update_last_error(CAT_EBUSY, "The semaphore is empty");
    return cat_false;
}

CAT_API int64_t cat_sem_get_value(const cat_sem_t *sem)
{
    return cat_atomic_int64_load(&sem->value);
}

CAT_API size_t cat_sem_get_waiter_count(const cat_sem_t *sem)
{
    return (size_t) cat_atomic_int64_load(&sem->wait.count);
}

#endif /* CAT_THREAD_SAFE */
