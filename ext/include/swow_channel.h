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

#ifndef SWOW_CHANNEL_H
#define SWOW_CHANNEL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_channel.h"

extern SWOW_API zend_class_entry *swow_channel_ce;
extern SWOW_API zend_object_handlers swow_channel_handlers;

extern SWOW_API zend_class_entry *swow_channel_selector_ce;
extern SWOW_API zend_object_handlers swow_channel_selector_handlers;

extern SWOW_API zend_class_entry *swow_channel_exception_ce;
extern SWOW_API zend_class_entry *swow_channel_selector_exception_ce;

typedef struct swow_channel_s {
    cat_channel_t channel;
    zend_object std;
} swow_channel_t;

typedef struct swow_channel_selector_s {
    /* requests */
    uint32_t size;
    uint32_t count;
    cat_channel_select_request_t *requests;
    cat_channel_select_request_t _requests[4];
    /* response */
    cat_channel_opcode_t last_opcode;
    zval zdata;
    /* internal */
    zval zstorage[4];
    zend_object std;
} swow_channel_selector_t;

/* loader */

zend_result swow_channel_module_init(INIT_FUNC_ARGS);

/* helper*/

static zend_always_inline swow_channel_t *swow_channel_get_from_handle(cat_channel_t *channel)
{
    return cat_container_of(channel, swow_channel_t, channel);
}

static zend_always_inline swow_channel_t *swow_channel_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_channel_t, std);
}

static zend_always_inline swow_channel_selector_t *swow_channel_selector_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_channel_selector_t, std);
}

#ifdef __cplusplus
}
#endif
#endif /* SWOW_CHANNEL_H */
