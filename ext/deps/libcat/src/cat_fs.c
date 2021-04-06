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
  | Author: dixyes <dixyes@gmail.com>                                        |
  +--------------------------------------------------------------------------+
 */

#include "cat_fs.h"
#include "cat_coroutine.h"
#include "cat_event.h"
#include "cat_time.h"
#include "cat_work.h"
#include "uv.h"
#ifdef CAT_OS_WIN
#   include <winternl.h>
#endif

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
// open, close, read, write

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
#ifndef CAT_OS_WIN
CAT_API cat_dir_t *cat_fs_opendir(const char* path){
    CAT_FS_DO_RESULT_EX({return NULL;}, {
        return (cat_dir_t *)context->fs.ptr;
    }, opendir, path);
}

/*
* cat_fs_readdirs: like readdir(3), but with multi entries
* Note: you should do free(dir->dirents[x].name), free(dir->dirents[x]) and free(dir->dirents)
*/
/*
static int cat_fs_readdirs(cat_dir_t* dir, uv_dirent_t *dirents, size_t nentries){
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
*/

CAT_API int cat_fs_closedir(cat_dir_t* dir){
    CAT_FS_DO_RESULT(int, closedir, dir);
}
#endif

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
			void* _tmp = realloc(tmp, len * sizeof(*tmp));
            if (!_tmp) {
                cat_update_last_error(CAT_ENOMEM, "Cannot allocate memory");
                for (cnt--; cnt >= 0; cnt--) {
                    free((void*)tmp[cnt].name);
                }
                free(tmp);
                return -1;
            }
            tmp = _tmp;
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
    //CAT_FS_DO_STAT(stat, path);
    cat_fs_context_t* context = (cat_fs_context_t*)cat_malloc(sizeof(*context));
    if (unlikely(context == NULL)) {
        cat_update_last_error_of_syscall("Malloc for file-system context failed");
        {return -1;}
    }
    cat_bool_t done;
    cat_bool_t ret;
    int error = uv_fs_stat(cat_event_loop, & context->fs, path, cat_fs_callback);
    if (error != 0) {
        cat_update_last_error_with_reason(error, "File-System stat init failed");
        cat_free(context);
        {return -1; }
    }
    context->coroutine = CAT_COROUTINE_G(current);
    ret = cat_time_wait(-1);
    done = context->coroutine == NULL;
    context->coroutine = NULL;
    if (unlikely(!ret)) {
        cat_update_last_error_with_previous("File-System stat wait failed");
        (void) uv_cancel(&context->req);
        {return -1; }
    }
    if (unlikely(!done)) {
        cat_update_last_error(CAT_ECANCELED, "File-System stat has been canceled");
        (void) uv_cancel(&context->req);
        {return -1; }
    }
    if (unlikely(context->fs.result < 0)) {
        cat_update_last_error_with_reason((cat_errno_t) context->fs.result, "File-System dysy failed");
        {return -1; }
    }
    memcpy(buf, &context->fs.statbuf, sizeof(uv_stat_t));
    return 0;
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

// cat_work wrapped fs functions
typedef struct cat_fs_ret_s{
    int error; // original errno / GetLastError()
    const char * msg; // cat error message, will be cat_free'd
    union {
        signed long long int num;
        void * ptr;
    }ret;
} cat_fs_ret_t;

#define CAT_FS_WORK_STRUCT1(name, ta) \
    struct cat_fs_ ##name ##_s { \
        cat_fs_ret_t ret; \
        ta a; \
    };
#define CAT_FS_WORK_STRUCT3(name, ta, tb, tc) \
    struct cat_fs_ ##name ##_s { \
        cat_fs_ret_t ret; \
        ta a; \
        tb b; \
        tc c; \
    };
#define CAT_FS_WORK_STRUCT_INIT(name, valname, ...) struct cat_fs_##name##_s valname = {{0}, __VA_ARGS__}
#define CAT_FS_WORK_CB(name) static void _cat_fs_##name##_cb(struct cat_fs_##name##_s* data)
#define CAT_FS_WORK_CB_CALL(name) ((void(*)(void*))_cat_fs_##name##_cb)
#define _CAT_FS_WORK_CB_MKERR(error, msg, fmt, errmsg, freemsg) if(0 != error){\
    const char* _msg = NULL;\
    cat_update_last_error(error, fmt, (NULL != msg) ? msg : (_msg=(errmsg))); \
    {freemsg} \
    if(msg){cat_free((void*)msg);} \
}
#ifdef CAT_OS_WIN
// so nasty, but i have no clear way to do this
cat_errno_t cat_translate_unix_error(int error);
#define CAT_FS_WORK_CB_MKERR(fmt)  _CAT_FS_WORK_CB_MKERR(cat_translate_unix_error(data.ret.error), data.ret.msg, fmt, strerror(data.ret.error), {})
#else
#define CAT_FS_WORK_CB_MKERR(fmt)  _CAT_FS_WORK_CB_MKERR(cat_translate_sys_error(data.ret.error), data.ret.msg, fmt, strerror(data.ret.error), {})
#endif

CAT_FS_WORK_STRUCT3(read, int, void*, size_t)
CAT_FS_WORK_CB(read){
    data->ret.ret.num = read(data->a, data->b,
#ifdef CAT_OS_WIN
    (unsigned int)
#endif
    data->c);
    if(0 > data->ret.ret.num){
        data->ret.error = errno;
        return;
    }
}
CAT_API ssize_t cat_fs_read(cat_file_t fd, void* buf, size_t size){
    CAT_FS_WORK_STRUCT_INIT(read, data, fd, buf, size);
    if(cat_work(CAT_WORK_KIND_FAST_IO, CAT_FS_WORK_CB_CALL(read), &data, CAT_TIMEOUT_FOREVER)){
        CAT_FS_WORK_CB_MKERR("Read failed: %s");
        return data.ret.ret.num;
    }
    return -1;
}

CAT_FS_WORK_STRUCT3(write, int, const void*, size_t)
CAT_FS_WORK_CB(write){
    data->ret.ret.num = (ssize_t)write(data->a, data->b,
#ifdef CAT_OS_WIN
    (unsigned int)
#endif
    data->c);
    if (0 > data->ret.ret.num) {
        data->ret.error = errno;
        return;
    }
}
CAT_API ssize_t cat_fs_write(cat_file_t fd, const void* buf, size_t length){
    CAT_FS_WORK_STRUCT_INIT(write, data, fd, buf, length);
    if (cat_work(CAT_WORK_KIND_FAST_IO, CAT_FS_WORK_CB_CALL(write), &data, CAT_TIMEOUT_FOREVER)) {
        CAT_FS_WORK_CB_MKERR("Write failed: %s");
        return (ssize_t)data.ret.ret.num;
    }
    return -1;
}

CAT_FS_WORK_STRUCT3(lseek, int, off_t, int)
CAT_FS_WORK_CB(lseek){
    data->ret.ret.num = (ssize_t)lseek(data->a, data->b, data->c);
    if (0 > data->ret.ret.num) {
        data->ret.error = errno;
        return;
    }
}
CAT_API off_t cat_fs_lseek(cat_file_t fd, off_t offset, int whence){
    CAT_FS_WORK_STRUCT_INIT(lseek, data, fd, offset, whence);
    if (cat_work(CAT_WORK_KIND_FAST_IO, CAT_FS_WORK_CB_CALL(lseek), &data, CAT_TIMEOUT_FOREVER)) {
        CAT_FS_WORK_CB_MKERR("Calling lseek failed: %s");
        return (off_t)data.ret.ret.num;
    }
    return -1;
}

#ifndef CAT_OS_WIN
CAT_FS_WORK_STRUCT1(readdir, uv_dir_t*)
CAT_FS_WORK_CB(readdir) {
    struct dirent * pdirent = readdir(data->a->dir);
    if(NULL == pdirent){
        data->ret.error = errno;
        return;
    }
    cat_dirent_t * pret = malloc(sizeof(*pret));
    if(NULL == pret){
        data->ret.error = errno;
        return;
    }
    pret->name = strdup(pdirent->d_name);
    if(NULL == pret->name){
        free(pret);
        data->ret.error = errno;
        return;
    }

    // from uv
#ifdef HAVE_DIRENT_TYPES
    int type = 0;
    switch (pdirent->d_type) {
    case UV__DT_DIR:
        type = UV_DIRENT_DIR;
        break;
    case UV__DT_FILE:
        type = UV_DIRENT_FILE;
        break;
    case UV__DT_LINK:
        type = UV_DIRENT_LINK;
        break;
    case UV__DT_FIFO:
        type = UV_DIRENT_FIFO;
        break;
    case UV__DT_SOCKET:
        type = UV_DIRENT_SOCKET;
        break;
    case UV__DT_CHAR:
        type = UV_DIRENT_CHAR;
        break;
    case UV__DT_BLOCK:
        type = UV_DIRENT_BLOCK;
        break;
    default:
        type = UV_DIRENT_UNKNOWN;
    }
    pret->type = type;
#else
    pret->type = UV_DIRENT_UNKNOWN;
#endif
    data->ret.ret.ptr = pret;
}
/*
* cat_fs_readdir: like readdir(3), but return cat_dirent_t
* Note: you should do both free(retval->name) and free(retval)
*/
CAT_API cat_dirent_t* cat_fs_readdir(cat_dir_t* dir){
    CAT_FS_WORK_STRUCT_INIT(readdir, data, dir);
    if (cat_work(CAT_WORK_KIND_FAST_IO, CAT_FS_WORK_CB_CALL(readdir), &data, CAT_TIMEOUT_FOREVER)) {
        CAT_FS_WORK_CB_MKERR("Readdir failed: %s");
        return data.ret.ret.ptr;
    }
    return NULL;
}

struct cat_fs_rewinddir_s {
    DIR* a;
};
CAT_FS_WORK_CB(rewinddir){
    rewinddir(data->a);
}
CAT_API void cat_fs_rewinddir(cat_dir_t * dir){
    uv_dir_t * uv_dir = (uv_dir_t*)dir;
    struct cat_fs_rewinddir_s data = {uv_dir->dir};
    cat_work(CAT_WORK_KIND_FAST_IO, CAT_FS_WORK_CB_CALL(rewinddir), &data, CAT_TIMEOUT_FOREVER);
}
#else
// use NtQueryDirectoryFile to mock readdir,rewinddir behavior.
typedef struct cat_dir_int_s {
    HANDLE dir;
    cat_bool_t rewind;
} cat_dir_int_t;

static LPCWSTR cat_fs_mbs2wcs(const char* mbs){
    DWORD bufsiz = MultiByteToWideChar(
        CP_UTF8, // code page
        0, // flags
        mbs, // src
        -1, // srclen: nul-terminated
        NULL, // dest: no output
        0 // destlen: nope
    );
    LPWSTR wcs = HeapAlloc(GetProcessHeap(), 0, bufsiz * sizeof(WCHAR));
    if(NULL == wcs){
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
    return (LPCWSTR)wcs;
}

static const char* cat_fs_wcs2mbs(LPCWSTR wcs){
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
    char* mbs = HeapAlloc(GetProcessHeap(), 0, bufsiz * sizeof(char));
    if(NULL == mbs){
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
    return (const char *)mbs;
}

static const char* cat_fs_win_strerror(int error) {
    LPVOID temp_msg;
    DWORD len_msg = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, // flags
        NULL, // source
        error, // messageid: last error
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // language id
        (LPWSTR)&temp_msg, // buffer
        0, // bufsiz: 0
        NULL // va_args
    );
    const char* msg = cat_fs_wcs2mbs((LPWSTR)temp_msg);
    LocalFree(temp_msg);
    return msg;
}

static HANDLE hntdll = NULL;
static void cat_fs_proventdll(){
    if (NULL != hntdll){
        return;
    }
    hntdll = GetModuleHandleW(L"ntdll.dll");
    if(NULL == hntdll){
        hntdll= LoadLibraryExW(L"ntdll.dll", NULL, 0);
    }
}

static ULONG(*pRtlNtStatusToDosError) (NTSTATUS) = NULL;
static const char * cat_fs_proventRtlNtStatusToDosError() {
    if (NULL == pRtlNtStatusToDosError) {
        if (NULL == hntdll) {
            cat_fs_proventdll();
            if (NULL == hntdll) {
                return "Cannot open ntdll.dll";
            }
        }
        pRtlNtStatusToDosError = (LPVOID)GetProcAddress(hntdll, "RtlNtStatusToDosError");
        if (NULL == pRtlNtStatusToDosError) {
            return "Cannot resolve RtlNtStatusToDosError";
        }
    }
    return NULL;
}
/*
static const char * cat_fs_nt_strerror(NTSTATUS status){
    //printf("NTSTATUS 0x%08x\n", status);
    const char* _msg;
    if (NULL != (_msg = cat_fs_proventRtlNtStatusToDosError())) {
        const char* msg = cat_sprintf("%s on processing NTSTATUS %08x", msg, status);
        cat_free(_msg);
        return _msg;
    }
    //printf("HRESULT 0x%08x\n", pRtlNtStatusToDosError(status));
    return cat_fs_win_strerror(pRtlNtStatusToDosError(status));
}
*/

#ifdef CAT_OS_WIN
#define CAT_FS_WORK_CB_MKERR_WIN(fmt)  _CAT_FS_WORK_CB_MKERR(data.ret.error, data.ret.msg, fmt, cat_fs_win_strerror(data.ret.error), {if(NULL!=_msg)HeapFree(GetProcessHeap(), 0, (LPVOID)_msg);})
#endif

CAT_FS_WORK_STRUCT1(opendir, const char*)
CAT_FS_WORK_CB(opendir){
    assert(NULL != data->a);
    LPCWSTR pathw = cat_fs_mbs2wcs(data->a);

    HANDLE dir_handle = CreateFileW(
        pathw, // filename
        FILE_LIST_DIRECTORY | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );
    HeapFree(GetProcessHeap(), 0, (LPVOID)pathw);

    if (INVALID_HANDLE_VALUE == dir_handle){
        data->ret.error = GetLastError();
        return;
    }
    cat_dir_int_t *pdir = malloc(sizeof(cat_dir_int_t));
    if(NULL == pdir){
        data->ret.error = ERROR_NOT_ENOUGH_MEMORY;
        return;
    }
    pdir->dir = dir_handle;
    pdir->rewind = cat_true;
    data->ret.ret.ptr = pdir;
}
CAT_API cat_dir_t *cat_fs_opendir(const char* path){
    CAT_FS_WORK_STRUCT_INIT(opendir, data, path);
    if (cat_work(CAT_WORK_KIND_FAST_IO, CAT_FS_WORK_CB_CALL(opendir), &data, CAT_TIMEOUT_FOREVER)) {
        CAT_FS_WORK_CB_MKERR_WIN("Opendir failed: %s");
        return data.ret.ret.ptr;
    }
    return NULL;
}

CAT_FS_WORK_STRUCT1(closedir, cat_dir_int_t*)
CAT_FS_WORK_CB(closedir){
    BOOL ret = CloseHandle(data->a->dir);
    if(FALSE == ret){
        data->ret.ret.num = -1;
        data->ret.error = GetLastError();
        return ;
    }
    data->ret.ret.num = 0;
    return;
}
CAT_API int cat_fs_closedir(cat_dir_t* dir){
    CAT_FS_WORK_STRUCT_INIT(closedir, data, dir);
    if (cat_work(CAT_WORK_KIND_FAST_IO, CAT_FS_WORK_CB_CALL(closedir), &data, CAT_TIMEOUT_FOREVER)) {
        CAT_FS_WORK_CB_MKERR_WIN("Closedir failed: %s");
        return (int)data.ret.ret.num;
    }
    return -1;
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

CAT_FS_WORK_STRUCT1(readdir, cat_dir_int_t*)
CAT_FS_WORK_CB(readdir){
    static __kernel_entry NTSTATUS (*pNtQueryDirectoryFile)(HANDLE,
        HANDLE, PVOID, PVOID, PVOID, PVOID, ULONG, FILE_INFORMATION_CLASS,
        BOOLEAN, PVOID, BOOLEAN) = NULL;
    if(NULL == pNtQueryDirectoryFile){
        if(NULL == hntdll){
            cat_fs_proventdll();
            if(NULL == hntdll){
                data->ret.msg = "Cannot open ntdll.dll on finding NtQueryDirectoryFile in readdir";
                data->ret.error = GetLastError();
                return;
            }
        }
        pNtQueryDirectoryFile = (LPVOID)GetProcAddress(hntdll, "NtQueryDirectoryFile");
        if(NULL == pNtQueryDirectoryFile){
            data->ret.msg = "Cannot find NtQueryDirectoryFile in readdir";
            data->ret.error = GetLastError();
            return;
        }
    }

    IO_STATUS_BLOCK iosb = {0};
    NTSTATUS status;
#if _MSC_VER
    __declspec(align(8))
#else
    __attribute__ ((aligned (8)))
#endif
        char buffer[8192];
    status = pNtQueryDirectoryFile(
        data->a->dir, // file handle
        NULL, // whatever
        NULL, // whatever
        NULL, // whatever
        &iosb,
        &buffer,
        sizeof(buffer),
        FileDirectoryInformation,
        TRUE, // if we get only 1 entry
        NULL, // wildcard filename
        data->a->rewind // from first
    );
    
    if(!NT_SUCCESS(status)){
        const char* _msg;
        if (NULL != (_msg = cat_fs_proventRtlNtStatusToDosError())) {
            data->ret.msg = cat_sprintf("%s on processing NTSTATUS %08x", _msg, status);
            cat_free((void*)_msg);
            data->ret.error = CAT_UNCODED;
            return;
        }
        else {
            data->ret.error = pRtlNtStatusToDosError(status);
            return;
        }
    }

    if (data->a->rewind) {
        data->a->rewind = cat_false;
    }

    cat_dirent_t* pdirent = malloc(sizeof(cat_dirent_t));
    if (NULL == pdirent) {
        data->ret.error = ERROR_NOT_ENOUGH_MEMORY;
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
    pdirent->name = strdup(fn_buf);

    // from uv
    if (pfdi->FileAttributes & FILE_ATTRIBUTE_DEVICE){
        pdirent->type = UV_DIRENT_CHAR;
    }else if (pfdi->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT){
        pdirent->type = UV_DIRENT_LINK;
    }else if (pfdi->FileAttributes & FILE_ATTRIBUTE_DIRECTORY){
        pdirent->type = UV_DIRENT_DIR;
    }else{
        pdirent->type = UV_DIRENT_FILE;
    }
    data->ret.ret.ptr = pdirent;
}
CAT_API uv_dirent_t* cat_fs_readdir(cat_dir_t* dir){
    CAT_FS_WORK_STRUCT_INIT(readdir, data, dir);
    if (cat_work(CAT_WORK_KIND_FAST_IO, CAT_FS_WORK_CB_CALL(readdir), &data, CAT_TIMEOUT_FOREVER)) {
        CAT_FS_WORK_CB_MKERR_WIN("Readdir failed: %s");
        return data.ret.ret.ptr;
    }
    return NULL;
}

CAT_API void cat_fs_rewinddir(cat_dir_t * dir){
    ((cat_dir_int_t*)dir)->rewind = cat_true;
}
#endif
