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

typedef union
{
    cat_coroutine_t *coroutine;
    uv_req_t req;
    uv_fs_t fs;
} cat_fs_context_t;

#define CAT_FS_DO_RESULT_EX(on_fail, on_done, operation, ...) do{\
    cat_fs_context_t *context = (cat_fs_context_t *) cat_malloc(sizeof(*context)); \
    if (unlikely(context == NULL)) { \
        cat_update_last_error_of_syscall("Malloc for file-system context failed"); \
        {on_fail} \
    } \
    cat_bool_t done; \
    cat_bool_t ret; \
    int error = uv_fs_##operation(cat_event_loop, &context->fs, ##__VA_ARGS__, cat_fs_callback); \
    if (error != 0) { \
        cat_update_last_error_with_reason(error, "File-System " #operation " init failed"); \
        cat_free(context); \
        {on_fail} \
    } \
    context->coroutine = CAT_COROUTINE_G(current); \
    ret = cat_time_wait(-1); \
    done = context->coroutine == NULL; \
    context->coroutine = NULL; \
    if (unlikely(!ret)) { \
        cat_update_last_error_with_previous("File-System " #operation " wait failed"); \
        (void) uv_cancel(&context->req); \
        {on_fail} \
    } \
    if (unlikely(!done)) { \
        cat_update_last_error(CAT_ECANCELED, "File-System " #operation " has been canceled"); \
        (void) uv_cancel(&context->req); \
        {on_fail} \
    } \
    if (unlikely(context->fs.result < 0)) { \
        cat_update_last_error_with_reason((cat_errno_t) context->fs.result, "File-System " #operation " failed"); \
        {on_fail} \
    } \
    {on_done} \
} while (0)

#define CAT_FS_DO_RESULT(return_type, operation, ...) CAT_FS_DO_RESULT_EX({return -1;}, {return (return_type)context->fs.result;}, operation, __VA_ARGS__)

static void cat_fs_callback(uv_fs_t *fs)
{
    cat_fs_context_t *context = cat_container_of(fs, cat_fs_context_t, fs);

    if (context->coroutine != NULL) {
        cat_coroutine_t *coroutine = context->coroutine;
        context->coroutine = NULL;
        if (unlikely(!cat_coroutine_resume(coroutine, NULL, NULL))) {
            cat_core_error_with_last(FS, "File-System schedule failed");
        }
    }

    uv_fs_req_cleanup(&context->fs);
    cat_free(context);
}

// basic functions for fs io
// open, close, read, write, lseek

CAT_API cat_file_t cat_fs_open(const char *path, int flags, ...)
{
    va_list args;
    int mode = 0666;

    if (flags & O_CREAT) {
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }

    CAT_FS_DO_RESULT(cat_file_t, open, path, flags, mode);
}

CAT_API off_t cat_fs_lseek(cat_file_t fd, off_t offset, int whence)
{
    return lseek(fd, offset, whence);
}

CAT_API ssize_t cat_fs_read(cat_file_t fd, void *buffer, size_t size)
{
    uv_buf_t buf = uv_buf_init((char *) buffer, (unsigned int) size);

    CAT_FS_DO_RESULT(ssize_t, read, fd, &buf, 1, 0);
}

CAT_API ssize_t cat_fs_write(cat_file_t fd, const void *buffer, size_t length)
{
    uv_buf_t buf = uv_buf_init((char *) buffer, (unsigned int) length);

    CAT_FS_DO_RESULT(ssize_t, write, fd, &buf, 1, 0);
}

CAT_API int cat_fs_close(cat_file_t fd)
{
    CAT_FS_DO_RESULT(int, close, fd);
}

CAT_API int cat_fs_fsync(cat_file_t fd){
    CAT_FS_DO_RESULT(int, fsync, fd);
}

CAT_API int cat_fs_fdatasync(cat_file_t fd){
    CAT_FS_DO_RESULT(int, fdatasync, fd);
}

CAT_API int cat_fs_ftruncate(cat_file_t fd, int64_t offset){
    CAT_FS_DO_RESULT(int, ftruncate, fd, offset);
}

// basic dir operations
// opendir, readdir, closedir, scandir
CAT_API cat_dir_t *cat_fs_opendir(const char* path){
    CAT_FS_DO_RESULT_EX({return NULL;}, {
        return (cat_dir_t *)context->fs.ptr;
    }, opendir, path);
}

/*
* cat_fs_readdirs: like readdir(3), but with multi entries
* Note: you should do free(dir->dirents[x].name), free(dir->dirents[x]) and free(dir->dirents)
*/
CAT_API int cat_fs_readdirs(cat_dir_t* dir, uv_dirent_t *dirents, size_t nentries){
    ((uv_dir_t *)dir)->dirents = dirents;
    ((uv_dir_t *)dir)->nentries = nentries;
    CAT_FS_DO_RESULT_EX({return -1;}, {
        // we donot duplicate names, that's hacky
        // better duplicate it, then uv__free original, then return our duplication
        // however we donot have uv__free

        // clean up dir struct to avoid uv's freeing names
        ((uv_dir_t *)dir)->dirents = NULL;
        ((uv_dir_t *)dir)->nentries = 0;
        int ret = (int)context->fs.result;
        context->fs.result = 0;
        return ret;
    }, readdir, dir);
}

/*
* cat_fs_readdir: like readdir(3), but return cat_dirent_t
* Note: you should do both free(retval->name) and free(retval)
*/
CAT_API uv_dirent_t* cat_fs_readdir(cat_dir_t* dir){
    uv_dirent_t *dirent = malloc(sizeof(*dirent));
    int ret = cat_fs_readdirs(dir, dirent, 1);
    if(1 != ret){
        free(dirent);
        return NULL;
    }
    return dirent;
}

CAT_API int cat_fs_closedir(cat_dir_t* dir){
    CAT_FS_DO_RESULT(int, closedir, dir);
}

static uv_fs_t* cat_fs_uv_scandir(const char* path, int flags){
    CAT_FS_DO_RESULT_EX({return NULL;}, {
        return &context->fs;
    }, scandir, path, flags/* no documents/source code coments refer to this, what is this ?*/);
}

/*
* cat_fs_scandir: like scandir(3), but with cat_dirent_t
* Note: you should do free(namelist[x].name), free(namelist[x]) and free(namelist)
*/
CAT_API int cat_fs_scandir(const char* path, cat_dirent_t ** namelist,
  int (*filter)(const cat_dirent_t *),
  int (*compar)(const cat_dirent_t *, const cat_dirent_t *)){

	cat_dir_t *dir = NULL;
    if (!(dir = cat_fs_opendir(path))) {
        // failed open dir
        return -1;
    }

    uv_fs_t *req = NULL;
    if(!(req = cat_fs_uv_scandir(path, 0))){
        // failed scandir
        return -1;
    }

    int ret = -1;
    cat_dirent_t dirent, *tmp=NULL;
    int cnt=0, len=0;

    while ((ret = uv_fs_scandir_next(req, &dirent)) == 0) {
		if (filter && !filter(&dirent)) {
            continue;
        }
		if (cnt >= len) {
			len = 2*len+1;
			tmp = realloc(tmp, len * sizeof(*tmp));
			if (!tmp) {
                cat_update_last_error(CAT_ENOMEM, "Cannot allocate memory");
                for(cnt--; cnt>=0; cnt--){
                    free((void*)tmp[cnt].name);
                }
                free(tmp);
                return -1;
            };
		}
		tmp[cnt].name = strdup(dirent.name);
        //printf("%s: %p\n", tmp[cnt].name, tmp[cnt].name);
		tmp[cnt++].type = dirent.type;
	}

    cat_fs_closedir(dir);

	if(compar) {
        qsort(tmp, cnt, sizeof(*tmp), (int (*)(const void *, const void *))compar);
    }
	*namelist = tmp;
	return cnt;
}

// directory/file operations
// mkdir, rmdir, rename, unlink

CAT_API int cat_fs_mkdir(const char *path, int mode)
{
    CAT_FS_DO_RESULT(int, mkdir, path, mode);
}

CAT_API int cat_fs_rmdir(const char *path)
{
    CAT_FS_DO_RESULT(int, rmdir, path);
}

CAT_API int cat_fs_rename(const char *path, const char *new_path)
{
    CAT_FS_DO_RESULT(int, rename, path, new_path);
}

CAT_API int cat_fs_unlink(const char *path)
{
    CAT_FS_DO_RESULT(int, unlink, path);
}

// file info utils
// access, stat(s), utime(s)

CAT_API int cat_fs_access(const char *path, int mode)
{
    CAT_FS_DO_RESULT(int, access, path, mode);
}

#define CAT_FS_DO_STAT(name, target) \
    CAT_FS_DO_RESULT_EX({return -1;}, {memcpy(buf, &context->fs.statbuf, sizeof(uv_stat_t)); return 0;}, name, target)

CAT_API int cat_fs_stat(const char * path, cat_stat_t * buf){
    CAT_FS_DO_STAT(stat, path);
}

CAT_API int cat_fs_lstat(const char * path, cat_stat_t * buf){
    CAT_FS_DO_STAT(lstat, path);
}

CAT_API int cat_fs_fstat(cat_file_t fd, cat_stat_t * buf){
    CAT_FS_DO_STAT(fstat, fd);
}

CAT_API int cat_fs_utime(const char* path, double atime, double mtime){
    CAT_FS_DO_RESULT(int, utime, path, atime, mtime);
}

CAT_API int cat_fs_lutime(const char* path, double atime, double mtime){
    CAT_FS_DO_RESULT(int, lutime, path, atime, mtime);
}

CAT_API int cat_fs_futime(cat_file_t fd, double atime, double mtime){
    CAT_FS_DO_RESULT(int, futime, fd, atime, mtime);
}

// hard link and symbol link
// link, symlink, readlink, realpath

CAT_API int cat_fs_link(const char * path, const char * new_path){
    CAT_FS_DO_RESULT(int, link, path, new_path);
}

CAT_API int cat_fs_symlink(const char * path, const char * new_path, int flags){
    CAT_FS_DO_RESULT(int, symlink, path, new_path, flags);
}

#ifdef CAT_OS_WIN
#   define PATH_MAX 32768
#endif
CAT_API int cat_fs_readlink(const char * pathname, char * buf, size_t len){
    CAT_FS_DO_RESULT_EX({return (int)-1;}, {
        size_t ret = strnlen(context->fs.ptr, PATH_MAX);
        if(ret > len){
            // will truncate
            ret = len;
        }
        strncpy(buf, context->fs.ptr, len);
        return (int)ret;
    }, readlink, pathname);
}

CAT_API char * cat_fs_realpath(const char *pathname, char* buf){
    CAT_FS_DO_RESULT_EX({return NULL;}, {
        if(NULL == buf){
            return strdup(context->fs.ptr);
        }
        strcpy(buf, context->fs.ptr);
        return buf;
    }, realpath, pathname);
}
#ifdef CAT_OS_WIN
#   undef PATH_MAX
#endif

// permissions
// chmod(s), chown(s)

CAT_API int cat_fs_chmod(const char *path, int mode){
    CAT_FS_DO_RESULT(int, chmod, path, mode);
}

CAT_API int cat_fs_fchmod(cat_file_t fd, int mode){
    CAT_FS_DO_RESULT(int, fchmod, fd, mode);
}

CAT_API int cat_fs_chown(const char *path, cat_uid_t uid, cat_gid_t gid){
    CAT_FS_DO_RESULT(int, chown, path, uid, gid);
}

CAT_API int cat_fs_fchown(cat_file_t fd, cat_uid_t uid, cat_gid_t gid){
    CAT_FS_DO_RESULT(int, fchown, fd, uid, gid);
}

CAT_API int cat_fs_lchown(const char *path, cat_uid_t uid, cat_gid_t gid){
    CAT_FS_DO_RESULT(int, lchown, path, uid, gid);
}

// miscellaneous
// copyfile
CAT_API int cat_fs_copyfile(const char* path, const char* new_path, int flags){
    CAT_FS_DO_RESULT(int, copyfile, path, new_path, flags);
}

CAT_API int cat_fs_sendfile(cat_file_t out_fd, cat_file_t in_fd, int64_t in_offset, size_t length){
    CAT_FS_DO_RESULT(int, sendfile, out_fd, in_fd, in_offset, length);
}

/*
* cat_fs_mkdtemp - like mkdtemp(3) but the returned path is not same as inputed template
*/
CAT_API const char* cat_fs_mkdtemp(const char* tpl){
    CAT_FS_DO_RESULT_EX({return NULL;}, {
        if(0 != context->fs.result){
            return NULL;
        }
        return context->fs.path;
    }, mkdtemp, tpl);
}

CAT_API int cat_fs_mkstemp(const char* tpl){
    CAT_FS_DO_RESULT_EX({return -1;}, {
        return (int)context->fs.result;
    }, mkstemp, tpl);
}

CAT_API int cat_fs_statfs(const char* path, cat_statfs_t* buf){
    CAT_FS_DO_RESULT_EX({return -1;}, {
        memcpy(buf, context->fs.ptr, sizeof(*buf));
        return 0;
    }, statfs, path);
}
