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

#include "swow_defer.h"

#include "swow_known_strings.h"

SWOW_API zend_class_entry *swow_defer_ce;
SWOW_API zend_object_handlers swow_defer_handlers;

#define SWOW_DEFER_KNOWN_STRING_MAP(XX) \
    XX(defer_magic_name, "\0Swow\\Defer") \

SWOW_DEFER_KNOWN_STRING_MAP(SWOW_KNOWN_STRING_STORAGE_GEN)

SWOW_API cat_bool_t swow_defer(zval *z_callable)
{
    swow_defer_task_t *task = (swow_defer_task_t *) emalloc(sizeof(*task));

    /* check callable and fetch the fcc */
    do {
        char *error;
        if (!zend_is_callable_ex(z_callable, NULL, 0, NULL, &task->fcc, &error)) {
            cat_update_last_error(CAT_EMISUSE, "Defer task must be callable, %s", error);
            efree(error);
            efree(task);
            return cat_false;
        }
        efree(error);
    } while (0);
    /* copy callable (addref) */
    ZVAL_COPY(&task->z_callable, z_callable);

    /* get or new defer object on symbol table, and push the task to it */
    do {
        zend_array *symbol_table = zend_rebuild_symbol_table();
        swow_defer_t *s_defer;
        zval *z_defer;

        ZEND_ASSERT(symbol_table && "A symbol table should always be available here");

        z_defer = zend_hash_find(symbol_table, SWOW_KNOWN_STRING(defer_magic_name));
        if (z_defer == NULL) {
            zend_object *defer = swow_object_create(swow_defer_ce);
            zval z_defer;
            ZVAL_OBJ(&z_defer, defer);
            zend_set_local_var(SWOW_KNOWN_STRING(defer_magic_name), &z_defer, true);
            s_defer = swow_defer_get_from_object(defer);
        } else {
            ZEND_ASSERT(Z_TYPE_P(z_defer) == IS_OBJECT && instanceof_function(Z_OBJCE_P(z_defer), swow_defer_ce));
            s_defer = swow_defer_get_from_object(Z_OBJ_P(z_defer));
        }
        cat_queue_push_back(&s_defer->tasks, &task->node);
    } while (0);

    return cat_true;
}

SWOW_API void swow_defer_do_tasks(swow_defer_t *s_defer)
{
    cat_queue_t *tasks = &s_defer->tasks;
    swow_defer_task_t *task;

    /* must be FILO */
    while ((task = cat_queue_back_data(tasks, swow_defer_task_t, node))) {
        zend_fcall_info fci;
        zval retval;
        cat_queue_remove(&task->node);
        fci.size = sizeof(fci);
        ZVAL_UNDEF(&fci.function_name);
        fci.object = NULL;
        fci.param_count = 0;
        fci.named_params = NULL;
        fci.retval = &retval;
        (void) swow_call_function_anyway(&fci, &task->fcc);
        zval_ptr_dtor(&retval);
        zval_ptr_dtor(&task->z_callable);
        efree(task);
    }
}

SWOW_API void swow_defer_do_main_tasks(void)
{
    zval *z_defer = zend_hash_find_known_hash(&EG(symbol_table), SWOW_KNOWN_STRING(defer_magic_name));
    if (z_defer != NULL) {
        swow_defer_t *s_defer = swow_defer_get_from_object(Z_OBJ_P(z_defer));
        swow_defer_do_tasks(s_defer);
    }
}

static zend_object *swow_defer_create_object(zend_class_entry *ce)
{
    swow_defer_t *s_defer = swow_object_alloc(swow_defer_t, ce, swow_defer_handlers);

    cat_queue_init(&s_defer->tasks);

    return &s_defer->std;
}

static void swow_defer_dtor_object(zend_object *object)
{
    swow_defer_t *s_defer = swow_defer_get_from_object(object);

    swow_defer_do_tasks(s_defer);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Defer___construct, 0, 0, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Defer, __construct)
{
    zend_throw_error(NULL, "The object of %s can not be constructed for security reasons", ZEND_THIS_NAME);
}

static const zend_function_entry swow_defer_methods[] = {
    PHP_ME(Swow_Defer, __construct, arginfo_class_Swow_Defer___construct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_defer, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, tasks, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(Swow_defer)
{
    zval *z_task;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(1, -1)
        Z_PARAM_ZVAL(z_task)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(swow_forbid_dynamic_call() != SUCCESS)) {
        RETURN_THROWS();
    }

    ret = swow_defer(z_task);

    if (UNEXPECTED(!ret)) {
        zend_throw_error(NULL, "%s", cat_get_last_error_message());
        RETURN_THROWS();
    }
}

static const zend_function_entry swow_defer_functions[] = {
    PHP_FENTRY(Swow\\defer, PHP_FN(Swow_defer), arginfo_Swow_defer, 0)
    PHP_FE_END
};

zend_result swow_defer_module_init(INIT_FUNC_ARGS)
{
    SWOW_DEFER_KNOWN_STRING_MAP(SWOW_KNOWN_STRING_INIT_STRL_GEN);

    swow_defer_ce = swow_register_internal_class(
        "Swow\\Defer", NULL, swow_defer_methods,
        &swow_defer_handlers, NULL,
        cat_false, cat_false,
        swow_defer_create_object, NULL,
        XtOffsetOf(swow_defer_t, std)
    );
    swow_defer_ce->ce_flags |= ZEND_ACC_FINAL;
    /* we do not need get_gc because we never expose defer object to user */
    swow_defer_handlers.dtor_obj = swow_defer_dtor_object;

    if (zend_register_functions(NULL, swow_defer_functions, NULL, type) != SUCCESS) {
        return FAILURE;
    }

    return SUCCESS;
}
