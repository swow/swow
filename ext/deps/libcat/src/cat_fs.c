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

#include "cat_fs.h"
#include "cat_coroutine.h"
#include "cat_event.h"
#include "cat_time.h"

#include <fcntl.h>

typedef union
{
    cat_coroutine_t *coroutine;
    uv_req_t req;
    uv_fs_t fs;
} cat_fs_context_t;

#define CAT_FS_CREATE_CONTEXT(context, failure) \
    cat_fs_context_t *context = (cat_fs_context_t *) cat_malloc(sizeof(*context)); do { \
    if (unlikely(context == NULL)) { \
        cat_update_last_error_of_syscall("Malloc for file-system context failed"); \
        failure; \
    } \
} while (0)

#define CAT_FS_PREPARE(failure) do { \
    CAT_FS_CREATE_CONTEXT(context, failure); \
    cat_bool_t done; \
    cat_bool_t ret; \
    int error; \
    \
    error = \

#define CAT_FS_CALL(operation, ...) \
    uv_fs_##operation(cat_event_loop, &context->fs, ##__VA_ARGS__, cat_fs_callback);

#define CAT_FS_HANDLE_ERROR(operation) do { \
    if (error != 0) { \
        cat_update_last_error_with_reason(error, "File-System " #operation " init failed"); \
        cat_free(context); \
        return -1; \
    } \
    context->coroutine = CAT_COROUTINE_G(current); \
    ret = cat_time_wait(-1); \
    done = context->coroutine == NULL; \
    context->coroutine = NULL; \
    if (unlikely(!ret)) { \
        cat_update_last_error_with_previous("File-System " #operation " wait failed"); \
        (void) uv_cancel(&context->req); \
        return -1; \
    } \
    if (unlikely(!done)) { \
        cat_update_last_error(CAT_ECANCELED, "File-System " #operation " has been canceled"); \
        (void) uv_cancel(&context->req); \
        return -1; \
    } \
} while (0)

#define CAT_FS_HANDLE_RESULT(operation) \
    CAT_FS_HANDLE_ERROR(operation); \
    if (unlikely(context->fs.result < 0)) { \
        cat_update_last_error_with_reason(context->fs.result, "File-System open failed"); \
        return -1; \
    } \
    return context->fs.result; \
} while (0);

#define CAT_FS_DO_RESULT(operation, ...) do { \
        CAT_FS_PREPARE(return -1) \
        CAT_FS_CALL(operation, ##__VA_ARGS__) \
        CAT_FS_HANDLE_RESULT() \
} while (0)

static void cat_fs_callback(uv_fs_t *fs)
{
    cat_fs_context_t *context = cat_container_of(fs, cat_fs_context_t, fs);

    if (context->coroutine != NULL) {
        cat_coroutine_t *coroutine = context->coroutine;
        context->coroutine = NULL;
        if (unlikely(!cat_coroutine_resume(coroutine, NULL, NULL))) {
            cat_core_error_with_last(DNS, "File-System schedule failed");
        }
    }

    uv_fs_req_cleanup(&context->fs);
    cat_free(context);
}

CAT_API int cat_fs_open(const char *path, int flags, ...)
{
    va_list args;
    int mode = 0666;

    if (flags & O_CREAT) {
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }

    CAT_FS_DO_RESULT(open, path, flags, mode);
}

CAT_API off_t cat_lseek(int fd, off_t offset, int whence)
{
#ifdef CAT_OS_WIN
#define lseek _lseek64
#endif
    return lseek(fd, offset, whence);
}

CAT_API ssize_t cat_fs_read(int fd, void *buffer, size_t size)
{
    uv_buf_t buf = uv_buf_init(buffer, size);

    CAT_FS_DO_RESULT(read, fd, &buf, 1, 0);
}

CAT_API ssize_t cat_fs_write(int fd, const void *buffer, size_t length)
{
    uv_buf_t buf = uv_buf_init((char *) buffer, length);

    CAT_FS_DO_RESULT(write, fd, &buf, 1, 0);
}

CAT_API int cat_fs_close(int fd)
{
    CAT_FS_DO_RESULT(close, fd);
}

CAT_API int cat_fs_access(const char *path, int mode)
{
    CAT_FS_DO_RESULT(access, path, mode);
}

CAT_API int cat_fs_mkdir(const char *path, int mode)
{
    CAT_FS_DO_RESULT(mkdir, path, mode);
}

CAT_API int cat_fs_rmdir(const char *path)
{
    CAT_FS_DO_RESULT(rmdir, path);
}

CAT_API int cat_fs_rename(const char *path, const char *new_path)
{
    CAT_FS_DO_RESULT(rename, path, new_path);
}

CAT_API int cat_fs_unlink(const char *path)
{
    CAT_FS_DO_RESULT(unlink, path);
}
