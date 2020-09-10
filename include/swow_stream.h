/*
  +--------------------------------------------------------------------------+
  | Swow                                                                     |
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

#ifndef SWOW_STREAM_H
#define SWOW_STREAM_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#if defined(PHP_WIN32) || defined(__riscos__)
#undef AF_UNIX
#endif

extern SWOW_API const php_stream_ops swow_stream_generic_socket_ops;
extern SWOW_API const php_stream_ops swow_stream_tcp_socket_ops;
extern SWOW_API const php_stream_ops swow_stream_udp_socket_ops;
extern SWOW_API const php_stream_ops swow_stream_pipe_socket_ops;
#ifdef AF_UNIX
extern SWOW_API const php_stream_ops swow_stream_unix_socket_ops;
extern SWOW_API const php_stream_ops swow_stream_udg_socket_ops;
#endif

int swow_stream_module_init(INIT_FUNC_ARGS);
int swow_stream_runtime_init(INIT_FUNC_ARGS);
int swow_stream_runtime_shutdown(INIT_FUNC_ARGS);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_STREAM_H */
