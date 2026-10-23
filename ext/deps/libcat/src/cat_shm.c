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
 * The first page of a mapping holds its header, and the caller data starts at the
 * next 4096-byte boundary. An attach that finds no header therefore refuses the
 * object instead of trusting a size that nobody wrote.
 */
#define CAT_SHM_HEADER_SIZE 4096

#define CAT_SHM_MAGIC 0x6d687363U /* "cshm" */

typedef struct cat_shm_header_s {
    uint32_t magic;
    uint32_t reserved;
    uint64_t size;
} cat_shm_header_t;

struct cat_shm_s {
    void *base;
    void *address;
    size_t size;
    size_t mapping_size;
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

    wide = (wchar_t *) cat_calloc((size_t) length, sizeof(wchar_t));
#if CAT_ALLOC_HANDLE_ERRORS
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
    size_t mapping_size = size + CAT_SHM_HEADER_SIZE;
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
        (DWORD) (((uint64_t) mapping_size) >> 32), (DWORD) (mapping_size & 0xFFFFFFFFU), wide);
    error = GetLastError();
    if (wide != NULL) {
        cat_free(wide);
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
    shm->mapping_size = mapping_size;
    shm->size = size;
    shm->address = (char *) base + CAT_SHM_HEADER_SIZE;

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
    cat_free(wide);

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
    /* the view is rounded up to a page, so the header gives the exact size */
    shm->mapping_size = (size_t) information.RegionSize;
    shm->address = (char *) base + CAT_SHM_HEADER_SIZE;

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

/* shm_open takes a leading slash in the name, and the caller does not. */
static char *cat_shm_posix_path(const char *name)
{
    if (name[0] == '/') {
        return cat_strdup(name);
    }

    return cat_sprintf("/%s", name);
}

static cat_bool_t cat_shm_platform_create(cat_shm_t *shm, const char *name, size_t size)
{
    size_t mapping_size = size + CAT_SHM_HEADER_SIZE;
    char *path = NULL;
    void *base;
    int fd = -1;

    if (name != NULL) {
        path = cat_shm_posix_path(name);
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(path == NULL)) {
            cat_update_last_error_of_syscall("Sprintf for the name of a shared mapping failed");
            return cat_false;
        }
#endif

        fd = shm_open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
        if (unlikely(fd < 0)) {
            cat_update_last_error_with_reason(
                errno == EEXIST ? CAT_EEXIST : cat_translate_sys_error(errno),
                "shm_open(%s) failed", path);
            cat_free(path);
            return cat_false;
        }

        if (unlikely(ftruncate(fd, (off_t) mapping_size) != 0)) {
            cat_update_last_error_with_reason(cat_translate_sys_error(errno),
                "ftruncate(%s) failed", path);
            (void) shm_unlink(path);
            (void) close(fd);
            cat_free(path);
            return cat_false;
        }
    }

    base = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
        MAP_SHARED | (name == NULL ? MAP_ANONYMOUS : 0), fd, 0);
    if (unlikely(base == MAP_FAILED)) {
        cat_update_last_error_with_reason(cat_translate_sys_error(errno), "mmap failed");
        if (path != NULL) {
            (void) shm_unlink(path);
            (void) close(fd);
            cat_free(path);
        }
        return cat_false;
    }

    if (path != NULL) {
        cat_free(path);
    }

    shm->base = base;
    shm->fd = fd;
    shm->mapping_size = mapping_size;
    shm->size = size;
    shm->address = (char *) base + CAT_SHM_HEADER_SIZE;

    return cat_true;
}

static cat_bool_t cat_shm_platform_attach(cat_shm_t *shm, const char *name)
{
    char *path = cat_shm_posix_path(name);
    struct stat information;
    void *base;
    int fd;

#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(path == NULL)) {
        cat_update_last_error_of_syscall("Sprintf for the name of a shared mapping failed");
        return cat_false;
    }
#endif

    fd = shm_open(path, O_RDWR, 0);
    if (unlikely(fd < 0)) {
        cat_update_last_error_with_reason(
            errno == ENOENT ? CAT_ENOENT : cat_translate_sys_error(errno),
            "shm_open(%s) failed", path);
        cat_free(path);
        return cat_false;
    }
    cat_free(path);

    if (unlikely(fstat(fd, &information) != 0)) {
        cat_update_last_error_with_reason(cat_translate_sys_error(errno), "fstat failed");
        (void) close(fd);
        return cat_false;
    }

    if (unlikely((uint64_t) information.st_size < CAT_SHM_HEADER_SIZE)) {
        cat_update_last_error(CAT_EINVAL, "The mapping has no libcat header");
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
    shm->mapping_size = (size_t) information.st_size;
    shm->address = (char *) base + CAT_SHM_HEADER_SIZE;

    return cat_true;
}

static void cat_shm_platform_detach(cat_shm_t *shm)
{
    if (shm->base != NULL) {
        (void) munmap(shm->base, shm->mapping_size);
    }
    if (shm->fd >= 0) {
        (void) close(shm->fd);
    }
}

static cat_bool_t cat_shm_platform_unlink(const char *name)
{
    char *path = cat_shm_posix_path(name);
    int result;

#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(path == NULL)) {
        cat_update_last_error_of_syscall("Sprintf for the name of a shared mapping failed");
        return cat_false;
    }
#endif

    result = shm_unlink(path);
    if (unlikely(result != 0)) {
        cat_update_last_error_with_reason(
            errno == ENOENT ? CAT_ENOENT : cat_translate_sys_error(errno),
            "shm_unlink(%s) failed", path);
    }
    cat_free(path);

    return result == 0 ? cat_true : cat_false;
}

#endif /* CAT_OS_WIN */

static void cat_shm_write_header(cat_shm_t *shm)
{
    cat_shm_header_t *header = (cat_shm_header_t *) shm->base;

    header->magic = CAT_SHM_MAGIC;
    header->reserved = 0;
    header->size = (uint64_t) shm->size;
}

static cat_bool_t cat_shm_check_header(cat_shm_t *shm)
{
    cat_shm_header_t *header = (cat_shm_header_t *) shm->base;

    if (unlikely(shm->mapping_size < CAT_SHM_HEADER_SIZE ||
        header->magic != CAT_SHM_MAGIC ||
        header->size == 0 ||
        header->size > (uint64_t) (shm->mapping_size - CAT_SHM_HEADER_SIZE))) {
        cat_update_last_error(CAT_EINVAL, "The mapping has no libcat header");
        return cat_false;
    }

    shm->size = (size_t) header->size;
    shm->address = (char *) shm->base + CAT_SHM_HEADER_SIZE;

    return cat_true;
}

static cat_shm_t *cat_shm_alloc(void)
{
    cat_shm_t *shm = (cat_shm_t *) cat_calloc(1, sizeof(cat_shm_t));

#if CAT_ALLOC_HANDLE_ERRORS
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
        cat_free(shm->name);
    }
    cat_free(shm);
}

static cat_shm_t *cat_shm_create(const char *name, size_t size)
{
    cat_shm_t *shm;

    if (unlikely(size == 0 || size > SIZE_MAX - CAT_SHM_HEADER_SIZE)) {
        cat_update_last_error(CAT_EINVAL, "The size of a shared mapping is out of range");
        return NULL;
    }

    shm = cat_shm_alloc();
    if (shm == NULL) {
        return NULL;
    }

    if (name != NULL) {
        shm->name = cat_strdup(name);
#if CAT_ALLOC_HANDLE_ERRORS
        if (unlikely(shm->name == NULL)) {
            cat_update_last_error_of_syscall("Strdup for the name of a shared mapping failed");
            cat_free(shm);
            return NULL;
        }
#endif
    }

    if (unlikely(!cat_shm_platform_create(shm, name, size))) {
        cat_shm_free(shm);
        return NULL;
    }

    cat_shm_write_header(shm);

    return shm;
}

CAT_API cat_shm_t *cat_shm_create_anonymous(size_t size)
{
    return cat_shm_create(NULL, size);
}

CAT_API cat_shm_t *cat_shm_create_named(const char *name, size_t size)
{
    if (unlikely(name == NULL || name[0] == '\0')) {
        cat_update_last_error(CAT_EINVAL, "The name of a shared mapping is empty");
        return NULL;
    }

    return cat_shm_create(name, size);
}

CAT_API cat_shm_t *cat_shm_attach_named(const char *name)
{
    cat_shm_t *shm;

    if (unlikely(name == NULL || name[0] == '\0')) {
        cat_update_last_error(CAT_EINVAL, "The name of a shared mapping is empty");
        return NULL;
    }

    shm = cat_shm_alloc();
    if (shm == NULL) {
        return NULL;
    }

    shm->name = cat_strdup(name);
#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(shm->name == NULL)) {
        cat_update_last_error_of_syscall("Strdup for the name of a shared mapping failed");
        cat_free(shm);
        return NULL;
    }
#endif

    if (unlikely(!cat_shm_platform_attach(shm, name))) {
        cat_shm_free(shm);
        return NULL;
    }

    if (unlikely(!cat_shm_check_header(shm))) {
        cat_shm_platform_detach(shm);
        cat_shm_free(shm);
        return NULL;
    }

    return shm;
}

CAT_API void *cat_shm_get_address(const cat_shm_t *shm)
{
    return shm->address;
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
    if (unlikely(name == NULL || name[0] == '\0')) {
        cat_update_last_error(CAT_EINVAL, "The name of a shared mapping is empty");
        return cat_false;
    }

    return cat_shm_platform_unlink(name);
}
