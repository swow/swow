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
  | Author: Twosee <twose@qq.com>                                            |
  +--------------------------------------------------------------------------+
 */

#include "swow_debug.h"

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

static void swow_build_trace_args(zval *arg, smart_str *str) /* {{{ */
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

static void swow_build_trace_string(smart_str *str, HashTable *ht, uint32_t num, zend_bool trim) /* {{{ */
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
        } else{
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
                swow_build_trace_args(arg, str);
            } ZEND_HASH_FOREACH_END();

            if (last_len != ZSTR_LEN(str->s)) {
                ZSTR_LEN(str->s) -= 2; /* remove last ', ' */
            }
        } else {
            zend_error(E_WARNING, "args element is no array");
        }
    }
    if (!trim) {
        smart_str_appends(str, ")\n");
    }
}

#undef TRACE_APPEND_KEY

SWOW_API HashTable *swow_get_trace(zend_long options, zend_long limit)
{
    zval retval;
    zend_fetch_debug_backtrace(&retval, 0, options, limit);
    return Z_ARRVAL(retval);
}

SWOW_API smart_str *swow_get_trace_to_string(smart_str *str, zend_long options, zend_long limit)
{
    HashTable *trace;
    zend_ulong index;
    zval *zframe;
    uint32_t num = 0;

    trace = swow_get_trace(options, limit);
    ZEND_HASH_FOREACH_NUM_KEY_VAL(trace, index, zframe) {
        if (Z_TYPE_P(zframe) != IS_ARRAY) {
            zend_error(E_WARNING, "Expected array for frame " ZEND_ULONG_FMT, index);
            continue;
        }
        swow_build_trace_string(str, Z_ARRVAL_P(zframe), num++, 0);
    } ZEND_HASH_FOREACH_END();
    zend_array_destroy(trace);

    smart_str_appendc(str, '#');
    smart_str_append_long(str, num);
    smart_str_appends(str, " {main}");

    return str;
}

SWOW_API zend_string *swow_get_trace_as_string(zend_long options, zend_long limit)
{
    smart_str str = {0};

    swow_get_trace_to_string(&str, options, limit);
    smart_str_0(&str);

    return str.s;
}

SWOW_API HashTable *swow_get_trace_as_list(zend_long options, zend_long limit)
{
    HashTable *trace, *list;
    zend_ulong index;
    zval *zframe, zline;

    trace = swow_get_trace(options, limit);
    list = zend_new_array(zend_hash_num_elements(trace));

    ZEND_HASH_FOREACH_NUM_KEY_VAL(trace, index, zframe) {
        smart_str str = {0};
        if (Z_TYPE_P(zframe) != IS_ARRAY) {
            zend_error(E_WARNING, "Expected array for frame " ZEND_ULONG_FMT, index);
            continue;
        }
        swow_build_trace_string(&str, Z_ARRVAL_P(zframe), 0, 1);
        smart_str_0(&str);
        ZVAL_STR(&zline, str.s);
        zend_hash_next_index_insert(list, &zline);
    } ZEND_HASH_FOREACH_END();
    zend_array_destroy(trace);

    ZVAL_STRING(&zline, "{main}");
    zend_hash_next_index_insert(list, &zline);

    return list;
}
