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

/* core driver */
#include "cat_driver.h"

#ifdef CAT_VM
/* virtual machine
 * (we can predefine something here) */
#include "cat_vm.h"
#endif

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
/* log */
#include "cat_log.h"
/* debug */
#include "cat_debug.h"
/* env */
#include "cat_env.h"
/* helper */
#include "cat_helper.h"

#ifndef CAT_USE_BUG_DETECTOR
# if !defined(CAT_DEBUG) && defined(CAT_OS_UNIX_LIKE) && defined(SIGSEGV) && !defined(CAT_HAVE_ASAN)
# define CAT_USE_BUG_DETECTOR 1 /* report segment fault for user */
# endif
#endif

#ifndef CAT_OS_WIN
#define CAT_EXEPATH_MAX (PATH_MAX + PATH_MAX + 1)
#else
#define CAT_EXEPATH_MAX 32768
#endif

/* globals */

CAT_GLOBALS_STRUCT_BEGIN(cat) {
    cat_bool_t runtime;
    cat_bool_t show_last_error;
    cat_error_t last_error;
    cat_const_string_t exepath;
    cat_log_globals_t log_globals;
} CAT_GLOBALS_STRUCT_END(cat);

extern CAT_API CAT_GLOBALS_DECLARE(cat);

#define CAT_G(x) CAT_GLOBALS_GET(cat, x)

#define CAT_LOG_G(x) CAT_G(log_globals).x

/* special constants */

#define CAT_MAGIC_NUMBER 9764

/* module initialization */

CAT_API cat_bool_t cat_module_init(void);
CAT_API cat_bool_t cat_module_shutdown(void);
CAT_API cat_bool_t cat_runtime_init(void);
CAT_API cat_bool_t cat_runtime_shutdown(void);
CAT_API cat_bool_t cat_runtime_close(void);

/* it may take ownership of the memory that argv points to */
CAT_API char **cat_setup_args(int argc, char** argv);

CAT_API const cat_const_string_t *cat_exepath(void);

#define CAT_PROCESS_TITLE_BUFFER_SIZE 512
/* we must call setup_args() before using title releated APIs  */
CAT_API char *cat_get_process_title(char* buffer, size_t size); CAT_MAY_FREE
CAT_API cat_bool_t cat_set_process_title(const char* title);

CAT_API cat_bool_t cat_is_main_thread(void);

#ifdef __cplusplus
}
#endif
#endif /* CAT_H */
