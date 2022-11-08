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

#ifndef SWOW_H
#define SWOW_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#define SWOW_MODULE_NAME "Swow"
#define SWOW_MODULE_NAME_LC "swow"

/* version */

#define SWOW_VERSION            "0.2.0-dev"
#define SWOW_VERSION_ID         200
#define SWOW_MAJOR_VERSION      0
#define SWOW_MINOR_VERSION      2
#define SWOW_RELEASE_VERSION    0
#define SWOW_EXTRA_VERSION      "dev"

/* compiler */

#define SWOW_API           CAT_API
#define SWOW_INTERNAL      CAT_INTERNAL
#define SWOW_UNSAFE        CAT_UNSAFE
#define SWOW_MAY_THROW

#include "swow_wrapper.h"
#include "swow_exception.h"

#if PHP_VERSION_ID < 80000
#error "require PHP version 8.0 or later"
#endif

/* globals */

extern SWOW_API zend_module_entry swow_module_entry;
#define phpext_swow_ptr &swow_module_entry

#if defined(ZTS) && defined(COMPILE_DL_SWOW)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

typedef enum swow_runtime_state_e {
    SWOW_RUNTIME_STATE_NONE = 0,
    SWOW_RUNTIME_STATE_INIT,
    SWOW_RUNTIME_STATE_RUNNING,
    SWOW_RUNTIME_STATE_SHUTDOWN,
} swow_runtime_state_t;

ZEND_BEGIN_MODULE_GLOBALS(swow)
    swow_runtime_state_t runtime_state;
ZEND_END_MODULE_GLOBALS(swow)

ZEND_EXTERN_MODULE_GLOBALS(swow)

#define SWOW_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(swow, v)

/* globals (not thread safe) */

typedef struct {
    zend_bool cli;
    zend_bool has_debug_extension;
    // TODO: add it to ini and make it work
    zend_bool observer_enabled;
} swow_nts_globals_t;

extern SWOW_API swow_nts_globals_t swow_nts_globals;

#define SWOW_NTS_G(x) swow_nts_globals.x

#ifdef ZTS
ZEND_TSRMLS_CACHE_EXTERN()
#endif

extern SWOW_API zend_class_entry *swow_ce;
extern SWOW_API zend_class_entry *swow_extension_ce;

zend_result swow_module_init(INIT_FUNC_ARGS);
zend_result swow_module_shutdown(INIT_FUNC_ARGS);
zend_result swow_runtime_init(INIT_FUNC_ARGS);
zend_result swow_runtime_shutdown(INIT_FUNC_ARGS);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_H */
