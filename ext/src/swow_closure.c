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

#include "swow_closure.h"

#include "swow_tokenizer.h"

#include <zend_language_parser.h>

SWOW_API CAT_GLOBALS_DECLARE(swow_closure)

CAT_GLOBALS_CTOR_DECLARE_SZ(swow_closure)

typedef struct swow_closure_s {
    zend_object       std;
    zend_function     func;
    zval              this_ptr;
    zend_class_entry *called_scope;
    zif_handler       orig_internal_handler;
} swow_closure_t;

static swow_object_create_t original_closure_create_object = NULL;

static zend_always_inline swow_closure_t *swow_closure_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_closure_t, std);
}

static zend_object *swow_closure_create_object_for_unserialization(zend_class_entry *class_type) /* {{{ */
{
    zend_object *object = SWOW_CLOSURE_G(current_object);
    /* recover it immediately here,
     * if there are closure objects created in zend_create_closure(),
     * this will avoid the problems  */
    SWOW_CLOSURE_G(current_object) = NULL;
    zend_ce_closure->create_object = original_closure_create_object;
    return object;
}

static void swow_closure_construct_from_another_closure(swow_closure_t *this_closure, swow_closure_t *closure)
{
    zval result;
    SWOW_CLOSURE_G(current_object) = &this_closure->std;
    zend_ce_closure->create_object = swow_closure_create_object_for_unserialization;
	zend_create_closure(&result, &closure->func,
		closure->func.common.scope, closure->called_scope, &closure->this_ptr);
    ZEND_ASSERT(swow_closure_get_from_object(Z_OBJ(result)) == this_closure);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Closure___serialize, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Closure, __serialize)
{
    ZEND_PARSE_PARAMETERS_NONE();

    swow_closure_t *closure = swow_closure_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_function *function = &closure->func;

    do {
        zval* closure_this = zend_get_closure_this_ptr(ZEND_THIS);
        if (!Z_ISUNDEF_P(closure_this)) {
            zend_throw_error(NULL, "Closure with bound '$this' cannot be serialized, it is recommended to add the 'static' modifier");
            RETURN_THROWS();
        }
    } while (0);

    zend_string *filename = function->op_array.filename;
    uint32_t line_start = function->op_array.line_start;
    uint32_t line_end = function->op_array.line_end;
    zend_string *doc_comment = function->op_array.doc_comment;
    zval z_static_variables, z_references;

    ZVAL_EMPTY_ARRAY(&z_static_variables);
    ZVAL_EMPTY_ARRAY(&z_references);
    if (function->op_array.static_variables != NULL) {
        array_init(&z_static_variables);
        HashTable *ht = ZEND_MAP_PTR_GET(function->op_array.static_variables_ptr);
        if (!ht) {
            ht = zend_array_dup(function->op_array.static_variables);
            ZEND_MAP_PTR_SET(function->op_array.static_variables_ptr, ht);
        }
        zend_string *key;
        zval *z_val, z_tmp;
        ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(ht, key, z_val) {
            if (Z_ISREF_P(z_val)) {
                if (Z_ARR(z_references) == &zend_empty_array) {
                    array_init(&z_references);
                }
                add_next_index_str(&z_references, zend_string_copy(key));
                continue;
            }
            if (UNEXPECTED(zval_update_constant_ex(z_val, function->common.scope) != SUCCESS)) {
                goto _serialize_use_error;
            }
            /* if (!Z_ISREF_P(z_val)) */ {
                php_serialize_data_t var_hash;
                smart_str serialized_data = {0};
                PHP_VAR_SERIALIZE_INIT(var_hash);
                php_var_serialize(&serialized_data, z_val, &var_hash);
                PHP_VAR_SERIALIZE_DESTROY(var_hash);
                if (EG(exception)) {
                    smart_str_free(&serialized_data);
                    goto _serialize_use_error;
                }
                ZEND_ASSERT(serialized_data.s && "Unexpected empty serialized value");
                ZVAL_STR(&z_tmp, serialized_data.s);
                zend_hash_update(Z_ARRVAL(z_static_variables), key, &z_tmp);
            }
        } ZEND_HASH_FOREACH_END();
    }

    CAT_LOG_DEBUG_V3(PHP, "Closure { filename=%s, line_start=%u, line_end=%u }",
        ZSTR_VAL(filename), line_start, line_end);

    /* file operations can generate E_WARNINGs which we want to promote to exceptions */
    zend_error_handling error_handling;
    zend_replace_error_handling(EH_THROW, spl_ce_RuntimeException, &error_handling);

    php_stream *stream = php_stream_open_wrapper_ex(ZSTR_VAL(filename), "rb", REPORT_ERRORS, NULL, NULL);
    if (stream == NULL) {
        goto _fs_open_error;
    }

    zend_string *contents = php_stream_copy_to_mem(stream, -1, 0);
    if (contents == NULL) {
        goto _fs_read_error;
    }

    php_token_list_t *token_list = php_tokenize(contents);
    enum {
        CLOSURE_PARSER_STATE_PARSING,
        CLOSURE_PARSER_STATE_NAMESPACE,
        CLOSURE_PARSER_STATE_USE,
        CLOSURE_PARSER_STATE_FUNCTION_START,
        CLOSURE_PARSER_STATE_FUNCTION_FIND_CLOSE_BRACE,
        CLOSURE_PARSER_STATE_FUNCTION_END,
        CLOSURE_PARSER_STATE_FN_START,
    } parser_state = CLOSURE_PARSER_STATE_PARSING;
    uint32_t current_line = 1;
    uint32_t brace_level = 0;
    bool previous_is_inline_html = false;

    smart_str buffer = {0};

    CAT_QUEUE_FOREACH_DATA_START(&token_list->queue, php_token_t, node, token) {
        CAT_LOG_DEBUG_V3(PHP, "token { type=%s, text='%.*s', line=%u, offset=%u }",
            php_token_get_name(token->type), (int) token->text.length, token->text.data, token->line, token->offset);
        if (token->line > current_line && token->type == T_WHITESPACE && token->text.data[0] == '\n') {
            current_line++;
            goto _append;
        }
        if (previous_is_inline_html) {
            smart_str_appends(&buffer, "?>");
            previous_is_inline_html = false;
        }
        if (token->type == T_INLINE_HTML) {
            smart_str_appends(&buffer, "<?php");
            previous_is_inline_html = true;
        }
        switch (parser_state) {
            case CLOSURE_PARSER_STATE_NAMESPACE:
            case CLOSURE_PARSER_STATE_USE: {
                if (token->type == ';') {
                    parser_state = CLOSURE_PARSER_STATE_PARSING;
                }
                goto _append;
            }
            case CLOSURE_PARSER_STATE_FUNCTION_START: {
                if (token->type == '{') {
                    brace_level++;
                    parser_state = CLOSURE_PARSER_STATE_FUNCTION_FIND_CLOSE_BRACE;
                }
                goto _append;
            }
            case CLOSURE_PARSER_STATE_FUNCTION_END: {
                smart_str_appends(&buffer, "; })()");
                if (function->common.scope != NULL) {
                    smart_str_appends(&buffer, "->bindTo(null, \\");
                    smart_str_append(&buffer, function->common.scope->name);
                    smart_str_appends(&buffer, "::class)");
                }
                smart_str_appendc(&buffer, ';');
                smart_str_0(&buffer);
                goto _resolve_token_done;
            }
            case CLOSURE_PARSER_STATE_FUNCTION_FIND_CLOSE_BRACE: {
                if (token->type == '{') {
                    brace_level++;
                } else if (token->type == '}') {
                    if (brace_level-- == 1) {
                        parser_state = CLOSURE_PARSER_STATE_FUNCTION_END;
                    }
                }
                goto _append;
            }
            default:
                break;
        }
        switch (token->type) {
            case T_OPEN_TAG: {
                goto _append;
            }
            case T_NAMESPACE: {
                parser_state = CLOSURE_PARSER_STATE_NAMESPACE;
                goto _append;
            }
            case T_USE: {
                parser_state = CLOSURE_PARSER_STATE_USE;
                goto _append;
            }
            case T_FUNCTION:
            case T_FN: {
                if (token->line != line_start) {
                    break;
                }
                zend_string *key;
                zval *z_val;
                bool use_references = false;
                smart_str_appends(&buffer, "return (function () ");
                ZEND_HASH_PACKED_FOREACH_VAL(Z_ARRVAL(z_references), z_val) {
                    CAT_LOG_DEBUG_WITH_LEVEL(PHP, 5, "Use reference for $%.*s", (int) Z_STRLEN_P(z_val), Z_STRVAL_P(z_val));
                    if (!use_references) {
                        smart_str_appends(&buffer, "use (");
                        use_references = true;
                    } else {
                        smart_str_appends(&buffer, ", ");
                    }
                    smart_str_appends(&buffer, "&$");
                    smart_str_append(&buffer, Z_STR_P(z_val));
                } ZEND_HASH_FOREACH_END();
                if (use_references) {
                    smart_str_appends(&buffer, ") ");
                }
                smart_str_appends(&buffer, "{ ");
                ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(Z_ARRVAL(z_static_variables), key, z_val) {
                    smart_str_appendc(&buffer, '$');
                    smart_str_append(&buffer, key);
                    smart_str_appends(&buffer, " = unserialize('");
                    smart_str_append(&buffer, Z_STR_P(z_val));
                    smart_str_appends(&buffer, "'); ");
                } ZEND_HASH_FOREACH_END();
                smart_str_appends(&buffer, "return ");
                if (function->common.fn_flags & ZEND_ACC_STATIC) {
                    smart_str_appends(&buffer, "static ");
                }
                if (token->type == T_FUNCTION) {
                    parser_state = CLOSURE_PARSER_STATE_FUNCTION_START;
                } else /* if (token->type == T_FN) */ {
                    parser_state = CLOSURE_PARSER_STATE_FN_START;
                }
                goto _append;
            }
        }
        if (0) {
            _append:
            smart_str_appendl(&buffer, token->text.data, token->text.length);
        }
    } CAT_QUEUE_FOREACH_DATA_END();
    _resolve_token_done:

    CAT_LOG_DEBUG_WITH_LEVEL(PHP, 5, "Serialized-Content (%zu): <<<PHP\n\"%.*s\"\nPHP;",
        ZSTR_LEN(buffer.s), (int) ZSTR_LEN(buffer.s), ZSTR_VAL(buffer.s));

    array_init(return_value);
    zval ztmp;
    ZVAL_STR_COPY(&ztmp, filename);
    zend_hash_update(Z_ARRVAL_P(return_value), ZSTR_KNOWN(ZEND_STR_FILE), &ztmp);
    ZVAL_STR(&ztmp, buffer.s);
    zend_hash_update(Z_ARRVAL_P(return_value), ZSTR_KNOWN(ZEND_STR_CODE), &ztmp);

    zval_ptr_dtor(&z_references);
    zval_ptr_dtor(&z_static_variables);
    // smart_str_free(&buffer);
    php_token_list_free(token_list);
    zend_string_release(contents);
    php_stream_close(stream);
    zend_restore_error_handling(&error_handling);

    return;
    _fs_read_error:
    php_stream_close(stream);
    _fs_open_error:
    _serialize_use_error:
    zval_ptr_dtor(&z_static_variables);
    zend_restore_error_handling(&error_handling);
    
    RETURN_THROWS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Closure___unserialize, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Closure, __unserialize)
{
    HashTable *data;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    zval *z_file = zend_hash_find_known_hash(data, ZSTR_KNOWN(ZEND_STR_FILE));
    if (Z_TYPE_P(z_file) != IS_STRING) {
        zend_value_error("Expected string for key 'file', got %s", zend_zval_type_name(z_file));
        RETURN_THROWS();
    }
    zval *z_code = zend_hash_find_known_hash(data, ZSTR_KNOWN(ZEND_STR_CODE));
    if (Z_TYPE_P(z_file) != IS_STRING) {
        zend_value_error("Expected string for key 'code', got %s", zend_zval_type_name(z_code));
        RETURN_THROWS();
    }
    zval retval;
    zend_string *file = Z_STR_P(z_file);
    zend_string *code = Z_STR_P(z_code);
    zend_op_array *op_array = zend_compile_string(code, ZSTR_VAL(file), ZEND_COMPILE_POSITION_AT_SHEBANG);
    if (op_array) {
        zend_execute(op_array, &retval);
        zend_exception_restore();
        zend_destroy_static_vars(op_array);
        destroy_op_array(op_array);
        efree_size(op_array, sizeof(zend_op_array));
    } else {
        zend_throw_error(NULL, "Failed to compile code");
    }

    ZEND_ASSERT(Z_TYPE(retval) == IS_OBJECT && instanceof_function(Z_OBJCE(retval), zend_ce_closure));
    swow_closure_t *this_closure = swow_closure_get_from_object(Z_OBJ_P(ZEND_THIS)), *closure = swow_closure_get_from_object(Z_OBJ(retval));
    swow_closure_construct_from_another_closure(this_closure, closure);
    zval_ptr_dtor(&retval);
}

static const zend_function_entry swow_closure_methods[] = {
    /* magic */
    PHP_ME(Swow_Closure, __serialize,   arginfo_class_Swow_Closure___serialize,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Closure, __unserialize, arginfo_class_Swow_Closure___unserialize, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_result swow_closure_module_init(INIT_FUNC_ARGS)
{
    CAT_GLOBALS_REGISTER(swow_closure, CAT_GLOBALS_CTOR(swow_closure), NULL);

    original_closure_create_object = zend_ce_closure->create_object;
#ifdef ZEND_ACC_NOT_SERIALIZABLE /* >= PHP_VERSION_ID >= 80100 */
    zend_ce_closure->ce_flags ^= ZEND_ACC_NOT_SERIALIZABLE;
#else
    ce->serialize = NULL;
    ce->unserialize = NULL;
#endif
    zend_register_functions(zend_ce_closure, swow_closure_methods, &zend_ce_closure->function_table, EG(current_module)->type);

    return SUCCESS;
}
