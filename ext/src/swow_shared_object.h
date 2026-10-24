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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#ifndef SWOW_SHARED_OBJECT_H
#define SWOW_SHARED_OBJECT_H

#include "swow.h"

#include "cat_atomic.h"

typedef struct swow_shared_object_s swow_shared_object_t;
typedef cat_bool_t (*swow_shared_object_free_t)(swow_shared_object_t *object);

struct swow_shared_object_s {
    cat_atomic_uint32_t refcount;
    uint64_t id;
    /* names the class of the payload, so a serialized handle of one class is
     * never applied to another */
    const char *type;
    swow_shared_object_free_t free;
};

zend_result swow_shared_object_module_init(INIT_FUNC_ARGS);
zend_result swow_shared_object_module_shutdown(SHUTDOWN_FUNC_ARGS);

swow_shared_object_t *swow_shared_object_create(size_t size, const char *type,
                                                swow_shared_object_free_t free);
void swow_shared_object_addref(swow_shared_object_t *object);
void swow_shared_object_release(swow_shared_object_t *object);

void swow_shared_object_serialize(zval *return_value, swow_shared_object_t *object);
cat_bool_t swow_shared_object_unserialize(HashTable *data, const char *type,
                                          swow_shared_object_t **object);

#endif /* SWOW_SHARED_OBJECT_H */
