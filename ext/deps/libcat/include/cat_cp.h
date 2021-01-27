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
#include <malloc.h>
#include <direct.h>
#include <sys/types.h>
#include <process.h>

typedef int uid_t;
typedef int gid_t;
typedef char* caddr_t;
typedef int pid_t;

#ifndef off_t
#define off_t _off_t
#endif

#ifndef getpid
#define getpid _getpid
#endif

#ifndef getcwd
#define getcwd(a, b) _getcwd(a, b)
#endif

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#if (_MSC_VER >= 1310)
#ifndef strdup
#define strdup _strdup
#endif
#endif

CAT_API char *strndup(const char *string, size_t length);

CAT_API int setenv(const char *name, const char *value, int overwrite);

struct timezone
{
    int tz_minuteswest;
    int tz_dsttime;
};

struct itimerval
{
    struct timeval it_interval; /* next value */
    struct timeval it_value; /* current value */
};

#if !defined(timespec) && _MSC_VER < 1900
struct timespec
{
    time_t   tv_sec;   /* seconds */
    long     tv_nsec;  /* nanoseconds */
};
#endif

#define ITIMER_REAL    0        /*generates sigalrm */
#define ITIMER_VIRTUAL 1        /*generates sigvtalrm */
#define ITIMER_VIRT    1        /*generates sigvtalrm */
#define ITIMER_PROF    2        /*generates sigprof */

typedef long suseconds_t;

CAT_API int gettimeofday(struct timeval *time_info, struct timezone *timezone_info);

CAT_API unsigned int sleep(unsigned int seconds);
CAT_API int usleep(unsigned int useconds);
CAT_API int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
#endif

#ifndef CAT_OS_WIN
/* Note: May be cast to struct iovec. See writev(2). */
typedef struct
{
    char *base;
    size_t length;
} cat_io_vector_t;
#else
/* Note: May be cast to WSABUF[]
 * see http://msdn.microsoft.com/en-us/library/ms741542(v=vs.85).aspx */
typedef struct
{
    ULONG length;
    char* base;
} cat_io_vector_t;
#endif

CAT_API size_t cat_io_vector_length(const cat_io_vector_t *vector, unsigned int vector_count);
