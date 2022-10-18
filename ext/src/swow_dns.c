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

static PHP_FUNCTION_EX(_gethostbyname_ex, zend_bool plus_pro_max)
{
    char *hostname;
    zend_long hostname_len;
    zend_long af = AF_INET;
    ZEND_PARSE_PARAMETERS_START(1, (plus_pro_max ? 2 : 1))
        Z_PARAM_PATH(hostname, hostname_len)
        if (plus_pro_max) {
            Z_PARAM_OPTIONAL
            Z_PARAM_LONG(af)
        }
    ZEND_PARSE_PARAMETERS_END();

    if (hostname_len > MAXFQDNLEN) {
        if (plus_pro_max) {
            zend_argument_value_error(1, "Host name cannot be longer than %d characters", MAXFQDNLEN);
            RETURN_THROWS();
        } else {
            /* name too long, protect from CVE-2015-0235 */
            php_error_docref(NULL, E_WARNING, "Host name cannot be longer than %d characters", MAXFQDNLEN);
            RETURN_STRINGL(hostname, hostname_len);
        }
    }

    char buffer[CAT_SOCKET_IPV6_BUFFER_SIZE];
    cat_bool_t ret = cat_dns_get_ip(buffer, sizeof(buffer), hostname, af);
    if (!ret) {
        if (plus_pro_max) {
            swow_throw_exception_with_last_as_reason(spl_ce_RuntimeException, "Failed get address info for host \"%s\"", hostname);
            RETURN_THROWS();
        } else {
            RETURN_STRINGL(hostname, hostname_len);
        }
    }
    RETURN_STRING_FAST(buffer);
}


ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_gethostbyname_ex, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, hostname, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address_family, IS_LONG, 0, "AF_INET")
ZEND_END_ARG_INFO()

static PHP_FUNCTION(gethostbyname_ex)
{
    PHP_FUNCTION_CALL(_gethostbyname_ex, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_gethostbyname, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, hostname, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(gethostbyname)
{
    PHP_FUNCTION_CALL(_gethostbyname_ex, 0);
}

static const zend_function_entry swow_dns_functions[] = {
    PHP_FENTRY(gethostbyname_ex, PHP_FN(gethostbyname_ex), arginfo_gethostbyname_ex, 0)
    PHP_FENTRY(gethostbyname, PHP_FN(gethostbyname), arginfo_gethostbyname, 0)
    PHP_FE_END
};

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
    }

    return SUCCESS;
}
