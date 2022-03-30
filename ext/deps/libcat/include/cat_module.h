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

#define CAT_MODULE_BUILTIN_TYPE_MAP(XX) \
    /* kernel (0 ~ 18) */ \
    XX(CORE,      1 << 0) \
    XX(COROUTINE, 1 << 1) \
    XX(CHANNEL,   1 << 2) \
    XX(SYNC,      1 << 3) \
    XX(EVENT,     1 << 4) \
    XX(TIME,      1 << 5) \
    XX(SOCKET,    1 << 6) \
    XX(DNS,       1 << 7) \
    XX(WORK,      1 << 8) \
    XX(BUFFER,    1 << 9) \
    XX(FS,        1 << 10) \
    XX(SIGNAL,    1 << 11) \
    XX(PROCESS,   1 << 12) \
    XX(THREAD,    1 << 13) \
    XX(OS,        1 << 14) \
    XX(WATCH_DOG, 1 << 17) \
    XX(PROTOCOL,  1 << 18) \
    /* optional (19 ~ 22) */ \
    XX(SSL,       1 << 21) \
    XX(EXT,       1 << 22) \
    /* test */ \
    XX(TEST,      1 << 23)

#define CAT_MODULE_USR_TYPE_POWER_MIN 24
#define CAT_MODULE_USR_TYPE_POWER_MAX 31

#define CAT_MODULE_USR_TYPE_MAP(XX) \
    /* usr (24 ~ 31) */ \
    XX(USR1, 1 << 24) \
    XX(USR2, 1 << 25) \
    XX(USR3, 1 << 26) \
    XX(USR4, 1 << 27) \
    XX(USR5, 1 << 28) \
    XX(USR6, 1 << 29) \
    XX(USR7, 1 << 30) \
    XX(USR8, 1 << 31) \

#define CAT_MODULE_MAX_COUNT 32

#define CAT_MODULE_TYPE_MAP(XX) \
    CAT_MODULE_BUILTIN_TYPE_MAP(XX) \
    CAT_MODULE_USR_TYPE_MAP(XX)

typedef enum cat_module_type_e {
#define CAT_MODULE_TYPE_GEN(name, value) CAT_ENUM_GEN(CAT_MODULE_TYPE_, name, value)
    CAT_MODULE_TYPE_MAP(CAT_MODULE_TYPE_GEN)
#undef CAT_MODULE_TYPE_GEN
} cat_module_type_t;

typedef uint32_t cat_module_types_t;

#define CAT_MODULE_TYPES_BUILTIN_VALUE_GEN(_, value)    (value) |
#define CAT_MODULE_TYPES_BUILTIN_VALUE                  CAT_MODULE_BUILTIN_TYPE_MAP(CAT_MODULE_TYPES_BUILTIN_VALUE_GEN) 0
#define CAT_MODULE_TYPES_USR_VALUE_GEN(_, value)        (value) |
#define CAT_MODULE_TYPES_USR_VALUE                      CAT_MODULE_USR_TYPE_MAP(CAT_MODULE_TYPES_USR_VALUE_GEN) 0

#define CAT_MODULE_UNION_TYPE_MAP(XX) \
    XX(BUILTIN, CAT_MODULE_TYPES_BUILTIN_VALUE) \
    XX(USR,     CAT_MODULE_TYPES_USR_VALUE) \
    XX(ALL,     CAT_MODULE_TYPES_BUILTIN_VALUE | CAT_MODULE_TYPES_USR_VALUE) \

typedef enum cat_module_union_types_e {
#define CAT_MODULE_UNION_TYPE_GEN(name, value) CAT_ENUM_GEN(CAT_MODULE_TYPES_, name, value)
    CAT_MODULE_UNION_TYPE_MAP(CAT_MODULE_UNION_TYPE_GEN)
#undef CAT_MODULE_UNION_TYPE_GEN
} cat_module_union_types_t;

// TODO: load module and check dependencies

typedef struct cat_module_info_s {
    cat_module_type_t type;
    const char *name;
    cat_bool_t registered;
    cat_bool_t running;
} cat_module_info_t;

#ifdef CAT_NOT_IMPL
extern CAT_API cat_module_info_t cat_module_info_map[CAT_MODULE_MAX_COUNT];

CAT_API const cat_module_info_t *cat_module_get_info(cat_module_type_t type);
CAT_API const cat_module_info_t *cat_module_get_info_by_name(const char *name);
CAT_API cat_bool_t cat_module_get_available_user_type(cat_module_type_t *type);

CAT_API cat_bool_t cat_module_register(cat_module_type_t type, const char *name, const char **dependencies);
CAT_API cat_bool_t cat_module_unregister(cat_module_type_t type);
CAT_API cat_bool_t cat_module_run(cat_module_type_t type, const char **dependencies);
CAT_API cat_bool_t cat_module_stop(cat_module_type_t type);
#endif /* CAT_NOT_IMPL */

CAT_API cat_module_types_t cat_module_get_types_from_names(const char *names);
