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

#include "cat_websocket.h"

CAT_API const char* cat_websocket_opcode_name(cat_websocket_opcode_t opcode)
{
    switch(opcode) {
#define CAT_WEBSOCKET_OPCODE_NAME_GEN(name, value) case value: return #name;
        CAT_WEBSOCKET_OPCODE_MAP(CAT_WEBSOCKET_OPCODE_NAME_GEN);
#undef CAT_WEBSOCKET_OPCODE_NAME_GEN
    }
    return "UNKNOWN";
}

CAT_API const char* cat_websocket_status_get_description(cat_websocket_status_code_t code)
{
    switch(code)
    {
#define CAT_WEBSOCKET_STATUS_DESCRIPTION_GEN(name, value, description) case value: return description;
        CAT_WEBSOCKET_STATUS_MAP(CAT_WEBSOCKET_STATUS_DESCRIPTION_GEN)
#undef CAT_WEBSOCKET_STATUS_DESCRIPTION_GEN
    }

    return "Unknown websocket status code";
}

CAT_API void cat_websocket_header_init(cat_websocket_header_t *header)
{
    memset(header, 0, sizeof(*header));
    header->opcode = CAT_WEBSOCKET_OPCODE_TEXT;
    header->fin = 1;
}

CAT_API uint8_t cat_websocket_header_get_length(const cat_websocket_header_t *header)
{
    uint8_t header_length = CAT_WEBSOCKET_HEADER_LENGTH;
    uint64_t payload_length = header->payload_length;

    if (likely(payload_length <= CAT_WEBSOCKET_EXT8_MAX_LENGTH)) {
        // header_length += 0;
    } else if (likely(payload_length <= CAT_WEBSOCKET_EXT16_MAX_LENGTH)) {
        header_length += sizeof(uint16_t);
    } else {
        header_length += sizeof(uint64_t);
    }

    if (header->mask) {
        header_length += CAT_WEBSOCKET_MASK_KEY_LENGTH;
    }

    return header_length;
}

CAT_API uint8_t cat_websocket_header_pack(cat_websocket_header_t *header, char *buffer, size_t size)
{
    char *p, *pe;

    if (unlikely(size < CAT_WEBSOCKET_HEADER_LENGTH)) {
        return 0;
    }

    p = buffer + CAT_WEBSOCKET_HEADER_LENGTH;
    pe = buffer + size;

    if (likely(header->payload_length <= CAT_WEBSOCKET_EXT8_MAX_LENGTH)) {
        header->len = (unsigned int) header->payload_length;
    } else if (likely(header->payload_length <= CAT_WEBSOCKET_EXT16_MAX_LENGTH)) {
        if (unlikely(p + sizeof(uint16_t) > pe)) {
            return 0;
        }
        header->len = CAT_WEBSOCKET_EXT16_LENGTH;
        *((uint16_t *) p) = (uint16_t) htons((uint16_t) header->payload_length);
        p += sizeof(uint16_t);
    } else {
        if (unlikely(p + sizeof(uint64_t) > pe)) {
            return 0;
        }
        header->len = CAT_WEBSOCKET_EXT64_LENGTH;
        *((uint64_t *) p) = cat_hton64(header->payload_length);
        p += sizeof(uint64_t);
    }
    memcpy(buffer, header, CAT_WEBSOCKET_HEADER_LENGTH);

    if (header->mask) {
        if (unlikely(p + CAT_WEBSOCKET_MASK_KEY_LENGTH > pe)) {
            return 0;
        }
        memcpy(p, header->mask_key, CAT_WEBSOCKET_MASK_KEY_LENGTH);
        p += CAT_WEBSOCKET_MASK_KEY_LENGTH;
    }

    return (uint8_t) (p - buffer);
}

CAT_API uint8_t cat_websocket_header_unpack(cat_websocket_header_t *header, const char *data, size_t length)
{
    const char *p, *pe;

    if (unlikely(length < CAT_WEBSOCKET_HEADER_LENGTH)) {
        return 0;
    }
    memcpy(header, data, CAT_WEBSOCKET_HEADER_LENGTH);
    p = data + CAT_WEBSOCKET_HEADER_LENGTH;
    pe = data + length;

    if (header->len < CAT_WEBSOCKET_EXT16_LENGTH) {
        header->payload_length = header->len;
    } else if (header->len != CAT_WEBSOCKET_EXT64_LENGTH) {
        if (unlikely(p + sizeof(uint16_t) > pe)) {
            return 0;
        }
        header->payload_length = ntohs(*((uint16_t *) p));
        p += sizeof(uint16_t);
    } else {
        if (unlikely(p + sizeof(uint64_t) > pe)) {
            return 0;
        }
        header->payload_length = cat_ntoh64(*((uint64_t *) p));
        p += sizeof(uint64_t);
    }

    if (header->mask) {
        if (unlikely(p + CAT_WEBSOCKET_MASK_KEY_LENGTH > pe)) {
            return 0;
        }
        memcpy(header->mask_key, p, CAT_WEBSOCKET_MASK_KEY_LENGTH);
        p += CAT_WEBSOCKET_MASK_KEY_LENGTH;
    }

    return (uint8_t) (p - data);
}

CAT_API void cat_websocket_mask(char *to, char *from, uint64_t length, const char *mask_key)
{
    if (memcmp(mask_key, CAT_STRS(CAT_WEBSOCKET_EMPTY_MASK_KEY)) == 0) {
        return;
    }
#ifndef CAT_L64
    uint64_t i = 0;
#else
    uint64_t i, chunk = length / 8;
    if (chunk > 0) {
        uint64_t mask_key_u64 = ((uint64_t) (*((uint32_t *) mask_key)) << 32) | *((uint32_t *) mask_key);
        for (i = 0; i < chunk; i++) {
            ((uint64_t *) to)[i] = ((uint64_t *) from)[i] ^ mask_key_u64;
        }
    }
    i = chunk * 8;
#endif
    for (; i < length; i++) {
        to[i] = from[i] ^ mask_key[i % CAT_WEBSOCKET_MASK_KEY_LENGTH];
    }
}

CAT_API void cat_websocket_unmask(char *data, uint64_t length, const char *mask_key)
{
    if (memcmp(mask_key, CAT_STRS(CAT_WEBSOCKET_EMPTY_MASK_KEY)) == 0) {
        return;
    }
#ifndef CAT_L64
    uint64_t i = 0;
#else
    uint64_t i, chunk = length / 8;
    if (chunk > 0) {
        uint64_t mask_key_u64 = ((uint64_t) (*((uint32_t *) mask_key)) << 32) | *((uint32_t *) mask_key);
        for (i = 0; i < chunk; i++) {
            ((uint64_t *) data)[i] ^= mask_key_u64;
        }
    }
    i = chunk * 8;
#endif
    for (; i < length; i++) {
        data[i] ^= mask_key[i % CAT_WEBSOCKET_MASK_KEY_LENGTH];
    }
}
