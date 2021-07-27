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

/* PHP 7.3 compatibility macro {{{*/
#ifndef GC_ADD_FLAGS
#define GC_ADD_FLAGS(p, flags) do { \
    GC_FLAGS(p) |= (flags); \
} while (0)
#endif

#ifndef GC_DEL_FLAGS
#define GC_DEL_FLAGS(p, flags) do { \
    GC_FLAGS(p) &= ~(flags); \
} while (0)
#endif

#ifndef GC_SET_REFCOUNT
#define GC_SET_REFCOUNT(p, rc) do { \
    GC_REFCOUNT(p) = rc; \
} while (0)
#endif

#ifndef GC_ADDREF
#define GC_ADDREF(ref) (++GC_REFCOUNT(ref))
#define GC_DELREF(ref) (--GC_REFCOUNT(ref))
#endif

#ifndef GC_IS_RECURSIVE
#define GC_IS_RECURSIVE(p) \
    (ZEND_HASH_GET_APPLY_COUNT(p) >= 1)
#define GC_PROTECT_RECURSION(p) \
    ZEND_HASH_INC_APPLY_COUNT(p)
#define GC_UNPROTECT_RECURSION(p) \
    ZEND_HASH_DEC_APPLY_COUNT(p)
#endif

#ifndef ZEND_CLOSURE_OBJECT
#define ZEND_CLOSURE_OBJECT(func) ((zend_object *) func->op_array.prototype)
#endif

#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
        ZEND_PARSE_PARAMETERS_START(0, 0) \
        ZEND_PARSE_PARAMETERS_END()
#endif
/* }}} */

/* PHP 7.4 compatibility macro {{{*/
#ifndef ZEND_COMPILE_EXTENDED_INFO
#define ZEND_COMPILE_EXTENDED_INFO ZEND_COMPILE_EXTENDED_STMT
#endif

#ifndef ZVAL_EMPTY_ARRAY
#define ZVAL_EMPTY_ARRAY(zval) (array_init((zval)))
#endif
#ifndef RETVAL_EMPTY_ARRAY
#define RETVAL_EMPTY_ARRAY()   ZVAL_EMPTY_ARRAY(return_value)
#endif
#ifndef RETURN_EMPTY_ARRAY
#define RETURN_EMPTY_ARRAY()   do { RETVAL_EMPTY_ARRAY(); return; } while (0)
#endif

#ifndef ZEND_THIS
#define ZEND_THIS (&EX(This))
#endif

#ifndef ZEND_THIS_OBJECT
#define ZEND_THIS_OBJECT Z_OBJ_P(ZEND_THIS)
#endif

#ifndef E_FATAL_ERRORS
#define E_FATAL_ERRORS (E_ERROR | E_CORE_ERROR | E_COMPILE_ERROR | E_USER_ERROR | E_RECOVERABLE_ERROR | E_PARSE)
#endif
/* }}} */

/* PHP 8 compatibility macro {{{*/
#if PHP_VERSION_ID < 80000
/* type is zval in PHP7, is object in PHP8 */
#define zend7_object      zval
/* zval to object only in PHP7 */
#define Z7_OBJ_P(zobject) Z_OBJ_P(zobject)
/* zval to object only in PHP8 */
#define Z8_OBJ_P(zobject) zobject
/* get this as zval in PHP7, as object in PHP8 */
#define ZEND7_THIS        ZEND_THIS
/* create zval in PHP7, do nothing in PHP8 */
#define ZVAL7_ALLOC_OBJECT(object) \
        zval z##object; ZVAL_OBJ(&z##object, object)
/* use zval in PHP7, object in PHP8 */
#define ZVAL7_OBJECT(object) &z##object
#else
#define zend7_object      zend_object
#define Z7_OBJ_P(object)  object
#define Z8_OBJ_P(zobject) Z_OBJ_P(zobject)
#define ZEND7_THIS        Z_OBJ_P(ZEND_THIS)
#define ZVAL7_ALLOC_OBJECT(object)
#define ZVAL7_OBJECT(object) object
#endif

#ifndef IS_MIXED
#define IS_MIXED 0 /* TODO: it works, but... */
#endif

/* TODO: maybe one day we can return static here (require PHP >= 8.0) */
#define ZEND_BEGIN_ARG_WITH_RETURN_THIS_INFO_EX(name, required_num_args) \
        ZEND_BEGIN_ARG_INFO_EX(name, 0, ZEND_RETURN_VALUE, required_num_args)

#define ZEND_BEGIN_ARG_WITH_RETURN_THIS_OR_NULL_INFO_EX(name, required_num_args) \
        ZEND_BEGIN_ARG_INFO_EX(name, 0, ZEND_RETURN_VALUE, required_num_args)

#ifndef ZEND_ARG_INFO_WITH_DEFAULT_VALUE
#define ZEND_ARG_INFO_WITH_DEFAULT_VALUE(pass_by_ref, name, default_value) \
        ZEND_ARG_INFO(pass_by_ref, name)
#endif

#ifndef ZEND_ARG_TYPE_MASK
#define ZEND_ARG_TYPE_MASK(pass_by_ref, name, type_mask, default_value) \
        ZEND_ARG_INFO_WITH_DEFAULT_VALUE(pass_by_ref, name, default_value)
#endif

#ifndef ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX
#define ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(name, return_reference, required_num_args, type) \
        ZEND_BEGIN_ARG_INFO_EX(name, unused, return_reference, required_num_args)
#endif

#ifndef ZEND_BEGIN_ARG_WITH_RETURN_OBJ_TYPE_MASK_EX
#define ZEND_BEGIN_ARG_WITH_RETURN_OBJ_TYPE_MASK_EX(name, return_reference, required_num_args, class_name, type) \
        ZEND_BEGIN_ARG_INFO_EX(name, unused, return_reference, required_num_args)
#endif

#ifndef ZEND_ARG_INFO_WITH_DEFAULT_VALUE
#define ZEND_ARG_INFO_WITH_DEFAULT_VALUE(pass_by_ref, name, default_value) \
        ZEND_ARG_INFO(pass_by_ref, name)
#endif

#ifndef ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE
#define ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(pass_by_ref, name, type_hint, allow_null, default_value) \
        ZEND_ARG_TYPE_INFO(pass_by_ref, name, type_hint, allow_null)
#endif

#ifndef ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE
#define ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE(pass_by_ref, name, classname, allow_null, default_value) \
        ZEND_ARG_OBJ_INFO(pass_by_ref, name, classname, allow_null)
#endif

#ifndef Z_PARAM_LONG_OR_NULL
#define Z_PARAM_LONG_OR_NULL(dest, is_null) \
        Z_PARAM_LONG_EX(dest, is_null, 1, 0)

static zend_always_inline int zend8_parse_arg_long(zval *arg, zend_long *dest, zend_bool *is_null, int check_null)
{
    return zend_parse_arg_long(arg, dest, is_null, check_null, 0);
}

#define zend_parse_arg_long(arg, dest, is_null, check_null, ...) zend8_parse_arg_long(arg, dest, is_null, check_null)
#endif

#ifndef RETURN_THROWS
extern SWOW_API zend_class_entry *zend_ce_value_error;
SWOW_API ZEND_COLD void zend_value_error(const char *format, ...) ZEND_ATTRIBUTE_FORMAT(printf, 1, 2);
#else
#undef RETURN_THROWS
#endif
#define RETURN_THROWS_ASSERTION() do { ZEND_ASSERT(EG(exception)); (void) return_value; } while (0)
#define RETURN_THROWS()  do { RETURN_THROWS_ASSERTION(); return; } while (0)

#if PHP_VERSION_ID < 80000
/* see: https://github.com/php/php-src/pull/6002 */
typedef int zend_result;
#endif

#if PHP_VERSION_ID < 80000
SWOW_API zend_string *zend_string_concat2(
    const char *str1, size_t str1_len,
    const char *str2, size_t str2_len);
SWOW_API zend_string *zend_string_concat3(
    const char *str1, size_t str1_len,
    const char *str2, size_t str2_len,
    const char *str3, size_t str3_len);
SWOW_API zend_string *zend_create_member_string(zend_string *class_name, zend_string *member_name);
SWOW_API zend_string *get_active_function_or_method_name(void);
SWOW_API zend_string *get_function_or_method_name(const zend_function *func);
SWOW_API const char *get_active_function_arg_name(uint32_t arg_num);
SWOW_API const char *get_function_arg_name(const zend_function *func, uint32_t arg_num);

SWOW_API ZEND_COLD void ZEND_FASTCALL zend_argument_error(zend_class_entry *error_ce, uint32_t arg_num, const char *format, ...);
SWOW_API ZEND_COLD void ZEND_FASTCALL zend_argument_type_error(uint32_t arg_num, const char *format, ...);
SWOW_API ZEND_COLD void ZEND_FASTCALL zend_argument_value_error(uint32_t arg_num, const char *format, ...);
#endif

/* see: https://github.com/php/php-src/commit/57670c6769039a2e7b6379a68f04ecc9cb127101 */
#if PHP_VERSION_ID < 80000
#ifndef CAT_DEBUG
#define ZEND_ACC_HAS_TYPE_HINTS_DENY 1 /* Improve the performance on the production env */

#if 0
#ifdef ZEND_ARG_OBJ_INFO
#undef ZEND_ARG_OBJ_INFO
#endif
#define ZEND_ARG_OBJ_INFO(pass_by_ref, name, classname, allow_null) \
        ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE(pass_by_ref, name, classname, allow_null, NULL)
#ifdef ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE
#undef ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE
#endif
#define ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE(pass_by_ref, name, classname, allow_null, default_value) \
        ZEND_ARG_TYPE_MASK(pass_by_ref, name, MAY_BE_OBJECT | (allow_null ? MAY_BE_NULL : 0), default_value)
#endif
#endif
#endif

#define ZEND_GET_GC_PARAMATERS                      zend7_object *object, zval **gc_data, int *gc_count
#define ZEND_GET_GC_RETURN_ZVAL(_zdata)             do { *gc_data = _zdata; *gc_count = 1; return zend_std_get_properties(object); } while (0)
#define ZEND_GET_GC_RETURN_EMPTY()                  do { *gc_data = NULL; *gc_count = 0; return zend_std_get_properties(object); } while (0)
#if PHP_VERSION_ID < 80000 || defined(CAT_IDE_HELPER)
#define ZEND_NO_GC_BUFFER 1
#endif
#ifdef ZEND_NO_GC_BUFFER
#define ZEND_GET_GC_BUFFER_DECLARE                  zval *zgc_buffer;
#define ZEND_GET_GC_BUFFER_INIT(_object)            (_object)->zgc_buffer = NULL
#define ZEND_GET_GC_BUFFER_CREATE(_object, _length) zval *zgc_buffer = (_object)->zgc_buffer; int _##zgc_buffer##_index = 0; \
                                                    (_object)->zgc_buffer = zgc_buffer = safe_erealloc(zgc_buffer, _length, sizeof(zval), 0)
#define ZEND_GET_GC_BUFFER_ADD(_zv)                 ZVAL_COPY_VALUE(&zgc_buffer[_##zgc_buffer##_index++], _zv)
#define ZEND_GET_GC_BUFFER_DONE()                   do { *gc_data = zgc_buffer; *gc_count = _##zgc_buffer##_index; return zend_std_get_properties(object); } while (0)
#define ZEND_GET_GC_BUFFER_FREE(_object)            do { if ((_object)->zgc_buffer != NULL) { efree((_object)->zgc_buffer); } } while (0)
#else
#define ZEND_GET_GC_BUFFER_DECLARE
#define ZEND_GET_GC_BUFFER_INIT(_object)
#define ZEND_GET_GC_BUFFER_CREATE(_object, _length) zend_get_gc_buffer *zgc_buffer = zend_get_gc_buffer_create()
#define ZEND_GET_GC_BUFFER_ADD(_zv)                 zend_get_gc_buffer_add_zval(zgc_buffer, _zv)
#define ZEND_GET_GC_BUFFER_DONE()                   do { zend_get_gc_buffer_use(zgc_buffer, gc_data, gc_count); return zend_std_get_properties(object); } while (0)
#define ZEND_GET_GC_BUFFER_FREE(_object)
#endif
/* }}} */

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
                        cat_core_error(CORE, "Module %s must be loaded before " SWOW_MODULE_NAME, _pre_module); \
                    } \
                } \
            } \
        } \
    } ZEND_HASH_FOREACH_END(); \
} while (0)

/* string */

#ifndef ZVAL_CHAR /* see: https://github.com/php/php-src/pull/5684 */
static zend_always_inline zend_string *zend_string_init_fast(const char *str, size_t len)
{
    if (len > 1) {
        return zend_string_init(str, len, 0);
    } else if (len == 0) {
        return zend_empty_string;
    } else /* if (len == 1) */ {
        return ZSTR_CHAR((zend_uchar) *str);
    }
}

#define ZVAL_CHAR(z, c)  do {                   \
        char _c = (c);                          \
        ZVAL_INTERNED_STR(z, ZSTR_CHAR((zend_uchar) _c));   \
    } while (0)

#define ZVAL_STRINGL_FAST(z, s, l) do {         \
        ZVAL_STR(z, zend_string_init_fast(s, l));   \
    } while (0)

#define ZVAL_STRING_FAST(z, s) do {             \
        const char *_s = (s);                   \
        ZVAL_STRINGL_FAST(z, _s, strlen(_s));   \
    } while (0)

#define RETVAL_STRING_FAST(s)           ZVAL_STRING_FAST(return_value, s)
#define RETVAL_STRINGL_FAST(s, l)       ZVAL_STRINGL_FAST(return_value, s, l)
#define RETVAL_CHAR(c)                  ZVAL_CHAR(return_value, c)

#define RETURN_STRING_FAST(s)           do { RETVAL_STRING_FAST(s); return; } while (0)
#define RETURN_STRINGL_FAST(s, l)       do { RETVAL_STRINGL_FAST(s, l); return; } while (0)
#define RETURN_CHAR(c)                  do { RETVAL_CHAR(c); return; } while (0)
#endif

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

#ifndef add_assoc_array
static zend_always_inline void add_next_index_array(zval *arg, zend_array *array)
{
    zval tmp;
    ZVAL_ARR(&tmp, array);
    add_next_index_zval(arg, &tmp);
}

#define add_assoc_array(arg, key, array) add_assoc_array_ex(arg, key, strlen(key), array)

static zend_always_inline void add_assoc_array_ex(zval *arg, const char *key, size_t key_len, zend_array *array)
{
    zval tmp;
    ZVAL_ARR(&tmp, array);
    add_assoc_zval_ex(arg, key, key_len, &tmp);
}
#endif /* add_assoc_array */

#ifndef add_assoc_object
static zend_always_inline void add_next_index_object(zval *arg, zend_object *object)
{
    zval tmp;
    ZVAL_OBJ(&tmp, object);
    add_next_index_zval(arg, &tmp);
}

#define add_assoc_object(arg, key, object) add_assoc_object_ex(arg, key, strlen(key), object)

static zend_always_inline void add_assoc_object_ex(zval *arg, const char *key, size_t key_len, zend_object *object)
{
    zval tmp;
    ZVAL_OBJ(&tmp, object);
    add_assoc_zval_ex(arg, key, key_len, &tmp);
}
#endif /* add_assoc_object */

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
SWOW_API zend_object *swow_custom_object_clone(zend7_object *object);

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
    zproperty = zend_read_property(Z_OBJCE_P(ZEND_THIS), Z8_OBJ_P(ZEND_THIS), ZEND_STRL(name), 1, &zretval); \
    ZVAL_DEREF(zproperty); \
    ZVAL_COPY(return_value, zproperty); \
} while (0)

#define RETURN_THIS_PROPERTY(name)  do { \
    RETVAL_THIS_PROPERTY(name); \
    return; \
} while (0)

#define RETURN_DEBUG_INFO_WITH_PROPERTIES(zdebug_info) do { \
    php_array_merge(Z_ARR_P(zdebug_info), ZEND_THIS_OBJECT->handlers->get_properties(ZEND7_THIS)); \
    RETURN_ZVAL(zdebug_info, 0, 0); \
} while (0)

/* function */

typedef struct
{
    zend_fcall_info info;
    zend_fcall_info_cache cache;
} swow_fcall_t;

/* method caller */

#define swow_call_method_with_0_params(zobject, object_ce, fn_ptr_ptr, fn_name, retval) \
        zend_call_method_with_0_params(Z8_OBJ_P(zobject), object_ce, fn_ptr_ptr, fn_name, retval)

#define swow_call_method_with_1_params(zobject, object_ce, fn_ptr_ptr, fn_name, retval, v1) \
        zend_call_method_with_1_params(Z8_OBJ_P(zobject), object_ce, fn_ptr_ptr, fn_name, retval, v1)

#define swow_call_method_with_2_params(zobject, object_ce, fn_ptr_ptr, fn_name, retval, v1, v2) \
        zend_call_method_with_2_params(Z8_OBJ_P(zobject), object_ce, fn_ptr_ptr, fn_name, retval, v1, v2)

/* callable helper */

extern SWOW_API zval swow_internal_callable_key;

SWOW_API cat_bool_t swow_function_is_internal_accessor(INTERNAL_FUNCTION_PARAMETERS);
SWOW_API cat_bool_t swow_function_internal_access_only_check(INTERNAL_FUNCTION_PARAMETERS);

#define SWOW_FUNCTION_INTERNAL_ACCESS_ONLY() do { \
    if (UNEXPECTED(!swow_function_internal_access_only_check(INTERNAL_FUNCTION_PARAM_PASSTHRU))) { \
        RETURN_THROWS(); \
    } \
} while (0)

/* output globals */

#if PHP_VERSION_ID >= 80100
#define SWOW_OUTPUT_GLOBALS_START_FILENAME OG(output_start_filename)
#else
#if PHP_VERSION_ID >= 80000
#define SWOW_OUTPUT_GLOBALS_START_FILENAME OG(output_start_filename_str)
#endif
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

static cat_always_inline void swow_output_globals_fast_shutdown(void)
{
    if (UNEXPECTED(OG(handlers).elements != NULL)) {
        swow_output_globals_shutdown();
    }
}

#ifdef __cplusplus
}
#endif
#endif /* SWOW_H */
