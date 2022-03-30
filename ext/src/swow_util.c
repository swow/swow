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

#include "swow_util.h"

SWOW_API zend_class_entry *swow_util_handler_ce;
SWOW_API zend_object_handlers swow_util_handler_handlers;

static zend_object *swow_util_handler_create_object(zend_class_entry *ce)
{
    swow_util_handler_t *handler = swow_object_alloc(swow_util_handler_t, ce, swow_util_handler_handlers);

    cat_queue_init(&handler->node);
    ZVAL_NULL(&handler->fcall.zcallable);

    return &handler->std;
}

static void swow_util_handler_free_object(zend_object *object)
{
    swow_util_handler_t *handler = swow_util_handler_get_from_object(object);

    cat_queue_remove(&handler->node);
    swow_fcall_storage_release(&handler->fcall);

    zend_object_std_dtor(&handler->std);
}

SWOW_API swow_util_handler_t *swow_util_handler_create(zval *zcallable)
{
    swow_util_handler_t *handler = swow_util_handler_get_from_object(
        swow_util_handler_create_object(swow_util_handler_ce)
    );

    if (!swow_fcall_storage_create(&handler->fcall, zcallable)) {
        cat_update_last_error_with_previous("Util handler create failed");
        return NULL;
    }

    return handler;
}

SWOW_API void swow_util_handler_push_back_to(swow_util_handler_t *handler, cat_queue_t *queue)
{
    cat_queue_push_back(queue, &handler->node);
}

SWOW_API void swow_util_handler_remove(swow_util_handler_t *handler)
{
    cat_queue_remove(&handler->node);
    cat_queue_init(&handler->node);
    zend_object_release(&handler->std);
}

SWOW_API void swow_util_handlers_release(cat_queue_t *handlers)
{
    swow_util_handler_t *handler;
    while ((handler = cat_queue_front_data(handlers, swow_util_handler_t, node))) {
        swow_util_handler_remove(handler);
    }
}

#define getThisUtilHandler() (swow_util_handler_get_from_object(Z_OBJ_P(ZEND_THIS)))

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Util_Handler___construct, 0, 0, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Util_Handler, __construct)
{
    ZEND_UNREACHABLE();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Util_Handler_remove, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Util_Handler, remove)
{
    swow_util_handler_remove(getThisUtilHandler());
}

static const zend_function_entry swow_util_handler_methods[] = {
    PHP_ME(Swow_Util_Handler, __construct, arginfo_class_Swow_Util_Handler___construct, ZEND_ACC_PROTECTED)
    PHP_ME(Swow_Util_Handler, remove,      arginfo_class_Swow_Util_Handler_remove,      ZEND_ACC_PUBLIC   )
    PHP_FE_END
};

static HashTable *swow_util_handler_get_gc(zend_object *object, zval **gc_data, int *gc_count)
{
    swow_util_handler_t *handler = swow_util_handler_get_from_object(object);

    if (ZVAL_IS_NULL(&handler->fcall.zcallable)) {
        *gc_data = NULL;
        *gc_count = 0;
    } else {
        *gc_data = &handler->fcall.zcallable;
        *gc_count = 1;
    }

    return zend_std_get_properties(object);
}

zend_result swow_util_module_init(INIT_FUNC_ARGS)
{
    swow_util_handler_ce = swow_register_internal_class(
        "Swow\\Util\\Handler", NULL, swow_util_handler_methods,
        &swow_util_handler_handlers, NULL,
        cat_false, cat_false,
        swow_util_handler_create_object,
        swow_util_handler_free_object,
        XtOffsetOf(swow_util_handler_t, std)
    );
    swow_util_handler_handlers.get_gc = swow_util_handler_get_gc;

    return SUCCESS;
}
