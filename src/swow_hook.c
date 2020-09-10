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

SWOW_API cat_bool_t swow_hook_internal_function(const zend_function_entry *fe)
{
    zend_function *function = (zend_function *) zend_hash_str_find_ptr(CG(function_table), fe->fname, strlen(fe->fname));

    if (UNEXPECTED(function == NULL)) {
        zend_function_entry fes[] = { fe[0], PHP_FE_END };
        if (zend_register_functions(NULL, fes, NULL, MODULE_PERSISTENT) != SUCCESS) {
            return cat_false;
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
