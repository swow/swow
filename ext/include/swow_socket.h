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

#ifndef SWOW_SOCKET_H
#define SWOW_SOCKET_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_socket.h"

extern SWOW_API zend_class_entry *swow_socket_ce;
extern SWOW_API zend_object_handlers swow_socket_handlers;

extern SWOW_API zend_class_entry *swow_socket_exception_ce;

typedef struct swow_socket_s {
    cat_socket_t socket;
    zend_object std;
} swow_socket_t;

/* loader */

zend_result swow_socket_module_init(INIT_FUNC_ARGS);
zend_result swow_socket_runtime_init(INIT_FUNC_ARGS);

/* helper*/

static zend_always_inline swow_socket_t *swow_socket_get_from_handle(cat_socket_t *socket)
{
    return cat_container_of(socket, swow_socket_t, socket);
}

static zend_always_inline swow_socket_t *swow_socket_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_socket_t, std);
}

#ifdef __cplusplus
}
#endif
#endif /* SWOW_SOCKET_H */
