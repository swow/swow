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

CAT_API const char* cat_websocket_opcode_get_name(cat_websocket_opcode_t opcode)
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

CAT_API uint8_t cat_websocket_calculate_masking_key_offset(uint64_t payload_length)
{
    uint8_t header_size = CAT_WEBSOCKET_HEADER_MIN_SIZE;

    if (payload_length <= CAT_WEBSOCKET_EXT8_MAX_LENGTH) {
        // header_length += 0;
    } else if (payload_length <= CAT_WEBSOCKET_EXT16_MAX_LENGTH) {
        header_size += sizeof(uint16_t);
    } else {
        header_size += sizeof(uint64_t);
    }

    return header_size;
}

CAT_API uint8_t cat_websocket_calculate_header_size(uint64_t payload_length, cat_bool_t mask)
{
    uint8_t header_size = cat_websocket_calculate_masking_key_offset(payload_length);

    if (mask) {
        header_size += CAT_WEBSOCKET_MASKING_KEY_LENGTH;
    }

    return header_size;
}

CAT_API void cat_websocket_header_init(cat_websocket_header_t *header)
{
    memset(header, 0, CAT_WEBSOCKET_HEADER_MIN_SIZE);
}

static cat_always_inline uint8_t cat_websocket_header_get_masking_key_offset(const cat_websocket_header_t *header)
{
    uint8_t header_size = CAT_WEBSOCKET_HEADER_MIN_SIZE;

    if (header->payload_length < CAT_WEBSOCKET_EXT16_PAYLOAD_LENGTH) {
        // header_length += 0;
    } else if (header->payload_length == CAT_WEBSOCKET_EXT16_PAYLOAD_LENGTH) {
        header_size += sizeof(uint16_t);
    } else {
        CAT_ASSERT(header->payload_length == CAT_WEBSOCKET_EXT64_PAYLOAD_LENGTH);
        header_size += sizeof(uint64_t);
    }

    return header_size;
}

CAT_API uint64_t cat_websocket_header_get_size(const cat_websocket_header_t *header)
{
    uint8_t header_size = cat_websocket_header_get_masking_key_offset(header);

    if (header->mask) {
        header_size += CAT_WEBSOCKET_MASKING_KEY_LENGTH;
    }

    return header_size;
}

CAT_API uint64_t cat_websocket_header_get_payload_length(const cat_websocket_header_t *header)
{
    if (header->payload_length < CAT_WEBSOCKET_EXT16_PAYLOAD_LENGTH) {
        return header->payload_length;
    } else if (header->payload_length == CAT_WEBSOCKET_EXT16_PAYLOAD_LENGTH) {
        return ntohs(*((uint16_t *) &header->extended_payload_length));
    } else {
        CAT_ASSERT(header->payload_length == CAT_WEBSOCKET_EXT64_PAYLOAD_LENGTH);
        return cat_ntoh64(*((uint64_t *) &header->extended_payload_length));
    }
}

CAT_API const char *cat_websocket_header_get_masking_key(const cat_websocket_header_t *header)
{
    const char *p = NULL;
    if (header->mask) {
        p = (const char *) header;
        p += cat_websocket_header_get_masking_key_offset(header);
    }
    return p;
}

CAT_API void cat_websocket_header_set_payload_length(cat_websocket_header_t *header, uint64_t payload_length)
{
    if (header->mask) {
        char *p = ((char *) header) + cat_websocket_header_get_masking_key_offset(header);
        char *new_p = ((char *) header) + cat_websocket_calculate_masking_key_offset(payload_length);
        memmove(new_p, p, CAT_WEBSOCKET_MASKING_KEY_LENGTH);
    }
    if (likely(payload_length <= CAT_WEBSOCKET_EXT8_MAX_LENGTH)) {
        header->payload_length = (uint8_t) payload_length;
    } else if (likely(payload_length <= CAT_WEBSOCKET_EXT16_MAX_LENGTH)) {
        header->payload_length = CAT_WEBSOCKET_EXT16_PAYLOAD_LENGTH;
        *((uint16_t *) &header->extended_payload_length) = (uint16_t) htons((uint16_t) payload_length);
    } else {
        header->payload_length = CAT_WEBSOCKET_EXT64_PAYLOAD_LENGTH;
        *((uint64_t *) &header->extended_payload_length) = cat_hton64(payload_length);
    }
}


CAT_API void cat_websocket_header_set_masking_key(cat_websocket_header_t *header, const char *masking_key)
{
    if (masking_key == NULL) {
        header->mask = cat_false;
    } else {
        char *p = ((char *) header);
        p += cat_websocket_header_get_masking_key_offset(header);
        memcpy(p, masking_key, CAT_WEBSOCKET_MASKING_KEY_LENGTH);
        header->mask = cat_true;
    }
}

CAT_API void cat_websocket_header_set_payload_info(cat_websocket_header_t *header, uint64_t payload_length, const char *masking_key)
{
    CAT_ASSERT(masking_key != header->masking_key && "it leads to memory overlap");
    header->mask = cat_false; // skip masking_key check in set_payload_length()
    cat_websocket_header_set_payload_length(header, payload_length);
    cat_websocket_header_set_masking_key(header, masking_key);
}

/* The difference between mask1 and mask2 is that
 * mask1 only needs to do calculate for the p++,
 * but mask2 should not only calculate for the from++, but also for the to++,
 * and for very large data, the amount of calculation is relatively reduced.
 *
 * @notice x % (2 ^ k) is equivalent to x & (2 ^ k - 1)
 */
static void cat_websocket_mask1(char *data, uint64_t length, const char *masking_key, uint64_t index)
{
    char *p = data;
    const char *pe = p + length;
#ifdef CAT_L64
    if (p + sizeof(uint64_t) <= pe) {
        for (; p < pe && ((index & (sizeof(uint64_t) - 1)) != 0); p++, index++) {
            *p ^= masking_key[index & (CAT_WEBSOCKET_MASKING_KEY_LENGTH - 1)];
        }
        uint64_t masking_key_u64 = ((uint64_t) (*((uint32_t *) masking_key)) << 32) | *((uint32_t *) masking_key);
        uint64_t unmasked_length_of_u64 = pe - p;
        unmasked_length_of_u64 = unmasked_length_of_u64 - (unmasked_length_of_u64 & (sizeof(uint64_t) - 1));
        index += unmasked_length_of_u64;
        const char *pe_of_u64 = p + unmasked_length_of_u64;
        for (; p < pe_of_u64; p += sizeof(uint64_t)) {
            *((uint64_t *) p) ^= masking_key_u64;
        }
    }
#endif
    for (; p < pe; index++, p++) {
        *p ^= masking_key[index & (CAT_WEBSOCKET_MASKING_KEY_LENGTH - 1)];
    }
}

static void cat_websocket_mask2(const char *from, char *to, uint64_t length, const char *masking_key, uint64_t index)
{
    const char *to_end = to + length;
#ifdef CAT_L64
    if (to + sizeof(uint64_t) <= to_end) {
        for (; to < to_end && ((index & (sizeof(uint64_t) - 1)) != 0); from++, to++, index++) {
            *to = *from ^ masking_key[index & (CAT_WEBSOCKET_MASKING_KEY_LENGTH - 1)];
        }
        uint64_t masking_key_u64 = ((uint64_t) (*((uint32_t *) masking_key)) << 32) | *((uint32_t *) masking_key);
        uint64_t unmasked_length_of_u64 = to_end - to;
        unmasked_length_of_u64 = unmasked_length_of_u64 - (unmasked_length_of_u64 & (sizeof(uint64_t) - 1));
        index += unmasked_length_of_u64;
        const char *to_end_of_u64 = to + unmasked_length_of_u64;
        for (; to < to_end_of_u64; to += sizeof(uint64_t), from += sizeof(uint64_t)) {
            *((uint64_t *) to) = *((uint64_t *) from) ^ masking_key_u64;
        }
    }
#endif
    for (; to < to_end; index++, to++, from++) {
        *to = *from ^ masking_key[index & (CAT_WEBSOCKET_MASKING_KEY_LENGTH - 1)];
    }
}

CAT_API void cat_websocket_mask(const char *from, char *to, uint64_t length, const char *masking_key)
{
    cat_websocket_mask_ex(from, to, length, masking_key, 0);
}

CAT_API void cat_websocket_unmask(char *data, uint64_t length, const char *masking_key)
{
    cat_websocket_unmask_ex(data, length, masking_key, 0);
}

CAT_API void cat_websocket_mask_ex(const char *from, char *to, uint64_t length, const char *masking_key, uint64_t index)
{
    cat_bool_t masking_key_is_empty = masking_key == NULL || memcmp(masking_key, CAT_STRS(CAT_WEBSOCKET_EMPTY_MASKING_KEY)) == 0;

    if (from == to) {
        if (!masking_key_is_empty) {
            cat_websocket_mask1(to, length, masking_key, index);
        }
    } else {
        if (masking_key_is_empty) {
            memmove(to, from, length);
        } else {
            cat_websocket_mask2(from, to, length, masking_key, index);
        }
    }
}

CAT_API void cat_websocket_unmask_ex(char *data, uint64_t length, const char *masking_key, uint64_t index)
{
    cat_websocket_mask_ex(data, data, length, masking_key, index);
}
