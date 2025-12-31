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
#include "swow_wrapper.h"
#include "swow_hook.h"

/* functions */

static cat_bool_t swow_function_is_hookable(const char *name, size_t name_length)
{
    return !cat_str_list_contains_ci(INI_STR("disable_functions"), name, name_length);
}

SWOW_API cat_bool_t swow_hook_internal_function_handler(const char *name, size_t name_length, zif_handler handler)
{
    return swow_hook_internal_function_handler_ex(name, name_length, handler, NULL);
}

SWOW_API cat_bool_t swow_hook_internal_function_handler_ex(const char *name, size_t name_length, zif_handler handler, zif_handler *original_handler)
{
    zend_function *function = (zend_function *) zend_hash_str_find_ptr(CG(function_table), name, name_length);

    if (original_handler != NULL) {
        *original_handler = NULL;
    }
    if (function == NULL) {
        return cat_false;
    }
    if (original_handler != NULL) {
        *original_handler = function->internal_function.handler;
    }
    function->internal_function.handler = handler;

    return cat_true;
}

SWOW_API cat_bool_t swow_hook_internal_function(const zend_function_entry *fe)
{
    const char *name = fe->fname;
    size_t name_length = strlen(fe->fname);
    zend_function *function = (zend_function *) zend_hash_str_find_ptr(CG(function_table), name, name_length);

    if (UNEXPECTED(function == NULL)) {
        if (swow_function_is_hookable(name, name_length)) {
            zend_function_entry fes[] = { fe[0], PHP_FE_END };
            if (zend_register_functions(NULL, fes, NULL, EG(current_module)->type) != SUCCESS) {
                return cat_false;
            }
        }
    } else {
        function->internal_function.handler = fe->handler;
    }

    return cat_true;
}

SWOW_API cat_bool_t swow_hook_internal_functions(const zend_function_entry *fes)
{
    const zend_function_entry *ptr = fes;
    cat_bool_t ret = cat_true;

    while (ptr->fname != NULL) {
        if (UNEXPECTED(!swow_hook_internal_function(ptr))) {
            ret = cat_false;
        }
        ptr++;
    }

    return ret;
}

/* constant */

static zend_always_inline void  swow_unregister_constant_if_exists(const char *name, size_t name_len)
{
    zend_constant *c = (zend_constant *) zend_hash_str_find_ptr(EG(zend_constants), name, name_len);
    if (c != NULL) {
        zend_hash_del(EG(zend_constants), c->name);
    }
}

SWOW_API void swow_register_null_constant_force(const char *name, size_t name_len, int flags, int module_number)
{
    swow_unregister_constant_if_exists(name, name_len);
    zend_register_null_constant(name, name_len, flags, module_number);
}

SWOW_API void swow_register_bool_constant_force(const char *name, size_t name_len, zend_bool bval, int flags, int module_number)
{
    swow_unregister_constant_if_exists(name, name_len);
    zend_register_bool_constant(name, name_len, bval, flags, module_number);
}

SWOW_API void swow_register_long_constant_force(const char *name, size_t name_len, zend_long lval, int flags, int module_number)
{
    swow_unregister_constant_if_exists(name, name_len);
    zend_register_long_constant(name, name_len, lval, flags, module_number);
}

SWOW_API void swow_register_double_constant_force(const char *name, size_t name_len, double dval, int flags, int module_number)
{
    swow_unregister_constant_if_exists(name, name_len);
    zend_register_double_constant(name, name_len, dval, flags, module_number);
}

SWOW_API void swow_register_string_constant_force(const char *name, size_t name_len, const char *strval, int flags, int module_number)
{
    swow_unregister_constant_if_exists(name, name_len);
    zend_register_string_constant(name, name_len, strval, flags, module_number);
}

SWOW_API void swow_register_stringl_constant_force(const char *name, size_t name_len, const char *strval, size_t strlen, int flags, int module_number)
{
    swow_unregister_constant_if_exists(name, name_len);
    zend_register_stringl_constant(name, name_len, strval, strlen, flags, module_number);
}

/* module cleaner */

static int swow_clean_module_constant_callback(zval *z_c, void *argument)
{
    zend_constant *c = (zend_constant *) Z_PTR_P(z_c);
    zend_module_entry *module = (zend_module_entry *) argument;
    if (((int) ZEND_CONSTANT_MODULE_NUMBER(c)) == module->module_number) {
        return ZEND_HASH_APPLY_REMOVE;
    } else {
        return ZEND_HASH_APPLY_KEEP;
    }
}

SWOW_API void swow_clean_module_constants(zend_module_entry *module)
{
    zend_hash_apply_with_argument(EG(zend_constants), swow_clean_module_constant_callback, module);
    /* Rehash the constant table after deleting constants. This ensures that all internal
     * constants are contiguous, which means we don't need to perform full table cleanup
     * on shutdown. */
    zend_hash_rehash(EG(zend_constants));
}

SWOW_API void swow_clean_module_classes(zend_module_entry *module)
{
    zval *zclass;
    zend_string *class_name;
    zend_class_entry *ce;
    ZEND_HASH_MAP_REVERSE_FOREACH_STR_KEY_VAL(CG(class_table), class_name, zclass) {
        ce = (zend_class_entry *) Z_PTR_P(zclass);
        if (ce->type == ZEND_INTERNAL_CLASS && ce->info.internal.module->module_number == module->module_number) {
            zend_hash_del(CG(class_table), class_name);
            zend_string_release_ex(class_name, 1);
        }
    } ZEND_HASH_FOREACH_END();
    /* Rehash the class table after deleting classes. This ensures that all internal
     * classes are contiguous, which means we don't need to perform full table cleanup
     * on shutdown. */
    zend_hash_rehash(CG(class_table));
}

static int swow_clean_module_function_callback(zval *z_fe, void *argument)
{
    zend_function *fe = (zend_function *) Z_PTR_P(z_fe);
    zend_module_entry *module = (zend_module_entry *) argument;
    if (fe->common.type == ZEND_INTERNAL_FUNCTION && fe->internal_function.module->module_number == module->module_number) {
        return ZEND_HASH_APPLY_REMOVE;
    } else {
        return ZEND_HASH_APPLY_KEEP;
    }
}

SWOW_API void swow_clean_module_functions(zend_module_entry *module)
{
    if (module->functions != NULL) {
        zend_unregister_functions(module->functions, -1, NULL);
        /* Clean functions registered separately from module->functions */
        zend_hash_apply_with_argument(CG(function_table), swow_clean_module_function_callback, module);
    }
    /* Rehash the function table after deleting functions. This ensures that all internal
     * functions are contiguous, which means we don't need to perform full table cleanup
     * on shutdown. Search "persistent_functions_count" for more. */
    zend_hash_rehash(CG(function_table));
}
