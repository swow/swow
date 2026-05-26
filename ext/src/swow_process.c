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

#include "swow_process.h"

#ifdef CAT_OS_WAIT

#include "swow_closure.h"

#include "cat_signal.h"

#include <sys/wait.h>

SWOW_API zend_class_entry *swow_process_interface_ce;

SWOW_API zend_class_entry *swow_fork_process_ce;
SWOW_API zend_object_handlers swow_fork_process_handlers;

SWOW_API zend_class_entry *swow_process_exit_status_ce;
SWOW_API zend_object_handlers swow_process_exit_status_handlers;

SWOW_API zend_class_entry *swow_process_exception_ce;

/* {{{ ProcessInterface */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ProcessInterface_getPid, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Process_ProcessInterface_wait, 0, 0, Swow\\Process\\ProcessExitStatus, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ProcessInterface_hasExited, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ProcessInterface_kill, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, signal, IS_LONG, 0, "\\Swow\\Signal::TERM")
ZEND_END_ARG_INFO()

static const zend_function_entry swow_process_interface_methods[] = {
    ZEND_ABSTRACT_ME(Swow_Process_ProcessInterface, getPid, arginfo_class_Swow_Process_ProcessInterface_getPid)
    ZEND_ABSTRACT_ME(Swow_Process_ProcessInterface, wait, arginfo_class_Swow_Process_ProcessInterface_wait)
    ZEND_ABSTRACT_ME(Swow_Process_ProcessInterface, hasExited, arginfo_class_Swow_Process_ProcessInterface_hasExited)
    ZEND_ABSTRACT_ME(Swow_Process_ProcessInterface, kill, arginfo_class_Swow_Process_ProcessInterface_kill)
    PHP_FE_END
};

/* }}} */

/* {{{ ForkProcess */

static zend_object *swow_fork_process_create_object(zend_class_entry *ce)
{
    swow_fork_process_t *s_process = swow_object_alloc(swow_fork_process_t, ce, swow_fork_process_handlers);

    s_process->pid = -1;
    s_process->status = 0;
    s_process->exited = cat_false;

    return &s_process->std;
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Process_ForkProcess_fork, 0, 1, Swow\\Process\\ForkProcess, 0)
    ZEND_ARG_OBJ_INFO(0, callback, Closure, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ForkProcess, fork)
{
    zval *zcallback;
    zend_fcall_info fci;
    zend_fcall_info_cache fcc;
    cat_pid_t pid;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(zcallback, zend_ce_closure)
    ZEND_PARSE_PARAMETERS_END();

    pid = cat_os_fork();
    if (pid < 0) {
        swow_throw_exception_with_last(swow_process_exception_ce);
        RETURN_THROWS();
    }

    if (pid == 0) {
        /* 子进程：执行 callback，用返回值作为 exit code */
        zval retval;
        int exit_code = 0;

        if (zend_fcall_info_init(zcallback, 0, &fci, &fcc, NULL, NULL) == SUCCESS) {
            fci.retval = &retval;
            fci.param_count = 0;
            fci.params = NULL;

            if (zend_call_function(&fci, &fcc) == SUCCESS) {
                if (Z_TYPE(retval) == IS_LONG) {
                    exit_code = (int) Z_LVAL(retval);
                }
                zval_ptr_dtor(&retval);
            } else {
                exit_code = 1;
            }
        } else {
            exit_code = 1;
        }

        /* 子进程必须退出，不能回到调用者的 PHP 代码流 */
        _exit(exit_code);
        /* NOTREACHED */
    }

    /* 父进程：返回 ForkProcess 对象 */
    object_init_ex(return_value, swow_fork_process_ce);
    swow_fork_process_t *s_process = swow_fork_process_get_from_object(Z_OBJ_P(return_value));
    s_process->pid = pid;
}

#define arginfo_class_Swow_Process_ForkProcess_getPid arginfo_class_Swow_Process_ProcessInterface_getPid

static PHP_METHOD(Swow_Process_ForkProcess, getPid)
{
    swow_fork_process_t *s_process;

    ZEND_PARSE_PARAMETERS_NONE();

    s_process = swow_fork_process_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_LONG(s_process->pid);
}

#define arginfo_class_Swow_Process_ForkProcess_wait arginfo_class_Swow_Process_ProcessInterface_wait

static PHP_METHOD(Swow_Process_ForkProcess, wait)
{
    swow_fork_process_t *s_process;
    zend_long timeout = -1;
    int wstatus;
    cat_pid_t wait_pid;
    swow_process_exit_status_t *s_exit_status;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    s_process = swow_fork_process_get_from_object(Z_OBJ_P(ZEND_THIS));

    if (s_process->exited) {
        swow_throw_exception(swow_process_exception_ce, 0, "Process has already exited");
        RETURN_THROWS();
    }

    if (timeout < 0) {
        wait_pid = cat_os_waitpid(s_process->pid, &wstatus, 0);
    } else {
        wait_pid = cat_os_waitpid_ex(s_process->pid, &wstatus, 0, timeout);
    }

    if (wait_pid <= 0) {
        swow_throw_exception_with_last(swow_process_exception_ce);
        RETURN_THROWS();
    }

    s_process->status = wstatus;
    s_process->exited = cat_true;

    /* 返回 ProcessExitStatus 对象 */
    object_init_ex(return_value, swow_process_exit_status_ce);
    s_exit_status = swow_process_exit_status_get_from_object(Z_OBJ_P(return_value));
    s_exit_status->status = wstatus;
}

#define arginfo_class_Swow_Process_ForkProcess_hasExited arginfo_class_Swow_Process_ProcessInterface_hasExited

static PHP_METHOD(Swow_Process_ForkProcess, hasExited)
{
    swow_fork_process_t *s_process;

    ZEND_PARSE_PARAMETERS_NONE();

    s_process = swow_fork_process_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_BOOL(s_process->exited);
}

#define arginfo_class_Swow_Process_ForkProcess_kill arginfo_class_Swow_Process_ProcessInterface_kill

static PHP_METHOD(Swow_Process_ForkProcess, kill)
{
    swow_fork_process_t *s_process;
    zend_long signal = SIGTERM;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(signal)
    ZEND_PARSE_PARAMETERS_END();

    s_process = swow_fork_process_get_from_object(Z_OBJ_P(ZEND_THIS));

    if (s_process->exited) {
        swow_throw_exception(swow_process_exception_ce, 0, "Process has already exited");
        RETURN_THROWS();
    }

    ret = cat_kill(s_process->pid, signal);

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_process_exception_ce);
        RETURN_THROWS();
    }
}

static const zend_function_entry swow_fork_process_methods[] = {
    PHP_ME(Swow_Process_ForkProcess, fork,      arginfo_class_Swow_Process_ForkProcess_fork,      ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Process_ForkProcess, getPid,    arginfo_class_Swow_Process_ForkProcess_getPid,    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ForkProcess, wait,      arginfo_class_Swow_Process_ForkProcess_wait,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ForkProcess, hasExited, arginfo_class_Swow_Process_ForkProcess_hasExited, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ForkProcess, kill,      arginfo_class_Swow_Process_ForkProcess_kill,      ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* }}} */

/* {{{ ProcessExitStatus */

static zend_object *swow_process_exit_status_create_object(zend_class_entry *ce)
{
    swow_process_exit_status_t *s_status = swow_object_alloc(swow_process_exit_status_t, ce, swow_process_exit_status_handlers);

    s_status->status = 0;

    return &s_status->std;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ProcessExitStatus_getExitCode, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ProcessExitStatus, getExitCode)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    if (WIFEXITED(s_status->status)) {
        RETURN_LONG(WEXITSTATUS(s_status->status));
    }
    RETURN_LONG(-1);
}

#define arginfo_class_Swow_Process_ProcessExitStatus_getTermSignal arginfo_class_Swow_Process_ProcessExitStatus_getExitCode

static PHP_METHOD(Swow_Process_ProcessExitStatus, getTermSignal)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    if (WIFSIGNALED(s_status->status)) {
        RETURN_LONG(WTERMSIG(s_status->status));
    }
    RETURN_LONG(0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ProcessExitStatus_isExited, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ProcessExitStatus, isExited)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_BOOL(WIFEXITED(s_status->status));
}

#define arginfo_class_Swow_Process_ProcessExitStatus_isSignaled arginfo_class_Swow_Process_ProcessExitStatus_isExited

static PHP_METHOD(Swow_Process_ProcessExitStatus, isSignaled)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_BOOL(WIFSIGNALED(s_status->status));
}

#define arginfo_class_Swow_Process_ProcessExitStatus_isStopped arginfo_class_Swow_Process_ProcessExitStatus_isExited

static PHP_METHOD(Swow_Process_ProcessExitStatus, isStopped)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_BOOL(WIFSTOPPED(s_status->status));
}

static const zend_function_entry swow_process_exit_status_methods[] = {
    PHP_ME(Swow_Process_ProcessExitStatus, getExitCode,   arginfo_class_Swow_Process_ProcessExitStatus_getExitCode,   ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ProcessExitStatus, getTermSignal, arginfo_class_Swow_Process_ProcessExitStatus_getTermSignal, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ProcessExitStatus, isExited,      arginfo_class_Swow_Process_ProcessExitStatus_isExited,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ProcessExitStatus, isSignaled,    arginfo_class_Swow_Process_ProcessExitStatus_isSignaled,    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ProcessExitStatus, isStopped,     arginfo_class_Swow_Process_ProcessExitStatus_isStopped,     ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* }}} */

/* {{{ module init */

zend_result swow_process_module_init(INIT_FUNC_ARGS)
{
    /* ProcessInterface */
    swow_process_interface_ce = swow_register_internal_interface(
        "Swow\\Process\\ProcessInterface",
        swow_process_interface_methods,
        NULL
    );

    /* ForkProcess */
    swow_fork_process_ce = swow_register_internal_class(
        "Swow\\Process\\ForkProcess", NULL, swow_fork_process_methods,
        &swow_fork_process_handlers, NULL,
        cat_false, cat_false,
        swow_fork_process_create_object, NULL,
        XtOffsetOf(swow_fork_process_t, std)
    );
    zend_class_implements(swow_fork_process_ce, 1, swow_process_interface_ce);

    /* ProcessExitStatus */
    swow_process_exit_status_ce = swow_register_internal_class(
        "Swow\\Process\\ProcessExitStatus", NULL, swow_process_exit_status_methods,
        &swow_process_exit_status_handlers, NULL,
        cat_false, cat_false,
        swow_process_exit_status_create_object, NULL,
        XtOffsetOf(swow_process_exit_status_t, std)
    );

    /* ProcessException */
    swow_process_exception_ce = swow_register_internal_class(
        "Swow\\Process\\ProcessException", swow_exception_ce, NULL,
        NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    return SUCCESS;
}

/* }}} */

#endif /* CAT_OS_WAIT */
