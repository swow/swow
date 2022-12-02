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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#include "swow_dns.h"
#include "swow_hook.h"

#ifndef MAXFQDNLEN
# define MAXFQDNLEN 255
#endif

static PHP_FUNCTION_EX(_swow_gethostbyname, zend_bool is_v2)
{
    char *hostname;
    size_t hostname_len;
    zend_long af = AF_INET;

    ZEND_PARSE_PARAMETERS_START(1, (is_v2 ? 2 : 1))
        Z_PARAM_PATH(hostname, hostname_len)
        if (is_v2) {
            Z_PARAM_OPTIONAL
            Z_PARAM_LONG(af)
        }
    ZEND_PARSE_PARAMETERS_END();

    if (hostname_len > MAXFQDNLEN) {
        if (is_v2) {
            zend_argument_value_error(1, "Host name cannot be longer than %d characters", MAXFQDNLEN);
            RETURN_THROWS();
        } else {
            /* name too long, protect from CVE-2015-0235 */
            php_error_docref(NULL, E_WARNING, "Host name cannot be longer than %d characters", MAXFQDNLEN);
            RETURN_STRINGL(hostname, hostname_len);
        }
    }

    zend_string *buffer = zend_string_alloc(af == AF_INET ? CAT_SOCKET_IPV4_BUFFER_SIZE : CAT_SOCKET_IPV6_BUFFER_SIZE, false);
    cat_bool_t ret = cat_dns_get_ip(ZSTR_VAL(buffer), ZSTR_LEN(buffer), hostname, af);
    if (!ret) {
        zend_string_release_ex(buffer, false);
        if (is_v2) {
            swow_throw_exception_with_last_as_reason(spl_ce_RuntimeException, "Failed get address info for host \"%s\"", hostname);
            RETURN_THROWS();
        } else {
            RETURN_STRINGL(hostname, hostname_len);
        }
    }
    ZSTR_VAL(buffer)[ZSTR_LEN(buffer) = strlen(ZSTR_VAL(buffer))] = '\0';

    RETURN_STR(buffer);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_gethostbyname, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, hostname, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(swow_gethostbyname)
{
    PHP_FUNCTION_CALL(_swow_gethostbyname, false);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_gethostbyname2, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, hostname, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address_family, IS_LONG, 0, "AF_INET")
ZEND_END_ARG_INFO()

static PHP_FUNCTION(swow_gethostbyname2)
{
    PHP_FUNCTION_CALL(_swow_gethostbyname, true);
}

static const zend_function_entry swow_dns_functions[] = {
    PHP_FENTRY(gethostbyname, PHP_FN(swow_gethostbyname), arginfo_swow_gethostbyname, 0)
    PHP_FENTRY(gethostbyname2, PHP_FN(swow_gethostbyname2), arginfo_swow_gethostbyname2, 0)
    PHP_FE_END
};

static bool has_sockets_extension = false;
static bool af_constants_checked = false;

#define SWOW_OBTAIN_CONSTANT_OWNERSHIP(name) swow_obtain_constant_ownership(ZEND_STRL(name), module_number)

static void swow_obtain_constant_ownership(const char *name, size_t name_length, int module_number)
{
    zend_constant *c = (zend_constant *) zend_hash_str_find_ptr(EG(zend_constants), name, name_length);
    ZEND_ASSERT(c != NULL);
    ZEND_CONSTANT_SET_FLAGS(c, ZEND_CONSTANT_FLAGS(c), module_number);
}

zend_result swow_dns_module_init(INIT_FUNC_ARGS)
{
    if (!swow_hook_internal_functions(swow_dns_functions)) {
        return FAILURE;
    }

    REGISTER_LONG_CONSTANT("AF_UNSPEC", AF_UNSPEC, CONST_PERSISTENT);
    if (!zend_hash_str_find_ptr(&module_registry, ZEND_STRL("sockets"))) {
        REGISTER_LONG_CONSTANT("AF_INET", AF_INET, CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("AF_INET6", AF_INET6, CONST_PERSISTENT);
        // TODO: others?
    } else {
        has_sockets_extension = true;
    }

    return SUCCESS;
}

zend_result swow_dns_runtime_init(INIT_FUNC_ARGS)
{
    if (!af_constants_checked) {
        if (has_sockets_extension) {
            SWOW_OBTAIN_CONSTANT_OWNERSHIP("AF_INET");
            SWOW_OBTAIN_CONSTANT_OWNERSHIP("AF_INET6");
        }
        af_constants_checked = true;
    }

    return SUCCESS;
}
