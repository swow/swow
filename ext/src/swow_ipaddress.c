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

#include "swow_ipaddress.h"

SWOW_API zend_class_entry *swow_ipaddress_ce;
SWOW_API zend_object_handlers swow_ipaddress_handlers;

SWOW_API zend_class_entry *swow_ipaddress_exception_ce;

static zend_object *swow_ipaddress_create_object(zend_class_entry *ce)
{
    swow_ipaddress_t *s_address = swow_object_alloc(swow_ipaddress_t, ce, swow_ipaddress_handlers);
    memset(&s_address->ipv6_address, 0, sizeof(s_address->ipv6_address));
    return &s_address->std;
}

#ifdef CAT_DEBUG

typedef struct {
    char *message;
    ipv6_diag_event_t event;
} swow_ipv6_parse_error;

static void swow_ipv6_parse_diag(ipv6_diag_event_t event, const ipv6_diag_info_t *info, swow_ipv6_parse_error *ret)
{
    ret->message = cat_sprintf("%s at position %d", info->message, info->position);
    ret->event = event;
    return;
}

static bool swow_parse_ipaddress(const char* address, size_t address_len, ipv6_address_full_t *dest)
{
    swow_ipv6_parse_error error = { NULL };
    bool ret = (bool) ipv6_from_str_diag(address, address_len, dest, (ipv6_diag_func_t) swow_ipv6_parse_diag, &error);
    if (!ret) {
        char *buffer;
        swow_throw_exception(
            swow_ipaddress_exception_ce,
            error.event,
            "Failed to parse ip address %s: %s",
            cat_log_str_quote_unlimited(address, address_len, &buffer),
            error.message ? error.message : "unknown"
        );
        cat_free(buffer);
    }
    if (error.message) {
        cat_free(error.message);
    }
    return ret;
}

#else

static bool swow_parse_ipaddress(const char* address, size_t address_len, ipv6_address_full_t *dest)
{
    bool ret = ipv6_from_str(address, address_len, dest);
    if (!ret) {
        swow_throw_exception(
            swow_ipaddress_exception_ce,
            0, "Failed to parse ip address"
        );
        return false;
    }
    return true;
}

#endif

static bool swow_ipaddress_in(swow_ipaddress_t *s_address, swow_ipaddress_t *s_cidr)
{
    ipv6_address_full_t *addr = &s_address->ipv6_address;
    ipv6_address_full_t *cidr = &s_cidr->ipv6_address;

    if (!(cidr->flags & IPV6_FLAG_HAS_MASK)) {
        swow_throw_exception(
            swow_ipaddress_exception_ce,
            0, "The range is not a cidr"
        );
        return false;
    }

    if ((addr->flags & IPV6_FLAG_IPV4_COMPAT) != (cidr->flags & IPV6_FLAG_IPV4_COMPAT)) {
        swow_throw_exception(
            swow_ipaddress_exception_ce,
            0, "Address and range addr family mismatch"
        );
        return false;
    }

    uint32_t maskLen = (addr->flags & IPV6_FLAG_HAS_MASK) ? addr->mask : ((addr->flags & IPV6_FLAG_IPV4_COMPAT) ? 32 : 128);
    if (cidr->mask > maskLen) {
        // cannot cover a bigger cidr
        // printf("%d %d\n", cidr->mask, maskLen);
        return false;
    }
    if (addr->flags & IPV6_FLAG_IPV4_COMPAT) {
        // ipv4
        uint32_t beIpv4Addr = addr->address.components[1] + (addr->address.components[0] << 16);
        uint32_t beIpv4Cidr = cidr->address.components[1] + (cidr->address.components[0] << 16);
        uint32_t mask = ((1 << cidr->mask) - 1) << (32 - cidr->mask);
        // printf("%08x %08x %08x\n", beIpv4Addr, beIpv4Cidr, mask);
        return (bool) ((beIpv4Addr & mask) == (beIpv4Cidr & mask));
    } else {
        // ipv6
        uint32_t sameComponents = cidr->mask / 16;
        uint32_t remainingMaskLen = cidr->mask % 16;
        uint32_t i = 0;
        for (; i < sameComponents; i++) {
            if (addr->address.components[i] != cidr->address.components[i]) {
                return false;
            }
        }
        uint32_t mask = ((1 << remainingMaskLen) - 1) << (16 - remainingMaskLen);
        if ((addr->address.components[i] & mask) != (cidr->address.components[i] & mask)) {
            return false;
        }

        return true;
    }
}

#define getThisAddr() (swow_ipaddress_get_from_object(Z_OBJ_P(ZEND_THIS)))

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_IpAddress___construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address, IS_STRING, 0, "\'\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, __construct)
{
    zend_string *address = zend_empty_string;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR_OR_NULL(address)
    ZEND_PARSE_PARAMETERS_END();

    swow_ipaddress_t *s_address = getThisAddr();

    if (ZSTR_LEN(address)) {
        bool ret = swow_parse_ipaddress(ZSTR_VAL(address), ZSTR_LEN(address), &s_address->ipv6_address);
        if (!ret) {
            RETURN_THROWS();
        }
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_IpAddress_getFlags, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, getFlags)
{
    ZEND_PARSE_PARAMETERS_NONE();

    swow_ipaddress_t *s_address = getThisAddr();

    RETURN_LONG(s_address->ipv6_address.flags);
}

#define arginfo_class_Swow_IpAddress_getFull arginfo_class_Swow_IpAddress_getFlags

static PHP_METHOD(Swow_IpAddress, getFull)
{
    ZEND_PARSE_PARAMETERS_NONE();

    swow_ipaddress_t *s_address = getThisAddr();
    char buffer[IPV6_STRING_SIZE];

    if (
        (s_address->ipv6_address.flags & IPV6_FLAG_IPV4_COMPAT) &&
        (s_address->ipv6_address.flags & IPV6_FLAG_HAS_MASK)
    ) {
        // upstream bug? here
        // no standard for how to generate a IPV4 CIDR with port
        // so we make it here
        size_t ret = snprintf(
            buffer,
            sizeof(buffer),
            "%d.%d.%d.%d/%d",
            s_address->ipv6_address.address.components[0] >> 8,
            s_address->ipv6_address.address.components[0] & 0xff,
            s_address->ipv6_address.address.components[1] >> 8,
            s_address->ipv6_address.address.components[1] & 0xff,
            s_address->ipv6_address.mask
        );

        RETURN_STRINGL(buffer, ret);
    }

    uint32_t flags = s_address->ipv6_address.flags;
    // clean flags to generate ip/mask part only
    s_address->ipv6_address.flags &= ~(IPV6_FLAG_HAS_PORT);
    size_t ret = ipv6_to_str(&s_address->ipv6_address, ZEND_STRS(buffer));
    s_address->ipv6_address.flags = flags;

    RETURN_STRINGL(buffer, ret);
}

#define arginfo_class_Swow_IpAddress_getIp arginfo_class_Swow_IpAddress_getFlags

static PHP_METHOD(Swow_IpAddress, getIp)
{
    swow_ipaddress_t *s_address = getThisAddr();
    char buffer[IPV6_STRING_SIZE];

    ZEND_PARSE_PARAMETERS_NONE();

    uint32_t flags = s_address->ipv6_address.flags;
    // clean flags to generate ip part only
    s_address->ipv6_address.flags &= ~(IPV6_FLAG_HAS_PORT | IPV6_FLAG_HAS_MASK | IPV6_FLAG_IPV4_EMBED);
    size_t ret = ipv6_to_str(&s_address->ipv6_address, ZEND_STRS(buffer));
    s_address->ipv6_address.flags = flags;

    RETURN_STRINGL(buffer, ret);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_IpAddress_getPort, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, getPort)
{
    swow_ipaddress_t *s_address = getThisAddr();

    RETURN_LONG(s_address->ipv6_address.port);
}

#define arginfo_class_Swow_IpAddress_getMaskLen arginfo_class_Swow_IpAddress_getPort

static PHP_METHOD(Swow_IpAddress, getMaskLen)
{
    swow_ipaddress_t *s_address = getThisAddr();

    RETURN_LONG(s_address->ipv6_address.mask);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_IpAddress_setFlags, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, setFlags, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, setFlags)
{
    zend_long flags;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    swow_ipaddress_t *s_address = getThisAddr();

    if (flags & (~(IPV6_FLAG_HAS_MASK | IPV6_FLAG_HAS_PORT | IPV6_FLAG_IPV4_COMPAT | IPV6_FLAG_IPV4_EMBED))) {
        swow_throw_exception(
            zend_ce_value_error,
            0,
            ZEND_LONG_FMT " is not a valid flag",
            flags
        );
    }

    s_address->ipv6_address.flags = (uint32_t) flags;

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_IpAddress_setFull, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, address, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, setFull)
{
    zend_string *address;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(address)
    ZEND_PARSE_PARAMETERS_END();

    swow_ipaddress_t *s_address = getThisAddr();

    bool ret = swow_parse_ipaddress(ZSTR_VAL(address), ZSTR_LEN(address), &s_address->ipv6_address);
    if (!ret) {
        RETURN_THROWS();
    }

    RETURN_THIS();
}

#define arginfo_class_Swow_IpAddress_setIp arginfo_class_Swow_IpAddress_setFull

static PHP_METHOD(Swow_IpAddress, setIp)
{
    zend_string *address;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(address)
    ZEND_PARSE_PARAMETERS_END();

    swow_ipaddress_t *s_address = getThisAddr();
    ipv6_address_full_t addr;

    bool ret = swow_parse_ipaddress(ZSTR_VAL(address), ZSTR_LEN(address), &addr);
    if (!ret) {
        RETURN_THROWS();
    }

    memcpy(&s_address->ipv6_address.address, &addr.address, sizeof(addr.address));
    if (addr.flags & IPV6_FLAG_IPV4_COMPAT) {
        s_address->ipv6_address.flags |= IPV6_FLAG_IPV4_COMPAT;
    }  else {
        s_address->ipv6_address.flags &= ~IPV6_FLAG_IPV4_COMPAT;
    }

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_IpAddress_setPort, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, port, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, setPort)
{
    zend_long port;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(port)
    ZEND_PARSE_PARAMETERS_END();

    swow_ipaddress_t *s_address = getThisAddr();

    if (port < 0 || port > UINT16_MAX) {
        swow_throw_exception(
            zend_ce_value_error,
            0,
            ZEND_LONG_FMT " is not a valid port number (range: 0-65535)",
            port
        );
    }

    s_address->ipv6_address.port = (uint16_t) port;

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_IpAddress_setMaskLen, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, maskLen, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, setMaskLen)
{
    zend_long maskLen;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(maskLen)
    ZEND_PARSE_PARAMETERS_END();

    swow_ipaddress_t *s_address = getThisAddr();

    const char *addrType = "ipv6";
    zend_long max = 128;
    if (s_address->ipv6_address.flags & IPV6_FLAG_IPV4_COMPAT) {
        addrType = "ipv4";
        max = 32;
    }
    if (maskLen < 0 || maskLen > max) {
        swow_throw_exception(
            zend_ce_value_error,
            0,
            ZEND_LONG_FMT " is not a valid mask length for %s (range: 0-" ZEND_LONG_FMT ")",
            maskLen,
            addrType,
            max
        );
    }
    s_address->ipv6_address.mask = maskLen;

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_IpAddress_isMappedIpv4, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, isMappedIpv4)
{
    ZEND_PARSE_PARAMETERS_NONE();

    swow_ipaddress_t *s_address = getThisAddr();

    if (
        !(s_address->ipv6_address.flags & IPV6_FLAG_IPV4_COMPAT) &&
        // mapped ipv4
        memcmp(&s_address->ipv6_address.address, ZEND_STRL("\0\0\0\0\0\0\0\0\0\0\xff\xff")) == 0
    ) {
        RETURN_TRUE;
    }

    RETURN_FALSE;
}

#define arginfo_class_Swow_IpAddress_isIpv4OrMappedIpv4 arginfo_class_Swow_IpAddress_isMappedIpv4

static PHP_METHOD(Swow_IpAddress, isIpv4OrMappedIpv4)
{
    ZEND_PARSE_PARAMETERS_NONE();

    swow_ipaddress_t *s_address = getThisAddr();

    if (
        // real ipv4
        (s_address->ipv6_address.flags & IPV6_FLAG_IPV4_COMPAT) ||
        // mapped ipv4
        memcmp(&s_address->ipv6_address.address, ZEND_STRL("\0\0\0\0\0\0\0\0\0\0\xff\xff")) == 0
    ) {
        RETURN_TRUE;
    }

    RETURN_FALSE;
}

#define arginfo_class_Swow_IpAddress_isLocal arginfo_class_Swow_IpAddress_isMappedIpv4

static PHP_METHOD(Swow_IpAddress, isLocal)
{
    ZEND_PARSE_PARAMETERS_NONE();

    swow_ipaddress_t *s_address = getThisAddr();
    uint32_t beIpv4;

    // char * _;
    // printf("???? %s\n", cat_log_str_quote_unlimited(s_address->ipv6_address.address.components, 16, &_));

    if (
        // real ipv4
        (s_address->ipv6_address.flags & IPV6_FLAG_IPV4_COMPAT))
    {
        beIpv4 = s_address->ipv6_address.address.components[1] + (s_address->ipv6_address.address.components[0] << 16);
        goto v4;
    } else if (
        // mapped ipv4
        memcmp(&s_address->ipv6_address.address, ZEND_STRL("\0\0\0\0\0\0\0\0\0\0\xff\xff")) == 0
    ) {
        beIpv4 = s_address->ipv6_address.address.components[7] + (s_address->ipv6_address.address.components[6] << 16);
        v4:
        if (
            // 10.0.0.0/8
            (beIpv4 & 0xff000000) == 0x0a000000 ||
            // 100.64.0.0./10
            (beIpv4 & 0xffc00000) == 0x64400000 ||
            // 172.16.0.0/12
            (beIpv4 & 0xfff00000) == 0xac100000 ||
            // 192.168.0.0/16
            (beIpv4 & 0xffff0000) == 0xc0a80000 ||
            // 169.254.0.0/16
            (beIpv4 & 0xffff0000) == 0xa9fe0000
        ) {
            RETURN_TRUE;
        }
        RETURN_FALSE;
    } else {
        if (
            // ula
            s_address->ipv6_address.address.components[0] == 0xfc00 ||
            s_address->ipv6_address.address.components[0] == 0xfd00 ||
            // ll
            s_address->ipv6_address.address.components[0] == 0xfe00
        ) {
            RETURN_TRUE;
        }
        RETURN_FALSE;
    }
}

#define arginfo_class_Swow_IpAddress_isLoopback arginfo_class_Swow_IpAddress_isMappedIpv4

static PHP_METHOD(Swow_IpAddress, isLoopback)
{
    ZEND_PARSE_PARAMETERS_NONE();

    swow_ipaddress_t *s_address = getThisAddr();
    uint32_t beIpv4;

    // char * _;
    // printf("???? %s\n", cat_log_str_quote_unlimited(s_address->ipv6_address.address.components, 16, &_));

    if (
        // real ipv4
        (s_address->ipv6_address.flags & IPV6_FLAG_IPV4_COMPAT))
    {
        beIpv4 = s_address->ipv6_address.address.components[1] + (s_address->ipv6_address.address.components[0] << 16);
        goto v4;
    } else if (
        // mapped ipv4
        memcmp(&s_address->ipv6_address.address, ZEND_STRL("\0\0\0\0\0\0\0\0\0\0\xff\xff")) == 0
    ) {
        beIpv4 = s_address->ipv6_address.address.components[7] + (s_address->ipv6_address.address.components[6] << 16);
        v4:
        if (
            (beIpv4 & 0xff000000) == 0x7f000000
        ) {
            RETURN_TRUE;
        }
        RETURN_FALSE;
    } else {
        if (
            memcmp(&s_address->ipv6_address.address, ZEND_STRL("\0\0\0\0\0\0\0\0\0\0\0\0\0\0")) == 0 &&
            s_address->ipv6_address.address.components[7] == 1
        ) {
            RETURN_TRUE;
        }
        RETURN_FALSE;
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_IpAddress_in, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_OBJ_INFO(0, range, Swow\\IpAddress, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, in)
{
    zend_object *cidr_object;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJ_OF_CLASS(cidr_object, swow_ipaddress_ce)
    ZEND_PARSE_PARAMETERS_END();

    swow_ipaddress_t *s_address = getThisAddr();
    bool ret = swow_ipaddress_in(s_address, swow_ipaddress_get_from_object(cidr_object));
    RETURN_BOOL(ret);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_IpAddress_covers, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_OBJ_INFO(0, addr, Swow\\IpAddress, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, covers)
{
    zend_object *addr_object;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJ_OF_CLASS(addr_object, swow_ipaddress_ce)
    ZEND_PARSE_PARAMETERS_END();

    swow_ipaddress_t *s_address = getThisAddr();
    bool ret = swow_ipaddress_in(swow_ipaddress_get_from_object(addr_object), s_address);
    RETURN_BOOL(ret);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_IpAddress_convertToMappedIpv4, 0, 0, IS_STATIC, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, convertToMappedIpv4)
{
    ZEND_PARSE_PARAMETERS_NONE();

    swow_ipaddress_t *s_address = getThisAddr();

    if (!(s_address->ipv6_address.flags & IPV6_FLAG_IPV4_COMPAT)) {
        swow_throw_exception(
            swow_ipaddress_exception_ce,
            0, "This is not an IPv4 address"
        );
        RETURN_THROWS();
    }

    s_address->ipv6_address.flags = IPV6_FLAG_IPV4_EMBED | (s_address->ipv6_address.flags & (IPV6_FLAG_HAS_MASK | IPV6_FLAG_HAS_PORT));
    if (s_address->ipv6_address.flags & IPV6_FLAG_HAS_MASK) {
        s_address->ipv6_address.mask += 96;
    }

    s_address->ipv6_address.address.components[6] = s_address->ipv6_address.address.components[0];
    s_address->ipv6_address.address.components[7] = s_address->ipv6_address.address.components[1];
    memcpy(&s_address->ipv6_address.address, ZEND_STRL("\0\0\0\0\0\0\0\0\0\0\xff\xff"));

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_IpAddress_convertToIpv4, 0, 0, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, force, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_IpAddress, convertToIpv4)
{
    bool force = false;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(force)
    ZEND_PARSE_PARAMETERS_END();

    swow_ipaddress_t *s_address = getThisAddr();

    if (s_address->ipv6_address.flags & IPV6_FLAG_IPV4_COMPAT) {
        RETURN_THIS();
    }

    if (!force && memcmp(&s_address->ipv6_address.address, ZEND_STRL("\0\0\0\0\0\0\0\0\0\0\xff\xff"))) {
        swow_throw_exception(
            swow_ipaddress_exception_ce,
            0, "This is not an IPv6 address with mapped IPv4"
        );
        RETURN_THROWS();
    }

    s_address->ipv6_address.flags = IPV6_FLAG_IPV4_COMPAT | (s_address->ipv6_address.flags & (IPV6_FLAG_HAS_MASK | IPV6_FLAG_HAS_PORT));

    if (s_address->ipv6_address.mask > 96) {
        s_address->ipv6_address.mask -= 96;
    } else {
        s_address->ipv6_address.mask = 0;
    }

    uint16_t a,b;
    a = s_address->ipv6_address.address.components[6];
    b = s_address->ipv6_address.address.components[7];
    memset(&s_address->ipv6_address.address, 0, 16);
    s_address->ipv6_address.address.components[0] = a;
    s_address->ipv6_address.address.components[1] = b;

    RETURN_THIS();
}


static const zend_function_entry swow_ipaddress_methods[] = {
    PHP_ME(Swow_IpAddress, __construct, arginfo_class_Swow_IpAddress___construct, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, getFlags, arginfo_class_Swow_IpAddress_getFlags, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, getFull, arginfo_class_Swow_IpAddress_getFull, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, getIp, arginfo_class_Swow_IpAddress_getIp, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, getPort, arginfo_class_Swow_IpAddress_getPort, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, getMaskLen, arginfo_class_Swow_IpAddress_getMaskLen, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, setFlags, arginfo_class_Swow_IpAddress_setFlags, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, setFull, arginfo_class_Swow_IpAddress_setFull, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, setIp, arginfo_class_Swow_IpAddress_setIp, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, setPort, arginfo_class_Swow_IpAddress_setPort, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, setMaskLen, arginfo_class_Swow_IpAddress_setMaskLen, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, isMappedIpv4, arginfo_class_Swow_IpAddress_isMappedIpv4, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, isIpv4OrMappedIpv4, arginfo_class_Swow_IpAddress_isIpv4OrMappedIpv4, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, isLocal, arginfo_class_Swow_IpAddress_isLocal, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, isLoopback, arginfo_class_Swow_IpAddress_isLoopback, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, in, arginfo_class_Swow_IpAddress_in, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, covers, arginfo_class_Swow_IpAddress_covers, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, convertToMappedIpv4, arginfo_class_Swow_IpAddress_convertToMappedIpv4, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_IpAddress, convertToIpv4, arginfo_class_Swow_IpAddress_convertToIpv4, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_ipaddress_init(INIT_FUNC_ARGS)
{
    swow_ipaddress_ce = swow_register_internal_class(
        "Swow\\IpAddress", NULL, swow_ipaddress_methods,
        &swow_ipaddress_handlers, NULL,
        true, false,
        swow_ipaddress_create_object, NULL,
        XtOffsetOf(swow_ipaddress_t, std)
    );
    zend_declare_class_constant_long(swow_ipaddress_ce, ZEND_STRL("IPV4"), IPV6_FLAG_IPV4_COMPAT);
    zend_declare_class_constant_long(swow_ipaddress_ce, ZEND_STRL("HAS_PORT"), IPV6_FLAG_HAS_PORT);
    zend_declare_class_constant_long(swow_ipaddress_ce, ZEND_STRL("HAS_MASK"), IPV6_FLAG_HAS_MASK);
    zend_declare_class_constant_long(swow_ipaddress_ce, ZEND_STRL("IPV4_EMBED"), IPV6_FLAG_IPV4_EMBED);

    swow_ipaddress_exception_ce = swow_register_internal_class(
        "Swow\\IpAddressException", swow_exception_ce, NULL, NULL, NULL, true, true, NULL, NULL, 0
    );

    return SUCCESS;
}
