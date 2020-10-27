/*
  +--------------------------------------------------------------------------+
  | libcat                                                                   |
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

#ifndef CAT_H
#define CAT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef CAT_VM
/* virtual machine
 * (we can predefine something here) */
#include "cat_vm.h"
#endif

/* kernel driver */
#include "cat_driver.h"

/* version */
#include "cat_version.h"
/* standard C libraries */
#include "cat_scl.h"
/* OS related */
#include "cat_os.h"
/* builtin (compiler) */
#include "cat_builtin.h"
/* cross platform */
#include "cat_cp.h"
/* types */
#include "cat_type.h"
/* memory */
#include "cat_memory.h"
/* math (calculation) */
#include "cat_math.h"
/* string */
#include "cat_string.h"
/* error */
#include "cat_error.h"
/* thread safe */
#include "cat_tsrm.h"
/* module */
#include "cat_module.h"
/* log */
#include "cat_log.h"
/* debug */
#include "cat_debug.h"
/* env */
#include "cat_env.h"

/* global */

CAT_GLOBALS_STRUCT_BEGIN(cat)
    cat_log_types_t log_types;
    cat_module_types_t log_module_types;
    cat_error_t last_error;
    cat_bool_t runtime;
#ifdef CAT_SOURCE_POSITION
    cat_bool_t log_source_postion;
#endif
#ifdef CAT_DEBUG
    cat_bool_t show_last_error;
#endif
CAT_GLOBALS_STRUCT_END(cat)

extern CAT_API CAT_GLOBALS_DECLARE(cat)

#define CAT_G(x) CAT_GLOBALS_GET(cat, x)

#define CAT_MAGIC_NUMBER 9764

/* module initialization */

CAT_API cat_bool_t cat_module_init(void);
CAT_API cat_bool_t cat_module_shutdown(void);
CAT_API cat_bool_t cat_runtime_init(void);
CAT_API cat_bool_t cat_runtime_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif /* CAT_H */
