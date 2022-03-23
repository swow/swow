/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Wez Furlong <wez@thebrainroom.com>                          |
   +----------------------------------------------------------------------+
  +--------------------------------------------------------------------------+
  | Swow                                                                     |
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
  | Author: dixyes <dixyes@gmail.com>                                        |
  +--------------------------------------------------------------------------+
 */

#include "swow.h"
#include "cat_fs.h"
#include "cat_work.h"
#include "cat_time.h"

// from main/streams/plain_wrapper.c @ aec08cc841a436a9a28468ce420e2f39adc3515a

#include "php.h"
#include "php_globals.h"
#include "php_network.h"
#include "php_open_temporary_file.h"
#include "ext/standard/file.h"
#include "ext/standard/flock_compat.h"
#include "ext/standard/php_filestat.h"
#include <stddef.h>
#include <fcntl.h>
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#if HAVE_SYS_FILE_H
# include <sys/file.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#include "SAPI.h"

// update errno from cat err code
#define UPDATE_ERRNO_FROM_CAT() do { \
    errno = cat_orig_errno(cat_get_last_error_code()); \
} while (0);

#if SWOW_DEBUG

# define emalloc_rel_orig(size) \
    ( __php_stream_call_depth == 0 \
    ? _emalloc((size) ZEND_FILE_LINE_CC ZEND_FILE_LINE_RELAY_CC) \
    : _emalloc((size) ZEND_FILE_LINE_CC ZEND_FILE_LINE_ORIG_RELAY_CC) )

# define erealloc_rel_orig(ptr, size) \
    ( __php_stream_call_depth == 0 \
    ? _erealloc((ptr), (size), 0 ZEND_FILE_LINE_CC ZEND_FILE_LINE_RELAY_CC) \
    : _erealloc((ptr), (size), 0 ZEND_FILE_LINE_CC ZEND_FILE_LINE_ORIG_RELAY_CC) )

# define pemalloc_rel_orig(size, persistent) ((persistent) ? malloc((size)) : emalloc_rel_orig((size)))
# define perealloc_rel_orig(ptr, size, persistent) ((persistent) ? realloc((ptr), (size)) : erealloc_rel_orig((ptr), (size)))
#else
# define pemalloc_rel_orig(size, persistent) pemalloc((size), (persistent))
# define perealloc_rel_orig(ptr, size, persistent) perealloc((ptr), (size), (persistent))
# define emalloc_rel_orig(size) emalloc((size))
#endif

#define STREAM_DEBUG 0
#define STREAM_WRAPPER_PLAIN_FILES    ((php_stream_wrapper*)-1)

#ifndef MAP_FAILED
# define MAP_FAILED ((void *) -1)
#endif

#define CHUNK_SIZE    8192

#ifdef PHP_WIN32
# ifdef EWOULDBLOCK
#  undef EWOULDBLOCK
# endif
# define EWOULDBLOCK WSAEWOULDBLOCK
# ifdef EMSGSIZE
#  undef EMSGSIZE
# endif
# define EMSGSIZE WSAEMSGSIZE
# include "win32/winutil.h"
// we use swow's shim
//# include "win32/time.h"
# include "win32/ioutil.h"
# include "win32/readdir.h"
#endif

#define swow_stream_fopen_from_fd_int(fd, mode, persistent_id)    _swow_stream_fopen_from_fd_int((fd), (mode), (persistent_id) STREAMS_CC)
#define swow_stream_fopen_from_fd_int_rel(fd, mode, persistent_id)     _swow_stream_fopen_from_fd_int((fd), (mode), (persistent_id) STREAMS_REL_CC)
#define swow_stream_fopen_from_file_int(file, mode)    _swow_stream_fopen_from_file_int((file), (mode) STREAMS_CC)
#define swow_stream_fopen_from_file_int_rel(file, mode)     _swow_stream_fopen_from_file_int((file), (mode) STREAMS_REL_CC)

#ifndef PHP_WIN32
extern int php_get_uid_by_name(const char *name, uid_t *uid);
extern int php_get_gid_by_name(const char *name, gid_t *gid);
#endif

#if defined(PHP_WIN32)
# define PLAIN_WRAP_BUF_SIZE(st) (((st) > UINT_MAX) ? UINT_MAX : (unsigned int) (st))
#else
# define PLAIN_WRAP_BUF_SIZE(st) (st)
#endif

#define COPY_MEMBER(x) statbuf->x = _statbuf.x
#ifndef PHP_WIN32
# define COPY_MEMBERS() do { \
    COPY_MEMBER(st_dev); \
    COPY_MEMBER(st_ino); \
    COPY_MEMBER(st_mode); \
    COPY_MEMBER(st_nlink); \
    COPY_MEMBER(st_uid); \
    COPY_MEMBER(st_gid); \
    COPY_MEMBER(st_rdev); \
    COPY_MEMBER(st_size); \
    statbuf->st_atime = _statbuf.st_atim.tv_sec; \
    statbuf->st_ctime = _statbuf.st_ctim.tv_sec; \
    statbuf->st_mtime = _statbuf.st_mtim.tv_sec; \
    COPY_MEMBER(st_blksize); \
    COPY_MEMBER(st_blocks); \
} while (0)
#else

# define COPY_MEMBERS() do { \
    COPY_MEMBER(st_dev); \
    COPY_MEMBER(st_ino); \
    COPY_MEMBER(st_mode); \
    COPY_MEMBER(st_nlink); \
    COPY_MEMBER(st_uid); \
    COPY_MEMBER(st_gid); \
    COPY_MEMBER(st_rdev); \
    COPY_MEMBER(st_size); \
    /* (uint32_t) is workaround for 2038 year problem */ \
    /* needs uv fix this */ \
    statbuf->st_atime = (uint32_t)_statbuf.st_atim.tv_sec; \
    statbuf->st_ctime = (uint32_t)_statbuf.st_ctim.tv_sec; \
    statbuf->st_mtime = (uint32_t)_statbuf.st_mtim.tv_sec; \
} while (0)
#endif

#ifdef PHP_WIN32
// for readdir on windows with non-utf8 filenames
static inline cat_dirent_t *swow_fs_readdir(cat_dir_t *dir)
{
# define CLEANUP(dirent) \
    do { \
        if (dirent) { \
            if ((dirent)->name) { \
                free((void *) (dirent)->name); \
            } \
            free((void *) dirent); \
        } \
    } while (0)

    cat_dirent_t *dirent = cat_fs_readdir(dir);
    if (NULL != dirent && NULL != dirent->name) {
        const wchar_t *wname = php_win32_cp_utf8_to_w(dirent->name);
        if (NULL == wname) {
            CLEANUP(dirent);
            return NULL;
        }
        free((void *) dirent->name);
        dirent->name = php_win32_cp_w_to_cur(wname);
        if (NULL == dirent->name) {
            CLEANUP(dirent);
            return NULL;
        }
        return dirent;
    }
    CLEANUP(dirent);
    return NULL;
# undef CLEANUP
}
#else
# define swow_fs_readdir cat_fs_readdir
#endif

#ifdef PHP_WIN32
// for junctions:
// uv will return 0o120666 for directory junctions' st_mode
// while php_win32_ioutil_stat_ex_w returns 0
struct _swow_fs_stat_ex_s {
    int ret;
    const wchar_t *pathw;
    size_t len;
    zend_stat_t *statbuf;
    int use_lstat;
};
static void _swow_fs_stat_ex_cb(cat_data_t *ptr)
{
    struct _swow_fs_stat_ex_s *data = (struct _swow_fs_stat_ex_s *) ptr;
    data->ret = php_win32_ioutil_stat_ex_w(data->pathw, data->len, data->statbuf, data->use_lstat);
}
static void _swow_fs_stat_ex_free(cat_data_t *ptr)
{
    struct _swow_fs_stat_ex_s *data = (struct _swow_fs_stat_ex_s *) ptr;
    if (data->pathw) {
        free((void *) data->pathw);
    }
    cat_free(data);
}
// for fstat a pipe on win:
// uv will return 0o100666 (regular file | rw-rw-rw-) for pipe
// this may be a uv bug?
// while php_win32_ioutil_fstat_ex_w returns 0o10666 (pipe | rw-rw-rw-)
struct _swow_fs_fstat_s {
    int ret;
    int fd;
    zend_stat_t *statbuf;
};
static void _swow_fs_fstat_cb(cat_data_t *ptr)
{
    struct _swow_fs_fstat_s *data = (struct _swow_fs_fstat_s *) ptr;
    data->ret = php_win32_ioutil_fstat(data->fd, data->statbuf);
}
static inline int swow_fs_fstat(int fd, zend_stat_t *statbuf)
{
    struct _swow_fs_fstat_s *data = cat_malloc(sizeof(*data));
    data->ret = -1;
    data->fd = fd;
    data->statbuf = statbuf;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, _swow_fs_fstat_cb, cat_free_function, data, CAT_TIMEOUT_FOREVER)) {
        data->ret = -1;
    }
    UPDATE_ERRNO_FROM_CAT();
    return data->ret;
}
#else
static inline int swow_fs_fstat(int fd, zend_stat_t *statbuf)
{
    int ret = -1;
    cat_stat_t _statbuf;
    if ((ret = cat_fs_fstat(fd, &_statbuf)) == 0) {
        COPY_MEMBERS();
    }
    return ret;
}
#endif

static inline int swow_fs_stat_mock(const char *path, zend_stat_t *statbuf, int use_lstat)
{
#ifdef PHP_WIN32
    size_t pathw_len;
    LPCWSTR pathw = php_win32_ioutil_conv_any_to_w(path, PHP_WIN32_CP_IGNORE_LEN, &pathw_len);
    if (!pathw) {
        SET_ERRNO_FROM_WIN32_CODE(ERROR_INVALID_PARAMETER);
        return -1;
    }
    struct _swow_fs_stat_ex_s *data = cat_malloc(sizeof(*data));
    data->ret = -1;
    data->pathw = pathw;
    data->len = pathw_len;
    data->statbuf = statbuf;
    data->use_lstat = use_lstat;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, _swow_fs_stat_ex_cb, _swow_fs_stat_ex_free, data, CAT_TIMEOUT_FOREVER)) {
        data->ret = -1;
    }
    UPDATE_ERRNO_FROM_CAT();
    return data->ret;
#else // PHP_WIN32
    // for unix-like
    // we use cat_fs_xstat then copy it
    int ret;
    cat_stat_t _statbuf;
    if (use_lstat) {
        ret = cat_fs_lstat(path, &_statbuf);
    } else {
        ret = cat_fs_stat(path, &_statbuf);
    }
    UPDATE_ERRNO_FROM_CAT();
    if (ret < 0) {
        return ret;
    }
    COPY_MEMBERS();
    return ret;
#endif
}
#undef COPY_MEMBER

// from win32/winutil.c @ 01b3fc03c30c6cb85038250bb5640be3a09c6a32
#ifdef PHP_WIN32
static int swow_win32_check_trailing_space(const char *path, const size_t path_len)
{
    if (path_len > MAXPATHLEN - 1) {
        return 1;
    }
    if (path) {
        if (path[0] == ' ' || path[path_len - 1] == ' ') {
            return 0;
        } else {
            return 1;
        }
    } else {
        return 0;
    }
}
#endif

// from main/streams/cast.c @ 01b3fc03c30c6cb85038250bb5640be3a09c6a32
static void swow_stream_mode_sanitize_fdopen_fopencookie(php_stream *stream, char *result)
{
    /* replace modes not supported by fdopen and fopencookie, but supported
     * by PHP's fread(), so that their calls won't fail */
    const char *cur_mode = stream->mode;
    int         has_plus = 0,
                has_bin  = 0,
                i,
                res_curs = 0;

    if (cur_mode[0] == 'r' || cur_mode[0] == 'w' || cur_mode[0] == 'a') {
        result[res_curs++] = cur_mode[0];
    } else {
        /* assume cur_mode[0] is 'c' or 'x'; substitute by 'w', which should not
         * truncate anything in fdopen/fopencookie */
        result[res_curs++] = 'w';

        /* x is allowed (at least by glibc & compat), but not as the 1st mode
         * as in PHP and in any case is (at best) ignored by fdopen and fopencookie */
    }

    /* assume current mode has at most length 4 (e.g. wbn+) */
    for (i = 1; i < 4 && cur_mode[i] != '\0'; i++) {
        if (cur_mode[i] == 'b') {
            has_bin = 1;
        } else if (cur_mode[i] == '+') {
            has_plus = 1;
        }
        /* ignore 'n', 't' or other stuff */
    }

    if (has_bin) {
        result[res_curs++] = 'b';
    }
    if (has_plus) {
        result[res_curs++] = '+';
    }

    result[res_curs] = '\0';
}

struct _swow_fs_open_s {
    int ret;
#ifdef PHP_WIN32
    const wchar_t *pathw;
#else
    const char *path;
#endif
    int flags;
    mode_t mode;
    int error; // errno
};
static void _swow_fs_open_cb(cat_data_t *ptr)
{
    struct _swow_fs_open_s *data = (struct _swow_fs_open_s *) ptr;
    errno = data->error;
#ifdef PHP_WIN32
    data->ret = php_win32_ioutil_open_w(data->pathw, data->flags, data->mode);
#else
    data->ret = open(data->path, data->flags, data->mode);
#endif
    data->error = errno;
}
static void _swow_fs_open_free(cat_data_t *ptr)
{
    struct _swow_fs_open_s *data = (struct _swow_fs_open_s *) ptr;
#ifdef PHP_WIN32
    free((void *) data->pathw);
#endif
    cat_free(data);
}
static inline int swow_fs_open(const char *path, int flags, ...)
{
    struct _swow_fs_open_s *data = cat_malloc(sizeof(*data));
    data->ret = -1;
    data->flags = flags;
    data->mode = 0666;
    data->error = 0;
#ifdef PHP_WIN32
    size_t pathw_len;
    data->pathw = php_win32_ioutil_conv_any_to_w(path, PHP_WIN32_CP_IGNORE_LEN, &pathw_len);
    if (!data->pathw) {
        SET_ERRNO_FROM_WIN32_CODE(ERROR_INVALID_PARAMETER);
        cat_free(data);
        return -1;
    }
    if (!PHP_WIN32_IOUTIL_PATH_IS_OK_W(data->pathw, pathw_len)) {
        free((void *) data->pathw);
        cat_free(data);
        SET_ERRNO_FROM_WIN32_CODE(ERROR_ACCESS_DENIED);
        cat_update_last_error(CAT_EACCES, "Bad file name");
        return -1;
    }
#else
    data->path = path;
#endif

    if (flags & CAT_FS_O_CREAT) {
        va_list arg;

        va_start(arg, flags);
        data->mode = (mode_t) va_arg(arg, int);
        va_end(arg);
    }

    data->error = errno;
    if (!cat_work(CAT_WORK_KIND_FAST_IO, _swow_fs_open_cb, _swow_fs_open_free, data, CAT_TIMEOUT_FOREVER)) {
        UPDATE_ERRNO_FROM_CAT();
        data->ret = -1;
    }
    errno = data->error;

    return data->ret;
}

// mock VCWD_XXXX macros
#ifndef HAVE_UTIME
struct utimbuf {
    time_t actime;
    time_t modtime;
};
#endif // HAVE_UTIME

// PHP_WIN32_IOUTIL_CHECK_PATH_W behavior
#ifdef PHP_WIN32
/*
 * check if path is good for using, then return wcs path
 */
static inline const wchar_t *swow_check_path_w(const char *path)
{
    PHP_WIN32_IOUTIL_INIT_W(path)
    if (!pathw) {
        SET_ERRNO_FROM_WIN32_CODE(ERROR_INVALID_PARAMETER);
        cat_update_last_error(CAT_EINVAL, "Bad file name");
        return NULL;
    }
    size_t _len = wcslen(pathw);
    if (!PHP_WIN32_IOUTIL_PATH_IS_OK_W(pathw, _len)) {
        free((void *) pathw);
        SET_ERRNO_FROM_WIN32_CODE(ERROR_ACCESS_DENIED);
        cat_update_last_error(CAT_EACCES, "Bad file name");
        return NULL;
    }
    return pathw;
}
/*
 * convert wide to utf8
 */
static inline const char *swow_check_path(const char *path)
{
    LPCWSTR pathw = swow_check_path_w(path);
    if (!pathw) {
        return NULL;
    }
    return php_win32_cp_w_to_utf8(pathw);
}
# define check_path_free free
# define SAVE_LE DWORD le = GetLastError()
# define RESTORE_LE SetLastError(le)
#else
# define swow_check_path(path) path
# define check_path_free(...) {}
# define SAVE_LE {}
# define RESTORE_LE {}
#endif // PHP_WIN32

#ifdef VIRTUAL_DIR
/*
 * parse vcwd(at zts) path
 * returned path should be efreed
 */
static inline const char *swow_vcwd_path(const char *path, int flags)
{
    cwd_state new_state = {0};
    new_state.cwd = virtual_getcwd_ex(&new_state.cwd_length);
    // virtual_file_ex returns non-zero(1 or -1) for error
    if (NULL == new_state.cwd || 0 != virtual_file_ex(&new_state, path, NULL, flags)) {
        SAVE_LE;
        if (new_state.cwd) {
            efree(new_state.cwd);
        }
# ifdef PHP_WIN32
        // this may omit LastError
        cat_update_last_error(cat_translate_sys_error(GetLastError()), "Bad file name");
# else
        cat_update_last_error(cat_translate_sys_error(errno), "Bad file name");
# endif // PHP_WIN32
        RESTORE_LE;
        return NULL;
    }
    return new_state.cwd;
}
# define SWOW_VCWD_WRAP(path, new_path, flags, failed, wrapped) do { \
    const char *new_path_cur = swow_vcwd_path(path, flags); \
    if (unlikely(NULL == new_path_cur)) { \
        {failed}; \
        break; \
    } \
    const char *new_path = swow_check_path(new_path_cur); \
    if (NULL == new_path) { \
        { \
            failed \
        } \
    } else { \
        { \
            wrapped \
        } \
    } \
    SAVE_LE; \
    efree((void *) new_path_cur); \
    check_path_free((void *) new_path); \
    RESTORE_LE; \
} while (0);
#else
# define SWOW_VCWD_WRAP(path, new_path, flags, failed, wrapped) do { \
    const char *new_path = path; \
    if (!(new_path = swow_check_path(path))) { \
        { \
            failed \
        } \
        break; \
    } \
    {wrapped} SAVE_LE; \
    check_path_free((void *) new_path); \
    RESTORE_LE; \
} while (0)
#endif // VIRTUAL_DIR

// one-shot use here, only needs one form
static inline int swow_virtual_open(const char *path, int flags)
{
    int ret;
    SWOW_VCWD_WRAP(path, real_path, CWD_FILEPATH, {
        ret = -1;
    }, {
        ret = cat_fs_open(real_path, flags);
    });
    return ret;
}

static inline int swow_virtual_unlink(const char *path)
{
    int ret;
    SWOW_VCWD_WRAP(path, real_path, CWD_EXPAND, {
        ret = -1;
    }, {
        ret = cat_fs_unlink(real_path);
    });
    return ret;
}

static inline cat_dir_t *swow_virtual_opendir(const char *path){
    cat_dir_t *ret;
    SWOW_VCWD_WRAP(path, real_path, CWD_REALPATH, {
        ret = NULL;
    }, {
        ret = cat_fs_opendir(real_path);
    });
    return ret;
}

static inline int swow_virtual_access(const char *path, int mode)
{
    int ret;
    SWOW_VCWD_WRAP(path, real_path, CWD_REALPATH, {
        ret = -1;
    }, {
        ret = cat_fs_access(real_path, mode);
    });
    return ret;
}

static inline int swow_virtual_stat_ex(const char *path, zend_stat_t *statbuf, int use_lstat, int flags)
{
#ifdef VIRTUAL_DIR
    const char *new_path_cur = swow_vcwd_path(path, flags);
    if (unlikely(NULL == new_path_cur)) {
        return -1;
    }
#else
    const char *new_path_cur = path;
#endif
    int ret;
#ifdef PHP_WIN32
    LPCWSTR pathw = swow_check_path_w(path);
    if (!pathw) {
        ret = -1;
    } else
#endif // PHP_WIN32
        ret = swow_fs_stat_mock(new_path_cur, statbuf, use_lstat);
#ifdef VIRTUAL_DIR
    efree((void *) new_path_cur);
#endif
    return ret;
}

static inline int swow_virtual_stat(const char *path, zend_stat_t *statbuf)
{
    return swow_virtual_stat_ex(path, statbuf, 0, CWD_REALPATH);
}

static inline int swow_virtual_lstat(const char *path, zend_stat_t *statbuf)
{
    return swow_virtual_stat_ex(path, statbuf, 1, CWD_EXPAND);
}

static inline int swow_virtual_utime(const char *path, struct utimbuf *buf)
{
    int ret;
    SWOW_VCWD_WRAP(path, real_path, CWD_REALPATH, {
        ret = -1;
    }, {
        ret = cat_fs_utime(real_path, buf->actime, buf->modtime);
    });
    return ret;
}

static inline int swow_virtual_rename(const char *a, const char *b)
{
    int ret;
    SWOW_VCWD_WRAP(a, real_a, CWD_EXPAND, {
        ret = -1;
    }, {
        SWOW_VCWD_WRAP(b, real_b, CWD_EXPAND, {
            ret = -1;
        }, {
            ret = cat_fs_rename(real_a, real_b);
        });
    });
    return ret;
}

static inline int swow_virtual_chmod(const char *path, mode_t mode)
{
    int ret;
    SWOW_VCWD_WRAP(
        path, real_path, CWD_REALPATH, { ret = -1; }, { ret = cat_fs_chmod(real_path, (int) mode); });
    return ret;
}

#ifdef PHP_WIN32
// '/' -> '\\'
# define NORMALIZE_PATH() do { \
    for (char *p = strchr(real_path, '/'); p != NULL; p = strchr(p, '/')) { \
        if (real_path_dup == real_path) { \
            real_path_dup = estrdup(real_path); \
            p = strchr(real_path_dup, '/'); \
        } \
        *p = '\\'; \
    } \
} while (0)
# define FREE_NORMALIZED_PATH() do { \
    if (real_path_dup != real_path) { \
        efree(real_path_dup); \
    } \
} while (0)
#else
# define NORMALIZE_PATH() {}
# define FREE_NORMALIZED_PATH() {}
#endif // PHP_WIN32
static inline int swow_virtual_mkdir(const char *path, mode_t mode)
{
    int ret;
    SWOW_VCWD_WRAP(path, real_path, CWD_FILEPATH, {
        ret = -1;
    }, {
        char *real_path_dup = (char*) real_path;
        NORMALIZE_PATH();
        ret = cat_fs_mkdir(real_path_dup, (int)mode);
        FREE_NORMALIZED_PATH();
    });
    return ret;
}

static inline int swow_virtual_rmdir(const char *path)
{
    int ret;
    SWOW_VCWD_WRAP(path, real_path, CWD_EXPAND, {
        ret = -1;
    }, {
        ret = cat_fs_rmdir(real_path);
    });
    return ret;
}

#ifndef PHP_WIN32
static inline int swow_virtual_chown(const char *path, uid_t owner, gid_t group)
{
    int ret;
    SWOW_VCWD_WRAP(path, real_path, CWD_REALPATH, {
        ret = -1;
    }, {
        ret = cat_fs_chown(real_path, (cat_uid_t)owner, (cat_gid_t)group);
    });
    return ret;
}
/*
// this shim is not used
static inline int swow_virtual_lchown(const char *path, uid_t owner, gid_t group){
    int ret;
    SWOW_VCWD_WRAP(path, real_path, CWD_REALPATH, {
        ret = -1;
    }, {
        ret = cat_fs_lchown(real_path, (cat_uid_t)owner, (cat_gid_t)group);
    });
    return ret;
}
*/
#endif // PHP_WIN32

/*
 * cat_fs_flock wrapper
 */
#if defined(LOCK_SH) && LOCK_SH == CAT_LOCK_SH && \
    defined(LOCK_EX) && LOCK_EX == CAT_LOCK_EX && \
    defined(LOCK_UN) && LOCK_UN == CAT_LOCK_UN && \
    defined(LOCK_NB) && LOCK_NB == CAT_LOCK_NB
# define swow_fs_flock cat_fs_flock
#else
static inline int swow_fs_flock(int fd, int op)
{
    int op_type = op | (LOCK_SH & LOCK_EX & LOCK_UN);
    int op_nb = op | LOCK_NB;
    int _op = op_nb != 0 ? CAT_LOCK_NB : 0;
    switch (op_type) {
        case LOCK_SH:
            _op |= CAT_LOCK_SH;
            break;
        case LOCK_EX:
            _op |= CAT_LOCK_EX;
            break;
        case LOCK_UN:
            _op |= CAT_LOCK_UN;
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    return cat_fs_flock(fd, _op);
}
#endif

// plain_wrappers.c
/* {{{ ------- plain file stream implementation -------*/

typedef struct {
    FILE *file;
    int fd;                    /* underlying file descriptor */
    unsigned is_process_pipe:1;    /* use pclose instead of fclose */
    unsigned is_pipe:1;        /* stream is an actual pipe, currently Windows only*/
    unsigned cached_fstat:1;    /* sb is valid */
    unsigned is_pipe_blocking:1; /* allow blocking read() on pipes, currently Windows only */
    unsigned no_forced_fstat:1;  /* Use fstat cache even if forced */
    unsigned is_seekable:1;        /* don't try and seek, if not set */
    unsigned _reserved:26;

    int lock_flag;            /* stores the lock state */
    zend_string *temp_name;    /* if non-null, this is the path to a temporary file that
                             * is to be deleted when the stream is closed */
#ifdef HAVE_FLUSHIO
    char last_op;
#endif

#ifdef HAVE_MMAP
    char *last_mapped_addr;
    size_t last_mapped_len;
#endif
#ifdef PHP_WIN32
    char *last_mapped_addr;
    HANDLE file_mapping;
#endif

    zend_stat_t sb;
} swow_stdio_stream_data;
#define SWOW_STDIOP_FD(data)    ((data)->file ? fileno((data)->file) : (data)->fd)

static int do_fstat(swow_stdio_stream_data *d, int force)
{
    if (!d->cached_fstat || (force && !d->no_forced_fstat)) {
        int r = swow_fs_fstat(SWOW_STDIOP_FD(d), &d->sb);
        d->cached_fstat = r == 0;

        return r;
    }
    return 0;
}

static php_stream *_swow_stream_fopen_from_fd_int(int fd, const char *mode, const char *persistent_id STREAMS_DC)
{
    swow_stdio_stream_data *self;

    self = pemalloc_rel_orig(sizeof(*self), persistent_id);
    memset(self, 0, sizeof(*self));
    self->file = NULL;
    self->is_seekable = 1;
    self->is_pipe = 0;
    self->lock_flag = LOCK_UN;
    self->is_process_pipe = 0;
    self->temp_name = NULL;
    self->fd = fd;
#ifdef PHP_WIN32
    self->is_pipe_blocking = 0;
#endif

    return php_stream_alloc_rel(&php_stream_stdio_ops, self, persistent_id, mode);
}

static php_stream *_swow_stream_fopen_from_file_int(FILE *file, const char *mode STREAMS_DC)
{
    swow_stdio_stream_data *self;

    self = emalloc_rel_orig(sizeof(*self));
    memset(self, 0, sizeof(*self));
    self->file = file;
    self->is_seekable = 1;
    self->is_pipe = 0;
    self->lock_flag = LOCK_UN;
    self->is_process_pipe = 0;
    self->temp_name = NULL;
    self->fd = fileno(file);
#ifdef PHP_WIN32
    self->is_pipe_blocking = 0;
#endif

    return php_stream_alloc_rel(&php_stream_stdio_ops, self, 0, mode);
}

// from main/php_open_temporary_file.c
static int swow_do_open_temporary_file(const char *path, const char *pfx, zend_string **opened_path_p)
{
    char opened_path[MAXPATHLEN];
    const char *trailing_slash;
    char cwd[MAXPATHLEN];
    cwd_state new_state;
    int fd = -1;

    if (!path || !path[0]) {
        return -1;
    }


#ifdef PHP_WIN32
    if (!swow_win32_check_trailing_space(pfx, strlen(pfx))) {
        cat_update_last_error(EINVAL, "Bad file name"); // fixme: check this!
        return -1;
    }
#endif

    if (!VCWD_GETCWD(cwd, MAXPATHLEN)) {
        cwd[0] = '\0';
    }

    new_state.cwd = estrdup(cwd);
    new_state.cwd_length = strlen(cwd);

    if (virtual_file_ex(&new_state, path, NULL, CWD_REALPATH)) {
        efree(new_state.cwd);
        return -1;
    }

    if (IS_SLASH(new_state.cwd[new_state.cwd_length - 1])) {
        trailing_slash = "";
    } else {
        trailing_slash = "/";
    }

    if (snprintf(opened_path, MAXPATHLEN, "%s%s%sXXXXXX", new_state.cwd, trailing_slash, pfx) >= MAXPATHLEN) {
        efree(new_state.cwd);
        return -1;
    }

    fd = cat_fs_mkstemp(opened_path);

    if (fd != -1 && opened_path_p) {
        *opened_path_p = zend_string_init(opened_path, strlen(opened_path), 0);
    }

    efree(new_state.cwd);
    return fd;
}

// from main/php_open_temporary_file.c
static int swow_open_temporary_fd(const char *dir, const char *pfx, zend_string **opened_path_p)
{
    int fd;
    const char *temp_dir;

    if (!pfx) {
        pfx = "tmp.";
    }
    if (opened_path_p) {
        *opened_path_p = NULL;
    }

    if (!dir || *dir == '\0') {
def_tmp:
        temp_dir = php_get_temporary_directory();

        if (temp_dir &&
            *temp_dir != '\0' &&
            (!php_check_open_basedir(temp_dir))) {
            return swow_do_open_temporary_file(temp_dir, pfx, opened_path_p);
        } else {
            return -1;
        }
    }

    /* Try the directory given as parameter. */
    fd = swow_do_open_temporary_file(dir, pfx, opened_path_p);
    if (fd == -1) {
        /* Use default temporary directory. */
        php_error_docref(NULL, E_NOTICE, "file created in the system's temporary directory");
        goto def_tmp;
    }
    return fd;
}

// todo: hijack TEMP stream implementation
SWOW_API php_stream *_swow_stream_fopen_temporary_file(const char *dir, const char *pfx, zend_string **opened_path_ptr STREAMS_DC)
{
    zend_string *opened_path = NULL;
    int fd;

    fd = swow_open_temporary_fd(dir, pfx, &opened_path);
    if (fd != -1)    {
        php_stream *stream;

        if (opened_path_ptr) {
            *opened_path_ptr = opened_path;
        }

        stream = swow_stream_fopen_from_fd_int_rel(fd, "r+b", NULL);
        if (stream) {
            swow_stdio_stream_data *self = (swow_stdio_stream_data*)stream->abstract;
            stream->wrapper = (php_stream_wrapper*)&php_plain_files_wrapper;
            stream->orig_path = estrndup(ZSTR_VAL(opened_path), ZSTR_LEN(opened_path));

            self->temp_name = opened_path;
            self->lock_flag = LOCK_UN;

            return stream;
        }
        close(fd);

        php_error_docref(NULL, E_WARNING, "Unable to allocate stream");

        return NULL;
    }
    return NULL;
}

// todo: swowify this
static void detect_is_seekable(swow_stdio_stream_data *self) {
#if defined(S_ISFIFO) && defined(S_ISCHR)
    if (self->fd >= 0 && do_fstat(self, 0) == 0) {
        self->is_seekable = !(S_ISFIFO(self->sb.st_mode) || S_ISCHR(self->sb.st_mode));
        self->is_pipe = S_ISFIFO(self->sb.st_mode);
    }
#elif defined(PHP_WIN32)
    zend_uintptr_t handle = _get_osfhandle(self->fd);

    if (handle != (zend_uintptr_t)INVALID_HANDLE_VALUE) {
        DWORD file_type = GetFileType((HANDLE)handle);

        self->is_seekable = !(file_type == FILE_TYPE_PIPE || file_type == FILE_TYPE_CHAR);
        self->is_pipe = file_type == FILE_TYPE_PIPE;

        /* Additional check needed to distinguish between pipes and sockets. */
        if (self->is_pipe && !GetNamedPipeInfo((HANDLE) handle, NULL, NULL, NULL, NULL)) {
            self->is_pipe = 0;
        }
    }
#endif
}

static php_stream *_swow_stream_fopen_from_fd(int fd, const char *mode, const char *persistent_id STREAMS_DC)
{
    php_stream *stream = swow_stream_fopen_from_fd_int_rel(fd, mode, persistent_id);

    if (stream) {
        swow_stdio_stream_data *self = (swow_stdio_stream_data*)stream->abstract;

        detect_is_seekable(self);
        if (!self->is_seekable) {
            stream->flags |= PHP_STREAM_FLAG_NO_SEEK;
            stream->position = -1;
        } else {
            stream->position = cat_fs_lseek(self->fd, 0, SEEK_CUR);
            UPDATE_ERRNO_FROM_CAT();
#ifdef ESPIPE
            /* FIXME: Is this code still needed? */
            if (stream->position == (zend_off_t)-1 && errno == ESPIPE) {
                stream->flags |= PHP_STREAM_FLAG_NO_SEEK;
                self->is_seekable = 0;
            }
#endif
        }
    }

    return stream;
}

SWOW_API php_stream *_swow_stream_fopen_from_file(FILE *file, const char *mode STREAMS_DC)
{
    php_stream *stream = swow_stream_fopen_from_file_int_rel(file, mode);

    if (stream) {
        swow_stdio_stream_data *self = (swow_stdio_stream_data*)stream->abstract;

        detect_is_seekable(self);
        if (!self->is_seekable) {
            stream->flags |= PHP_STREAM_FLAG_NO_SEEK;
            stream->position = -1;
        } else {
            stream->position = cat_fs_ftell(file);
        }
    }

    return stream;
}

SWOW_API php_stream *_swow_stream_fopen_from_pipe(FILE *file, const char *mode STREAMS_DC)
{
    swow_stdio_stream_data *self;
    php_stream *stream;

    self = emalloc_rel_orig(sizeof(*self));
    memset(self, 0, sizeof(*self));
    self->file = file;
    self->is_seekable = 0;
    self->is_pipe = 1;
    self->lock_flag = LOCK_UN;
    self->is_process_pipe = 1;
    self->fd = fileno(file);
    self->temp_name = NULL;
#ifdef PHP_WIN32
    self->is_pipe_blocking = 0;
#endif

    stream = php_stream_alloc_rel(&php_stream_stdio_ops, self, 0, mode);
    stream->flags |= PHP_STREAM_FLAG_NO_SEEK;
    return stream;
}

static ssize_t swow_stdiop_fs_write(php_stream *stream, const char *buf, size_t count)
{
    swow_stdio_stream_data *data = (swow_stdio_stream_data*)stream->abstract;

    assert(data != NULL);

    if (data->fd >= 0) {
        ssize_t bytes_written = cat_fs_write(data->fd, buf, count);
        UPDATE_ERRNO_FROM_CAT();
        if (bytes_written < 0) {
            cat_errno_t cat_errno = cat_get_last_error_code();
            if (cat_errno == CAT_EAGAIN) {
                return 0;
            }
            if (cat_errno == CAT_EINTR) {
                /* TODO: Should this be treated as a proper error or not? */
                return bytes_written;
            }
            if (!(stream->flags & PHP_STREAM_FLAG_SUPPRESS_ERRORS)) {
                php_error_docref(NULL, E_NOTICE, "Write of %zu bytes failed with errno=%d %s", count, errno, cat_get_last_error_message());
            }
        }
        return (ssize_t) bytes_written;
    } else {

#ifdef HAVE_FLUSHIO
        if (data->is_seekable && data->last_op == 'r') {
            cat_fs_fseek(data->file, 0, SEEK_CUR);
        }
        data->last_op = 'w';
#endif // HAVE_FLUSHIO
        return (ssize_t) cat_fs_fwrite(buf, 1, count, data->file);
    }
}

static ssize_t swow_stdiop_fs_read(php_stream *stream, char *buf, size_t count)
{
    swow_stdio_stream_data *data = (swow_stdio_stream_data*) stream->abstract;
    ssize_t ret;

    assert(data != NULL);

    if (data->fd >= 0) {
#ifdef PHP_WIN32
        if ((data->is_pipe || data->is_process_pipe) && !data->is_pipe_blocking) {
            HANDLE ph = (HANDLE)_get_osfhandle(data->fd);
            int retry = 0;
            DWORD avail_read = 0;

            do {
                /* Look ahead to get the available data amount to read. Do the same
                    as read() does, however not blocking forever. In case it failed,
                    no data will be read (better than block). */
                if (!PeekNamedPipe(ph, NULL, 0, NULL, &avail_read, NULL)) {
                    break;
                }
                /* If there's nothing to read, wait in 10us periods. */
                if (0 == avail_read) {
                    cat_time_usleep(10);
                }
            } while (0 == avail_read && retry++ < 3200000);

            /* Reduce the required data amount to what is available, otherwise read()
                will block.*/
            if (avail_read < count) {
                count = avail_read;
            }
        }
#endif
        ret = cat_fs_read(data->fd, buf,  PLAIN_WRAP_BUF_SIZE(count));

        if (ret == -1 && cat_get_last_error_code() == CAT_EINTR) {
            /* Read was interrupted, retry once,
               If read still fails, giveup with feof==0
               so script can retry if desired */
            ret = cat_fs_read(data->fd, buf,  PLAIN_WRAP_BUF_SIZE(count));
        }

        UPDATE_ERRNO_FROM_CAT();
        cat_errno_t cat_errno = cat_get_last_error_code();

        if (ret < 0) {
            if (cat_errno == CAT_EAGAIN) {
                /* Not an error. */
                ret = 0;
            } else if (cat_errno == CAT_EINTR) {
                /* TODO: Should this be treated as a proper error or not? */
            } else {
                if (!(stream->flags & PHP_STREAM_FLAG_SUPPRESS_ERRORS)) {
                    php_error_docref(NULL, E_NOTICE, "Read of %zu bytes failed with errno=%d %s", count, errno, cat_get_last_error_message());
                }

                /* TODO: Remove this special-case? */
                if (cat_errno != CAT_EBADF) {
                    stream->eof = 1;
                }
            }
        } else if (ret == 0) {
            stream->eof = 1;
        }
    } else {
#ifdef HAVE_FLUSHIO
        if (data->is_seekable && data->last_op == 'w') {
            cat_fs_fseek(data->file, 0, SEEK_CUR);
        }
        data->last_op = 'r';
#endif
        ret = cat_fs_fread(buf, 1, count, data->file);

        stream->eof = feof(data->file);
    }
    return (ssize_t) ret;
}

static int swow_stdiop_fs_close(php_stream *stream, int close_handle)
{
    int ret;
    swow_stdio_stream_data *data = (swow_stdio_stream_data*)stream->abstract;

    assert(data != NULL);

#ifdef HAVE_MMAP
    if (data->last_mapped_addr) {
        munmap(data->last_mapped_addr, data->last_mapped_len);
        data->last_mapped_addr = NULL;
    }
#elif defined(PHP_WIN32)
    if (data->last_mapped_addr) {
        UnmapViewOfFile(data->last_mapped_addr);
        data->last_mapped_addr = NULL;
    }
    if (data->file_mapping) {
        CloseHandle(data->file_mapping);
        data->file_mapping = NULL;
    }
#endif

    if (close_handle) {
        if (data->file) {
            if (data->is_process_pipe) {
                errno = 0;
                ret = pclose(data->file);

#ifdef HAVE_SYS_WAIT_H
                if (WIFEXITED(ret)) {
                    ret = WEXITSTATUS(ret);
                }
#endif
            } else {
                ret = cat_fs_fclose(data->file);
                data->file = NULL;
            }
        } else if (data->fd != -1) {
            ret = cat_fs_close(data->fd);
            UPDATE_ERRNO_FROM_CAT();
            data->fd = -1;
        } else {
            return 0; /* everything should be closed already -> success */
        }
        if (data->temp_name) {
            cat_fs_unlink(ZSTR_VAL(data->temp_name));
            UPDATE_ERRNO_FROM_CAT();
            /* temporary streams are never persistent */
            zend_string_release_ex(data->temp_name, 0);
            data->temp_name = NULL;
        }
    } else {
        ret = 0;
        data->file = NULL;
        data->fd = -1;
    }

    pefree(data, stream->is_persistent);

    return ret;
}

static int swow_stdiop_fs_flush(php_stream *stream)
{
    swow_stdio_stream_data *data = (swow_stdio_stream_data*)stream->abstract;

    assert(data != NULL);

    /*
     * stdio buffers data in user land. By calling fflush(3), this
     * data is send to the kernel using write(2). fsync'ing is
     * something completely different.
     */
    if (data->file) {
        return cat_fs_fflush(data->file);
    }
    return 0;
}

#ifdef PHP_STREAM_OPTION_SYNC_API
static int swow_stdiop_fs_sync(php_stream *stream, bool dataonly)
{
    swow_stdio_stream_data *data = (swow_stdio_stream_data*)stream->abstract;
    FILE *fp;

    if (php_stream_cast(stream, PHP_STREAM_AS_STDIO, (void**)&fp, REPORT_ERRORS) == FAILURE) {
        return -1;
    }

    if (swow_stdiop_fs_flush(stream) == 0) {
        int fd = data->file ? fileno(data->file) : data->fd;
        if (dataonly) {
            return cat_fs_fdatasync(fd);
        } else {
            return cat_fs_fsync(fd);
        }
    }
    return -1;
}
#endif

static int swow_stdiop_fs_seek(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset)
{
    swow_stdio_stream_data *data = (swow_stdio_stream_data*)stream->abstract;
    int ret;

    assert(data != NULL);

    if (!data->is_seekable) {
        php_error_docref(NULL, E_WARNING, "Cannot seek on this stream");
        return -1;
    }

    if (data->fd >= 0) {
        zend_off_t result;

        result = cat_fs_lseek(data->fd, offset, whence);
        UPDATE_ERRNO_FROM_CAT();
        if (result == (zend_off_t)-1)
            return -1;

        *newoffset = result;
        return 0;

    } else {
        ret = cat_fs_fseek(data->file, offset, whence);
        *newoffset = cat_fs_ftell(data->file);
        return ret;
    }
}

static int swow_stdiop_fs_cast(php_stream *stream, int castas, void **ret)
{
    php_socket_t fd;
    swow_stdio_stream_data *data = (swow_stdio_stream_data*) stream->abstract;

    assert(data != NULL);

    /* as soon as someone touches the stdio layer, buffering may ensue,
     * so we need to stop using the fd directly in that case */

    switch (castas)    {
        case PHP_STREAM_AS_STDIO:
            if (ret) {

                if (data->file == NULL) {
                    /* we were opened as a plain file descriptor, so we
                     * need fdopen now */
                    char fixed_mode[5];
                    swow_stream_mode_sanitize_fdopen_fopencookie(stream, fixed_mode);
                    data->file = fdopen(data->fd, fixed_mode);
                    if (data->file == NULL) {
                        return FAILURE;
                    }
                }

                *(FILE**)ret = data->file;
                data->fd = SOCK_ERR;
            }
            return SUCCESS;

        case PHP_STREAM_AS_FD_FOR_SELECT:
            fd = SWOW_STDIOP_FD(data);
            if (SOCK_ERR == fd) {
                return FAILURE;
            }
            if (ret) {
                *(php_socket_t *)ret = fd;
            }
            return SUCCESS;

        case PHP_STREAM_AS_FD:
            fd = SWOW_STDIOP_FD(data);

            if (SOCK_ERR == fd) {
                return FAILURE;
            }
            if (data->file) {
                cat_fs_fflush(data->file);
            }
            if (ret) {
                *(php_socket_t *)ret = fd;
            }
            return SUCCESS;
        default:
            return FAILURE;
    }
}

static int swow_stdiop_fs_stat(php_stream *stream, php_stream_statbuf *ssb)
{
    int ret;
    swow_stdio_stream_data *data = (swow_stdio_stream_data*) stream->abstract;

    assert(data != NULL);
    if((ret = do_fstat(data, 1)) == 0) {
        memcpy(&ssb->sb, &data->sb, sizeof(ssb->sb));
    }

    return ret;
}

static int swow_stdiop_fs_set_option(php_stream *stream, int option, int value, void *ptrparam)
{
    swow_stdio_stream_data *data = (swow_stdio_stream_data*) stream->abstract;
    size_t size;
    int fd = SWOW_STDIOP_FD(data);
#ifdef O_NONBLOCK
    /* FIXME: make this work for win32 */
    int flags;
    int oldval;
#endif

    switch(option) {
        case PHP_STREAM_OPTION_BLOCKING:
            if (fd == -1)
                return -1;
#ifdef O_NONBLOCK
            flags = fcntl(fd, F_GETFL, 0);
            oldval = (flags & O_NONBLOCK) ? 0 : 1;
            if (value)
                flags &= ~O_NONBLOCK;
            else
                flags |= O_NONBLOCK;

            if (-1 == fcntl(fd, F_SETFL, flags))
                return -1;
            return oldval;
#else
            return -1; /* not yet implemented */
#endif

        case PHP_STREAM_OPTION_WRITE_BUFFER:

            if (data->file == NULL) {
                return -1;
            }

            if (ptrparam)
                size = *(size_t *)ptrparam;
            else
                size = BUFSIZ;

            switch(value) {
                case PHP_STREAM_BUFFER_NONE:
                    return setvbuf(data->file, NULL, _IONBF, 0);

                case PHP_STREAM_BUFFER_LINE:
                    return setvbuf(data->file, NULL, _IOLBF, size);

                case PHP_STREAM_BUFFER_FULL:
                    return setvbuf(data->file, NULL, _IOFBF, size);

                default:
                    return -1;
            }
            break;

        case PHP_STREAM_OPTION_LOCKING:
            if (fd == -1) {
                return -1;
            }

            if ((zend_uintptr_t) ptrparam == PHP_STREAM_LOCK_SUPPORTED) {
                return 0;
            }

            if (!swow_fs_flock(fd, value)) {
                data->lock_flag = value;
                return 0;
            } else {
#ifdef PHP_WIN32
                if (CAT_EAGAIN == cat_get_last_error_code()) {
                    errno = WSAEWOULDBLOCK;
                }
#endif // PHP_WIN32
                return -1;
            }
            break;

        case PHP_STREAM_OPTION_MMAP_API:
#ifdef HAVE_MMAP
            {
                php_stream_mmap_range *range = (php_stream_mmap_range*)ptrparam;
                int prot, flags;

                switch (value) {
                    case PHP_STREAM_MMAP_SUPPORTED:
                        return fd == -1 ? PHP_STREAM_OPTION_RETURN_ERR : PHP_STREAM_OPTION_RETURN_OK;

                    case PHP_STREAM_MMAP_MAP_RANGE:
                        if (do_fstat(data, 1) != 0) {
                            return PHP_STREAM_OPTION_RETURN_ERR;
                        }
                        if (range->offset > (size_t) data->sb.st_size) {
                            range->offset = data->sb.st_size;
                        }
                        if (range->length == 0 ||
                                range->length > data->sb.st_size - range->offset) {
                            range->length = data->sb.st_size - range->offset;
                        }
                        switch (range->mode) {
                            case PHP_STREAM_MAP_MODE_READONLY:
                                prot = PROT_READ;
                                flags = MAP_PRIVATE;
                                break;
                            case PHP_STREAM_MAP_MODE_READWRITE:
                                prot = PROT_READ | PROT_WRITE;
                                flags = MAP_PRIVATE;
                                break;
                            case PHP_STREAM_MAP_MODE_SHARED_READONLY:
                                prot = PROT_READ;
                                flags = MAP_SHARED;
                                break;
                            case PHP_STREAM_MAP_MODE_SHARED_READWRITE:
                                prot = PROT_READ | PROT_WRITE;
                                flags = MAP_SHARED;
                                break;
                            default:
                                return PHP_STREAM_OPTION_RETURN_ERR;
                        }
                        range->mapped = (char*)mmap(NULL, range->length, prot, flags, fd, range->offset);
                        if (range->mapped == (char*)MAP_FAILED) {
                            range->mapped = NULL;
                            return PHP_STREAM_OPTION_RETURN_ERR;
                        }
                        /* remember the mapping */
                        data->last_mapped_addr = range->mapped;
                        data->last_mapped_len = range->length;
                        return PHP_STREAM_OPTION_RETURN_OK;

                    case PHP_STREAM_MMAP_UNMAP:
                        if (data->last_mapped_addr) {
                            munmap(data->last_mapped_addr, data->last_mapped_len);
                            data->last_mapped_addr = NULL;

                            return PHP_STREAM_OPTION_RETURN_OK;
                        }
                        return PHP_STREAM_OPTION_RETURN_ERR;
                }
            }
#elif defined(PHP_WIN32)
            {
                php_stream_mmap_range *range = (php_stream_mmap_range*)ptrparam;
                HANDLE hfile = (HANDLE)_get_osfhandle(fd);
                DWORD prot, acc, loffs = 0, hoffs = 0, delta = 0;
                LARGE_INTEGER file_size;

                switch (value) {
                    case PHP_STREAM_MMAP_SUPPORTED:
                        return hfile == INVALID_HANDLE_VALUE ? PHP_STREAM_OPTION_RETURN_ERR : PHP_STREAM_OPTION_RETURN_OK;

                    case PHP_STREAM_MMAP_MAP_RANGE:
                        switch (range->mode) {
                            case PHP_STREAM_MAP_MODE_READONLY:
                                prot = PAGE_READONLY;
                                acc = FILE_MAP_READ;
                                break;
                            case PHP_STREAM_MAP_MODE_READWRITE:
                                prot = PAGE_READWRITE;
                                acc = FILE_MAP_READ | FILE_MAP_WRITE;
                                break;
                            case PHP_STREAM_MAP_MODE_SHARED_READONLY:
                                prot = PAGE_READONLY;
                                acc = FILE_MAP_READ;
                                /* TODO: we should assign a name for the mapping */
                                break;
                            case PHP_STREAM_MAP_MODE_SHARED_READWRITE:
                                prot = PAGE_READWRITE;
                                acc = FILE_MAP_READ | FILE_MAP_WRITE;
                                /* TODO: we should assign a name for the mapping */
                                break;
                            default:
                                return PHP_STREAM_OPTION_RETURN_ERR;
                        }

                        /* create a mapping capable of viewing the whole file (this costs no real resources) */
                        data->file_mapping = CreateFileMapping(hfile, NULL, prot, 0, 0, NULL);

                        if (data->file_mapping == NULL) {
                            return PHP_STREAM_OPTION_RETURN_ERR;
                        }

                        if (!GetFileSizeEx(hfile, &file_size)) {
                            CloseHandle(data->file_mapping);
                            data->file_mapping = NULL;
                            return PHP_STREAM_OPTION_RETURN_ERR;
                        }
# if defined(_WIN64)
                        size = file_size.QuadPart;
# else
                        if (file_size.HighPart) {
                            CloseHandle(data->file_mapping);
                            data->file_mapping = NULL;
                            return PHP_STREAM_OPTION_RETURN_ERR;
                        } else {
                            size = file_size.LowPart;
                        }
# endif
                        if (range->offset > size) {
                            range->offset = size;
                        }
                        if (range->length == 0 || range->length > size - range->offset) {
                            range->length = size - range->offset;
                        }

                        /* figure out how big a chunk to map to be able to view the part that we need */
                        if (range->offset != 0) {
                            SYSTEM_INFO info;
                            DWORD gran;

                            GetSystemInfo(&info);
                            gran = info.dwAllocationGranularity;
                            ZEND_ASSERT(gran != 0 && (gran & (gran - 1)) == 0);
                            size_t rounded_offset = (range->offset / gran) * gran;
                            delta = (DWORD)(range->offset - rounded_offset);
                            loffs = (DWORD)rounded_offset;
                            hoffs = (DWORD)(rounded_offset >> 32);
                        }

                        /* MapViewOfFile()ing zero bytes would map to the end of the file; match *nix behavior instead */
                        if (range->length + delta == 0) {
                            return PHP_STREAM_OPTION_RETURN_ERR;
                        }

                        data->last_mapped_addr = MapViewOfFile(data->file_mapping, acc, hoffs, loffs, range->length + delta);

                        if (data->last_mapped_addr) {
                            /* give them back the address of the start offset they requested */
                            range->mapped = data->last_mapped_addr + delta;
                            return PHP_STREAM_OPTION_RETURN_OK;
                        }

                        CloseHandle(data->file_mapping);
                        data->file_mapping = NULL;

                        return PHP_STREAM_OPTION_RETURN_ERR;

                    case PHP_STREAM_MMAP_UNMAP:
                        if (data->last_mapped_addr) {
                            UnmapViewOfFile(data->last_mapped_addr);
                            data->last_mapped_addr = NULL;
                            CloseHandle(data->file_mapping);
                            data->file_mapping = NULL;
                            return PHP_STREAM_OPTION_RETURN_OK;
                        }
                        return PHP_STREAM_OPTION_RETURN_ERR;

                    default:
                        return PHP_STREAM_OPTION_RETURN_ERR;
                }
            }

#endif
            return PHP_STREAM_OPTION_RETURN_NOTIMPL;

#ifdef PHP_STREAM_OPTION_SYNC_API
        case PHP_STREAM_OPTION_SYNC_API:
            switch (value) {
                case PHP_STREAM_SYNC_SUPPORTED:
                    return fd == -1 ? PHP_STREAM_OPTION_RETURN_ERR : PHP_STREAM_OPTION_RETURN_OK;
                case PHP_STREAM_SYNC_FSYNC:
                    return swow_stdiop_fs_sync(stream, 0) == 0 ? PHP_STREAM_OPTION_RETURN_OK : PHP_STREAM_OPTION_RETURN_ERR;
                case PHP_STREAM_SYNC_FDSYNC:
                    return swow_stdiop_fs_sync(stream, 1) == 0 ? PHP_STREAM_OPTION_RETURN_OK : PHP_STREAM_OPTION_RETURN_ERR;
            }
            /* Invalid option passed */
            return PHP_STREAM_OPTION_RETURN_ERR;
#endif

        case PHP_STREAM_OPTION_TRUNCATE_API:
            switch (value) {
                case PHP_STREAM_TRUNCATE_SUPPORTED:
                    return fd == -1 ? PHP_STREAM_OPTION_RETURN_ERR : PHP_STREAM_OPTION_RETURN_OK;

                case PHP_STREAM_TRUNCATE_SET_SIZE: {
                    ptrdiff_t new_size = *(ptrdiff_t*)ptrparam;
                    if (new_size < 0) {
                        return PHP_STREAM_OPTION_RETURN_ERR;
                    }
                    int _ret = cat_fs_ftruncate(fd, new_size);
                    UPDATE_ERRNO_FROM_CAT();
                    return _ret == 0 ? PHP_STREAM_OPTION_RETURN_OK : PHP_STREAM_OPTION_RETURN_ERR;
                }
            }
            return PHP_STREAM_OPTION_RETURN_NOTIMPL;

#ifdef PHP_WIN32
        case PHP_STREAM_OPTION_PIPE_BLOCKING:
            data->is_pipe_blocking = value;
            return PHP_STREAM_OPTION_RETURN_OK;
#endif
        case PHP_STREAM_OPTION_META_DATA_API:
            if (fd == -1)
                return -1;
#ifdef O_NONBLOCK
            flags = fcntl(fd, F_GETFL, 0);

            add_assoc_bool((zval*)ptrparam, "timed_out", 0);
            add_assoc_bool((zval*)ptrparam, "blocked", (flags & O_NONBLOCK)? 0 : 1);
            add_assoc_bool((zval*)ptrparam, "eof", stream->eof);

            return PHP_STREAM_OPTION_RETURN_OK;
#endif
            return -1;
        default:
            return PHP_STREAM_OPTION_RETURN_NOTIMPL;
    }
}

SWOW_API const php_stream_ops swow_stream_stdio_ops_async = {
    swow_stdiop_fs_write, swow_stdiop_fs_read,
    swow_stdiop_fs_close, swow_stdiop_fs_flush,
    "STDIO",
    swow_stdiop_fs_seek,
    swow_stdiop_fs_cast,
    swow_stdiop_fs_stat,
    swow_stdiop_fs_set_option
};
/* }}} */

/* {{{ plain files opendir/readdir implementation */
static ssize_t swow_plain_files_dirstream_read(php_stream *stream, char *buf, size_t count)
{
    cat_dir_t *dir = (cat_dir_t *)stream->abstract;
    cat_dirent_t *result;
    php_stream_dirent *ent = (php_stream_dirent*)buf;

    /* avoid problems if someone mis-uses the stream */
    if (count != sizeof(php_stream_dirent)){
        return -1;
    }

    result = swow_fs_readdir(dir);
    UPDATE_ERRNO_FROM_CAT();
    if (result) {
        PHP_STRLCPY(ent->d_name, result->name, sizeof(ent->d_name), strlen(result->name));
        free((void*)result->name);
        free(result);
        return sizeof(php_stream_dirent);
    }
    return 0;
}

static int swow_plain_files_dirstream_close(php_stream *stream, int close_handle)
{
    int ret = cat_fs_closedir((cat_dir_t*)stream->abstract);
    UPDATE_ERRNO_FROM_CAT();
    return ret;
}

static int swow_plain_files_dirstream_rewind(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffs)
{
    cat_fs_rewinddir((cat_dir_t*)stream->abstract);
    return 0;
}

static const php_stream_ops swow_plain_files_dirstream_ops_async = {
    NULL, swow_plain_files_dirstream_read,
    swow_plain_files_dirstream_close, NULL,
    "dir",
    swow_plain_files_dirstream_rewind,
    NULL, /* cast */
    NULL, /* stat */
    NULL  /* set_option */
};

#if defined(PHP_WIN32)
# if PHP_VERSION_ID < 80100
// using shim
static ZEND_COLD void swow_php_win32_docref1_from_error(DWORD error, const char *param1) {
    static void (*orig)(DWORD, const char *) = swow_php_win32_docref1_from_error;
    while (swow_php_win32_docref1_from_error == orig) {
        orig = NULL;
        DWORD le = GetLastError();
        HMODULE hModule = GetModuleHandleA("php" CAT_TO_STR(PHP_MAJOR_VERSION)
#  ifdef ZTS
        "ts"
#  endif // ZTS
#  if ZEND_DEBUG
        "_debug"
#  endif // ZEND_DEBUG
        );
        ZEND_ASSERT(hModule);
        orig = (void (*)(DWORD, const char *))GetProcAddress(hModule, "php_win32_docref1_from_error");
        SetLastError(le);
    }
    if(orig) {
        orig(error, param1);
    }else{
        php_win32_docref2_from_error(error, param1, param1);
    }
}
# else
// native docref1
#  define swow_php_win32_docref1_from_error(le, arg) php_win32_docref1_from_error(le, arg);
# endif
#endif

static php_stream *swow_plain_files_dir_opener(php_stream_wrapper *wrapper, const char *path, const char *mode,
    int options, zend_string **opened_path, php_stream_context *context STREAMS_DC)
{
    php_stream *stream = NULL;

#ifdef HAVE_GLOB
    if (options & STREAM_USE_GLOB_DIR_OPEN) {
        return php_glob_stream_wrapper.wops->dir_opener((php_stream_wrapper*)&php_glob_stream_wrapper, path, mode, options, opened_path, context STREAMS_REL_CC);
    }
#endif

    if (((options & STREAM_DISABLE_OPEN_BASEDIR) == 0) && php_check_open_basedir(path)) {
        return NULL;
    }

    cat_dir_t * dir = swow_virtual_opendir(path);
#ifdef PHP_WIN32
    if (!dir) {
        swow_php_win32_docref1_from_error(GetLastError(), path);
    }
#endif // PHP_WIN32
    if (dir) {
        stream = php_stream_alloc(&swow_plain_files_dirstream_ops_async, dir, 0, mode);
        if (stream == NULL){
            cat_fs_closedir(dir);
            UPDATE_ERRNO_FROM_CAT();
        }
    }

    return stream;
}
/* }}} */

SWOW_API php_stream_ops swow_stream_stdio_ops_sync;

/* {{{ php_stream_fopen */
SWOW_API  php_stream *_swow_stream_fopen(const char *filename, const char *mode, zend_string **opened_path, int options STREAMS_DC)
{
    zend_bool open_for_include = options & STREAM_OPEN_FOR_INCLUDE;
    /** phar_open_archive_fp, cannot use async-io */

    if (!open_for_include && EG(current_execute_data) && EG(current_execute_data)->func &&
        ZEND_USER_CODE(EG(current_execute_data)->func->type)) {
        const zend_op *opline = EG(current_execute_data)->opline;
        if (opline && opline->opcode == ZEND_INCLUDE_OR_EVAL &&
            (opline->extended_value & (ZEND_INCLUDE | ZEND_INCLUDE_ONCE | ZEND_REQUIRE | ZEND_REQUIRE_ONCE))) {
            open_for_include = 1;
        }
    }
    /* OPEN_FOR_INCLUDE must be blocking */
    if (open_for_include) {
        php_stream *stream = _php_stream_fopen(filename, mode, opened_path, options STREAMS_REL_CC);
        if (stream != NULL) {
            stream->ops = &swow_stream_stdio_ops_sync;
        }
        return stream;
    }

    char realpath[MAXPATHLEN];
    int open_flags;
    int fd;
    php_stream *ret;
    int persistent = options & STREAM_OPEN_PERSISTENT;
    char *persistent_id = NULL;

    if (FAILURE == php_stream_parse_fopen_modes(mode, &open_flags)) {
        php_stream_wrapper_log_error(&php_plain_files_wrapper, options, "`%s' is not a valid mode for fopen", mode);
        return NULL;
    }

    if (options & STREAM_ASSUME_REALPATH) {
        strlcpy(realpath, filename, sizeof(realpath));
    } else {
        if (expand_filepath(filename, realpath) == NULL) {
            return NULL;
        }
    }

    if (persistent) {
        spprintf(&persistent_id, 0, "streams_stdio_%d_%s", open_flags, realpath);
        switch (php_stream_from_persistent_id(persistent_id, &ret)) {
            case PHP_STREAM_PERSISTENT_SUCCESS:
                if (opened_path) {
                    //TODO: avoid reallocation???
                    *opened_path = zend_string_init(realpath, strlen(realpath), 0);
                }
                /* fall through */

            case PHP_STREAM_PERSISTENT_FAILURE:
                efree(persistent_id);
                return ret;
        }
    }
    fd = swow_fs_open(realpath, open_flags, 0666);
    if (fd != -1)    {
        ret = _swow_stream_fopen_from_fd(fd, mode, persistent_id STREAMS_REL_CC);

        if (ret)    {
            if (opened_path) {
                *opened_path = zend_string_init(realpath, strlen(realpath), 0);
            }
            if (persistent_id) {
                efree(persistent_id);
            }

            /* WIN32 always set ISREG flag */
#ifndef PHP_WIN32
            /* sanity checks for include/require.
             * We check these after opening the stream, so that we save
             * on fstat() syscalls */
            if (options & STREAM_OPEN_FOR_INCLUDE) {
                swow_stdio_stream_data *self = (swow_stdio_stream_data*)ret->abstract;
                int r;

                r = do_fstat(self, 0);
                if ((r == 0 && !S_ISREG(self->sb.st_mode))) {
                    if (opened_path) {
                        zend_string_release_ex(*opened_path, 0);
                        *opened_path = NULL;
                    }
                    php_stream_close(ret);
                    return NULL;
                }

                /* Make sure the fstat result is reused when we later try to get the
                 * file size. */
                self->no_forced_fstat = 1;
            }

            if (options & STREAM_USE_BLOCKING_PIPE) {
                swow_stdio_stream_data *self = (swow_stdio_stream_data*)ret->abstract;
                self->is_pipe_blocking = 1;
            }
#endif

            return ret;
        }
        cat_fs_close(fd);
        UPDATE_ERRNO_FROM_CAT();
    }
    if (persistent_id) {
        efree(persistent_id);
    }
    return NULL;
}
/* }}} */


static php_stream *swow_plain_files_stream_opener(php_stream_wrapper *wrapper, const char *path, const char *mode,
    int options, zend_string **opened_path, php_stream_context *context STREAMS_DC)
{
    if (((options & STREAM_DISABLE_OPEN_BASEDIR) == 0) && php_check_open_basedir(path)) {
        return NULL;
    }

    return _swow_stream_fopen(path, mode, opened_path, options STREAMS_REL_CC);
}

static int swow_plain_files_url_stater(php_stream_wrapper *wrapper, const char *url, int flags, php_stream_statbuf *ssb, php_stream_context *context)
{
    // note: PHP_STREAM_URL_STAT_NOCACHE is also 4 and will be passed here before 8.1
#ifdef PHP_STREAM_URL_STAT_IGNORE_OPEN_BASEDIR
    if (!(flags & PHP_STREAM_URL_STAT_IGNORE_OPEN_BASEDIR))
#endif
    {
        if (strncasecmp(url, "file://", sizeof("file://") - 1) == 0) {
            url += sizeof("file://") - 1;
        }

        if (php_check_open_basedir_ex(url, (flags & PHP_STREAM_URL_STAT_QUIET) ? 0 : 1)) {
            return -1;
        }
    }

#ifdef PHP_WIN32
    if (flags & PHP_STREAM_URL_STAT_LINK) {
        return swow_virtual_lstat(url, &ssb->sb);
    }
#else
# ifdef HAVE_SYMLINK
    if (flags & PHP_STREAM_URL_STAT_LINK) {
        return swow_virtual_lstat(url, &ssb->sb);
    } else
# endif
#endif
        return swow_virtual_stat(url, &ssb->sb);
}

static int swow_plain_files_unlink(php_stream_wrapper *wrapper, const char *url, int options, php_stream_context *context)
{
    int ret;

    if (strncasecmp(url, "file://", sizeof("file://") - 1) == 0) {
        url += sizeof("file://") - 1;
    }

    if (php_check_open_basedir(url)) {
        return 0;
    }

    ret = swow_virtual_unlink(url);
    if (ret == -1) {
        if (options & REPORT_ERRORS) {
            php_error_docref1(NULL, url, E_WARNING, "%s", cat_get_last_error_message());
        }
        return 0;
    }

    /* Clear stat cache (and realpath cache) */
    php_clear_stat_cache(1, NULL, 0);

    return 1;
}

static int swow_plain_files_rename(php_stream_wrapper *wrapper, const char *url_from, const char *url_to, int options, php_stream_context *context)
{
    int ret;

    if (!url_from || !url_to) {
        return 0;
    }

#ifdef PHP_WIN32
    if (!swow_win32_check_trailing_space(url_from, strlen(url_from))) {
        php_win32_docref2_from_error(ERROR_INVALID_NAME, url_from, url_to);
        return 0;
    }
    if (!swow_win32_check_trailing_space(url_to, strlen(url_to))) {
        php_win32_docref2_from_error(ERROR_INVALID_NAME, url_from, url_to);
        return 0;
    }
#endif

    if (strncasecmp(url_from, "file://", sizeof("file://") - 1) == 0) {
        url_from += sizeof("file://") - 1;
    }

    if (strncasecmp(url_to, "file://", sizeof("file://") - 1) == 0) {
        url_to += sizeof("file://") - 1;
    }

    if (php_check_open_basedir(url_from) || php_check_open_basedir(url_to)) {
        return 0;
    }

    ret = swow_virtual_rename(url_from, url_to);

    if (ret == -1) {
#ifndef PHP_WIN32
# ifdef CAT_EXDEV
        if (cat_get_last_error_code() == CAT_EXDEV) {
            zend_stat_t sb;
# if !defined(ZTS) && !defined(TSRM_WIN32)
            /* not sure what to do in ZTS case, umask is not thread-safe */
            int oldmask = umask(077);
# endif
            int success = 0;
            if (php_copy_file(url_from, url_to) == SUCCESS) {
                if (swow_virtual_stat(url_from, &sb)) {
                    success = 1;
#  ifndef TSRM_WIN32
                    /*
                     * Try to set user and permission info on the target.
                     * If we're not root, then some of these may fail.
                     * We try chown first, to set proper group info, relying
                     * on the system environment to have proper umask to not allow
                     * access to the file in the meantime.
                     */
                    if (swow_virtual_chown(url_to, sb.st_uid, sb.st_gid)) {
                        php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", cat_get_last_error_message());
                        if (errno != EPERM) {
                            success = 0;
                        }
                    }

                    if (success) {
                        if (swow_virtual_chmod(url_to, sb.st_mode)) {
                            php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", cat_get_last_error_message());
                            if (errno != EPERM) {
                                success = 0;
                            }
                        }
                    }
#  endif
                    if (success) {
                        swow_virtual_unlink(url_from);
                    }
                } else {
                    php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", cat_get_last_error_message());
                }
            } else {
                php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", cat_get_last_error_message());
            }
#  if !defined(ZTS) && !defined(TSRM_WIN32)
            umask(oldmask);
#  endif
            return success;
        }
# endif
#endif

#ifdef PHP_WIN32
        // should be php_win32_docref2_from_error(GetLastError(), url_from, url_to);
        // but {G,S}etLastError is not usable
        php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s (code: %d)", cat_get_last_error_message(), cat_orig_errno(cat_get_last_error_code()));
#else
        php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", cat_get_last_error_message());
#endif
        return 0;
    }

    /* Clear stat cache (and realpath cache) */
    php_clear_stat_cache(1, NULL, 0);

    return 1;
}

static int swow_mkdir_ex(const char *dir, zend_long mode, int options){
    int ret;

    if (php_check_open_basedir(dir)) {
        return -1;
    }

    if ((ret = swow_virtual_mkdir(dir, (mode_t)mode)) < 0 && (options & REPORT_ERRORS)) {
        php_error_docref(NULL, E_WARNING, "%s", cat_get_last_error_message());
    }

    return ret;
}

static int swow_plain_files_mkdir(php_stream_wrapper *wrapper, const char *dir, int mode, int options, php_stream_context *context)
{
    if (strncasecmp(dir, "file://", sizeof("file://") - 1) == 0) {
        dir += sizeof("file://") - 1;
    }

    if (!(options & PHP_STREAM_MKDIR_RECURSIVE)) {
        return swow_mkdir_ex(dir, mode, REPORT_ERRORS) == 0;
    }

    char buf[MAXPATHLEN];
    if (!expand_filepath_with_mode(dir, buf, NULL, 0, CWD_EXPAND)) {
        php_error_docref(NULL, E_WARNING, "Invalid path");
        return 0;
    }

    if (php_check_open_basedir(buf)) {
        return 0;
    }

    /* we look for directory separator from the end of string, thus hopefully reducing our work load */
    char *p;
    zend_stat_t sb;
    size_t dir_len = strlen(dir), offset = 0;
    char *e = buf +  strlen(buf);

    if ((p = memchr(buf, DEFAULT_SLASH, dir_len))) {
        offset = p - buf + 1;
    }

    if (p && dir_len == 1) {
        /* buf == "DEFAULT_SLASH" */
    }
    else {
        /* find a top level directory we need to create */
        while ( (p = strrchr(buf + offset, DEFAULT_SLASH)) || (offset != 1 && (p = strrchr(buf, DEFAULT_SLASH))) ) {
            int n = 0;

            *p = '\0';
            while (p > buf && *(p-1) == DEFAULT_SLASH) {
                ++n;
                --p;
                *p = '\0';
            }
            if (VCWD_STAT(buf, &sb) == 0) {
                while (1) {
                    *p = DEFAULT_SLASH;
                    if (!n) break;
                    --n;
                    ++p;
                }
                break;
            }
        }
    }

    if (!p) {
        p = buf;
    }
    while (1) {
        int ret = swow_virtual_mkdir(buf, (mode_t) mode);
        if (ret < 0 && errno != EEXIST) {
            if (options & REPORT_ERRORS) {
                php_error_docref(NULL, E_WARNING, "%s", cat_get_last_error_message());
            }
            return 0;
        }

        zend_bool replaced_slash = 0;
        while (++p != e) {
            if (*p == '\0') {
                replaced_slash = 1;
                *p = DEFAULT_SLASH;
                if (*(p+1) != '\0') {
                    break;
                }
            }
        }
        if (p == e || !replaced_slash) {
            /* No more directories to create */
            /* issue a warning to client when the last directory was created failed */
            if (ret < 0) {
                if (options & REPORT_ERRORS) {
                    php_error_docref(NULL, E_WARNING, "%s", cat_get_last_error_message());
                }
                return 0;
            }
            return 1;
        }
    }
}

static int swow_plain_files_rmdir(php_stream_wrapper *wrapper, const char *url, int options, php_stream_context *context)
{
    if (strncasecmp(url, "file://", sizeof("file://") - 1) == 0) {
        url += sizeof("file://") - 1;
    }

    if (php_check_open_basedir(url)) {
        return 0;
    }

#ifdef PHP_WIN32
    if (!swow_win32_check_trailing_space(url, strlen(url))) {
        php_error_docref1(NULL, url, E_WARNING, "%s", strerror(ENOENT));
        return 0;
    }
#endif

    if (swow_virtual_rmdir(url) < 0) {
        php_error_docref1(NULL, url, E_WARNING, "%s", cat_get_last_error_message());
        return 0;
    }

    /* Clear stat cache (and realpath cache) */
    php_clear_stat_cache(1, NULL, 0);

    return 1;
}

static int swow_plain_files_metadata(php_stream_wrapper *wrapper, const char *url, int option, void *value, php_stream_context *context)
{
    struct utimbuf *newtime;
#ifndef PHP_WIN32
    uid_t uid;
    gid_t gid;
#endif
    mode_t mode;
    int ret = 0;

#ifdef PHP_WIN32
    if (!swow_win32_check_trailing_space(url, strlen(url))) {
        php_error_docref1(NULL, url, E_WARNING, "%s", strerror(ENOENT));
        return 0;
    }
#endif

    if (strncasecmp(url, "file://", sizeof("file://") - 1) == 0) {
        url += sizeof("file://") - 1;
    }

    if (php_check_open_basedir(url)) {
        return 0;
    }

    switch(option) {
        case PHP_STREAM_META_TOUCH:
            newtime = (struct utimbuf *)value;
            if (swow_virtual_access(url, F_OK) != 0) {
                int fd = swow_virtual_open(url,  O_TRUNC | O_CREAT);
                if (fd < 0) {
                    php_error_docref1(NULL, url, E_WARNING, "Unable to create file %s because %s", url, cat_get_last_error_message());
                    return 0;
                }
                cat_fs_close(fd);
                UPDATE_ERRNO_FROM_CAT();
            }

            ret = swow_virtual_utime(url, newtime);
            break;
#ifndef PHP_WIN32
        case PHP_STREAM_META_OWNER_NAME:
        case PHP_STREAM_META_OWNER:
            if(option == PHP_STREAM_META_OWNER_NAME) {
                if(php_get_uid_by_name((char *)value, &uid) != SUCCESS) {
                    php_error_docref1(NULL, url, E_WARNING, "Unable to find uid for %s", (char *)value);
                    return 0;
                }
            } else {
                uid = (uid_t)*(long *)value;
            }
            ret = swow_virtual_chown(url, uid, -1);
            break;
        case PHP_STREAM_META_GROUP:
        case PHP_STREAM_META_GROUP_NAME:
            if(option == PHP_STREAM_META_GROUP_NAME) {
                if(php_get_gid_by_name((char *)value, &gid) != SUCCESS) {
                    php_error_docref1(NULL, url, E_WARNING, "Unable to find gid for %s", (char *)value);
                    return 0;
                }
            } else {
                gid = (gid_t)*(long *)value;
            }
            ret = swow_virtual_chown(url, -1, gid);
            break;
#endif
        case PHP_STREAM_META_ACCESS:
            mode = (mode_t)*(zend_long *)value;
            ret = swow_virtual_chmod(url, mode);
            break;
        default:
            zend_value_error("Unknown option %d for stream_metadata", option);
            return 0;
    }
    if (ret == -1) {
        php_error_docref1(NULL, url, E_WARNING, "Operation failed: %s", cat_get_last_error_message());
        return 0;
    }
    php_clear_stat_cache(0, NULL, 0);
    return 1;
}

static const php_stream_wrapper_ops swow_plain_files_wrapper_ops_async = {
    swow_plain_files_stream_opener,
    NULL,
    NULL,
    swow_plain_files_url_stater,
    swow_plain_files_dir_opener,
    "plainfile",
    swow_plain_files_unlink,
    swow_plain_files_rename,
    swow_plain_files_mkdir,
    swow_plain_files_rmdir,
    swow_plain_files_metadata
};

SWOW_API const php_stream_wrapper swow_plain_files_wrapper_async = {
    &swow_plain_files_wrapper_ops_async,
    NULL,
    0
};

/* {{{ php_stream_fopen_with_path */
SWOW_API php_stream *_swow_stream_fopen_with_path(const char *filename, const char *mode, const char *path, zend_string **opened_path, int options STREAMS_DC)
{
    /* code ripped off from fopen_wrappers.c */
    char *pathbuf, *end;
    const char *ptr;
    char trypath[MAXPATHLEN];
    php_stream *stream;
    size_t filename_length;
    zend_string *exec_filename;

    if (opened_path) {
        *opened_path = NULL;
    }

    if(!filename) {
        return NULL;
    }

    filename_length = strlen(filename);
#ifndef PHP_WIN32
    (void) filename_length;
#endif

    /* Relative path open */
    if (*filename == '.' && (IS_SLASH(filename[1]) || filename[1] == '.')) {
        /* further checks, we could have ....... filenames */
        ptr = filename + 1;
        if (*ptr == '.') {
            while (*(++ptr) == '.');
            if (!IS_SLASH(*ptr)) { /* not a relative path after all */
                goto not_relative_path;
            }
        }


        if (((options & STREAM_DISABLE_OPEN_BASEDIR) == 0) && php_check_open_basedir(filename)) {
            return NULL;
        }

        return _swow_stream_fopen(filename, mode, opened_path, options STREAMS_REL_CC);
    }

not_relative_path:

    /* Absolute path open */
    if (IS_ABSOLUTE_PATH(filename, filename_length)) {

        if (((options & STREAM_DISABLE_OPEN_BASEDIR) == 0) && php_check_open_basedir(filename)) {
            return NULL;
        }

        return _swow_stream_fopen(filename, mode, opened_path, options STREAMS_REL_CC);
    }

#ifdef PHP_WIN32
    if (IS_SLASH(filename[0])) {
        size_t cwd_len;
        char *cwd;
        cwd = virtual_getcwd_ex(&cwd_len);
        /* getcwd() will return always return [DRIVE_LETTER]:/) on windows. */
        *(cwd+3) = '\0';

        if (snprintf(trypath, MAXPATHLEN, "%s%s", cwd, filename) >= MAXPATHLEN) {
            php_error_docref(NULL, E_NOTICE, "%s/%s path was truncated to %d", cwd, filename, MAXPATHLEN);
        }

        efree(cwd);

        if (((options & STREAM_DISABLE_OPEN_BASEDIR) == 0) && php_check_open_basedir(trypath)) {
            return NULL;
        }

        return _swow_stream_fopen(trypath, mode, opened_path, options STREAMS_REL_CC);
    }
#endif

    if (!path || !*path) {
        return _swow_stream_fopen(filename, mode, opened_path, options STREAMS_REL_CC);
    }

    /* check in provided path */
    /* append the calling scripts' current working directory
     * as a fall back case
     */
    if (zend_is_executing() &&
        (exec_filename = zend_get_executed_filename_ex()) != NULL) {
        const char *exec_fname = ZSTR_VAL(exec_filename);
        size_t exec_fname_length = ZSTR_LEN(exec_filename);

        while ((--exec_fname_length < SIZE_MAX) && !IS_SLASH(exec_fname[exec_fname_length]));
        if (exec_fname_length<=0) {
            /* no path */
            pathbuf = estrdup(path);
        } else {
            size_t path_length = strlen(path);

            pathbuf = (char *) emalloc(exec_fname_length + path_length +1 +1);
            memcpy(pathbuf, path, path_length);
            pathbuf[path_length] = DEFAULT_DIR_SEPARATOR;
            memcpy(pathbuf+path_length+1, exec_fname, exec_fname_length);
            pathbuf[path_length + exec_fname_length +1] = '\0';
        }
    } else {
        pathbuf = estrdup(path);
    }

    ptr = pathbuf;

    while (ptr && *ptr) {
        end = strchr(ptr, DEFAULT_DIR_SEPARATOR);
        if (end != NULL) {
            *end = '\0';
            end++;
        }
        if (*ptr == '\0') {
            goto stream_skip;
        }
        if (snprintf(trypath, MAXPATHLEN, "%s/%s", ptr, filename) >= MAXPATHLEN) {
            php_error_docref(NULL, E_NOTICE, "%s/%s path was truncated to %d", ptr, filename, MAXPATHLEN);
        }

        if (((options & STREAM_DISABLE_OPEN_BASEDIR) == 0) && php_check_open_basedir_ex(trypath, 0)) {
            goto stream_skip;
        }

        stream = _swow_stream_fopen(trypath, mode, opened_path, options STREAMS_REL_CC);
        if (stream) {
            efree(pathbuf);
            return stream;
        }
stream_skip:
        ptr = end;
    } /* end provided path */

    efree(pathbuf);
    return NULL;

}
/* }}} */
