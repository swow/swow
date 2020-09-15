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

CAT_API char *cat_vsprintf(const char *format, va_list args)
{
    va_list _args;
    size_t size;
    char *string;

    va_copy(_args, args);
    size = vsnprintf(NULL, 0, format, _args) + 1;
    va_end(_args);
    string = (char *) cat_malloc(size);
    if (unlikely(string == NULL)) {
        return NULL;
    }
    if (unlikely(vsnprintf(string, size, format, args) < 0)) {
        cat_free(string);
        return NULL;
    }
    string[size - 1] = '\0';

    return string;
}

CAT_API char *cat_sprintf(const char *format, ...)
{
    va_list args;
    char *string;

    va_start(args, format);
    string = cat_vsprintf(format, args);
    va_end(args);

    return string;
}

CAT_API char *cat_hexprint(const char *data, size_t length)
{
    const size_t width = 5;
    char *buffer;
    size_t size = width * length, offset, offsetx;
    int n;

    buffer = (char *) cat_malloc(size + 1);
    if (unlikely(buffer == NULL)) {
        return NULL;
    }
    for (offset = 0, offsetx = 0; offset < length; offset++)
    {
        n = snprintf(buffer + offsetx, size - offsetx, "0x%02X ", data[offset] & 0xff);
        if (unlikely(n < 0 || (size_t) n != width)) {
            break;
        }
        offsetx += width;
    }
    buffer[size] = '\0';

    return buffer;
}

CAT_API char *cat_srand(char *buffer, size_t count)
{
    buffer = cat_snrand(buffer, count);

    if (likely(buffer != NULL)) {
        buffer[count - 1] = '\0';
    }

    return buffer;
}

CAT_API char *cat_snrand(char *buffer, size_t count)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t n = 0;

    if (buffer == NULL) {
        buffer = (char *) cat_malloc(count);
        if (unlikely(buffer == NULL)) {
            return NULL;
        }
    }

    for (; n < count; n++) {
        int key = rand() % (int) (sizeof charset - 1);
        buffer[n] = charset[key];
    }

    return buffer;
}
