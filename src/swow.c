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
  | Author: Twosee <twosee@php.net>                                          |
  +--------------------------------------------------------------------------+
 */

#include "swow.h"
#include "swow_log.h"
#include "swow_defer.h"
#include "swow_coroutine.h"
#include "swow_channel.h"
#include "swow_sync.h"
#include "swow_event.h"
#include "swow_time.h"
#include "swow_buffer.h"
#include "swow_socket.h"
#include "swow_stream.h"
#include "swow_signal.h"
#include "swow_watch_dog.h"
#include "swow_debug.h"
#include "swow_http.h"
#include "swow_websocket.h"

#include "SAPI.h"

#include "ext/standard/info.h"

#ifdef SWOW_DISABLE_SESSION
#if HAVE_PHP_SESSION && !defined(COMPILE_DL_SESSION)
#define SWOW_HAVE_SESSION
#include "ext/session/php_session.h"
#endif
#endif

SWOW_API zend_class_entry *swow_ce;
SWOW_API zend_object_handlers swow_handlers;

SWOW_API zend_class_entry *swow_module_ce;
SWOW_API zend_object_handlers swow_module_handlers;

ZEND_DECLARE_MODULE_GLOBALS(swow)

typedef int (*zend_loader_t)(INIT_FUNC_ARGS);
typedef int (*zend_delay_loader_t)(void);

#ifdef ZEND_ACC_HAS_TYPE_HINTS_DENY
HashTable swow_type_hint_functions;
#endif

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(swow)
{
    swow_wrapper_init();

    static const zend_loader_t minit_callbacks[] = {
        swow_module_init,
        swow_log_module_init,
        swow_exceptions_module_init,
        swow_defer_module_init,
        swow_coroutine_module_init,
        swow_channel_module_init,
        swow_sync_module_init,
        swow_event_module_init,
        swow_time_module_init,
        swow_buffer_module_init,
        swow_socket_module_init,
        swow_stream_module_init,
        swow_signal_module_init,
        swow_watch_dog_module_init,
        swow_debug_module_init,
        swow_http_module_init,
        swow_websocket_module_init,
    };

    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(minit_callbacks); i++) {
        if (minit_callbacks[i](INIT_FUNC_ARGS_PASSTHRU) != SUCCESS) {
            return FAILURE;
        }
    }

#ifdef ZEND_ACC_HAS_TYPE_HINTS_DENY
    zend_hash_init(&swow_type_hint_functions, 0, NULL, NULL, 1);
#define SWOW_FUNCTIONS_REMOVE_TYPE_HINT(function_table) do { \
    void *ptr; \
    ZEND_HASH_FOREACH_PTR(function_table, ptr) { \
        zend_function *zif = (zend_function *) ptr; \
        if ( \
            (zif->common.fn_flags & ZEND_ACC_HAS_TYPE_HINTS) && \
            zif->common.type == ZEND_INTERNAL_FUNCTION && \
            zif->internal_function.module->module_number == module_number \
        ) { \
            zif->common.fn_flags ^= ZEND_ACC_HAS_TYPE_HINTS; \
            zend_hash_next_index_insert_ptr(&swow_type_hint_functions, zif); \
        } \
    } ZEND_HASH_FOREACH_END(); \
} while (0);
    SWOW_FUNCTIONS_REMOVE_TYPE_HINT(CG(function_table));
    do {
        void *ptr;
        ZEND_HASH_FOREACH_PTR(CG(class_table), ptr) {
            zend_class_entry *ce = (zend_class_entry *) ptr;
            if (ce->info.internal.module->module_number == module_number) {
                SWOW_FUNCTIONS_REMOVE_TYPE_HINT(&ce->function_table);
            }
        } ZEND_HASH_FOREACH_END();
    } while (0);
#undef SWOW_FUNCTIONS_REMOVE_TYPE_HINT
#endif

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(swow)
{
#ifdef ZEND_ACC_HAS_TYPE_HINTS_DENY
    do {
        void *ptr;
        ZEND_HASH_FOREACH_PTR(&swow_type_hint_functions, ptr) {
            zend_function *zif = (zend_function *) ptr;
            zif->common.fn_flags |= ZEND_ACC_HAS_TYPE_HINTS;
        } ZEND_HASH_FOREACH_END();
    } while (0);
    zend_hash_destroy(&swow_type_hint_functions);
#endif

    static const zend_loader_t mshutdown_callbacks[] = {
        swow_module_shutdown,
    };

    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(mshutdown_callbacks); i++) {
        if (mshutdown_callbacks[i](SHUTDOWN_FUNC_ARGS_PASSTHRU) != SUCCESS) {
            return FAILURE;
        }
    }

    swow_wrapper_shutdown();

    return SUCCESS;
}
/* }}} */

/* Notice: runtime order is by dependency */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(swow)
{
    static const zend_loader_t rinit_callbacks[] = {
        swow_runtime_init,
        swow_coroutine_runtime_init,
        swow_event_runtime_init,
        swow_socket_runtime_init,
        swow_stream_runtime_init,
        swow_watch_dog_runtime_init,
        swow_debug_runtime_init,
    };

#ifdef ZEND_COMPILE_PRELOAD
    if (CG(compiler_options) & ZEND_COMPILE_PRELOAD) {
        return SUCCESS;
    }
#endif

    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(rinit_callbacks); i++) {
        if (rinit_callbacks[i](INIT_FUNC_ARGS_PASSTHRU) != SUCCESS) {
            return FAILURE;
        }
    }

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(swow)
{
    static const zend_loader_t rshutdown_callbacks[] = {
        swow_debug_runtime_shutdown,
        swow_watch_dog_runtime_shudtown,
        swow_stream_runtime_shutdown,
        swow_event_runtime_shutdown,
        swow_coroutine_runtime_shutdown,
        swow_runtime_shutdown,
    };

#ifdef ZEND_COMPILE_PRELOAD
    if (CG(compiler_options) & ZEND_COMPILE_PRELOAD) {
        return SUCCESS;
    }
#endif

    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(rshutdown_callbacks); i++) {
        if (rshutdown_callbacks[i](SHUTDOWN_FUNC_ARGS_PASSTHRU) != SUCCESS) {
            return FAILURE;
        }
    }

    return SUCCESS;
}
/* }}} */

static int swow_delay_runtime_shutdown(void)
{
    static const zend_delay_loader_t rshutdown_callbacks[] = {
        swow_event_delay_runtime_shutdown,
    };

    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(rshutdown_callbacks); i++) {
        if (rshutdown_callbacks[i]() != SUCCESS) {
            return FAILURE;
        }
    }

    return SUCCESS;
}

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(swow)
{
#ifndef CAT_THREAD_SAFE
#define SWOW_VERSION_SUFFIX "NTS"
#else
#define SWOW_VERSION_SUFFIX "CTS"
#endif

#ifdef CAT_DEBUG
#define SWOW_VERSION_SUFFIX_EXT " DEBUG"
#else
#define SWOW_VERSION_SUFFIX_EXT ""
#endif

#ifdef CAT_COROUTINE_USE_UCONTEXT
#define SWOW_COROUTINE_CONTEXT_TYPE "ucontext"
#else
#define SWOW_COROUTINE_CONTEXT_TYPE "boost-context"
#endif

    php_info_print_table_start();
    php_info_print_table_header(2, "Status", "enabled");
    php_info_print_table_header(2, "Author", "twosee <twosee@php.net>");
    php_info_print_table_header(2, "Version", SWOW_VERSION " ( " SWOW_VERSION_SUFFIX SWOW_VERSION_SUFFIX_EXT " )");
    php_info_print_table_header(2, "Context", SWOW_COROUTINE_CONTEXT_TYPE);
    php_info_print_table_header(2, "Scheduler", "libuv-event");
    php_info_print_table_end();
}
/* }}} */

static const zend_function_entry swow_functions[] = {
    PHP_FE_END
};

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_errno_strerror, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, error, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(swow_errno_strerror)
{
    zend_long error;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(error)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING(cat_strerror(error));
}

static const zend_function_entry swow_errno_functions[] = {
    PHP_FENTRY(Swow\\Errno\\strerror, PHP_FN(swow_errno_strerror), arginfo_swow_errno_strerror, 0)
    PHP_FE_END
};

int swow_module_init(INIT_FUNC_ARGS)
{
#ifdef SWOW_DISABLE_SESSION
    /* disable some conflict internal modules */
    do {
        SWOW_MODULES_CHECK_PRE_START() {
            "session"
        } SWOW_MODULES_CHECK_PRE_END();
    #ifdef SWOW_HAVE_SESSION
        PS(auto_start) = 0;
    #endif
        zend_disable_function(ZEND_STRL("session_start"));
    } while (0);
#endif

#ifdef ZEND_SIGNALS
    /* disable warning even in ZEND_DEBUG because we may register our own signal handlers  */
    SIGG(check) = 0;
#endif

    cat_module_init();

    /* Swow class is reserved  */
    swow_ce = swow_register_internal_class(
        "Swow", NULL, NULL,
        &swow_handlers, NULL,
        cat_false, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );

    /* Errno constants */
#define SWOW_ERRNO_GEN(name, message) REGISTER_LONG_CONSTANT("Swow\\Errno\\" #name, CAT_##name, CONST_CS | CONST_PERSISTENT);
    CAT_ERRNO_MAP(SWOW_ERRNO_GEN)
#undef SWOW_ERRNO_GEN
    if (zend_register_functions(NULL, swow_errno_functions, NULL, MODULE_PERSISTENT) != SUCCESS) {
        return FAILURE;
    }

    /* Module constants */
    swow_module_ce = swow_register_internal_class(
        "Swow\\Module", NULL, NULL,
        &swow_module_handlers, NULL,
        cat_false, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );
#define SWOW_MODULE_TYPE_GEN(name, value) \
    zend_declare_class_constant_long(swow_module_ce, ZEND_STRL("TYPE_" #name), (value));
    CAT_MODULE_TYPE_MAP(SWOW_MODULE_TYPE_GEN)
#undef SWOW_MODULE_TYPE_GEN
#define SWOW_MODULE_UNION_TYPE_GEN(name, value) \
    zend_declare_class_constant_long(swow_module_ce, ZEND_STRL("TYPES_" #name), (value));
    CAT_MODULE_UNION_TYPE_MAP(SWOW_MODULE_UNION_TYPE_GEN)
#undef SWOW_MODULE_UNION_TYPE_GEN

    return SUCCESS;
}

int swow_module_shutdown(INIT_FUNC_ARGS)
{
    cat_module_shutdown();

    return SUCCESS;
}

int swow_runtime_init(INIT_FUNC_ARGS)
{
#if defined(ZTS) && defined(COMPILE_DL_SWOW)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    /* See: https://bugs.php.net/bug.php?id=79064 */
    EG(full_tables_cleanup) = cat_true;

    cat_runtime_init();

    return SUCCESS;
}

int swow_runtime_shutdown(INIT_FUNC_ARGS)
{
    cat_runtime_shutdown();

    return SUCCESS;
}

/* {{{ swow_module_entry
 */
SWOW_API zend_module_entry swow_module_entry = {
    STANDARD_MODULE_HEADER,
    "Swow",                       /* Extension name */
    swow_functions,              /* zend_function_entry */
    PHP_MINIT(swow),             /* PHP_MINIT - Module initialization */
    PHP_MSHUTDOWN(swow),         /* PHP_MSHUTDOWN - Module shutdown */
    PHP_RINIT(swow),             /* PHP_RINIT - Request initialization */
    PHP_RSHUTDOWN(swow),         /* PHP_RSHUTDOWN - Request shutdown */
    PHP_MINFO(swow),             /* PHP_MINFO - Module info */
    SWOW_VERSION,                /* Version */
    0, NULL,                     /* globals descriptor */
    NULL,                        /* globals ctor */
    NULL,                        /* globals dtor */
    swow_delay_runtime_shutdown, /* post deactivate */
    STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_SWOW
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(swow)
#endif
