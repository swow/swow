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

#include "cat_thannel.h"

#include "cat_time.h"

#include "cat_atomic.h"

#ifdef CAT_THREAD

typedef uint32_t cat_thannel_size_t;
#define CAT_THANNEL_SIZE_FMT "%u"
#define CAT_THANNEL_SIZE_FMT_SPEC "u"
#define CAT_THANNEL_SIZE_MAX UINT32_MAX

typedef uint8_t cat_thannel_data_size_t;
#define CAT_THANNEL_DATA_SIZE_FMT "%u"
#define CAT_THANNEL_DATA_SIZE_FMT_SPEC "u"
#define CAT_THANNEL_DATA_SIZE_MAX UINT8_MAX

typedef void (*cat_thannel_data_dtor_t)(const cat_data_t *data);

typedef struct cat_thannel_s {
    cat_thannel_data_size_t data_size;
    cat_thannel_data_dtor_t dtor;
    cat_thannel_size_t capacity;
    cat_thannel_size_t length;
    cat_queue_t producers;
    cat_queue_t consumers;
    union {
        struct {
            union {
                const cat_data_t *in;
                cat_data_t *out;
            } data;
            union {
                cat_bool_t push;
                cat_bool_t pop;
            } able;
        } unbuffered;
        struct {
            cat_queue_t storage;
        } buffered;
    } u;
    uv_mutex_t mutex;
} cat_thannel_t;

typedef struct cat_thannel_storage_s {
    cat_queue_node_t node;
    char data[1];
} cat_thannel_storage_t;

CAT_API cat_thannel_t *cat_thannel_create(cat_thannel_t *thannel, cat_thannel_size_t capacity, cat_thannel_data_size_t data_size, cat_thannel_data_dtor_t dtor)
{
    cat_thannel_t *thannel;
    int error;

    thannel = (cat_thannel_t *) cat_sys_malloc(sizeof(*thannel));
    if (thannel == NULL) {
        cat_update_last_error_of_syscall("Malloc for thannel failed");
        return NULL;
    }
    thannel->data_size = data_size;
    thannel->dtor = dtor;
    thannel->capacity = capacity;
    thannel->length = 0;
    cat_queue_init(&thannel->storage);
    error = uv_mutex_init(&thannel->mutex);
    if (error != 0) {
        cat_update_last_error_with_reason(error, "Thocket storage mutex init failed");
    }

    return thannel;

    _error:
    cat_sys_free(thannel);
    return NULL;
}

CAT_API cat_bool_t cat_thannel_push(cat_thannel_t *thannel, const cat_data_t *data, cat_timeout_t timeout)
{
    cat_thannel_storage_t *storage;

    uv_mutex_lock(&thannel->mutex);

    if (thannel->length == thannel->capacity) {
        cat_bool_t ret;
        uv_mutex_unlock(&thannel->mutex);
        cat_queue_push_back(&thannel->producers, &storage->node);
        ret = cat_time_wait(timeout);
        cat_queue_remove(&storage->node);
        if (unlikely(!ret)) {
            /* sleep failed or timedout */
            cat_update_last_error_with_previous("Thannel wait consumer failed");
            return cat_false;
        }
        uv_mutex_lock(&thannel->mutex);
        CAT_ASSERT(thannel->length < thannel->capacity);
    }

    storage = (cat_thannel_storage_t *) cat_sys_malloc(offsetof(cat_thannel_storage_t, data) + thannel->data_size);
    if (storage == NULL) {
        cat_update_last_error_of_syscall("Malloc for thannel storage failed");
        return cat_false;
    }
    memcpy(storage->data, data, thannel->data_size);
    cat_queue_push_back(&thannel->storage, &storage->node);
    thannel->length++;

    uv_mutex_unlock(&thannel->mutex);

    return cat_true;
}



#endif /* CAT_THREAD */
