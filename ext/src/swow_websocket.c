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

#include "swow_websocket.h"

#include "swow_buffer.h"

SWOW_API zend_class_entry *swow_websocket_ce;
SWOW_API zend_class_entry *swow_websocket_opcode_ce;
SWOW_API zend_class_entry *swow_websocket_status_ce;
SWOW_API zend_class_entry *swow_websocket_header_ce;

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Opcode_getNameOf, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, opcode, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Opcode, getNameOf)
{
    zend_long opcode;
    const char *name;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(opcode)
    ZEND_PARSE_PARAMETERS_END();

    name = cat_websocket_opcode_get_name(opcode);

    RETURN_STRING(name);
}

static const zend_function_entry swow_websocket_opcode_methods[] = {
    PHP_ME(Swow_WebSocket_Opcode, getNameOf, arginfo_class_Swow_WebSocket_Opcode_getNameOf, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Status_getDescriptionOf, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, code, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Status, getDescriptionOf)
{
    zend_long code;
    const char *description;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(code)
    ZEND_PARSE_PARAMETERS_END();

    description = cat_websocket_status_get_description(code);

    RETURN_STRING(description);
}

static const zend_function_entry swow_websocket_status_methods[] = {
    PHP_ME(Swow_WebSocket_Status, getDescriptionOf, arginfo_class_Swow_WebSocket_Status_getDescriptionOf, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

static zend_object *swow_websocket_header_create_object(zend_class_entry *ce)
{
    zend_object *buffer_object = swow_buffer_ce->create_object(ce);
    swow_buffer_t *s_buffer = swow_buffer_get_from_object(buffer_object);

    (void) cat_buffer_alloc(&s_buffer->buffer, sizeof(cat_websocket_header_t));
    cat_websocket_header_init((cat_websocket_header_t *) s_buffer->buffer.value);
    swow_buffer_virtual_write(s_buffer, 0, CAT_WEBSOCKET_HEADER_MIN_SIZE);

    return buffer_object;
}

#define getThisHeaderBuffer() (swow_buffer_get_from_object(Z_OBJ_P(ZEND_THIS)))

#define SWOW_WEBSOCKET_HEADER_GETTER(_s_header, _header) \
    swow_buffer_t *_s_header = getThisHeaderBuffer(); \
    cat_websocket_header_t *_header = (cat_websocket_header_t *) _s_header->buffer.value; \
    do { \
        if (UNEXPECTED(_header == NULL || _s_header->buffer.size < CAT_WEBSOCKET_HEADER_MAX_SIZE)) { \
            zend_throw_error(NULL, "WebSocket header buffer is unavailable"); \
        } \
    } while (0)

#define SWOW_WEBSOCKET_HEADER_MASK_KEY_CHECK(mask_key) do { \
    if (mask_key == NULL || ZSTR_LEN(mask_key) == 0) { \
        mask_key = NULL; \
    } else if (UNEXPECTED(ZSTR_LEN(mask_key) != CAT_WEBSOCKET_MASK_KEY_LENGTH)) { \
        zend_argument_value_error(1, "length should be %u", CAT_WEBSOCKET_MASK_KEY_LENGTH); \
        RETURN_THROWS(); \
    } \
} while (0)


ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_WebSocket_Header___construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, opcode, IS_LONG, 0, "Swow\\WebSocket\\Opcode::TEXT")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, payloadLength, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maskKey, IS_STRING, 0, "\'\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, fin, _IS_BOOL, 0, "true")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, rsv1, _IS_BOOL, 0, "false")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, rsv2, _IS_BOOL, 0, "false")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, rsv3, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, __construct)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zend_long opcode = CAT_WEBSOCKET_OPCODE_TEXT;
    zend_long payload_length = 0;
    zend_string *mask_key = NULL;
    zend_bool fin = true;
    zend_bool rsv1 = false;
    zend_bool rsv2 = false;
    zend_bool rsv3 = false;

    ZEND_PARSE_PARAMETERS_START(0, 7)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(opcode)
        Z_PARAM_LONG(payload_length)
        Z_PARAM_STR(mask_key)
        Z_PARAM_BOOL(fin)
        Z_PARAM_BOOL(rsv1)
        Z_PARAM_BOOL(rsv2)
        Z_PARAM_BOOL(rsv3)
    ZEND_PARSE_PARAMETERS_END();

    SWOW_WEBSOCKET_HEADER_MASK_KEY_CHECK(mask_key);

    cat_websocket_header_init(header);
    header->opcode = opcode;
    header->fin = fin;
    header->rsv1 = rsv1;
    header->rsv2 = rsv2;
    header->rsv3 = rsv3;
    cat_websocket_header_set_payload_info(
        header,
        payload_length,
        mask_key != NULL ? ZSTR_VAL(mask_key) : NULL
    );
    swow_buffer_update(s_header, cat_websocket_header_get_size(header));

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_getHeaderSize, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, getHeaderSize)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(cat_websocket_header_get_size(header));
}

#define arginfo_class_Swow_WebSocket_Header_getOpcode arginfo_class_Swow_WebSocket_Header_getHeaderSize

static PHP_METHOD(Swow_WebSocket_Header, getOpcode)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_LONG(header->opcode);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_setOpcode, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, opcode, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, setOpcode)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zend_long opcode;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(opcode)
    ZEND_PARSE_PARAMETERS_END();

    header->opcode = opcode;

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_getFin, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, getFin)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(header->fin);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_setFin, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, fin, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, setFin)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zend_bool fin;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(fin)
    ZEND_PARSE_PARAMETERS_END();

    header->fin = fin;

    RETURN_THIS();
}

#define arginfo_class_Swow_WebSocket_Header_getRSV1 arginfo_class_Swow_WebSocket_Header_getFin

static PHP_METHOD(Swow_WebSocket_Header, getRSV1)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(header->rsv1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_setRSV1, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, rsv1, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, setRSV1)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zend_bool rsv1;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(rsv1)
    ZEND_PARSE_PARAMETERS_END();

    header->rsv1 = rsv1;

    RETURN_THIS();
}

#define arginfo_class_Swow_WebSocket_Header_getRSV2 arginfo_class_Swow_WebSocket_Header_getFin

static PHP_METHOD(Swow_WebSocket_Header, getRSV2)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(header->rsv2);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_setRSV2, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, rsv2, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, setRSV2)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zend_bool rsv2;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(rsv2)
    ZEND_PARSE_PARAMETERS_END();

    header->rsv2 = rsv2;

    RETURN_THIS();
}

#define arginfo_class_Swow_WebSocket_Header_getRSV3 arginfo_class_Swow_WebSocket_Header_getFin

static PHP_METHOD(Swow_WebSocket_Header, getRSV3)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(header->rsv3);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_setRSV3, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, rsv3, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, setRSV3)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zend_bool rsv3;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(rsv3)
    ZEND_PARSE_PARAMETERS_END();

    header->rsv3 = rsv3;

    RETURN_THIS();
}

#define arginfo_class_Swow_WebSocket_Header_getPayloadLength arginfo_class_Swow_WebSocket_Header_getHeaderSize


static PHP_METHOD(Swow_WebSocket_Header, getPayloadLength)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);

    RETURN_LONG(cat_websocket_header_get_payload_length(header));
}

#define arginfo_class_Swow_WebSocket_Header_getMask arginfo_class_Swow_WebSocket_Header_getFin

static PHP_METHOD(Swow_WebSocket_Header, getMask)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(header->mask);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_getMaskKey, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, getMaskKey)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    const char *mask_key;

    ZEND_PARSE_PARAMETERS_NONE();

    mask_key = cat_websocket_header_get_mask_key(header);
    if (mask_key == NULL) {
        RETURN_EMPTY_STRING();
    }

    RETURN_STRINGL(mask_key, CAT_WEBSOCKET_MASK_KEY_LENGTH);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_setPayloadInfo, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, payloadLength, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, maskKey, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, setPayloadInfo)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zend_long payload_length;
    zend_string *mask_key = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(payload_length)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(mask_key)
    ZEND_PARSE_PARAMETERS_END();

    SWOW_WEBSOCKET_HEADER_MASK_KEY_CHECK(mask_key);

    cat_websocket_header_set_payload_info(
        header,
        payload_length,
        mask_key != NULL ? ZSTR_VAL(mask_key) : NULL
    );
    swow_buffer_update(s_header, cat_websocket_header_get_size(header));

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, __debugInfo)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zval zdebug_info;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(&zdebug_info);
    add_assoc_long(&zdebug_info, "opcode", header->opcode);
    add_assoc_bool(&zdebug_info, "fin", header->fin);
    add_assoc_bool(&zdebug_info, "rsv1", header->rsv1);
    add_assoc_bool(&zdebug_info, "rsv2", header->rsv2);
    add_assoc_bool(&zdebug_info, "rsv3", header->rsv3);
    add_assoc_bool(&zdebug_info, "mask", header->mask);
    add_assoc_long(&zdebug_info, "payload_length", cat_websocket_header_get_payload_length(header));
    do {
        char *mask_key = (char *) cat_websocket_header_get_mask_key(header);
        if (mask_key == NULL) {
            break;
        }
        mask_key = cat_hexprint(mask_key, CAT_WEBSOCKET_MASK_KEY_LENGTH);
        add_assoc_string(&zdebug_info, "mask_key", mask_key);
        cat_free(mask_key);
    } while (0);

    RETURN_DEBUG_INFO_WITH_PROPERTIES(&zdebug_info);
}

static const zend_function_entry swow_websocket_header_methods[] = {
    PHP_ME(Swow_WebSocket_Header, __construct,            arginfo_class_Swow_WebSocket_Header___construct,            ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getHeaderSize,          arginfo_class_Swow_WebSocket_Header_getHeaderSize,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getOpcode,              arginfo_class_Swow_WebSocket_Header_getOpcode,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setOpcode,              arginfo_class_Swow_WebSocket_Header_setOpcode,              ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getFin,                 arginfo_class_Swow_WebSocket_Header_getFin,                 ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setFin,                 arginfo_class_Swow_WebSocket_Header_setFin,                 ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getRSV1,                arginfo_class_Swow_WebSocket_Header_getRSV1,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setRSV1,                arginfo_class_Swow_WebSocket_Header_setRSV1,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getRSV2,                arginfo_class_Swow_WebSocket_Header_getRSV2,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setRSV2,                arginfo_class_Swow_WebSocket_Header_setRSV2,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getRSV3,                arginfo_class_Swow_WebSocket_Header_getRSV3,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setRSV3,                arginfo_class_Swow_WebSocket_Header_setRSV3,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getPayloadLength,       arginfo_class_Swow_WebSocket_Header_getPayloadLength,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getMask,                arginfo_class_Swow_WebSocket_Header_getMask,                ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getMaskKey,             arginfo_class_Swow_WebSocket_Header_getMaskKey,             ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setPayloadInfo,         arginfo_class_Swow_WebSocket_Header_setPayloadInfo,         ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, __debugInfo,            arginfo_class_Swow_WebSocket_Header___debugInfo,            ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_websocket_module_init(INIT_FUNC_ARGS)
{
    swow_websocket_ce = swow_register_internal_class(
        "Swow\\WebSocket", NULL, NULL,
        NULL, NULL, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );

#define SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(name) do { \
    zend_declare_class_constant_long(swow_websocket_ce, ZEND_STRL(#name), CAT_WEBSOCKET_##name); \
} while (0);

#define SWOW_WEBSOCKET_REGISTER_STRING_CONSTANT(name) do { \
    zend_declare_class_constant_string(swow_websocket_ce, ZEND_STRL(#name), CAT_WEBSOCKET_##name); \
} while (0);

    SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(VERSION);
    SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(SECRET_KEY_LENGTH);
    SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(SECRET_KEY_ENCODED_LENGTH);
    SWOW_WEBSOCKET_REGISTER_STRING_CONSTANT(GUID);
    SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(HEADER_MIN_SIZE);
    SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(HEADER_MAX_SIZE);
    SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(EXT16_PAYLOAD_LENGTH);
    SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(EXT64_PAYLOAD_LENGTH);
    SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(EXT8_MAX_LENGTH);
    SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(EXT16_MAX_LENGTH);
    SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(MASK_KEY_LENGTH);
    SWOW_WEBSOCKET_REGISTER_STRING_CONSTANT(EMPTY_MASK_KEY);

    swow_websocket_opcode_ce = swow_register_internal_class(
        "Swow\\WebSocket\\Opcode", NULL, swow_websocket_opcode_methods,
        NULL, NULL, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );
#define SWOW_WEBSOCKET_OPCODE_GEN(name, value) zend_declare_class_constant_long(swow_websocket_opcode_ce, ZEND_STRL(#name), value);
    CAT_WEBSOCKET_OPCODE_MAP(SWOW_WEBSOCKET_OPCODE_GEN)
#undef SWOW_WEBSOCKET_OPCODE_GEN

    swow_websocket_status_ce = swow_register_internal_class(
        "Swow\\WebSocket\\Status", NULL, swow_websocket_status_methods,
        NULL, NULL, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );
#define SWOW_WEBSOCKET_STATUS_GEN(name, value, description) zend_declare_class_constant_long(swow_websocket_status_ce, ZEND_STRL(#name), value);
    CAT_WEBSOCKET_STATUS_MAP(SWOW_WEBSOCKET_STATUS_GEN)
#undef SWOW_WEBSOCKET_STATUS_GEN

    swow_websocket_header_ce = swow_register_internal_class(
        "Swow\\WebSocket\\Header", swow_buffer_ce, swow_websocket_header_methods,
        NULL, NULL, cat_true, cat_false, swow_websocket_header_create_object, NULL, 0
    );

    do {
        cat_websocket_header_t header = { 0 };
        header.fin = 1;
        header.opcode = CAT_WEBSOCKET_OPCODE_PING;
        zend_declare_class_constant_stringl(swow_websocket_header_ce, ZEND_STRL("PING"), (const char *) &header, CAT_WEBSOCKET_HEADER_MIN_SIZE);
        header.opcode = CAT_WEBSOCKET_OPCODE_PONG;
        header.mask = true;
        zend_declare_class_constant_stringl(swow_websocket_header_ce, ZEND_STRL("PONG"), (const char *) &header, CAT_WEBSOCKET_HEADER_MIN_SIZE);
    } while (0);

    return SUCCESS;
}
