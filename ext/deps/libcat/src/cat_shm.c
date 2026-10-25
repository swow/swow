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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#include "cat.h"
#include "cat_shm.h"

#if defined(CAT_OS_WIN)

#else /* !CAT_OS_WIN */

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif

#endif

/*
 * The platform bounds the name of a shared object. Windows takes the name as it
 * is, and the POSIX backend adds a leading slash to a name without one.
 */
#ifdef CAT_OS_WIN
/* A named section lives in the kernel object namespace, and the Win32 name is
 * limited to MAX_PATH (260) characters. The CreateFileMappingW page does not
 * repeat that limit, which the CreateMutexW and the CreateEventW pages state
 * for the same namespace */
# define CAT_SHM_NAME_MAX ((size_t) 260) /* MAX_PATH */
#elif defined(CAT_OS_DARWIN)
/* macOS accepts at most 31 characters in the name of a shared memory object,
 * and the leading slash counts toward that limit */
# define CAT_SHM_NAME_MAX ((size_t) 30)
#else
/* a POSIX file name component is bounded by NAME_MAX */
# define CAT_SHM_NAME_MAX ((size_t) 255)
#endif

struct cat_shm_s {
    void *base;
    size_t size;
    char *name;
#ifdef CAT_OS_WIN
    HANDLE handle;
#else
    int fd;
#endif
};

#ifdef CAT_OS_WIN

/* The caller passes a UTF-8 name, and the Win32 section API takes UTF-16. The
 * length from libuv includes the terminator. */
static wchar_t *cat_shm_win32_name(const char *name)
{
    ssize_t length = uv_wtf8_length_as_utf16(name);
    wchar_t *wide;

    if (unlikely(length < 0)) {
        cat_update_last_error(CAT_EINVAL, "The name is not valid UTF-8");
        return NULL;
    }

    wide = (wchar_t *) cat_sys_calloc((size_t) length, sizeof(wchar_t));
#if CAT_SYS_ALLOC_HANDLE_ERRORS
    if (unlikely(wide == NULL)) {
        cat_update_last_error_of_syscall("Calloc for the name of a shared mapping failed");
        return NULL;
    }
#endif

    uv_wtf8_to_utf16(name, (uint16_t *) wide, (size_t) length);

    return wide;
}

static cat_bool_t cat_shm_platform_create(cat_shm_t *shm, const char *name, size_t size)
{
    wchar_t *wide = NULL;
    HANDLE handle;
    void *base;
    DWORD error;

    if (name != NULL) {
        wide = cat_shm_win32_name(name);
        if (wide == NULL) {
            return cat_false;
        }
    }

    handle = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
        (DWORD) (((uint64_t) size) >> 32), (DWORD) (size & 0xFFFFFFFFU), wide);
    error = GetLastError();
    if (wide != NULL) {
        cat_sys_free(wide);
    }

    if (unlikely(handle == NULL)) {
        cat_update_last_error_with_reason(cat_translate_sys_error((int) error),
            "CreateFileMapping failed");
        return cat_false;
    }

    if (name != NULL && error == ERROR_ALREADY_EXISTS) {
        CloseHandle(handle);
        cat_update_last_error(CAT_EEXIST, "A shared mapping already has the name");
        return cat_false;
    }

    base = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (unlikely(base == NULL)) {
        cat_update_last_error_with_reason(cat_translate_sys_error((int) GetLastError()),
            "MapViewOfFile failed");
        CloseHandle(handle);
        return cat_false;
    }

    shm->handle = handle;
    shm->base = base;
    shm->size = size;

    return cat_true;
}

static cat_bool_t cat_shm_platform_attach(cat_shm_t *shm, const char *name)
{
    MEMORY_BASIC_INFORMATION information;
    wchar_t *wide = cat_shm_win32_name(name);
    HANDLE handle;
    void *base;
    DWORD error;

    if (wide == NULL) {
        return cat_false;
    }

    handle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, wide);
    error = GetLastError();
    cat_sys_free(wide);

    if (unlikely(handle == NULL)) {
        cat_update_last_error_with_reason(
            error == ERROR_FILE_NOT_FOUND ? CAT_ENOENT : cat_translate_sys_error((int) error),
            "OpenFileMapping failed");
        return cat_false;
    }

    base = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (unlikely(base == NULL)) {
        cat_update_last_error_with_reason(cat_translate_sys_error((int) GetLastError()),
            "MapViewOfFile failed");
        CloseHandle(handle);
        return cat_false;
    }

    if (unlikely(VirtualQuery(base, &information, sizeof(information)) == 0)) {
        cat_update_last_error_with_reason(cat_translate_sys_error((int) GetLastError()),
            "VirtualQuery failed");
        UnmapViewOfFile(base);
        CloseHandle(handle);
        return cat_false;
    }

    shm->handle = handle;
    shm->base = base;
    /* the view is rounded up to a page, and that is the only size available */
    shm->size = (size_t) information.RegionSize;

    return cat_true;
}

static void cat_shm_platform_detach(cat_shm_t *shm)
{
    if (shm->base != NULL) {
        UnmapViewOfFile(shm->base);
    }
    if (shm->handle != NULL) {
        CloseHandle(shm->handle);
    }
}

static cat_bool_t cat_shm_platform_unlink(const char *name)
{
    (void) name;

    /* a named section ends when the last handle closes, and no call removes the
     * name earlier */
    return cat_true;
}

#else /* !CAT_OS_WIN */

/* shm_open takes a leading slash in the name, and the caller does not. The
 * path lives until the create or the attach returns, and the system allocator
 * keeps it valid when a mapping outlives the request heap */
static char *cat_shm_posix_path(const char *name)
{
    size_t length = strlen(name);
    cat_bool_t has_slash = name[0] == '/';
    char *path = (char *) cat_sys_malloc(length + (has_slash ? 1 : 2));

    if (unlikely(path == NULL)) {
        return NULL;
    }

    if (has_slash) {
        memcpy(path, name, length + 1);
    } else {
        path[0] = '/';
        memcpy(path + 1, name, length + 1);
    }

    return path;
}

static cat_bool_t cat_shm_platform_create(cat_shm_t *shm, const char *name, size_t size)
{
    char *path = NULL;
    void *base;
    int fd = -1;

    if (name != NULL) {
        path = cat_shm_posix_path(name);
#if CAT_SYS_ALLOC_HANDLE_ERRORS
        if (unlikely(path == NULL)) {
            cat_update_last_error_of_syscall("Strdup for the name of a shared mapping failed");
            return cat_false;
        }
#endif

        fd = shm_open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
        if (unlikely(fd < 0)) {
            cat_update_last_error_with_reason(
                errno == EEXIST ? CAT_EEXIST : cat_translate_sys_error(errno),
                "shm_open(%s) failed", path);
            cat_sys_free(path);
            return cat_false;
        }

        if (unlikely(ftruncate(fd, (off_t) size) != 0)) {
            cat_update_last_error_with_reason(cat_translate_sys_error(errno),
                "ftruncate(%s) failed", path);
            (void) shm_unlink(path);
            (void) close(fd);
            cat_sys_free(path);
            return cat_false;
        }
    }

    base = mmap(NULL, size, PROT_READ | PROT_WRITE,
        MAP_SHARED | (name == NULL ? MAP_ANONYMOUS : 0), fd, 0);
    if (unlikely(base == MAP_FAILED)) {
        cat_update_last_error_with_reason(cat_translate_sys_error(errno), "mmap failed");
        if (path != NULL) {
            (void) shm_unlink(path);
            (void) close(fd);
            cat_sys_free(path);
        }
        return cat_false;
    }

    if (path != NULL) {
        cat_sys_free(path);
    }

    shm->base = base;
    shm->fd = fd;
    shm->size = size;

    return cat_true;
}

static cat_bool_t cat_shm_platform_attach(cat_shm_t *shm, const char *name)
{
    char *path = cat_shm_posix_path(name);
    struct stat information;
    void *base;
    int fd;

#if CAT_SYS_ALLOC_HANDLE_ERRORS
    if (unlikely(path == NULL)) {
        cat_update_last_error_of_syscall("Strdup for the name of a shared mapping failed");
        return cat_false;
    }
#endif

    fd = shm_open(path, O_RDWR, 0);
    if (unlikely(fd < 0)) {
        cat_update_last_error_with_reason(
            errno == ENOENT ? CAT_ENOENT : cat_translate_sys_error(errno),
            "shm_open(%s) failed", path);
        cat_sys_free(path);
        return cat_false;
    }
    cat_sys_free(path);

    if (unlikely(fstat(fd, &information) != 0)) {
        cat_update_last_error_with_reason(cat_translate_sys_error(errno), "fstat failed");
        (void) close(fd);
        return cat_false;
    }

    /* the name is visible before the create finished, so an empty object means
     * an early attach */
    if (unlikely(information.st_size == 0)) {
        cat_update_last_error(CAT_EINVAL, "The size of a shared mapping is out of range");
        (void) close(fd);
        return cat_false;
    }

    base = mmap(NULL, (size_t) information.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (unlikely(base == MAP_FAILED)) {
        cat_update_last_error_with_reason(cat_translate_sys_error(errno), "mmap failed");
        (void) close(fd);
        return cat_false;
    }

    shm->base = base;
    shm->fd = fd;
    shm->size = (size_t) information.st_size;

    return cat_true;
}

static void cat_shm_platform_detach(cat_shm_t *shm)
{
    if (shm->base != NULL) {
        (void) munmap(shm->base, shm->size);
    }
    if (shm->fd >= 0) {
        (void) close(shm->fd);
    }
}

static cat_bool_t cat_shm_platform_unlink(const char *name)
{
    char *path = cat_shm_posix_path(name);
    int result;

#if CAT_SYS_ALLOC_HANDLE_ERRORS
    if (unlikely(path == NULL)) {
        cat_update_last_error_of_syscall("Strdup for the name of a shared mapping failed");
        return cat_false;
    }
#endif

    result = shm_unlink(path);
    if (unlikely(result != 0)) {
        cat_update_last_error_with_reason(
            errno == ENOENT ? CAT_ENOENT : cat_translate_sys_error(errno),
            "shm_unlink(%s) failed", path);
    }
    cat_sys_free(path);

    return result == 0 ? cat_true : cat_false;
}

#endif /* CAT_OS_WIN */

static cat_shm_t *cat_shm_alloc(void)
{
    cat_shm_t *shm = (cat_shm_t *) cat_sys_calloc(1, sizeof(cat_shm_t));

#if CAT_SYS_ALLOC_HANDLE_ERRORS
    if (unlikely(shm == NULL)) {
        cat_update_last_error_of_syscall("Calloc for a shared mapping failed");
        return NULL;
    }
#endif

#ifndef CAT_OS_WIN
    shm->fd = -1;
#endif

    return shm;
}

static void cat_shm_free(cat_shm_t *shm)
{
    if (shm->name != NULL) {
        cat_sys_free(shm->name);
    }
    cat_sys_free(shm);
}

static cat_bool_t cat_shm_name_is_valid(const char *name)
{
    size_t length;

    if (unlikely(name == NULL || name[0] == '\0')) {
        cat_update_last_error(CAT_EINVAL, "The name of a shared mapping is empty");
        return cat_false;
    }

#ifdef CAT_OS_WIN
    length = strlen(name);
#else
    /* the POSIX backend adds the leading slash, and the platform counts it */
    length = strlen(name) - (name[0] == '/' ? 1 : 0);
#endif
    if (unlikely(length > CAT_SHM_NAME_MAX)) {
        cat_update_last_error(CAT_EINVAL,
            "The name of a shared mapping is longer than %zu characters",
            CAT_SHM_NAME_MAX);
        return cat_false;
    }

    return cat_true;
}

static cat_shm_t *cat_shm_create(const char *name, size_t size)
{
    cat_shm_t *shm;

    if (unlikely(size == 0)) {
        cat_update_last_error(CAT_EINVAL, "The size of a shared mapping is out of range");
        return NULL;
    }

    shm = cat_shm_alloc();
    if (shm == NULL) {
        return NULL;
    }

    if (name != NULL) {
        shm->name = cat_sys_strdup(name);
#if CAT_SYS_ALLOC_HANDLE_ERRORS
        if (unlikely(shm->name == NULL)) {
            cat_update_last_error_of_syscall("Strdup for the name of a shared mapping failed");
            cat_sys_free(shm);
            return NULL;
        }
#endif
    }

    if (unlikely(!cat_shm_platform_create(shm, name, size))) {
        cat_shm_free(shm);
        return NULL;
    }

    return shm;
}

CAT_API cat_shm_t *cat_shm_create_anonymous(size_t size)
{
    return cat_shm_create(NULL, size);
}

CAT_API cat_shm_t *cat_shm_create_named(const char *name, size_t size)
{
    if (unlikely(!cat_shm_name_is_valid(name))) {
        return NULL;
    }

    return cat_shm_create(name, size);
}

CAT_API cat_shm_t *cat_shm_attach_named(const char *name)
{
    cat_shm_t *shm;

    if (unlikely(!cat_shm_name_is_valid(name))) {
        return NULL;
    }

    shm = cat_shm_alloc();
    if (shm == NULL) {
        return NULL;
    }

    shm->name = cat_sys_strdup(name);
#if CAT_SYS_ALLOC_HANDLE_ERRORS
    if (unlikely(shm->name == NULL)) {
        cat_update_last_error_of_syscall("Strdup for the name of a shared mapping failed");
        cat_sys_free(shm);
        return NULL;
    }
#endif

    if (unlikely(!cat_shm_platform_attach(shm, name))) {
        cat_shm_free(shm);
        return NULL;
    }

    return shm;
}

CAT_API void *cat_shm_get_address(const cat_shm_t *shm)
{
    return shm->base;
}

CAT_API size_t cat_shm_get_size(const cat_shm_t *shm)
{
    return shm->size;
}

CAT_API const char *cat_shm_get_name(const cat_shm_t *shm)
{
    return shm->name;
}

CAT_API void cat_shm_detach(cat_shm_t *shm)
{
    cat_shm_platform_detach(shm);
    cat_shm_free(shm);
}

CAT_API cat_bool_t cat_shm_unlink(const char *name)
{
    if (unlikely(!cat_shm_name_is_valid(name))) {
        return cat_false;
    }

    return cat_shm_platform_unlink(name);
}
