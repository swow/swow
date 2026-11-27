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

#include "swow_proc_open.h"

#include "swow_hook.h"
#include "swow_stream.h"

#include "zend_list.h"
#include "ext/standard/proc_open.h"

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

static int le_proc_open = -1;

static int swow_proc_open_rsrc_close(php_process_handle *proc)
{
#ifdef PHP_WIN32
    DWORD wstatus;
#else
    int wstatus;
    pid_t wait_pid;
#endif

    /* Close all handles to avoid a deadlock */
    for (int i = 0; i < proc->npipes; i++) {
        if (proc->pipes[i] != NULL) {
            GC_DELREF(proc->pipes[i]);
            zend_list_close(proc->pipes[i]);
            proc->pipes[i] = NULL;
        }
    }

#ifdef PHP_WIN32
    // FIXME: may block on Windows
    // 1. wait in a new thread
    // 2. use RegisterWaitForSingleObject()
    WaitForSingleObject(proc->childHandle, INFINITE);
    GetExitCodeProcess(proc->childHandle, &wstatus);
    if (wstatus == STILL_ACTIVE) {
        wstatus = -1;
    }
    /* proc_open_rsrc_dtor closes childHandle. Do not close it here. */
    return wstatus;
#else
# if PHP_VERSION_ID >= 80300
    /* proc_get_status() may have reaped the child already, PHP keeps its exit
     * status for proc_close() since 8.3 (GH-10239), waiting again would get ECHILD */
    if (proc->has_cached_exit_wait_status) {
        wstatus = proc->cached_exit_wait_status_value;
        return WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : wstatus;
    }
# endif
    wait_pid = cat_os_waitpid(proc->child, &wstatus, 0);

    if (wait_pid <= 0) {
        return -1;
    } else {
        // TODO: fix this behaviour
        return WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : wstatus;
    }
#endif
}
/* }}} */

/* {{{ Close a process opened by `proc_open` */
PHP_FUNCTION(swow_proc_close)
{
    zval *z_proc;
    php_process_handle *proc;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_RESOURCE(z_proc)
    ZEND_PARSE_PARAMETERS_END();

    proc = (php_process_handle *) zend_fetch_resource(Z_RES_P(z_proc), "process", le_proc_open);
    if (proc == NULL) {
        RETURN_THROWS();
    }

    int status = swow_proc_open_rsrc_close(proc);
    zend_list_close(Z_RES_P(z_proc));
    RETURN_LONG(status);
}
/* }}} */

static zif_handler PHP_FN(original_proc_open) = (zif_handler) -1;

/* proc_open() wraps a ['socket'] descriptor with PHP's own blocking socket ops,
 * let those pipes wait on the event loop as well */
PHP_FUNCTION(swow_proc_open)
{
    PHP_FN(original_proc_open)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    if (Z_TYPE_P(return_value) == IS_RESOURCE && ZEND_NUM_ARGS() >= 3) {
        zval *zpipes = ZEND_CALL_ARG(execute_data, 3);
        zval *zpipe;
        ZVAL_DEREF(zpipes);
        if (Z_TYPE_P(zpipes) == IS_ARRAY) {
            ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(zpipes), zpipe) {
                swow_stream_native_socket_adopt_zval(zpipe);
            } ZEND_HASH_FOREACH_END();
        }
    }
}

zend_result swow_proc_open_module_init(INIT_FUNC_ARGS)
{
    le_proc_open = zend_fetch_list_dtor_id("process");

    if (!cat_os_wait_module_init()) {
        return FAILURE;
    }

    if (!swow_hook_internal_function_handler(ZEND_STRL("proc_close"), PHP_FN(swow_proc_close))) {
        return FAILURE;
    }
    if (!swow_hook_internal_function_handler_ex(ZEND_STRL("proc_open"), PHP_FN(swow_proc_open), &PHP_FN(original_proc_open))) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result swow_proc_open_module_shutdown(INIT_FUNC_ARGS)
{
    if (!cat_os_wait_module_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result swow_proc_open_runtime_init(INIT_FUNC_ARGS)
{
    if (!cat_os_wait_runtime_init()) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result swow_proc_open_runtime_shutdown(INIT_FUNC_ARGS)
{
    if (!cat_os_wait_runtime_shutdown()) {
        return FAILURE;
    }

    return SUCCESS;
}
