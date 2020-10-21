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

#ifndef SWOW_WATCH_DOG_H
#define SWOW_WATCH_DOG_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_watch_dog.h"

extern SWOW_API zend_class_entry *swow_watch_dog_ce;
extern SWOW_API zend_object_handlers swow_watch_dog_handlers;

extern SWOW_API zend_class_entry *swow_watch_dog_exception_ce;

typedef struct
{
    cat_watch_dog_t watch_dog;
    zend_bool vm_interrupted;
    zend_bool *vm_interrupt_ptr;
    cat_msec_t delay;
    zval zalerter;
    zend_fcall_info_cache alerter;
} swow_watch_dog_t;

/* loader */

int swow_watch_dog_module_init(INIT_FUNC_ARGS);
int swow_watch_dog_runtime_init(INIT_FUNC_ARGS);
int swow_watch_dog_runtime_shudtown(INIT_FUNC_ARGS);

/* helper */

static cat_always_inline swow_watch_dog_t *swow_watch_dog_get_from_handle(cat_watch_dog_t *watch_dog)
{
    return cat_container_of(watch_dog, swow_watch_dog_t, watch_dog);
}

static cat_always_inline swow_watch_dog_t *swow_watch_dog_get_current(void)
{
    cat_watch_dog_t *watch_dog = CAT_WATCH_DOG_G(watch_dog);
    return watch_dog != NULL ? swow_watch_dog_get_from_handle(watch_dog) : NULL;
}

/* APIs */

SWOW_API cat_bool_t swow_watch_dog_run(cat_usec_t quantum, cat_nsec_t threshold, zval *zalerter);
SWOW_API cat_bool_t swow_watch_dog_stop(void);

SWOW_API void swow_watch_dog_alert_standard(cat_watch_dog_t *watch_dog);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_WATCH_DOG_H */
