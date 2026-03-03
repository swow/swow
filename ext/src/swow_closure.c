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
  |         dixyes <dixyes@gmail.com>                                        |
  +--------------------------------------------------------------------------+
 */

#include "swow_closure.h"

#include "swow_known_strings.h"
#include "swow_tokenizer.h"

#include <zend_language_parser.h>

#define smart_str_appendcstr(pstr, str) smart_str_appendl((pstr), (str), strlen(str))

SWOW_API CAT_GLOBALS_DECLARE(swow_closure);

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

typedef struct swow_closure_walk_context_s {
    uint32_t line_start;
    uint32_t found_function;
    smart_str code_str; /* prepend code string */
    smart_str closure_str; /* closure code string */
    bool in_namespace_brace; /* in namespace ... { */
} swow_closure_walk_context_t;

static swow_php_ast_walker_op swow_closure_walker(zend_ast *ast, void *context_ptr)
{
    swow_closure_walk_context_t *context = (swow_closure_walk_context_t *) context_ptr;
    if (ast->kind == ZEND_AST_CLOSURE || ast->kind == ZEND_AST_ARROW_FUNC) {
        if (ast->lineno == context->line_start) {
            if (context->found_function == 0) {
                zend_string *code = zend_ast_export("", ast, "");
                smart_str_setl(&context->closure_str, ZSTR_VAL(code), ZSTR_LEN(code));
                zend_string_release(code);
            }
            context->found_function++;
            return SWOW_PHP_AST_WALKER_SKIP;
        }
        return SWOW_PHP_AST_WALKER_CONTINUE;
    } else if (context->found_function != 0) {
        // already found function, other namespace, use, etc. is ignored
        return SWOW_PHP_AST_WALKER_STOP;
    }

    switch (ast->kind) {
        case ZEND_AST_NAMESPACE:
        {
            zend_ast_zval *namespace_name = (zend_ast_zval *) ast->child[0];
            zend_ast_list *stmts = (zend_ast_list *) ast->child[1];
            zend_string *namespace = NULL;
            if (namespace_name) {
                ZEND_ASSERT(namespace_name->kind == ZEND_AST_ZVAL);
                ZEND_ASSERT(Z_TYPE(namespace_name->val) == IS_STRING);
                namespace = Z_STR(namespace_name->val);
            }

            if (!stmts) {
                // single namespace <T_STRING>; statement
                // see Zend/zend_language_parser.y near L369 top_statement syntax
                ZEND_ASSERT(namespace != NULL);

                smart_str_setl(&context->code_str, CAT_STRL("namespace "));
                smart_str_appendl(&context->code_str, ZSTR_VAL(namespace), ZSTR_LEN(namespace));
                smart_str_appendc(&context->code_str, ';');
                context->in_namespace_brace = false;

                break;
            }

            // namespace <T_STRING> { STMT_LIST }; statement
            // see Zend/zend_language_parser.y near L369 top_statement syntax

            ZEND_ASSERT(stmts->kind == ZEND_AST_STMT_LIST);
            if (!namespace) {
                // at root namespace
                smart_str_setl(&context->code_str, CAT_STRL("namespace {"));
            } else {
                smart_str_setl(&context->code_str, CAT_STRL("namespace "));
                smart_str_appendl(&context->code_str, ZSTR_VAL(namespace), ZSTR_LEN(namespace));
                smart_str_appendcstr(&context->code_str, " {");
            }
            context->in_namespace_brace = true;
            break;
        }
        case ZEND_AST_USE:
        case ZEND_AST_GROUP_USE:
        {
            swow_php_ast_export_kinds_of_use(ast, &context->code_str, true);
            return SWOW_PHP_AST_WALKER_SKIP;
        }
        default:
            // ignore other nodes
            break;
    }
    return SWOW_PHP_AST_WALKER_CONTINUE;
}

static swow_php_ast_walker_op _swow_closure_walk_callback(zend_ast *ast, void *context_ptr)
{
    zend_ast **child;
    uint32_t children;
    uint32_t index;
    swow_php_ast_walker_op ret;

    ret = swow_closure_walker(ast, context_ptr);
    if (ret != SWOW_PHP_AST_WALKER_CONTINUE) {
        return ret;
    }

    children = swow_php_ast_children(ast, &child);
    for (index = 0; index < children; index++) {
        if (child[index] == NULL) {
            continue;
        }
        ret = _swow_closure_walk_callback(child[index], context_ptr);
        if (ret == SWOW_PHP_AST_WALKER_STOP) {
            return ret;
        } else if (ret == SWOW_PHP_AST_WALKER_SKIP) {
            continue;
        } else if (ret != SWOW_PHP_AST_WALKER_CONTINUE) {
            CAT_NEVER_HERE("unknown ast walker op");
        }
    }
    return SWOW_PHP_AST_WALKER_CONTINUE;
}

static void swow_closure_walk_callback(zend_ast *ast, void *context_ptr)
{
    _swow_closure_walk_callback(ast, context_ptr);
}

SWOW_API SWOW_MAY_THROW HashTable *swow_serialize_user_anonymous_function(zend_function *function)
{
    zend_string *filename = function->op_array.filename;
    uint32_t line_start = function->op_array.line_start;
    uint32_t line_end = function->op_array.line_end;
    zend_string *doc_comment = function->op_array.doc_comment;
    zval z_static_variables, z_references, z_tmp;
    HashTable *ht = NULL;
    zend_string *contents = NULL;
    swow_closure_walk_context_t context = {
        /* .line_start = */ 0,
        /* .found_function = */ 0,
        /* .code_str = */ { 0 },
        /* .closure_str = */ { 0 },
        /* .in_namespace_brace = */ false,
    };
    swow_php_token_list_t *token_list = NULL;

    if (!swow_function_is_user_anonymous(function)) {
        zend_value_error("Closure is not a user anonymous function");
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
                if (Z_ISNULL(z_references)) {
                    array_init(&z_references);
                }
                CAT_LOG_DEBUG_WITH_LEVEL(CLOSURE, 5, "Use reference $%.*s", (int) ZSTR_LEN(key), ZSTR_VAL(key));
                add_next_index_str(&z_references, zend_string_copy(key));
                continue;
            }
            if (UNEXPECTED(zval_update_constant_ex(z_val, function->common.scope) != SUCCESS)) {
                zend_throw_error(NULL, "Failed to solve static variable %.*s", (int) ZSTR_LEN(key), ZSTR_VAL(key));
                goto _err;
            }
            /* if (!Z_ISREF_P(z_val)) */ {
                CAT_LOG_DEBUG_WITH_LEVEL(CLOSURE, 5, "Use variable $%.*s", (int) ZSTR_LEN(key), ZSTR_VAL(key));
                Z_TRY_ADDREF_P(z_val);
                zend_hash_update(Z_ARRVAL(z_static_variables), key, z_val);
            }
        } ZEND_HASH_FOREACH_END();
    }

    CAT_LOG_DEBUG_V3(CLOSURE, "Closure { filename=%s, line_start=%u, line_end=%u }",
        ZSTR_VAL(filename), line_start, line_end);

    contents = swow_file_get_contents(filename);

    if (contents == NULL) {
        zend_throw_error(NULL, "Closure serialize error: cannot read file %s", ZSTR_VAL(filename));
        goto _err;
    }

    context.line_start = line_start;
    token_list = swow_php_tokenize(contents, swow_closure_walk_callback, &context);

    // printf("closure_str: %.*s\n", (int)context.closure_str.s->len, context.closure_str.s->val);

    // if (smart_str_get_len(&context.code_str) > 0) {
    //     printf("code_str: %.*s\n", (int)context.code_str.s->len, context.code_str.s->val);
    // }

    if (smart_str_get_len(&context.closure_str) == 0) {
        // no closure found
        zend_throw_error(NULL, "Closure serialize error: no closure found in source code");
        goto _err;
    }
    if (context.found_function > 1) {
        php_error_docref(NULL, E_WARNING, "Found multiple closure on %s:%d, using the first one", ZSTR_VAL(filename), line_start);
    }
    // now: "namespace A { use A; use B;"

    // align start line
    for (uint32_t i = 1; i < line_start; i++) {
        smart_str_appendc(&context.code_str, '\n');
    }
    // now: "namespace A { use A; use B;\n\n\n\n"

    // static variables and references needs wrapper
    if (!Z_ISNULL(z_static_variables) || !Z_ISNULL(z_references)) {
        zend_string *key;
        zval *z_val;
        smart_str_appendcstr(&context.code_str, "return (static function () ");
        if (!Z_ISNULL(z_references)) {
            bool first = true;
            smart_str_appendcstr(&context.code_str, "use (");
            ZEND_HASH_PACKED_FOREACH_VAL(Z_ARRVAL(z_references), z_val) {
                CAT_LOG_DEBUG_WITH_LEVEL(CLOSURE, 5, "Use reference for $%.*s", (int) Z_STRLEN_P(z_val), Z_STRVAL_P(z_val));
                if (first) {
                    first = false;
                } else {
                    smart_str_appendcstr(&context.code_str, ", ");
                }
                smart_str_appendcstr(&context.code_str, "&$");
                smart_str_append(&context.code_str, Z_STR_P(z_val));
            } ZEND_HASH_FOREACH_END();
            smart_str_appendcstr(&context.code_str, ") ");
        }
        smart_str_appendcstr(&context.code_str, "{ ");
        if (!Z_ISNULL(z_static_variables)) {
            ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(Z_ARRVAL(z_static_variables), key, z_val) {
                smart_str_appendc(&context.code_str, '$');
                smart_str_append(&context.code_str, key);
                smart_str_appendcstr(&context.code_str, " = NULL; ");
            } ZEND_HASH_FOREACH_END();
        }
    }
    // now: "namespace A { use A; use B;\n\n\n\nreturn (static function () use (...) { $a = NULL; "

    // append clusore
    smart_str_appendcstr(&context.code_str, "return (");
    smart_str_append_smart_str(&context.code_str, &context.closure_str);
    smart_str_appendcstr(&context.code_str, ")");
    // now:"namespace A { use A; use B;\n\n\n\nreturn (static function () use (...) { $a = NULL; return (fn()=>1)"

    // scope binding
    if (function->common.scope != NULL) {
        smart_str_appendcstr(&context.code_str, "->bindTo(null, \\");
        smart_str_append(&context.code_str, function->common.scope->name);
        smart_str_appendcstr(&context.code_str, "::class)");
    }
    // now: "namespace A { use A; use B;\n\n\n\nreturn (static function () use (...) { $a = NULL; return (fn()=>1)->bindTo(numm, \\A::class)"

    // return semi-colon
    smart_str_appendc(&context.code_str, ';');
    // now: "namespace A { use A; use B;\n\n\n\nreturn (static function () use (...) { $a = NULL; return (fn()=>1)->bindTo(numm, \\A::class);"

    // wrapper end brace
    if (!Z_ISNULL(z_static_variables) || !Z_ISNULL(z_references)) {
        smart_str_appendcstr(&context.code_str, " })();");
    }
    // now: "namespace A { use A; use B;\n\n\n\nreturn (static function () use (...) { $a = NULL; return (fn()=>1)->bindTo(numm, \\A::class); })();"

    // if in namespace brace
    if (context.in_namespace_brace) {
        smart_str_appendc(&context.code_str, '}');
    }
    smart_str_0(&context.code_str);

    ht = zend_new_array(3);
    ZVAL_STR_COPY(&z_tmp, filename);
    zend_hash_update(ht, ZSTR_KNOWN(ZEND_STR_FILE), &z_tmp);
    ZVAL_STR_COPY(&z_tmp, context.code_str.s);
    zend_hash_update(ht, ZSTR_KNOWN(ZEND_STR_CODE), &z_tmp);
    if (doc_comment != NULL) {
        ZVAL_STR(&z_tmp, zend_string_copy(doc_comment));
        zend_hash_update(ht, SWOW_KNOWN_STRING(doc_comment), &z_tmp);
    }
    if (!Z_ISNULL(z_static_variables)) {
        Z_TRY_ADDREF(z_static_variables);
        zend_hash_update(ht, SWOW_KNOWN_STRING(static_variables), &z_static_variables);
    }

    CAT_LOG_DEBUG_VA_WITH_LEVEL(CLOSURE, 5, {
        zend_string *output;
        ZVAL_ARR(&z_tmp, ht);
        SWOW_OB_START(output) {
            php_var_dump(&z_tmp, 0);
        } SWOW_OB_END();
        CAT_LOG_DEBUG_D(CLOSURE, "Closure hash: <<<DUMP\n%.*s}}}\nDUMP;",
            (int) ZSTR_LEN(output), ZSTR_VAL(output));
        zend_string_release(output);
    });

    _err:
    if (token_list) {
        swow_php_token_list_free(token_list);
    }
    if (contents != NULL) {
        zend_string_release_ex(contents, false);
    }
    smart_str_free(&context.code_str);
    smart_str_free(&context.closure_str);
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
    smart_str_appendcstr(&buffer, "return Closure::fromCallable(");
    if (scope != NULL) {
        smart_str_appendc(&buffer, '[');
        smart_str_append(&buffer, scope->name);
        smart_str_appendcstr(&buffer, "::class, '");
        smart_str_append(&buffer, function_name);
        smart_str_appendcstr(&buffer, "']");
    } else {
        smart_str_appendc(&buffer, '\'');
        smart_str_append(&buffer, function_name);
        smart_str_appendc(&buffer, '\'');
    }
    smart_str_appendcstr(&buffer, ");");
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
    bool is_named_function = swow_function_is_named(function);

    CAT_LOG_DEBUG_WITH_LEVEL(CLOSURE, 5, "Closure function name = [%zu] '%.*s'\n",
        ZSTR_LEN(function->op_array.function_name), (int) ZSTR_LEN(function->op_array.function_name), ZSTR_VAL(function->op_array.function_name));

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
        zend_throw_error(NULL, "Closure source code is corrupt");
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
        zend_argument_value_error(1, "Expected string for key 'file', got %s", zend_zval_type_name(z_file));
        RETURN_THROWS();
    }
    zval *z_code = zend_hash_find_known_hash(data, ZSTR_KNOWN(ZEND_STR_CODE));
    if (z_code == NULL || Z_TYPE_P(z_code) != IS_STRING) {
        zend_argument_value_error(1, "Expected string for key 'code', got %s", z_code == NULL ? "null" : zend_zval_type_name(z_code));
        RETURN_THROWS();
    }
    zval *z_doc_comment = zend_hash_find(data, SWOW_KNOWN_STRING(doc_comment));
    if (z_doc_comment != NULL && Z_TYPE_P(z_doc_comment) != IS_STRING) {
        zend_argument_value_error(1, "Expected string for key 'doc_comment', got %s", zend_zval_type_name(z_code));
        RETURN_THROWS();
    }
    zval *z_static_variables = zend_hash_find(data, SWOW_KNOWN_STRING(static_variables));
    if (z_static_variables != NULL && (Z_TYPE_P(z_static_variables) != IS_ARRAY || HT_IS_PACKED(Z_ARR_P(z_static_variables)))) {
        zend_argument_value_error(1, "Expected assoc array for key 'static_variables', got %s", zend_zval_type_name(z_code));
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
#if PHP_VERSION_ID < 80600
    zend_exception_restore();
#endif
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
    if (!SWOW_G(ini.closure_serializer)) {
        return SUCCESS;
    }

    CAT_GLOBALS_REGISTER(swow_closure);

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

zend_result swow_closure_module_shutdown(INIT_FUNC_ARGS)
{
    if (!SWOW_G(ini.closure_serializer)) {
        return SUCCESS;
    }

    CAT_GLOBALS_UNREGISTER(swow_closure);

    return SUCCESS;
}
