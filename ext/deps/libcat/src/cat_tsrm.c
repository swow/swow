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

#include "cat.h"

#if defined(CAT_USE_THREAD_LOCAL)

static _CAT_THREAD_LOCAL cat_bool_t cat_globals_initialized = cat_false;

CAT_API cat_bool_t cat_globals_is_initialized(void)
{
    return cat_globals_initialized;
}

CAT_API void cat_globals_runtime_init(void)
{
    cat_globals_initialized = cat_true;
}

CAT_API void cat_globals_runtime_shutdown(void)
{
    cat_globals_initialized = cat_false;
}

#elif defined(CAT_USE_THREAD_KEY)

CAT_API uv_key_t cat_globals_key;
CAT_API size_t cat_globals_size;

CAT_API cat_bool_t cat_globals_key_available(void)
{
    return uv_key_get(&cat_globals_key) != NULL ? cat_true : cat_false;
}

CAT_API void cat_globals_module_init(void)
{
    int error = uv_key_create(&cat_globals_key);
    if (error != 0) {
        fprintf(stderr, "Globals register failed when creating key, reason: %s", cat_strerror(error));
        abort();
    }
    cat_globals_size = 0;
}

CAT_API void cat_globals_module_shutdown(void)
{
    uv_key_delete(&cat_globals_key);
}

CAT_API void cat_globals_runtime_init(void)
{
    /* Keep the block that this thread already owns.
     * The module accessors are no-ops on the non-thread-safe build, so a runtime
     * re-init must not throw away the state that the other modules set up. */
    if (uv_key_get(&cat_globals_key) != NULL) {
        return;
    }
    void *globals = cat_sys_calloc_unrecoverable(1, cat_globals_size);
    uv_key_set(&cat_globals_key, globals);
}

CAT_API void cat_globals_runtime_shutdown(void)
{
    cat_sys_free(uv_key_get(&cat_globals_key));
    uv_key_set(&cat_globals_key, NULL);
}

CAT_API void cat_globals_register(size_t size, size_t *offset)
{
    *offset = cat_globals_size;
    cat_globals_size += size;
}

CAT_API void cat_globals_unregister(size_t size, size_t *offset)
{
    cat_globals_size -= size;
    *offset = (size_t) -1;
}

#endif
