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

#include "cat.h"

#ifdef CAT_NOT_IMPL
CAT_API cat_module_info_t cat_module_info_map[CAT_MODULE_MAX_COUNT];

CAT_API const cat_module_info_t *cat_module_get_info(cat_module_type_t type)
{
    int pos = cat_bit_pos(type);

    if (pos < 0 || (unsigned int) pos > (CAT_ARRAY_SIZE(cat_module_info_map) - 1)) {
        return NULL;
    }

    return &cat_module_info_map[pos];
}

CAT_API const cat_module_info_t *cat_module_get_info_by_name(const char *name)
{
    size_t i;

    for (i = 0; i < CAT_ARRAY_SIZE(cat_module_info_map); i++) {
        cat_module_info_t *info = &cat_module_info_map[i];
        if (info->registered && cat_strcasecmp(name, info->name) == 0) {
            return info;
        }
    }

    return NULL;
}

CAT_API cat_bool_t cat_module_get_available_user_type(cat_module_type_t *type)
{
    size_t i;

    for (i = CAT_MODULE_USR_TYPE_POWER_MIN; i < CAT_MODULE_USR_TYPE_POWER_MAX; i++) {
        cat_module_info_t *info = &cat_module_info_map[i];
        if (!info->registered) {
            *type = (cat_module_type_t) (1 << i);
            return cat_true;
        }
    }

    return cat_false;
}

CAT_API cat_bool_t cat_module_register(cat_module_type_t type, const char *name, const char **dependencies)
{
    cat_module_info_t *info = (cat_module_info_t *) cat_module_get_info(type);

    CAT_ASSERT(info != NULL);
    if (info->registered) {
        cat_update_last_error(CAT_EEXIST, "Module type %u has been registered by %s", type, info->name);
        return cat_false;
    }
    if (dependencies != NULL) {
        const char *dependency = *dependencies;
        while (dependency != NULL) {
            const cat_module_info_t *info = cat_module_get_info_by_name(dependency);
            if (info == NULL || !info->registered) {
                cat_update_last_error(CAT_EAGAIN, "Module %s require module %s", name, dependency);
                return cat_false;
            }
            dependency++;
        }
    }
    info->type = type;
    info->name = name != NULL ? name : "Unnamed";
    info->registered = cat_true;

    return cat_true;
}

CAT_API cat_bool_t cat_module_unregister(cat_module_type_t type)
{
    cat_module_info_t *info = (cat_module_info_t *) cat_module_get_info(type);

    CAT_ASSERT(info != NULL);
    if (!info->registered) {
        cat_update_last_error(CAT_EINVAL, "Module type %u has not been registered", type);
        return cat_false;
    }
    info->registered = cat_false;

    return cat_true;
}

CAT_API cat_bool_t cat_module_run(cat_module_type_t type, const char **dependencies)
{
    cat_module_info_t *info = (cat_module_info_t *) cat_module_get_info(type);

    CAT_ASSERT(info != NULL);
    if (!info->registered) {
        cat_update_last_error(CAT_EINVAL, "Module type %u has not been registered", type);
        return cat_false;
    }
    if (info->running) {
        cat_update_last_error(CAT_EALREADY, "Module %s is running", info->name);
        return cat_false;
    }
    if (dependencies != NULL) {
        const char *dependency = *dependencies;
        while (dependency != NULL) {
            const cat_module_info_t *dinfo = cat_module_get_info_by_name(dependency);
            if (dinfo == NULL || !dinfo->running) {
                cat_update_last_error(CAT_EAGAIN, "Module %s require module %s is runable", info->name, dependency);
                return cat_false;
            }
            dependency++;
        }
    }
    info->running = cat_true;

    return cat_true;
}

CAT_API cat_bool_t cat_module_stop(cat_module_type_t type)
{
    cat_module_info_t *info = (cat_module_info_t *) cat_module_get_info(type);

    CAT_ASSERT(info != NULL);
    if (!info->registered) {
        cat_update_last_error(CAT_EINVAL, "Module type %u has not been registered", type);
        return cat_false;
    }
    if (!info->running) {
        cat_update_last_error(CAT_EINVAL, "Module %s is not running", info->name);
        return cat_false;
    }
    info->running = cat_false;

    return cat_true;
}
#endif /* CAT_NOT_IMPL */

CAT_API cat_module_types_t cat_module_get_types_from_names(const char *names)
{
    const char *s = NULL, *e = names;
    cat_module_types_t types = 0;

    while (1) {
        if (*e == ' ' || *e == ',' || *e == '\0') {
            if (s) {
                const char *name;
                size_t name_length;
#define CAT_MODULE_TYPE_CMP_GEN(_name, _unused) \
                name = #_name; \
                name_length = CAT_STRLEN(#_name); \
                if ((size_t) (e - s) == name_length && cat_strncasecmp(s, name, name_length) == 0) { \
                    types |= CAT_MODULE_TYPE_##_name; \
                }
                CAT_MODULE_TYPE_MAP(CAT_MODULE_TYPE_CMP_GEN)
#undef CAT_MODULE_TYPE_CMP_GEN
                s = NULL;
            }
        } else {
            if (!s) {
                s = e;
            }
        }
        if (*e == '\0') {
            break;
        }
        e++;
    }

    return types;
}
