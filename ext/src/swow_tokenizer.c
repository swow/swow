#include "swow_tokenizer.h"

#include "zend_language_scanner.h"
#include "zend_language_scanner_defs.h"
#include <zend_language_parser.h>

typedef struct tokenizer_event_context_s {
    php_token_list_t *token_list;
} tokenizer_event_context_t;

SWOW_API php_token_list_t *php_token_list_alloc(void)
{
    php_token_list_t *token_list = (php_token_list_t *) emalloc(sizeof(*token_list));
    cat_queue_init(&token_list->queue);
    token_list->count = 0;
    return token_list;
}

SWOW_API void php_token_list_add(php_token_list_t *token_list, int token_type, const char *text, size_t text_length, int line)
{
    php_token_t *token = emalloc(sizeof(*token));

    token->type = token_type;
    cat_const_string_create(&token->text, text, text_length);
    token->line = line;
    token->offset = ((unsigned char *) text) - LANG_SCNG(yy_start);

    cat_queue_push_back(&token_list->queue, &token->node);
    token_list->count++;
}

SWOW_API void php_token_list_clear(php_token_list_t *token_list)
{
    php_token_t *token;
    while ((token = cat_queue_front_data(&token_list->queue, php_token_t, node))) {
        cat_queue_remove(&token->node);
        efree(token);
        token_list->count--;
    }
    ZEND_ASSERT(token_list->count == 0);
    zend_string_release(token_list->source);
    token_list->source = NULL;
}

SWOW_API void php_token_list_free(php_token_list_t *token_list)
{
    php_token_list_clear(token_list);
    efree(token_list);
}

static php_token_t *tokenizer_extract_token_to_replace_id(php_token_t *token, const char *text, size_t length)
{
    ZEND_ASSERT(token != NULL);

    /* There are multiple candidate tokens to which this feedback may apply,
     * check text to make sure this is the right one. */
    if (token->text.length == length && memcmp(token->text.data, text, length) == 0) {
        return token;
    }

    return NULL;
}

void tokenizer_on_language_scanner_event(
        zend_php_scanner_event event, int token_type, int line,
        const char *text, size_t length, void *event_context)
{
    tokenizer_event_context_t *context = event_context;

    switch (event) {
        case ON_TOKEN:
            if (token_type == END) break;
            /* Special cases */
            if (token_type == ';' && LANG_SCNG(yy_leng) > 1) { /* ?> or ?>\n or ?>\r\n */
                token_type = T_CLOSE_TAG;
            } else if (token_type == T_ECHO && LANG_SCNG(yy_leng) == sizeof("<?=") - 1) {
                token_type = T_OPEN_TAG_WITH_ECHO;
            }
            php_token_list_add(context->token_list, token_type, text, length, line);
            break;
        case ON_FEEDBACK: {
            php_token_list_t *token_list = context->token_list;
            php_token_t *target_token = NULL;
            CAT_QUEUE_REVERSE_FOREACH_DATA_START(&token_list->queue, php_token_t, node, token) {
                target_token = tokenizer_extract_token_to_replace_id(token, text, length);
                if (target_token) {
                    break;
                }
            } CAT_QUEUE_FOREACH_DATA_END();
            ZEND_ASSERT(target_token != NULL);
            target_token->type = token_type;
            break;
        }
        case ON_STOP:
            if (LANG_SCNG(yy_cursor) != LANG_SCNG(yy_limit)) {
                php_token_list_add(context->token_list, T_INLINE_HTML,
                    (const char *) LANG_SCNG(yy_cursor),
                    LANG_SCNG(yy_limit) - LANG_SCNG(yy_cursor),
                    CG(zend_lineno));
            }
            break;
    }
}

SWOW_API int swow_ast_children(zend_ast *node, zend_ast ***child)
{
    int children;

    // get all children
    if (node->kind & (1 << ZEND_AST_IS_LIST_SHIFT)) {
        // is list
        children = ((zend_ast_list *) node)->children;
        *child = (zend_ast **) (((zend_ast_list *) node)->child);
        // printf("list size %d\n", children);
    } else if (node->kind & (1 << ZEND_AST_SPECIAL_SHIFT)) {
        // is special
        CAT_ASSERT(node->kind != ZEND_AST_ZNODE);
        switch (node->kind) {
            case ZEND_AST_ZVAL:
            case ZEND_AST_CONSTANT:
                children = 0;
                *child = NULL;
                break;
            case ZEND_AST_FUNC_DECL:
            case ZEND_AST_CLOSURE:
            case ZEND_AST_METHOD:
            case ZEND_AST_CLASS:
            case ZEND_AST_ARROW_FUNC:
                children = 5;
                *child = (zend_ast **) (((zend_ast_decl *) node)->child);
                break;
            default:
                CAT_NEVER_HERE("unknown ast kind");
                return -1;
        }
    } else {
        children = (node->kind >> ZEND_AST_NUM_CHILDREN_SHIFT) & 7;
        *child = node->child;
    }

    return children;
}

#if 0

SWOW_API const char *ast_kind_name(int kind) {
    switch (kind)
    {
        case ZEND_AST_ZVAL:
            return "ZEND_AST_ZVAL";
        case ZEND_AST_CONSTANT:
            return "ZEND_AST_CONSTANT";
        case ZEND_AST_ZNODE:
            return "ZEND_AST_ZNODE";

        case ZEND_AST_FUNC_DECL:
            return "ZEND_AST_FUNC_DECL";
        case ZEND_AST_CLOSURE:
            return "ZEND_AST_CLOSURE";
        case ZEND_AST_METHOD:
            return "ZEND_AST_METHOD";
        case ZEND_AST_CLASS:
            return "ZEND_AST_CLASS";
        case ZEND_AST_ARROW_FUNC:
            return "ZEND_AST_ARROW_FUNC";

        case ZEND_AST_ARG_LIST:
            return "ZEND_AST_ARG_LIST";
        case ZEND_AST_ARRAY:
            return "ZEND_AST_ARRAY";
        case ZEND_AST_ENCAPS_LIST:
            return "ZEND_AST_ENCAPS_LIST";
        case ZEND_AST_EXPR_LIST:
            return "ZEND_AST_EXPR_LIST";
        case ZEND_AST_STMT_LIST:
            return "ZEND_AST_STMT_LIST";
        case ZEND_AST_IF:
            return "ZEND_AST_IF";
        case ZEND_AST_SWITCH_LIST:
            return "ZEND_AST_SWITCH_LIST";
        case ZEND_AST_CATCH_LIST:
            return "ZEND_AST_CATCH_LIST";
        case ZEND_AST_PARAM_LIST:
            return "ZEND_AST_PARAM_LIST";
        case ZEND_AST_CLOSURE_USES:
            return "ZEND_AST_CLOSURE_USES";
        case ZEND_AST_PROP_DECL:
            return "ZEND_AST_PROP_DECL";
        case ZEND_AST_CONST_DECL:
            return "ZEND_AST_CONST_DECL";
        case ZEND_AST_CLASS_CONST_DECL:
            return "ZEND_AST_CLASS_CONST_DECL";
        case ZEND_AST_NAME_LIST:
            return "ZEND_AST_NAME_LIST";
        case ZEND_AST_TRAIT_ADAPTATIONS:
            return "ZEND_AST_TRAIT_ADAPTATIONS";
        case ZEND_AST_USE:
            return "ZEND_AST_USE";
        case ZEND_AST_TYPE_UNION:
            return "ZEND_AST_TYPE_UNION";
        case ZEND_AST_TYPE_INTERSECTION:
            return "ZEND_AST_TYPE_INTERSECTION";
        case ZEND_AST_ATTRIBUTE_LIST:
            return "ZEND_AST_ATTRIBUTE_LIST";
        case ZEND_AST_ATTRIBUTE_GROUP:
            return "ZEND_AST_ATTRIBUTE_GROUP";
        case ZEND_AST_MATCH_ARM_LIST:
            return "ZEND_AST_MATCH_ARM_LIST";

        case ZEND_AST_MAGIC_CONST:
            return "ZEND_AST_MAGIC_CONST";
        case ZEND_AST_TYPE:
            return "ZEND_AST_TYPE";
        case ZEND_AST_CONSTANT_CLASS:
            return "ZEND_AST_CONSTANT_CLASS";
        case ZEND_AST_CALLABLE_CONVERT:
            return "ZEND_AST_CALLABLE_CONVERT";

        case ZEND_AST_VAR:
            return "ZEND_AST_VAR";
        case ZEND_AST_CONST:
            return "ZEND_AST_CONST";
        case ZEND_AST_UNPACK:
            return "ZEND_AST_UNPACK";
        case ZEND_AST_UNARY_PLUS:
            return "ZEND_AST_UNARY_PLUS";
        case ZEND_AST_UNARY_MINUS:
            return "ZEND_AST_UNARY_MINUS";
        case ZEND_AST_CAST:
            return "ZEND_AST_CAST";
        case ZEND_AST_EMPTY:
            return "ZEND_AST_EMPTY";
        case ZEND_AST_ISSET:
            return "ZEND_AST_ISSET";
        case ZEND_AST_SILENCE:
            return "ZEND_AST_SILENCE";
        case ZEND_AST_SHELL_EXEC:
            return "ZEND_AST_SHELL_EXEC";
        case ZEND_AST_CLONE:
            return "ZEND_AST_CLONE";
        case ZEND_AST_EXIT:
            return "ZEND_AST_EXIT";
        case ZEND_AST_PRINT:
            return "ZEND_AST_PRINT";
        case ZEND_AST_INCLUDE_OR_EVAL:
            return "ZEND_AST_INCLUDE_OR_EVAL";
        case ZEND_AST_UNARY_OP:
            return "ZEND_AST_UNARY_OP";
        case ZEND_AST_PRE_INC:
            return "ZEND_AST_PRE_INC";
        case ZEND_AST_PRE_DEC:
            return "ZEND_AST_PRE_DEC";
        case ZEND_AST_POST_INC:
            return "ZEND_AST_POST_INC";
        case ZEND_AST_POST_DEC:
            return "ZEND_AST_POST_DEC";
        case ZEND_AST_YIELD_FROM:
            return "ZEND_AST_YIELD_FROM";
        case ZEND_AST_CLASS_NAME:
            return "ZEND_AST_CLASS_NAME";

        case ZEND_AST_GLOBAL:
            return "ZEND_AST_GLOBAL";
        case ZEND_AST_UNSET:
            return "ZEND_AST_UNSET";
        case ZEND_AST_RETURN:
            return "ZEND_AST_RETURN";
        case ZEND_AST_LABEL:
            return "ZEND_AST_LABEL";
        case ZEND_AST_REF:
            return "ZEND_AST_REF";
        case ZEND_AST_HALT_COMPILER:
            return "ZEND_AST_HALT_COMPILER";
        case ZEND_AST_ECHO:
            return "ZEND_AST_ECHO";
        case ZEND_AST_THROW:
            return "ZEND_AST_THROW";
        case ZEND_AST_GOTO:
            return "ZEND_AST_GOTO";
        case ZEND_AST_BREAK:
            return "ZEND_AST_BREAK";
        case ZEND_AST_CONTINUE:
            return "ZEND_AST_CONTINUE";

        case ZEND_AST_DIM:
            return "ZEND_AST_DIM";
        case ZEND_AST_PROP:
            return "ZEND_AST_PROP";
        case ZEND_AST_NULLSAFE_PROP:
            return "ZEND_AST_NULLSAFE_PROP";
        case ZEND_AST_STATIC_PROP:
            return "ZEND_AST_STATIC_PROP";
        case ZEND_AST_CALL:
            return "ZEND_AST_CALL";
        case ZEND_AST_CLASS_CONST:
            return "ZEND_AST_CLASS_CONST";
        case ZEND_AST_ASSIGN:
            return "ZEND_AST_ASSIGN";
        case ZEND_AST_ASSIGN_REF:
            return "ZEND_AST_ASSIGN_REF";
        case ZEND_AST_ASSIGN_OP:
            return "ZEND_AST_ASSIGN_OP";
        case ZEND_AST_BINARY_OP:
            return "ZEND_AST_BINARY_OP";
        case ZEND_AST_GREATER:
            return "ZEND_AST_GREATER";
        case ZEND_AST_GREATER_EQUAL:
            return "ZEND_AST_GREATER_EQUAL";
        case ZEND_AST_AND:
            return "ZEND_AST_AND";
        case ZEND_AST_OR:
            return "ZEND_AST_OR";
        case ZEND_AST_ARRAY_ELEM:
            return "ZEND_AST_ARRAY_ELEM";
        case ZEND_AST_NEW:
            return "ZEND_AST_NEW";
        case ZEND_AST_INSTANCEOF:
            return "ZEND_AST_INSTANCEOF";
        case ZEND_AST_YIELD:
            return "ZEND_AST_YIELD";
        case ZEND_AST_COALESCE:
            return "ZEND_AST_COALESCE";
        case ZEND_AST_ASSIGN_COALESCE:
            return "ZEND_AST_ASSIGN_COALESCE";

        case ZEND_AST_STATIC:
            return "ZEND_AST_STATIC";
        case ZEND_AST_WHILE:
            return "ZEND_AST_WHILE";
        case ZEND_AST_DO_WHILE:
            return "ZEND_AST_DO_WHILE";
        case ZEND_AST_IF_ELEM:
            return "ZEND_AST_IF_ELEM";
        case ZEND_AST_SWITCH:
            return "ZEND_AST_SWITCH";
        case ZEND_AST_SWITCH_CASE:
            return "ZEND_AST_SWITCH_CASE";
        case ZEND_AST_DECLARE:
            return "ZEND_AST_DECLARE";
        case ZEND_AST_USE_TRAIT:
            return "ZEND_AST_USE_TRAIT";
        case ZEND_AST_TRAIT_PRECEDENCE:
            return "ZEND_AST_TRAIT_PRECEDENCE";
        case ZEND_AST_METHOD_REFERENCE:
            return "ZEND_AST_METHOD_REFERENCE";
        case ZEND_AST_NAMESPACE:
            return "ZEND_AST_NAMESPACE";
        case ZEND_AST_USE_ELEM:
            return "ZEND_AST_USE_ELEM";
        case ZEND_AST_TRAIT_ALIAS:
            return "ZEND_AST_TRAIT_ALIAS";
        case ZEND_AST_GROUP_USE:
            return "ZEND_AST_GROUP_USE";
        case ZEND_AST_CLASS_CONST_GROUP:
            return "ZEND_AST_CLASS_CONST_GROUP";
        case ZEND_AST_ATTRIBUTE:
            return "ZEND_AST_ATTRIBUTE";
        case ZEND_AST_MATCH:
            return "ZEND_AST_MATCH";
        case ZEND_AST_MATCH_ARM:
            return "ZEND_AST_MATCH_ARM";
        case ZEND_AST_NAMED_ARG:
            return "ZEND_AST_NAMED_ARG";

        case ZEND_AST_METHOD_CALL:
            return "ZEND_AST_METHOD_CALL";
        case ZEND_AST_NULLSAFE_METHOD_CALL:
            return "ZEND_AST_NULLSAFE_METHOD_CALL";
        case ZEND_AST_STATIC_CALL:
            return "ZEND_AST_STATIC_CALL";
        case ZEND_AST_CONDITIONAL:
            return "ZEND_AST_CONDITIONAL";

        case ZEND_AST_TRY:
            return "ZEND_AST_TRY";
        case ZEND_AST_CATCH:
            return "ZEND_AST_CATCH";
        case ZEND_AST_PROP_GROUP:
            return "ZEND_AST_PROP_GROUP";
        case ZEND_AST_PROP_ELEM:
            return "ZEND_AST_PROP_ELEM";
        case ZEND_AST_CONST_ELEM:
            return "ZEND_AST_CONST_ELEM";

        case ZEND_AST_CONST_ENUM_INIT:
            return "ZEND_AST_CONST_ENUM_INIT";

        case ZEND_AST_FOR:
            return "ZEND_AST_FOR";
        case ZEND_AST_FOREACH:
            return "ZEND_AST_FOREACH";
        case ZEND_AST_ENUM_CASE:
            return "ZEND_AST_ENUM_CASE";

        case ZEND_AST_PARAM:
            return "ZEND_AST_PARAM";
        default:
            return NULL;
    }
}

SWOW_API int ast_walk_callbacks(zend_ast *node, zend_ast *parent, int children, zend_ast **child, ast_walk_callbacks_aggregate *callbacks) {
    int ret = AST_WALK_CONTINUE;
    for (int i = 0; i < callbacks->len; i++) {
        if ((ret = callbacks->callbacks[i].callback(node, parent, children, child, callbacks->callbacks[i].context)) != AST_WALK_CONTINUE) {
            return ret;
        }
    }
    return ret;
}

SWOW_API int swow_walk_ast(zend_ast *node, zend_ast *parent, ast_walk_callback enter, ast_walk_callback leave, void *context)
{
    int children;
    zend_ast **child;

    children = swow_ast_children(node, &child);
    CAT_ASSERT(children >= 0);

#define parse_result(stmt) do { \
    switch (stmt) { \
        case AST_WALK_STOP: \
            /* interrupted in callbacks */ \
            return AST_WALK_STOP; \
        case AST_WALK_SKIP: \
            /* skip current node */ \
            return AST_WALK_CONTINUE; \
        case AST_WALK_CONTINUE: \
            break; \
        default: \
            CAT_NEVER_HERE("bad ast walk cb retval"); \
            return AST_WALK_ERROR; \
    } \
} while (0)

    // call enter node callback
    if (enter) {
        parse_result(enter(node, parent, children, child, context));
    }

    for (int i = 0; i < children; i++) {
        if (child[i]) {
            parse_result(swow_walk_ast(child[i], node, enter, leave, context));
        }
    }

    if (leave) {
        parse_result(leave(node, parent, children, child, context));
    }

#undef parse_result

    return AST_WALK_CONTINUE;
}

#endif

SWOW_API php_token_list_t *php_tokenize(zend_string *source, int (*ast_callback)(zend_ast *, void *), void *ast_callback_context)
{
    zval source_zval;
    tokenizer_event_context_t ctx;
    zend_lex_state original_lex_state;
    bool original_in_compilation;

    ZVAL_STR_COPY(&source_zval, source);

    original_in_compilation = CG(in_compilation);
    CG(in_compilation) = 1;
    zend_save_lexical_state(&original_lex_state);

#if PHP_VERSION_ID < 80100
    zend_prepare_string_for_scanning(&source_zval, "");
#else
    zend_prepare_string_for_scanning(&source_zval, ZSTR_EMPTY_ALLOC());
#endif

    ctx.token_list = php_token_list_alloc();

    CG(ast) = NULL;
    CG(ast_arena) = zend_arena_create(32 * 1024);
    LANG_SCNG(yy_state) = yycINITIAL;
    LANG_SCNG(on_event) = tokenizer_on_language_scanner_event;
    LANG_SCNG(on_event_context) = &ctx;

    if (zendparse() != SUCCESS) {
        php_token_list_free(ctx.token_list);
        return NULL;
    }

    if (ast_callback) {
        ast_callback(CG(ast), ast_callback_context);
    }

    zend_ast_destroy(CG(ast));
    zend_arena_destroy(CG(ast_arena));

    /* restore compiler and scanner global states */
    zend_restore_lexical_state(&original_lex_state);
    CG(in_compilation) = original_in_compilation;

    ZEND_ASSERT(Z_TYPE(source_zval) == IS_STRING);
    ctx.token_list->source = Z_STR(source_zval);

    return ctx.token_list;
}

/* data */

SWOW_API const char *php_token_get_name(const php_token_t *token)
{
    return php_token_get_name_from_type(token->type);
}

SWOW_API const char *php_token_get_name_from_type(int type)
{
    if (type < 256) {
        static char text_map[256 + 256];
        static bool initialized = false;
        if (!initialized) {
            for (int i = 0; i < 256; i++) {
                char *p = text_map + (i + i);
                *p = (char) i;
                *(p + 1) = '\0';
            }
            initialized = true;
        }
        return &text_map[type + type];
    }
    switch (type) {
        case T_LNUMBER: return "T_LNUMBER";
        case T_DNUMBER: return "T_DNUMBER";
        case T_STRING: return "T_STRING";
        case T_NAME_FULLY_QUALIFIED: return "T_NAME_FULLY_QUALIFIED";
        case T_NAME_RELATIVE: return "T_NAME_RELATIVE";
        case T_NAME_QUALIFIED: return "T_NAME_QUALIFIED";
        case T_VARIABLE: return "T_VARIABLE";
        case T_INLINE_HTML: return "T_INLINE_HTML";
        case T_ENCAPSED_AND_WHITESPACE: return "T_ENCAPSED_AND_WHITESPACE";
        case T_CONSTANT_ENCAPSED_STRING: return "T_CONSTANT_ENCAPSED_STRING";
        case T_STRING_VARNAME: return "T_STRING_VARNAME";
        case T_NUM_STRING: return "T_NUM_STRING";
        case T_INCLUDE: return "T_INCLUDE";
        case T_INCLUDE_ONCE: return "T_INCLUDE_ONCE";
        case T_EVAL: return "T_EVAL";
        case T_REQUIRE: return "T_REQUIRE";
        case T_REQUIRE_ONCE: return "T_REQUIRE_ONCE";
        case T_LOGICAL_OR: return "T_LOGICAL_OR";
        case T_LOGICAL_XOR: return "T_LOGICAL_XOR";
        case T_LOGICAL_AND: return "T_LOGICAL_AND";
        case T_PRINT: return "T_PRINT";
        case T_YIELD: return "T_YIELD";
        case T_YIELD_FROM: return "T_YIELD_FROM";
        case T_INSTANCEOF: return "T_INSTANCEOF";
        case T_NEW: return "T_NEW";
        case T_CLONE: return "T_CLONE";
        case T_EXIT: return "T_EXIT";
        case T_IF: return "T_IF";
        case T_ELSEIF: return "T_ELSEIF";
        case T_ELSE: return "T_ELSE";
        case T_ENDIF: return "T_ENDIF";
        case T_ECHO: return "T_ECHO";
        case T_DO: return "T_DO";
        case T_WHILE: return "T_WHILE";
        case T_ENDWHILE: return "T_ENDWHILE";
        case T_FOR: return "T_FOR";
        case T_ENDFOR: return "T_ENDFOR";
        case T_FOREACH: return "T_FOREACH";
        case T_ENDFOREACH: return "T_ENDFOREACH";
        case T_DECLARE: return "T_DECLARE";
        case T_ENDDECLARE: return "T_ENDDECLARE";
        case T_AS: return "T_AS";
        case T_SWITCH: return "T_SWITCH";
        case T_ENDSWITCH: return "T_ENDSWITCH";
        case T_CASE: return "T_CASE";
        case T_DEFAULT: return "T_DEFAULT";
        case T_MATCH: return "T_MATCH";
        case T_BREAK: return "T_BREAK";
        case T_CONTINUE: return "T_CONTINUE";
        case T_GOTO: return "T_GOTO";
        case T_FUNCTION: return "T_FUNCTION";
        case T_FN: return "T_FN";
        case T_CONST: return "T_CONST";
        case T_RETURN: return "T_RETURN";
        case T_TRY: return "T_TRY";
        case T_CATCH: return "T_CATCH";
        case T_FINALLY: return "T_FINALLY";
        case T_THROW: return "T_THROW";
        case T_USE: return "T_USE";
        case T_INSTEADOF: return "T_INSTEADOF";
        case T_GLOBAL: return "T_GLOBAL";
        case T_STATIC: return "T_STATIC";
        case T_ABSTRACT: return "T_ABSTRACT";
        case T_FINAL: return "T_FINAL";
        case T_PRIVATE: return "T_PRIVATE";
        case T_PROTECTED: return "T_PROTECTED";
        case T_PUBLIC: return "T_PUBLIC";
#if PHP_VERSION_ID >= 80100
        case T_READONLY: return "T_READONLY";
#endif
        case T_VAR: return "T_VAR";
        case T_UNSET: return "T_UNSET";
        case T_ISSET: return "T_ISSET";
        case T_EMPTY: return "T_EMPTY";
        case T_HALT_COMPILER: return "T_HALT_COMPILER";
        case T_CLASS: return "T_CLASS";
        case T_TRAIT: return "T_TRAIT";
        case T_INTERFACE: return "T_INTERFACE";
#if PHP_VERSION_ID >= 80100
        case T_ENUM: return "T_ENUM";
#endif
        case T_EXTENDS: return "T_EXTENDS";
        case T_IMPLEMENTS: return "T_IMPLEMENTS";
        case T_NAMESPACE: return "T_NAMESPACE";
        case T_LIST: return "T_LIST";
        case T_ARRAY: return "T_ARRAY";
        case T_CALLABLE: return "T_CALLABLE";
        case T_LINE: return "T_LINE";
        case T_FILE: return "T_FILE";
        case T_DIR: return "T_DIR";
        case T_CLASS_C: return "T_CLASS_C";
        case T_TRAIT_C: return "T_TRAIT_C";
        case T_METHOD_C: return "T_METHOD_C";
        case T_FUNC_C: return "T_FUNC_C";
        case T_NS_C: return "T_NS_C";
        case T_ATTRIBUTE: return "T_ATTRIBUTE";
        case T_PLUS_EQUAL: return "T_PLUS_EQUAL";
        case T_MINUS_EQUAL: return "T_MINUS_EQUAL";
        case T_MUL_EQUAL: return "T_MUL_EQUAL";
        case T_DIV_EQUAL: return "T_DIV_EQUAL";
        case T_CONCAT_EQUAL: return "T_CONCAT_EQUAL";
        case T_MOD_EQUAL: return "T_MOD_EQUAL";
        case T_AND_EQUAL: return "T_AND_EQUAL";
        case T_OR_EQUAL: return "T_OR_EQUAL";
        case T_XOR_EQUAL: return "T_XOR_EQUAL";
        case T_SL_EQUAL: return "T_SL_EQUAL";
        case T_SR_EQUAL: return "T_SR_EQUAL";
        case T_COALESCE_EQUAL: return "T_COALESCE_EQUAL";
        case T_BOOLEAN_OR: return "T_BOOLEAN_OR";
        case T_BOOLEAN_AND: return "T_BOOLEAN_AND";
        case T_IS_EQUAL: return "T_IS_EQUAL";
        case T_IS_NOT_EQUAL: return "T_IS_NOT_EQUAL";
        case T_IS_IDENTICAL: return "T_IS_IDENTICAL";
        case T_IS_NOT_IDENTICAL: return "T_IS_NOT_IDENTICAL";
        case T_IS_SMALLER_OR_EQUAL: return "T_IS_SMALLER_OR_EQUAL";
        case T_IS_GREATER_OR_EQUAL: return "T_IS_GREATER_OR_EQUAL";
        case T_SPACESHIP: return "T_SPACESHIP";
        case T_SL: return "T_SL";
        case T_SR: return "T_SR";
        case T_INC: return "T_INC";
        case T_DEC: return "T_DEC";
        case T_INT_CAST: return "T_INT_CAST";
        case T_DOUBLE_CAST: return "T_DOUBLE_CAST";
        case T_STRING_CAST: return "T_STRING_CAST";
        case T_ARRAY_CAST: return "T_ARRAY_CAST";
        case T_OBJECT_CAST: return "T_OBJECT_CAST";
        case T_BOOL_CAST: return "T_BOOL_CAST";
        case T_UNSET_CAST: return "T_UNSET_CAST";
        case T_OBJECT_OPERATOR: return "T_OBJECT_OPERATOR";
        case T_NULLSAFE_OBJECT_OPERATOR: return "T_NULLSAFE_OBJECT_OPERATOR";
        case T_DOUBLE_ARROW: return "T_DOUBLE_ARROW";
        case T_COMMENT: return "T_COMMENT";
        case T_DOC_COMMENT: return "T_DOC_COMMENT";
        case T_OPEN_TAG: return "T_OPEN_TAG";
        case T_OPEN_TAG_WITH_ECHO: return "T_OPEN_TAG_WITH_ECHO";
        case T_CLOSE_TAG: return "T_CLOSE_TAG";
        case T_WHITESPACE: return "T_WHITESPACE";
        case T_START_HEREDOC: return "T_START_HEREDOC";
        case T_END_HEREDOC: return "T_END_HEREDOC";
        case T_DOLLAR_OPEN_CURLY_BRACES: return "T_DOLLAR_OPEN_CURLY_BRACES";
        case T_CURLY_OPEN: return "T_CURLY_OPEN";
        case T_PAAMAYIM_NEKUDOTAYIM: return "T_DOUBLE_COLON";
        case T_NS_SEPARATOR: return "T_NS_SEPARATOR";
        case T_ELLIPSIS: return "T_ELLIPSIS";
        case T_COALESCE: return "T_COALESCE";
        case T_POW: return "T_POW";
        case T_POW_EQUAL: return "T_POW_EQUAL";
#if PHP_VERSION_ID >= 80100
        case T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG: return "T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG";
        case T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG: return "T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG";
#endif
        case T_BAD_CHARACTER: return "T_BAD_CHARACTER";
        default: return "UNKNOWN";
    }
}
