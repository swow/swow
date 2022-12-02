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

#ifndef CAT_WEBSOCKET_H
#define CAT_WEBSOCKET_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#define CAT_WEBSOCKET_VERSION                   13
#define CAT_WEBSOCKET_SECRET_KEY_LENGTH         16
#define CAT_WEBSOCKET_SECRET_KEY_ENCODED_LENGTH (((CAT_WEBSOCKET_SECRET_KEY_LENGTH + 2) / 3) * 4)
#define CAT_WEBSOCKET_GUID                      "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define CAT_WEBSOCKET_HEADER_MIN_SIZE           2
#define CAT_WEBSOCKET_HEADER_MAX_SIZE           14 /* HEADER_MIN_SIZE + sizeof(uint64_t) + MASKING_KEY_LENGTH */
#define CAT_WEBSOCKET_EXT16_PAYLOAD_LENGTH      126
#define CAT_WEBSOCKET_EXT64_PAYLOAD_LENGTH      127
#define CAT_WEBSOCKET_EXT8_MAX_LENGTH           125
#define CAT_WEBSOCKET_EXT16_MAX_LENGTH          65535
#define CAT_WEBSOCKET_MASKING_KEY_LENGTH        4
#define CAT_WEBSOCKET_EMPTY_MASKING_KEY         "\0\0\0\0"

#define CAT_WEBSOCKET_OPCODE_MAP(XX) \
    XX(CONTINUATION, 0x0) \
    XX(TEXT, 0x1) \
    XX(BINARY, 0x2) \
    XX(CLOSE, 0x8) \
    XX(PING, 0x9) \
    XX(PONG, 0xa)

enum cat_websocket_opcode_e
{
#define CAT_WEBSOCKET_OPCODE_GEN(name, code) CAT_WEBSOCKET_OPCODE_##name = code,
    CAT_WEBSOCKET_OPCODE_MAP(CAT_WEBSOCKET_OPCODE_GEN)
#undef CAT_WEBSOCKET_OPCODE_GEN
};

typedef uint8_t cat_websocket_opcode_t;

CAT_API const char* cat_websocket_opcode_get_name(cat_websocket_opcode_t opcode);

#define CAT_WEBSOCKET_STATUS_MAP(XX) \
    XX(NORMAL_CLOSURE, 1000, "Normal closure; the connection successfully completed whatever purpose for which it was created") \
    XX(GOING_AWAY, 1001, "The endpoint is going away, either because of a server failure or because the browser is navigating away from the page that opened the connection") \
    XX(PROTOCOL_ERROR, 1002, "The endpoint is terminating the connection due to a protocol error") \
    XX(UNSUPPORTED_DATA, 1003, "The connection is being terminated because the endpoint received data of a type it cannot accept (for example, a text-only endpoint received binary data)") \
    XX(NO_STATUS_RECEIVED, 1005, "Indicates that no status code was provided even though one was expected") \
    XX(ABNORMAL_CLOSURE, 1006, "Reserved. Used to indicate that a connection was closed abnormally (that is, with no close frame being sent) when a status code is expected") \
    XX(INVALID_FRAME_PAYLOAD_DATA, 1007, "The endpoint is terminating the connection because a message was received that contained inconsistent data (e.g., non-UTF-8 data within a text message)") \
    XX(POLICY_VIOLATION, 1008, "The endpoint is terminating the connection because it received a message that violates its policy. This is a generic status code, used when codes 1003 and 1009 are not suitable") \
    XX(MESSAGE_TOO_BIG, 1009, "Message too big The endpoint is terminating the connection because a data frame was received that is too large") \
    XX(MISSING_EXTENSION, 1010, "The client is terminating the connection because it expected the server to negotiate one or more extension, but the server didn't") \
    XX(INTERNAL_ERROR, 1011, "The server is terminating the connection because it encountered an unexpected condition that prevented it from fulfilling the request") \
    XX(SERVICE_RESTART, 1012, "The server is terminating the connection because it is restarting") \
    XX(TRY_AGAIN_LATER, 1013, "The server is terminating the connection due to a temporary condition, e.g. it is overloaded and is casting off some of its clients") \
    XX(BAD_GATEWAY, 1014, "The server was acting as a gateway or proxy and received an invalid response from the upstream server. This is similar to 502 HTTP Status Code") \
    XX(TLS_HANDSHAKE, 1015, "Reserved. Indicates that the connection was closed due to a failure to perform a TLS handshake (e.g., the server certificate can't be verified)") \

enum cat_websocket_status_e
{
#define CAT_WEBSOCKET_STATUS_GEN(name, code, description) CAT_WEBSOCKET_STATUS_##name = code,
    CAT_WEBSOCKET_STATUS_MAP(CAT_WEBSOCKET_STATUS_GEN)
#undef CAT_WEBSOCKET_STATUS_GEN
};

typedef uint16_t cat_websocket_status_code_t;

CAT_API const char* cat_websocket_status_get_description(cat_websocket_status_code_t code);

#define CAT_WEBSOCKET_STATUS_CODE_LENGTH                 2
#define CAT_WEBSOCKET_CONTROL_FRAME_MAX_PAYLOAD_LENGTH   125

/*  WebSocket Frame:
 +-+-+-+-+-------+-+-------------+-------------------------------+
 0                   1                   2                   3   |
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 |
 +-+-+-+-+-------+-+-------------+-------------------------------+
 |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 | |1|2|3|       |K|             |                               |
 +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 |     Extended payload length continued, if payload len == 127  |
 + - - - - - - - - - - - - - - - +-------------------------------+
 |                               |Masking-key, if MASK set to 1  |
 +-------------------------------+-------------------------------+
 | Masking-key (continued)       |          Payload Data         |
 +-------------------------------- - - - - - - - - - - - - - - - +
 :                     Payload Data continued ...                :
 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 |                     Payload Data continued ...                |
 +---------------------------------------------------------------+
 */

CAT_API uint8_t cat_websocket_calculate_masking_key_offset(uint64_t payload_length);
CAT_API uint8_t cat_websocket_calculate_header_size(uint64_t payload_length, cat_bool_t mask);

typedef struct cat_websocket_header_s {
    /* public readable/writable */
    uint8_t opcode :4;
    uint8_t rsv3 :1;
    uint8_t rsv2 :1;
    uint8_t rsv1 :1;
    uint8_t fin :1;
    /* public readable / protected writable (should be written by API) */
    uint8_t payload_length :7;
    uint8_t mask :1;
    uint8_t extended_payload_length[8];
    char masking_key[CAT_WEBSOCKET_MASKING_KEY_LENGTH];
} cat_websocket_header_t;

CAT_STATIC_ASSERT(sizeof(cat_websocket_header_t) == CAT_WEBSOCKET_HEADER_MAX_SIZE);

CAT_API void cat_websocket_header_init(cat_websocket_header_t *header);
CAT_API uint64_t cat_websocket_header_get_size(const cat_websocket_header_t *header);
CAT_API uint64_t cat_websocket_header_get_payload_length(const cat_websocket_header_t *header);
CAT_API const char *cat_websocket_header_get_masking_key(const cat_websocket_header_t *header);
CAT_API void cat_websocket_header_set_payload_length(cat_websocket_header_t *header, uint64_t payload_length);
CAT_API void cat_websocket_header_set_masking_key(cat_websocket_header_t *header, const char *masking_key);
CAT_API void cat_websocket_header_set_payload_info(cat_websocket_header_t *header, uint64_t payload_length, const char *masking_key);

/**
 * To convert masked data into unmasked data, or vice versa,
 * the following algorithm is applied.
 * The same algorithm applies regardless of the direction of the translation,
 * e.g., the same steps are applied to mask the data as to unmask the data.
 * Octet i of the transformed data ("transformed-octet-i")
 * is the XOR of octet i of the original data ("original-octet-i") with octet
 * at index i modulo 4 of the masking key ("masking-key-octet-j"):
 *     j = i MOD 4
 *     transformed-octet-i = original-octet-i XOR masking-key-octet-j
 *
 * param index is the index to the payload_data[0]
 */

CAT_API void cat_websocket_mask(const char *from, char *to, uint64_t length, const char *masking_key);
CAT_API void cat_websocket_mask_ex(const char *from, char *to, uint64_t length, const char *masking_key, uint64_t index);

CAT_API void cat_websocket_unmask(char *data, uint64_t length, const char *masking_key);
CAT_API void cat_websocket_unmask_ex(char *data, uint64_t length, const char *masking_key, uint64_t index);

#ifdef __cplusplus
}
#endif
#endif /* CAT_WEBSOCKET_H */
