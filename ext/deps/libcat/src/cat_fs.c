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
  | Author: dixyes <dixyes@gmail.com>                                        |
  |         Twosee <twosee@php.net>                                          |
  +--------------------------------------------------------------------------+
 */

#include "cat_fs.h"
#include "cat_coroutine.h"
#include "cat_event.h"
#include "cat_time.h"
#include "cat_work.h"
#include "cat_async.h"
#ifdef CAT_OS_WIN
# include <winternl.h>
#endif // CAT_OS_WIN

#ifdef CAT_OS_WIN
# ifdef _WIN64
#  define fseeko _fseeki64
#  define ftello _ftelli64
# else
#  define fseeko fseek
#  define ftello ftell
# endif // _WIN64
#endif // CAT_OS_WIN

typedef enum cat_fs_error_type_e {
    CAT_FS_ERROR_NONE = 0, // no error
    CAT_FS_ERROR_ERRNO, // is errno
    CAT_FS_ERROR_CAT_ERRNO, // is cat_errno_t
#ifdef CAT_OS_WIN
    CAT_FS_ERROR_WIN32, // is GetLastError() result
    // CAT_FS_ERROR_NT // is NTSTATUS, not implemented yet
#endif // CAT_OS_WIN
} cat_fs_error_type_t;

typedef enum cat_fs_freer_type_e {
    CAT_FS_FREER_NONE = 0, // do not free
    CAT_FS_FREER_FREE, // free(x)
    CAT_FS_FREER_CAT_FREE, // cat_free(x)
#ifdef CAT_OS_WIN
    CAT_FS_FREER_LOCAL_FREE, // LocalFree(x)
    CAT_FS_FREER_HEAP_FREE, // HeapFree(GetProcessHeap(), 0, x)
#endif // CAT_OS_WIN
} cat_fs_freer_type_t;

typedef struct cat_fs_error_s {
    cat_fs_error_type_t type;
    union {
        int error;
        cat_errno_t cat_errno;
#ifdef CAT_OS_WIN
        DWORD le;
        // NTSTATUS nt;
#endif // CAT_OS_WIN
    } val;
    cat_fs_freer_type_t msg_free;
    const char *msg;
} cat_fs_error_t;

typedef union {
    cat_coroutine_t *coroutine;
    uv_req_t req;
    uv_fs_t fs;
} cat_fs_context_t;

static cat_bool_t cat_fs_do_result(cat_fs_context_t *context, int error, const char *operation)
{
    cat_bool_t done;
    cat_bool_t ret;

    if (error != 0) {
        cat_update_last_error_with_reason(error, "File-System %s init failed", operation);
        errno = cat_orig_errno(cat_get_last_error_code());
        CAT_LOG_DEBUG(FS, "Failed uv_fs_%s context=%p, uv_errno=%d", operation, context, error);
        cat_free(context);
        return cat_false;
    }
    context->coroutine = CAT_COROUTINE_G(current);
    ret = cat_time_wait(CAT_TIMEOUT_FOREVER);
    done = context->coroutine == NULL;
    context->coroutine = NULL;
    if (unlikely(!ret)) {
        cat_update_last_error_with_previous("File-System %s wait failed", operation);
        (void) uv_cancel(&context->req);
        errno = cat_orig_errno(cat_get_last_error_code());
        CAT_LOG_DEBUG(FS, "Failed %s() context=%p waiting failed", operation, context);
        return cat_false;
    }
    if (unlikely(!done)) {
        cat_update_last_error(CAT_ECANCELED, "File-System %s has been canceled", operation);
        (void) uv_cancel(&context->req);
        errno = ECANCELED;
        CAT_LOG_DEBUG(FS, "Failed %s() context=%p canceled", operation, context);
        return cat_false;
    }
    if (unlikely(context->fs.result < 0)) {
        cat_update_last_error_with_reason((cat_errno_t) context->fs.result, "File-System %s failed", operation);
        errno = cat_orig_errno((cat_errno_t) context->fs.result);
        CAT_LOG_DEBUG(FS, "Failed %s() context=%p, uv_errno=%d", operation, context, (int) context->fs.result);
        return cat_false;
    }

    CAT_LOG_DEBUG(FS, "Done %s() context=%p", operation, context);
    return cat_true;
}

#define CAT_FS_DO_RESULT_EX(on_fail, on_done, operation, ...) do { \
    cat_fs_context_t *context = (cat_fs_context_t *) cat_malloc(sizeof(*context)); \
    if (unlikely(context == NULL)) { \
        cat_update_last_error_of_syscall("Malloc for file-system context failed"); \
        errno = ENOMEM; \
        {on_fail} \
    } \
    CAT_LOG_DEBUG(FS, "Start " #operation "() context=%p", context); \
    int error = uv_fs_##operation(&CAT_EVENT_G(loop), &context->fs, ##__VA_ARGS__, cat_fs_callback); \
    if (!cat_fs_do_result(context, error, #operation)) { \
        {on_fail} \
    } \
    {on_done} \
} while (0)

#define CAT_FS_DO_RESULT(return_type, operation, ...) \
        CAT_FS_DO_RESULT_EX({return -1;}, {return (return_type) context->fs.result;}, operation, __VA_ARGS__)

static void cat_fs_callback(uv_fs_t *fs)
{
    cat_fs_context_t *context = cat_container_of(fs, cat_fs_context_t, fs);

    if (context->coroutine != NULL) {
        cat_coroutine_t *coroutine = context->coroutine;
        context->coroutine = NULL;
        cat_coroutine_schedule(coroutine, FS, "File-System");
    }

    uv_fs_req_cleanup(&context->fs);
    cat_free(context);
}

#ifdef CAT_OS_WIN
# define wrappath(_path, path) \
char path##buf[(32767/*hard limit*/ + 4/* \\?\ */ + 1/* \0 */)*sizeof(wchar_t)] = {'\\', '\\', '?', '\\'}; \
const char *path = NULL; \
do { \
    if (!_path) { \
        path = NULL; \
        break; \
    } \
    const size_t lenpath = cat_strnlen(_path, 32767); \
    if ( \
        !( /* not  \\?\-ed */ \
            '\\' == _path[0] && \
            '\\' == _path[1] && \
            '?' == _path[2] && \
            '\\' == _path[3] \
        ) && \
        lenpath >= MAX_PATH - 12 && /* longer than 260(MAX_PATH) - 12(8.3 filename) */ \
        lenpath < sizeof(path##buf) - 4 - 1 /* shorter than hard limit*/ \
    ) { \
        /* fix it: prepend "\\?\" */ \
        memcpy(&path##buf[4], _path, lenpath + 1/*\0*/); \
        path = path##buf; \
    } else { \
        path = _path; \
    } \
} while (0)
#else
# define wrappath(_path, path) const char *path = _path
#endif // CAT_OS_WIN

// basic functions for fs io
// open, close, read, write

CAT_API cat_file_t cat_fs_open(const char *_path, int flags, ...)
{
    va_list args;
    int mode = 0666;

    if (flags & O_CREAT) {
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }

    wrappath(_path, path);

    CAT_FS_DO_RESULT(cat_file_t, open, path, flags, mode);
}

CAT_API ssize_t cat_fs_pread(cat_file_t fd, void *buffer, size_t size, off_t offset)
{
    uv_buf_t buf = uv_buf_init((char *) buffer, (unsigned int) size);

    CAT_FS_DO_RESULT(ssize_t, read, fd, &buf, 1, offset);
}

CAT_API ssize_t cat_fs_pwrite(cat_file_t fd, const void *buffer, size_t length, off_t offset)
{
    uv_buf_t buf = uv_buf_init((char *) buffer, (unsigned int) length);

    CAT_FS_DO_RESULT(ssize_t, write, fd, &buf, 1, offset);
}

CAT_API int cat_fs_close(cat_file_t fd)
{
    CAT_FS_DO_RESULT(int, close, fd);
}

CAT_API int cat_fs_fsync(cat_file_t fd)
{
    CAT_FS_DO_RESULT(int, fsync, fd);
}

CAT_API int cat_fs_fdatasync(cat_file_t fd)
{
    CAT_FS_DO_RESULT(int, fdatasync, fd);
}

CAT_API int cat_fs_ftruncate(cat_file_t fd, int64_t offset)
{
    CAT_FS_DO_RESULT(int, ftruncate, fd, offset);
}

// basic dir operations
// opendir, readdir, closedir, scandir
#ifndef CAT_OS_WIN
/*
* cat_fs_readdirs: like readdir(3), but with multi entries
* Note: you should do free(dir->dirents[x].name), free(dir->dirents[x]) and free(dir->dirents)
*/
/*
static int cat_fs_readdirs(cat_dir_t *dir, uv_dirent_t *dirents, size_t nentries)
{
    ((uv_dir_t *) dir)->dirents = dirents;
    ((uv_dir_t *) dir)->nentries = nentries;
    CAT_FS_DO_RESULT_EX({return -1;}, {
        // we donot duplicate names, that's hacky
        // better duplicate it, then uv__free original, then return our duplication
        // however we donot have uv__free

        // clean up dir struct to avoid uv's freeing names
        ((uv_dir_t *) dir)->dirents = NULL;
        ((uv_dir_t *) dir)->nentries = 0;
        int ret = (int) context->fs.result;
        context->fs.result = 0;
        return ret;
    }, readdir, dir);
}
*/
#endif // CAT_OS_WIN

static uv_fs_t *cat_fs_uv_scandir(const char *path, int flags)
{
    CAT_FS_DO_RESULT_EX({return NULL;}, {
        return &context->fs;
    }, scandir, path, flags/* no documents/source code coments refer to this, what is this ?*/);
}

/*
* cat_fs_scandir: like scandir(3), but with cat_dirent_t
* Note: you should do free(namelist[x].name), free(namelist[x]) and free(namelist)
*/
CAT_API int cat_fs_scandir(const char *path, cat_dirent_t **namelist,
  int (*filter)(const cat_dirent_t *),
  int (*compar)(const cat_dirent_t *, const cat_dirent_t *)) {

    uv_fs_t *req = NULL;
    if (!(req = cat_fs_uv_scandir(path, 0))) {
        // failed scandir
        return -1;
    }

    int ret = -1;
    cat_dirent_t dirent, *tmp = NULL;
    int cnt = 0, len = 0;

    while ((ret = uv_fs_scandir_next(req, &dirent)) == 0) {
        if (filter && !filter(&dirent)) {
            continue;
        }
        if (cnt >= len) {
            len = 2*len+1;
            void *_tmp = realloc(tmp, len * sizeof(*tmp));
            if (!_tmp) {
                cat_update_last_error(CAT_ENOMEM, "Cannot allocate memory");
                errno = ENOMEM;
                for (cnt--; cnt >= 0; cnt--) {
                    free((void*) tmp[cnt].name);
                }
                free(tmp);
                return -1;
            }
            tmp = _tmp;
        }
        tmp[cnt].name = cat_sys_strdup(dirent.name);
        // printf("%s: %p\n", tmp[cnt].name, tmp[cnt].name);
        tmp[cnt++].type = dirent.type;
    }

    if (compar) {
        qsort(tmp, cnt, sizeof(*tmp), (int (*)(const void *, const void *))compar);
    }
    *namelist = tmp;
    return cnt;
}

// directory/file operations
// mkdir, rmdir, rename, unlink

CAT_API int cat_fs_mkdir(const char *_path, int mode)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT(int, mkdir, path, mode);
}

CAT_API int cat_fs_rmdir(const char *_path)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT(int, rmdir, path);
}

CAT_API int cat_fs_rename(const char *_path, const char *_new_path)
{
    wrappath(_path, path);
    wrappath(_new_path, new_path);
    CAT_FS_DO_RESULT(int, rename, path, new_path);
}

CAT_API int cat_fs_unlink(const char *_path)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT(int, unlink, path);
}

// file info utils
// access, stat(s), utime(s)

CAT_API int cat_fs_access(const char *_path, int mode)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT(int, access, path, mode);
}

#define CAT_FS_DO_STAT(name, target) \
    CAT_FS_DO_RESULT_EX({return -1;}, {memcpy(buf, &context->fs.statbuf, sizeof(uv_stat_t)); return 0;}, name, target)

CAT_API int cat_fs_stat(const char *_path, cat_stat_t *buf)
{
    wrappath(_path, path);
    CAT_FS_DO_STAT(stat, path);
}

CAT_API int cat_fs_lstat(const char *_path, cat_stat_t *buf)
{
    wrappath(_path, path);
    CAT_FS_DO_STAT(lstat, path);
}

CAT_API int cat_fs_fstat(cat_file_t fd, cat_stat_t *buf)
{
    CAT_FS_DO_STAT(fstat, fd);
}

CAT_API int cat_fs_utime(const char *_path, double atime, double mtime)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT(int, utime, path, atime, mtime);
}

CAT_API int cat_fs_lutime(const char *_path, double atime, double mtime)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT(int, lutime, path, atime, mtime);
}

CAT_API int cat_fs_futime(cat_file_t fd, double atime, double mtime)
{
    CAT_FS_DO_RESULT(int, futime, fd, atime, mtime);
}

// hard link and symbol link
// link, symlink, readlink, realpath

CAT_API int cat_fs_link(const char *_path, const char *_new_path)
{
    wrappath(_path, path);
    wrappath(_new_path, new_path);
    CAT_FS_DO_RESULT(int, link, path, new_path);
}

CAT_API int cat_fs_symlink(const char *_path, const char *_new_path, int flags)
{
    wrappath(_path, path);
    wrappath(_new_path, new_path);
    CAT_FS_DO_RESULT(int, symlink, path, new_path, flags);
}

#ifdef CAT_OS_WIN
# define PATH_MAX 32768
#endif // CAT_OS_WIN
CAT_API int cat_fs_readlink(const char *_path, char *buf, size_t len)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT_EX({return (int)-1;}, {
        size_t ret = cat_strnlen(context->fs.ptr, PATH_MAX);
        if (ret > len) {
            // will truncate
            ret = len;
        }
        strncpy(buf, context->fs.ptr, len);
        return (int) ret;
    }, readlink, path);
}

CAT_API char *cat_fs_realpath(const char *_path, char *buf)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT_EX({return NULL;}, {
        if (NULL == buf) {
            return cat_sys_strdup(context->fs.ptr);
        }
        strcpy(buf, context->fs.ptr);
        return buf;
    }, realpath, path);
}
#ifdef CAT_OS_WIN
# undef PATH_MAX
#endif // CAT_OS_WIN

// permissions
// chmod(s), chown(s)

CAT_API int cat_fs_chmod(const char *_path, int mode)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT(int, chmod, path, mode);
}

CAT_API int cat_fs_fchmod(cat_file_t fd, int mode)
{
    CAT_FS_DO_RESULT(int, fchmod, fd, mode);
}

CAT_API int cat_fs_chown(const char *_path, cat_uid_t uid, cat_gid_t gid)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT(int, chown, path, uid, gid);
}

CAT_API int cat_fs_lchown(const char *_path, cat_uid_t uid, cat_gid_t gid)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT(int, lchown, path, uid, gid);
}

CAT_API int cat_fs_fchown(cat_file_t fd, cat_uid_t uid, cat_gid_t gid)
{
    CAT_FS_DO_RESULT(int, fchown, fd, uid, gid);
}

// miscellaneous
// copyfile
CAT_API int cat_fs_copyfile(const char *_path, const char *_new_path, int flags)
{
    wrappath(_path, path);
    wrappath(_new_path, new_path);
    CAT_FS_DO_RESULT(int, copyfile, path, new_path, flags);
}

CAT_API int cat_fs_sendfile(cat_file_t out_fd, cat_file_t in_fd, int64_t in_offset, size_t length)
{
    CAT_FS_DO_RESULT(int, sendfile, out_fd, in_fd, in_offset, length);
}

/*
* cat_fs_mkdtemp - like mkdtemp(3) but the returned path is not same as inputed template
*/
CAT_API const char *cat_fs_mkdtemp(const char *_tpl)
{
    wrappath(_tpl, tpl);
    CAT_FS_DO_RESULT_EX({return NULL;}, {
        if (0 != context->fs.result) {
            return NULL;
        }
        return context->fs.path;
    }, mkdtemp, tpl);
}

CAT_API int cat_fs_mkstemp(const char *_tpl)
{
    wrappath(_tpl, tpl);
    CAT_FS_DO_RESULT_EX({return -1;}, {
        return (int) context->fs.result;
    }, mkstemp, tpl);
}

CAT_API int cat_fs_statfs(const char *_path, cat_statfs_t *buf)
{
    wrappath(_path, path);
    CAT_FS_DO_RESULT_EX({return -1;}, {
        memcpy(buf, context->fs.ptr, sizeof(*buf));
        return 0;
    }, statfs, path);
}

// cat_work wrapped fs functions
typedef struct cat_fs_work_ret_s {
    cat_fs_error_t error;
    union {
        signed long long int num;
        void *ptr;
#ifdef CAT_OS_WIN
        HANDLE handle;
#endif // CAT_OS_WIN
    } ret;
} cat_fs_work_ret_t;

// so nasty, but i have no clear way to do this
cat_errno_t cat_translate_unix_error(int error);
/*
* set error code(GetLastError/errno), then return cat_errno_t
*/
static inline cat_errno_t cat_fs_set_error_code(cat_fs_error_t *e)
{
    switch (e->type) {
#ifdef CAT_OS_WIN
        case CAT_FS_ERROR_WIN32:
            SetLastError(e->val.le);
            return cat_translate_sys_error(e->val.le);
        case CAT_FS_ERROR_ERRNO:
            errno = e->val.error;
            return cat_translate_unix_error(errno);
#else
        case CAT_FS_ERROR_ERRNO:
            errno = e->val.error;
            return cat_translate_sys_error(errno);
#endif // CAT_OS_WIN
        case CAT_FS_ERROR_CAT_ERRNO:
            errno = cat_orig_errno(e->val.cat_errno);
            return e->val.cat_errno;
        case CAT_FS_ERROR_NONE:
        default:
            // never here
            CAT_NEVER_HERE("Strange error type");
    }
}

#ifdef CAT_OS_WIN
static LPCWSTR cat_fs_mbs2wcs(const char *mbs)
{
    DWORD bufsiz = MultiByteToWideChar(
        CP_UTF8, // code page
        0, // flags
        mbs, // src
        -1, // srclen: nul-terminated
        NULL, // dest: no output
        0 // destlen: nope
    );
    LPWSTR wcs = HeapAlloc(GetProcessHeap(), 0, bufsiz * sizeof(WCHAR));
    if (NULL == wcs) {
        return wcs;
    }
    DWORD r = MultiByteToWideChar(
        CP_UTF8, // code page
        0, // flags
        mbs, // src
        -1, // srclen: nul-terminated
        wcs, // dest
        bufsiz
    );
    wcs[bufsiz-1] = 0;
    assert(r == bufsiz); // impossible
    return (LPCWSTR) wcs;
}

static const char *cat_fs_wcs2mbs(LPCWSTR wcs)
{
    DWORD bufsiz = WideCharToMultiByte(
        CP_UTF8, // code page
        0, // flags
        wcs, // src
        -1, // srclen: nul-terminated
        NULL, // dest: no output
        0, // destlen: nope
        NULL, // default char
        FALSE // use default char
    );
    char *mbs = HeapAlloc(GetProcessHeap(), 0, bufsiz * sizeof(char));
    if (NULL == mbs) {
        return mbs;
    }
    DWORD r = WideCharToMultiByte(
        CP_UTF8, // code page
        0, // flags
        wcs, // src
        -1, // srclen: nul-terminated
        mbs, // dest: no output
        bufsiz, // destlen: nope
        NULL, // default char
        FALSE // use default char
    );
    mbs[bufsiz-1] = 0;
    assert(r == bufsiz); // impossible
    return (const char *) mbs;
}

static const char *cat_fs_win_strerror(DWORD le)
{
    LPVOID temp_msg;
    // DWORD len_msg =
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, // flags
        NULL, // source
        le, // messageid: last error
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // language id
        (LPWSTR)&temp_msg, // buffer
        0, // bufsiz: 0
        NULL // va_args
    );
    const char *msg = cat_fs_wcs2mbs((LPWSTR) temp_msg);
    LocalFree(temp_msg);
    return msg;
}

#endif // CAT_OS_WIN

static inline const char *cat_fs_error_msg(cat_fs_error_t *e)
{
    if (e->msg) {
        return e->msg;
    }
    switch (e->type) {
        case CAT_FS_ERROR_NONE:
            e->msg_free = CAT_FS_FREER_NONE;
            e->msg = "";
            break;
#ifdef CAT_OS_WIN
        case CAT_FS_ERROR_WIN32:
            e->msg_free = CAT_FS_FREER_HEAP_FREE;
            e->msg = cat_fs_win_strerror(e->val.le);
            break;
#endif // CAT_OS_WIN
        case CAT_FS_ERROR_ERRNO:
            e->msg_free = CAT_FS_FREER_NONE;
            e->msg = strerror(e->val.error);
            break;
        case CAT_FS_ERROR_CAT_ERRNO:
            e->msg_free = CAT_FS_FREER_NONE;
            e->msg = strerror(cat_orig_errno(e->val.cat_errno));
            break;
        default:
            CAT_NEVER_HERE("Strange error type");
    }
    return e->msg;
}

static inline void cat_fs_error_msg_free(cat_fs_error_t *e)
{
    assert(e->msg);
    switch (e->msg_free) {
        case CAT_FS_FREER_NONE:
            e->msg = NULL;
            return;
#ifdef CAT_OS_WIN
        case CAT_FS_FREER_HEAP_FREE:
            HeapFree(GetProcessHeap(), 0, (LPVOID) e->msg);
            return;
#endif // CAT_OS_WIN
        case CAT_FS_FREER_CAT_FREE:
            cat_free((void*) e->msg);
            return;
        case CAT_FS_FREER_FREE:
            free((void*) e->msg);
            return;
#ifdef CAT_OS_WIN
        case CAT_FS_FREER_LOCAL_FREE:
            // not implemented
#endif // CAT_OS_WIN
        default:
            return;
    }
}

// if error occured, this macro will set error
#define cat_fs_work_check_error(error, fmt) do { \
    cat_fs_error_t *_error = error; \
    if (_error->type != CAT_FS_ERROR_NONE) { \
        cat_fs_work_error(_error, "File-System " fmt " failed: %s"); \
    } \
} while (0)

static CAT_COLD void cat_fs_work_error(cat_fs_error_t *error, const char *fmt)
{
    cat_errno_t cat_errno = cat_fs_set_error_code(error);
    const char *msg = cat_fs_error_msg(error);

    cat_update_last_error(cat_errno, fmt, msg);
    cat_fs_error_msg_free(error);
}

#ifndef CAT_OS_WIN
typedef size_t cat_fs_read_size_t;
typedef size_t cat_fs_write_size_t;
#else
typedef unsigned int cat_fs_read_size_t;
typedef unsigned int cat_fs_write_size_t;
#endif // CAT_OS_WIN

typedef struct cat_fs_read_data_s {
    cat_fs_work_ret_t ret;
    int fd;
    void *buf;
    cat_fs_read_size_t size;
} cat_fs_read_data_t;

static void cat_fs_read_cb(cat_data_t *ptr)
{
    cat_fs_read_data_t *data = (cat_fs_read_data_t *) ptr;
    data->ret.ret.num = read(data->fd, data->buf, data->size);
    if (0 > data->ret.ret.num) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
    }
}

CAT_API ssize_t cat_fs_read(cat_file_t fd, void *buf, size_t size)
{
    cat_fs_read_data_t *data = (cat_fs_read_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs read failed");
        return -1;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->fd = fd;
    data->buf = buf;
    data->size = (cat_fs_read_size_t)size;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_read_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
        return -1;
    }
    cat_fs_work_check_error(&data->ret.error, "read");
    return data->ret.ret.num;
}

typedef struct cat_fs_write_data_s {
    cat_fs_work_ret_t ret;
    int fd;
    const void* buf;
    cat_fs_write_size_t length;
} cat_fs_write_data_t;

static void cat_fs_write_cb(cat_data_t *ptr)
{
    cat_fs_write_data_t *data = (cat_fs_write_data_t *) ptr;
    data->ret.ret.num = (ssize_t) write(data->fd, data->buf, data->length);
    if (0 > data->ret.ret.num) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
    }
}

CAT_API ssize_t cat_fs_write(cat_file_t fd, const void *buf, size_t length)
{
    cat_fs_write_data_t *data = (cat_fs_write_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs write failed");
        return -1;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->fd = fd;
    data->buf = buf;
    data->length = (cat_fs_write_size_t)length;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_write_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
        return -1;
    }
    cat_fs_work_check_error(&data->ret.error, "write");
    return (ssize_t) data->ret.ret.num;
}

typedef struct cat_fs_lseek_data_s {
    cat_fs_work_ret_t ret;
    int fd;
    off_t offset;
    int whence;
} cat_fs_lseek_data_t;

static void cat_fs_lseek_cb(cat_data_t *ptr)
{
    cat_fs_lseek_data_t *data = (cat_fs_lseek_data_t *) ptr;
    data->ret.ret.num = (ssize_t) lseek(data->fd, data->offset, data->whence);
    if (0 > data->ret.ret.num) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
    }
}

CAT_API off_t cat_fs_lseek(cat_file_t fd, off_t offset, int whence)
{
    cat_fs_lseek_data_t *data = (cat_fs_lseek_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs lseek failed");
        return -1;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->fd = fd;
    data->offset = offset;
    data->whence = whence;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_lseek_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
        return -1;
    }
    cat_fs_work_check_error(&data->ret.error, "lseek");
    return (off_t) data->ret.ret.num;
}

// TODO: fopen wrapper

typedef struct cat_fs_fclose_data_s {
    cat_fs_work_ret_t ret;
    FILE* stream;
} cat_fs_fclose_data_t;

static void cat_fs_fclose_cb(cat_data_t *ptr)
{
    cat_fs_fclose_data_t *data = (cat_fs_fclose_data_t *) ptr;
    data->ret.ret.num = fclose(data->stream);
    if (0 != data->ret.ret.num) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
    }
}

CAT_API int cat_fs_fclose(FILE *stream)
{
    if (stream == NULL) {
        cat_fs_error_t error = { 0 };
        error.type = CAT_FS_ERROR_ERRNO;
        error.val.error = EINVAL;
        cat_fs_work_error(&error, "File-System fclose failed: %s");
        return -1;
    }
    cat_fs_fclose_data_t *data = (cat_fs_fclose_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs fclose failed");
        return -1;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->stream = stream;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_fclose_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
        return -1;
    }
    cat_fs_work_check_error(&data->ret.error, "fclose");
    return (int) data->ret.ret.num;
}

typedef struct cat_fs_fread_data_s {
    cat_fs_work_ret_t ret;
    void *ptr;
    size_t size;
    size_t nmemb;
    FILE *stream;
} cat_fs_fread_data_t;

static void cat_fs_fread_cb(cat_data_t *ptr)
{
    cat_fs_fread_data_t *data = (cat_fs_fread_data_t *) ptr;
    errno = 0;
    data->ret.ret.num = fread(data->ptr, data->size, data->nmemb, data->stream);
    if (0 != errno) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
    }
}

CAT_API size_t cat_fs_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    cat_fs_fread_data_t *data = (cat_fs_fread_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs fread failed");
        return 0;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->ptr = ptr;
    data->size = size;
    data->nmemb = nmemb;
    data->stream = stream;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_fread_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
        return 0;
    }
    cat_fs_work_check_error(&data->ret.error, "fread");
    return (size_t) data->ret.ret.num;
}

typedef struct cat_fs_fwrite_data_s {
    cat_fs_work_ret_t ret;
    const void *ptr;
    size_t size;
    size_t nmemb;
    FILE *stream;
} cat_fs_fwrite_data_t;

static void cat_fs_fwrite_cb(cat_data_t *ptr)
{
    cat_fs_fwrite_data_t *data = (cat_fs_fwrite_data_t *) ptr;
    errno = 0;
    data->ret.ret.num = fwrite(data->ptr, data->size, data->nmemb, data->stream);
    if (0 != errno) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
    }
}

CAT_API size_t cat_fs_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    cat_fs_fwrite_data_t *data = (cat_fs_fwrite_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs fwrite failed");
        return 0;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->ptr = ptr;
    data->size = size;
    data->nmemb = nmemb;
    data->stream = stream;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_fwrite_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
        return 0;
    }
    cat_fs_work_check_error(&data->ret.error, "fwrite");
    return (size_t) data->ret.ret.num;
}

typedef struct cat_fs_fseek_data_s {
    cat_fs_work_ret_t ret;
    FILE *stream;
    off_t offset;
    int whence;
} cat_fs_fseek_data_t;

static void cat_fs_fseek_cb(cat_data_t *ptr)
{
    cat_fs_fseek_data_t *data = (cat_fs_fseek_data_t *) ptr;
    data->ret.ret.num = fseeko(data->stream, data->offset, data->whence);
    if (0 != data->ret.ret.num) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
    }
}

CAT_API int cat_fs_fseek(FILE *stream, off_t offset, int whence)
{
    cat_fs_fseek_data_t *data = (cat_fs_fseek_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs fseek failed");
        return -1;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->stream = stream;
    data->offset = offset;
    data->whence = whence;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_fseek_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
        return -1;
    }
    cat_fs_work_check_error(&data->ret.error, "fseek");
    return (int) data->ret.ret.num;
}

typedef struct cat_fs_ftell_data_s {
    cat_fs_work_ret_t ret;
    FILE *stream;
} cat_fs_ftell_data_t;

static void cat_fs_ftell_cb(cat_data_t *ptr)
{
    cat_fs_ftell_data_t *data = (cat_fs_ftell_data_t *) ptr;
    data->ret.ret.num = ftello(data->stream);
    if (0 != data->ret.ret.num) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
    }
}

CAT_API off_t cat_fs_ftell(FILE *stream)
{
    cat_fs_ftell_data_t *data = (cat_fs_ftell_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs ftell failed");
        return -1;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->stream = stream;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_ftell_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
        return -1;
    }
    cat_fs_work_check_error(&data->ret.error, "ftell");
    return (long) data->ret.ret.num;
}

typedef struct cat_fs_fflush_data_s {
    cat_fs_work_ret_t ret;
    FILE *stream;
} cat_fs_fflush_data_t;

static void cat_fs_fflush_cb(cat_data_t *ptr)
{
    cat_fs_fflush_data_t *data = (cat_fs_fflush_data_t *) ptr;
    data->ret.ret.num = fflush(data->stream);
    if (0 != data->ret.ret.num) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
    }
}

CAT_API int cat_fs_fflush(FILE *stream)
{
    cat_fs_fflush_data_t *data = (cat_fs_fflush_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs fflush failed");
        return -1;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->stream = stream;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_fflush_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
        return -1;
    }
    cat_fs_work_check_error(&data->ret.error, "fflush");
    return (long) data->ret.ret.num;
}

// platform-specific cat_work wrapped fs functions
#ifndef CAT_OS_WIN

static void cat_fs_async_closedir(void *ptr)
{
    uv_dir_t *dir = (uv_dir_t *) calloc(1, sizeof(*dir));
    if (dir == NULL) {
        return;
    }
    dir->dir = ptr;
    cat_fs_context_t *context = (cat_fs_context_t *) cat_malloc(sizeof(*context));
#if CAT_ALLOC_HANDLE_ERRORS
    if (context == NULL) {
        goto _malloc_context_error;
    }
#endif
    context->coroutine = NULL;
    if (uv_fs_closedir(&CAT_EVENT_G(loop), &context->fs, dir, cat_fs_callback) != 0) {
        goto _closedir_error;
    }
    return;
    _closedir_error:
    cat_free(context);
#if CAT_ALLOC_HANDLE_ERRORS
    _malloc_context_error:
#endif
    free(dir);
}

typedef struct cat_fs_opendir_data_s {
    cat_fs_work_ret_t ret;
    char *path;
    cat_bool_t canceled;
} cat_fs_opendir_data_t;

static void cat_fs_opendir_cb(cat_data_t *ptr)
{
    cat_fs_opendir_data_t *data = (cat_fs_opendir_data_t *) ptr;
    CAT_ASSERT(NULL != data->path);

    DIR *d = opendir(data->path);

    if (NULL == d) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
    }
    data->ret.ret.ptr = d;
}

static void cat_fs_opendir_free(cat_data_t *ptr)
{
    cat_fs_opendir_data_t *data = (cat_fs_opendir_data_t *) ptr;

    if (data->canceled && NULL != data->ret.ret.ptr) {
        cat_fs_async_closedir(data->ret.ret.ptr);
    }
    cat_free(data->path);
    cat_free(data);
}

CAT_API cat_dir_t *cat_fs_opendir(const char *path)
{
    if (!path) {
        cat_fs_error_t error = {
            .type = CAT_FS_ERROR_ERRNO,
            .val.error = EINVAL,
            .msg_free = CAT_FS_FREER_NONE,
            .msg = "Invalid path (NULL)"
        };
        cat_fs_work_check_error(&error, "opendir");
        return NULL;
    }
    cat_fs_opendir_data_t *data = (cat_fs_opendir_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs opendir failed");
        return NULL;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->canceled = cat_false;
    data->path = cat_strdup(path);
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_opendir_cb, cat_fs_opendir_free, data, CAT_TIMEOUT_FOREVER)) {
        // canceled, tell freer close handle
        data->canceled = cat_true;
        return NULL;
    }
    cat_fs_work_check_error(&data->ret.error, "opendir");
    if (CAT_FS_ERROR_NONE != data->ret.error.type) {
        return NULL;
    }
    // if no error occured, ret handle must be assigned
    CAT_ASSERT(data->ret.ret.ptr);
    // copy retval out
    uv_dir_t *pdir = (uv_dir_t *) calloc(1, sizeof(uv_dir_t));
    pdir->dir = data->ret.ret.ptr;
    return pdir;
}

typedef struct cat_fs_readdir_data_s {
    cat_fs_work_ret_t ret;
    DIR *dir;
    cat_bool_t canceled;
} cat_fs_readdir_data_t;

static void cat_fs_readdir_cb(cat_data_t *ptr)
{
    cat_fs_readdir_data_t *data = (cat_fs_readdir_data_t *) ptr;

    struct dirent *pdirent = readdir(data->dir);
    if (NULL == pdirent) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
        return;
    }
    cat_dirent_t *pret = malloc(sizeof(*pret));
    if (NULL == pret) {
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
        return;
    }
    pret->name = cat_sys_strdup(pdirent->d_name);
#ifndef CAT_SYS_ALLOC_NEVER_RETURNS_NULL
    if (NULL == pret->name) {
        free(pret);
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = errno;
        return;
    }
#endif

    // from uv
    int type = 0;
#ifdef HAVE_DIRENT_TYPES
    switch (pdirent->d_type) {
# define __UV_DIRENT_MAP(XX) \
        XX(DIR) \
        XX(FILE) \
        XX(LINK) \
        XX(FIFO) \
        XX(SOCKET) \
        XX(CHAR) \
        XX(BLOCK)
# define __UV_DIRENT_GEN(name) case UV__DT_##name: type = UV_DIRENT_##name; break;
        __UV_DIRENT_MAP(__UV_DIRENT_GEN)
# undef __UV_DIRENT_GEN
# undef __UV_DIRENT_MAP
    default:
        type = UV_DIRENT_UNKNOWN;
    }
#endif
    pret->type = type;
    data->ret.ret.ptr = pret;
}

static void cat_fs_readdir_free(cat_data_t *ptr)
{
    cat_fs_readdir_data_t *data = (cat_fs_readdir_data_t *) ptr;

    if (data->canceled) {
        // if canceled, tell freer to free things
        cat_fs_async_closedir(data->dir);
    }
    if (data->ret.ret.ptr) {
        cat_dirent_t *dirent = data->ret.ret.ptr;
        if (dirent->name) {
            free((void*)dirent->name);
        }
        free(dirent);
    }
    cat_free(data);
}

/*
* cat_fs_readdir: like readdir(3), but return cat_dirent_t
* Note: you should do both free(retval->name) and free(retval)
*/
CAT_API cat_dirent_t *cat_fs_readdir(cat_dir_t *dir)
{
    uv_dir_t *uv_dir = (uv_dir_t*) dir;
    if (NULL == uv_dir || uv_dir->dir == NULL) {
        cat_fs_error_t error = {
            .type = CAT_FS_ERROR_ERRNO,
            .val.error = EINVAL,
            .msg_free = CAT_FS_FREER_NONE,
            .msg = "Invalid dir handle"
        };
        cat_fs_work_check_error(&error, "readdir");
        return NULL;
    }
    cat_fs_readdir_data_t *data = (cat_fs_readdir_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs readdir failed");
        return NULL;
    }
#endif
    memset(data, 0, sizeof(*data));
    data->dir = uv_dir->dir;
    data->canceled = cat_false;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_readdir_cb, cat_fs_readdir_free, data, CAT_TIMEOUT_FOREVER)) {
        uv_dir->dir = NULL;
        data->canceled = cat_true;
        return NULL;
    }
    cat_fs_work_check_error(&data->ret.error, "readdir");
    if (CAT_FS_ERROR_NONE != data->ret.error.type) {
        return NULL;
    }
    cat_dirent_t *work_ret = (cat_dirent_t*)data->ret.ret.ptr;
    CAT_ASSERT(work_ret);
    CAT_ASSERT(work_ret->name);
    cat_dirent_t *ret = malloc(sizeof(*ret));
    ret->type = work_ret->type;
    ret->name = cat_sys_strdup(work_ret->name);
    return ret;
}

typedef struct cat_fs_rewinddir_data_s {
    DIR *dir;
    cat_bool_t canceled;
} cat_fs_rewinddir_data_t;

static void cat_fs_rewinddir_cb(cat_data_t *ptr)
{
    cat_fs_rewinddir_data_t *data = (cat_fs_rewinddir_data_t *) ptr;
    rewinddir(data->dir);
}

static void cat_fs_rewinddir_free(cat_data_t *ptr)
{
    cat_fs_rewinddir_data_t *data = (cat_fs_rewinddir_data_t *) ptr;
    if (data->canceled) {
        // if canceled, tell freer to free things
        cat_fs_async_closedir(data->dir);
    }
    cat_free(data);
}

CAT_API void cat_fs_rewinddir(cat_dir_t *dir)
{
    uv_dir_t *uv_dir = (uv_dir_t*) dir;
    if (NULL == uv_dir || NULL == uv_dir->dir) {
        cat_fs_error_t error = {
            .type = CAT_FS_ERROR_ERRNO,
            .val.error = EINVAL,
            .msg_free = CAT_FS_FREER_NONE,
            .msg = "Invalid dir handle"
        };
        cat_fs_work_check_error(&error, "rewinddir");
        return;
    }
    cat_fs_rewinddir_data_t *data = (cat_fs_rewinddir_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs rewinddir failed");
        return;
    }
#endif
    data->dir = uv_dir->dir;
    data->canceled = cat_false;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_rewinddir_cb, cat_fs_rewinddir_free, data, CAT_TIMEOUT_FOREVER)) {
        data->canceled = cat_true;
        uv_dir->dir = NULL;
    }
}

typedef struct cat_fs_closedir_data_s {
    DIR *dir;
} cat_fs_closedir_data_t;

static void cat_fs_closedir_cb(cat_data_t *ptr)
{
    cat_fs_closedir_data_t *data = (cat_fs_closedir_data_t *) ptr;
    closedir(data->dir);
}

CAT_API int cat_fs_closedir(cat_dir_t *dir)
{
    uv_dir_t *uv_dir = (uv_dir_t*) dir;
    if (NULL == uv_dir) {
        cat_fs_error_t error = {
            .type = CAT_FS_ERROR_ERRNO,
            .val.error = EINVAL,
            .msg_free = CAT_FS_FREER_NONE,
            .msg = "Invalid dir handle"
        };
        cat_fs_work_check_error(&error, "closedir");
        return -1;
    }
    if (NULL == uv_dir->dir) {
        free(uv_dir);
        return 0;
    }
    cat_fs_closedir_data_t *data = (cat_fs_closedir_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs closedir failed");
        return -1;
    }
#endif
    data->dir = uv_dir->dir;
    free(uv_dir);
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_closedir_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
        return -1;
    }
    return 0;
}
#else
// use NtQueryDirectoryFile to mock readdir,rewinddir behavior.
typedef struct cat_dir_int_s {
    HANDLE dir;
    cat_bool_t rewind;
} cat_dir_int_t;

static HANDLE hntdll = NULL;

static void cat_fs_proventdll(void)
{
    if (NULL != hntdll) {
        return;
    }
    hntdll = GetModuleHandleW(L"ntdll.dll");
    if (NULL == hntdll) {
        hntdll= LoadLibraryExW(L"ntdll.dll", NULL, 0);
    }
}

static ULONG(*pRtlNtStatusToDosError) (NTSTATUS) = NULL;

static const char *cat_fs_proveRtlNtStatusToDosError(void)
{
    if (NULL == pRtlNtStatusToDosError) {
        if (NULL == hntdll) {
            cat_fs_proventdll();
            if (NULL == hntdll) {
                return "Cannot open ntdll.dll";
            }
        }
        pRtlNtStatusToDosError = (ULONG (*)(NTSTATUS))GetProcAddress(hntdll, "RtlNtStatusToDosError");
        if (NULL == pRtlNtStatusToDosError) {
            return "Cannot resolve RtlNtStatusToDosError";
        }
    }
    return NULL;
}

/*
static const char *cat_fs_nt_strerror(NTSTATUS status)
{
    // printf("NTSTATUS 0x%08x\n", status);
    const char *_msg;
    if (NULL != (_msg = cat_fs_proveRtlNtStatusToDosError())) {
        const char *msg = cat_sprintf("%s on processing NTSTATUS %08x", msg, status);
        cat_free(_msg);
        return _msg;
    }
    // printf("HRESULT 0x%08x\n", pRtlNtStatusToDosError(status));
    return cat_fs_win_strerror(pRtlNtStatusToDosError(status));
}
*/

typedef struct cat_fs_opendir_data_s {
    cat_fs_work_ret_t ret;
    const char *path;
    cat_bool_t canceled;
} cat_fs_opendir_data_t;

static void cat_fs_opendir_cb(cat_data_t *ptr)
{
    cat_fs_opendir_data_t *data = (cat_fs_opendir_data_t *) ptr;
    CAT_ASSERT(NULL != data->path);
    LPCWSTR pathw = cat_fs_mbs2wcs(data->path);

    HANDLE dir_handle = CreateFileW(
        pathw, // filename
        FILE_LIST_DIRECTORY | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );
    // printf("created %p\n", dir_handle);
    HeapFree(GetProcessHeap(), 0, (LPVOID) pathw);

    if (INVALID_HANDLE_VALUE == dir_handle) {
        data->ret.error.type = CAT_FS_ERROR_WIN32;
        data->ret.error.val.error = GetLastError();
    }
    data->ret.ret.handle = dir_handle;
}

static void cat_fs_opendir_free(cat_fs_opendir_data_t *data)
{
    if (data->canceled && INVALID_HANDLE_VALUE != data->ret.ret.handle) {
        CloseHandle(data->ret.ret.handle);
    }
    if (data->path) {
        cat_free((void*)data->path);
    }
    cat_free(data);
}

CAT_API cat_dir_t *cat_fs_opendir(const char *_path)
{
    wrappath(_path, path);
    if (!path) {
        cat_fs_error_t error = {
            .type = CAT_FS_ERROR_ERRNO,
            .val.error = EINVAL,
            .msg_free = CAT_FS_FREER_NONE,
            .msg = "Invalid path (NULL)"
        };
        cat_fs_work_check_error(&error, "opendir");
        return NULL;
    }
    cat_fs_opendir_data_t *data = (cat_fs_opendir_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs opendir failed");
        return NULL;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->canceled = cat_false;
    data->path = cat_strdup(path);
    data->ret.ret.handle = INVALID_HANDLE_VALUE;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_opendir_cb, cat_fs_opendir_free, data, CAT_TIMEOUT_FOREVER)) {
        // canceled, tell freer close handle
        data->canceled = cat_true;
        return NULL;
    }
    cat_fs_work_check_error(&data->ret.error, "opendir");
    if (CAT_FS_ERROR_NONE != data->ret.error.type) {
        return NULL;
    }
    // if no error occured, ret handle must be assigned
    CAT_ASSERT(INVALID_HANDLE_VALUE != data->ret.ret.handle);
    // copy retval out
    cat_dir_int_t *pdir = malloc(sizeof(cat_dir_int_t));
    pdir->dir = data->ret.ret.handle;
    pdir->rewind = cat_true;
    return pdir;
}

typedef struct cat_fs_closedir_data_s {
    cat_fs_work_ret_t ret;
    HANDLE handle;
} cat_fs_closedir_data_t;

static void cat_fs_closedir_cb(cat_data_t *ptr)
{
    cat_fs_closedir_data_t *data = (cat_fs_closedir_data_t *) ptr;
    BOOL ret = CloseHandle(data->handle);
    if (FALSE == ret) {
        data->ret.error.type = CAT_FS_ERROR_WIN32;
        data->ret.error.val.error = GetLastError();
    }
    data->ret.ret.num = ret;
    return;
}
/*
*   cat_fs_closedir: close dir returned by cat_fs_opendir
*   NOTE: this will free the dir passed in unconditionally
*/
CAT_API int cat_fs_closedir(cat_dir_t *dir)
{
    if (NULL == dir) {
        cat_fs_error_t error = {
            .type = CAT_FS_ERROR_ERRNO,
            .val.error = EINVAL,
            .msg_free = CAT_FS_FREER_NONE,
            .msg = "Invalid dir handle"
        };
        cat_fs_work_check_error(&error, "closedir");
        return -1;
    }
    if (INVALID_HANDLE_VALUE == ((cat_dir_int_t*)dir)->dir) {
        free(dir);
        return 0;
    }
    cat_fs_closedir_data_t *data = (cat_fs_closedir_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs closedir failed");
        return -1;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->handle = ((cat_dir_int_t*)dir)->dir;
    free(dir);
    cat_bool_t ret = cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_closedir_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER);
    if (!ret) {
        return -1;
    }
    cat_fs_work_check_error(&data->ret.error, "closedir");
    if (CAT_FS_ERROR_NONE != data->ret.error.type) {
        return -1;
    }
    return 0;
}

typedef struct _CAT_FILE_DIRECTORY_INFORMATION {
  ULONG         NextEntryOffset;
  ULONG         FileIndex;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER EndOfFile;
  LARGE_INTEGER AllocationSize;
  ULONG         FileAttributes;
  ULONG         FileNameLength;
  WCHAR         FileName[1];
} CAT_FILE_DIRECTORY_INFORMATION, *CAT_PFILE_DIRECTORY_INFORMATION;

typedef struct cat_fs_readdir_data_s {
    cat_fs_work_ret_t ret;
    cat_dir_int_t dir;
} cat_fs_readdir_data_t;

static void cat_fs_readdir_cb(cat_data_t *ptr)
{
    cat_fs_readdir_data_t *data = (cat_fs_readdir_data_t *) ptr;
    static __kernel_entry NTSTATUS (*pNtQueryDirectoryFile)(HANDLE,
        HANDLE, PVOID, PVOID, PVOID, PVOID, ULONG, FILE_INFORMATION_CLASS,
        BOOLEAN, PVOID, BOOLEAN) = NULL;
    if (NULL == pNtQueryDirectoryFile) {
        if (NULL == hntdll) {
            cat_fs_proventdll();
            if (NULL == hntdll) {
                data->ret.error.msg_free = CAT_FS_FREER_NONE;
                data->ret.error.msg = "Cannot open ntdll.dll on finding NtQueryDirectoryFile in readdir";
                data->ret.error.type = CAT_FS_ERROR_WIN32;
                data->ret.error.val.error = GetLastError();
                return;
            }
        }
        pNtQueryDirectoryFile = (NTSTATUS (*)(HANDLE, HANDLE, PVOID, PVOID, PVOID, PVOID, ULONG, FILE_INFORMATION_CLASS, BOOLEAN, PVOID, BOOLEAN))
            GetProcAddress(hntdll, "NtQueryDirectoryFile");
        if (NULL == pNtQueryDirectoryFile) {
            data->ret.error.msg_free = CAT_FS_FREER_NONE;
            data->ret.error.msg = "Cannot find NtQueryDirectoryFile in readdir";
            data->ret.error.type = CAT_FS_ERROR_WIN32;
            data->ret.error.val.error = GetLastError();
            return;
        }
    }

    IO_STATUS_BLOCK iosb = {0};
    NTSTATUS status;
# if _MSC_VER
    __declspec(align(8))
# else
    __attribute__ ((aligned (8)))
# endif // _MSC_VER
        char buffer[8192];
    status = pNtQueryDirectoryFile(
        data->dir.dir, // file handle
        NULL, // whatever
        NULL, // whatever
        NULL, // whatever
        &iosb,
        &buffer,
        sizeof(buffer),
        FileDirectoryInformation,
        TRUE, // if we get only 1 entry
        NULL, // wildcard filename
        data->dir.rewind // from first
    );

    if (!NT_SUCCESS(status)) {
        const char *_msg;
        if (NULL != (_msg = cat_fs_proveRtlNtStatusToDosError())) {
            data->ret.error.msg_free = CAT_FS_FREER_CAT_FREE;
            data->ret.error.msg = cat_sprintf("%s on processing NTSTATUS %08x", _msg, status);
            data->ret.error.type = CAT_FS_ERROR_CAT_ERRNO;
            data->ret.error.val.cat_errno = CAT_UNCODED;
            return;
        }
        else {
            data->ret.error.type = CAT_FS_ERROR_WIN32;
            data->ret.error.val.error = pRtlNtStatusToDosError(status);
            return;
        }
    }

    if (data->dir.rewind) {
        data->dir.rewind = cat_false;
    }

    cat_dirent_t *pdirent = malloc(sizeof(*pdirent));
    if (NULL == pdirent) {
        data->ret.error.type = CAT_FS_ERROR_WIN32;
        data->ret.error.val.error =  ERROR_NOT_ENOUGH_MEMORY;
        return;
    }

    CAT_PFILE_DIRECTORY_INFORMATION pfdi = (CAT_PFILE_DIRECTORY_INFORMATION)&buffer;

    char fn_buf[4096];
    DWORD r = WideCharToMultiByte(
        CP_UTF8, // code page
        0, // flags
        pfdi->FileName, // src
        pfdi->FileNameLength / sizeof(WCHAR), // srclen
        fn_buf, // dest
        4096, // destlen
        NULL, // default char
        FALSE // use default char
    );
    fn_buf[r] = 0;
    pdirent->name = cat_sys_strdup(fn_buf);

    // from uv
    if (pfdi->FileAttributes & FILE_ATTRIBUTE_DEVICE) {
        pdirent->type = UV_DIRENT_CHAR;
    } else if (pfdi->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        pdirent->type = UV_DIRENT_LINK;
    } else if (pfdi->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        pdirent->type = UV_DIRENT_DIR;
    } else {
        pdirent->type = UV_DIRENT_FILE;
    }
    data->ret.ret.ptr = pdirent;
}

static void cat_fs_readdir_free(cat_data_t *ptr)
{
    cat_fs_readdir_data_t *data = (cat_fs_readdir_data_t*)ptr;
    cat_dirent_t *dirent = data->ret.ret.ptr;
    if (dirent) {
        if (dirent->name) {
            free((void*)dirent->name);
        }
        free(dirent);
    }
    cat_free(data);
}
/*
*   cat_fs_readdir: read a file entry in cat_dir_t
*   NOTE: if canceled, the dir will be unusable after cancel.
*/
CAT_API cat_dirent_t *cat_fs_readdir(cat_dir_t *dir)
{
    cat_dir_int_t *pintdir = dir;
    if (NULL == pintdir || INVALID_HANDLE_VALUE == pintdir->dir) {
        cat_fs_error_t error = {
            .type = CAT_FS_ERROR_ERRNO,
            .val.error = EINVAL,
            .msg_free = CAT_FS_FREER_NONE,
            .msg = "Invalid dir handle"
        };
        cat_fs_work_check_error(&error, "closedir");
        return NULL;
    }
    cat_fs_readdir_data_t *data = (cat_fs_readdir_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs readdir failed");
        return NULL;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->dir.dir = pintdir->dir;
    data->dir.rewind = pintdir->rewind;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_readdir_cb, cat_fs_readdir_free, data, CAT_TIMEOUT_FOREVER)) {
        // canceled
        CloseHandle(pintdir->dir);
        pintdir->dir = INVALID_HANDLE_VALUE;
        return NULL;
    }
    cat_fs_work_check_error(&data->ret.error, "readdir");
    if (data->ret.error.type != CAT_FS_ERROR_NONE) {
        return NULL;
    }
    CAT_ASSERT(data->ret.ret.ptr);
    pintdir->rewind = data->dir.rewind;
    cat_dirent_t *cbret = data->ret.ret.ptr;
    cat_dirent_t *ret = malloc(sizeof(*ret));
    ret->name = cat_sys_strdup(cbret->name);
    ret->type = cbret->type;
    return ret;
}

CAT_API void cat_fs_rewinddir(cat_dir_t *dir)
{
    if (NULL == dir) {
        cat_fs_error_t error = {
            .type = CAT_FS_ERROR_ERRNO,
            .val.error = EINVAL,
            .msg_free = CAT_FS_FREER_NONE,
            .msg = "Invalid dir handle"
        };
        cat_fs_work_check_error(&error, "rewinddir");
        return;
    }
    ((cat_dir_int_t *) dir)->rewind = cat_true;
}
#endif // CAT_OS_WIN

typedef struct cat_fs_flock_data_s {
    cat_fs_work_ret_t ret;
    cat_file_t fd;
    int op;
    cat_bool_t non_blocking; /* operation is non-blocking (UN/NB) */
    cat_bool_t started; /* new thread in worker */
    cat_bool_t done; /* new thread finished */
    uv_thread_t tid;
    cat_async_t async;
    cat_event_task_t *shutdown_task;
} cat_fs_flock_data_t;

/*
* original flock(2) platform-specific implement
*/
static void cat_fs_orig_flock(cat_data_t *ptr)
{
    cat_fs_flock_data_t *data = (cat_fs_flock_data_t *) ptr;
    cat_file_t fd = data->fd;
    int op = data->op;
#ifdef CAT_OS_WIN
    // Windows implement
    int op_type = op & (CAT_LOCK_SH | CAT_LOCK_EX | CAT_LOCK_UN);
    DWORD le;
    HANDLE hFile;
    OVERLAPPED overlapped = { 0 };
    if (INVALID_HANDLE_VALUE == (hFile = (HANDLE) _get_osfhandle(fd))) {
        // bad fd, return error
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = EBADF;
        data->ret.ret.num = -1;
        goto _done;
    }

    // mock unix-like behavier: if we already have a lock,
    // flock only update the lock to specified type (shared or exclusive)
    // so we unlock first, then re-lock it
    if (
        (!UnlockFileEx(
            hFile,
            0,
            MAXDWORD,
            MAXDWORD,
            &overlapped
        )) &&
        (le = GetLastError()) != ERROR_NOT_LOCKED // not an error
    ) {
        // unlock failed
        // printf("u failed\n");
        // printf("le %08x\n", errno, GetLastError());
        data->ret.error.type = CAT_FS_ERROR_WIN32;
        data->ret.error.val.le = le;
        data->ret.ret.num = -1;
        goto _done;
    }

    if (CAT_LOCK_UN == op_type) {
        // request unlock and already unlocked
        // printf("u end\n");
        data->ret.ret.num = 0;
        goto _done;
    }

    if (LockFileEx(
        hFile,
        (
            ((CAT_LOCK_NB & op) ? LOCKFILE_FAIL_IMMEDIATELY : 0) |
            ((CAT_LOCK_EX == op_type) ? LOCKFILE_EXCLUSIVE_LOCK : 0)
        ),
        0,
        MAXDWORD,
        MAXDWORD,
        &overlapped
    )) {
        // lock success
        data->ret.ret.num = 0;
        goto _done;
    }

    if ((CAT_LOCK_NB & op) != CAT_LOCK_NB) {
        // blocking lock failed
        // printf("b done\n");
        data->ret.error.type = CAT_FS_ERROR_WIN32;
        data->ret.error.val.le = GetLastError();
        data->ret.ret.num = -1;
        goto _done;
    }

    // if LOCK_NB is set, but we donot get lock
    // we should unlock it after it was done (cancelling lock behavior)
    // printf("nb wait\n");
    data->ret.error.type = CAT_FS_ERROR_CAT_ERRNO;
    data->ret.error.val.le = CAT_EAGAIN;
    data->ret.ret.num = -1;

    // tell calling thread return
    data->done = cat_true;
    cat_async_notify(&data->async);

    DWORD dummy;
    // BOOL done =
    GetOverlappedResult(hFile, &overlapped, &dummy, 1);
    // printf("nb wait done\n");
    UnlockFileEx(
        hFile,
        0,
        MAXDWORD,
        MAXDWORD,
        &overlapped
    );
    return;

#elif defined(LOCK_EX) && defined(LOCK_SH) && defined(LOCK_UN)
    // Linux / BSDs / macOS implement with flock(2)
    int operation = 0;
# ifdef LOCK_NB
#  define FLOCK_HAVE_NB
    operation = op;
# else
    operation = op & (CAT_LOCK_SH | CAT_LOCK_EX | CAT_LOCK_UN);
# endif // LOCK_NB
    data->ret.ret.num = flock(fd, operation);
    data->ret.error.type = CAT_FS_ERROR_ERRNO;
    data->ret.error.val.error = errno;
    goto _done;
#elif defined(F_SETLK) && defined(F_SETLKW) && defined(F_RDLCK) && defined(F_WRLCK) && defined(F_UNLCK)
    // fcntl implement
# define FLOCK_HAVE_NB
    int op_type = op & (CAT_LOCK_SH | CAT_LOCK_EX | CAT_LOCK_UN);
    int cmd = (CAT_LOCK_NB == (op & CAT_LOCK_NB)) ? F_SETLK : F_SETLKW;
    struct flock lbuf = {
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0
    };
    if (CAT_LOCK_SH == op_type) {
        lbuf.l_type = F_RDLCK;
    } else if (CAT_LOCK_EX == op_type) {
        lbuf.l_type = F_WRLCK;
    } else if (CAT_LOCK_UN == op_type) {
        lbuf.l_type = F_UNLCK;
    }
    data->ret.ret.num = fcntl(fd, cmd, &lbuf);
    data->ret.error.type = CAT_FS_ERROR_ERRNO;
    data->ret.error.val.error = errno;
    goto _done;
#elif defined(F_LOCK) && defined(F_ULOCK) && defined(F_TLOCK)
    // lockf implement (not well tested, maybe buggy)
    // linux man page lockf(3) says
    // "POSIX.1 leaves the relationship between lockf() and fcntl(2) locks unspecified"
    // so we assume that some os may have an indepednent lockf implement
# define FLOCK_HAVE_NB
    int op_type = op & (CAT_LOCK_SH | CAT_LOCK_EX | CAT_LOCK_UN);
    int cmd;
    if (CAT_LOCK_SH == op_type) {
        // fcntl donot have a share flock
        data->ret.ret.num = -1;
        data->ret.error.type = CAT_FS_ERROR_ERRNO;
        data->ret.error.val.error = EINVAL;
        goto _done;
    } else if (CAT_LOCK_EX == op_type) {
        if ((CAT_LOCK_NB & op) == CAT_LOCK_NB) {
            cmd = F_TLOCK;
        } else {
            cmd = F_LOCK;
        }
    } else if (CAT_LOCK_UN == op_type) {
        cmd = F_ULOCK;
    }
    // we need seek to 0, then do this
    // however this is not atomic for this fd
    data->ret.ret.num = lockf(fd, cmd, 0); // not real full file
    data->ret.error.type = CAT_FS_ERROR_ERRNO;
    data->ret.error.val.error = errno;
    goto _done;
#else
    // maybe sometimes we can implement robust flock by ourselves?
# warning "not supported platform for flock"
    data->ret.ret.num = -1;
    data->ret.error.type = CAT_FS_ERROR_ERRNO;
    data->ret.error.val.error = ENOSYS;
    goto _done;
#endif // LOCK_EX...
    _done:
    data->done = cat_true;
    if (!data->non_blocking) {
        cat_async_notify(&data->async);
    }
}

// work (thread pool) callback
static void cat_fs_flock_cb(cat_data_t *ptr)
{
    cat_fs_flock_data_t *data = (cat_fs_flock_data_t *) ptr;
    // we do not put blocking flock into thread pool directly,
    // we create another thread to do flock to avoid deadlock when thread pool is full
    // (all threads in pool waiting flock(xx, LOCK_EX), while flock(xx, LOCK_UN) after them in queue)
    uv_thread_options_t params = {
        .flags = UV_THREAD_HAS_STACK_SIZE,
        .stack_size = 4 * cat_getpagesize()
    };
    int error = uv_thread_create_ex(&data->tid, &params, cat_fs_orig_flock, data);
    if (0 != error) {
        // printf("thread failed\n");
        data->ret.ret.num = -1;
        data->ret.error.type = CAT_FS_ERROR_CAT_ERRNO;
        data->ret.error.val.cat_errno = error;
        cat_async_notify(&data->async);
    } else {
        data->started = cat_true;
    }
}

static void cat_fs_flock_data_cleanup(cat_async_t *async)
{
    cat_fs_flock_data_t *data = cat_container_of(async, cat_fs_flock_data_t, async);

    CAT_ASSERT(!data->started || data->done);
    if (data->shutdown_task != NULL) {
        cat_event_unregister_runtime_shutdown_task(data->shutdown_task);
    }
    if (data->started) {
        uv_thread_join(&data->tid); // shell we run it in work()?
    }
    cat_free(data);
}

static void cat_fs_flock_shutdown(cat_data_t *ptr)
{
    cat_fs_flock_data_t *data = (cat_fs_flock_data_t *) ptr;

    while (!data->started) {
        cat_sys_usleep(1000);
    }
    if (!data->done) {
        // try to kill thread as much as possible
#ifndef CAT_OS_WIN
        (void) pthread_kill(data->tid, SIGKILL);
#else
        (void) TerminateThread(data->tid, 1);
#endif
        // force close
        data->shutdown_task = NULL;
        cat_async_close(&data->async, cat_fs_flock_data_cleanup);
    }
}

#ifdef FLOCK_HAVE_NB
#define _CAT_LOCK_NB CAT_LOCK_NB
#else
#define _CAT_LOCK_NB 0
#endif // FLOCK_HAVE_NB

/*
* flock(2) like implement for coroutine model
*/
CAT_API int cat_fs_flock(cat_file_t fd, int op)
{
    int op_type = (CAT_LOCK_UN | CAT_LOCK_EX | CAT_LOCK_SH) & op;
    if (
        (op & (~(CAT_LOCK_NB | CAT_LOCK_UN | CAT_LOCK_EX | CAT_LOCK_SH))) ||
        (CAT_LOCK_UN != op_type && CAT_LOCK_EX != op_type && CAT_LOCK_SH != op_type)
    ) {
        cat_update_last_error_with_reason(CAT_EINVAL, "Flock failed");
        return -1;
    }
    cat_fs_flock_data_t *data = (cat_fs_flock_data_t *) cat_malloc(sizeof(*data));
#if CAT_ALLOC_HANDLE_ERRORS
    if (data == NULL) {
        cat_update_last_error_of_syscall("Malloc for fs flock failed");
        return -1;
    }
#endif
    memset(&data->ret, 0, sizeof(data->ret));
    data->fd = fd;
    data->op = op;
    data->started = cat_false;
    data->done = cat_false;
    data->non_blocking = ((data->op & (CAT_LOCK_SH | CAT_LOCK_EX | CAT_LOCK_UN)) == CAT_LOCK_UN) || (data->op & _CAT_LOCK_NB);
    data->shutdown_task = NULL;
    if (data->non_blocking) {
        // operation is non-blocking, things done immediately
        if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_orig_flock, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
            return -1;
        }
    } else {
        if (cat_async_create(&data->async) != &data->async) {
            return -1;
        }
        if (!cat_work(CAT_WORK_KIND_FAST_IO, cat_fs_flock_cb, NULL, data, CAT_TIMEOUT_FOREVER)) {
            data->shutdown_task = cat_event_register_runtime_shutdown_task(cat_fs_flock_shutdown, data);
            cat_async_cleanup(&data->async, cat_fs_flock_data_cleanup);
            return -1;
        }
        if (!cat_async_wait_and_close(&data->async, cat_fs_flock_data_cleanup, CAT_TIMEOUT_FOREVER)) {
            data->shutdown_task = cat_event_register_runtime_shutdown_task(cat_fs_flock_shutdown, data);
            return -1;
        }
    }
    cat_fs_work_check_error(&data->ret.error, "flock");
    return (int) data->ret.ret.num;
}

#undef _CAT_LOCK_NB

CAT_API char *cat_fs_get_contents(const char *filename, size_t *length)
{
    cat_file_t fd = cat_fs_open(filename, O_RDONLY);

    if (length != NULL) {
        *length = 0;
    }
    if (fd == CAT_OS_INVALID_FD) {
        return NULL;
    }
    off_t offset = cat_fs_lseek(fd, 0, SEEK_END);
    if (offset < 0) {
        return NULL;
    }
    size_t size = (size_t) offset;
    char *buffer = (char *) cat_malloc(size + 1);
    if (buffer == NULL) {
        cat_update_last_error_of_syscall("Malloc for file content failed");
        return NULL;
    }
    ssize_t nread = cat_fs_pread(fd, buffer, size, 0);
    if (nread < 0) {
        cat_free(buffer);
        return NULL;
    }
    buffer[nread] = '\0';
    if (length != NULL) {
        *length = (size_t) nread;
    }

    return buffer;
}

CAT_API ssize_t cat_fs_put_contents(const char *filename, const char *content, size_t length)
{
    cat_file_t fd = cat_fs_open(filename,  O_CREAT | O_TRUNC | O_WRONLY, 0666);

    if (fd == CAT_OS_INVALID_FD) {
        return -1;
    }

    return cat_fs_pwrite(fd, content, length, 0);
}
