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

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__) || defined(__NT__)
/* WIN {{{ */
#define CAT_OS_FAMILY    "Windows"
#define CAT_OS_WIN       1
/* }}} WIN */
#else
/* UNIX {{{ */
#include <unistd.h>
#define CAT_OS_UNIX_LIKE 1
#if defined(BSD) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define CAT_OS_FAMILY    "BSD"
#define CAT_OS_BSD       1
#elif defined(__APPLE__) || defined(__MACH__)
#define CAT_OS_FAMILY    "Darwin"
#define CAT_OS_DARWIN    1
#elif defined(__sun__)
#define CAT_OS_FAMILY    "Solaris"
#define CAT_OS_SOLARIS   1
#elif defined(__linux__)
#define CAT_OS_FAMILY    "Linux"
#define CAT_OS_LINUX     1
#else
#define CAT_OS_FAMILY    "Unknown"
#endif
/* }}} UNIX */
#endif

#define CAT_LF     '\n'
#define CAT_CR     '\r'
#define CAT_CRLF   "\r\n"

#ifdef CAT_OS_WIN
#define CAT_DIR_SEPARATOR '\\'
#define CAT_EOL "\r\n"
#else
#define CAT_DIR_SEPARATOR '/'
#define CAT_EOL "\n"
#endif
