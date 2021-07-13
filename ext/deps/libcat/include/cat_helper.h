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

/* Some content in the header file depends on each other,
 * for example, we can not declare os_is_windows() in os.h,
 * because always_inline macro has not been defined yet at that time,
 * and always_inline macro rely on OS_WIN macro,
 * so we must declare os_is_windows() here (delay). */

static cat_always_inline cat_bool_t cat_os_is_windows(void)
{
#ifndef CAT_OS_WIN
    return cat_false;
#else
    return cat_true;
#endif
}
