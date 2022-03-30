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

#ifndef SWOW_WRAPPER_H
#define SWOW_WRAPPER_H
#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__cplusplus) && !defined(_MSC_VER)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
#include "php.h"

#include "zend_API.h"
#include "zend_builtin_functions.h"
#include "zend_closures.h"
#include "zend_interfaces.h"
#include "zend_smart_str.h"
#include "zend_variables.h"

#include "ext/standard/php_var.h"
#include "ext/standard/php_array.h"
#if !defined(__cplusplus) && !defined(_MSC_VER)
#pragma GCC diagnostic pop
#endif

void swow_wrapper_init(void);
void swow_wrapper_shutdown(void);

/* PHP 8.1 compatibility macro {{{*/
#if PHP_VERSION_ID < 80100
SWOW_API zend_string* ZEND_FASTCALL zend_ulong_to_str(zend_ulong num);
#endif
/* }}} */

/* ZTS */

#ifdef ZTS
#ifdef TSRMG_FAST
#ifdef ZEND_ENABLE_STATIC_TSRMLS_CACHE
#define SWOW_TSRMG_FAST_BULK TSRMG_FAST_BULK_STATIC
#else
#define SWOW_TSRMG_FAST_BULK TSRMG_FAST_BULK
#endif
#endif
#ifdef ZEND_ENABLE_STATIC_TSRMLS_CACHE
#define SWOW_TSRMG_BULK TSRMG_BULK_STATIC
#else
#define SWOW_TSRMG_BULK TSRMG_BULK
#endif
#endif

/* the way to get zend globals ptr */

#ifdef ZTS
#define SWOW_GLOBALS_PTR(name)         SWOW_TSRMG_BULK(name##_id, zend_##name *)
#ifdef TSRMG_FAST
#define SWOW_GLOBALS_FAST_PTR(name)    SWOW_TSRMG_FAST_BULK(name##_offset, zend_##name *)
#else
#define SWOW_GLOBALS_FAST_PTR(name)    SWOW_GLOBALS_PTR(name)
#endif
#else
#define SWOW_GLOBALS_PTR(name)         (&name)
#define SWOW_GLOBALS_FAST_PTR(name)    (&name)
#endif

/* memory */

#define SWOW_IS_OUT_OF_MEMORY() (CG(unclean_shutdown) && PG(last_error_type) == E_ERROR && (size_t) PG(memory_limit) < zend_memory_usage(1))

/* modules */

#define SWOW_MODULE_NAME "swow"

#define SWOW_MODULES_CHECK_PRE_START() do { \
    const char *_pre_modules[] =

#define SWOW_MODULES_CHECK_PRE_END() ; \
    size_t _pre_modules_length = CAT_ARRAY_SIZE(_pre_modules); \
    cat_bool_t _loaded = cat_false; \
    zend_string *_module_name; \
    ZEND_HASH_FOREACH_STR_KEY(&module_registry, _module_name) { \
        if (memcmp(ZSTR_VAL(_module_name), ZEND_STRL(SWOW_MODULE_NAME)) == 0) { \
            _loaded = cat_true; \
        } else { \
            size_t _n = 0; \
            for (; _n < _pre_modules_length; _n++) { \
                const char *_pre_module = _pre_modules[_n]; \
                if (memcmp(ZSTR_VAL(_module_name), _pre_module, strlen(_pre_module)) == 0) { \
                    if (_loaded) { \
                        CAT_CORE_ERROR(CORE, "Module %s must be loaded before " SWOW_MODULE_NAME, _pre_module); \
                    } \
                } \
            } \
        } \
    } ZEND_HASH_FOREACH_END(); \
} while (0)

/* array */

#ifndef add_assoc_string_fast
static zend_always_inline void add_next_index_string_fast(zval *arg, const char *str)
{
    zval tmp;
    ZVAL_STRING_FAST(&tmp, str);
    add_next_index_zval(arg, &tmp);
}

static zend_always_inline void add_next_index_stringl_fast(zval *arg, const char *str, size_t length)
{
    zval tmp;
    ZVAL_STRINGL_FAST(&tmp, str, length);
    add_next_index_zval(arg, &tmp);
}

#define add_assoc_string_fast(arg, key, str) add_assoc_string_fast_ex(arg, key, strlen(key), str)

static zend_always_inline void add_assoc_string_fast_ex(zval *arg, const char *key, size_t key_len, const char *str)
{
    zval tmp;
    ZVAL_STRING_FAST(&tmp, str);
    add_assoc_zval_ex(arg, key, key_len, &tmp);
}

#define add_assoc_stringl_fast(arg, key, str, length) add_assoc_stringl_ex(arg, key, strlen(key), str, length)

static zend_always_inline void add_assoc_stringl_fast_ex(zval *arg, const char *key, size_t key_len, const char *str, size_t length)
{
    zval tmp;
    ZVAL_STRINGL_FAST(&tmp, str, length);
    add_assoc_zval_ex(arg, key, key_len, &tmp);
}
#endif

#if PHP_VERSION_ID < 80100
static zend_always_inline zend_result add_next_index_array(zval *arg, zend_array *arr)
{
    zval tmp;
    ZVAL_ARR(&tmp, arr);
    return zend_hash_next_index_insert(Z_ARRVAL_P(arg), &tmp) ? SUCCESS : FAILURE;
}

#define add_assoc_array(arg, key, arr) add_assoc_array_ex(arg, key, strlen(key), arr)

static zend_always_inline void add_assoc_array_ex(zval *arg, const char *key, size_t key_len, zend_array *arr)
{
    zval tmp;
    ZVAL_ARR(&tmp, arr);
    zend_symtable_str_update(Z_ARRVAL_P(arg), key, key_len, &tmp);
}

static zend_always_inline zend_result add_next_index_object(zval *arg, zend_object *obj)
{
    zval tmp;
    ZVAL_OBJ(&tmp, obj);
    return zend_hash_next_index_insert(Z_ARRVAL_P(arg), &tmp) ? SUCCESS : FAILURE;
}

#define add_assoc_object(arg, key, obj) add_assoc_object_ex(arg, key, strlen(key), obj)

static zend_always_inline void add_assoc_object_ex(zval *arg, const char *key, size_t key_len, zend_object *obj)
{
    zval tmp;
    ZVAL_OBJ(&tmp, obj);
    zend_symtable_str_update(Z_ARRVAL_P(arg), key, key_len, &tmp);
}
#endif /* PHP_VERSION_ID < 80100 (add_array, add_object) */

/* class */

typedef zend_object *(*swow_object_create_t)(zend_class_entry *class_type);
typedef zend_object_dtor_obj_t swow_object_dtor_t;
typedef zend_object_free_obj_t swow_object_free_t;
typedef int (*swow_interface_gets_implemented_t)(zend_class_entry *iface, zend_class_entry *class_type);

SWOW_API zend_class_entry *swow_register_internal_class(
    const char *name, zend_class_entry *parent_ce, const zend_function_entry methods[],
    zend_object_handlers *handlers, const zend_object_handlers *parent_handlers,
    const cat_bool_t cloneable, const cat_bool_t serializable,
    const swow_object_create_t create_object, const swow_object_free_t free_object, const int offset
);

SWOW_API zend_class_entry *swow_register_internal_interface(
    const char *name,
    const zend_function_entry methods[],
    swow_interface_gets_implemented_t interface_gets_implemented
);

/* object */

SWOW_API zend_object *swow_create_object_deny(zend_class_entry *ce);
SWOW_API zend_object *swow_custom_object_clone(zend_object *object);

#define swow_object_alloc(type, ce, handlers) ((type *) swow_object_alloc_ex(sizeof(type), ce, &handlers))

static zend_always_inline void* swow_object_alloc_ex(size_t size, zend_class_entry *ce, zend_object_handlers *handlers)
{
    void *ptr = emalloc(size + zend_object_properties_size(ce));
    zend_object *object = (zend_object *) (((char *) ptr) + handlers->offset);

    zend_object_std_init(object, ce);
    object_properties_init(object, ce);
    object->handlers = handlers;

    return ptr;
}

static zend_always_inline zend_object* swow_object_create(zend_class_entry *ce)
{
    return ce->create_object(ce);
}

#ifndef Z_OBJCENAME_P
#define Z_OBJCENAME_P(zobject) ZSTR_VAL(Z_OBJCE_P(zobject)->name)
#endif

#ifndef ZEND_THIS_NAME
#define ZEND_THIS_NAME Z_OBJCENAME_P(ZEND_THIS)
#endif

/* function/method */

#ifndef PHP_FENTRY
#define PHP_FENTRY                              ZEND_FENTRY
#endif

#define ZEND_NAMED_FUNCTION_EX(name, ...)       void ZEND_FASTCALL name(INTERNAL_FUNCTION_PARAMETERS, ##__VA_ARGS__)
#define ZEND_FUNCTION_EX(name, ...)             ZEND_NAMED_FUNCTION_EX(zif_##name, ##__VA_ARGS__)
#define ZEND_METHOD_EX(classname, name, ...)    ZEND_NAMED_FUNCTION_EX(zim_##classname##_##name, ##__VA_ARGS__)
#define PHP_FUNCTION_EX                         ZEND_FUNCTION_EX
#define PHP_METHOD_EX                           ZEND_METHOD_EX

#define ZEND_FUNCTION_CALL(name, ...)           PHP_FN(name)(INTERNAL_FUNCTION_PARAM_PASSTHRU, ##__VA_ARGS__)
#define ZEND_METHOD_CALL(classname, name, ...)  PHP_MN(classname##_##name)(INTERNAL_FUNCTION_PARAM_PASSTHRU, ##__VA_ARGS__)
#define PHP_FUNCTION_CALL                       ZEND_FUNCTION_CALL
#define PHP_METHOD_CALL                         ZEND_METHOD_CALL

#define RETVAL_STATIC() do { \
    RETVAL_OBJ(swow_object_create(zend_get_executed_scope())); \
} while (0)

#define RETURN_STATIC() do { \
    RETVAL_STATIC(); \
    return; \
} while (0)

#define RETVAL_THIS() do { \
    ZVAL_COPY(return_value, ZEND_THIS); \
} while (0)

#define RETURN_THIS() do { \
    RETVAL_THIS(); \
    return; \
} while (0)

#define RETVAL_THIS_PROPERTY(name) do { \
    zval *zproperty, zretval; \
    zproperty = zend_read_property(Z_OBJCE_P(ZEND_THIS), Z_OBJ_P(ZEND_THIS), ZEND_STRL(name), 1, &zretval); \
    ZVAL_DEREF(zproperty); \
    ZVAL_COPY(return_value, zproperty); \
} while (0)

#define RETURN_THIS_PROPERTY(name) do { \
    RETVAL_THIS_PROPERTY(name); \
    return; \
} while (0)

#define RETURN_DEBUG_INFO_WITH_PROPERTIES(zdebug_info) do { \
    php_array_merge(Z_ARR_P(zdebug_info), Z_OBJ_P(ZEND_THIS)->handlers->get_properties(Z_OBJ_P(ZEND_THIS))); \
    RETURN_ZVAL(zdebug_info, 0, 0); \
} while (0)

#define ZEND_ASSERT_HAS_EXCEPTION() do { \
    ZEND_ASSERT(EG(exception)); \
    (void) return_value; /* suppress unused return_value */ \
} while (0)

#undef RETURN_THROWS
#define RETURN_THROWS() do { \
    ZEND_ASSERT_HAS_EXCEPTION(); \
    return; \
} while (0)

/* function */

typedef struct swow_fcall_s {
    zend_fcall_info info;
    zend_fcall_info_cache cache;
} swow_fcall_info_t;

typedef struct swow_fcall_storage_s {
    zval zcallable;
    zend_fcall_info_cache fcc;
} swow_fcall_storage_t;

SWOW_API zend_bool swow_fcall_storage_is_available(const swow_fcall_storage_t *fcall);
SWOW_API zend_bool swow_fcall_storage_create(swow_fcall_storage_t *fcall, zval *zcallable);
SWOW_API void swow_fcall_storage_release(swow_fcall_storage_t *fcall);

#define SWOW_PARAM_FCALL(fcall) do { \
    zend_fcall_info _fci; \
    Z_PARAM_FUNC(_fci, fcall.fcc) \
    fcall.zcallable = *_arg; \
} while (0);

/* function caller */

SWOW_API int swow_call_function_anyway(zend_fcall_info *fci, zend_fcall_info_cache *fci_cache);

/* method caller */

#define swow_call_method_with_0_params(zobject, object_ce, fn_ptr_ptr, fn_name, retval) \
        zend_call_method_with_0_params(Z_OBJ_P(zobject), object_ce, fn_ptr_ptr, fn_name, retval)

#define swow_call_method_with_1_params(zobject, object_ce, fn_ptr_ptr, fn_name, retval, v1) \
        zend_call_method_with_1_params(Z_OBJ_P(zobject), object_ce, fn_ptr_ptr, fn_name, retval, v1)

#define swow_call_method_with_2_params(zobject, object_ce, fn_ptr_ptr, fn_name, retval, v1, v2) \
        zend_call_method_with_2_params(Z_OBJ_P(zobject), object_ce, fn_ptr_ptr, fn_name, retval, v1, v2)

/* output globals */

#if PHP_VERSION_ID >= 80100
#define SWOW_OUTPUT_GLOBALS_START_FILENAME OG(output_start_filename)
#else
#define SWOW_OUTPUT_GLOBALS_START_FILENAME OG(output_start_filename_str)
#endif
#define SWOW_OUTPUT_GLOBALS_START_LINENO OG(output_start_lineno)

#ifdef SWOW_OUTPUT_GLOBALS_START_FILENAME
#define SWOW_OUTPUT_GLOBALS_MODIFY_START() do { \
    zend_string *output_start_filename = SWOW_OUTPUT_GLOBALS_START_FILENAME; \
    int output_start_lineno = SWOW_OUTPUT_GLOBALS_START_LINENO; \

#define SWOW_OUTPUT_GLOBALS_MODIFY_END() \
    SWOW_OUTPUT_GLOBALS_START_FILENAME = output_start_filename; \
    SWOW_OUTPUT_GLOBALS_START_LINENO = output_start_lineno; \
} while (0)
#else
#define SWOW_OUTPUT_GLOBALS_MODIFY_START()
#define SWOW_OUTPUT_GLOBALS_MODIFY_END()
#endif

SWOW_API void swow_output_globals_init(void);
SWOW_API void swow_output_globals_shutdown(void);

static zend_always_inline void swow_output_globals_fast_shutdown(void)
{
    if (UNEXPECTED(OG(handlers).elements != NULL)) {
        swow_output_globals_shutdown();
    }
}

#define SWOW_OB_START(output) do { \
    zval zoutput; \
    zend_string **output_ptr = &output; \
    zend_result ret; \
    php_output_start_user(NULL, 0, PHP_OUTPUT_HANDLER_STDFLAGS); \

#define SWOW_OB_END() \
    ret = php_output_get_contents(&zoutput); \
    php_output_discard(); \
    *output_ptr = ret == SUCCESS ? Z_STR(zoutput) : NULL; \
} while (0);

#if PHP_VERSION_ID >= 80200
#define zend_forbid_dynamic_call(x) zend_forbid_dynamic_call()
#endif

#ifdef __cplusplus
}
#endif
#endif /* SWOW_H */
