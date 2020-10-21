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

CAT_API cat_errno_t cat_get_last_error_code(void)
{
    return CAT_G(last_error).code;
}

CAT_API const char *cat_get_last_error_message(void)
{
    const char *message = CAT_G(last_error).message;
    return message ? message : "";
}

CAT_API CAT_DESTRUCTOR void cat_clear_last_error(void)
{
    cat_error_t *last_error = &CAT_G(last_error);
    last_error->code = 0;
    if (last_error->message != NULL) {
        cat_free(last_error->message);
        last_error->message = NULL;
    }
}

CAT_API void cat_update_last_error(cat_errno_t code, const char *format, ...)
{
    va_list args;
    char *message;

    /* Notice: new message maybe relying on the previous message */
    va_start(args, format);
    message = cat_vsprintf(format, args);
    if (unlikely(message == NULL)) {
        fprintf(stderr, "Sprintf last error message failed" CAT_EOL);
        return;
    }
    va_end(args);

    cat_set_last_error(code, message);
}

CAT_API void cat_set_last_error_code(cat_errno_t code)
{
    CAT_G(last_error).code = code;
}

CAT_API void cat_set_last_error(cat_errno_t code, char *message)
{
    cat_error_t *last_error = &CAT_G(last_error);
    char *last_error_message = last_error->message;

    last_error->code = code;
    last_error->message = message;
    if (last_error_message != NULL && last_error_message != message) {
        cat_free(last_error_message);
    }
#ifdef CAT_DEBUG
    if (CAT_G(show_last_error)) {
        cat_show_last_error();
    }
#endif
}

#ifdef CAT_DEBUG
CAT_API void cat_show_last_error(void)
{
    cat_info(
        CORE,  "Last-error: code=" CAT_ERRNO_FMT ", message=%s",
        cat_get_last_error_code(), cat_get_last_error_message()
    );
}
#endif

CAT_API CAT_NORETURN void cat_abort(void)
{
    exit(233);
}

/* sys error */

CAT_API const char *cat_strerror(cat_errno_t error)
{
    if (error > 0) {
        error = cat_translate_sys_error(error);
    }
#define CAT_STRERROR_GEN(name, msg) case UV_ ## name: return msg;
    switch (error) {
        CAT_ERRNO_MAP(CAT_STRERROR_GEN)
    }
#undef CAT_STRERROR_GEN
    return "unknown error";
}
