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
#include "swow_errno.h"
#include "swow_log.h"
#include "swow_debug.h"
#include "swow_util.h"
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
#include "swow_watchdog.h"
#include "swow_http.h"
#include "swow_websocket.h"
#include "swow_proc_open.h"
#include "swow_curl.h"

#include "cat_api.h"

#include "SAPI.h"

#include "zend_smart_str.h"

#include "ext/standard/info.h"

#ifdef SWOW_DISABLE_SESSION
# if HAVE_PHP_SESSION && !defined(COMPILE_DL_SESSION)
#  define SWOW_HAVE_SESSION
#  include "ext/session/php_session.h"
# endif
#endif

#if (defined(HAVE_PCRE) || defined(HAVE_BUNDLED_PCRE)) && !defined(COMPILE_DL_PCRE) && \
     defined(__MACH__) && !defined(CAT_DEBUG)
# include "ext/pcre/php_pcre.h"
#endif

#ifdef CAT_HAVE_CURL
# include <curl/curl.h>
#endif

SWOW_API zend_class_entry *swow_ce;

SWOW_API zend_class_entry *swow_module_ce;

ZEND_DECLARE_MODULE_GLOBALS(swow)

SWOW_API swow_nts_globals_t swow_nts_globals;

typedef zend_result (*swow_init_function_t)(INIT_FUNC_ARGS);
typedef zend_result (*swow_shutdown_function_t)(INIT_FUNC_ARGS);
typedef zend_result (*swow_close_function_t)(void);

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
        swow_errno_module_init,
        swow_log_module_init,
        swow_exceptions_module_init,
        swow_debug_module_init,
        swow_util_module_init,
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
        swow_watchdog_module_init,
        swow_http_module_init,
        swow_websocket_module_init,
#ifdef CAT_OS_WAIT
        swow_proc_open_module_init,
#endif
#ifdef CAT_HAVE_CURL
        swow_curl_module_init,
#endif
    };

    for (i = 0; i < CAT_ARRAY_SIZE(minit_functions); i++) {
        if (minit_functions[i](INIT_FUNC_ARGS_PASSTHRU) != SUCCESS) {
            return FAILURE;
        }
    }

    SWOW_NTS_G(cli) = strcmp(sapi_module.name, "cli") != 0 &&
                      strcmp(sapi_module.name, "phpdbg") != 0;

    return SUCCESS;
}
/* }}} */

#define SWOW_COMPATIBLE_WITH_DL 1 // TODO: fix it in php-src
#ifdef SWOW_COMPATIBLE_WITH_DL
static int swow_clean_module_function(zval *el, void *arg)
{
    zend_function *fe = (zend_function *) Z_PTR_P(el);
    int module_number = *(int *) arg;
    if (fe->common.type == ZEND_INTERNAL_FUNCTION && fe->internal_function.module->module_number == module_number) {
        return ZEND_HASH_APPLY_REMOVE;
    } else {
        return ZEND_HASH_APPLY_KEEP;
    }
}

static void swow_clean_module_functions(int module_number)
{
    zend_hash_apply_with_argument(CG(function_table), swow_clean_module_function, (void *) &module_number);
}
#endif

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(swow)
{
    static const swow_shutdown_function_t mshutdown_functions[] = {
#ifdef CAT_HAVE_CURL
        swow_curl_module_shutdown,
#endif
        swow_stream_module_shutdown,
        swow_module_shutdown,
    };

    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(mshutdown_functions); i++) {
        if (mshutdown_functions[i](SHUTDOWN_FUNC_ARGS_PASSTHRU) != SUCCESS) {
            return FAILURE;
        }
    }

#ifdef SWOW_COMPATIBLE_WITH_DL
    if (type == MODULE_TEMPORARY) {
        swow_clean_module_functions(module_number);
    }
#endif

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
        swow_debug_runtime_init,
        swow_coroutine_runtime_init,
        swow_event_runtime_init,
        swow_socket_runtime_init,
        swow_stream_runtime_init,
        swow_watchdog_runtime_init,
#ifdef CAT_OS_WAIT
        swow_proc_open_runtime_init,
#endif
#ifdef CAT_HAVE_CURL
        swow_curl_runtime_init,
#endif
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
#ifdef CAT_HAVE_CURL
        swow_curl_runtime_shutdown,
#endif
#ifdef CAT_OS_WAIT
        swow_proc_open_runtime_shutdown,
#endif
        swow_watchdog_runtime_shutdown,
        swow_stream_runtime_shutdown,
        swow_event_runtime_shutdown,
        swow_coroutine_runtime_shutdown,
        swow_debug_runtime_shutdown,
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

    /* Notice: some PHP resources still have not been released
     * (e.g. static vars or set global vars on __destruct or circular references),
     * so we must re-create C coroutine env to finish them
     * In other words, now we are in pure C coroutine env instead of PHP runtime */

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

static zend_result swow_post_deactivate(void)
{
    do {
        cat_bool_t ret = cat_true;

        ret = cat_stop();

        ret = cat_runtime_shutdown_all() && ret;

        if (!ret) {
            return FAILURE;
        }
    } while (0);

    static const swow_close_function_t rclose_functions[] = {
        swow_event_runtime_close,
    };

    size_t i = 0;
    for (; i < CAT_ARRAY_SIZE(rclose_functions); i++) {
        if (rclose_functions[i]() != SUCCESS) {
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
# define SWOW_VERSION_SUFFIX "NTS"
#else
# define SWOW_VERSION_SUFFIX "ZTS"
#endif

#ifdef CAT_DEBUG
# define SWOW_VERSION_SUFFIX_EXT " DEBUG"
#else
# define SWOW_VERSION_SUFFIX_EXT ""
#endif

#ifdef CAT_COROUTINE_USE_UCONTEXT
# define SWOW_COROUTINE_CONTEXT_TYPE "ucontext"
#else
# define SWOW_COROUTINE_CONTEXT_TYPE "boost-context"
#endif

#ifndef SWOW_GIT_VERSION
# define SWOW_GIT_VERSION ""
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
# ifdef CAT_HAVE_OPENSSL
    smart_str_append_printf(&str, "%s, ", cat_ssl_version());
# endif
# ifdef CAT_HAVE_CURL
    curl_version_info_data *curl_vid = curl_version_info(CURLVERSION_NOW);
    smart_str_append_printf(&str, "cURL %s, ", curl_vid->version);
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_isBuiltWith, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, lib, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow, isBuiltWith)
{
    zend_string *lib;
    zend_bool ret = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(lib)
    ZEND_PARSE_PARAMETERS_END();

    if (0) { }
#ifdef CAT_DEBUG
    else if (zend_string_equals_literal_ci(lib, "debug")) {
        ret = 1;
    }
#endif
#ifdef CAT_HAVE_ASAN
    else if (zend_string_equals_literal_ci(lib, "asan")) {
        ret = 1;
    }
#endif
#ifdef CAT_HAVE_MSAN
    else if (zend_string_equals_literal_ci(lib, "msan")) {
        ret = 1;
    }
#endif
#ifdef CAT_HAVE_UBSAN
    else if (zend_string_equals_literal_ci(lib, "ubsan")) {
        ret = 1;
    }
#endif
#ifdef CAT_HAVE_OPENSSL
    else if (zend_string_equals_literal_ci(lib, "ssl") ||
             zend_string_equals_literal_ci(lib, "openssl")) {
        ret = 1;
    }
#endif
#ifdef CAT_HAVE_CURL
    else if (zend_string_equals_literal_ci(lib, "curl")) {
        ret = 1;
    }
#endif

    RETURN_BOOL(ret);
}

static const zend_function_entry swow_methods[] = {
    PHP_ME(Swow, isBuiltWith, arginfo_class_Swow_isBuiltWith, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

zend_result swow_module_init(INIT_FUNC_ARGS)
{
#ifdef SWOW_DISABLE_SESSION
    /* disable some conflict internal modules */
    do {
        SWOW_MODULES_CHECK_PRE_START() {
            "session"
        } SWOW_MODULES_CHECK_PRE_END();
# ifdef SWOW_HAVE_SESSION
        PS(auto_start) = 0;
# endif
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
        "Swow", NULL, swow_methods,
        NULL, NULL, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );
    /* Version constants (TODO: remove type cast if we no longger support PHP 7.x) */
#define SWOW_VERSION_MAP(XX) \
    XX(MAJOR_VERSION, long, zend_long) \
    XX(MINOR_VERSION, long, zend_long) \
    XX(RELEASE_VERSION, long, zend_long) \
    XX(EXTRA_VERSION, string, char *) \
    XX(VERSION, string, char *) \
    XX(VERSION_ID, long, zend_long)
#define SWOW_VERSION_GEN(name, type, cast) \
    zend_declare_class_constant_##type(swow_ce, ZEND_STRL(#name), (cast) SWOW_##name);
    SWOW_VERSION_MAP(SWOW_VERSION_GEN)
#undef SWOW_VERSION_GEN

    /* Module class with constants */
    swow_module_ce = swow_register_internal_class(
        "Swow\\Module", NULL, NULL,
        NULL, NULL, cat_false, cat_false,
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

zend_result swow_module_shutdown(INIT_FUNC_ARGS)
{
    cat_module_shutdown();

    return SUCCESS;
}

zend_result swow_runtime_init(INIT_FUNC_ARGS)
{
    /* See: https://bugs.php.net/bug.php?id=79064 */
    // EG(full_tables_cleanup) = cat_true; // not needed for now

    cat_runtime_init();

    return SUCCESS;
}

zend_result swow_runtime_shutdown(INIT_FUNC_ARGS)
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
    swow_post_deactivate, /* post deactivate */
    STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_SWOW
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(swow)
#endif
