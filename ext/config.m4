dnl config.m4 for extension swow

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary.

dnl If your extension references something external, use 'with':

dnl PHP_ARG_WITH([swow],
dnl   [for swow support],
dnl   [AS_HELP_STRING([--with-swow],
dnl     [Include swow support])])

dnl Otherwise use 'enable':

PHP_ARG_ENABLE(
  [swow],
  [whether to enable swow support],
  [AS_HELP_STRING([--enable-swow], [Enable swow support])],
  [no]
)

if test "${SWOW}" != "no"; then

  AC_DEFUN([SWOW_DEFINE],[
    if test "$2" = ""; then
      CFLAGS="${CFLAGS} -D$1"
    else
      CFLAGS="${CFLAGS} -D$1=$2"
    fi
  ])

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

  AC_MSG_CHECKING([if compiling with clang])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([], [[
        #ifndef __clang__
            not clang
        #endif
    ]])],
    [CLANG=yes], [CLANG=no]
  )
  AC_MSG_RESULT([$CLANG])

  CFLAGS="${CFLAGS} -fvisibility=hidden -std=gnu90"
  CFLAGS="${CFLAGS} -Wall -Wextra -Wstrict-prototypes"
  CFLAGS="${CFLAGS} -Wno-unused-parameter"
  CFLAGS="${CFLAGS} -g"

  AC_ARG_ENABLE(debug,
    [  --enable-debug, compile with debug symbols],
    [PHP_DEBUG=${enableval}],
    [PHP_DEBUG=0]
  )

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

      SWOW_DEFINE([HAVE_LIBCAT], 1)

      dnl Use ZendVM, e.g. malloc to emalloc
      SWOW_DEFINE([CAT_VM], 1)

      AC_MSG_CHECKING([if debug is enabled])
      if test "${PHP_DEBUG}" = "${enableval}"; then
        SWOW_DEFINE([CAT_DEBUG], 1)
      fi
      AC_MSG_RESULT([${PHP_DEBUG}])

      AC_DEFUN([CAT_CHECK_VALGRIND], [
          AC_MSG_CHECKING([for valgrind])
          AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
              #include <valgrind/valgrind.h>
          ]], [[
          ]])],[
              SWOW_DEFINE([HAVE_VALGRIND], 1, [Have Valgrind])
              AC_MSG_RESULT([yes])
          ],[
              AC_MSG_RESULT([no])
          ])
      ])
      CAT_CHECK_VALGRIND

      LIBCAT_DIR="${SWOW_DEPS_DIR}/libcat"

      PHP_ADD_INCLUDE(${LIBCAT_DIR}/include)
      PHP_ADD_INCLUDE(${LIBCAT_DIR}/include/cat)

      CAT_SOURCE_FILES="
        ${LIBCAT_DIR}/src/cat/cat_cp.c
        ${LIBCAT_DIR}/src/cat/cat_memory.c
        ${LIBCAT_DIR}/src/cat/cat_string.c
        ${LIBCAT_DIR}/src/cat/cat_error.c
        ${LIBCAT_DIR}/src/cat/cat_module.c
        ${LIBCAT_DIR}/src/cat/cat_log.c
        ${LIBCAT_DIR}/src/cat/cat_env.c
        ${LIBCAT_DIR}/src/cat.c
        ${LIBCAT_DIR}/src/cat_api.c
        ${LIBCAT_DIR}/src/cat_coroutine.c
        ${LIBCAT_DIR}/src/cat_channel.c
        ${LIBCAT_DIR}/src/cat_sync.c
        ${LIBCAT_DIR}/src/cat_event.c
        ${LIBCAT_DIR}/src/cat_time.c
        ${LIBCAT_DIR}/src/cat_socket.c
        ${LIBCAT_DIR}/src/cat_dns.c
        ${LIBCAT_DIR}/src/cat_work.c
        ${LIBCAT_DIR}/src/cat_buffer.c
        ${LIBCAT_DIR}/src/cat_fs.c
        ${LIBCAT_DIR}/src/cat_signal.c
        ${LIBCAT_DIR}/src/cat_watch_dog.c
        ${LIBCAT_DIR}/src/cat_http.c
        ${LIBCAT_DIR}/src/cat_websocket.c
      "

      dnl ====== Check boost context dependency ======

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
          SWOW_DEFINE(CAT_COROUTINE_USE_UCONTEXT, 1, [ Cat Coroutine use ucontxt ])
          if test "${SWOW_OS}" = "DARWIN"; then
            SWOW_DEFINE(_XOPEN_SOURCE, 700, [ OSX ucontxt compatibility ])
          fi
      else
          CAT_SOURCE_FILES="
            ${CAT_SOURCE_FILES}
            ${LIBCAT_DIR}/deps/context/asm/make_${CAT_CONTEXT_FILE_SUFFIX}
            ${LIBCAT_DIR}/deps/context/asm/jump_${CAT_CONTEXT_FILE_SUFFIX}
          "
      fi

      dnl ====== Check libuv dependency ======

      UV_DIR="${LIBCAT_DIR}/deps/libuv"

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

      SWOW_DEFINE(_FILE_OFFSET_BITS, 64, [_FILE_OFFSET_BITS])
      SWOW_DEFINE(_LARGEFILE_SOURCE, , [_LARGEFILE_SOURCE])

      dnl  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android|OS390")
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
        SWOW_DEFINE(HAVE_KQUEUE, 1, [Have Kqueue])
        UV_SOURCE_FILES="
          ${UV_SOURCE_FILES}
          ${UV_DIR}/src/unix/bsd-ifaddrs.c
          ${UV_DIR}/src/unix/kqueue.c
        "
      ])

      if test "${SWOW_OS}" = "DARWIN"; then
        SWOW_DEFINE(_DARWIN_UNLIMITED_SELECT, 1, [_DARWIN_UNLIMITED_SELECT])
        SWOW_DEFINE(_DARWIN_USE_64_BIT_INODE, 1, [_DARWIN_USE_64_BIT_INODE])
        UV_SOURCE_FILES="
          ${UV_SOURCE_FILES}
          ${UV_DIR}/src/unix/darwin-proctitle.c
          ${UV_DIR}/src/unix/darwin.c
          ${UV_DIR}/src/unix/fsevents.c
        "
      fi

      if test "${SWOW_OS}" = "LINUX"; then
        SWOW_DEFINE(_GNU_SOURCE, , [_GNU_SOURCE])
        SWOW_DEFINE(_POSIX_C_SOURCE, 200112, [_POSIX_C_SOURCE])
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
      dnl   list(APPEND ${uv_dir}/uv_sources src/unix/netbsd.c)
      dnl endif()

      dnl if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
      dnl   list(APPEND ${uv_dir}/uv_sources src/unix/openbsd.c)
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
      dnl   list(APPEND ${uv_dir}/uv_defines __EXTENSIONS__ _XOPEN_SOURCE=500)
      dnl   list(APPEND ${uv_dir}/uv_libraries kstat nsl sendfile socket)
      dnl   list(APPEND ${uv_dir}/uv_sources src/unix/no-proctitle.c src/unix/sunos.c)
      dnl endif()

      CAT_SOURCE_FILES="${CAT_SOURCE_FILES} ${UV_SOURCE_FILES}"
      PHP_ADD_INCLUDE("${UV_DIR}/include")
      PHP_ADD_INCLUDE("${UV_DIR}/src")

      dnl ====== Check llhttp dependency ======

      LLHTTP_DIR="${LIBCAT_DIR}/deps/llhttp"
      LLHTTP_SOURCE_FILES="
        ${LLHTTP_DIR}/src/api.c
        ${LLHTTP_DIR}/src/http.c
        ${LLHTTP_DIR}/src/llhttp.c
      "

      CAT_SOURCE_FILES="${CAT_SOURCE_FILES} ${LLHTTP_SOURCE_FILES}"
      PHP_ADD_INCLUDE("${LLHTTP_DIR}/include")
  fi

  SWOW_SOURCE_FILES="${SWOW_SOURCE_FILES} ${CAT_SOURCE_FILES}"

  PHP_NEW_EXTENSION(swow, $SWOW_SOURCE_FILES, $ext_shared)
fi
