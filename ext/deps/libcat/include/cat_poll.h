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

#ifndef CAT_POLL_H
#define CAT_POLL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#ifdef CAT_OS_UNIX_LIKE
#include <sys/types.h>
#include <poll.h>
#endif

#ifndef CAT_OS_WIN
typedef struct pollfd cat_pollfd_t;
typedef nfds_t cat_nfds_t;
typedef int cat_pollfd_events_t;
#else
typedef WSAPOLLFD cat_pollfd_t;
typedef ULONG cat_nfds_t;
typedef SHORT cat_pollfd_events_t;
#endif

#ifndef POLLNONE
#define POLLNONE 0
#endif
#ifndef POLLIN
# define POLLIN      0x0001    /* There is data to read */
# define POLLPRI     0x0002    /* There is urgent data to read */
# define POLLOUT     0x0004    /* Writing now will not block */
# define POLLERR     0x0008    /* Error condition */
# define POLLHUP     0x0010    /* Hung up */
# define POLLNVAL    0x0020    /* Invalid request: fd not open */
#endif

/* OK: events triggered, NONE: timedout, ERROR: error ocurred */
CAT_API cat_ret_t cat_poll_one(cat_os_socket_t fd, cat_pollfd_events_t events, cat_pollfd_events_t *revents, cat_timeout_t timeout);

/* same with poll(),
 * returns the number of descriptors that are ready for I/O, or -1 if an error occurred.
 * If the time limit expires, poll() returns 0. */
CAT_API int cat_poll(cat_pollfd_t *fds, cat_nfds_t nfds, cat_timeout_t timeout);

/* same with select(),
 * but we can not use it for file IO */
CAT_API int cat_select(int max_fd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

#ifdef __cplusplus
}
#endif
#endif /* CAT_POLL_H */
