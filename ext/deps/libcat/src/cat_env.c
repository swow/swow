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

CAT_API char *cat_env_get(const char *name)
{
    return cat_env_get_ex(name, NULL, NULL);
}

CAT_API char *cat_env_get_ex(const char *name, char *buffer, size_t *size)
{
    char _buffer[CAT_ENV_BUFFER_SIZE];
    size_t *size_ptr, alloc_size = 0;
    int error;

    if (buffer == NULL) {
        if (size == NULL || *size == 0) {
            alloc_size = sizeof(_buffer);
            size_ptr = &alloc_size;
        } else {
            alloc_size = *size;
            size_ptr = size;
        }
        if (alloc_size <= sizeof(_buffer)) {
            buffer = _buffer;
        } else {
            _retry:
            buffer = (char *) cat_malloc(alloc_size);
            if (unlikely(buffer == NULL)) {
                cat_update_last_error_of_syscall("Malloc for env failed");
                return NULL;
            }
        }
    } else {
        size_ptr = size;
    }

    error = uv_os_getenv(name, buffer, size_ptr);

    if (error != 0) {
        if (alloc_size != 0 && buffer != _buffer) {
            cat_free(buffer);
        }
        if (error == CAT_ENOBUFS && size_ptr == &alloc_size) {
            goto _retry;
        }
        cat_update_last_error_with_reason(error, "Env get failed");
        return NULL;
    }

    if (buffer == _buffer) {
        buffer = cat_strdup(buffer);
        if (unlikely(buffer == NULL)) {
            cat_update_last_error_of_syscall("Dup for env failed");
            return NULL;
        }
    }

    return buffer;
}

CAT_API cat_bool_t cat_env_set(const char *name, const char *value)
{
    int error;

    error = uv_os_setenv(name, value);

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Env set failed");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_env_unset(const char *name)
{
    int error;

    error = uv_os_unsetenv(name);

    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "Env unset failed");
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_env_exists(const char *name)
{
    char buffer[1];
    size_t size = 1;
    int error;

    error = uv_os_getenv(name, buffer, &size);

    if (error != 0 && error != CAT_ENOBUFS) {
        return cat_false;
    }

    return cat_true;
}

CAT_API cat_bool_t cat_env_is(const char *name, const char *value, cat_bool_t default_value)
{
    char *env, buffer[CAT_ENV_BUFFER_SIZE];
    size_t size = strlen(value) + 1;
    int error;

    if (likely(size <= sizeof(buffer))) {
        env = buffer;
        size = sizeof(buffer);
    } else {
        env = (char *) cat_malloc(size);
        if (unlikely(env == NULL)) {
            return default_value;
        }
    }

    error = uv_os_getenv(name, env, &size);

    if (error == 0) {
        default_value = strcmp(env, value) == 0;
    }

    if (unlikely(env != buffer)) {
        cat_free(env);
    }

    return default_value;
}

CAT_API cat_bool_t cat_env_is_true(const char *name, cat_bool_t default_value)
{
    return cat_env_is(name, "1", default_value);
}
