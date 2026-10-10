/* MongoDB native runtime ABI. Distributed under the Apache-2.0 license.
 * This header is also distributed with consumers; change the ABI version
 * whenever the callback layout or a callback's contract changes. */
#ifndef PHP_MONGODB_RUNTIME_H
#define PHP_MONGODB_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
typedef WSAPOLLFD php_mongodb_pollfd_t;
#else
#include <poll.h>
typedef struct pollfd php_mongodb_pollfd_t;
#endif

#define PHP_MONGODB_RUNTIME_ABI 1

typedef struct php_mongodb_runtime_hooks {
    bool (*active)(void);
    int (*poll)(php_mongodb_pollfd_t *fds, size_t count, int timeout_ms);
    void (*sleep)(int64_t usec);
    /* The work function must not call Zend. Cleanup runs exactly once after
     * work completes or is cancelled, including after the waiter has left. */
    bool (*work)(void (*function)(void *), void (*cleanup)(void *), void *data);
    /* Keys are copied by the provider. The same coroutine may re-enter. */
    bool (*acquire)(const void *key, size_t length);
    void (*release)(const void *key, size_t length);
    void (*forget)(const void *key, size_t length);
} php_mongodb_runtime_hooks;

typedef bool (*php_mongodb_register_runtime_t)(uint32_t abi, size_t size, const php_mongodb_runtime_hooks *hooks);
#endif
