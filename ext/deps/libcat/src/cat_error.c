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
    if (message == NULL) {
        cat_errno_t code = cat_get_last_error_code();
        return code == 0 ? "" : cat_strerror(code);
    }
    return message;
}

CAT_API void cat_clear_last_error(void)
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

    if (format == NULL) {
        message = NULL;
    } else {
        /* Notice: new message maybe relying on the previous message */
        va_start(args, format);
        message = cat_vsprintf(format, args);
        va_end(args);
        if (unlikely(message == NULL)) {
            fprintf(CAT_G(error_log), "Sprintf last error message failed" CAT_EOL);
            return;
        }
    }

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
    if (CAT_G(show_last_error)) {
#undef cat_show_last_error
        cat_show_last_error();
    }
}

CAT_API void cat_show_last_error(void)
{
    CAT_LOG_INFO(
        CORE,  "Last-error: code=" CAT_ERRNO_FMT ", message=%s",
        cat_get_last_error_code(), cat_get_last_error_message()
    );
}

#ifndef CAT_IDE_HELPER
CAT_API CAT_NORETURN void cat_abort(void)
{
    exit(233);
}
#endif

/* sys error (uv style) */

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
    return "Unknown error";
}

#ifndef E2BIG
#define E2BIG 7
#endif
#ifndef EACCES
#define EACCES 13
#endif
#ifndef EADDRINUSE
#define EADDRINUSE 98
#endif
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL 99
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT 97
#endif
#ifndef EAGAIN
#define EAGAIN 11
#endif
#ifndef EALREADY
#define EALREADY 114
#endif
#ifndef EBADF
#define EBADF 9
#endif
#ifndef ECHILD
#define ECHILD 10
#endif
#ifndef EBUSY
#define EBUSY 16
#endif
#ifndef ECANCELED
#define ECANCELED 125
#endif
#ifndef ECHARSET
#define ECHARSET 84
#endif
#ifndef ECONNABORTED
#define ECONNABORTED 103
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED 111
#endif
#ifndef ECONNRESET
#define ECONNRESET 104
#endif
#ifndef EDESTADDRREQ
#define EDESTADDRREQ 89
#endif
#ifndef EEXIST
#define EEXIST 17
#endif
#ifndef EFAULT
#define EFAULT 14
#endif
#ifndef EFBIG
#define EFBIG 27
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH 113
#endif
#ifndef EINTR
#define EINTR 4
#endif
#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EIO
#define EIO 5
#endif
#ifndef EISCONN
#define EISCONN 106
#endif
#ifndef EISDIR
#define EISDIR 21
#endif
#ifndef ELOOP
#define ELOOP 40
#endif
#ifndef EMFILE
#define EMFILE 24
#endif
#ifndef EMSGSIZE
#define EMSGSIZE 90
#endif
#ifndef ENAMETOOLONG
#define ENAMETOOLONG 36
#endif
#ifndef ENETDOWN
#define ENETDOWN 100
#endif
#ifndef ENETUNREACH
#define ENETUNREACH 101
#endif
#ifndef ENFILE
#define ENFILE 23
#endif
#ifndef ENOBUFS
#define ENOBUFS 105
#endif
#ifndef ENODEV
#define ENODEV 19
#endif
#ifndef ENOENT
#define ENOENT 2
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif
#ifndef ENONET
#define ENONET 64
#endif
#ifndef ENOPROTOOPT
#define ENOPROTOOPT 92
#endif
#ifndef ENOSPC
#define ENOSPC 28
#endif
#ifndef ENOSYS
#define ENOSYS 38
#endif
#ifndef ENOTCONN
#define ENOTCONN 107
#endif
#ifndef ENOTDIR
#define ENOTDIR 20
#endif
#ifndef ENOTEMPTY
#define ENOTEMPTY 39
#endif
#ifndef ENOTSOCK
#define ENOTSOCK 88
#endif
#ifndef ENOTSUP
#define ENOTSUP 95
#endif
#ifndef EPERM
#define EPERM 1
#endif
#ifndef EPIPE
#define EPIPE 32
#endif
#ifndef EPROTO
#define EPROTO 71
#endif
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT 93
#endif
#ifndef EPROTOTYPE
#define EPROTOTYPE 91
#endif
#ifndef ERANGE
#define ERANGE 34
#endif
#ifndef EROFS
#define EROFS 30
#endif
#ifndef ESHUTDOWN
#define ESHUTDOWN 108
#endif
#ifndef ESPIPE
#define ESPIPE 29
#endif
#ifndef ESRCH
#define ESRCH 3
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif
#ifndef ETXTBSY
#define ETXTBSY 26
#endif
#ifndef EXDEV
#define EXDEV 18
#endif
#ifndef ENXIO
#define ENXIO 6
#endif
#ifndef EMLINK
#define EMLINK 31
#endif
#ifndef EHOSTDOWN
#define EHOSTDOWN 112
#endif
#ifndef EREMOTEIO
#define EREMOTEIO 121
#endif
#ifndef ENOTTY
#define ENOTTY 25
#endif
#ifndef EILSEQ
#define EILSEQ 84
#endif
#ifndef EUNKNOWN
#define EUNKNOWN -1
#endif

#define ORIG_ERRNO_MAP(E) \
    E(EAGAIN) \
    E(ECONNABORTED) \
    E(ECONNREFUSED) \
    E(ECONNRESET) \
    E(EIO) \
    E(EPERM) \
    E(EPIPE) \
    E(EACCES) \
    E(E2BIG) \
    E(EADDRINUSE) \
    E(EADDRNOTAVAIL) \
    E(EAFNOSUPPORT) \
    E(EALREADY) \
    E(EBADF) \
    E(EBUSY) \
    E(ECANCELED) \
    E(ECHARSET) \
    E(EDESTADDRREQ) \
    E(EEXIST) \
    E(EFAULT) \
    E(EFBIG) \
    E(EHOSTUNREACH) \
    E(EINTR) \
    E(EINVAL) \
    E(EISCONN) \
    E(EISDIR) \
    E(ELOOP) \
    E(EMFILE) \
    E(EMSGSIZE) \
    E(ENAMETOOLONG) \
    E(ENETDOWN) \
    E(ENETUNREACH) \
    E(ENFILE) \
    E(ENOBUFS) \
    E(ENODEV) \
    E(ENOENT) \
    E(ENOMEM) \
    E(ENONET) \
    E(ENOPROTOOPT) \
    E(ENOSPC) \
    E(ENOSYS) \
    E(ENOTCONN) \
    E(ENOTDIR) \
    E(ENOTEMPTY) \
    E(ENOTSOCK) \
    E(ENOTSUP) \
    E(EPROTO) \
    E(EPROTONOSUPPORT) \
    E(EPROTOTYPE) \
    E(ERANGE) \
    E(EROFS) \
    E(ESHUTDOWN) \
    E(ESPIPE) \
    E(ESRCH) \
    E(ETIMEDOUT) \
    E(ETXTBSY) \
    E(EXDEV) \
    E(ENXIO) \
    E(EMLINK) \
    E(EHOSTDOWN) \
    E(EREMOTEIO) \
    E(ENOTTY) \
    E(EILSEQ)

// shall we make separate posix errno code ?
/* return original posix errno from uv errno */
CAT_API int cat_orig_errno(cat_errno_t error)
{
#define CAT_ERROR_GEN(name) \
    if (error == UV_ ## name) { \
        return name; \
    } else
    ORIG_ERRNO_MAP(CAT_ERROR_GEN)
#undef CAT_ERROR_GEN

#define CAT_ERROR_GEN(a, b) \
    if (error == CAT_##a) { \
        return b; \
    } else
    CAT_ERRNO_EXT_ORIG_MAP(CAT_ERROR_GEN)
#undef CAT_ERROR_GEN

    return EUNKNOWN;
}

#ifdef CAT_OS_WIN
/* return uv errno from posix errno */
/* why so WET */
cat_errno_t cat_translate_unix_error(int error) {
#define CAT_ERROR_GEN(name) \
    if (error == name){ \
        return CAT_##name; \
    } else

    ORIG_ERRNO_MAP(CAT_ERROR_GEN)
#undef CAT_ERROR_GEN

    return CAT_UNCODED;
}
#endif
