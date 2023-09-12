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

SWOW_API zend_class_entry *swow_websocket_websocket_ce;
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

#define SWOW_WEBSOCKET_HEADER_MASKING_KEY_CHECK(masking_key, arg_num) do { \
    if (masking_key == NULL || ZSTR_LEN(masking_key) == 0) { \
        masking_key = NULL; \
    } else if (UNEXPECTED(ZSTR_LEN(masking_key) != CAT_WEBSOCKET_MASKING_KEY_LENGTH)) { \
        zend_argument_value_error(arg_num, "length should be 0 or %u", CAT_WEBSOCKET_MASKING_KEY_LENGTH); \
        RETURN_THROWS(); \
    } \
} while (0)

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_WebSocket_Header___construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, fin, _IS_BOOL, 0, "true")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, rsv1, _IS_BOOL, 0, "false")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, rsv2, _IS_BOOL, 0, "false")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, rsv3, _IS_BOOL, 0, "false")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, opcode, IS_LONG, 0, "Swow\\WebSocket\\Opcode::TEXT")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, payloadLength, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maskingKey, IS_STRING, 0, "\'\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, __construct)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    bool fin = true;
    bool rsv1 = false;
    bool rsv2 = false;
    bool rsv3 = false;
    zend_long opcode = CAT_WEBSOCKET_OPCODE_TEXT;
    zend_long payload_length = 0;
    zend_string *masking_key = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 7)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(fin)
        Z_PARAM_BOOL(rsv1)
        Z_PARAM_BOOL(rsv2)
        Z_PARAM_BOOL(rsv3)
        Z_PARAM_LONG(opcode)
        Z_PARAM_LONG(payload_length)
        Z_PARAM_STR(masking_key)
    ZEND_PARSE_PARAMETERS_END();

    SWOW_WEBSOCKET_HEADER_MASKING_KEY_CHECK(masking_key, 3);

    cat_websocket_header_init(header);
    header->fin = fin;
    header->rsv1 = rsv1;
    header->rsv2 = rsv2;
    header->rsv3 = rsv3;
    header->opcode = opcode;
    cat_websocket_header_set_payload_info(
        header,
        payload_length,
        masking_key != NULL ? ZSTR_VAL(masking_key) : NULL
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
    bool fin;

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
    bool rsv1;

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
    bool rsv2;

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
    bool rsv3;

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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_setPayloadLength, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, payloadLength, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, setPayloadLength)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zend_long payload_length;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(payload_length)
    ZEND_PARSE_PARAMETERS_END();

    cat_websocket_header_set_payload_length(header, payload_length);
    swow_buffer_update(s_header, cat_websocket_header_get_size(header));

    RETURN_THIS();
}

#define arginfo_class_Swow_WebSocket_Header_getMask arginfo_class_Swow_WebSocket_Header_getFin

static PHP_METHOD(Swow_WebSocket_Header, getMask)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(header->mask);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_getMaskingKey, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, getMaskingKey)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    const char *masking_key;

    ZEND_PARSE_PARAMETERS_NONE();

    masking_key = cat_websocket_header_get_masking_key(header);
    if (masking_key == NULL) {
        RETURN_EMPTY_STRING();
    }

    RETURN_STRINGL(masking_key, CAT_WEBSOCKET_MASKING_KEY_LENGTH);
}


ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_setMaskingKey, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, maskingKey, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, setMaskingKey)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zend_string *masking_key = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(masking_key)
    ZEND_PARSE_PARAMETERS_END();

    SWOW_WEBSOCKET_HEADER_MASKING_KEY_CHECK(masking_key, 1);

    cat_websocket_header_set_masking_key(header, masking_key != NULL ? ZSTR_VAL(masking_key) : NULL);
    swow_buffer_update(s_header, cat_websocket_header_get_size(header));

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header_setPayloadInfo, 0, 1, IS_STATIC, 0)
    ZEND_ARG_TYPE_INFO(0, payloadLength, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maskingKey, IS_STRING, 0, "\'\'")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, setPayloadInfo)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zend_long payload_length;
    zend_string *masking_key = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(payload_length)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(masking_key)
    ZEND_PARSE_PARAMETERS_END();

    SWOW_WEBSOCKET_HEADER_MASKING_KEY_CHECK(masking_key, 2);

    cat_websocket_header_set_payload_info(
        header,
        payload_length,
        masking_key != NULL ? ZSTR_VAL(masking_key) : NULL
    );
    swow_buffer_update(s_header, cat_websocket_header_get_size(header));

    RETURN_THIS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_Header___debugInfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_Header, __debugInfo)
{
    SWOW_WEBSOCKET_HEADER_GETTER(s_header, header);
    zval z_debug_info;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(&z_debug_info);
    add_assoc_long(&z_debug_info, "opcode", header->opcode);
    add_assoc_bool(&z_debug_info, "fin", header->fin);
    add_assoc_bool(&z_debug_info, "rsv1", header->rsv1);
    add_assoc_bool(&z_debug_info, "rsv2", header->rsv2);
    add_assoc_bool(&z_debug_info, "rsv3", header->rsv3);
    add_assoc_bool(&z_debug_info, "mask", header->mask);
    add_assoc_long(&z_debug_info, "payload_length", cat_websocket_header_get_payload_length(header));
    do {
        const char *masking_key = cat_websocket_header_get_masking_key(header);
        if (masking_key == NULL) {
            break;
        }
        char *escaped_masking_key;
        size_t escaped_masking_key_length;
        cat_str_quote_ex(
            masking_key, CAT_WEBSOCKET_MASKING_KEY_LENGTH,
            &escaped_masking_key, &escaped_masking_key_length,
            CAT_STR_QUOTE_STYLE_FLAG_OMIT_LEADING_TRAILING_QUOTES | CAT_STR_QUOTE_STYLE_FLAG_PRINT_NON_ASCII_STRINGS_IN_HEX,
            NULL, NULL
        );
        add_assoc_stringl(&z_debug_info, "masking_key", escaped_masking_key, escaped_masking_key_length);
        cat_free(escaped_masking_key);
    } while (0);

    RETURN_DEBUG_INFO_WITH_PROPERTIES(&z_debug_info);
}

static const zend_function_entry swow_websocket_header_methods[] = {
    PHP_ME(Swow_WebSocket_Header, __construct,      arginfo_class_Swow_WebSocket_Header___construct,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getHeaderSize,    arginfo_class_Swow_WebSocket_Header_getHeaderSize,    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getOpcode,        arginfo_class_Swow_WebSocket_Header_getOpcode,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setOpcode,        arginfo_class_Swow_WebSocket_Header_setOpcode,        ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getFin,           arginfo_class_Swow_WebSocket_Header_getFin,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setFin,           arginfo_class_Swow_WebSocket_Header_setFin,           ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getRSV1,          arginfo_class_Swow_WebSocket_Header_getRSV1,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setRSV1,          arginfo_class_Swow_WebSocket_Header_setRSV1,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getRSV2,          arginfo_class_Swow_WebSocket_Header_getRSV2,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setRSV2,          arginfo_class_Swow_WebSocket_Header_setRSV2,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getRSV3,          arginfo_class_Swow_WebSocket_Header_getRSV3,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setRSV3,          arginfo_class_Swow_WebSocket_Header_setRSV3,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getPayloadLength, arginfo_class_Swow_WebSocket_Header_getPayloadLength, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setPayloadLength, arginfo_class_Swow_WebSocket_Header_setPayloadLength, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getMask,          arginfo_class_Swow_WebSocket_Header_getMask,          ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, getMaskingKey,       arginfo_class_Swow_WebSocket_Header_getMaskingKey,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setMaskingKey,       arginfo_class_Swow_WebSocket_Header_setMaskingKey,       ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, setPayloadInfo,   arginfo_class_Swow_WebSocket_Header_setPayloadInfo,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_WebSocket_Header, __debugInfo,      arginfo_class_Swow_WebSocket_Header___debugInfo,      ZEND_ACC_PUBLIC)
    PHP_FE_END
};

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_WebSocket_mask, 0, 1, IS_STRING, 0)
    ZEND_ARG_OBJ_TYPE_MASK(0, data, Stringable, MAY_BE_STRING, NULL)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, start, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maskingKey, IS_STRING, 0, "\'\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, index, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_WebSocket, mask)
{
    zend_string *masking_key = NULL, *data, *masked_data;
    zend_long start = 0;
    zend_long length = -1;
    zend_long index = 0;
    const char *ptr;

    ZEND_PARSE_PARAMETERS_START(1, 5)
        SWOW_PARAM_STRINGABLE_EXPECT_BUFFER_FOR_READING(data)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(start)
        Z_PARAM_LONG(length)
        Z_PARAM_STR(masking_key)
        Z_PARAM_LONG(index)
    ZEND_PARSE_PARAMETERS_END();

    SWOW_WEBSOCKET_HEADER_MASKING_KEY_CHECK(masking_key, 4);

    if (UNEXPECTED(masking_key == NULL ||
        memcmp(masking_key, CAT_STRS(CAT_WEBSOCKET_EMPTY_MASKING_KEY)) == 0)) {
        RETURN_STR_COPY(data);
    }
    ptr = swow_string_get_readable_space(data, start, &length, 1);
    if (UNEXPECTED(length == 0)) {
        RETURN_EMPTY_STRING();
    }
    masked_data = zend_string_alloc(length, false);
    cat_websocket_mask_ex(ptr, ZSTR_VAL(masked_data), length, masking_key != NULL ? ZSTR_VAL(masking_key) : NULL, index);
    ZSTR_VAL(masked_data)[length] = '\0';

    RETURN_STR(masked_data);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_WebSocket_WebSocket_unmask, 0, 1, IS_VOID, 0)
    ZEND_ARG_OBJ_INFO(0, data, Swow\\Buffer, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, start, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "-1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maskingKey, IS_STRING, 0, "\'\'")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, index, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_WebSocket_WebSocket, unmask)
{
    zend_object *buffer_object;
    zend_string *masking_key = NULL;
    swow_buffer_t *data;
    zend_long start = 0;
    zend_long length = -1;
    zend_long index = 0;
    char *ptr;

    ZEND_PARSE_PARAMETERS_START(1, 5)
        Z_PARAM_OBJ_OF_CLASS(buffer_object, swow_buffer_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(start)
        Z_PARAM_LONG(length)
        Z_PARAM_STR(masking_key)
        Z_PARAM_LONG(index)
    ZEND_PARSE_PARAMETERS_END();

    SWOW_WEBSOCKET_HEADER_MASKING_KEY_CHECK(masking_key, 4);

    data = swow_buffer_get_from_object(buffer_object);
    ptr = (char *) swow_buffer_get_readable_space(data, start, &length, 1);

    cat_websocket_unmask_ex(ptr, length, masking_key != NULL ? ZSTR_VAL(masking_key) : NULL, index);
}

static const zend_function_entry swow_websocket_websocket_methods[] = {
    PHP_ME(Swow_WebSocket_WebSocket, mask,   arginfo_class_Swow_WebSocket_WebSocket_mask,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_WebSocket_WebSocket, unmask, arginfo_class_Swow_WebSocket_WebSocket_unmask, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

zend_result swow_websocket_module_init(INIT_FUNC_ARGS)
{
    swow_websocket_websocket_ce = swow_register_internal_class(
        "Swow\\WebSocket\\WebSocket", NULL, swow_websocket_websocket_methods,
        NULL, NULL, cat_false, cat_false,
        swow_create_object_deny, NULL, 0
    );

#define SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(name) do { \
    zend_declare_class_constant_long(swow_websocket_websocket_ce, ZEND_STRL(#name), CAT_WEBSOCKET_##name); \
} while (0);

#define SWOW_WEBSOCKET_REGISTER_STRING_CONSTANT(name) do { \
    zend_declare_class_constant_string(swow_websocket_websocket_ce, ZEND_STRL(#name), CAT_WEBSOCKET_##name); \
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
    SWOW_WEBSOCKET_REGISTER_LONG_CONSTANT(MASKING_KEY_LENGTH);
    SWOW_WEBSOCKET_REGISTER_STRING_CONSTANT(EMPTY_MASKING_KEY);
    SWOW_WEBSOCKET_REGISTER_STRING_CONSTANT(DEFAULT_MASKING_KEY);

    do {
        cat_websocket_header_t header;
        cat_websocket_header_init(&header);
        header.fin = 1;
        header.opcode = CAT_WEBSOCKET_OPCODE_PING;
        zend_declare_class_constant_stringl(swow_websocket_websocket_ce, ZEND_STRL("PING_FRAME"), (const char *) &header, cat_websocket_header_get_size(&header));
        header.opcode = CAT_WEBSOCKET_OPCODE_PONG;
        cat_websocket_header_set_payload_info(&header, 0, CAT_WEBSOCKET_EMPTY_MASKING_KEY);
        zend_declare_class_constant_stringl(swow_websocket_websocket_ce, ZEND_STRL("PONG_FRAME"), (const char *) &header, cat_websocket_header_get_size(&header));
    } while (0);

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

    return SUCCESS;
}
