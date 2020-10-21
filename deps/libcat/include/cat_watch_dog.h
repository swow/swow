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

#ifndef CAT_WATCH_DOG_H
#define CAT_WATCH_DOG_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#include "cat_coroutine.h"
#include "cat_time.h"

#define CAT_WATCH_DOG_DEFAULT_QUANTUM    (5 * 1000 * 1000)
#define CAT_WATCH_DOG_DEFAULT_THRESHOLD  (10 * 1000 * 1000)
#define CAT_WATCH_DOG_THRESHOLD_DISABLED -1

#ifndef CAT_THREAD_SAFE
#define CAT_WATCH_DOG_ROLE_NAME "process"
#else
#define CAT_WATCH_DOG_ROLE_NAME "thread"
#endif

#define CAT_ALERT_COUNT_FMT "%" PRIu64
typedef uint64_t cat_alert_count_t;

typedef struct cat_watch_dog_s cat_watch_dog_t;

typedef void (*cat_watch_dog_alerter_t)(cat_watch_dog_t *watch_dog);

CAT_GLOBALS_STRUCT_BEGIN(cat_watch_dog)
    cat_watch_dog_t *watch_dog;
CAT_GLOBALS_STRUCT_END(cat_watch_dog)

extern CAT_API CAT_GLOBALS_DECLARE(cat_watch_dog)

#define CAT_WATCH_DOG_G(x) CAT_GLOBALS_GET(cat_watch_dog, x)

struct cat_watch_dog_s
{
    /* public (options, readonly) */
    cat_nsec_t quantum;
    cat_nsec_t threshold;
    cat_watch_dog_alerter_t alerter;
    /* private */
    cat_alert_count_t alert_count;
    cat_bool_t allocated;
    cat_bool_t stop;
    uv_pid_t pid; /* TODO: cat_pid_t */
    CAT_GLOBALS_TYPE(cat_coroutine) *globals;
    cat_coroutine_round_t last_round;
    uv_thread_t thread;
    uv_sem_t *sem;
    uv_cond_t cond;
    uv_mutex_t mutex;
};

CAT_API cat_bool_t cat_watch_dog_module_init(void);
CAT_API cat_bool_t cat_watch_dog_runtime_init(void);
CAT_API cat_bool_t cat_watch_dog_runtime_shutdown(void);

CAT_API cat_bool_t cat_watch_dog_run(cat_watch_dog_t *watch_dog, cat_nsec_t quantum, cat_nsec_t threshold, cat_watch_dog_alerter_t alerter);
CAT_API cat_bool_t cat_watch_dog_stop(void);

CAT_API void cat_watch_dog_alert_standard(cat_watch_dog_t *watch_dog);

CAT_API cat_bool_t cat_watch_dog_is_running(void);
CAT_API cat_nsec_t cat_watch_dog_get_quantum(void);
CAT_API cat_nsec_t cat_watch_dog_get_threshold(void);

#ifdef __cplusplus
}
#endif
#endif /* CAT_WATCH_DOG_H */
