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

#include "cat_signal.h"

#include "main/php_network.h"

#include <limits.h>
#include <stdint.h>

#ifdef CAT_OS_UNIX_LIKE

# include "swow_closure.h"
# include "swow_debug.h"

# include <sys/wait.h>
# include <fcntl.h>
# include <signal.h>
# include <unistd.h>
# include <errno.h>
# include <string.h>

extern char **environ;

# define SWOW_PROCESS_DEFAULT_SIGNAL_NAME "\\Swow\\Process\\SIGTERM"
# define SWOW_PROCESS_DEFAULT_SIGNAL SIGTERM

#elif defined(CAT_OS_WIN)

# include <fcntl.h>
# include <io.h>
# include <sys/stat.h>
# include <windows.h>

# ifndef CAT_SIGKILL
#  define CAT_SIGKILL 9
# endif

# define SWOW_PROCESS_DEFAULT_SIGNAL_NAME "\\Swow\\Process\\SIGINT"
# define SWOW_PROCESS_DEFAULT_SIGNAL CAT_SIGINT

#else
# error "Unsupported platform"
#endif

SWOW_API zend_class_entry *swow_process_interface_ce;

SWOW_API zend_class_entry *swow_child_process_ce;
SWOW_API zend_object_handlers swow_child_process_handlers;

SWOW_API zend_class_entry *swow_process_exit_status_ce;
SWOW_API zend_object_handlers swow_process_exit_status_handlers;

SWOW_API zend_class_entry *swow_process_exception_ce;

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

/* platform helpers */

#ifdef CAT_OS_UNIX_LIKE

static int swow_child_process_open_file(const char *path, const char *mode, int target_fd)
{
    int flags;

    switch (mode[0]) {
        case 'r':
            flags = mode[1] == '+' ? O_RDWR : O_RDONLY;
            break;
        case 'w':
            flags = (mode[1] == '+' ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
            break;
        case 'a':
            flags = (mode[1] == '+' ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
            break;
        default:
            return -1;
    }

    return open(path, flags, 0666);
}

static int swow_child_process_make_pipe(int pipe_fds[2], int child_index, int parent_index)
{
    if (pipe(pipe_fds) < 0) {
        return -1;
    }

    (void) fcntl(pipe_fds[parent_index], F_SETFD, FD_CLOEXEC);

    return 0;
}

static void swow_child_process_close_fd(int fd)
{
    close(fd);
}

#else

static int swow_child_process_open_file(const char *path, const char *mode, int target_fd)
{
    int flags = _O_BINARY;

    switch (mode[0]) {
        case 'r':
            flags |= mode[1] == '+' ? _O_RDWR : _O_RDONLY;
            break;
        case 'w':
            flags |= (mode[1] == '+' ? _O_RDWR : _O_WRONLY) | _O_CREAT | _O_TRUNC;
            break;
        case 'a':
            flags |= (mode[1] == '+' ? _O_RDWR : _O_WRONLY) | _O_CREAT | _O_APPEND;
            break;
        default:
            return -1;
    }

    return _open(path, flags, _S_IREAD | _S_IWRITE);
}

static int swow_child_process_make_pipe(int pipe_fds[2], int child_index, int parent_index)
{
    if (_pipe(pipe_fds, 4096, _O_BINARY) < 0) {
        return -1;
    }

    HANDLE child_handle = (HANDLE) _get_osfhandle(pipe_fds[child_index]);
    HANDLE parent_handle = (HANDLE) _get_osfhandle(pipe_fds[parent_index]);
    if (child_handle != INVALID_HANDLE_VALUE) {
        SetHandleInformation(child_handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    }
    if (parent_handle != INVALID_HANDLE_VALUE) {
        SetHandleInformation(parent_handle, HANDLE_FLAG_INHERIT, 0);
    }

    return 0;
}

static void swow_child_process_close_fd(int fd)
{
    _close(fd);
}

#endif

static zend_object *swow_child_process_create_object(zend_class_entry *ce)
{
    swow_child_process_t *s_process = swow_object_alloc(swow_child_process_t, ce, swow_child_process_handlers);

    s_process->process = NULL;
    s_process->pid = -1;
    s_process->raw_status = 0;
    s_process->exited = cat_false;
    s_process->closed = cat_false;
    s_process->waited = cat_false;
    for (int i = 0; i < 3; i++) {
        ZVAL_UNDEF(&s_process->streams[i]);
    }

    return &s_process->std;
}

static zend_object *swow_process_exit_status_create_object(zend_class_entry *ce)
{
    swow_process_exit_status_t *s_status = swow_object_alloc(swow_process_exit_status_t, ce, swow_process_exit_status_handlers);

    s_status->exit_code = -1;
    s_status->term_signal = -1;
    s_status->exited = cat_false;
    s_status->signaled = cat_false;
    s_status->stopped = cat_false;

    return &s_status->std;
}

static cat_bool_t swow_child_process_check_open(swow_child_process_t *s_process)
{
    if (UNEXPECTED(s_process->closed)) {
        swow_throw_exception(swow_process_exception_ce, 0, "Process has been closed");
        return cat_false;
    }

    return cat_true;
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

static void swow_child_process_free_envp(char **envp)
{
    if (envp == NULL) {
        return;
    }
    for (char **p = envp; *p != NULL; p++) {
        efree(*p);
    }
    efree(envp);
}

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
        php_socket_t stream_fd = (php_socket_t) -1;
        stream = (php_stream *) zend_fetch_resource2_ex(value, "stream", php_file_le_stream(), php_file_le_pstream());
        if (stream == NULL) {
            return false;
        }
        /* php_stream_cast() writes php_socket_t; Windows SOCKET is 64-bit. */
        if (php_stream_cast(stream, PHP_STREAM_AS_FD, (void **) &stream_fd, 1) != SUCCESS) {
            return false;
        }
        /* stream_fd is int on Unix and an unsigned 64-bit SOCKET on Windows. */
        if ((uintmax_t) stream_fd > INT_MAX) {
            return false;
        }
        stdio->mode = SWOW_CHILD_STDIO_RESOURCE;
        stdio->fd = (int) stream_fd;
        return true;
    }

    if (Z_TYPE_P(value) == IS_ARRAY) {
        zval *kind = zend_hash_index_find(Z_ARRVAL_P(value), 0);
        zval *path = zend_hash_index_find(Z_ARRVAL_P(value), 1);
        zval *mode = zend_hash_index_find(Z_ARRVAL_P(value), 2);
        const char *mode_str = mode != NULL && Z_TYPE_P(mode) == IS_STRING
            ? Z_STRVAL_P(mode)
            : (target_fd == 0 ? "r" : "w");

        if (kind == NULL || Z_TYPE_P(kind) != IS_STRING || !zend_string_equals_literal(Z_STR_P(kind), "file")
            || path == NULL || Z_TYPE_P(path) != IS_STRING) {
            return false;
        }

        stdio->fd = swow_child_process_open_file(Z_STRVAL_P(path), mode_str, target_fd);
        if (stdio->fd < 0) {
            return false;
        }
        stdio->mode = SWOW_CHILD_STDIO_FILE;
        return true;
    }

    return false;
}

#define SWOW_CHILD_PROCESS_MAX_FD 256

static bool swow_child_process_parse_fd(zval *value, int *fd)
{
    if (Z_TYPE_P(value) == IS_LONG) {
        if (Z_LVAL_P(value) < 0) {
            return false;
        }
        *fd = (int) Z_LVAL_P(value);
        return true;
    }

    if (Z_TYPE_P(value) == IS_RESOURCE) {
        php_stream *stream;
        php_socket_t stream_fd = (php_socket_t) -1;
        stream = (php_stream *) zend_fetch_resource2_ex(value, "stream", php_file_le_stream(), php_file_le_pstream());
        if (stream == NULL) {
            return false;
        }
        /* php_stream_cast() writes php_socket_t; Windows SOCKET is 64-bit. */
        if (php_stream_cast(stream, PHP_STREAM_AS_FD, (void **) &stream_fd, 1) != SUCCESS) {
            return false;
        }
        /* stream_fd is int on Unix and an unsigned 64-bit SOCKET on Windows. */
        if ((uintmax_t) stream_fd > INT_MAX) {
            return false;
        }
        *fd = (int) stream_fd;
        return true;
    }

    return false;
}

static void swow_child_process_close_child_fds(swow_child_stdio_t stdio[3])
{
    for (int i = 0; i < 3; i++) {
        if (stdio[i].mode == SWOW_CHILD_STDIO_PIPE) {
            int child_index = i == 0 ? 0 : 1;
            if (stdio[i].pipe_fds[child_index] >= 0) {
                swow_child_process_close_fd(stdio[i].pipe_fds[child_index]);
                stdio[i].pipe_fds[child_index] = -1;
            }
        } else if (stdio[i].mode == SWOW_CHILD_STDIO_FILE && stdio[i].fd >= 0) {
            swow_child_process_close_fd(stdio[i].fd);
            stdio[i].fd = -1;
        }
    }
}

static void swow_child_process_close_spawn_fds(swow_child_stdio_t stdio[3])
{
    for (int i = 0; i < 3; i++) {
        if (stdio[i].mode == SWOW_CHILD_STDIO_PIPE) {
            if (stdio[i].pipe_fds[0] >= 0) {
                swow_child_process_close_fd(stdio[i].pipe_fds[0]);
                stdio[i].pipe_fds[0] = -1;
            }
            if (stdio[i].pipe_fds[1] >= 0) {
                swow_child_process_close_fd(stdio[i].pipe_fds[1]);
                stdio[i].pipe_fds[1] = -1;
            }
        } else if (stdio[i].mode == SWOW_CHILD_STDIO_FILE && stdio[i].fd >= 0) {
            swow_child_process_close_fd(stdio[i].fd);
            stdio[i].fd = -1;
        }
    }
}

/* {{{ ProcessInterface */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ProcessInterface_getPid, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Process_ProcessInterface_wait, 0, 0, Swow\\Process\\ProcessExitStatus, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ProcessInterface_hasExited, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ProcessInterface_kill, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, signal, IS_LONG, 0)
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

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Process_ChildProcess_fork, 0, 1, Swow\\Process\\ChildProcess, 0)
    ZEND_ARG_OBJ_INFO(0, callback, Closure, 0)
ZEND_END_ARG_INFO()

#ifdef CAT_OS_UNIX_LIKE

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

        _exit(exit_code);
    }

    object_init_ex(return_value, swow_child_process_ce);
    swow_child_process_t *s_process = swow_child_process_get_from_object(Z_OBJ_P(return_value));
    s_process->pid = pid;
}

#else

static PHP_METHOD(Swow_Process_ChildProcess, fork)
{
    zval *zcallback;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(zcallback, zend_ce_closure)
    ZEND_PARSE_PARAMETERS_END();

    swow_throw_exception(swow_process_exception_ce, 0, "ChildProcess::fork() is not supported on Windows");
    RETURN_THROWS();
}

#endif

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Process_ChildProcess_spawn, 0, 1, Swow\\Process\\ChildProcess, 0)
    ZEND_ARG_TYPE_INFO(0, command, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

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
    zval *zfds = NULL;
    swow_child_stdio_t stdio[3];
    cat_process_stdio_container_t *cat_stdio = NULL;
    cat_process_options_t options = { 0 };
    cat_process_t *process = NULL;
    int max_fd = 2;

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
                    swow_child_process_free_envp(envp);
                    swow_child_process_close_spawn_fds(stdio);
                    swow_throw_exception(swow_process_exception_ce, 0, "Invalid stdio option");
                    RETURN_THROWS();
                }
            }
        }
        zfds = zend_hash_str_find(Z_ARRVAL_P(zoptions), "fds", sizeof("fds") - 1);
        if (zfds != NULL) {
            zend_ulong child_fd;
            zend_string *fd_key;
            zval *fd_value;
            if (Z_TYPE_P(zfds) != IS_ARRAY) {
                swow_child_process_free_argv(argv);
                swow_child_process_free_envp(envp);
                swow_throw_exception(swow_process_exception_ce, 0, "fds must be an array");
                RETURN_THROWS();
            }
            ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(zfds), child_fd, fd_key, fd_value) {
                int fd;
                if (fd_key != NULL || child_fd < 3 || child_fd > SWOW_CHILD_PROCESS_MAX_FD) {
                    swow_child_process_free_argv(argv);
                    swow_child_process_free_envp(envp);
                    swow_throw_exception(swow_process_exception_ce, 0, "fds keys must be integers between 3 and %d", SWOW_CHILD_PROCESS_MAX_FD);
                    RETURN_THROWS();
                }
                if (!swow_child_process_parse_fd(fd_value, &fd)) {
                    swow_child_process_free_argv(argv);
                    swow_child_process_free_envp(envp);
                    swow_throw_exception(swow_process_exception_ce, 0, "fds values must be streams or file descriptors");
                    RETURN_THROWS();
                }
                if ((int) child_fd > max_fd) {
                    max_fd = (int) child_fd;
                }
            } ZEND_HASH_FOREACH_END();
        }
    }

    cat_stdio = ecalloc((size_t) max_fd + 1, sizeof(*cat_stdio));

    for (i = 0; i < 3; i++) {
        switch (stdio[i].mode) {
            case SWOW_CHILD_STDIO_INHERIT:
                cat_stdio[i].flags = CAT_PROCESS_STDIO_FLAG_INHERIT_FD;
                cat_stdio[i].data.fd = (int) i;
                break;
            case SWOW_CHILD_STDIO_PIPE:
            {
                int child_index = i == 0 ? 0 : 1;
                int parent_index = i == 0 ? 1 : 0;
                if (swow_child_process_make_pipe(stdio[i].pipe_fds, child_index, parent_index) < 0) {
                    goto spawn_error;
                }
                cat_stdio[i].flags = CAT_PROCESS_STDIO_FLAG_INHERIT_FD;
                cat_stdio[i].data.fd = stdio[i].pipe_fds[child_index];
                break;
            }
            case SWOW_CHILD_STDIO_FILE:
            case SWOW_CHILD_STDIO_RESOURCE:
                cat_stdio[i].flags = CAT_PROCESS_STDIO_FLAG_INHERIT_FD;
                cat_stdio[i].data.fd = stdio[i].fd;
                break;
        }
    }

    if (zfds != NULL) {
        zend_ulong child_fd;
        zend_string *fd_key;
        zval *fd_value;
        ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(zfds), child_fd, fd_key, fd_value) {
            int fd;
            (void) fd_key;
            (void) swow_child_process_parse_fd(fd_value, &fd);
            cat_stdio[child_fd].flags = CAT_PROCESS_STDIO_FLAG_INHERIT_FD;
            cat_stdio[child_fd].data.fd = fd;
        } ZEND_HASH_FOREACH_END();
    }

    options.file = argv[0];
    options.args = (const char **) argv;
    options.env = (const char **) envp;
    options.cwd = cwd != NULL ? ZSTR_VAL(cwd) : NULL;
#ifdef CAT_OS_WIN
    options.flags = CAT_PROCESS_FLAG_WINDOWS_NEW_PROCESS_GROUP;
#endif
    options.stdio_count = max_fd + 1;
    options.stdio = cat_stdio;

    process = cat_process_run(&options);
    if (process == NULL) {
        goto spawn_error;
    }

    swow_child_process_close_child_fds(stdio);

    object_init_ex(return_value, swow_child_process_ce);
    {
        swow_child_process_t *s_process = swow_child_process_get_from_object(Z_OBJ_P(return_value));
        s_process->process = process;
        s_process->pid = cat_process_get_pid(process);
        s_process->waited = cat_false;
        for (i = 0; i < 3; i++) {
            if (stdio[i].mode == SWOW_CHILD_STDIO_PIPE) {
                int parent_index = i == 0 ? 1 : 0;
                int parent_fd = stdio[i].pipe_fds[parent_index];
                php_stream *stream = php_stream_fopen_from_fd(parent_fd, i == 0 ? "w" : "r", NULL);
                if (stream == NULL) {
                    swow_child_process_close_fd(parent_fd);
                } else {
                    php_stream_to_zval(stream, &s_process->streams[i]);
                }
                stdio[i].pipe_fds[parent_index] = -1;
            }
        }
    }

    swow_child_process_free_argv(argv);
    swow_child_process_free_envp(envp);
    efree(cat_stdio);
    return;

spawn_error:
    swow_child_process_close_spawn_fds(stdio);
    swow_child_process_free_argv(argv);
    swow_child_process_free_envp(envp);
    if (cat_stdio != NULL) {
        efree(cat_stdio);
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

    if (!swow_child_process_check_open(s_process)) {
        RETURN_THROWS();
    }

    if (s_process->process != NULL) {
        RETURN_LONG(cat_process_get_pid(s_process->process));
    }

    RETURN_LONG(s_process->pid);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Swow_Process_ChildProcess_wait, 0, 0, Swow\\Process\\ProcessExitStatus, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ChildProcess, wait)
{
    swow_child_process_t *s_process;
    zend_long timeout = -1;
    swow_process_exit_status_t *s_exit_status;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS));

    if (!swow_child_process_check_open(s_process)) {
        RETURN_THROWS();
    }

    if (s_process->waited) {
        swow_throw_exception(swow_process_exception_ce, 0, "Process has already exited");
        RETURN_THROWS();
    }

    object_init_ex(return_value, swow_process_exit_status_ce);
    s_exit_status = swow_process_exit_status_get_from_object(Z_OBJ_P(return_value));
    s_exit_status->exit_code = -1;
    s_exit_status->term_signal = -1;
    s_exit_status->exited = cat_false;
    s_exit_status->signaled = cat_false;
    s_exit_status->stopped = cat_false;

    if (s_process->process != NULL) {
        cat_bool_t ret = timeout < 0
            ? cat_process_wait(s_process->process)
            : cat_process_wait_ex(s_process->process, (cat_timeout_t) timeout);
        if (UNEXPECTED(!ret)) {
            swow_throw_exception_with_last(swow_process_exception_ce);
            RETURN_THROWS();
        }
        cat_process_status_t status = cat_process_get_status(s_process->process);
        s_process->waited = cat_true;
        s_exit_status->exit_code = status.exit_code;
        s_exit_status->term_signal = status.term_signal;
        s_exit_status->exited = status.exited && !status.signaled;
        s_exit_status->signaled = status.signaled;
        s_exit_status->stopped = cat_false;
    } else {
#ifdef CAT_OS_UNIX_LIKE
        int raw_status;
        cat_pid_t wait_pid = timeout < 0
            ? cat_os_waitpid(s_process->pid, &raw_status, 0)
            : cat_os_waitpid_ex(s_process->pid, &raw_status, 0, timeout);
        if (wait_pid <= 0) {
            swow_throw_exception_with_last(swow_process_exception_ce);
            RETURN_THROWS();
        }
        s_process->raw_status = raw_status;
        s_process->exited = cat_true;
        s_process->waited = cat_true;
        s_exit_status->exit_code = cat_os_wait_status_exited(raw_status)
            ? cat_os_wait_status_exit_code(raw_status)
            : -1;
        s_exit_status->term_signal = cat_os_wait_status_signaled(raw_status)
            ? cat_os_wait_status_term_signal(raw_status)
            : -1;
        s_exit_status->exited = cat_os_wait_status_exited(raw_status);
        s_exit_status->signaled = cat_os_wait_status_signaled(raw_status);
        s_exit_status->stopped = WIFSTOPPED(raw_status);
#else
        swow_throw_exception(swow_process_exception_ce, 0, "ChildProcess::fork() is not supported on Windows");
        RETURN_THROWS();
#endif
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ChildProcess_hasExited, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ChildProcess, hasExited)
{
    swow_child_process_t *s_process;

    ZEND_PARSE_PARAMETERS_NONE();

    s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS));

    if (!swow_child_process_check_open(s_process)) {
        RETURN_THROWS();
    }

    if (s_process->process != NULL) {
        RETURN_BOOL(cat_process_has_exited(s_process->process));
    }

    RETURN_BOOL(s_process->exited);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ChildProcess_kill, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, signal, IS_LONG, 0, SWOW_PROCESS_DEFAULT_SIGNAL_NAME)
ZEND_END_ARG_INFO()

static PHP_METHOD(Swow_Process_ChildProcess, kill)
{
    swow_child_process_t *s_process;
    zend_long signal = SWOW_PROCESS_DEFAULT_SIGNAL;
    cat_bool_t ret;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(signal)
    ZEND_PARSE_PARAMETERS_END();

    s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS));

    if (!swow_child_process_check_open(s_process)) {
        RETURN_THROWS();
    }

    if (s_process->process != NULL) {
        if (cat_process_has_exited(s_process->process)) {
            swow_throw_exception(swow_process_exception_ce, 0, "Process has already exited");
            RETURN_THROWS();
        }
        ret = cat_process_kill(s_process->process, (int) signal);
    } else {
        if (s_process->exited) {
            swow_throw_exception(swow_process_exception_ce, 0, "Process has already exited");
            RETURN_THROWS();
        }
        ret = cat_kill_ex(s_process->pid, (int) signal);
    }

    if (UNEXPECTED(!ret)) {
        swow_throw_exception_with_last(swow_process_exception_ce);
        RETURN_THROWS();
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
        if (s_process->closed || Z_ISUNDEF(s_process->streams[index])) { \
            RETURN_NULL(); \
        } \
        RETURN_COPY(&s_process->streams[index]); \
    }

SWOW_CHILD_PROCESS_STREAM_GETTER(getStdin, 0)
SWOW_CHILD_PROCESS_STREAM_GETTER(getStdout, 1)
SWOW_CHILD_PROCESS_STREAM_GETTER(getStderr, 2)

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Swow_Process_ChildProcess_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Swow_Process_ChildProcess___destruct, 0, 0, 0)
ZEND_END_ARG_INFO()

static void swow_child_process_force_close_callback(uv_handle_t *handle)
{
    cat_process_t *process = cat_container_of(handle, cat_process_t, u.handle);
    cat_free(process);
}

static cat_bool_t swow_child_process_close(swow_child_process_t *s_process, cat_bool_t throw_on_error)
{
    if (s_process->closed) {
        return cat_true;
    }

    swow_child_process_close_streams(s_process);

    if (s_process->process != NULL) {
        cat_process_t *process = s_process->process;
        if (UNEXPECTED(!cat_queue_empty(&process->waiters))) {
            if (throw_on_error) {
                swow_throw_exception(swow_process_exception_ce, 0, "Process has waiters");
            }
            return cat_false;
        }
        s_process->process = NULL;
        if (!uv_is_closing((uv_handle_t *) &process->u.handle)) {
            uv_close((uv_handle_t *) &process->u.handle, swow_child_process_force_close_callback);
        }
    }

    s_process->closed = cat_true;
    return cat_true;
}

static PHP_METHOD(Swow_Process_ChildProcess, close)
{
    swow_child_process_t *s_process;

    ZEND_PARSE_PARAMETERS_NONE();

    s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS));
    if (!swow_child_process_close(s_process, cat_true)) {
        RETURN_THROWS();
    }
}

static PHP_METHOD(Swow_Process_ChildProcess, __destruct)
{
    swow_child_process_t *s_process;

    ZEND_PARSE_PARAMETERS_NONE();

    s_process = swow_child_process_get_from_object(Z_OBJ_P(ZEND_THIS));
    (void) swow_child_process_close(s_process, cat_false);
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
    PHP_ME(Swow_Process_ChildProcess, __destruct, arginfo_class_Swow_Process_ChildProcess___destruct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* }}} */

/* {{{ ProcessExitStatus */

#define arginfo_class_Swow_Process_ProcessExitStatus_getExitCode arginfo_class_Swow_Process_ChildProcess_getPid
#define arginfo_class_Swow_Process_ProcessExitStatus_getTermSignal arginfo_class_Swow_Process_ChildProcess_getPid
#define arginfo_class_Swow_Process_ProcessExitStatus_isExited arginfo_class_Swow_Process_ChildProcess_hasExited
#define arginfo_class_Swow_Process_ProcessExitStatus_isSignaled arginfo_class_Swow_Process_ChildProcess_hasExited
#define arginfo_class_Swow_Process_ProcessExitStatus_isStopped arginfo_class_Swow_Process_ChildProcess_hasExited

static PHP_METHOD(Swow_Process_ProcessExitStatus, getExitCode)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_LONG(s_status->exit_code);
}

static PHP_METHOD(Swow_Process_ProcessExitStatus, getTermSignal)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_LONG(s_status->term_signal);
}

static PHP_METHOD(Swow_Process_ProcessExitStatus, isExited)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_BOOL(s_status->exited);
}

static PHP_METHOD(Swow_Process_ProcessExitStatus, isSignaled)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_BOOL(s_status->signaled);
}

static PHP_METHOD(Swow_Process_ProcessExitStatus, isStopped)
{
    swow_process_exit_status_t *s_status;

    ZEND_PARSE_PARAMETERS_NONE();

    s_status = swow_process_exit_status_get_from_object(Z_OBJ_P(ZEND_THIS));

    RETURN_BOOL(s_status->stopped);
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_process_kill, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, pid, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, signal, IS_LONG, 0, SWOW_PROCESS_DEFAULT_SIGNAL_NAME)
ZEND_END_ARG_INFO()

ZEND_FUNCTION(kill)
{
    zend_long pid;
    zend_long signal = SWOW_PROCESS_DEFAULT_SIGNAL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(pid)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(signal)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(!cat_kill_ex((cat_pid_t) pid, (int) signal))) {
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
    swow_process_interface_ce = swow_register_internal_interface(
        "Swow\\Process\\ProcessInterface",
        swow_process_interface_methods,
        NULL
    );

    swow_child_process_ce = swow_register_internal_class(
        "Swow\\Process\\ChildProcess", NULL, swow_child_process_methods,
        &swow_child_process_handlers, NULL,
        cat_false, cat_false,
        swow_child_process_create_object, NULL,
        offsetof(swow_child_process_t, std)
    );
    zend_class_implements(swow_child_process_ce, 1, swow_process_interface_ce);

    swow_process_exit_status_ce = swow_register_internal_class(
        "Swow\\Process\\ProcessExitStatus", NULL, swow_process_exit_status_methods,
        &swow_process_exit_status_handlers, NULL,
        cat_false, cat_false,
        swow_process_exit_status_create_object, NULL,
        offsetof(swow_process_exit_status_t, std)
    );

    swow_process_exception_ce = swow_register_internal_class(
        "Swow\\Process\\ProcessException", swow_exception_ce, NULL,
        NULL, NULL, cat_true, cat_true, NULL, NULL, 0
    );

#ifdef CAT_OS_UNIX_LIKE
    REGISTER_LONG_CONSTANT("Swow\\Process\\SIGINT", SIGINT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("Swow\\Process\\SIGKILL", SIGKILL, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("Swow\\Process\\SIGTERM", SIGTERM, CONST_PERSISTENT);
#else
    REGISTER_LONG_CONSTANT("Swow\\Process\\SIGINT", CAT_SIGINT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("Swow\\Process\\SIGBREAK", CAT_SIGBREAK, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("Swow\\Process\\SIGKILL", CAT_SIGKILL, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("Swow\\Process\\CTRL_C_EVENT", CTRL_C_EVENT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("Swow\\Process\\CTRL_BREAK_EVENT", CTRL_BREAK_EVENT, CONST_PERSISTENT);
#endif

    if (zend_register_functions(NULL, swow_process_functions, NULL, MODULE_PERSISTENT) != SUCCESS) {
        return FAILURE;
    }

    return SUCCESS;
}

/* }}} */
