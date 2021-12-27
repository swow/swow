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

#ifndef SWOW_{{FILE_NAME}}_H
#define SWOW_{{FILE_NAME}}_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_{{file_name}}.h"

extern SWOW_API zend_class_entry *swow_{{type_name}}_ce;
extern SWOW_API zend_object_handlers swow_{{type_name}}_handlers;

typedef struct swow_{{type_name}}_s {
    cat_{{type_name}}_t {{cat_var_name}};
    zend_object std;
} swow_{{type_name}}_t;

/* loader */

int swow_{{module_name}}_module_init(INIT_FUNC_ARGS);

/* helper*/

static zend_always_inline swow_{{type_name}}_t* swow_{{type_name}}_get_from_handle(cat_{{type_name}}_t *{{cat_var_name}})
{
    CAT_STATIC_ASSERT(offsetof(swow_{{type_name}}_t, {{cat_var_name}}) == 0);
    return (swow_{{type_name}}_t *) {{cat_var_name}};
}

static zend_always_inline swow_{{type_name}}_t* swow_{{type_name}}_get_from_object(zend_object *object)
{
    return (swow_{{type_name}}_t *) ((char *) object - swow_{{type_name}}_handlers.offset);
}

#ifdef __cplusplus
}
#endif
#endif /* SWOW_{{FILE_NAME}}_H */
