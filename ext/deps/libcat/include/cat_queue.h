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
  | Author: Ben Noordhuis <info@bnoordhuis.nl>                               |
  |         Twosee <twosee@php.net>                                          |
  +--------------------------------------------------------------------------+
 */

#ifndef CAT_QUEUE_H_
#define CAT_QUEUE_H_

/* copy from libuv queue.h */

#include <stddef.h>

typedef void *cat_queue_t[2];
typedef void *cat_queue_node_t[2];

/* private */

#define cat_queue_next(node)       (*(cat_queue_t **) &((*(node))[0]))
#define cat_queue_prev(node)       (*(cat_queue_t **) &((*(node))[1]))
#define cat_queue_prev_next(node)  (cat_queue_next(cat_queue_prev(node)))
#define cat_queue_next_prev(node)  (cat_queue_prev(cat_queue_next(node)))

/* public */

#define cat_queue_init(queue) do { \
    cat_queue_next(queue) = (queue); \
    cat_queue_prev(queue) = (queue); \
} while(0)

#define cat_queue_data(node, type, field) \
  ((type *) ((char *) (node) - offsetof(type, field)))

#define cat_queue_empty(queue) \
  ((const cat_queue_t *) (queue) == (const cat_queue_t *) cat_queue_next(queue))

#define cat_queue_front(queue) \
    ((!cat_queue_empty(queue)) ? cat_queue_next(queue) : NULL)

#define cat_queue_back(queue) \
    ((!cat_queue_empty(queue)) ? cat_queue_prev(queue) : NULL)

#define cat_queue_front_data(queue, type, node) \
    ((!cat_queue_empty(queue)) ? cat_queue_data(cat_queue_next(queue), type, node) : NULL)

#define cat_queue_back_data(queue, type, node) \
    ((!cat_queue_empty(queue)) ? cat_queue_data(cat_queue_prev(queue), type, node) : NULL)

#define cat_queue_push_front(queue, node) do { \
    cat_queue_next(node) = cat_queue_next(queue); \
    cat_queue_prev(node) = (queue); \
    cat_queue_next_prev(node) = (node); \
    cat_queue_next(queue) = (node); \
} while(0)

#define cat_queue_push_back(queue, node) do { \
    cat_queue_next(node) = (queue); \
    cat_queue_prev(node) = cat_queue_prev(queue); \
    cat_queue_prev_next(node) = (node); \
    cat_queue_prev(queue) = (node); \
} while (0)

#define cat_queue_pop_front(queue) cat_queue_remove(cat_queue_next(queue))

#define cat_queue_pop_back(queue)  cat_queue_remove(cat_queue_prev(queue))

#define cat_queue_remove(node) do { \
    cat_queue_prev_next(node) = cat_queue_next(node); \
    cat_queue_next_prev(node) = cat_queue_prev(node); \
} while (0)

#define CAT_QUEUE_FOREACH(queue, node) do { \
  cat_queue_t *_queue = queue, *node; \
  for ((node) = cat_queue_next(_queue); (node) != (_queue); (node) = cat_queue_next(node))

#define CAT_QUEUE_FOREACH_END() \
} while (0)

#define CAT_QUEUE_FOREACH_DATA_START(queue, type, field, name) do { \
    type *name; \
    CAT_QUEUE_FOREACH(queue, _node) { \
        name = cat_queue_data(_node, type, field); \

#define CAT_QUEUE_FOREACH_DATA_END() \
    } CAT_QUEUE_FOREACH_END(); \
} while (0)

#endif /* CAT_QUEUE_H_ */
