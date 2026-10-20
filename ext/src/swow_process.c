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
#include "swow_debug.h"

#include "cat_signal.h"

#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

extern char **environ;

SWOW_API zend_class_entry *swow_process_interface_ce;

SWOW_API zend_class_entry *swow_child_process_ce;
SWOW_API zend_object_handlers swow_child_process_handlers;

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

/* {{{ ChildProcess */

static zend_object *swow_child_process_create_object(zend_class_entry *ce)
{
    swow_child_process_t *s_process = swow_object_alloc(swow_child_process_t, ce, swow_child_process_handlers);

    s_process->pid = -1;
    s_process->status = 0;
    s_process->exited = cat_false;
    for (int i = 0; i < 3; i++) {
        ZVAL_UNDEF(&s_process->streams[i]);
    }

    return &s_process->std;
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Process_ChildProcess_fork, 0, 1, Swow\\Process\\ChildProcess, 0)
    ZEND_ARG_OBJ_INFO(0, callback, Closure, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ChildProcess, fork)
{
    zval *zcallback;
    zend_fcall_info fci;
    zend_fcall_info_cache fcc;
    cat_pid_t pid;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(zcallback, zend_ce_closure)
    ZEND_PARSE_PARAMETERS_END();

    bool restart_debug_sampler = swow_debug_sampler_stop();
    pid = cat_os_fork();
    if (restart_debug_sampler && !swow_debug_sampler_start()) {
        RETURN_THROWS();
    }
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

    /* 父进程：返回 ChildProcess 对象 */
    object_init_ex(return_value, swow_child_process_ce);
    swow_child_process_t *s_process = swow_child_process_get_from_object(Z_OBJ_P(return_value));
    s_process->pid = pid;
}


ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Process_ChildProcess_spawn, 0, 1, Swow\\Process\\ChildProcess, 0)
    ZEND_ARG_TYPE_INFO(0, command, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

typedef enum swow_child_stdio_mode_e {
    SWOW_CHILD_STDIO_INHERIT = 0,
    SWOW_CHILD_STDIO_PIPE,
    SWOW_CHILD_STDIO_FILE,
    SWOW_CHILD_STDIO_RESOURCE,
} swow_child_stdio_mode_t;

typedef struct swow_child_stdio_s {
    swow_child_stdio_mode_t mode;
    int fd;
    int pipe_fds[2];
} swow_child_stdio_t;

static bool swow_child_process_parse_stdio(zval *value, int target_fd, swow_child_stdio_t *stdio)
{
    stdio->mode = SWOW_CHILD_STDIO_INHERIT;
    stdio->fd = -1;
    stdio->pipe_fds[0] = stdio->pipe_fds[1] = -1;

    if (value == NULL) {
        return true;
    }

    if (Z_TYPE_P(value) == IS_STRING) {
        if (zend_string_equals_literal(Z_STR_P(value), "inherit")) {
            return true;
        }
        if (zend_string_equals_literal(Z_STR_P(value), "pipe")) {
            stdio->mode = SWOW_CHILD_STDIO_PIPE;
            return true;
        }
        return false;
    }

    if (Z_TYPE_P(value) == IS_RESOURCE) {
        php_stream *stream;
        int fd;
        stream = (php_stream *) zend_fetch_resource2_ex(value, "stream", php_file_le_stream(), php_file_le_pstream());
        if (stream == NULL) {
            return false;
        }
        if (php_stream_cast(stream, PHP_STREAM_AS_FD, (void **) &fd, 1) != SUCCESS) {
            return false;
        }
        stdio->mode = SWOW_CHILD_STDIO_RESOURCE;
        stdio->fd = fd;
        return true;
    }

    if (Z_TYPE_P(value) == IS_ARRAY) {
        zval *kind = zend_hash_index_find(Z_ARRVAL_P(value), 0);
        zval *path = zend_hash_index_find(Z_ARRVAL_P(value), 1);
        zval *mode = zend_hash_index_find(Z_ARRVAL_P(value), 2);
        const char *mode_str = mode != NULL && Z_TYPE_P(mode) == IS_STRING
            ? Z_STRVAL_P(mode)
            : (target_fd == 0 ? "r" : "w");
        int flags;

        if (kind == NULL || Z_TYPE_P(kind) != IS_STRING || !zend_string_equals_literal(Z_STR_P(kind), "file")
            || path == NULL || Z_TYPE_P(path) != IS_STRING) {
            return false;
        }

        switch (mode_str[0]) {
            case 'r':
                flags = mode_str[1] == '+' ? O_RDWR : O_RDONLY;
                break;
            case 'w':
                flags = (mode_str[1] == '+' ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
                break;
            case 'a':
                flags = (mode_str[1] == '+' ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
                break;
            default:
                return false;
        }

        stdio->fd = open(Z_STRVAL_P(path), flags, 0666);
        if (stdio->fd < 0) {
            return false;
        }
        stdio->mode = SWOW_CHILD_STDIO_FILE;
        return true;
    }

    return false;
}


static char *swow_child_process_resolve_executable(const char *file, char **envp)
{
    const char *path = NULL;
    const char *start;
    size_t file_len;

    if (strchr(file, '/') != NULL) {
        return estrdup(file);
    }

    if (envp != NULL) {
        for (char **p = envp; *p != NULL; p++) {
            if (strncmp(*p, "PATH=", sizeof("PATH=") - 1) == 0) {
                path = *p + sizeof("PATH=") - 1;
                break;
            }
        }
    }
    if (path == NULL) {
        path = getenv("PATH");
    }
    if (path == NULL) {
        return NULL;
    }

    file_len = strlen(file);
    start = path;
    for (;;) {
        const char *end = strchr(start, ':');
        size_t dir_len = end != NULL ? (size_t) (end - start) : strlen(start);
        size_t size = dir_len + 1 + file_len + 1;
        char *candidate = emalloc(size);
        if (dir_len == 0) {
            snprintf(candidate, size, "%s", file);
        } else {
            snprintf(candidate, size, "%.*s/%s", (int) dir_len, start, file);
        }
        if (access(candidate, X_OK) == 0) {
            return candidate;
        }
        efree(candidate);
        if (end == NULL) {
            break;
        }
        start = end + 1;
    }

    return NULL;
}

static void swow_child_process_free_argv(char **argv)
{
    if (argv == NULL) {
        return;
    }
    for (char **p = argv; *p != NULL; p++) {
        efree(*p);
    }
    efree(argv);
}

static PHP_METHOD(Swow_Process_ChildProcess, spawn)
{
    zval *zcommand;
    zval *zoptions = NULL;
    zend_string *cwd = NULL;
    char **argv = NULL;
    char **envp = NULL;
    uint32_t argc;
    uint32_t i;
    zval *value;
    swow_child_stdio_t stdio[3];
    int errpipe[2] = { -1, -1 };
    pid_t pid;
    bool restart_debug_sampler;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ARRAY(zcommand)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(zoptions)
    ZEND_PARSE_PARAMETERS_END();

    argc = zend_hash_num_elements(Z_ARRVAL_P(zcommand));
    if (argc == 0) {
        swow_throw_exception(swow_process_exception_ce, 0, "Command must not be empty");
        RETURN_THROWS();
    }

    argv = ecalloc(argc + 1, sizeof(char *));
    i = 0;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(zcommand), value) {
        if (Z_TYPE_P(value) != IS_STRING) {
            swow_child_process_free_argv(argv);
            zend_type_error("ChildProcess::spawn(): Argument #1 ($command) must contain only strings");
            RETURN_THROWS();
        }
        argv[i++] = estrdup(Z_STRVAL_P(value));
    } ZEND_HASH_FOREACH_END();
    argv[i] = NULL;

    for (i = 0; i < 3; i++) {
        stdio[i].mode = SWOW_CHILD_STDIO_INHERIT;
        stdio[i].fd = -1;
        stdio[i].pipe_fds[0] = stdio[i].pipe_fds[1] = -1;
    }

    if (zoptions != NULL) {
        if ((value = zend_hash_str_find(Z_ARRVAL_P(zoptions), "cwd", sizeof("cwd") - 1)) != NULL) {
            if (Z_TYPE_P(value) != IS_STRING) {
                swow_child_process_free_argv(argv);
                swow_throw_exception(swow_process_exception_ce, 0, "cwd must be a string");
                RETURN_THROWS();
            }
            cwd = Z_STR_P(value);
        }
        if ((value = zend_hash_str_find(Z_ARRVAL_P(zoptions), "env", sizeof("env") - 1)) != NULL) {
            uint32_t env_count;
            zend_string *key;
            zval *env_value;
            uint32_t env_index = 0;
            if (Z_TYPE_P(value) != IS_ARRAY) {
                swow_child_process_free_argv(argv);
                swow_throw_exception(swow_process_exception_ce, 0, "env must be an array");
                RETURN_THROWS();
            }
            env_count = zend_hash_num_elements(Z_ARRVAL_P(value));
            envp = ecalloc(env_count + 1, sizeof(char *));
            ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(value), key, env_value) {
                zend_string *entry;
                if (key == NULL || Z_TYPE_P(env_value) != IS_STRING) {
                    swow_child_process_free_argv(argv);
                    for (uint32_t j = 0; j < env_index; j++) {
                        efree(envp[j]);
                    }
                    efree(envp);
                    swow_throw_exception(swow_process_exception_ce, 0, "env must contain string keys and values");
                    RETURN_THROWS();
                }
                entry = strpprintf(0, "%s=%s", ZSTR_VAL(key), Z_STRVAL_P(env_value));
                envp[env_index++] = estrdup(ZSTR_VAL(entry));
                zend_string_release(entry);
            } ZEND_HASH_FOREACH_END();
            envp[env_index] = NULL;
        }
        {
            static const char *names[3] = { "stdin", "stdout", "stderr" };
            for (i = 0; i < 3; i++) {
                value = zend_hash_str_find(Z_ARRVAL_P(zoptions), names[i], strlen(names[i]));
                if (value != NULL && !swow_child_process_parse_stdio(value, (int) i, &stdio[i])) {
                    swow_child_process_free_argv(argv);
                    if (envp != NULL) {
                        for (char **p = envp; *p != NULL; p++) {
                            efree(*p);
                        }
                        efree(envp);
                    }
                    swow_throw_exception(swow_process_exception_ce, 0, "Invalid stdio option");
                    RETURN_THROWS();
                }
            }
        }
    }

    if (pipe(errpipe) < 0) {
        swow_child_process_free_argv(argv);
        if (envp != NULL) {
            for (char **p = envp; *p != NULL; p++) {
                efree(*p);
            }
            efree(envp);
        }
        swow_throw_exception_with_last(swow_process_exception_ce);
        RETURN_THROWS();
    }
    (void) fcntl(errpipe[1], F_SETFD, FD_CLOEXEC);

    for (i = 0; i < 3; i++) {
        if (stdio[i].mode == SWOW_CHILD_STDIO_PIPE && pipe(stdio[i].pipe_fds) < 0) {
            goto spawn_error;
        }
    }

    restart_debug_sampler = swow_debug_sampler_stop();
    pid = fork();
    if (restart_debug_sampler && !swow_debug_sampler_start()) {
        goto spawn_error;
    }
    if (pid < 0) {
        goto spawn_error;
    }

    if (pid == 0) {
        int child_errno = 0;

        close(errpipe[0]);
        for (i = 0; i < 3; i++) {
            switch (stdio[i].mode) {
                case SWOW_CHILD_STDIO_PIPE:
                {
                    int child_fd = i == 0 ? stdio[i].pipe_fds[0] : stdio[i].pipe_fds[1];
                    if (dup2(child_fd, (int) i) < 0) {
                        child_errno = errno;
                        goto child_error;
                    }
                    break;
                }
                case SWOW_CHILD_STDIO_FILE:
                case SWOW_CHILD_STDIO_RESOURCE:
                    if (dup2(stdio[i].fd, (int) i) < 0) {
                        child_errno = errno;
                        goto child_error;
                    }
                    break;
                case SWOW_CHILD_STDIO_INHERIT:
                    break;
            }
        }
        for (i = 0; i < 3; i++) {
            if (stdio[i].mode == SWOW_CHILD_STDIO_PIPE) {
                close(stdio[i].pipe_fds[0]);
                close(stdio[i].pipe_fds[1]);
            } else if (stdio[i].mode == SWOW_CHILD_STDIO_FILE) {
                close(stdio[i].fd);
            } else if (stdio[i].mode == SWOW_CHILD_STDIO_RESOURCE && stdio[i].fd > 2) {
                close(stdio[i].fd);
            }
        }
        if (cwd != NULL && chdir(ZSTR_VAL(cwd)) != 0) {
            child_errno = errno;
            goto child_error;
        }
        if (envp != NULL) {
            char *executable = swow_child_process_resolve_executable(argv[0], envp);
            environ = envp;
            if (executable != NULL) {
                execve(executable, argv, envp);
                efree(executable);
            } else {
                execvp(argv[0], argv);
            }
        } else {
            execvp(argv[0], argv);
        }
        child_errno = errno;
child_error:
        (void) write(errpipe[1], &child_errno, sizeof(child_errno));
        _exit(127);
    }

    close(errpipe[1]);
    errpipe[1] = -1;
    for (i = 0; i < 3; i++) {
        if (stdio[i].mode == SWOW_CHILD_STDIO_PIPE) {
            close(i == 0 ? stdio[i].pipe_fds[0] : stdio[i].pipe_fds[1]);
            stdio[i].pipe_fds[i == 0 ? 0 : 1] = -1;
        } else if (stdio[i].mode == SWOW_CHILD_STDIO_FILE) {
            close(stdio[i].fd);
            stdio[i].fd = -1;
        }
    }

    {
        int child_errno = 0;
        ssize_t n;
        do {
            n = read(errpipe[0], &child_errno, sizeof(child_errno));
        } while (n < 0 && errno == EINTR);
        close(errpipe[0]);
        errpipe[0] = -1;
        if (n > 0) {
            int status;
            (void) waitpid(pid, &status, 0);
            swow_child_process_free_argv(argv);
            if (envp != NULL) {
                for (char **p = envp; *p != NULL; p++) {
                    efree(*p);
                }
                efree(envp);
            }
            for (i = 0; i < 3; i++) {
                if (stdio[i].mode == SWOW_CHILD_STDIO_PIPE) {
                    close(i == 0 ? stdio[i].pipe_fds[1] : stdio[i].pipe_fds[0]);
                }
            }
            swow_throw_exception(swow_process_exception_ce, 0, "Failed to execute child process");
            RETURN_THROWS();
        }
    }

    object_init_ex(return_value, swow_child_process_ce);
    {
        swow_child_process_t *s_process = swow_child_process_get_from_object(Z_OBJ_P(return_value));
        s_process->pid = pid;
        for (i = 0; i < 3; i++) {
            if (stdio[i].mode == SWOW_CHILD_STDIO_PIPE) {
                int parent_fd = i == 0 ? stdio[i].pipe_fds[1] : stdio[i].pipe_fds[0];
                php_stream *stream = php_stream_fopen_from_fd(parent_fd, i == 0 ? "w" : "r", NULL);
                if (stream == NULL) {
                    close(parent_fd);
                } else {
                    php_stream_to_zval(stream, &s_process->streams[i]);
                }
            }
        }
    }

    swow_child_process_free_argv(argv);
    if (envp != NULL) {
        for (char **p = envp; *p != NULL; p++) {
            efree(*p);
        }
        efree(envp);
    }
    return;

spawn_error:
    if (errpipe[0] >= 0) {
        close(errpipe[0]);
    }
    if (errpipe[1] >= 0) {
        close(errpipe[1]);
    }
    for (i = 0; i < 3; i++) {
        if (stdio[i].mode == SWOW_CHILD_STDIO_PIPE) {
            if (stdio[i].pipe_fds[0] >= 0) {
                close(stdio[i].pipe_fds[0]);
            }
            if (stdio[i].pipe_fds[1] >= 0) {
                close(stdio[i].pipe_fds[1]);
            }
        } else if (stdio[i].mode == SWOW_CHILD_STDIO_FILE && stdio[i].fd >= 0) {
            close(stdio[i].fd);
        }
    }
    swow_child_process_free_argv(argv);
    if (envp != NULL) {
        for (char **p = envp; *p != NULL; p++) {
            efree(*p);
        }
        efree(envp);
    }
    swow_throw_exception_with_last(swow_process_exception_ce);
    RETURN_THROWS();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ChildProcess_getPid, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ChildProcess, getPid)
{
    swow_child_process_t *s_process;

    ZEND_PARSE_PARAMETERS_NONE();

    s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_LONG(s_process->pid);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Process_ChildProcess_wait, 0, 0, Swow\\Process\\ProcessExitStatus, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ChildProcess, wait)
{
    swow_child_process_t *s_process;
    zend_long timeout = -1;
    int wstatus;
    cat_pid_t wait_pid;
    swow_process_exit_status_t *s_exit_status;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS));

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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ChildProcess_hasExited, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ChildProcess, hasExited)
{
    swow_child_process_t *s_process;

    ZEND_PARSE_PARAMETERS_NONE();

    s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_BOOL(s_process->exited);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ChildProcess_kill, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, signal, IS_LONG, 0, "\\Swow\\Process\\SIGTERM")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ChildProcess, kill)
{
    swow_child_process_t *s_process;
    zend_long signal = SIGTERM;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(signal)
    ZEND_PARSE_PARAMETERS_END();

    s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS));

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


static void swow_child_process_close_streams(swow_child_process_t *s_process)
{
    for (int i = 0; i < 3; i++) {
        if (!Z_ISUNDEF(s_process->streams[i])) {
            zval_ptr_dtor(&s_process->streams[i]);
            ZVAL_UNDEF(&s_process->streams[i]);
        }
    }
}

#define SWOW_CHILD_PROCESS_STREAM_GETTER(name, index) \
    ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Process_ChildProcess_##name, 0, 0, 0) \
    ZEND_END_ARG_INFO() \
    static PHP_METHOD(Swow_Process_ChildProcess, name) \
    { \
        swow_child_process_t *s_process; \
        ZEND_PARSE_PARAMETERS_NONE(); \
        s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS)); \
        if (Z_ISUNDEF(s_process->streams[index])) { \
            RETURN_NULL(); \
        } \
        RETURN_COPY(&s_process->streams[index]); \
    }

SWOW_CHILD_PROCESS_STREAM_GETTER(getStdin, 0)
SWOW_CHILD_PROCESS_STREAM_GETTER(getStdout, 1)
SWOW_CHILD_PROCESS_STREAM_GETTER(getStderr, 2)

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Process_ChildProcess_close, 0, 0, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ChildProcess, close)
{
    swow_child_process_t *s_process;
    ZEND_PARSE_PARAMETERS_NONE();
    s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS));
    swow_child_process_close_streams(s_process);
}

static PHP_METHOD(Swow_Process_ChildProcess, __destruct)
{
    swow_child_process_t *s_process;
    ZEND_PARSE_PARAMETERS_NONE();
    s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS));
    swow_child_process_close_streams(s_process);
}

static const zend_function_entry swow_child_process_methods[] = {
    PHP_ME(Swow_Process_ChildProcess, fork,      arginfo_class_Swow_Process_ChildProcess_fork,      ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Process_ChildProcess, spawn,     arginfo_class_Swow_Process_ChildProcess_spawn,     ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Swow_Process_ChildProcess, getPid,    arginfo_class_Swow_Process_ChildProcess_getPid,    ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ChildProcess, wait,      arginfo_class_Swow_Process_ChildProcess_wait,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ChildProcess, hasExited, arginfo_class_Swow_Process_ChildProcess_hasExited, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ChildProcess, getStdin,  arginfo_class_Swow_Process_ChildProcess_getStdin,  ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ChildProcess, getStdout, arginfo_class_Swow_Process_ChildProcess_getStdout, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ChildProcess, getStderr, arginfo_class_Swow_Process_ChildProcess_getStderr, ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ChildProcess, kill,      arginfo_class_Swow_Process_ChildProcess_kill,      ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ChildProcess, close,     arginfo_class_Swow_Process_ChildProcess_close,     ZEND_ACC_PUBLIC)
    PHP_ME(Swow_Process_ChildProcess, __destruct, arginfo_class_Swow_Process_ChildProcess_close,    ZEND_ACC_PUBLIC)
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

#define arginfo_class_Swow_Process_ProcessExitStatus_getExitCode arginfo_class_Swow_Process_ChildProcess_getPid

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

#define arginfo_class_Swow_Process_ProcessExitStatus_getTermSignal arginfo_class_Swow_Process_ChildProcess_getPid

static PHP_METHOD(Swow_Process_ProcessExitStatus, getTermSignal)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    if (WIFSIGNALED(s_status->status)) {
        RETURN_LONG(WTERMSIG(s_status->status));
    }
    RETURN_LONG(-1);
}

#define arginfo_class_Swow_Process_ProcessExitStatus_isExited arginfo_class_Swow_Process_ChildProcess_hasExited

static PHP_METHOD(Swow_Process_ProcessExitStatus, isExited)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_BOOL(WIFEXITED(s_status->status));
}

#define arginfo_class_Swow_Process_ProcessExitStatus_isSignaled arginfo_class_Swow_Process_ChildProcess_hasExited

static PHP_METHOD(Swow_Process_ProcessExitStatus, isSignaled)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_BOOL(WIFSIGNALED(s_status->status));
}

#define arginfo_class_Swow_Process_ProcessExitStatus_isStopped arginfo_class_Swow_Process_ChildProcess_hasExited

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


/* {{{ Swow\Process\kill */

ZEND_BEGIN_ARG_INFO_EX(arginfo_swow_process_kill, 0, 0, 2)
    ZEND_ARG_TYPE_INFO(0, pid, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, signal, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_FUNCTION(kill)
{
    zend_long pid;
    zend_long signal;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(pid)
        Z_PARAM_LONG(signal)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(!cat_kill((cat_pid_t) pid, (int) signal))) {
        swow_throw_exception_with_last(swow_process_exception_ce);
        RETURN_THROWS();
    }
}

static const zend_function_entry swow_process_functions[] = {
    ZEND_NS_FE("Swow\\Process", kill, arginfo_swow_process_kill)
    ZEND_FE_END
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

    /* ChildProcess */
    swow_child_process_ce = swow_register_internal_class(
        "Swow\\Process\\ChildProcess", NULL, swow_child_process_methods,
        &swow_child_process_handlers, NULL,
        cat_false, cat_false,
        swow_child_process_create_object, NULL,
        offsetof(swow_child_process_t, std)
    );
    zend_class_implements(swow_child_process_ce, 1, swow_process_interface_ce);

    /* ProcessExitStatus */
    swow_process_exit_status_ce = swow_register_internal_class(
        "Swow\\Process\\ProcessExitStatus", NULL, swow_process_exit_status_methods,
        &swow_process_exit_status_handlers, NULL,
        cat_false, cat_false,
        swow_process_exit_status_create_object, NULL,
        offsetof(swow_process_exit_status_t, std)
    );

    /* ProcessException */
    swow_process_exception_ce = swow_register_internal_class(
        "Swow\\Process\\ProcessException", swow_exception_ce, NULL,
        NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

    REGISTER_LONG_CONSTANT("Swow\\Process\\SIGINT", SIGINT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("Swow\\Process\\SIGKILL", SIGKILL, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("Swow\\Process\\SIGTERM", SIGTERM, CONST_PERSISTENT);

    if (zend_register_functions(NULL, swow_process_functions, NULL, MODULE_PERSISTENT) != SUCCESS) {
        return FAILURE;
    }

    return SUCCESS;
}

/* }}} */

#endif /* CAT_OS_WAIT */
