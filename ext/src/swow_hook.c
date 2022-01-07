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

#include "swow_hook.h"

static cat_bool_t swow_function_is_hookable(const char *name, size_t name_length)
{
    const char *s = NULL, *e = INI_STR("disable_functions");

    while (1) {
        if (*e == ' ' || *e == ',' || *e == '\0') {
            if (s) {
                if ((size_t) (e - s) == name_length && cat_strncasecmp(s, name, name_length) == 0) {
                    return cat_false;
                }
                s = NULL;
            }
        } else {
            if (!s) {
                s = e;
            }
        }
        if (*e == '\0') {
            break;
        }
        e++;
    }

    return cat_true;
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
