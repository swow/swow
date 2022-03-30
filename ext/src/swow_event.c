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

#include "swow_event.h"

#include "swow_defer.h"
#include "swow_coroutine.h"

static cat_bool_t swow_event_scheduler_run(void)
{
    swow_coroutine_t *scoroutine;
    cat_coroutine_t *coroutine;

    /* Notice: if object id is 0, its destructor will never be called */
    do {
        uint32_t top = EG(objects_store).top;
        EG(objects_store).top = 0;
        scoroutine = swow_coroutine_get_from_object(
            swow_object_create(swow_coroutine_ce)
        );
        EG(objects_store).top = top;
        EG(objects_store).object_buckets[0] = NULL;
        scoroutine->std.handle = UINT32_MAX;
    } while (0);

    /* do not call __destruct() on scheduler scoroutine (but unnecessary here) */
    // GC_ADD_FLAGS(&scoroutine->std, IS_OBJ_DESTRUCTOR_CALLED);

    coroutine = cat_event_scheduler_run(&scoroutine->coroutine);

    return coroutine != NULL;
}

static cat_bool_t swow_event_scheduler_close(void)
{
    swow_coroutine_t *scoroutine;
    cat_coroutine_t *coroutine;

    coroutine = cat_event_scheduler_close();

    if (coroutine == NULL) {
        CAT_WARN_WITH_LAST(EVENT, "Event scheduler close failed");
        return cat_false;
    }

    do {
        scoroutine = swow_coroutine_get_from_handle(coroutine);
        scoroutine->std.handle = 0;
        swow_coroutine_close(scoroutine);
        EG(objects_store).object_buckets[0] = NULL;
    } while (0);

    return cat_true;
}

static zif_handler original_pcntl_fork = (zif_handler) -1;

static PHP_FUNCTION(swow_pcntl_fork)
{
    original_pcntl_fork(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    if (Z_TYPE_P(return_value) == IS_LONG || Z_LVAL_P(return_value) == 0) {
        /* Fork event loop in child process
         * TODO: kill all coroutines?  */
        cat_event_fork();
    }
}

zend_result swow_event_module_init(INIT_FUNC_ARGS)
{
    if (!cat_event_module_init()) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result swow_event_runtime_init(INIT_FUNC_ARGS)
{
    if (!cat_event_runtime_init()) {
        return FAILURE;
    }

    if (!swow_event_scheduler_run()) {
        return FAILURE;
    }

    if (original_pcntl_fork == (zif_handler) -1) {
        zend_function *pcntl_fork_function =
            (zend_function *) zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("pcntl_fork"));
        if (pcntl_fork_function != NULL && pcntl_fork_function->type == ZEND_INTERNAL_FUNCTION) {
            original_pcntl_fork = pcntl_fork_function->internal_function.handler;
            pcntl_fork_function->internal_function.handler = PHP_FN(swow_pcntl_fork);
        }
    }

    return SUCCESS;
}

zend_result swow_event_runtime_shutdown(SHUTDOWN_FUNC_ARGS)
{
    /* after register_shutdown and call all destructors */

    /* stop event scheduler (event_wait() is included) */
    if (!swow_event_scheduler_close()) {
        return FAILURE;
    }

    if (!cat_event_runtime_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result swow_event_runtime_close(void)
{
    if (!cat_event_runtime_close()) {
        return FAILURE;
    }

    return SUCCESS;
}
