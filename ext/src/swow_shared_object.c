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

#include "swow_shared_object.h"

#include <stdint.h>

#define SWOW_SHARED_OBJECT_INITIAL_CAPACITY 64
#define SWOW_SHARED_OBJECT_LOAD_NUMERATOR 7
#define SWOW_SHARED_OBJECT_LOAD_DENOMINATOR 10
#define SWOW_SHARED_OBJECT_TOMBSTONE ((swow_shared_object_t *) (uintptr_t) 1)

typedef struct swow_shared_object_context_s {
    uv_mutex_t lock;
    swow_shared_object_t **buckets;
    size_t capacity;
    size_t count;
    size_t deleted;
    uint64_t token;
    uint64_t id_seed;
    uint32_t pid;
    cat_bool_t initialized;
} swow_shared_object_context_t;

static swow_shared_object_context_t swow_shared_object_context;

/* SplitMix64 finalizer
 * Constants and shifts from https://prng.di.unimi.it/splitmix64.c */
static uint64_t swow_shared_object_hash(uint64_t id)
{
    id ^= id >> 30;
    id *= UINT64_C(0xbf58476d1ce4e5b9);
    id ^= id >> 27;
    id *= UINT64_C(0x94d049bb133111eb);
    id ^= id >> 31;

    return id;
}

static uint64_t swow_shared_object_random(void)
{
    /* SplitMix64 Weyl increment: floor(2^64 / phi) */
    uint64_t x = ++swow_shared_object_context.id_seed + UINT64_C(0x9e3779b97f4a7c15);

    x ^= ((uint64_t) swow_shared_object_context.pid) << 32;
    /* stack, data and text ASLR */
    x ^= (uint64_t) (uintptr_t) &x;
    x ^= (uint64_t) (uintptr_t) &swow_shared_object_context;
    x ^= (uint64_t) (uintptr_t) (void *) swow_shared_object_hash;

    /* SplitMix64 finalizer, same constants as swow_shared_object_hash() */
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31;

    return x;
}

static size_t swow_shared_object_find_slot(uint64_t id, cat_bool_t for_insert)
{
    size_t mask = swow_shared_object_context.capacity - 1;
    size_t index = (size_t) swow_shared_object_hash(id) & mask;
    size_t tombstone = SIZE_MAX;

    for (;;) {
        swow_shared_object_t *bucket = swow_shared_object_context.buckets[index];

        if (bucket == NULL) {
            return for_insert && tombstone != SIZE_MAX ? tombstone : index;
        }
        if (bucket == SWOW_SHARED_OBJECT_TOMBSTONE) {
            if (tombstone == SIZE_MAX) {
                tombstone = index;
            }
        } else if (bucket->id == id) {
            return index;
        }

        index = (index + 1) & mask;
    }
}

static swow_shared_object_t *swow_shared_object_find_locked(uint64_t id)
{
    size_t index = swow_shared_object_find_slot(id, cat_false);
    swow_shared_object_t *object = swow_shared_object_context.buckets[index];

    return object != SWOW_SHARED_OBJECT_TOMBSTONE ? object : NULL;
}

static cat_bool_t swow_shared_object_resize(size_t capacity)
{
    swow_shared_object_t **buckets = cat_sys_calloc(capacity, sizeof(*buckets));
    swow_shared_object_t **old_buckets = swow_shared_object_context.buckets;
    size_t old_capacity = swow_shared_object_context.capacity;

    if (buckets == NULL) {
        return cat_false;
    }

    swow_shared_object_context.buckets = buckets;
    swow_shared_object_context.capacity = capacity;
    swow_shared_object_context.deleted = 0;

    for (size_t i = 0; i < old_capacity; i++) {
        swow_shared_object_t *object = old_buckets[i];

        if (object != NULL && object != SWOW_SHARED_OBJECT_TOMBSTONE) {
            size_t index = swow_shared_object_find_slot(object->id, cat_true);

            swow_shared_object_context.buckets[index] = object;
        }
    }

    cat_sys_free(old_buckets);

    return cat_true;
}

static cat_bool_t swow_shared_object_insert(swow_shared_object_t *object)
{
    if ((swow_shared_object_context.count + swow_shared_object_context.deleted + 1) *
            SWOW_SHARED_OBJECT_LOAD_DENOMINATOR >
        swow_shared_object_context.capacity * SWOW_SHARED_OBJECT_LOAD_NUMERATOR) {
        if (!swow_shared_object_resize(swow_shared_object_context.capacity * 2)) {
            return cat_false;
        }
    }

    size_t index = swow_shared_object_find_slot(object->id, cat_true);

    if (swow_shared_object_context.buckets[index] == SWOW_SHARED_OBJECT_TOMBSTONE) {
        swow_shared_object_context.deleted--;
    }
    swow_shared_object_context.buckets[index] = object;
    swow_shared_object_context.count++;

    return cat_true;
}

static void swow_shared_object_remove(swow_shared_object_t *object)
{
    size_t index = swow_shared_object_find_slot(object->id, cat_false);

    if (swow_shared_object_context.buckets[index] == object) {
        swow_shared_object_context.buckets[index] = SWOW_SHARED_OBJECT_TOMBSTONE;
        swow_shared_object_context.count--;
        swow_shared_object_context.deleted++;
    }
}

swow_shared_object_t *swow_shared_object_create(size_t size, swow_shared_object_free_t free)
{
    swow_shared_object_t *object;

    if (size < sizeof(*object)) {
        size = sizeof(*object);
    }

    object = cat_sys_calloc(1, size);
    if (object == NULL) {
        cat_update_last_error(CAT_ENOMEM, "Failed to allocate shared object");
        return NULL;
    }

    uv_mutex_lock(&swow_shared_object_context.lock);

    do {
        object->id = swow_shared_object_random();
    } while (swow_shared_object_find_locked(object->id) != NULL);

    cat_atomic_uint32_init(&object->refcount, 1);
    object->free = free;

    if (!swow_shared_object_insert(object)) {
        uv_mutex_unlock(&swow_shared_object_context.lock);
        cat_sys_free(object);
        cat_update_last_error(CAT_ENOMEM, "Failed to register shared object");
        return NULL;
    }

    uv_mutex_unlock(&swow_shared_object_context.lock);

    return object;
}

void swow_shared_object_addref(swow_shared_object_t *object)
{
    cat_atomic_uint32_fetch_add(&object->refcount, 1);
}

void swow_shared_object_release(swow_shared_object_t *object)
{
    uv_mutex_lock(&swow_shared_object_context.lock);

    if (cat_atomic_uint32_fetch_sub(&object->refcount, 1) != 1) {
        uv_mutex_unlock(&swow_shared_object_context.lock);
        return;
    }

    swow_shared_object_remove(object);

    uv_mutex_unlock(&swow_shared_object_context.lock);

    cat_bool_t free_memory = cat_true;

    if (object->free != NULL) {
        free_memory = object->free(object);
    }
    if (free_memory) {
        cat_sys_free(object);
    }
}

static swow_shared_object_t *swow_shared_object_find(uint64_t id)
{
    swow_shared_object_t *object;

    uv_mutex_lock(&swow_shared_object_context.lock);

    object = swow_shared_object_find_locked(id);
    if (object != NULL) {
        swow_shared_object_addref(object);
    }

    uv_mutex_unlock(&swow_shared_object_context.lock);

    return object;
}

void swow_shared_object_serialize(zval *return_value, swow_shared_object_t *object)
{
    array_init(return_value);
    add_assoc_long(return_value, "id", (zend_long) object->id);
    add_assoc_long(return_value, "token", (zend_long) swow_shared_object_context.token);
}

cat_bool_t swow_shared_object_unserialize(HashTable *data, swow_shared_object_t **object)
{
    zval *z_id = zend_hash_str_find(data, ZEND_STRL("id"));
    zval *z_token = zend_hash_str_find(data, ZEND_STRL("token"));

    if (z_id == NULL || z_token == NULL ||
        Z_TYPE_P(z_id) != IS_LONG || Z_TYPE_P(z_token) != IS_LONG) {
        cat_update_last_error(CAT_EINVAL, "Invalid serialized shared object");
        return cat_false;
    }

    if ((uint64_t) Z_LVAL_P(z_token) != swow_shared_object_context.token) {
        cat_update_last_error(CAT_EINVAL, "Serialized shared object belongs to another process");
        return cat_false;
    }

    *object = swow_shared_object_find((uint64_t) Z_LVAL_P(z_id));
    if (*object == NULL) {
        cat_update_last_error(CAT_ENOENT, "Serialized shared object is no longer alive");
        return cat_false;
    }

    return cat_true;
}

zend_result swow_shared_object_module_init(INIT_FUNC_ARGS)
{
    if (uv_mutex_init(&swow_shared_object_context.lock) != 0) {
        return FAILURE;
    }

    swow_shared_object_context.capacity = SWOW_SHARED_OBJECT_INITIAL_CAPACITY;
    swow_shared_object_context.buckets = cat_sys_calloc(swow_shared_object_context.capacity,
                                                 sizeof(*swow_shared_object_context.buckets));
    if (swow_shared_object_context.buckets == NULL) {
        uv_mutex_destroy(&swow_shared_object_context.lock);
        return FAILURE;
    }

    swow_shared_object_context.pid = (uint32_t) uv_os_getpid();

    if (uv_random(NULL, NULL, &swow_shared_object_context.id_seed,
                  sizeof(swow_shared_object_context.id_seed), 0, NULL) != 0) {
        uint64_t seed = uv_hrtime();

        seed ^= ((uint64_t) swow_shared_object_context.pid) << 32;
        seed ^= (uint64_t) (uintptr_t) &swow_shared_object_context;
        swow_shared_object_context.id_seed = seed;
    }

    do {
        swow_shared_object_context.token = swow_shared_object_random();
    } while (swow_shared_object_context.token == 0);

    swow_shared_object_context.initialized = cat_true;

    return SUCCESS;
}

zend_result swow_shared_object_module_shutdown(SHUTDOWN_FUNC_ARGS)
{
    if (!swow_shared_object_context.initialized) {
        return SUCCESS;
    }

    for (size_t i = 0; i < swow_shared_object_context.capacity; i++) {
        swow_shared_object_t *object = swow_shared_object_context.buckets[i];

        if (object != NULL && object != SWOW_SHARED_OBJECT_TOMBSTONE) {
            cat_bool_t free_memory = cat_true;

            if (object->free != NULL) {
                free_memory = object->free(object);
            }
            if (free_memory) {
                cat_sys_free(object);
            }
        }
    }

    cat_sys_free(swow_shared_object_context.buckets);
    uv_mutex_destroy(&swow_shared_object_context.lock);

    memset(&swow_shared_object_context, 0, sizeof(swow_shared_object_context));

    return SUCCESS;
}
