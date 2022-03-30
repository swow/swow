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

#include "swow_debug.h"

#include "swow_coroutine.h"

#include "zend_generators.h"

SWOW_API zend_long swow_debug_backtrace_depth(zend_execute_data *call, zend_long limit)
{
    zend_long depth;
    swow_debug_backtrace_resolve_ex(call, ZEND_LONG_MAX, limit, &depth);
    return depth;
}

SWOW_API zend_execute_data *swow_debug_backtrace_resolve(zend_execute_data *call, zend_long level)
{
    if (level < 0) {
        level = swow_debug_backtrace_depth(call, 0) + level;
    }

    return swow_debug_backtrace_resolve_ex(call, level, 0, NULL);
}

#if PHP_VERSION_ID < 80100
static inline zend_bool swow_debug_skip_internal_handler(zend_execute_data *skip) /* {{{ */
{
    return !(skip->func && ZEND_USER_CODE(skip->func->common.type))
            && skip->prev_execute_data
            && skip->prev_execute_data->func
            && ZEND_USER_CODE(skip->prev_execute_data->func->common.type)
            && skip->prev_execute_data->opline->opcode != ZEND_DO_FCALL
            && skip->prev_execute_data->opline->opcode != ZEND_DO_ICALL
            && skip->prev_execute_data->opline->opcode != ZEND_DO_UCALL
            && skip->prev_execute_data->opline->opcode != ZEND_DO_FCALL_BY_NAME
            && skip->prev_execute_data->opline->opcode != ZEND_INCLUDE_OR_EVAL;
}

SWOW_API zend_execute_data *swow_debug_backtrace_resolve_ex(zend_execute_data *execute_data, zend_long level, zend_long limit, zend_long *depth)
{
    zend_execute_data *prev = execute_data, *skip, *call = NULL;
    zend_long count = 0;
    zend_string *filename, *function_name;

    if (depth) {
        *depth = 0;
    }
    if (!prev) {
        return NULL;
    }

    if (!prev->func || !ZEND_USER_CODE(prev->func->common.type)) {
        call = prev;
        prev = prev->prev_execute_data;
    }
    if (prev) {
        /* skip "new Exception()" */
        if (prev->func && ZEND_USER_CODE(prev->func->common.type) && (prev->opline->opcode == ZEND_NEW)) {
            call = prev;
            prev = prev->prev_execute_data;
        }
        if (!call) {
            call = prev;
            prev = prev->prev_execute_data;
        }
    }
    while (level > 0 && (limit <= 0 || count < limit)) {
        prev = zend_generator_check_placeholder_frame(prev);
        skip = prev;
        /* skip internal handler */
        if (swow_debug_skip_internal_handler(skip)) {
            skip = skip->prev_execute_data;
        }
        if (skip->func && ZEND_USER_CODE(skip->func->common.type)) {
            filename = skip->func->op_array.filename;
        } else {
            filename = NULL;
        }
        if (call && call->func) {
            function_name = call->func->common.function_name;
        } else {
            function_name = NULL;
        }
        if (!function_name) {
            uint32_t include_kind = 0;
            if (prev->func && ZEND_USER_CODE(prev->func->common.type) && prev->opline->opcode == ZEND_INCLUDE_OR_EVAL) {
                include_kind = prev->opline->extended_value;
            }
            switch (include_kind) {
                case ZEND_EVAL:
                case ZEND_INCLUDE:
                case ZEND_REQUIRE:
                case ZEND_INCLUDE_ONCE:
                case ZEND_REQUIRE_ONCE:
                    break;
                default:
                    if (!filename) {
                        /* Skip dummy frame unless it is needed to preserve filename/lineno info. */
                        goto _skip_frame;
                    }
            }
        }
        count++;
        level--;
_skip_frame:
        if (!skip->prev_execute_data) {
            break;
        }
        call = skip;
        prev = skip->prev_execute_data;
    }

    if (depth) {
        *depth = count;
    }
    return call;
}
#else
SWOW_API zend_execute_data *swow_debug_backtrace_resolve_ex(zend_execute_data *call, zend_long level, zend_long limit, zend_long *depth)
{
    zend_function *func;
    zend_string *filename;
    zend_long count = 0;
    bool fake_frame = 0;

    if (depth) {
        *depth = 0;
    }
    if (!call) {
        return NULL;
    }

    while (level > 0 && (limit <= 0 || count < limit)) {
        zend_execute_data *prev = call->prev_execute_data;
        if (!prev) {
            /* add frame for a handler call without {main} code */
            if (EXPECTED((ZEND_CALL_INFO(call) & ZEND_CALL_TOP_FUNCTION) == 0)) {
                break;
            }
        } else if (UNEXPECTED((ZEND_CALL_INFO(call) & ZEND_CALL_GENERATOR) != 0)) {
            prev = zend_generator_check_placeholder_frame(prev);
        }
        if (swow_debug_is_user_call(prev)) {
            filename = prev->func->op_array.filename;
        } else {
            filename = NULL;
        }
        func = call->func;
        if (fake_frame || !func->common.function_name) {
            uint32_t include_kind = 0;
            if (swow_debug_is_user_call(prev) && prev->opline->opcode == ZEND_INCLUDE_OR_EVAL) {
                include_kind = prev->opline->extended_value;
            }
            switch (include_kind) {
                case ZEND_EVAL:
                case ZEND_INCLUDE:
                case ZEND_REQUIRE:
                case ZEND_INCLUDE_ONCE:
                case ZEND_REQUIRE_ONCE:
                    break;
                default:
                    if (!filename) {
                        /* Skip dummy frame unless it is needed to preserve filename/lineno info. */
                        goto _skip_frame;
                    }
            }
        }
        count++;
        level--;
_skip_frame:
        if (!prev) {
            break;
        }
        if (UNEXPECTED((ZEND_CALL_INFO(call) & ZEND_CALL_TOP_FUNCTION) != 0) && !fake_frame &&
            swow_debug_is_user_call(prev) && prev->opline->opcode == ZEND_INCLUDE_OR_EVAL) {
            fake_frame = 1;
        } else {
            fake_frame = 0;
            call = prev;
        }
    }

    if (depth) {
        *depth = count;
    }
    return call;
}
#endif

#define TRACE_APPEND_KEY(key) do {                                          \
        tmp = zend_hash_find(ht, key);                                      \
        if (tmp) {                                                          \
            if (Z_TYPE_P(tmp) != IS_STRING) {                               \
                zend_error(E_WARNING, "Value for %s is no string",          \
                    ZSTR_VAL(key));                                         \
                smart_str_appends(str, "[unknown]");                        \
            } else {                                                        \
                smart_str_appends(str, Z_STRVAL_P(tmp));                    \
            }                                                               \
        } \
    } while (0)

static void swow_debug_build_trace_args(zval *arg, smart_str *str) /* {{{ */
{
    /* the trivial way would be to do
     * convert_to_string_ex(arg);
     * append it and kill the now tmp arg.
     * but that could cause some E_NOTICE and also damn long lines.
     */

    ZVAL_DEREF(arg);
    switch (Z_TYPE_P(arg)) {
        case IS_NULL:
            smart_str_appends(str, "NULL, ");
            break;
        case IS_STRING:
            smart_str_appendc(str, '\'');
            smart_str_append_escaped(str, Z_STRVAL_P(arg), MIN(Z_STRLEN_P(arg), 15));
            if (Z_STRLEN_P(arg) > 15) {
                smart_str_appends(str, "...', ");
            } else {
                smart_str_appends(str, "', ");
            }
            break;
        case IS_FALSE:
            smart_str_appends(str, "false, ");
            break;
        case IS_TRUE:
            smart_str_appends(str, "true, ");
            break;
        case IS_RESOURCE:
            smart_str_appends(str, "Resource id #");
            smart_str_append_long(str, Z_RES_HANDLE_P(arg));
            smart_str_appends(str, ", ");
            break;
        case IS_LONG:
            smart_str_append_long(str, Z_LVAL_P(arg));
            smart_str_appends(str, ", ");
            break;
        case IS_DOUBLE: {
            smart_str_append_printf(str, "%.*G", (int) EG(precision), Z_DVAL_P(arg));
            smart_str_appends(str, ", ");
            break;
        }
        case IS_ARRAY:
            smart_str_appends(str, "Array, ");
            break;
        case IS_OBJECT: {
            zend_string *class_name = Z_OBJ_HANDLER_P(arg, get_class_name)(Z_OBJ_P(arg));
            smart_str_appends(str, "Object(");
            smart_str_appends(str, ZSTR_VAL(class_name));
            smart_str_appends(str, "), ");
            zend_string_release(class_name);
            break;
        }
    }
}

static void swow_debug_build_trace_string(smart_str *str, HashTable *ht, uint32_t num, zend_bool trim) /* {{{ */
{
    zval *file, *tmp;

    if (!trim) {
        smart_str_appendc(str, '#');
        smart_str_append_long(str, num);
        smart_str_appendc(str, ' ');
    }

    file = zend_hash_find_ex(ht, ZSTR_KNOWN(ZEND_STR_FILE), 1);
    if (file) {
        if (Z_TYPE_P(file) != IS_STRING) {
            zend_error(E_WARNING, "Function name is no string");
            smart_str_appends(str, "[unknown function]");
        } else {
            zend_long line;
            tmp = zend_hash_find_ex(ht, ZSTR_KNOWN(ZEND_STR_LINE), 1);
            if (tmp) {
                if (Z_TYPE_P(tmp) == IS_LONG) {
                    line = Z_LVAL_P(tmp);
                } else {
                    zend_error(E_WARNING, "Line is no long");
                    line = 0;
                }
            } else {
                line = 0;
            }
            smart_str_append(str, Z_STR_P(file));
            smart_str_appendc(str, '(');
            smart_str_append_long(str, line);
            smart_str_appends(str, "): ");
        }
    } else {
        smart_str_appends(str, "[internal function]: ");
    }
    TRACE_APPEND_KEY(ZSTR_KNOWN(ZEND_STR_CLASS));
    TRACE_APPEND_KEY(ZSTR_KNOWN(ZEND_STR_TYPE));
    TRACE_APPEND_KEY(ZSTR_KNOWN(ZEND_STR_FUNCTION));
    smart_str_appendc(str, '(');
    tmp = zend_hash_find_ex(ht, ZSTR_KNOWN(ZEND_STR_ARGS), 1);
    if (tmp) {
        if (Z_TYPE_P(tmp) == IS_ARRAY) {
            size_t last_len = ZSTR_LEN(str->s);
            zval *arg;

            ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(tmp), arg) {
                swow_debug_build_trace_args(arg, str);
            } ZEND_HASH_FOREACH_END();

            if (last_len != ZSTR_LEN(str->s)) {
                ZSTR_LEN(str->s) -= 2; /* remove last ', ' */
            }
        } else {
            zend_error(E_WARNING, "args element is no array");
        }
    }
    if (trim) {
        smart_str_appendc(str, ')');
    } else {
        smart_str_appends(str, ")\n");
    }
}

#undef TRACE_APPEND_KEY

SWOW_API smart_str *swow_debug_build_trace_as_smart_str(smart_str *str, HashTable *trace)
{
    zend_ulong index;
    zval *zframe;
    uint32_t num = 0;

    ZEND_HASH_FOREACH_NUM_KEY_VAL(trace, index, zframe) {
        if (Z_TYPE_P(zframe) != IS_ARRAY) {
            zend_error(E_WARNING, "Expected array for frame " ZEND_ULONG_FMT, index);
            continue;
        }
        swow_debug_build_trace_string(str, Z_ARRVAL_P(zframe), num++, 0);
    } ZEND_HASH_FOREACH_END();

    smart_str_appendc(str, '#');
    smart_str_append_long(str, num);
    smart_str_appends(str, " {main}");

    return str;
}

SWOW_API zend_string *swow_debug_build_trace_as_string(HashTable *trace)
{
    smart_str str = { 0 };

    swow_debug_build_trace_as_smart_str(&str, trace);

    smart_str_0(&str);

    return str.s;
}

SWOW_API HashTable *swow_debug_get_trace(zend_long options, zend_long limit)
{
    zval retval;
    zend_fetch_debug_backtrace(&retval, 0, options, limit > 0 ? limit : 0);
    return Z_ARRVAL(retval);
}

SWOW_API smart_str *swow_debug_get_trace_as_smart_str(smart_str *str, zend_long options, zend_long limit)
{
    HashTable *trace;

    trace = swow_debug_get_trace(options, limit);

    str = swow_debug_build_trace_as_smart_str(str, trace);

    zend_array_destroy(trace);

    return str;
}

SWOW_API zend_string *swow_debug_get_trace_as_string(zend_long options, zend_long limit)
{
    smart_str str = { 0 };

    swow_debug_get_trace_as_smart_str(&str, options, limit);
    smart_str_0(&str);

    return str.s;
}

SWOW_API HashTable *swow_debug_get_trace_as_list(zend_long options, zend_long limit)
{
    HashTable *trace, *list;
    zend_ulong index;
    zval *zframe, zline;

    trace = swow_debug_get_trace(options, limit);
    list = zend_new_array(zend_hash_num_elements(trace));

    ZEND_HASH_FOREACH_NUM_KEY_VAL(trace, index, zframe) {
        smart_str str = {0};
        if (Z_TYPE_P(zframe) != IS_ARRAY) {
            zend_error(E_WARNING, "Expected array for frame " ZEND_ULONG_FMT, index);
            continue;
        }
        swow_debug_build_trace_string(&str, Z_ARRVAL_P(zframe), 0, 1);
        smart_str_0(&str);
        ZVAL_STR(&zline, str.s);
        zend_hash_next_index_insert(list, &zline);
    } ZEND_HASH_FOREACH_END();
    zend_array_destroy(trace);

    ZVAL_STRING(&zline, "{main}");
    zend_hash_next_index_insert(list, &zline);

    return list;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Debug_buildTraceAsString, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, trace, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(Swow_Debug_buildTraceAsString)
{
    HashTable *trace;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY_HT(trace)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STR(swow_debug_build_trace_as_string(trace));
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_Swow_Debug_registerExtendedStatementHandler, 0, 1, Swow\x5cUtil\\Handler, 0)
    ZEND_ARG_TYPE_INFO(0, handler, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(Swow_Debug_registerExtendedStatementHandler)
{
    swow_util_handler_t *handler;
    swow_fcall_storage_t fcall;
    zval zfcall;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        SWOW_PARAM_FCALL(fcall)
    ZEND_PARSE_PARAMETERS_END();

    if (!(CG(compiler_options) & ZEND_COMPILE_EXTENDED_INFO)) {
        zend_throw_error(NULL, "Please re-run your program with \"-e\" option");
        RETURN_THROWS();
    }

    ZVAL_PTR(&zfcall, &fcall);
    handler = swow_util_handler_create(&zfcall);
    ZEND_ASSERT(handler != NULL);

    swow_util_handler_push_back_to(handler, &SWOW_DEBUG_G(extended_statement_handlers));

    RETURN_OBJ_COPY(&handler->std);
}

static const zend_function_entry swow_debug_functions[] = {
    PHP_FENTRY(Swow\\Debug\\buildTraceAsString, PHP_FN(Swow_Debug_buildTraceAsString), arginfo_Swow_Debug_buildTraceAsString, 0)
    /* for breakpoint debugging  */
    PHP_FENTRY(Swow\\Debug\\registerExtendedStatementHandler, PHP_FN(Swow_Debug_registerExtendedStatementHandler), arginfo_Swow_Debug_registerExtendedStatementHandler, 0)
    PHP_FE_END
};

SWOW_API CAT_GLOBALS_DECLARE(swow_debug)

CAT_GLOBALS_CTOR_DECLARE_SZ(swow_debug)

static user_opcode_handler_t original_zend_ext_stmt_handler;

static int swow_debug_ext_stmt_handler(zend_execute_data *execute_data)
{
    swow_coroutine_t *scoroutine = swow_coroutine_get_current();

    if (!cat_queue_empty(&SWOW_DEBUG_G(extended_statement_handlers)) &&
        !(scoroutine->coroutine.flags & SWOW_COROUTINE_FLAG_DEBUGGING)) {
        zend_fcall_info fci;
        zval retval;

        fci.size = sizeof(fci);
        ZVAL_UNDEF(&fci.function_name);
        fci.object = NULL;
        fci.param_count = 0;
        fci.named_params = NULL;
        fci.retval = &retval;

        scoroutine->coroutine.flags |= SWOW_COROUTINE_FLAG_DEBUGGING;

        CAT_QUEUE_FOREACH_DATA_START(&SWOW_DEBUG_G(extended_statement_handlers), swow_util_handler_t, node, handler) {
            (void) zend_call_function(&fci, &handler->fcall.fcc);
        } CAT_QUEUE_FOREACH_DATA_END();

        scoroutine->coroutine.flags ^= SWOW_COROUTINE_FLAG_DEBUGGING;

        zval_ptr_dtor(&retval);
    }

    if (original_zend_ext_stmt_handler != NULL) {
        return original_zend_ext_stmt_handler(execute_data);
    }

    return ZEND_USER_OPCODE_DISPATCH;
}

zend_result swow_debug_module_init(INIT_FUNC_ARGS)
{
    CAT_GLOBALS_REGISTER(swow_debug, CAT_GLOBALS_CTOR(swow_debug), NULL);

    if (zend_register_functions(NULL, swow_debug_functions, NULL, type) != SUCCESS) {
        return FAILURE;
    }

    original_zend_ext_stmt_handler = zend_get_user_opcode_handler(ZEND_EXT_STMT);
    (void) zend_set_user_opcode_handler(ZEND_EXT_STMT, swow_debug_ext_stmt_handler);

    return SUCCESS;
}

zend_result swow_debug_runtime_init(INIT_FUNC_ARGS)
{
    cat_queue_init(&SWOW_DEBUG_G(extended_statement_handlers));

    return SUCCESS;
}

zend_result swow_debug_runtime_shutdown(INIT_FUNC_ARGS)
{
    swow_util_handlers_release(&SWOW_DEBUG_G(extended_statement_handlers));

    return SUCCESS;
}
