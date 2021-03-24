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

#ifndef CAT_FS_H
#define CAT_FS_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#define CAT_FS_COPYFILE_EXCL          UV_FS_COPYFILE_EXCL
#define CAT_FS_COPYFILE_FICLONE       UV_FS_COPYFILE_FICLONE
#define CAT_FS_COPYFILE_FICLONE_FORCE UV_FS_COPYFILE_FICLONE_FORCE

#define CAT_FS_O_APPEND       UV_FS_O_APPEND
#define CAT_FS_O_CREAT        UV_FS_O_CREAT
#define CAT_FS_O_DIRECT       UV_FS_O_DIRECT
#define CAT_FS_O_DIRECTORY    UV_FS_O_DIRECTORY
#define CAT_FS_O_DSYNC        UV_FS_O_DSYNC
#define CAT_FS_O_EXCL         UV_FS_O_EXCL
#define CAT_FS_O_EXLOCK       UV_FS_O_EXLOCK
#define CAT_FS_O_NOATIME      UV_FS_O_NOATIME
#define CAT_FS_O_NOCTTY       UV_FS_O_NOCTTY
#define CAT_FS_O_NOFOLLOW     UV_FS_O_NOFOLLOW
#define CAT_FS_O_NONBLOCK     UV_FS_O_NONBLOCK
#define CAT_FS_O_RDONLY       UV_FS_O_RDONLY
#define CAT_FS_O_RDWR         UV_FS_O_RDWR
#define CAT_FS_O_SYMLINK      UV_FS_O_SYMLINK
#define CAT_FS_O_SYNC         UV_FS_O_SYNC
#define CAT_FS_O_TRUNC        UV_FS_O_TRUNC
#define CAT_FS_O_WRONLY       UV_FS_O_WRONLY
#define CAT_FS_O_FILEMAP      UV_FS_O_FILEMAP
#define CAT_FS_O_RANDOM       UV_FS_O_RANDOM
#define CAT_FS_O_SHORT_LIVED  UV_FS_O_SHORT_LIVED
#define CAT_FS_O_SEQUENTIAL   UV_FS_O_SEQUENTIAL
#define CAT_FS_O_TEMPORARY    UV_FS_O_TEMPORARY

#define CAT_FS_SYMLINK_DIR        UV_FS_SYMLINK_DIR
#define CAT_FS_SYMLINK_JUNCTION   UV_FS_SYMLINK_JUNCTION

typedef uv_stat_t cat_stat_t;
typedef uv_statfs_t cat_statfs_t;
typedef uv_uid_t cat_uid_t;
typedef uv_gid_t cat_gid_t;
typedef uv_file cat_file_t;

#define CAT_DIRENT_UNKNOWN    UV_DIRENT_UNKNOWN
#define CAT_DIRENT_FILE       UV_DIRENT_FILE
#define CAT_DIRENT_DIR        UV_DIRENT_DIR
#define CAT_DIRENT_LINK       UV_DIRENT_LINK
#define CAT_DIRENT_FIFO       UV_DIRENT_FIFO
#define CAT_DIRENT_SOCKET     UV_DIRENT_SOCKET
#define CAT_DIRENT_CHAR       UV_DIRENT_CHAR
#define CAT_DIRENT_BLOCK      UV_DIRENT_BLOCK
typedef uv_dirent_t cat_dirent_t;
typedef void cat_dir_t;

CAT_API cat_file_t cat_fs_open(const char *path, int flags, ...);
CAT_API ssize_t cat_fs_pread(cat_file_t fd, void *buffer, size_t size, off_t offset);
CAT_API ssize_t cat_fs_pwrite(cat_file_t fd, const void *buffer, size_t length, off_t offset);
CAT_API ssize_t cat_fs_read(cat_file_t fd, void *buffer, size_t size);
CAT_API ssize_t cat_fs_write(cat_file_t fd, const void *buffer, size_t length);
CAT_API int cat_fs_close(cat_file_t fd);
CAT_API off_t cat_fs_lseek(cat_file_t fd, off_t offset, int whence);
CAT_API int cat_fs_fsync(cat_file_t fd);
CAT_API int cat_fs_fdatasync(cat_file_t fd);
CAT_API int cat_fs_ftruncate(cat_file_t fd, int64_t offset);

CAT_API cat_dir_t *cat_fs_opendir(const char* path);
CAT_API uv_dirent_t* cat_fs_readdir(cat_dir_t* dir);
CAT_API void cat_fs_rewinddir(cat_dir_t* dir);
CAT_API int cat_fs_closedir(cat_dir_t* dir);
CAT_API int cat_fs_scandir(const char* path, cat_dirent_t ** namelist,
  int (*filter)(const cat_dirent_t *),
  int (*compar)(const cat_dirent_t *, const cat_dirent_t *));

CAT_API int cat_fs_access(const char *path, int mode);
CAT_API int cat_fs_stat(const char* path, cat_stat_t * buf);
CAT_API int cat_fs_lstat(const char* path, cat_stat_t * buf);
CAT_API int cat_fs_fstat(cat_file_t fd, cat_stat_t * buf);
CAT_API int cat_fs_utime(const char* path, double atime, double mtime);
CAT_API int cat_fs_lutime(const char* path, double atime, double mtime);
CAT_API int cat_fs_futime(cat_file_t fd, double atime, double mtime);

CAT_API int cat_fs_mkdir(const char *path, int mode);
CAT_API int cat_fs_rmdir(const char *path);
CAT_API int cat_fs_rename(const char *path, const char *new_path);
CAT_API int cat_fs_unlink(const char *path);

CAT_API int cat_fs_link(const char * path, const char * new_path);
CAT_API int cat_fs_symlink(const char * path, const char * new_path, int flags);

CAT_API int cat_fs_readlink(const char * pathname, char * buf, size_t len);
CAT_API char * cat_fs_realpath(const char *pathname, char* buf);

CAT_API int cat_fs_chmod(const char *path, int mode);
CAT_API int cat_fs_fchmod(cat_file_t fd, int mode);
CAT_API int cat_fs_chown(const char *path, cat_uid_t uid, cat_gid_t gid);
CAT_API int cat_fs_fchown(cat_file_t fd, cat_uid_t uid, cat_gid_t gid);
CAT_API int cat_fs_lchown(const char *path, cat_uid_t uid, cat_gid_t gid);

CAT_API int cat_fs_copyfile(const char* path, const char* new_path, int flags);
CAT_API int cat_fs_sendfile(cat_file_t out_fd, cat_file_t in_fd, int64_t in_offset, size_t length);
CAT_API const char* cat_fs_mkdtemp(const char* tpl);
CAT_API cat_file_t cat_fs_mkstemp(const char* tpl);
CAT_API int cat_fs_statfs(const char* path, cat_statfs_t* buf);

#ifdef __cplusplus
}
#endif
#endif /* CAT_FS_H */
