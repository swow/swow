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

#ifndef CAT_FS_H
#define CAT_FS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#ifndef CAT_OS_WIN
# include <sys/file.h>
#endif

typedef uv_file cat_file_t;
#define CAT_FS_FILE_FMT "%d"
#define CAT_FS_FILE_FMT_SPEC "d"
#define CAT_FS_INVALID_FILE -1

#define CAT_FS_OPEN_FLAG_MAP(XX) \
    XX(APPEND) \
    XX(CREAT) \
    XX(DIRECT) \
    XX(DIRECTORY) \
    XX(DSYNC) \
    XX(EXCL) \
    XX(EXLOCK) \
    XX(NOATIME) \
    XX(NOCTTY) \
    XX(NOFOLLOW) \
    XX(NONBLOCK) \
    XX(RDONLY) \
    XX(RDWR) \
    XX(SYMLINK) \
    XX(SYNC) \
    XX(TRUNC) \
    XX(WRONLY) \
    XX(FILEMAP) \
    XX(RANDOM) \
    XX(SHORT_LIVED) \
    XX(SEQUENTIAL) \
    XX(TEMPORARY)

typedef enum cat_fs_open_flag_e {
#define CAT_FS_OPEN_FLAG_GEN(name) CAT_FS_OPEN_FLAG_##name = UV_FS_O_##name,
  CAT_FS_OPEN_FLAG_MAP(CAT_FS_OPEN_FLAG_GEN)
#undef CAT_FS_OPEN_FLAG_GEN
} cat_fs_open_flag_t;

typedef int cat_fs_open_flags_t;
#define CAT_FS_OPEN_FLAGS_FMT "%d"
#define CAT_FS_OPEN_FLAGS_FMT_SPEC "d"

CAT_API cat_file_t cat_fs_open(const char *path, cat_fs_open_flags_t flags, ...);
CAT_API int cat_fs_close(cat_file_t fd);
CAT_API ssize_t cat_fs_read(cat_file_t fd, void *buffer, size_t size);
CAT_API ssize_t cat_fs_write(cat_file_t fd, const void *buffer, size_t length);
CAT_API ssize_t cat_fs_pread(cat_file_t fd, void *buffer, size_t size, off_t offset);
CAT_API ssize_t cat_fs_pwrite(cat_file_t fd, const void *buffer, size_t length, off_t offset);
CAT_API off_t cat_fs_lseek(cat_file_t fd, off_t offset, int whence);
CAT_API int cat_fs_fsync(cat_file_t fd);
CAT_API int cat_fs_fdatasync(cat_file_t fd);
CAT_API int cat_fs_ftruncate(cat_file_t fd, int64_t offset);

/*
  Note: fopen(3) is not fork-safe,
  back fd for a FILE struct may be modified without modifing itself
  Use cat_fs_open to get a fd, manually manage the fd,
  then use fdopen(3) to make FILE struct up for other C stream functions.
*/
/* TODO: implement this */
// CAT_API FILE *cat_fs_fopen(const char *path, const char *mode);

/*
 * Note: fclose will also close the backing fd that the struct used
 */
CAT_API int cat_fs_fclose(FILE *stream);

CAT_API size_t cat_fs_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
CAT_API size_t cat_fs_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
CAT_API int cat_fs_fseek(FILE *stream, off_t offset, int whence);
CAT_API off_t cat_fs_ftell(FILE *stream);
CAT_API int cat_fs_fflush(FILE *stream);

#define CAT_DIRENT_TYPE_MAP(XX) \
    XX(FILE) \
    XX(DIR) \
    XX(LINK) \
    XX(FIFO) \
    XX(SOCKET) \
    XX(CHAR) \
    XX(BLOCK)

typedef enum cat_dirent_type_e {
    CAT_DIRENT_TYPE_UNKNOWN = UV_DIRENT_UNKNOWN,
#define CAT_DIRENT_TYPE_GEN(name) CAT_DIRENT_TYPE_##name = UV_DIRENT_##name,
    CAT_DIRENT_TYPE_MAP(CAT_DIRENT_TYPE_GEN)
#undef CAT_DIRENT_TYPE_GEN
} cat_dirent_type_t;

typedef struct {
  const char* name;
  cat_dirent_type_t type;
} cat_dirent_t;
typedef void cat_dir_t;

CAT_API cat_dir_t *cat_fs_opendir(const char *path);
CAT_API cat_dirent_t *cat_fs_readdir(cat_dir_t *dir);
CAT_API void cat_fs_rewinddir(cat_dir_t *dir);
CAT_API int cat_fs_closedir(cat_dir_t *dir);

typedef int (*cat_dirent_filter_t)(const cat_dirent_t *dirent);
typedef int (*cat_dirent_compar_t)(const cat_dirent_t *dirent1, const cat_dirent_t *dirent2);
CAT_API int cat_fs_scandir(const char *path, cat_dirent_t **namelist, cat_dirent_filter_t filter, cat_dirent_compar_t compar);
CAT_API int cat_fs_mkdir(const char *path, int mode);
CAT_API int cat_fs_rmdir(const char *path);

CAT_API int cat_fs_rename(const char *path, const char *new_path);
CAT_API int cat_fs_unlink(const char *path);


CAT_API int cat_fs_access(const char *path, int mode);

typedef uv_stat_t cat_stat_t;
typedef uv_statfs_t cat_statfs_t;
CAT_API int cat_fs_stat(const char *path, cat_stat_t *statbuf);
CAT_API int cat_fs_lstat(const char *path, cat_stat_t *statbuf);
CAT_API int cat_fs_fstat(cat_file_t fd, cat_stat_t *statbuf);

CAT_API int cat_fs_utime(const char *path, double atime, double mtime);
CAT_API int cat_fs_lutime(const char *path, double atime, double mtime);
CAT_API int cat_fs_futime(cat_file_t fd, double atime, double mtime);

#define CAT_FS_SYMLINK_FLAG_MAP(XX) \
    /* This flag can be used with uv_fs_symlink() on Windows to specify whether */ \
    /* path argument points to a directory. */ \
    XX(DIR) \
    /* This flag can be used with uv_fs_symlink() on Windows to specify whether */ \
    /* the symlink is to be created using junction points. */ \
    XX(JUNCTION)

typedef enum cat_fs_symlink_flag_e {
#define CAT_FS_SYMLINK_FLAG_GEN(name) CAT_FS_SYMLINK_FLAG_##name = UV_FS_SYMLINK_##name,
    CAT_FS_SYMLINK_FLAG_MAP(CAT_FS_SYMLINK_FLAG_GEN)
#undef CAT_FS_SYMLINK_FLAG_GEN
} cat_fs_symlink_flag_t;

typedef int cat_fs_symlink_flags_t;
#define CAT_FS_SYMLINK_FLAGS_FMT "%d"
#define CAT_FS_SYMLINK_FLAGS_FMT_SPEC "d"

CAT_API int cat_fs_link(const char *path, const char *new_path);
CAT_API int cat_fs_symlink(const char *path, const char *new_path, cat_fs_symlink_flags_t flags);

CAT_API int cat_fs_readlink(const char *path, char *buffer, size_t size);
CAT_API char *cat_fs_realpath(const char *path, char *buffer);

CAT_API int cat_fs_chmod(const char *path, int mode);
CAT_API int cat_fs_fchmod(cat_file_t fd, int mode);
CAT_API int cat_fs_chown(const char *path, cat_uid_t uid, cat_gid_t gid);
CAT_API int cat_fs_fchown(cat_file_t fd, cat_uid_t uid, cat_gid_t gid);
CAT_API int cat_fs_lchown(const char *path, cat_uid_t uid, cat_gid_t gid);

#define  CAT_FS_COPYFILE_FLAG_MAP(XX) \
    /* This flag can be used with uv_fs_copyfile() to return an error if the */ \
    /* destination already exists. */ \
    XX(EXCL) \
    /* This flag can be used with uv_fs_copyfile() to attempt to create a reflink. */ \
    /* If copy-on-write is not supported, a fallback copy mechanism is used. */ \
    XX(FICLONE) \
    /* This flag can be used with uv_fs_copyfile() to attempt to create a reflink. */ \
    /* If copy-on-write is not supported, an error is returned. */ \
    XX(FICLONE_FORCE)

typedef enum cat_fs_copyfile_flag_e {
    CAT_FS_COPYFILE_FLAG_NONE = 0,
#define CAT_FS_COPYFILE_FLAG_GEN(name) CAT_FS_COPYFILE_FLAG_##name = UV_FS_COPYFILE_##name,
    CAT_FS_COPYFILE_FLAG_MAP(CAT_FS_COPYFILE_FLAG_GEN)
#undef CAT_FS_COPYFILE_FLAG_GEN
} cat_fs_copyfile_flag_t;

typedef int cat_fs_copyfile_flags_t;
#define CAT_FS_COPYFILE_FLAGS_FMT "%d"
#define CAT_FS_COPYFILE_FLAGS_FMT_SPEC "d"

CAT_API int cat_fs_copyfile(const char *path, const char *new_path, cat_fs_copyfile_flags_t flags);

CAT_API int cat_fs_sendfile(cat_file_t out_fd, cat_file_t in_fd, int64_t in_offset, size_t length);
/*
* fs_mkdtemp - like mkdtemp(3) but the returned path is not same as inputed template
* @note: you must use the return str before next event loop
*/
CAT_API const char *cat_fs_mkdtemp(const char *tpl);
CAT_API cat_file_t cat_fs_mkstemp(const char *tpl);
CAT_API int cat_fs_statfs(const char *path, cat_statfs_t *buf);

#define CAT_FS_FLOCK_FLAG_MAP(XX) \
    XX(SHARED) \
    XX(EXCLUSIVE) \
    XX(NONBLOCK) \
    XX(UNLOCK)

typedef enum cat_fs_flock_flag_e {
#ifdef LOCK_SH
# define _CAT_FS_FLOCK_FLAG_SHARED LOCK_SH
#else
# define _CAT_FS_FLOCK_FLAG_SHARED 0x1
#endif // LOCK_SH
#ifdef LOCK_EX
# define _CAT_FS_FLOCK_FLAG_EXCLUSIVE LOCK_EX
#else
# define _CAT_FS_FLOCK_FLAG_EXCLUSIVE 0x2
#endif // LOCK_EX
#ifdef LOCK_NB
# define _CAT_FS_FLOCK_FLAG_NONBLOCK LOCK_NB
#else
# define _CAT_FS_FLOCK_FLAG_NONBLOCK 0x4
#endif // LOCK_NB
#ifdef LOCK_UN
# define _CAT_FS_FLOCK_FLAG_UNLOCK LOCK_UN
#else
# define _CAT_FS_FLOCK_FLAG_UNLOCK 0x8
#endif // LOCK_UN
#define CAT_FS_FLOCK_FLAG_GEN(name) CAT_FS_FLOCK_FLAG_##name = _CAT_FS_FLOCK_FLAG_##name,
    CAT_FS_FLOCK_FLAG_MAP(CAT_FS_FLOCK_FLAG_GEN)
#undef CAT_FS_FLOCK_FLAG_GEN
#undef _CAT_FS_FLOCK_FLAG_SHARED
#undef _CAT_FS_FLOCK_FLAG_EXCLUSIVE
#undef _CAT_FS_FLOCK_FLAG_NONBLOCK
#undef _CAT_FS_FLOCK_FLAG_UNLOCK
} cat_fs_flock_flag_t;

typedef int cat_fs_flock_flags_t;
#define CAT_FS_FLOCK_FLAGS_FMT "%d"
#define CAT_FS_FLOCK_FLAGS_FMT_SPEC "d"

CAT_API int cat_fs_flock(cat_file_t fd, cat_fs_flock_flags_t operation);

CAT_API char *cat_fs_get_contents(const char *filename, size_t *length);
CAT_API ssize_t cat_fs_put_contents(const char *filename, const char *content, size_t length);

#ifdef __cplusplus
}
#endif
#endif /* CAT_FS_H */
