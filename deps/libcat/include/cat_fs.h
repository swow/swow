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
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

CAT_API int cat_fs_open(const char *path, int flags, ...);
CAT_API off_t cat_lseek(int fd, off_t offset, int whence);
CAT_API ssize_t cat_fs_read(int fd, void *buffer, size_t size);
CAT_API ssize_t cat_fs_write(int fd, const void *buffer, size_t length);
CAT_API int cat_fs_close(int fd);

CAT_API int cat_fs_access(const char *path, int mode);
CAT_API int cat_fs_mkdir(const char *path, int mode);
CAT_API int cat_fs_rmdir(const char *path);
CAT_API int cat_fs_rename(const char *path, const char *new_path);
CAT_API int cat_fs_unlink(const char *path);

#ifdef __cplusplus
}
#endif
#endif /* CAT_FS_H */
