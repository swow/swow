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

#define CAT_GLOBALS_STRUCT(name)                        name##_globals_s
#define CAT_GLOBALS_TYPE(name)                          name##_globals_t
#define CAT_GLOBALS(name)                               name##_globals

#define CAT_GLOBALS_STRUCT_BEGIN(name)                  typedef struct CAT_GLOBALS_STRUCT(name) {
#define CAT_GLOBALS_STRUCT_END(name)                    } CAT_GLOBALS_TYPE(name);

#define CAT_GLOBALS_CTOR(name)                          name##_globals##_ctor
#define CAT_GLOBALS_DTOR(name)                          name##_globals##_dtor
#define CAT_GLOBALS_CTOR_DECLARE(name, param)           void CAT_GLOBALS_CTOR(name)(CAT_GLOBALS_TYPE(name) *param)
#define CAT_GLOBALS_DTOR_DECLARE(name, param)           void CAT_GLOBALS_DTOR(name)(CAT_GLOBALS_TYPE(name) *param)
#define CAT_GLOBALS_CTOR_DECLARE_SZ(name)               static CAT_GLOBALS_CTOR_DECLARE(name, g) { memset(g, 0, sizeof(*g)); }
#define CAT_GLOBALS_DTOR_DECLARE_SZ(name)               static CAT_GLOBALS_DTOR_DECLARE(name, g) { memset(g, 0, sizeof(*g)); }

#ifndef CAT_THREAD_SAFE

#ifdef CAT_USE_THREAD_LOCAL
#define CAT_THREAD_SAFE 1
#if defined(CAT_OS_WIN) && defined(_MSC_VER)
#define CAT_THREAD_LOCAL __declspec(thread)
#else
#define CAT_THREAD_LOCAL __thread
#endif
#endif /* CAT_USE_THREAD_LOCAL */

#ifdef CAT_THREAD_LOCAL
#define CAT_GLOBALS_DECLARE(name)                       CAT_THREAD_LOCAL CAT_GLOBALS_TYPE(name) CAT_GLOBALS(name);
#else
#define CAT_GLOBALS_DECLARE(name)                       CAT_GLOBALS_TYPE(name) CAT_GLOBALS(name);
#endif
#define CAT_GLOBALS_GET(name, value)                    CAT_GLOBALS(name).value
#define CAT_GLOBALS_BULK(name)                          &CAT_GLOBALS(name)

#define CAT_GLOBALS_REGISTER(name, ctor, dtor) do { \
    ctor(CAT_GLOBALS_BULK(name)); \
} while (0)

#endif /* CAT_THREAD_SAFE */
