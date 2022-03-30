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

typedef int cat_errno_t;
#define CAT_ERRNO_FMT "%d"
#define CAT_ERRNO_FMT_SPEC "d"

typedef struct cat_error_s {
    cat_errno_t code;
    char *message;
} cat_error_t;

#define CAT_ERRNO_MAP(XX) \
    UV_ERRNO_MAP2(XX) \
    CAT_ERRNO_EXT_MAP(XX) \

#define CAT_ERRNO_EXT_MAP(XX) \
    /* the error has no specific code, we should check the message of error  */ \
    XX(UNCODED, "Please check the error message") \
    /* we should check the previous error */ \
    XX(EPREV, "Please check the previous error") \
    /* must fix your code to prevent from this error */ \
    XX(EMISUSE, "Misuse error") \
    /* illegal value (e.g. pass negative value to unsigned interface) */ \
    XX(EVALUE, "Value error") \
    /* it's different from EBUSY, it always caused by misuse */ \
    XX(ELOCKED, "Resource locked") \
    /* resource is closing (similar to locked) */ \
    XX(ECLOSING, "Resource is closing") \
    /* resource has been closed */ \
    XX(ECLOSED, "Resource has been closed") \
    /* deadlock */ \
    XX(EDEADLK, "Deadlock") \
    /* generic error about ssl */ \
    XX(ESSL, "SSL error") \
    /* no certificate */ \
    XX(ENOCERT, "Certificate not found") \
    /* certificate verify error */ \
    XX(ECERT, "Certificate verify error") \
    /* the process specified by pid does not exist */ \
    XX(ECHILD, "No child processes") \

// workaround for orig_errno()
#define CAT_ERRNO_EXT_ORIG_MAP(XX) \
    XX(EMISUSE, EINVAL) \
    XX(EVALUE, EINVAL) \
    XX(ELOCKED, EBUSY) \
    XX(ECLOSING, EBADF) \
    XX(ECLOSED, EBADF) \
    XX(EDEADLK, EBUSY) \
    XX(ESSL, ECONNRESET) \
    XX(ENOCERT, ECONNRESET) \
    XX(ECERT, ECONNRESET) \
    XX(ECHILD, ECHILD) \

typedef enum uv_errno_ext_e {
    UV__EEXT = -9764,
#define CAT_ERRNO_EXT_GEN(code, unused) UV_ ## code,
    CAT_ERRNO_EXT_MAP(CAT_ERRNO_EXT_GEN)
#undef CAT_ERRNO_EXT_GEN
} uv_errno_ext_t;

typedef enum cat_errnos_e {
#define CAT_ERRNO_GEN(code, unused) CAT_ ## code = UV_ ## code,
    CAT_ERRNO_MAP(CAT_ERRNO_GEN)
#undef CAT_ERRNO_GEN
} cat_errnos_t;

typedef enum cat_ret_e {
    CAT_RET_ERROR    = -1,
    CAT_RET_NONE     = 0,
    CAT_RET_OK       = 1,
} cat_ret_t;

CAT_API cat_errno_t cat_get_last_error_code(void);
CAT_API const char *cat_get_last_error_message(void);
CAT_API void cat_clear_last_error(void);
CAT_API void cat_update_last_error(cat_errno_t code, const char *format, ...) CAT_ATTRIBUTE_FORMAT(printf, 2, 3);
CAT_API void cat_set_last_error_code(cat_errno_t code);
CAT_API void cat_set_last_error(cat_errno_t code, char *message); CAT_INTERNAL
CAT_API void cat_show_last_error(void);
#ifndef CAT_DEBUG
#define cat_show_last_error() __remove_me__
#endif
#ifndef CAT_IDE_HELPER
CAT_API CAT_NORETURN void cat_abort(void);
#else
#define cat_abort abort /* make IDE happy */
#endif

#define cat_update_last_error_with_previous(format, ...) \
       cat_update_last_error(cat_get_last_error_code(), format ", reason: %s", ##__VA_ARGS__, cat_get_last_error_message())

#define cat_update_last_error_of_syscall(format, ...) do { \
       cat_errno_t _error = cat_translate_sys_error(cat_sys_errno); \
       cat_update_last_error(_error, format " (syscall failure " CAT_ERRNO_FMT ": %s)", ##__VA_ARGS__, _error, cat_strerror(_error)); \
} while (0)

#define cat_update_last_error_with_reason(code , format, ...) \
       cat_update_last_error(code, format ", reason: %s", ##__VA_ARGS__, cat_strerror(code))

#define CAT_PROTECT_LAST_ERROR_START() do { \
    cat_error_t *_last_error = &CAT_G(last_error), __last_error; \
    __last_error = *_last_error; /* save */ \
    _last_error->code = 0; /* it is not necessary but we'd better do it */ \
    _last_error->message = NULL; /* prevent free */ \

#define CAT_PROTECT_LAST_ERROR_END() \
    if (unlikely(_last_error->message != NULL)) { \
        cat_free(_last_error->message); /* discard */ \
    } \
    *_last_error = __last_error; /* recover */ \
} while (0)

/* sys error */

#ifndef CAT_OS_WIN
#define cat_sys_errno                  ((cat_errno_t) errno)
#define cat_set_sys_errno(error)       errno = error
#define cat_translate_sys_error(error) ((cat_errno_t) (-(error)))
#else
#define cat_sys_errno                   ((cat_errno_t) WSAGetLastError())
#define cat_set_sys_errno(error)        do { errno = error; WSASetLastError(error); } while (0)
#define cat_translate_sys_error(error)  ((cat_errno_t) uv_translate_sys_error(error))
#endif

CAT_API const char *cat_strerror(cat_errno_t error);
CAT_API int cat_orig_errno(cat_errno_t error);
