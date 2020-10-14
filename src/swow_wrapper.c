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

/* PHP 7.3 compatibility macro {{{*/
#if PHP_VERSION_ID < 70300
ZEND_API HashTable zend_empty_array;
#endif
/* }}} */

/* PHP 8 compatibility macro {{{*/
#if PHP_VERSION_ID < 80000
ZEND_API zend_string *zend_string_concat2(
        const char *str1, size_t str1_len,
        const char *str2, size_t str2_len)
{
    size_t len = str1_len + str2_len;
    zend_string *res = zend_string_alloc(len, 0);

    memcpy(ZSTR_VAL(res), str1, str1_len);
    memcpy(ZSTR_VAL(res) + str1_len, str2, str2_len);
    ZSTR_VAL(res)[len] = '\0';

    return res;
}

ZEND_API zend_string *zend_string_concat3(
        const char *str1, size_t str1_len,
        const char *str2, size_t str2_len,
        const char *str3, size_t str3_len)
{
    size_t len = str1_len + str2_len + str3_len;
    zend_string *res = zend_string_alloc(len, 0);

    memcpy(ZSTR_VAL(res), str1, str1_len);
    memcpy(ZSTR_VAL(res) + str1_len, str2, str2_len);
    memcpy(ZSTR_VAL(res) + str1_len + str2_len, str3, str3_len);
    ZSTR_VAL(res)[len] = '\0';

    return res;
}

ZEND_API zend_string *zend_create_member_string(zend_string *class_name, zend_string *member_name)
{
    return zend_string_concat3(
        ZSTR_VAL(class_name), ZSTR_LEN(class_name),
        "::", sizeof("::") - 1,
        ZSTR_VAL(member_name), ZSTR_LEN(member_name));
}

ZEND_API zend_string *get_active_function_or_method_name(void) /* {{{ */
{
    ZEND_ASSERT(zend_is_executing());

    return get_function_or_method_name(EG(current_execute_data)->func);
}
/* }}} */

ZEND_API zend_string *get_function_or_method_name(const zend_function *func) /* {{{ */
{
    if (func->common.scope) {
        return zend_create_member_string(func->common.scope->name, func->common.function_name);
    }

    return func->common.function_name ? zend_string_copy(func->common.function_name) : zend_string_init("main", sizeof("main") - 1, 0);
}
/* }}} */

ZEND_API const char *get_active_function_arg_name(uint32_t arg_num) /* {{{ */
{
    zend_function *func;

    if (!zend_is_executing()) {
        return NULL;
    }

    func = EG(current_execute_data)->func;

    return get_function_arg_name(func, arg_num);
}
/* }}} */

ZEND_API const char *get_function_arg_name(const zend_function *func, uint32_t arg_num) /* {{{ */
{
    if (!func || func->common.num_args < arg_num) {
        return NULL;
    }

    switch (func->type) {
        case ZEND_USER_FUNCTION:
            return ZSTR_VAL(func->common.arg_info[arg_num - 1].name);
        case ZEND_INTERNAL_FUNCTION:
            return ((zend_internal_arg_info*) func->common.arg_info)[arg_num - 1].name;
        default:
            return NULL;
    }
}
/* }}} */

ZEND_API zend_class_entry *zend_ce_value_error;

ZEND_API ZEND_COLD void zend_value_error(const char *format, ...) /* {{{ */
{
    va_list va;
    char *message = NULL;

    va_start(va, format);
    zend_vspprintf(&message, 0, format, va);
    zend_throw_exception(zend_ce_value_error, message, 0);
    efree(message);
    va_end(va);
} /* }}} */

static ZEND_COLD void ZEND_FASTCALL zend_argument_error_variadic(zend_class_entry *error_ce, uint32_t arg_num, const char *format, va_list va) /* {{{ */
{
    const char *space;
    const char *class_name;
    const char *arg_name;
    char *message = NULL;
    if (EG(exception)) {
        return;
    }

    class_name = get_active_class_name(&space);
    arg_name = get_active_function_arg_name(arg_num);

    zend_vspprintf(&message, 0, format, va);
    zend_throw_error(error_ce, "%s%s%s(): Argument #%d%s%s%s %s",
        class_name, space, get_active_function_name(), arg_num,
        arg_name ? " ($" : "", arg_name ? arg_name : "", arg_name ? ")" : "", message
    );
    efree(message);
}
/* }}} */

ZEND_API ZEND_COLD void ZEND_FASTCALL zend_argument_error(zend_class_entry *error_ce, uint32_t arg_num, const char *format, ...) /* {{{ */
{
    va_list va;

    va_start(va, format);
    zend_argument_error_variadic(error_ce, arg_num, format, va);
    va_end(va);
}
/* }}} */

ZEND_API ZEND_COLD void ZEND_FASTCALL zend_argument_type_error(uint32_t arg_num, const char *format, ...) /* {{{ */
{
    va_list va;

    va_start(va, format);
    zend_argument_error_variadic(zend_ce_type_error, arg_num, format, va);
    va_end(va);
}
/* }}} */

ZEND_API ZEND_COLD void ZEND_FASTCALL zend_argument_value_error(uint32_t arg_num, const char *format, ...) /* {{{ */
{
    va_list va;

    va_start(va, format);
    zend_argument_error_variadic(zend_ce_value_error, arg_num, format, va);
    va_end(va);
}
/* }}} */
#endif
/* }}} */

/* class */

SWOW_API zend_class_entry *swow_register_internal_class(
    const char *name, zend_class_entry *parent_ce, const zend_function_entry methods[],
    zend_object_handlers *handlers, const zend_object_handlers *parent_handlers,
    const cat_bool_t cloneable,const cat_bool_t serializable, const cat_bool_t unserializable,
    const swow_object_create_t create_object, const swow_object_free_t free_object, const int offset
)
{
    zend_class_entry *ce, _ce;
    cat_bool_t is_final; /* ignore final flag */

    INIT_CLASS_ENTRY_EX(_ce, name, strlen(name), methods);
    if ((is_final = (parent_ce && !!(parent_ce->ce_flags & ZEND_ACC_FINAL)))) {
        parent_ce->ce_flags ^= ZEND_ACC_FINAL;
    }
    ce = zend_register_internal_class_ex(&_ce, parent_ce);
    if (is_final) {
        ce->ce_flags |= ZEND_ACC_FINAL;
        parent_ce->ce_flags |= ZEND_ACC_FINAL;
    }
    if (!serializable) {
        ce->serialize = zend_class_serialize_deny;
    }
    if (!unserializable) {
        ce->unserialize = zend_class_unserialize_deny;
    }
    if (create_object != NULL) {
        ce->create_object = create_object;
    }
    if (handlers) {
        if (parent_handlers == NULL) {
            parent_handlers = zend_get_std_object_handlers();
        }
        memcpy(handlers, parent_handlers, sizeof(*parent_handlers));
        if (!cloneable) {
            handlers->clone_obj = NULL;
        }
        if (free_object != NULL) {
            handlers->free_obj = free_object;
        }
        if (offset != 0) {
            handlers->offset = offset;
            if (cloneable) {
                handlers->clone_obj = swow_custom_object_clone;
            }
        }
    } else {
        CAT_ASSERT(create_object == swow_create_object_deny || cloneable);
        CAT_ASSERT(free_object == NULL);
        CAT_ASSERT(offset == 0);
    }

    return ce;
}

SWOW_API zend_class_entry *swow_register_internal_interface(
    const char *name,
    const zend_function_entry methods[],
    swow_interface_gets_implemented_t interface_gets_implemented
)
{
    zend_class_entry *ce, _ce;

    INIT_CLASS_ENTRY_EX(_ce, name, strlen(name), methods);
    ce = zend_register_internal_interface(&_ce);
    ce->interface_gets_implemented = interface_gets_implemented;

    return ce;
}

SWOW_API zend_object *swow_create_object_deny(zend_class_entry *ce)
{
    zend_object *object;

    object = zend_objects_new(ce);
    /* Initialize default properties */
    if (EXPECTED(ce->default_properties_count != 0)) {
        zval *p = object->properties_table;
        zval *end = p + ce->default_properties_count;
        do {
            ZVAL_UNDEF(p);
            p++;
        } while (p != end);
    }
    zend_throw_error(NULL, "The object of %s can not be created for security reasons", ZSTR_VAL(ce->name));

    return object;
}

SWOW_API zend_object *swow_custom_object_clone(zend7_object *object)
{
    zend_object *old_object = Z7_OBJ_P(object), *new_object;
    zend_class_entry *ce = old_object->ce;
    int offset = old_object->handlers->offset;

    new_object = swow_object_create(ce);
    memcpy((((char *) new_object) - offset), (((char *) old_object) - offset), offset);

    zend_objects_clone_members(new_object, old_object);

    return new_object;
}

/* callable helper */

SWOW_API zval swow_internal_callable_key;

SWOW_API cat_bool_t swow_function_is_internal_accessor(INTERNAL_FUNCTION_PARAMETERS)
{
    zval *zdata = NULL;

    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL(zdata)
    ZEND_PARSE_PARAMETERS_END_EX(/* do nothing */);

    if (!zdata || Z_TYPE_P(zdata) != IS_PTR || Z_PTR_P(zdata) != &swow_internal_callable_key) {
        return cat_false;
    }
    return cat_true;
}

SWOW_API cat_bool_t swow_function_internal_access_only_check(INTERNAL_FUNCTION_PARAMETERS)
{
    cat_bool_t ret;

    ret = swow_function_is_internal_accessor(INTERNAL_FUNCTION_PARAM_PASSTHRU);

    if (unlikely(!ret)) {
        const char *space, *class_name;
        class_name = get_active_class_name(&space);
        zend_throw_error(
            NULL, "%s%s%s can not be called for security reasons",
            class_name, space, get_active_function_name()
        );
        return cat_false;
    }

    return cat_true;
}

/* output globals */

#include "SAPI.h"

SWOW_API void swow_output_globals_end(void)
{
    zend_bool no_headers = SG(request_info).no_headers;

    SG(request_info).no_headers = 1;
    if (OG(active)) {
        if (UNEXPECTED(SWOW_IS_OUT_OF_MEMORY())) {
            php_output_discard_all();
        } else {
            php_output_end_all();
        }
    }
    php_output_deactivate();
    php_output_activate();
    SG(request_info).no_headers = no_headers;
}

/* wrapper init/shutdown */

void swow_wrapper_init(void)
{
#if PHP_VERSION_ID < 70300
    zend_hash_init(&zend_empty_array, 0, NULL, NULL, 1);
#endif

#ifdef ZEND_NO_VALUE_ERROR
    do {
        zend_class_entry ce;
        INIT_CLASS_ENTRY(ce, "ValueError", NULL);
        zend_ce_value_error = zend_register_internal_class_ex(&ce, zend_ce_error);
        zend_ce_value_error->create_object = zend_ce_error->create_object;
    } while (0);
#endif

    ZVAL_PTR(&swow_internal_callable_key, &swow_internal_callable_key);
}

void swow_wrapper_shutdown(void)
{
#if PHP_VERSION_ID < 70300
    zend_hash_destroy(&zend_empty_array);
#endif
}
