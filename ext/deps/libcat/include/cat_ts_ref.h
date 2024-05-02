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

#ifndef CAT_TS_REF_H
#define CAT_TS_REF_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#include "cat_atomic.h"

typedef uint32_t cat_ts_refcount_t;

typedef struct cat_ts_ref_s {
    cat_atomic_uint32_t count;
} cat_ts_ref_t;

#define CAT_TS_REF_FIELD cat_ts_ref_t ref
#define CAT_TS_REF_INIT(object) cat_ts_ref_init(&((object)->ref))
#define CAT_TS_REF_GET(object)  cat_ts_ref_get(&((object)->ref))
#define CAT_TS_REF_ADD(object)  cat_ts_ref_add(&((object)->ref))
#define CAT_TS_REF_DEL(object)  cat_ts_ref_del(&((object)->ref))

static cat_always_inline void cat_ts_ref_init(cat_ts_ref_t *ref)
{
    cat_atomic_uint32_init(&ref->count, 1);
}

static cat_always_inline cat_ts_refcount_t cat_ts_ref_get(const cat_ts_ref_t *ref)
{
    return cat_atomic_uint32_load(&ref->count);
}

static cat_always_inline cat_ts_refcount_t cat_ts_ref_add(cat_ts_ref_t *ref)
{
    return cat_atomic_uint32_fetch_add(&ref->count, 1) + 1;
}

static cat_always_inline cat_ts_refcount_t cat_ts_ref_del(cat_ts_ref_t *ref)
{
    return cat_atomic_uint32_fetch_sub(&ref->count, 1) - 1;
}

#ifdef __cplusplus
}
#endif
#endif /* CAT_TS_REF_H */
