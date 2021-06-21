/*
  +--------------------------------------------------------------------------+
  | libcat                                                                   |
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

#ifndef CAT_PROCESS_H
#define CAT_PROCESS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#include "cat_coroutine.h"
#include "cat_time.h"
#include "cat_socket.h"

/* These are the flags that can be used for the uv_process_options.flags field. */
#define CAT_PROCESS_FLAG_MAP(XX) \
        /* Set the child process' user id. The user id is supplied in the `uid` field
        * of the options struct. This does not work on windows; setting this flag
        * will cause uv_spawn() to fail. */ \
        XX(SETUID) \
        /* Set the child process' group id. The user id is supplied in the `gid`
        * field of the options struct. This does not work on windows; setting this
        * flag will cause uv_spawn() to fail. */ \
        XX(SETGID) \
        /* Do not wrap any arguments in quotes, or perform any other escaping, when
        * converting the argument list into a command line string. This option is
        * only meaningful on Windows systems. On Unix it is silently ignored. */ \
        XX(WINDOWS_VERBATIM_ARGUMENTS) \
        /* Spawn the child process in a detached state - this will make it a process
        * group leader, and will effectively enable the child to keep running after
        * the parent exits.  Note that the child process will still keep the
        * parent's event loop alive unless the parent process calls uv_unref() on
        * the child's process handle. */ \
        XX(DETACHED) \
        /* Hide the subprocess window that would normally be created. This option is
        * only meaningful on Windows systems. On Unix it is silently ignored. */ \
        XX(WINDOWS_HIDE) \
        /* Hide the subprocess console window that would normally be created. This
        * option is only meaningful on Windows systems. On Unix it is silently
        * ignored. */ \
        XX(WINDOWS_HIDE_CONSOLE) \
        /* Hide the subprocess GUI window that would normally be created. This
        * option is only meaningful on Windows systems. On Unix it is silently
        * ignored. */ \
        XX(WINDOWS_HIDE_GUI) \

typedef enum cat_process_flag_e {
#define CAT_PROCESS_FLAG_GEN(name) CAT_PROCESS_FLAG_##name = UV_PROCESS_##name,
    CAT_PROCESS_FLAG_MAP(CAT_PROCESS_FLAG_GEN)
#undef CAT_PROCESS_FLAG_GEN
} cat_process_flag_t;

typedef unsigned int cat_process_flags_t;

#define CAT_PROCESS_STDIO_FLAG_MAP(XX) \
        XX(IGNORE) \
        XX(CREATE_PIPE) \
        XX(INHERIT_FD) \
        XX(INHERIT_STREAM) \
        /* When UV_CREATE_PIPE is specified, UV_READABLE_PIPE and UV_WRITABLE_PIPE
         * determine the direction of flow, from the child process' perspective. Both
         * flags may be specified to create a duplex data stream. */ \
        XX(READABLE_PIPE) \
        XX(WRITABLE_PIPE) \
        /* When UV_CREATE_PIPE is specified, specifying UV_NONBLOCK_PIPE opens the
         * handle in non-blocking mode in the child. This may cause loss of data,
         * if the child is not designed to handle to encounter this mode,
         * but can also be significantly more efficient. */ \
        XX(NONBLOCK_PIPE) \
        XX(OVERLAPPED_PIPE) /* old name, for compatibility */ \

typedef enum cat_process_stdio_flags_e {
#define CAT_PROCESS_STDIO_FLAG_GEN(name) CAT_PROCESS_STDIO_FLAG_##name = UV_##name,
    CAT_PROCESS_STDIO_FLAG_MAP(CAT_PROCESS_STDIO_FLAG_GEN)
#undef CAT_PROCESS_STDIO_FLAG_GEN
} cat_process_stdio_flag_t;

typedef unsigned int cat_process_stdio_flags_t;

typedef struct cat_process_stdio_container_s {
    cat_process_stdio_flags_t flags;
    union {
        cat_socket_t *stream;
        cat_os_fd_t fd;
    } data;
} cat_process_stdio_container_t;

typedef struct cat_process_options_s {
    void *unused;
    /* Path to program to execute. */
    const char *file;
    /* Command line arguments. args[0] should be the path to the program. On
     * Windows this uses CreateProcess which concatenates the arguments into a
     * string this can cause some strange errors. See the note at
     * windows_verbatim_arguments. */
    const char **args;
    /* This will be set as the environ variable in the subprocess. If this is
     * NULL then the parents environ will be used. */
    const char **env;
    /* If non-null this represents a directory the subprocess should execute
     * in. Stands for current working directory. */
    const char *cwd;
    /* Various flags that control how uv_spawn() behaves. See the definition of
     * `enum uv_process_flags` below. */
    cat_process_flags_t flags;
    /* The `stdio` field points to an array of uv_stdio_container_t structs that
     * describe the file descriptors that will be made available to the child
     * process. The convention is that stdio[0] points to stdin, fd 1 is used for
     * stdout, and fd 2 is stderr.
     *
     * Note that on windows file descriptors greater than 2 are available to the
     * child process only if the child processes uses the MSVCRT runtime. */
    int stdio_count;
    const cat_process_stdio_container_t* stdio;
    /* Libcat can change the child process' user/group id. This happens only when
     * the appropriate bits are set in the flags fields. This is not supported on
     * windows; uv_spawn() will fail and set the error to CAT_ENOTSUP. */
    cat_uid_t uid;
    cat_gid_t gid;
} cat_process_options_t;

typedef struct cat_process_s cat_process_t;

struct cat_process_s {
    cat_queue_t waiters;
    int64_t exit_status;
    int term_signal;
    cat_bool_t exited;
    union {
        uv_process_t process;
        uv_handle_t handle;
    } u;
};

CAT_API cat_process_t *cat_process_run(const cat_process_options_t *options);
CAT_API cat_bool_t cat_process_wait(cat_process_t *process);
CAT_API cat_bool_t cat_process_wait_ex(cat_process_t *process, cat_timeout_t timeout);
CAT_API void cat_process_close(cat_process_t *process);

CAT_API cat_pid_t cat_process_get_pid(const cat_process_t *process);

CAT_API cat_bool_t cat_process_has_exited(const cat_process_t *process);
CAT_API int64_t cat_process_get_exit_status(const cat_process_t *process);
CAT_API int cat_process_get_term_signal(const cat_process_t *process);

CAT_API cat_bool_t cat_process_kill(cat_process_t *process, int signum);

#ifdef __cplusplus
}
#endif
#endif  /* CAT_PROCESS_H */
