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

#include "swow_thread.h"

SWOW_API zend_class_entry *swow_thread_wait_reference_ce;
SWOW_API zend_object_handlers swow_thread_wait_reference_handlers;

SWOW_API zend_class_entry *swow_thread_exception_ce;

SWOW_API CAT_GLOBALS_DECLARE(swow_thread)

CAT_GLOBALS_CTOR_DECLARE_SZ(swow_thread)

zend_result swow_thread_module_init(INIT_FUNC_ARGS)
{
    
    return SUCCESS;
}
