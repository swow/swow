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

#ifndef CAT_API_H
#define CAT_API_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#include "cat_coroutine.h"
#include "cat_channel.h"
#include "cat_sync.h"
#include "cat_event.h"
#include "cat_time.h"
#include "cat_socket.h"
#include "cat_dns.h"
#include "cat_work.h"
#include "cat_buffer.h"
#include "cat_fs.h"
#include "cat_signal.h"
#include "cat_watch_dog.h"
#include "cat_ssl.h"

typedef enum
{
    CAT_RUN_EASY = 0,
} cat_run_mode;

CAT_API cat_bool_t cat_init_all(void);
CAT_API cat_bool_t cat_shutdown_all(void);
CAT_API cat_bool_t cat_run(cat_run_mode run_mode);
CAT_API void cat_stop(void);

#ifdef CAT_DEBUG
CAT_API void cat_enable_debug_mode(void);
#else
#define cat_enable_debug_mode()
#endif

#ifdef __cplusplus
}
#endif
#endif /* CAT_API_H */
