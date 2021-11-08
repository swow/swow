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
#include "swow_curl.h"

#include "cat_api.h"

#include "SAPI.h"

#include "zend_smart_str.h"

#include "ext/standard/info.h"

#ifdef SWOW_DISABLE_SESSION
#if HAVE_PHP_SESSION && !defined(COMPILE_DL_SESSION)
#define SWOW_HAVE_SESSION
#include "ext/session/php_session.h"
#endif
#endif

#if (defined(HAVE_PCRE) || defined(HAVE_BUNDLED_PCRE)) && !defined(COMPILE_DL_PCRE) && \
     defined(__MACH__) && !defined(CAT_DEBUG)
#include "ext/pcre/php_pcre.h"
#endif

#ifdef CAT_HAVE_CURL
#include <curl/curl.h>
#endif

SWOW_API zend_class_entry *swow_ce;
SWOW_API zend_object_handlers swow_handlers;

SWOW_API zend_class_entry *swow_module_ce;
SWOW_API zend_object_handlers swow_module_handlers;

ZEND_DECLARE_MODULE_GLOBALS(swow)

SWOW_API swow_nts_globals_t swow_nts_globals;

typedef int (*swow_init_function_t)(INIT_FUNC_ARGS);
typedef int (*swow_shutdown_function_t)(INIT_FUNC_ARGS);
typedef zend_result (*swow_delay_shutdown_function_t)(void);

#ifdef ZEND_ACC_HAS_TYPE_HINTS_DENY
HashTable swow_type_hint_functions;
#endif

/* {{{ PHP_GINIT_FUNCTION */
static PHP_GINIT_FUNCTION(swow)
{
#if defined(COMPILE_DL_BCMATH) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    memset(swow_globals, 0, sizeof(*swow_globals));
}
/* }}} */

/* {{{ PHP_GSHUTDOWN_FUNCTION */
static PHP_GSHUTDOWN_FUNCTION(swow)
{
    /* reserved */
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(swow)
{
    size_t i;

    /* Conflict extensions check */
    if (zend_hash_str_find_ptr(&module_registry, ZEND_STRL("swoole"))) {
        zend_error(E_WARNING, "Swow is incompatible with Swoole "
            "because both of Swow and Swoole provide the similar functionality through different implementations. "
            "Please disable one of Swow or Swoole and re-run.");
        return FAILURE;
    }

    /* Debug extensions check */
    static const char *debug_extension_names[] = { "xdebug", "ddtrace" };
#ifndef SWOW_COROUTINE_MOCK_FIBER_CONTEXT
    smart_str str;
    memset(&str, 0, sizeof(str));
#endif
    for (i = 0; i < CAT_ARRAY_SIZE(debug_extension_names); i++) {
        const char *name = debug_extension_names[i];
        if (zend_hash_str_find_ptr(&module_registry, name, strlen(name))) {
#ifndef SWOW_COROUTINE_MOCK_FIBER_CONTEXT
            smart_str_appends(&str, name);
            smart_str_appends(&str, ", ");
#endif
            SWOW_NTS_G(has_debug_extension) = 1;
        }
    }
#ifndef SWOW_COROUTINE_MOCK_FIBER_CONTEXT
    if (str.s != NULL) {
        ZSTR_LEN(str.s) -= CAT_STRLEN(", "); // rtrim(&str, ", ")
        smart_str_0(&str);
        zend_error(E_WARNING, "Please note that using Swow with %s may cause unknown problems when PHP version < 8.1", ZSTR_VAL(str.s));
        smart_str_free(&str);
    }
#endif

    swow_wrapper_init();

    static const swow_init_function_t minit_functions[] = {
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
#ifdef CAT_HAVE_CURL
        swow_curl_module_init,
#endif
    };

    for (i = 0; i < CAT_ARRAY_SIZE(minit_functions); i++) {
        if (minit_functions[i](INIT_FUNC_ARGS_PASSTHRU) != SUCCESS) {
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

    SWOW_NTS_G(cli) = strcmp(sapi_module.name, "cli") != 0 &&
                      strcmp(sapi_module.name, "phpdbg") != 0;

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

    static const swow_shutdown_function_t mshutdown_functions[] = {
#ifdef CAT_HAVE_CURL
        swow_curl_module_shutdown,
#endif
        swow_stream_module_shutdown,
        swow_event_module_shutdown,
        swow_module_shutdown,
    };

    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(mshutdown_functions); i++) {
        if (mshutdown_functions[i](SHUTDOWN_FUNC_ARGS_PASSTHRU) != SUCCESS) {
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
    static const swow_init_function_t rinit_functions[] = {
        swow_runtime_init,
        swow_coroutine_runtime_init,
        swow_event_runtime_init,
        swow_socket_runtime_init,
        swow_stream_runtime_init,
        swow_watch_dog_runtime_init,
        swow_debug_runtime_init,
    };

// FIXME: Why opcache do not set ZEND_COMPILE_PRELOAD before runtime init anymore?
// #ifdef ZEND_COMPILE_PRELOAD
//     if (CG(compiler_options) & ZEND_COMPILE_PRELOAD) {
//         return SUCCESS;
//     }
// #endif

    SWOW_G(runtime_state) = SWOW_RUNTIME_STATE_INIT;

    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(rinit_functions); i++) {
        if (rinit_functions[i](INIT_FUNC_ARGS_PASSTHRU) != SUCCESS) {
            return FAILURE;
        }
    }

    SWOW_G(runtime_state) = SWOW_RUNTIME_STATE_RUNNING;

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(swow)
{
    static const swow_shutdown_function_t rshutdown_functions[] = {
        swow_debug_runtime_shutdown,
        swow_watch_dog_runtime_shudtown,
        swow_stream_runtime_shutdown,
        swow_event_runtime_shutdown,
        swow_coroutine_runtime_shutdown,
        swow_runtime_shutdown,
    };

// #ifdef ZEND_COMPILE_PRELOAD
//     if (CG(compiler_options) & ZEND_COMPILE_PRELOAD) {
//         return SUCCESS;
//     }
// #endif

    SWOW_G(runtime_state) = SWOW_RUNTIME_STATE_SHUTDOWN;

    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(rshutdown_functions); i++) {
        if (rshutdown_functions[i](SHUTDOWN_FUNC_ARGS_PASSTHRU) != SUCCESS) {
            return FAILURE;
        }
    }

    /* Notice: some PHP objects still has not been released
     * (e.g. static vars or set global vars on __destruct or circular references)
     * so we must re-create C coroutine env to finish them */

    if (!cat_runtime_init_all()) {
        return FAILURE;
    }

    if (!cat_run(CAT_RUN_EASY)) {
        return FAILURE;
    }

    SWOW_G(runtime_state) = SWOW_RUNTIME_STATE_NONE;

    return SUCCESS;
}
/* }}} */

static zend_result swow_delay_runtime_shutdown(void)
{
    static const swow_delay_shutdown_function_t delay_rshutdown_functions[] = {
#if defined(CAT_HAVE_CURL) && PHP_VERSION_ID < 80000
        swow_curl_delay_runtime_shutdown,
#endif
        swow_coroutine_delay_runtime_shutdown,
    };

    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(delay_rshutdown_functions); i++) {
        if (delay_rshutdown_functions[i]() != SUCCESS) {
            return FAILURE;
        }
    }

    cat_bool_t ret;

    ret = cat_stop();

    ret = cat_runtime_shutdown_all() && ret;

    return ret ? SUCCESS : FAILURE;
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

#ifndef SWOW_GIT_VERSION
#define SWOW_GIT_VERSION ""
#endif
    smart_str str;

    php_info_print_table_start();
    php_info_print_table_row(2, "Status", "enabled");
    php_info_print_table_row(2, "Author", "Swow Team");
    php_info_print_table_row(2, "Link", "https://github.com/swow/swow");
    php_info_print_table_row(2, "Contact", "Twosee <twosee@php.net>");
    php_info_print_table_row(2, "Version", SWOW_VERSION SWOW_GIT_VERSION " ( " SWOW_VERSION_SUFFIX SWOW_VERSION_SUFFIX_EXT " )");
    str.s = zend_strpprintf(0, "%s %s", __DATE__, __TIME__);
    php_info_print_table_row(2, "Built", str.s->val);
    zend_string_release(str.s);
    php_info_print_table_row(2, "Context", SWOW_COROUTINE_CONTEXT_TYPE);
    php_info_print_table_row(2, "Scheduler", "libuv-event");
#if defined(CAT_HAVE_MSAN) || defined(CAT_HAVE_ASAN) || defined(CAT_HAVE_UBSAN)
    memset(&str, 0, sizeof(str));
# if defined(CAT_HAVE_MSAN)
    smart_str_appends(&str, "MSan, ");
# endif
# if defined(CAT_HAVE_ASAN)
    smart_str_appends(&str, "ASan, ");
# endif
# if defined(CAT_HAVE_UBSAN)
    smart_str_appends(&str, "UBSan, ");
# endif
    ZSTR_LEN(str.s) -= CAT_STRLEN(", "); // rtrim(&str, ", ")
    smart_str_0(&str);
    php_info_print_table_row(2, "Sanitizers", ZSTR_VAL(str.s));
    smart_str_free(&str);
#endif
#if defined(CAT_HAVE_CURL) || defined(CAT_HAVE_OPENSSL)
    memset(&str, 0, sizeof(str));
# ifdef CAT_HAVE_CURL
    curl_version_info_data * curl_vid = curl_version_info(CURLVERSION_NOW);
    smart_str_append_printf(&str, "cURL %s, ", curl_vid->version);
# endif
# ifdef CAT_HAVE_OPENSSL
    smart_str_append_printf(&str, "%s, ", cat_ssl_version());
# endif
    ZEND_ASSERT(str.s != NULL && ZSTR_LEN(str.s) > 1);
    ZSTR_LEN(str.s) -= CAT_STRLEN(", "); // rtrim(&str, ", ")
    smart_str_0(&str);
    php_info_print_table_row(2, "With", ZSTR_VAL(str.s));
    smart_str_free(&str);
#endif
    php_info_print_table_end();
}
/* }}} */

static const zend_function_entry swow_functions[] = {
    PHP_FE_END
};

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Errno_strerror, ZEND_RETURN_VALUE, 0, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, error, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(Swow_Errno_strerror)
{
    zend_long error;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(error)
    ZEND_PARSE_PARAMETERS_END();

    // overflow for int
    if(error < INT_MIN || error > INT_MAX){
        zend_value_error("Errno passed in is not in errno_t range");
        RETURN_THROWS();
    }

    RETURN_STRING(cat_strerror(error));
}

static const zend_function_entry swow_errno_functions[] = {
    PHP_FENTRY(Swow\\Errno\\strerror, PHP_FN(Swow_Errno_strerror), arginfo_Swow_Errno_strerror, 0)
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

#if defined(PHP_PCRE_VERSION) && defined(HAVE_PCRE_JIT_SUPPORT)
    /* enable pcre.jit with coroutine on MacOS will lead to segment fault, disable it temporarily */
    PCRE_G(jit) = 0;
#endif

    cat_module_init();

    /* Swow class is reserved  */
    swow_ce = swow_register_internal_class(
        "Swow", NULL, NULL,
        &swow_handlers, NULL,
        cat_false, cat_false,
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
        cat_false, cat_false,
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

    /* Version constants (TODO: remove type cast if we no longger support PHP 7.x) */
#define SWOW_VERSION_MAP(XX) \
    XX(MAJOR_VERSION, LONG, zend_long) \
    XX(MINOR_VERSION, LONG, zend_long) \
    XX(RELEASE_VERSION, LONG, zend_long) \
    XX(EXTRA_VERSION, STRING, char *) \
    XX(VERSION, STRING, char *) \
    XX(VERSION_ID, LONG, zend_long)

#define SWOW_VERSION_GEN(name, type, cast) REGISTER_##type##_CONSTANT("Swow\\" #name, (cast) SWOW_##name, CONST_CS | CONST_PERSISTENT);
    SWOW_VERSION_MAP(SWOW_VERSION_GEN)
#undef SWOW_VERSION_GEN

    return SUCCESS;
}

int swow_module_shutdown(INIT_FUNC_ARGS)
{
    cat_module_shutdown();

    return SUCCESS;
}

int swow_runtime_init(INIT_FUNC_ARGS)
{
    /* See: https://bugs.php.net/bug.php?id=79064 */
    // EG(full_tables_cleanup) = cat_true; // not needed for now

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
    "Swow",                      /* Extension name */
    swow_functions,              /* zend_function_entry */
    PHP_MINIT(swow),             /* PHP_MINIT - Module initialization */
    PHP_MSHUTDOWN(swow),         /* PHP_MSHUTDOWN - Module shutdown */
    PHP_RINIT(swow),             /* PHP_RINIT - Request initialization */
    PHP_RSHUTDOWN(swow),         /* PHP_RSHUTDOWN - Request shutdown */
    PHP_MINFO(swow),             /* PHP_MINFO - Module info */
    SWOW_VERSION,                /* version */
    PHP_MODULE_GLOBALS(swow),    /* globals descriptor */
    PHP_GINIT(swow),             /* globals ctor */
    PHP_GSHUTDOWN(swow),         /* globals dtor */
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
