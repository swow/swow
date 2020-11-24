dnl config.m4 for extension swow

PHP_ARG_ENABLE([swow],
  [whether to enable Swow support],
  [AS_HELP_STRING([--enable-swow], [Enable Swow support])],
  [no]
)

PHP_ARG_ENABLE([swow-debug],
  [whether to enable Swow debug build flags],
  [AS_HELP_STRING([--enable-swow-debug], [Enable Swow debug build flags])],
  [no], [no]
)

PHP_ARG_ENABLE([swow-valgrind],
  [whether to enable Swow valgrind support],
  [AS_HELP_STRING([--enable-swow-valgrind], [Enable Swow valgrind support])],
  [$PHP_SWOW_DEBUG], [$PHP_SWOW_DEBUG]
)

if test "${SWOW}" != "no"; then

  AC_DEFINE([HAVE_SWOW], 1, [Have Swow])

  SWOW_CFLAGS=""
  SWOW_STD_CFLAGS=""
  SWOW_STD_CFLAGS="${SWOW_STD_CFLAGS} -fvisibility=hidden -std=gnu90"
  SWOW_STD_CFLAGS="${SWOW_STD_CFLAGS} -Wall -Wextra -Wstrict-prototypes"
  SWOW_STD_CFLAGS="${SWOW_STD_CFLAGS} -Wno-unused-parameter"
  SWOW_STD_CFLAGS="${SWOW_STD_CFLAGS} -g"

  if test "${PHP_SWOW_DEBUG}" = "yes"; then
    SWOW_STD_CFLAGS="${SWOW_STD_CFLAGS} -O0"

    AX_CHECK_COMPILE_FLAG(-Wextra,                         SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wextra")
    AX_CHECK_COMPILE_FLAG(-Wbool-conversion,               SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wbool-conversion")
    AX_CHECK_COMPILE_FLAG(-Wignored-qualifiers,            SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wignored-qualifiers")
    AX_CHECK_COMPILE_FLAG(-Wduplicate-enum,                SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wduplicate-enum")
    AX_CHECK_COMPILE_FLAG(-Wempty-body,                    SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wempty-body")
    AX_CHECK_COMPILE_FLAG(-Wenum-compare,                  SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wenum-compare")
    AX_CHECK_COMPILE_FLAG(-Wformat-security,               SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wformat-security")
    AX_CHECK_COMPILE_FLAG(-Wheader-guard,                  SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wheader-guard")
    AX_CHECK_COMPILE_FLAG(-Wincompatible-pointer-types-discards-qualifiers,
                                                           SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wincompatible-pointer-types-discards-qualifiers")
    AX_CHECK_COMPILE_FLAG(-Winit-self,                     SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Winit-self")
    AX_CHECK_COMPILE_FLAG(-Wlogical-not-parentheses,       SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wlogical-not-parentheses")
    AX_CHECK_COMPILE_FLAG(-Wlogical-op-parentheses,        SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wlogical-op-parentheses")
    AX_CHECK_COMPILE_FLAG(-Wloop-analysis,                 SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wloop-analysis")
    AX_CHECK_COMPILE_FLAG(-Wuninitialized,                 SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wuninitialized")
    AX_CHECK_COMPILE_FLAG(-Wno-missing-field-initializers, SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wno-missing-field-initializers")
    AX_CHECK_COMPILE_FLAG(-Wno-sign-compare,               SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wno-sign-compare")
    AX_CHECK_COMPILE_FLAG(-Wno-unused-const-variable,      SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wno-unused-const-variable")
    AX_CHECK_COMPILE_FLAG(-Wno-unused-parameter,           SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wno-unused-parameter")
    AX_CHECK_COMPILE_FLAG(-Wno-variadic-macros,            SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wno-variadic-macros")
    AX_CHECK_COMPILE_FLAG(-Wparentheses,                   SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wparentheses")
    AX_CHECK_COMPILE_FLAG(-Wpointer-bool-conversion,       SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wpointer-bool-conversion")
    AX_CHECK_COMPILE_FLAG(-Wsizeof-array-argument,         SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wsizeof-array-argument")
    AX_CHECK_COMPILE_FLAG(-Wwrite-strings,                 SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Wwrite-strings")
    AX_CHECK_COMPILE_FLAG(-fdiagnostics-show-option,       SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fdiagnostics-show-option")
    AX_CHECK_COMPILE_FLAG(-fno-omit-frame-pointer,         SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fno-omit-frame-pointer")
    AX_CHECK_COMPILE_FLAG(-fno-optimize-sibling-calls,     SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fno-optimize-sibling-calls")
    AX_CHECK_COMPILE_FLAG(-fsanitize-address,              SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fsanitize-address")
    AX_CHECK_COMPILE_FLAG(-fstack-protector,               SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fstack-protector")
  fi

  SWOW_INCLUDE_DIR="include"
  SWOW_SRC_DIR="src"
  SWOW_DEPS_DIR="deps"
  SWOW_BUILD_DIR="build"

  PHP_ADD_BUILD_DIR(${SWOW_BUILD_DIR}, 1)
  PHP_ADD_INCLUDE(${SWOW_INCLUDE_DIR})

  SWOW_SOURCE_FILES="
    ${SWOW_SRC_DIR}/swow.c
    ${SWOW_SRC_DIR}/swow_wrapper.c
    ${SWOW_SRC_DIR}/swow_log.c
    ${SWOW_SRC_DIR}/swow_exceptions.c
    ${SWOW_SRC_DIR}/swow_debug.c
    ${SWOW_SRC_DIR}/swow_hook.c
    ${SWOW_SRC_DIR}/swow_defer.c
    ${SWOW_SRC_DIR}/swow_coroutine.c
    ${SWOW_SRC_DIR}/swow_channel.c
    ${SWOW_SRC_DIR}/swow_sync.c
    ${SWOW_SRC_DIR}/swow_event.c
    ${SWOW_SRC_DIR}/swow_time.c
    ${SWOW_SRC_DIR}/swow_buffer.c
    ${SWOW_SRC_DIR}/swow_socket.c
    ${SWOW_SRC_DIR}/swow_stream.c
    ${SWOW_SRC_DIR}/swow_signal.c
    ${SWOW_SRC_DIR}/swow_watch_dog.c
    ${SWOW_SRC_DIR}/swow_http.c
    ${SWOW_SRC_DIR}/swow_websocket.c
  "

  if test "libcat" != ""; then
      dnl $Id: bddeea6baf539277b4f15f85f38de8cf3b4f7fa1 $

      AC_DEFINE([HAVE_LIBCAT], 1, [Have libcat])

      dnl Use Zend VM, e.g. define malloc to emalloc
      AC_DEFINE([CAT_VM], 1, [Use libcat in Zend VM])

      if test "${PHP_SWOW_DEBUG}" = "yes"; then
        AC_DEFINE([CAT_DEBUG], 1, [Debug build])
      fi

      CAT_CFLAGS=""
      CAT_DIR="${SWOW_DEPS_DIR}/libcat"
      CAT_SOURCE_FILES="
        ${CAT_DIR}/src/cat_cp.c
        ${CAT_DIR}/src/cat_memory.c
        ${CAT_DIR}/src/cat_string.c
        ${CAT_DIR}/src/cat_error.c
        ${CAT_DIR}/src/cat_module.c
        ${CAT_DIR}/src/cat_log.c
        ${CAT_DIR}/src/cat_env.c
        ${CAT_DIR}/src/cat.c
        ${CAT_DIR}/src/cat_api.c
        ${CAT_DIR}/src/cat_coroutine.c
        ${CAT_DIR}/src/cat_channel.c
        ${CAT_DIR}/src/cat_sync.c
        ${CAT_DIR}/src/cat_event.c
        ${CAT_DIR}/src/cat_time.c
        ${CAT_DIR}/src/cat_socket.c
        ${CAT_DIR}/src/cat_dns.c
        ${CAT_DIR}/src/cat_work.c
        ${CAT_DIR}/src/cat_buffer.c
        ${CAT_DIR}/src/cat_fs.c
        ${CAT_DIR}/src/cat_signal.c
        ${CAT_DIR}/src/cat_watch_dog.c
        ${CAT_DIR}/src/cat_http.c
        ${CAT_DIR}/src/cat_websocket.c
      "

      PHP_ADD_INCLUDE(${CAT_DIR}/include)

      dnl ====== Check valgrind ======

      if test "${PHP_VALGRIND}" = "yes" || test "${PHP_SWOW_VALGRIND}" = "yes"; then
        AC_MSG_CHECKING([for valgrind])
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
            #include <valgrind/valgrind.h>
        ]], [[
        ]])],[
            AC_DEFINE([HAVE_VALGRIND], 1, [Have Valgrind])
            AC_MSG_RESULT([yes])
        ],[
            AC_MSG_RESULT([no])
        ])
      fi

      dnl ====== Check boost context dependency ======

      AS_CASE([$host_os],
        [darwin*], [SWOW_OS="DARWIN"],
        [cygwin*], [SWOW_OS="CYGWIN"],
        [mingw*],  [SWOW_OS="MINGW"],
        [linux*],  [SWOW_OS="LINUX"],
                   [SWOW_OS="UNKNOWN"]
      )

      AS_CASE([$host_cpu],
        [x86_64*],  [SWOW_CPU_ARCH="x86_64"],
        [x86*],     [SWOW_CPU_ARCH="x86"],
        [i?86*],    [SWOW_CPU_ARCH="x86"],
        [arm*],     [SWOW_CPU_ARCH="arm"],
        [aarch64*], [SWOW_CPU_ARCH="arm64"],
        [arm64*],   [SWOW_CPU_ARCH="arm64"],
        [mips*],    [SWOW_CPU_ARCH="mips32"],
                    [SWOW_CPU_ARCH="unsupported"]
      )

      CAT_CONTEXT_FILE_SUFFIX=""
      if test "${SWOW_OS}" = "DARWIN"; then
        if test "${SWOW_CPU_ARCH}" = "arm"; then
          CAT_CONTEXT_FILE_SUFFIX="arm_aapcs_macho_gas.S"
        elif test "${SWOW_CPU_ARCH}" = "arm64"; then
          CAT_CONTEXT_FILE_SUFFIX="arm64_aapcs_macho_gas.S"
        else
          CAT_CONTEXT_FILE_SUFFIX="combined_sysv_macho_gas.S"
        fi
      elif test "${SWOW_CPU_ARCH}" = "x86_64"; then
        if test "${SWOW_OS}" = "LINUX"; then
          CAT_CONTEXT_FILE_SUFFIX="x86_64_sysv_elf_gas.S"
        fi
      elif test "${SWOW_CPU_ARCH}" = "x86"; then
        if test "${SWOW_OS}" = "LINUX"; then
          CAT_CONTEXT_FILE_SUFFIX="i386_sysv_elf_gas.S"
        fi
      elif test "${SWOW_CPU_ARCH}" = "arm"; then
        if test "${SWOW_OS}" = "LINUX"; then
          CAT_CONTEXT_FILE_SUFFIX="arm_aapcs_elf_gas.S"
        fi
      elif test "${SWOW_CPU_ARCH}" = "arm64"; then
        if test "${SWOW_OS}" = "LINUX"; then
          CAT_CONTEXT_FILE_SUFFIX="arm64_aapcs_elf_gas.S"
        fi
      elif test "${SWOW_CPU_ARCH}" = "ppc32"; then
        if test "${SWOW_OS}" = "LINUX"; then
          CAT_CONTEXT_FILE_SUFFIX="ppc32_sysv_elf_gas.S"
        fi
      elif test "${SWOW_CPU_ARCH}" = "ppc64"; then
        if test "${SWOW_OS}" = "LINUX"; then
          CAT_CONTEXT_FILE_SUFFIX="ppc64_sysv_elf_gas.S"
        fi
      elif test "${SWOW_CPU_ARCH}" = "mips32"; then
        if test "${SWOW_OS}" = "LINUX"; then
          CAT_CONTEXT_FILE_SUFFIX="mips32_o32_elf_gas.S"
        fi
      fi
      if test "${CAT_CONTEXT_FILE_SUFFIX}" = ""; then
        AC_DEFINE(CAT_COROUTINE_USE_UCONTEXT, 1, [ Cat Coroutine use ucontxt ])
      else
        CAT_SOURCE_FILES="
          ${CAT_SOURCE_FILES}
          ${CAT_DIR}/deps/context/asm/make_${CAT_CONTEXT_FILE_SUFFIX}
          ${CAT_DIR}/deps/context/asm/jump_${CAT_CONTEXT_FILE_SUFFIX}
        "
      fi

      dnl ====== Check libuv dependency ======

      AC_DEFUN([UV_DEFINE],[
        if test "$2" = ""; then
          UV_CFLAGS="${UV_CFLAGS} -D$1"
        else
          UV_CFLAGS="${UV_CFLAGS} -D$1=$2"
        fi
      ])

      UV_CFLAGS=""
      UV_DIR="${CAT_DIR}/deps/libuv"

      UV_SOURCE_FILES="
        ${UV_DIR}/src/fs-poll.c
        ${UV_DIR}/src/idna.c
        ${UV_DIR}/src/inet.c
        ${UV_DIR}/src/random.c
        ${UV_DIR}/src/strscpy.c
        ${UV_DIR}/src/threadpool.c
        ${UV_DIR}/src/timer.c
        ${UV_DIR}/src/uv-common.c
        ${UV_DIR}/src/uv-data-getter-setters.c
        ${UV_DIR}/src/version.c
      "

      UV_DEFINE(_FILE_OFFSET_BITS, 64, [_FILE_OFFSET_BITS])
      UV_DEFINE(_LARGEFILE_SOURCE, , [_LARGEFILE_SOURCE])

      dnl  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android|OS390|QNX")
      dnl    # Android has pthread as part of its c library, not as a separate
      dnl    # libpthread.so.
             PHP_ADD_LIBRARY(pthread)
      dnl  endif()

      UV_SOURCE_FILES="
        ${UV_SOURCE_FILES}
        ${UV_DIR}/src/unix/async.c
        ${UV_DIR}/src/unix/core.c
        ${UV_DIR}/src/unix/dl.c
        ${UV_DIR}/src/unix/fs.c
        ${UV_DIR}/src/unix/getaddrinfo.c
        ${UV_DIR}/src/unix/getnameinfo.c
        ${UV_DIR}/src/unix/loop-watcher.c
        ${UV_DIR}/src/unix/loop.c
        ${UV_DIR}/src/unix/pipe.c
        ${UV_DIR}/src/unix/poll.c
        ${UV_DIR}/src/unix/process.c
        ${UV_DIR}/src/unix/random-devurandom.c
        ${UV_DIR}/src/unix/signal.c
        ${UV_DIR}/src/unix/stream.c
        ${UV_DIR}/src/unix/tcp.c
        ${UV_DIR}/src/unix/thread.c
        ${UV_DIR}/src/unix/tty.c
        ${UV_DIR}/src/unix/udp.c
      "

      dnl if(CMAKE_SYSTEM_NAME STREQUAL "AIX")
      dnl   list(APPEND uv_defines
      dnl        _ALL_SOURCE
      dnl        _LINUX_SOURCE_COMPAT
      dnl        _THREAD_SAFE
      dnl        _XOPEN_SOURCE=500
      dnl        HAVE_SYS_AHAFS_EVPRODS_H)
      dnl   list(APPEND uv_libraries perfstat)
      dnl   list(APPEND uv_sources
      dnl        ${uv_dir}/src/unix/aix.c
      dnl        ${uv_dir}/src/unix/aix-common.c)
      dnl endif()

      dnl if(CMAKE_SYSTEM_NAME STREQUAL "Android")
      dnl   list(APPEND uv_defines _GNU_SOURCE)
      dnl   list(APPEND uv_libraries dl)
      dnl   list(APPEND uv_sources
      dnl        ${uv_dir}/src/unix/android-ifaddrs.c
      dnl        ${uv_dir}/src/unix/linux-core.c
      dnl        ${uv_dir}/src/unix/linux-inotify.c
      dnl        ${uv_dir}/src/unix/linux-syscalls.c
      dnl        ${uv_dir}/src/unix/procfs-exepath.c
      dnl        ${uv_dir}/src/unix/pthread-fixes.c
      dnl        ${uv_dir}/src/unix/random-getentropy.c
      dnl        ${uv_dir}/src/unix/random-getrandom.c
      dnl        ${uv_dir}/src/unix/random-sysctl-linux.c)
      dnl endif()

      dnl  if(APPLE OR CMAKE_SYSTEM_NAME MATCHES "Android|Linux|OS/390")
      if test "${SWOW_OS}" = "LINUX" || test "${SWOW_OS}" = "DARWIN" ; then
        UV_SOURCE_FILES="
          ${UV_SOURCE_FILES}
          ${UV_DIR}/src/unix/proctitle.c
        "
      fi
      dnl  endif()

      dnl if(CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD")
      dnl   list(APPEND uv_sources${uv_dir}/ src/unix/freebsd.c)
      dnl endif()

      dnl if(CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD|NetBSD|OpenBSD")
      dnl   list(APPEND uv_sources ${uv_dir}/src/unix/posix-hrtime.c src/unix/bsd-proctitle.c)
      dnl   list(APPEND uv_libraries kvm)
      dnl endif()

      dnl if(APPLE OR CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD|NetBSD|OpenBSD")
      dnl   list(APPEND uv_sources ${uv_dir}/src/unix/bsd-ifaddrs.c src/unix/kqueue.c)
      dnl endif()

      dnl if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
      dnl   list(APPEND uv_sources ${uv_dir}/src/unix/random-getrandom.c)
      dnl endif()

      dnl if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
      dnl   list(APPEND uv_sources ${uv_dir}/src/unix/random-getentropy.c)
      dnl endif()

      AC_CHECK_LIB(c, kqueue, [
        UV_DEFINE(HAVE_KQUEUE, 1, [Have Kqueue])
        UV_SOURCE_FILES="
          ${UV_SOURCE_FILES}
          ${UV_DIR}/src/unix/bsd-ifaddrs.c
          ${UV_DIR}/src/unix/kqueue.c
        "
      ])

      if test "${SWOW_OS}" = "DARWIN"; then
        UV_DEFINE(_DARWIN_UNLIMITED_SELECT, 1, [_DARWIN_UNLIMITED_SELECT])
        UV_DEFINE(_DARWIN_USE_64_BIT_INODE, 1, [_DARWIN_USE_64_BIT_INODE])
        UV_SOURCE_FILES="
          ${UV_SOURCE_FILES}
          ${UV_DIR}/src/unix/darwin-proctitle.c
          ${UV_DIR}/src/unix/darwin.c
          ${UV_DIR}/src/unix/fsevents.c
        "
      fi

      if test "${SWOW_OS}" = "LINUX"; then
        UV_DEFINE(_GNU_SOURCE, , [_GNU_SOURCE])
        UV_DEFINE(_POSIX_C_SOURCE, 200112, [_POSIX_C_SOURCE])
        PHP_ADD_LIBRARY(dl)
        PHP_ADD_LIBRARY(rt)
        UV_SOURCE_FILES="
           ${UV_SOURCE_FILES}
           ${UV_DIR}/src/unix/linux-core.c
           ${UV_DIR}/src/unix/linux-inotify.c
           ${UV_DIR}/src/unix/linux-syscalls.c
           ${UV_DIR}/src/unix/procfs-exepath.c
           ${UV_DIR}/src/unix/random-getrandom.c
           ${UV_DIR}/src/unix/random-sysctl-linux.c
         "
      fi

      dnl if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
      dnl   list(APPEND uv_sources src/unix/netbsd.c)
      dnl endif()

      dnl if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
      dnl   list(APPEND uv_sources src/unix/openbsd.c)
      dnl endif()

      dnl if(CMAKE_SYSTEM_NAME STREQUAL "OS390")
      dnl   list(APPEND uv_defines PATH_MAX=255)
      dnl   list(APPEND uv_defines _AE_BIMODAL)
      dnl   list(APPEND uv_defines _ALL_SOURCE)
      dnl   list(APPEND uv_defines _ISOC99_SOURCE)
      dnl   list(APPEND uv_defines _LARGE_TIME_API)
      dnl   list(APPEND uv_defines _OPEN_MSGQ_EXT)
      dnl   list(APPEND uv_defines _OPEN_SYS_FILE_EXT)
      dnl   list(APPEND uv_defines _OPEN_SYS_IF_EXT)
      dnl   list(APPEND uv_defines _OPEN_SYS_SOCK_EXT3)
      dnl   list(APPEND uv_defines _OPEN_SYS_SOCK_IPV6)
      dnl   list(APPEND uv_defines _UNIX03_SOURCE)
      dnl   list(APPEND uv_defines _UNIX03_THREADS)
      dnl   list(APPEND uv_defines _UNIX03_WITHDRAWN)
      dnl   list(APPEND uv_defines _XOPEN_SOURCE_EXTENDED)
      dnl   list(APPEND uv_sources
      dnl        ${uv_dir}/src/unix/pthread-fixes.c
      dnl        ${uv_dir}/src/unix/os390.c
      dnl        ${uv_dir}/src/unix/os390-syscalls.c)
      dnl   list(APPEND uv_cflags -Wc,DLL -Wc,exportall -Wc,xplink)
      dnl   list(APPEND uv_libraries -Wl,xplink)
      dnl endif()

      dnl if(CMAKE_SYSTEM_NAME STREQUAL "OS400")
      dnl   list(APPEND uv_defines
      dnl        _ALL_SOURCE
      dnl        _LINUX_SOURCE_COMPAT
      dnl        _THREAD_SAFE
      dnl        _XOPEN_SOURCE=500)
      dnl   list(APPEND uv_sources
      dnl     ${uv_dir}/src/unix/aix-common.c
      dnl     ${uv_dir}/src/unix/ibmi.c
      dnl     ${uv_dir}/src/unix/no-fsevents.c
      dnl     ${uv_dir}/src/unix/no-proctitle.c
      dnl     ${uv_dir}/src/unix/posix-poll.c)
      dnl endif()

      dnl if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
      dnl   list(APPEND uv_defines __EXTENSIONS__ _XOPEN_SOURCE=500)
      dnl   list(APPEND uv_libraries kstat nsl sendfile socket)
      dnl   list(APPEND uv_sources ${uv_dir}/src/unix/no-proctitle.c ${uv_dir}/src/unix/sunos.c)
      dnl endif()

      dnl if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
      dnl   list(APPEND uv_defines _BSD_SOURCE)
      dnl   list(APPEND uv_libraries bsd network)
      dnl   list(APPEND uv_sources
      dnl       ${uv_dir}/src/unix/haiku.c
      dnl       ${uv_dir}/src/unix/bsd-ifaddrs.c
      dnl       ${uv_dir}/src/unix/no-fsevents.c
      dnl       ${uv_dir}/src/unix/no-proctitle.c
      dnl       ${uv_dir}/src/unix/posix-hrtime.c
      dnl       ${uv_dir}/src/unix/posix-poll.c)
      dnl endif()

      CAT_CFLAGS="${CAT_CFLAGS} ${UV_CFLAGS}"
      CAT_SOURCE_FILES="${CAT_SOURCE_FILES} ${UV_SOURCE_FILES}"
      PHP_ADD_INCLUDE("${UV_DIR}/include")
      PHP_ADD_INCLUDE("${UV_DIR}/src")

      dnl ====== Check llhttp dependency ======

      LLHTTP_DIR="${CAT_DIR}/deps/llhttp"
      LLHTTP_SOURCE_FILES="
        ${LLHTTP_DIR}/src/api.c
        ${LLHTTP_DIR}/src/http.c
        ${LLHTTP_DIR}/src/llhttp.c
      "

      CAT_SOURCE_FILES="${CAT_SOURCE_FILES} ${LLHTTP_SOURCE_FILES}"
      PHP_ADD_INCLUDE("${LLHTTP_DIR}/include")
  fi

  SWOW_CFLAGS="${SWOW_CFLAGS} ${SWOW_STD_CFLAGS} ${SWOW_MAINTAINER_CFLAGS} ${CAT_CFLAGS}"
  SWOW_SOURCE_FILES="${SWOW_SOURCE_FILES} ${CAT_SOURCE_FILES}"

  PHP_NEW_EXTENSION(swow, $SWOW_SOURCE_FILES, $ext_shared,,$SWOW_CFLAGS)
fi
