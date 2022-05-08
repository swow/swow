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

#include "swow_known_strings.h"
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

#define SWOW_CLOSURE_KNOWN_STRING_MAP(XX) \
    XX(doc_comment) \
    XX(static_variables) \

SWOW_CLOSURE_KNOWN_STRING_MAP(SWOW_KNOWN_STRING_STORAGE_GEN)

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

SWOW_API SWOW_MAY_THROW HashTable *swow_serialize_user_anonymous_function(zend_function *function)
{
    zend_string *filename = function->op_array.filename;
    uint32_t line_start = function->op_array.line_start;
    uint32_t line_end = function->op_array.line_end;
    zend_string *doc_comment = function->op_array.doc_comment;
    zval z_static_variables, z_references, z_tmp;
    HashTable *ht = NULL;

    if (!swow_function_is_user_anonymous(function)) {
        zend_value_error( "Closure is not a user anonymous function");
        return NULL;
    }
    if (filename == NULL) {
        zend_throw_error(NULL, "Closure with no filename cannot be serialized");
        return NULL;
    }

    ZVAL_NULL(&z_static_variables);
    ZVAL_NULL(&z_references);
    if (function->op_array.static_variables != NULL) {
        array_init(&z_static_variables);
        HashTable *ht = ZEND_MAP_PTR_GET(function->op_array.static_variables_ptr);
        if (!ht) {
            ht = zend_array_dup(function->op_array.static_variables);
            ZEND_MAP_PTR_SET(function->op_array.static_variables_ptr, ht);
        }
        zend_string *key;
        zval *z_val;
        ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(ht, key, z_val) {
            if (Z_ISREF_P(z_val)) {
                if (ZVAL_IS_NULL(&z_references)) {
                    array_init(&z_references);
                }
                CAT_LOG_DEBUG_WITH_LEVEL(PHP, 6, "Use reference $%.*s", (int) ZSTR_LEN(key), ZSTR_VAL(key));
                add_next_index_str(&z_references, zend_string_copy(key));
                continue;
            }
            if (UNEXPECTED(zval_update_constant_ex(z_val, function->common.scope) != SUCCESS)) {
                zend_throw_error(NULL, "Failed to solve static variable %.*s", (int) ZSTR_LEN(key), ZSTR_VAL(key));
                goto _serialize_use_error;
            }
            /* if (!Z_ISREF_P(z_val)) */ {
                CAT_LOG_DEBUG_WITH_LEVEL(PHP, 6, "Use variable $%.*s", (int) ZSTR_LEN(key), ZSTR_VAL(key));
                Z_TRY_ADDREF_P(z_val);
                zend_hash_update(Z_ARRVAL(z_static_variables), key, z_val);
            }
        } ZEND_HASH_FOREACH_END();
    }

    CAT_LOG_DEBUG_V3(PHP, "Closure { filename=%s, line_start=%u, line_end=%u }",
        ZSTR_VAL(filename), line_start, line_end);

    zend_string *contents = swow_file_get_contents(filename);

    if (contents == NULL) {
        return NULL;
    }

    php_token_list_t *token_list = php_tokenize(contents);
    enum parser_state_e {
        CLOSURE_PARSER_STATE_FIND_OPEN_TAG,
        CLOSURE_PARSER_STATE_PARSING,
        CLOSURE_PARSER_STATE_NAMESPACE,
        CLOSURE_PARSER_STATE_USE,
        CLOSURE_PARSER_STATE_FUNCTION_START,
        CLOSURE_PARSER_STATE_FUNCTION_FIND_CLOSE_BRACE,
        CLOSURE_PARSER_STATE_FN_FIND_SEMICOLON,
        CLOSURE_PARSER_STATE_END,
    };
    enum parser_state_e parser_state = CLOSURE_PARSER_STATE_FIND_OPEN_TAG;
    enum parser_state_e original_parser_state = CLOSURE_PARSER_STATE_PARSING;
    uint32_t brace_level = 0;
    bool captured = false;
    bool is_arrow_function = false;
    bool use_extra_function_wrapper = false;

    smart_str buffer = {0};

    CAT_QUEUE_FOREACH_DATA_START(&token_list->queue, php_token_t, node, token) {
        bool previous_was_captured = captured;
        captured = false;
        if (!previous_was_captured && cat_queue_prev(&token->node) != &token_list->queue) {
            php_token_t *prev_token = cat_queue_data(cat_queue_prev(&token->node), php_token_t, node);
            if (token->line > prev_token->line) {
                uint32_t line_diff = token->line - prev_token->line;
                CAT_LOG_DEBUG_WITH_LEVEL(PHP, 6, "Insert %u new lines", line_diff);
                for (uint32_t i = 0; i < line_diff; i++) {
                    smart_str_appendc(&buffer, '\n');
                }
            }
        }
        CAT_LOG_DEBUG_V3(PHP, "token { type=%s, text='%.*s', line=%u, offset=%u }",
            php_token_get_name(token), (int) token->text.length, token->text.data, token->line, token->offset);
        if (parser_state == CLOSURE_PARSER_STATE_FIND_OPEN_TAG) {
            if (token->type == T_OPEN_TAG) {
                parser_state = original_parser_state;
            }
            goto _next;
        }
        if (token->type == T_CLOSE_TAG) {
            original_parser_state = parser_state;
            parser_state = CLOSURE_PARSER_STATE_FIND_OPEN_TAG;
            goto _next;
        }
        switch (parser_state) {
            case CLOSURE_PARSER_STATE_NAMESPACE:
            case CLOSURE_PARSER_STATE_USE: {
                if (token->type == ';') {
                    parser_state = CLOSURE_PARSER_STATE_PARSING;
                }
                goto _capture;
            }
            case CLOSURE_PARSER_STATE_FUNCTION_START: {
                if (token->type == '{') {
                    ZEND_ASSERT(brace_level == 0);
                    brace_level = 1;
                    parser_state = CLOSURE_PARSER_STATE_FUNCTION_FIND_CLOSE_BRACE;
                }
                goto _capture;
            }
            case CLOSURE_PARSER_STATE_FUNCTION_FIND_CLOSE_BRACE: {
                if (token->type == '{') {
                    brace_level++;
                } else if (token->type == '}') {
                    if (brace_level-- == 1) {
                        parser_state = CLOSURE_PARSER_STATE_END;
                    }
                }
                goto _capture;
            }
            case CLOSURE_PARSER_STATE_FN_FIND_SEMICOLON: {
                if (token->type == ';') {
                    parser_state = CLOSURE_PARSER_STATE_END;
                    goto _end;
                }
                goto _capture;
            }
            default:
                break;
        }
        switch (token->type) {
            case T_NAMESPACE: {
                parser_state = CLOSURE_PARSER_STATE_NAMESPACE;
                goto _capture;
            }
            case T_USE: {
                parser_state = CLOSURE_PARSER_STATE_USE;
                goto _capture;
            }
            case T_FUNCTION:
            case T_FN: {
                if (token->line != line_start) {
                    break;
                }
                if (token->type == T_FN) {
                    is_arrow_function = true;
                }
                if (!is_arrow_function) {
                    if (!ZVAL_IS_NULL(&z_static_variables) || !ZVAL_IS_NULL(&z_references)) {
                        use_extra_function_wrapper = true;
                    }
                } else {
                    if (!ZVAL_IS_NULL(&z_static_variables)) {
                        use_extra_function_wrapper = true;
                    }
                }
                /* Solve function use (start) */
                if (use_extra_function_wrapper) {
                    zend_string *key;
                    zval *z_val;
                    smart_str_appends(&buffer, "return (static function () ");
                    if (!ZVAL_IS_NULL(&z_references)) {
                        bool first = true;
                        smart_str_appends(&buffer, "use (");
                        ZEND_HASH_PACKED_FOREACH_VAL(Z_ARRVAL(z_references), z_val) {
                            CAT_LOG_DEBUG_WITH_LEVEL(PHP, 6, "Use reference for $%.*s", (int) Z_STRLEN_P(z_val), Z_STRVAL_P(z_val));
                            if (first) {
                                first = false;
                            } else {
                                smart_str_appends(&buffer, ", ");
                            }
                            smart_str_appends(&buffer, "&$");
                            smart_str_append(&buffer, Z_STR_P(z_val));
                        } ZEND_HASH_FOREACH_END();
                        smart_str_appends(&buffer, ") ");
                    }
                    smart_str_appends(&buffer, "{ ");
                    if (!ZVAL_IS_NULL(&z_static_variables)) {
                        ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(Z_ARRVAL(z_static_variables), key, z_val) {
                            smart_str_appendc(&buffer, '$');
                            smart_str_append(&buffer, key);
                            smart_str_appends(&buffer, " = NULL; ");
                        } ZEND_HASH_FOREACH_END();
                    }
                }
                smart_str_appends(&buffer, "return ");
                if (!use_extra_function_wrapper && function->common.scope != NULL) {
                    smart_str_appendc(&buffer, '(');
                }
                if (function->common.fn_flags & ZEND_ACC_STATIC) {
                    smart_str_appends(&buffer, "static ");
                }
                if (!is_arrow_function) {
                    parser_state = CLOSURE_PARSER_STATE_FUNCTION_START;
                } else {
                    parser_state = CLOSURE_PARSER_STATE_FN_FIND_SEMICOLON;
                }
                goto _capture;
            }
        }
        if (0) {
            _capture:
            smart_str_appendl(&buffer, token->text.data, token->text.length);
            captured = true;
        }
        if (parser_state == CLOSURE_PARSER_STATE_END) {
            _end:
            /* Solve function use (end) */
            if (!use_extra_function_wrapper) {
                if (function->common.scope != NULL) {
                    smart_str_appendc(&buffer, ')');
                }
            } else {
                smart_str_appends(&buffer, "; })()");
            }
            /* Solve scope */
            if (function->common.scope != NULL) {
                smart_str_appends(&buffer, "->bindTo(null, \\");
                smart_str_append(&buffer, function->common.scope->name);
                smart_str_appends(&buffer, "::class)");
            }
            smart_str_appendc(&buffer, ';');
            break;
        }
        _next:
        continue;
    } CAT_QUEUE_FOREACH_DATA_END();
    smart_str_0(&buffer);
    if (parser_state != CLOSURE_PARSER_STATE_END) {
        zend_throw_error(NULL, "Failure to parse tokens");
        goto _token_parse_error;
    }


    ht = zend_new_array(3);
    ZVAL_STR_COPY(&z_tmp, filename);
    zend_hash_update(ht, ZSTR_KNOWN(ZEND_STR_FILE), &z_tmp);
    ZVAL_STR_COPY(&z_tmp, buffer.s);
    zend_hash_update(ht, ZSTR_KNOWN(ZEND_STR_CODE), &z_tmp);
    if (doc_comment != NULL) {
        ZVAL_STR(&z_tmp, zend_string_copy(doc_comment));
        zend_hash_update(ht, SWOW_KNOWN_STRING(doc_comment), &z_tmp);
    }
    if (!ZVAL_IS_NULL(&z_static_variables)) {
        Z_TRY_ADDREF(z_static_variables);
        zend_hash_update(ht, SWOW_KNOWN_STRING(static_variables), &z_static_variables);
    }

    CAT_LOG_DEBUG_SCOPE_START_WITH_LEVEL(PHP, 5) {
        zend_string *output;
        ZVAL_ARR(&z_tmp, ht);
        SWOW_OB_START(output) {
            php_var_dump(&z_tmp, 0);
        } SWOW_OB_END();
        CAT_LOG_DEBUG_D(PHP, "Closure hash: <<<DUMP\n%.*s}}}\nDUMP;",
            (int) ZSTR_LEN(output), ZSTR_VAL(output));
        zend_string_release(output);
    } CAT_LOG_DEBUG_SCOPE_END();

    if (0) {
        _token_parse_error:;
        CAT_LOG_DEBUG_WITH_LEVEL(PHP, 5, "Closure serialization code: <<<DUMP\n%.*s}}}\nDUMP;",
            (int) ZSTR_LEN(buffer.s), ZSTR_VAL(buffer.s));
    }
    zend_string_release_ex(buffer.s, false);
    php_token_list_free(token_list);
    zend_string_release_ex(contents, false);
    _serialize_use_error:
    zval_ptr_dtor(&z_references);
    zval_ptr_dtor(&z_static_variables);

    return ht;
}

SWOW_API SWOW_MAY_THROW HashTable *swow_serialize_named_function(zend_function *function)
{
    zend_string *function_name = function->op_array.function_name;
    zend_class_entry *scope = function->common.scope;

    if (!swow_function_is_named(function)) {
        zend_value_error("Closure is not a named function");
        return NULL;
    }

    smart_str buffer = {0};
    smart_str_appends(&buffer, "return Closure::fromCallable(");
    if (scope != NULL) {
        smart_str_appendc(&buffer, '[');
        smart_str_append(&buffer, scope->name);
        smart_str_appends(&buffer, "::class, '");
        smart_str_append(&buffer, function_name);
        smart_str_appends(&buffer, "']");
    } else {
        smart_str_appendc(&buffer, '\'');
        smart_str_append(&buffer, function_name);
        smart_str_appendc(&buffer, '\'');
    }
    smart_str_appends(&buffer, ");");
    smart_str_0(&buffer);

    HashTable *ht = zend_new_array(1);
    zval z_tmp;
    ZVAL_STR(&z_tmp, buffer.s);
    zend_hash_update(ht, ZSTR_KNOWN(ZEND_STR_CODE), &z_tmp);

    return ht;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Closure___serialize, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Closure, __serialize)
{
    ZEND_PARSE_PARAMETERS_NONE();

    swow_closure_t *closure = swow_closure_get_from_object(Z_OBJ_P(ZEND_THIS));
    zend_function *function = &closure->func;
    bool is_user_anonymous_function = swow_function_is_user_anonymous(function);
    bool is_named_function = !is_user_anonymous_function && swow_function_is_named(function);

    if (!is_user_anonymous_function && !is_named_function) {
        zend_throw_error(NULL, "Closure which is not user-defined anonymous function and has no name cannot be serialized");
        RETURN_THROWS();
    }
    do {
        zval* closure_this = zend_get_closure_this_ptr(ZEND_THIS);
        if (!Z_ISUNDEF_P(closure_this)) {
            zend_throw_error(NULL, "Closure with bound '$this' cannot be serialized, it is recommended to add the 'static' modifier");
            RETURN_THROWS();
        }
    } while (0);

    HashTable *data;
    if (is_user_anonymous_function) {
        data = swow_serialize_user_anonymous_function(function);
    } else /* if (is_named_function) */ {
        data = swow_serialize_named_function(function);
    }
    if (data == NULL) {
        RETURN_THROWS();
    }
    RETURN_ARR(data);
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
    if (z_file != NULL && Z_TYPE_P(z_file) != IS_STRING) {
        zend_value_error("Expected string for key 'file', got %s", zend_zval_type_name(z_file));
        RETURN_THROWS();
    }
    zval *z_code = zend_hash_find_known_hash(data, ZSTR_KNOWN(ZEND_STR_CODE));
    if (z_code == NULL || Z_TYPE_P(z_code) != IS_STRING) {
        zend_value_error("Expected string for key 'code', got %s", z_code == NULL ? "null" : zend_zval_type_name(z_code));
        RETURN_THROWS();
    }
    zval *z_doc_comment = zend_hash_find(data, SWOW_KNOWN_STRING(doc_comment));
    if (z_doc_comment != NULL && Z_TYPE_P(z_doc_comment) != IS_STRING) {
        zend_value_error("Expected string for key 'doc_comment', got %s", zend_zval_type_name(z_code));
        RETURN_THROWS();
    }
    zval *z_static_variables = zend_hash_find(data, SWOW_KNOWN_STRING(static_variables));
    if (z_static_variables != NULL && (Z_TYPE_P(z_static_variables) != IS_ARRAY || HT_IS_PACKED(Z_ARR_P(z_static_variables)))) {
        zend_value_error("Expected assoc array for key 'static_variables', got %s", zend_zval_type_name(z_code));
        RETURN_THROWS();
    }
    zval retval;
    zend_string *file = z_file != NULL ? Z_STR_P(z_file) : NULL;
    zend_string *code = Z_STR_P(z_code);
    zend_string *doc_comment = z_doc_comment != NULL ? Z_STR_P(z_doc_comment) : NULL;
    HashTable *static_variables = z_static_variables != NULL ? Z_ARR_P(z_static_variables) : NULL;

    /* compile code */
    zend_op_array *op_array = swow_compile_string(code, file != NULL ? ZSTR_VAL(file) : "Closure::__unserialize()");
    if (op_array == NULL) {
        zend_throw_error(NULL, "Closure compilation failed");
        RETURN_THROWS();
    }

    /* execute code to generate a closure instance */
    zend_execute(op_array, &retval);
    zend_exception_restore();
#if PHP_VERSION_ID >= 80100
    zend_destroy_static_vars(op_array);
#endif
    destroy_op_array(op_array);
    efree_size(op_array, sizeof(zend_op_array));

    /* check whether return value is valid */
    if (Z_TYPE(retval) != IS_OBJECT || !instanceof_function(Z_OBJCE(retval), zend_ce_closure)) {
        zend_throw_error(NULL, "Expected Closure instance, got %s", zend_zval_type_name(&retval));
        zval_ptr_dtor(&retval);
        RETURN_THROWS();
    }

    swow_closure_t *this_closure = swow_closure_get_from_object(Z_OBJ_P(ZEND_THIS));
    swow_closure_t *closure = swow_closure_get_from_object(Z_OBJ(retval));
    swow_closure_construct_from_another_closure(this_closure, closure);
    zval_ptr_dtor(&retval);

    if (doc_comment != NULL) {
        this_closure->func.op_array.doc_comment = zend_string_copy(doc_comment);
    }
    if (static_variables != NULL) {
        HashTable *this_closure_static_variables = ZEND_MAP_PTR_GET(this_closure->func.op_array.static_variables_ptr);
        if (this_closure_static_variables == NULL) {
            zend_throw_error(NULL, "Closure should have static variables");
            RETURN_THROWS();
        }
        zend_string *key;
        zval *z_val;
        ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(static_variables, key, z_val) {
            Z_TRY_ADDREF_P(z_val);
            zend_hash_update(this_closure_static_variables, key, z_val);
        } ZEND_HASH_FOREACH_END();
    }
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

    SWOW_CLOSURE_KNOWN_STRING_MAP(SWOW_KNOWN_STRING_INIT_GEN);

    original_closure_create_object = zend_ce_closure->create_object;
#ifdef ZEND_ACC_NOT_SERIALIZABLE /* >= PHP_VERSION_ID >= 80100 */
    zend_ce_closure->ce_flags ^= ZEND_ACC_NOT_SERIALIZABLE;
#else
    zend_ce_closure->serialize = NULL;
    zend_ce_closure->unserialize = NULL;
#endif
    zend_register_functions(zend_ce_closure, swow_closure_methods, &zend_ce_closure->function_table, EG(current_module)->type);

    return SUCCESS;
}
