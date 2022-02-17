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

#ifndef CAT_CHANNEL_H
#define CAT_CHANNEL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"
#include "cat_queue.h"

typedef enum cat_channel_flag_e {
    CAT_CHANNEL_FLAG_NONE    = 0,
    CAT_CHANNEL_FLAG_CLOSING = 1 << 0,
    CAT_CHANNEL_FLAG_CLOSED  = 1 << 1,
    CAT_CHANNEL_FLAG_REUSE   = 1 << 2,
} cat_channel_flag_t;

typedef uint8_t cat_channel_flags_t;

typedef uint32_t cat_channel_size_t;
#define CAT_CHANNEL_SIZE_FMT "%u"
#define CAT_CHANNEL_SIZE_FMT_SPEC "u"
#define CAT_CHANNEL_SIZE_MAX UINT32_MAX

typedef uint8_t cat_channel_data_size_t;
#define CAT_CHANNEL_DATA_SIZE_FMT "%u"
#define CAT_CHANNEL_DATA_SIZE_FMT_SPEC "u"
#define CAT_CHANNEL_DATA_SIZE_MAX UINT8_MAX

typedef void (*cat_channel_data_dtor_t)(const cat_data_t *data);

typedef struct cat_channel_bucket_s {
    cat_queue_node_t node;
    char data[1];
} cat_channel_bucket_t;

typedef struct cat_channel_s {
    cat_channel_flags_t flags;
    cat_channel_data_size_t data_size;
    cat_channel_data_dtor_t dtor;
    cat_channel_size_t capacity;
    cat_channel_size_t length;
    cat_queue_t producers;
    cat_queue_t consumers;
    union {
        struct {
            union {
                const cat_data_t *in;
                cat_data_t *out;
            } data;
            union {
                cat_bool_t push;
                cat_bool_t pop;
            } able;
        } unbuffered;
        struct {
            cat_queue_t storage;
        } buffered;
    } u;
} cat_channel_t;

/* common */

CAT_API cat_channel_t *cat_channel_create(cat_channel_t *channel, cat_channel_size_t capacity, cat_channel_data_size_t data_size, cat_channel_data_dtor_t dtor);

CAT_API cat_bool_t cat_channel_push(cat_channel_t *channel, const cat_data_t *data, cat_timeout_t timeout);
CAT_API cat_bool_t cat_channel_pop(cat_channel_t *channel, cat_data_t *data, cat_timeout_t timeout);

/* Notice: close will never break the channel so we can reuse the channel after close done (if necessary) */
CAT_API void cat_channel_close(cat_channel_t *channel);

/* select */

typedef enum cat_channel_opcode_e {
    CAT_CHANNEL_OPCODE_PUSH,
    CAT_CHANNEL_OPCODE_POP,
} cat_channel_opcode_t;

typedef struct cat_channel_select_message_s {
    cat_channel_t *channel;
    union {
        cat_data_t *common;
        const cat_data_t *in;
        cat_data_t *out;
    } data;
    cat_channel_opcode_t opcode;
    cat_bool_t error;
} cat_channel_select_message_t;

typedef cat_channel_select_message_t cat_channel_select_request_t;
typedef cat_channel_select_message_t cat_channel_select_response_t;

CAT_API cat_channel_select_response_t *cat_channel_select(cat_channel_select_request_t *requests, size_t count, cat_timeout_t timeout);

/* status */

CAT_API cat_channel_size_t cat_channel_get_capacity(const cat_channel_t *channel);
CAT_API cat_channel_size_t cat_channel_get_length(const cat_channel_t *channel);
CAT_API cat_bool_t cat_channel_is_available(const cat_channel_t *channel);
CAT_API cat_bool_t cat_channel_has_producers(const cat_channel_t *channel);
CAT_API cat_bool_t cat_channel_has_consumers(const cat_channel_t *channel);
CAT_API cat_bool_t cat_channel_is_empty(const cat_channel_t *channel);
CAT_API cat_bool_t cat_channel_is_full(const cat_channel_t *channel);
CAT_API cat_bool_t cat_channel_is_readable(const cat_channel_t *channel);
CAT_API cat_bool_t cat_channel_is_writable(const cat_channel_t *channel);

/* special */

CAT_API cat_channel_flags_t cat_channel_get_flags(const cat_channel_t *channel);
CAT_API void cat_channel_enable_reuse(cat_channel_t *channel);
CAT_API cat_channel_data_dtor_t cat_channel_get_dtor(const cat_channel_t *channel);
CAT_API cat_channel_data_dtor_t cat_channel_set_dtor(cat_channel_t *channel, cat_channel_data_dtor_t dtor);

/* ext */

CAT_API cat_queue_t *cat_channel_get_storage(cat_channel_t *channel); CAT_INTERNAL

#ifdef __cplusplus
}
#endif
#endif /* CAT_CHANNEL_H */
