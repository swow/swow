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

#ifndef CAT_TIME_H
#define CAT_TIME_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

typedef int64_t cat_nsec_t;
#define CAT_NSEC_FMT "%" PRIu64

typedef int64_t cat_usec_t;
#define CAT_USEC_FMT "%" PRIu64

typedef int64_t cat_msec_t;
#define CAT_MSEC_FMT "%" PRIu64

typedef cat_msec_t cat_timeout_t;
#define CAT_TIMEOUT_FMT     CAT_MSEC_FMT
#define CAT_TIMEOUT_FOREVER -1
#define CAT_TIMEOUT_INVALID INT64_MIN

CAT_API cat_nsec_t cat_time_nsec(void);
CAT_API cat_msec_t cat_time_msec(void);

CAT_API char *cat_time_format_msec(cat_msec_t msec);

/* cat_false: yield failed or sleep failed or timeout, cat_true: cancelled */
CAT_API cat_bool_t cat_time_wait(cat_timeout_t timeout);
CAT_API unsigned int cat_time_sleep(unsigned int seconds);
/* -1: failed, 0: success, >0: left time (cancelled) */
CAT_API cat_msec_t cat_time_msleep(cat_msec_t msec);
CAT_API int cat_time_usleep(uint64_t microseconds);
CAT_API int cat_time_nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

/* timeval to timeout */
CAT_API cat_timeout_t cat_time_tv2to(const struct timeval *tv);

#ifdef __cplusplus
}
#endif
#endif /* CAT_TIME_H */
