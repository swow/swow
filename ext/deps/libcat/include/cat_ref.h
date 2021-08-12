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

#ifndef CAT_REF_H
#define CAT_REF_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

typedef uint32_t cat_refcount_t;

typedef struct cat_ref_s {
    cat_refcount_t count;
} cat_ref_t;

#define CAT_REF_FIELD cat_ref_t ref
#define CAT_REF_INIT(object) cat_ref_init(&((object)->ref))
#define CAT_REF_GET(object)  cat_ref_get(&((object)->ref))
#define CAT_REF_ADD(object)  cat_ref_add(&((object)->ref))
#define CAT_REF_DEL(object)  cat_ref_del(&((object)->ref))

static cat_always_inline void cat_ref_init(cat_ref_t *ref)
{
    ref->count = 1;
}

static cat_always_inline cat_refcount_t cat_ref_get(const cat_ref_t *ref)
{
    return ref->count;
}

static cat_always_inline cat_refcount_t cat_ref_add(cat_ref_t *ref)
{
    return ++ref->count;
}

static cat_always_inline cat_refcount_t cat_ref_del(cat_ref_t *ref)
{
    return --ref->count;
}

#ifdef __cplusplus
}
#endif
#endif /* CAT_REF_H */
