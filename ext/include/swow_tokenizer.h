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

#ifndef SWOW_TOKENIZER_H
#define SWOW_TOKENIZER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "swow.h"

#include "cat_queue.h"

// TODO: add swow_ prefix

typedef struct php_token_s {
    cat_queue_t node;
    int type;
    cat_const_string_t text;
    uint32_t line;
    uint32_t offset;
} php_token_t;

typedef struct php_token_list_s {
    cat_queue_t queue;
    size_t count;
    zend_string *source;
} php_token_list_t;

#if 0

typedef int (*ast_walk_callback)(zend_ast *node, zend_ast *parent, int children, zend_ast **child, void *context);

/*
 * walk ast, enter will be invoked at entering this node, leave wille be invoked when leaving
 */
SWOW_API int swow_walk_ast(zend_ast *node, zend_ast *parent, ast_walk_callback enter, ast_walk_callback leave, void *context);

typedef struct _ast_walk_callback_with_context {
    ast_walk_callback callback;
    void *context;
} ast_walk_callback_with_context;

typedef struct _ast_walk_callbacks_aggregate {
    size_t len;
    ast_walk_callback_with_context callbacks[1];
} ast_walk_callbacks_aggregate;

/*
 * aggregates callbacks
 *
 * usage:
 *      ast_walk_callbacks_aggregate *callbacks = calloc(8, 1 + 2 * 2);
 *      callbacks->len = 2;
 *      callbacks->callbacks[0].callback = enter_node_1;
 *      callbacks->callbacks[0].context = (void *)contenxt1;
 *      callbacks->callbacks[1].callback = enter_node_2;
 *      callbacks->callbacks[1].context = (void *)contenxt2;
 *      swow_walk_ast(node, NULL, (ast_walk_callback)ast_walk_callbacks, NULL, callbacks);
 *      or use as leaving node callbacks
 *      swow_walk_ast(node, NULL, NULL, (ast_walk_callback)ast_walk_callbacks, callbacks);
 */
SWOW_API int ast_walk_callbacks(zend_ast *node, zend_ast *parent, int children, zend_ast **child, ast_walk_callbacks_aggregate *callbacks);

/*
 * ast kind int to name
 */
SWOW_API const char *ast_kind_name(int kind);

#endif

SWOW_API int swow_ast_children(zend_ast *node, zend_ast ***child);

SWOW_API php_token_list_t *php_tokenize(zend_string *source, int (*ast_callback)(zend_ast *, void *), void *ast_callback_context);

SWOW_API php_token_list_t *php_token_list_alloc(void);
SWOW_API void php_token_list_add(php_token_list_t *token_list, int token_type, const char *text, size_t text_length, int line);
SWOW_API void php_token_list_clear(php_token_list_t *token_list);
SWOW_API void php_token_list_free(php_token_list_t *token_list);

SWOW_API const char *php_token_get_name(const php_token_t *token);
SWOW_API const char *php_token_get_name_from_type(int type);

#ifdef __cplusplus
}
#endif
#endif /* SWOW_TOKENIZER_H */
