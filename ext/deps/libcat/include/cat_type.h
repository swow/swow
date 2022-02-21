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

enum cat_bool_e {
    cat_false = 0,
    cat_true = 1
};

#ifdef CAT_IDE_HELPER
#ifdef __cplusplus
typedef bool cat_bool_t;
#define cat_true true
#define cat_false false
#else
typedef enum cat_bool_e cat_bool_t;
#endif
#else
#define cat_bool_t uint8_t
#endif

typedef void cat_data_t;
typedef void (*cat_data_callback_t)(cat_data_t *data);
typedef void (*cat_data_dtor_t)(cat_data_t *data);

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t) ~0)
#endif

#if defined(__x86_64__) || defined(__LP64__) || defined(_LP64) || defined(_WIN64)
#define CAT_L64 1
#endif
#ifndef PRId64
#define PRId64 "lld"
#endif
#ifndef PRIu64
#define PRIu64 "llu"
#endif
#ifndef PRIx64
#define PRIx64 "llx"
#endif

/* time */

typedef uint64_t cat_nsec_t;
#define CAT_NSEC_FMT "%" PRIu64
#define CAT_NSEC_FMT_SPEC PRIu64

typedef uint64_t cat_usec_t;
#define CAT_USEC_FMT "%" PRIu64
#define CAT_USEC_FMT_SPEC PRIu64

typedef uint64_t cat_msec_t;
#define CAT_MSEC_FMT "%" PRIu64
#define CAT_MSEC_FMT_SPEC PRIu64

typedef int64_t cat_timeout_t;
#define CAT_TIMEOUT_FMT "%" PRId64
#define CAT_TIMEOUT_FMT_SPEC PRId64
#define CAT_TIMEOUT_FOREVER -1
#define CAT_TIMEOUT_INVALID INT64_MIN
