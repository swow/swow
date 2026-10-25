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

#ifndef SWOW_SHARED_BUFFER_ABI_H
#define SWOW_SHARED_BUFFER_ABI_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "cat_atomic.h"

/* the PHP binding defines SWOW_API through swow.h */
#ifndef SWOW_API
# define SWOW_API CAT_API
#endif

/*
 * The header sits at the start of the mapping, and the data area starts at
 * data_offset. The header holds no pointer, so the same bytes are valid in
 * every process that maps the object
 */

#define SWOW_SHARED_BUFFER_MAGIC "SWBS"
#define SWOW_SHARED_BUFFER_VERSION 1
/* unlink the name when the last mapping releases the buffer */
#define SWOW_SHARED_BUFFER_FLAG_UNLINK_ON_LAST 0b1

typedef struct swow_shared_buffer_header_s {
    char magic[4];
    uint16_t version;
    uint16_t flags;
    /* mapping-level reference count, accessed atomically */
    uint32_t refcount;
    uint32_t data_offset;
    uint64_t size;
    uint64_t alignment;
    uint32_t header_size;
    uint32_t reserved0;
    uint64_t reserved[3];
} swow_shared_buffer_header_t;

CAT_STATIC_ASSERT(sizeof(swow_shared_buffer_header_t) == 64);
CAT_STATIC_ASSERT(offsetof(swow_shared_buffer_header_t, magic) == 0);
CAT_STATIC_ASSERT(offsetof(swow_shared_buffer_header_t, version) == 4);
CAT_STATIC_ASSERT(offsetof(swow_shared_buffer_header_t, flags) == 6);
CAT_STATIC_ASSERT(offsetof(swow_shared_buffer_header_t, refcount) == 8);
CAT_STATIC_ASSERT(offsetof(swow_shared_buffer_header_t, data_offset) == 12);
CAT_STATIC_ASSERT(offsetof(swow_shared_buffer_header_t, size) == 16);
CAT_STATIC_ASSERT(offsetof(swow_shared_buffer_header_t, alignment) == 24);
CAT_STATIC_ASSERT(offsetof(swow_shared_buffer_header_t, header_size) == 32);
CAT_STATIC_ASSERT(offsetof(swow_shared_buffer_header_t, reserved0) == 36);
CAT_STATIC_ASSERT(offsetof(swow_shared_buffer_header_t, reserved) == 40);

typedef struct swow_shared_buffer_s swow_shared_buffer_t; /* opaque */

/* the bare-address atomics have no unsigned 32-bit operation, and the
 * refcount stays far below the sign bit */
static inline void swow_shared_buffer_header_addref(swow_shared_buffer_header_t *header)
{
    cat_atomic_int32_fetch_add_at(&header->refcount, 1);
}

/* cat_true when this call dropped the last mapping reference */
static inline cat_bool_t swow_shared_buffer_header_release(swow_shared_buffer_header_t *header)
{
    return cat_atomic_int32_fetch_sub_at(&header->refcount, 1) == 1;
}

static inline void *swow_shared_buffer_header_data(const swow_shared_buffer_header_t *header)
{
    return (void *) ((char *) header + header->data_offset);
}

/* cat_true when the magic and the version match, so the header is complete */
static inline cat_bool_t swow_shared_buffer_header_check(const swow_shared_buffer_header_t *header)
{
    return (cat_bool_t) (memcmp(header->magic, SWOW_SHARED_BUFFER_MAGIC,
                                sizeof(header->magic)) == 0 &&
                         header->version == SWOW_SHARED_BUFFER_VERSION);
}

/*
 * A named buffer lives in the shared memory object of that name, and an
 * anonymous buffer has no name. A slice is a view of the data area of another
 * handle
 *
 * Every pointer function returns NULL on failure, and the error code and text
 * come from the last_error functions. The C interface never throws
 */

/* NULL name for an anonymous buffer */
SWOW_API swow_shared_buffer_t *swow_shared_buffer_create(const char *name,
    size_t size, size_t alignment, uint32_t flags);
SWOW_API swow_shared_buffer_t *swow_shared_buffer_attach(const char *name);

/* the handle-level reference count, not the mapping-level one */
SWOW_API void swow_shared_buffer_addref(swow_shared_buffer_t *buffer);
SWOW_API void swow_shared_buffer_release(swow_shared_buffer_t *buffer);

SWOW_API cat_bool_t swow_shared_buffer_unlink(const char *name);

SWOW_API void *swow_shared_buffer_get_data(const swow_shared_buffer_t *buffer);
SWOW_API size_t swow_shared_buffer_get_size(const swow_shared_buffer_t *buffer);
SWOW_API size_t swow_shared_buffer_get_alignment(const swow_shared_buffer_t *buffer);
/* NULL for an anonymous buffer and for a slice */
SWOW_API const char *swow_shared_buffer_get_name(const swow_shared_buffer_t *buffer);
SWOW_API swow_shared_buffer_header_t *swow_shared_buffer_get_header(
    const swow_shared_buffer_t *buffer);
/* the mapping-level reference count */
SWOW_API uint32_t swow_shared_buffer_get_refcount(const swow_shared_buffer_t *buffer);

/*
 * A new handle that holds a handle reference on buffer. Its data pointer is
 * the data pointer of buffer plus offset. The bounds check fails with
 * CAT_EINVAL. A slice can not be serialized
 */
SWOW_API swow_shared_buffer_t *swow_shared_buffer_slice(swow_shared_buffer_t *buffer,
    size_t offset, size_t size);

SWOW_API int swow_shared_buffer_last_error_code(void);
SWOW_API const char *swow_shared_buffer_last_error_message(void);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_SHARED_BUFFER_ABI_H */
