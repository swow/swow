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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#ifndef CAT_SHM_H_
#define CAT_SHM_H_

#include "cat.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Memory that threads share, and that a forked child or an unrelated process
 * shares too. The pages come from the kernel, so a copy on write does not
 * separate the two sides the way it separates heap memory.
 *
 * The mapping holds no libcat metadata. A create maps exactly size bytes, and
 * cat_shm_get_address returns the start of the mapping. An attach takes the
 * size from the platform, and a platform that allocates a mapping by page
 * reports the requested size rounded up to its page size, as macOS does, so an
 * exact-size check holds only where the platform keeps the requested size.
 *
 * A name needs no leading slash, because the POSIX backend adds one. The
 * platform bounds the length: Linux accepts 255 characters (NAME_MAX), macOS
 * accepts 31 characters including that slash, and Windows accepts 260
 * characters (MAX_PATH). A longer name fails with CAT_EINVAL. macOS accepts a
 * slash inside the name, and Linux rejects it with CAT_EINVAL.
 *
 * A named object keeps the name until cat_shm_unlink removes it, so a later
 * process still finds it. A create fails with CAT_EEXIST while the name exists,
 * so a leftover name is an error and never a silent share. On Windows the
 * section goes away when the last mapping closes, and no call removes the name
 * earlier.
 *
 * Attach only after the create call returned. The name is visible before the
 * mapping is ready, so an early attach fails with CAT_EINVAL.
 */

typedef struct cat_shm_s cat_shm_t;

/* A mapping that threads and forked children see.
 * NULL on failure: CAT_EINVAL for a size of 0, or the reason of a failed map. */
CAT_API cat_shm_t *cat_shm_create_anonymous(size_t size);

/* A mapping that other processes find by name.
 * NULL on failure: CAT_EEXIST while the name exists, CAT_EINVAL on a bad name. */
CAT_API cat_shm_t *cat_shm_create_named(const char *name, size_t size);

/* A second mapping of an object that another call created.
 * NULL on failure: CAT_ENOENT when no object has the name, CAT_EINVAL when the
 * object is empty, or the reason of a failed map. */
CAT_API cat_shm_t *cat_shm_attach_named(const char *name);

/* The start of the mapping. */
CAT_API void       *cat_shm_get_address(const cat_shm_t *shm);

/* The size that the create call passed, or the size that the platform reports
 * on attach. */
CAT_API size_t      cat_shm_get_size(const cat_shm_t *shm);
/* NULL for an anonymous mapping. */
CAT_API const char *cat_shm_get_name(const cat_shm_t *shm);

/*
 * Drop this mapping and free the object. A mapping that another attach made
 * stays valid, and a named object stays under its name.
 */
CAT_API void cat_shm_detach(cat_shm_t *shm);

/*
 * Remove the name of a named object. A mapping that already exists stays valid.
 *
 * On Windows the call does nothing: a named section goes away when the last
 * mapping closes, and no API removes the name earlier.
 *
 * cat_false: CAT_ENOENT when no object has the name.
 */
CAT_API cat_bool_t cat_shm_unlink(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* CAT_SHM_H_ */
