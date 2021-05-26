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

#define CAT_ENV_BUFFER_SIZE 512

CAT_API char *cat_env_get(const char *name); CAT_FREE
CAT_API char *cat_env_get_ex(const char *name, char *buffer, size_t *size); CAT_MAY_FREE
CAT_API cat_bool_t cat_env_set(const char *name, const char *value);
CAT_API cat_bool_t cat_env_unset(const char *name);
CAT_API cat_bool_t cat_env_exists(const char *name);

typedef int (cat_env_comparer_t)(const char *s1, const char *s2);
CAT_API cat_bool_t cat_env_compare(const char *name, const char *value, cat_env_comparer_t comparer, cat_bool_t default_value);
CAT_API cat_bool_t cat_env_compares(const char *name, const char **values, size_t count, cat_env_comparer_t comparer, cat_bool_t default_value);

CAT_API cat_bool_t cat_env_is(const char *name, const char *value, cat_bool_t default_value);
CAT_API cat_bool_t cat_env_is_true(const char *name, cat_bool_t default_value);
