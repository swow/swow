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

#ifndef CAT_FUTEX_H_
#define CAT_FUTEX_H_

#include "cat.h"
#include "cat_atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A wait on a value in memory, with no object around it. The caller owns the
 * memory, so two threads share the wait when they share the address. Two
 * processes share it when they map the same page at any address.
 *
 * The wait blocks the calling thread in the kernel. It does not yield to the
 * scheduler, so call it only on a thread that runs no other coroutine. A
 * coroutine on a running event loop must not call it.
 */

typedef cat_atomic_int32_t cat_futex_t;

/*
 * Wait until the value changes from expected, or the timeout runs out.
 *
 * Store the new value before the wake. A waiter that enters after the wake sees
 * the new value and returns CAT_EAGAIN, so the change still reaches it.
 *
 * cat_true: a wake arrived, or the value changed at the same time.
 * cat_false: CAT_EAGAIN when the value differs at the call, CAT_ETIMEDOUT when
 * the timeout runs out, CAT_ENOTSUP on a platform without a wait on an address.
 *
 * A timeout of 0 checks the value and returns without a wait. A timeout of
 * CAT_TIMEOUT_FOREVER waits without one.
 */
CAT_API cat_bool_t cat_futex_wait(cat_futex_t *futex, int32_t expected, cat_timeout_t timeout);

/*
 * Wake up to count waiters on the value. A count below zero wakes all of them,
 * and a count of zero wakes none of them.
 *
 * cat_true: the call succeeded. It does not tell how many waiters it woke,
 * because not every platform reports that count.
 * cat_false: CAT_ENOTSUP on a platform without a wake on an address.
 */
CAT_API cat_bool_t cat_futex_wake(cat_futex_t *futex, int32_t count);

#ifdef __cplusplus
}
#endif

#endif /* CAT_FUTEX_H_ */
