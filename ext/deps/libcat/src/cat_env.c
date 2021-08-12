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
    cat_errno_t error;
    char *env = cat_env_get_silent_ex(name, buffer, size, &error);
    if (error != 0) {
        cat_update_last_error_with_reason(error, "Env get \"%s\" failed", name);
    }
    return env;
}

CAT_API char *cat_env_get_silent(const char *name, cat_errno_t *error_ptr)
{
    return cat_env_get_silent_ex(name, NULL, NULL, error_ptr);
}

CAT_API char *cat_env_get_silent_ex(const char *name, char *buffer, size_t *size, cat_errno_t *error_ptr)
{
    char _buffer[CAT_ENV_BUFFER_SIZE];
    size_t *size_ptr, alloc_size = 0;
    int error;

    if (error_ptr != NULL) {
        *error_ptr = 0;
    }
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
#if CAT_ALLOC_HANDLE_ERRORS
            if (unlikely(buffer == NULL)) {
                if (error_ptr != NULL) {
                    *error_ptr = cat_translate_sys_error(cat_sys_errno);
                }
                return NULL;
            }
#endif
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
        if (error_ptr != NULL) {
            *error_ptr = error;
        }
        return NULL;
    }

    if (buffer == _buffer) {
        buffer = cat_strdup(buffer);
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(buffer == NULL)) {
            if (error_ptr != NULL) {
                *error_ptr = cat_translate_sys_error(cat_sys_errno);
            }
            return NULL;
        }
#endif
    }

    return buffer;
}

CAT_API int cat_env_get_i(const char *name, int default_value)
{
    char *env = cat_env_get_silent(name, NULL);

    if (env != NULL) {
        int i = atoi(env);
        cat_free(env);
        return i;
    }

    return default_value;
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

static cat_always_inline int cat_env_peek(const char *name, char buffer[1])
{
    size_t size = 1;
    return uv_os_getenv(name, buffer, &size);
}

CAT_API cat_bool_t cat_env_exists(const char *name)
{
    char buffer[1];
    int error = cat_env_peek(name, buffer);
    return error == CAT_ENOBUFS || error == 0;
}

CAT_API cat_bool_t cat_env_is_empty(const char *name)
{
    char buffer[1];
    int error = cat_env_peek(name, buffer);
    return error != CAT_ENOBUFS && (error != 0 || buffer[0] == '\0');
}

CAT_API cat_bool_t cat_env_compare(const char *name, const char *value, cat_env_comparer_t comparer, cat_bool_t default_value)
{
    const char *values[1] = { value };
    return cat_env_compares(name, values, 1, comparer, default_value);
}

CAT_API cat_bool_t cat_env_compares(const char *name, const char **values, size_t count, cat_env_comparer_t comparer, cat_bool_t default_value)
{
    char *env, buffer[CAT_ENV_BUFFER_SIZE];
    size_t n, size = 0;
    cat_bool_t ret;

    for (n = 0; n < count; n++) {
        size_t length = strlen(values[n]) + 1;
        if (length > size) {
            size = length;
        }
    }

    if (likely(size <= sizeof(buffer))) {
        env = buffer;
        size = sizeof(buffer);
    } else {
        env = (char *) cat_malloc(size);
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(env == NULL)) {
            return default_value;
        }
#endif
    }

    if (uv_os_getenv(name, env, &size) == 0) {
        ret = cat_false;
        for (n = 0; n < count; n++) {
            if (comparer(env, values[n]) == 0) {
                ret = cat_true;
                break;
            }
        }
    } else {
        ret = default_value;
    }

    if (unlikely(env != buffer)) {
        cat_free(env);
    }

    return ret;
}

CAT_API cat_bool_t cat_env_is(const char *name, const char *value, cat_bool_t default_value)
{
    return cat_env_compare(name, value, strcmp, default_value);
}

CAT_API cat_bool_t cat_env_is_true(const char *name, cat_bool_t default_value)
{
    const char *values[3] = { "1", "true", "on" };
    return cat_env_compares(name, values, CAT_ARRAY_SIZE(values), cat_strcasecmp, default_value);
}
