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

#include "swow_utils.h"

SWOW_API zend_class_entry *swow_utils_handler_ce;
SWOW_API zend_object_handlers swow_utils_handler_handlers;

static zend_object *swow_utils_handler_create_object(zend_class_entry *ce)
{
    swow_utils_handler_t *handler = swow_object_alloc(swow_utils_handler_t, ce, swow_utils_handler_handlers);

    cat_queue_init(&handler->node);
    ZVAL_NULL(&handler->fcall.z_callable);

    return &handler->std;
}

static void swow_utils_handler_free_object(zend_object *object)
{
    swow_utils_handler_t *handler = swow_utils_handler_get_from_object(object);

    cat_queue_remove(&handler->node);
    swow_fcall_storage_release(&handler->fcall);

    zend_object_std_dtor(&handler->std);
}

SWOW_API swow_utils_handler_t *swow_utils_handler_create(zval *z_callable)
{
    swow_utils_handler_t *handler = swow_utils_handler_get_from_object(
        swow_utils_handler_create_object(swow_utils_handler_ce)
    );

    if (!swow_fcall_storage_create(&handler->fcall, z_callable)) {
        cat_update_last_error_with_previous("Util handler create failed");
        return NULL;
    }

    return handler;
}

SWOW_API void swow_utils_handler_push_back_to(swow_utils_handler_t *handler, cat_queue_t *queue)
{
    cat_queue_push_back(queue, &handler->node);
}

SWOW_API void swow_utils_handler_remove(swow_utils_handler_t *handler)
{
    cat_queue_remove(&handler->node);
    cat_queue_init(&handler->node);
    zend_object_release(&handler->std);
}

SWOW_API void swow_utils_handlers_release(cat_queue_t *handlers)
{
    swow_utils_handler_t *handler;
    while ((handler = cat_queue_front_data(handlers, swow_utils_handler_t, node))) {
        swow_utils_handler_remove(handler);
    }
}

#define getThisUtilHandler() (swow_utils_handler_get_from_object(Z_OBJ_P(ZEND_THIS)))

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Utils_Handler___construct, 0, 0, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Utils_Handler, __construct)
{
    ZEND_UNREACHABLE();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Utils_Handler_remove, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Utils_Handler, remove)
{
    swow_utils_handler_remove(getThisUtilHandler());
}

static const zend_function_entry swow_utils_handler_methods[] = {
    PHP_ME(Swow_Utils_Handler, __construct, arginfo_class_Swow_Utils_Handler___construct, ZEND_ACC_PROTECTED)
    PHP_ME(Swow_Utils_Handler, remove,      arginfo_class_Swow_Utils_Handler_remove,      ZEND_ACC_PUBLIC   )
    PHP_FE_END
};

static HashTable *swow_utils_handler_get_gc(zend_object *object, zval **gc_data, int *gc_count)
{
    swow_utils_handler_t *handler = swow_utils_handler_get_from_object(object);

    if (ZVAL_IS_NULL(&handler->fcall.z_callable)) {
        *gc_data = NULL;
        *gc_count = 0;
    } else {
        *gc_data = &handler->fcall.z_callable;
        *gc_count = 1;
    }

    return zend_std_get_properties(object);
}

zend_result swow_util_module_init(INIT_FUNC_ARGS)
{
    swow_utils_handler_ce = swow_register_internal_class(
        "Swow\\Utils\\Handler", NULL, swow_utils_handler_methods,
        &swow_utils_handler_handlers, NULL,
        cat_false, cat_false,
        swow_utils_handler_create_object,
        swow_utils_handler_free_object,
        XtOffsetOf(swow_utils_handler_t, std)
    );
    swow_utils_handler_ce->ce_flags |= ZEND_ACC_FINAL;
    swow_utils_handler_handlers.get_gc = swow_utils_handler_get_gc;

    return SUCCESS;
}
