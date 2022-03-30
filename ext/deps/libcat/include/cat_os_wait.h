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

#ifndef CAT_OS_WAIT_H
#define CAT_OS_WAIT_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#ifdef CAT_OS_UNIX_LIKE

#include <sys/resource.h>

# define CAT_OS_WAIT 1

/* we believe it's always available on nowadays machines,
 * we can consider checking it only when someone met this problem :) */
# define CAT_OS_WAIT_HAVE_RUSAGE 1

CAT_API cat_bool_t cat_os_wait_module_init(void);
CAT_API cat_bool_t cat_os_wait_runtime_init(void);
CAT_API cat_bool_t cat_os_wait_runtime_shutdown(void);

CAT_API cat_pid_t cat_os_wait(int *status);
CAT_API cat_pid_t cat_os_wait_ex(int *status, cat_msec_t timeout);
CAT_API cat_pid_t cat_os_waitpid(cat_pid_t pid, int *status, int options);
CAT_API cat_pid_t cat_os_waitpid_ex(cat_pid_t pid, int *status, int options, cat_msec_t timeout);

#ifdef CAT_OS_WAIT_HAVE_RUSAGE
CAT_API cat_pid_t cat_os_wait3(int *status, int options, struct rusage *rusage);
CAT_API cat_pid_t cat_os_wait3_ex(int *status, int options, struct rusage *rusage, cat_msec_t timeout);
CAT_API cat_pid_t cat_os_wait4(cat_pid_t pid, int *status, int options, struct rusage *rusage);
CAT_API cat_pid_t cat_os_wait4_ex(cat_pid_t pid, int *status, int options, struct rusage *rusage, cat_msec_t timeout);
#endif /* CAT_OS_WAIT_HAVE_RUSAGE */

#endif

#ifdef __cplusplus
}
#endif
#endif /* CAT_OS_WAIT_H */
