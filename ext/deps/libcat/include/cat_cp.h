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

#ifdef CAT_OS_WIN

#include <io.h>

#define cat_strcasecmp  _stricmp
#define cat_strncasecmp _strnicmp

typedef long cat_timeval_sec_t;

struct cat_timespec {
    time_t   tv_sec;   /* seconds */
    long     tv_nsec;  /* nanoseconds */
};

CAT_API unsigned int cat_sys_sleep(unsigned int seconds);
CAT_API int cat_sys_usleep(unsigned int useconds);
CAT_API int cat_sys_nanosleep(const struct cat_timespec *req, struct cat_timespec *rem);

#else

#define cat_strcasecmp  strcasecmp
#define cat_strncasecmp strncasecmp

typedef time_t cat_timeval_sec_t;

#define cat_timespec timespec

#define cat_sys_sleep     sleep
#define cat_sys_usleep    usleep
#define cat_sys_nanosleep nanosleep

#endif // CAT_OS_WIN

/* process/group/user */

typedef uv_pid_t cat_pid_t;
typedef uv_gid_t cat_gid_t;
typedef uv_uid_t cat_uid_t;

CAT_API cat_pid_t cat_getpid(void);
CAT_API cat_pid_t cat_getppid(void);

/* vector */

#ifndef CAT_OS_WIN
typedef size_t cat_io_vector_length_t;
/* Note: May be cast to struct iovec. See writev(2). */
typedef struct cat_io_vector_s {
    char *base;
    size_t length;
} cat_io_vector_t;
#else
typedef ULONG cat_io_vector_length_t;
/* Note: May be cast to WSABUF[]
 * see http://msdn.microsoft.com/en-us/library/ms741542(v=vs.85).aspx */
typedef struct cat_io_vector_s {
    ULONG length;
    char* base;
} cat_io_vector_t;
#endif

CAT_API size_t cat_io_vector_length(const cat_io_vector_t *vector, unsigned int vector_count);
