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

#ifdef CAT_OS_WAIT

#include "swow_hook.h"

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
    CloseHandle(proc->childHandle);
    return wstatus;
#else
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
    zval *zproc;
    php_process_handle *proc;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_RESOURCE(zproc)
    ZEND_PARSE_PARAMETERS_END();

    proc = (php_process_handle *) zend_fetch_resource(Z_RES_P(zproc), "process", le_proc_open);
    if (proc == NULL) {
        RETURN_THROWS();
    }

    int status = swow_proc_open_rsrc_close(proc);
    zend_list_close(Z_RES_P(zproc));
    RETURN_LONG(status);
}
/* }}} */

zend_result swow_proc_open_module_init(INIT_FUNC_ARGS)
{
    le_proc_open = zend_fetch_list_dtor_id("process");

    if (!cat_os_wait_module_init()) {
        return FAILURE;
    }

    if (!swow_hook_internal_function_handler(ZEND_STRL("proc_close"), PHP_FN(swow_proc_close))) {
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

#endif /* CAT_OS_WAIT */
