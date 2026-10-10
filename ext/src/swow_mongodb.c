/* Native socket/TLS ownership and error interpretation remain with MongoDB.
 * libcat only waits; the driver identifies the clients requiring protection. */
#include "swow_mongodb.h"
#ifdef SWOW_HAVE_MONGODB
#include "php_mongodb_runtime.h"
#include "cat_poll.h"
#include "cat_work.h"
#include "cat_time.h"
#include "cat_channel.h"
#include "cat_coroutine.h"
#include <errno.h>

static php_mongodb_register_runtime_t swow_mongodb_register;

typedef struct {
    cat_channel_t channel;
    cat_coroutine_t *owner;
    size_t depth;
    bool retired;
    size_t handoffs;
} swow_mongodb_lock_t;

static void swow_mongodb_lock_dtor(zval *value)
{
    swow_mongodb_lock_t *lock = Z_PTR_P(value);
    cat_channel_cleanup(&lock->channel);
    efree(lock);
}

static bool swow_mongodb_active(void)
{
    return SWOW_G(mongodb.active) && cat_coroutine_get_current() != NULL;
}

static void swow_mongodb_check_process(void)
{
    if (SWOW_G(mongodb.pid) != getpid()) {
        swow_mongodb_lock_t *lock;
        /* A forked child has no owners from the parent's other coroutines.
         * Remove inherited waiters without scheduling their copied stacks. */
        ZEND_HASH_FOREACH_PTR(&SWOW_G(mongodb.locks), lock) {
            cat_queue_init(&lock->channel.producers);
            cat_queue_init(&lock->channel.consumers);
        } ZEND_HASH_FOREACH_END();
        zend_hash_clean(&SWOW_G(mongodb.locks));
        SWOW_G(mongodb.pid) = getpid();
    }
}

static bool swow_mongodb_acquire(const void *key, size_t length)
{
    swow_mongodb_lock_t *lock;
    cat_coroutine_t *current = cat_coroutine_get_current();
    unsigned char token = 0;
    swow_mongodb_check_process();
    lock = zend_hash_str_find_ptr(&SWOW_G(mongodb.locks), key, length);
    if (lock == NULL) {
        lock = ecalloc(1, sizeof(*lock));
        cat_channel_create(&lock->channel, 1, sizeof(token), NULL);
        zend_hash_str_add_ptr(&SWOW_G(mongodb.locks), key, length, lock);
    }
    if (lock->owner == current) {
        lock->depth++;
        return true;
    }
    if (!cat_channel_push(&lock->channel, &token, CAT_TIMEOUT_FOREVER)) {
        return false;
    }
    lock->owner = current;
    lock->depth = 1;
    return true;
}

static void swow_mongodb_release(const void *key, size_t length)
{
    swow_mongodb_lock_t *lock = zend_hash_str_find_ptr(&SWOW_G(mongodb.locks), key, length);
    unsigned char token;
    if (lock == NULL || lock->owner != cat_coroutine_get_current()) {
        return;
    }
    if (--lock->depth == 0) {
        /* pop may resume the next owner immediately; never overwrite the
         * owner after pop has returned. */
        lock->owner = NULL;
        lock->handoffs++;
        (void) cat_channel_pop(&lock->channel, &token, 0);
        lock->handoffs--;
        if (lock->owner == NULL && lock->retired && lock->handoffs == 0 && !cat_channel_has_producers(&lock->channel)) {
            zend_hash_str_del(&SWOW_G(mongodb.locks), key, length);
        }
    }
}

static void swow_mongodb_forget(const void *key, size_t length)
{
    swow_mongodb_lock_t *lock = zend_hash_str_find_ptr(&SWOW_G(mongodb.locks), key, length);
    if (lock == NULL) {
        return;
    }
    lock->retired = true;
    if (lock->owner == NULL && lock->handoffs == 0 && !cat_channel_has_producers(&lock->channel)) {
        zend_hash_str_del(&SWOW_G(mongodb.locks), key, length);
    }
}

static int swow_mongodb_poll(php_mongodb_pollfd_t *fds, size_t count, int timeout_ms)
{
    int saved_errno = errno;
#ifdef CAT_OS_WIN
    int saved_socket_error = WSAGetLastError();
#endif
    int ret;
    if (EG(exception) != NULL) {
        goto cancelled;
    }
    ret = cat_poll((cat_pollfd_t *) fds, (cat_nfds_t) count, timeout_ms);
    /* A forced resume for throw()/kill() may look like a zero-event poll.
     * Return a terminal system error so libmongoc unwinds its native I/O
     * instead of repeatedly polling while PHP has a pending exception. */
    if (EG(exception) != NULL) {
        goto cancelled;
    }
    if (ret < 0) {
        errno = cat_orig_errno(cat_get_last_error_code());
#ifdef CAT_OS_WIN
        WSASetLastError(errno);
#endif
    } else {
        errno = saved_errno;
#ifdef CAT_OS_WIN
        WSASetLastError(saved_socket_error);
#endif
    }
    return ret;

cancelled:
#ifdef CAT_OS_WIN
    errno = WSAECONNABORTED;
    WSASetLastError(errno);
#else
    errno = ECANCELED;
#endif
    return -1;
}

static void swow_mongodb_sleep(int64_t usec)
{
    int saved_errno = errno;
    if (cat_time_usleep((cat_usec_t) usec) == 0) {
        errno = saved_errno;
    }
}

static bool swow_mongodb_work(void (*function)(void *), void (*cleanup)(void *), void *data)
{
    return cat_work(CAT_WORK_KIND_SLOW_IO, function, cleanup, data, CAT_TIMEOUT_FOREVER);
}

static const php_mongodb_runtime_hooks swow_mongodb_hooks = {
    swow_mongodb_active,
    swow_mongodb_poll,
    swow_mongodb_sleep,
    swow_mongodb_work,
    swow_mongodb_acquire,
    swow_mongodb_release,
    swow_mongodb_forget,
};

zend_result swow_mongodb_module_init(INIT_FUNC_ARGS)
{
    zend_module_entry *mongodb = zend_hash_str_find_ptr(&module_registry, ZEND_STRL("mongodb"));
    if (mongodb != NULL && mongodb->handle != NULL) {
        swow_mongodb_register = (php_mongodb_register_runtime_t) DL_FETCH_SYMBOL(mongodb->handle, "php_mongodb_register_runtime");
    }
    return SUCCESS;
}

zend_result swow_mongodb_runtime_init(INIT_FUNC_ARGS)
{
    if (swow_mongodb_register == NULL) {
        return SUCCESS;
    }
    zend_hash_init(&SWOW_G(mongodb.locks), 0, NULL, swow_mongodb_lock_dtor, 0);
    SWOW_G(mongodb.pid) = getpid();
    SWOW_G(mongodb.active) = true;
    if (!swow_mongodb_register(PHP_MONGODB_RUNTIME_ABI, sizeof(swow_mongodb_hooks), &swow_mongodb_hooks)) {
        SWOW_G(mongodb.active) = false;
        zend_hash_destroy(&SWOW_G(mongodb.locks));
        php_error_docref(NULL, E_WARNING, "MongoDB runtime ABI is incompatible with Swow");
        return FAILURE;
    }
    return SUCCESS;
}

zend_result swow_mongodb_runtime_shutdown(SHUTDOWN_FUNC_ARGS)
{
    if (SWOW_G(mongodb.active)) {
        /* Called after Swow has stopped its coroutines, and before MongoDB
         * destroys request clients. Driver teardown then uses native waits. */
        swow_mongodb_register(PHP_MONGODB_RUNTIME_ABI, sizeof(swow_mongodb_hooks), NULL);
        SWOW_G(mongodb.active) = false;
        zend_hash_destroy(&SWOW_G(mongodb.locks));
    }
    return SUCCESS;
}
#endif
