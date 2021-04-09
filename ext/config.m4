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
  [$PHP_SWOW_DEBUG], [no]
)

PHP_ARG_ENABLE([swow-ssl],
  [whether to enable Swow OpenSSL support],
  [AS_HELP_STRING([--enable-swow-ssl], [Enable Swow OpenSSL support])],
  [yes], [no]
)

PHP_ARG_ENABLE([swow-curl],
  [whether to enable Swow cURL support],
  [AS_HELP_STRING([--enable-swow-curl], [Enable Swow cURL support])],
  [yes], [no]
)

if test "${SWOW}" != "no"; then
  dnl check if this php version we support
  AC_MSG_CHECKING([Check for supported PHP versions])
  if test -z "$PHP_VERSION"; then
    if test -z "$PHP_CONFIG"; then
      AC_MSG_ERROR([php-config not found])
    fi
    PHP_VERSION=`$PHP_CONFIG --version`
  fi

  if test -z "$PHP_VERSION"; then
    AC_MSG_ERROR([failed to detect PHP version, please report])
  fi
  PHP_VERSION_ID=`echo "${PHP_VERSION}" | $AWK 'BEGIN { FS = "."; } { printf "%d", ([$]1 * 100 + [$]2) * 100 + [$]3;}'`

  if test "${PHP_VERSION_ID}" -lt "70300" || test "${PHP_VERSION_ID}" -ge "80200"; then
    AC_MSG_ERROR([not supported. Need a PHP version >= 7.3.0 and < 8.2.0 (found $PHP_VERSION)])
  else
    AC_MSG_RESULT([supported ($PHP_VERSION)])
  fi

  AC_DEFINE([HAVE_SWOW], 1, [Have Swow])

  SWOW_STD_CFLAGS="${SWOW_STD_CFLAGS} -DHAVE_CONFIG_H"

  dnl start build SWOW_CFLAGS
  SWOW_STD_CFLAGS="${SWOW_STD_CFLAGS} -fvisibility=hidden -std=gnu99"
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
    AX_CHECK_COMPILE_FLAG(-Werror=implicit-function-declaration, SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -Werror=implicit-function-declaration")
    AX_CHECK_COMPILE_FLAG(-fdiagnostics-show-option,       SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fdiagnostics-show-option")
    AX_CHECK_COMPILE_FLAG(-fno-omit-frame-pointer,         SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fno-omit-frame-pointer")
    AX_CHECK_COMPILE_FLAG(-fno-optimize-sibling-calls,     SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fno-optimize-sibling-calls")
    AX_CHECK_COMPILE_FLAG(-fsanitize-address,              SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fsanitize-address")
    AX_CHECK_COMPILE_FLAG(-fstack-protector,               SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fstack-protector")
  fi
  SWOW_CFLAGS="${SWOW_CFLAGS} ${SWOW_STD_CFLAGS} ${SWOW_MAINTAINER_CFLAGS}"

  dnl start declare extension sources

  PHP_NEW_EXTENSION(swow, "swow.c", $ext_shared, ,\\$(SWOW_CFLAGS) \\$(SWOW_GIT_VERSION_CFLAG))

  dnl for git version number in swow.c
  AC_PATH_PROG(SWOW_GIT, git, no)
  if test x"$SWOW_GIT" != x"" && test -d ${ext_srcdir}/../.git ; then
     SWOW_GIT_VERSION_CFLAG=-DSWOW_GIT_VERSION=\\\"'-"`'"$SWOW_GIT --git-dir=${ext_srcdir}/../.git describe --always --abbrev=8 --dirty 2>&-"'`"'\\\"
  else
     SWOW_GIT_VERSION_CFLAG=
  fi
  PHP_SUBST(SWOW_GIT_VERSION_CFLAG)

  dnl solve in-tree build config.h problem
  dnl just make a fake config.h before all things
  PHP_ADD_BUILD_DIR("${ext_builddir}/build", 1)
  cat > ${ext_builddir}/build/config.h << EOF
#include "php_config.h"
EOF
  INCLUDES="-I. -I${ext_builddir}/build ${INCLUDES}"
  dnl add sources to makefile
  AC_DEFUN([SWOW_ADD_SOURCES],[
    ac_extra='$('$3')'
    if test $ext_shared = "yes"; then
      PHP_ADD_SOURCES_X(PHP_EXT_DIR(swow)/$1, $2, $ac_extra, shared_objects_swow, yes)
    else
      PHP_ADD_SOURCES(PHP_EXT_DIR(swow)/$1, $2, $ac_extra)
    fi
  ])

  SWOW_INCLUDES="-I${ext_srcdir}/include"
  SWOW_ADD_SOURCES(src,
    swow_wrapper.c \
    swow_log.c \
    swow_exceptions.c \
    swow_debug.c \
    swow_hook.c \
    swow_defer.c \
    swow_coroutine.c \
    swow_channel.c \
    swow_sync.c \
    swow_event.c \
    swow_time.c \
    swow_buffer.c \
    swow_socket.c \
    swow_fs.c \
    swow_stream.c \
    swow_signal.c \
    swow_watch_dog.c \
    swow_http.c \
    swow_websocket.c, SWOW_CFLAGS)

  dnl TODO: may use separate libcat
  if test "libcat" != ""; then

    AC_DEFINE([HAVE_LIBCAT], 1, [Have libcat])

    dnl Use Zend VM, e.g. define malloc to emalloc
    AC_DEFINE([CAT_VM], 1, [Use libcat in Zend VM])

    if test "${PHP_SWOW_DEBUG}" = "yes"; then
      AC_DEFINE([CAT_DEBUG], 1, [Cat debug options])
    fi

    dnl check if we use valgrind
    if test "${PHP_VALGRIND}" = "yes" || test "${PHP_SWOW_VALGRIND}" = "yes"; then
      AC_MSG_CHECKING([for valgrind])
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
          #include <valgrind/valgrind.h>
      ]], [[
      ]])],[
          AC_DEFINE([CAT_HAVE_VALGRIND], 1, [Have Valgrind])
          AC_MSG_RESULT([yes])
      ],[
          AC_MSG_RESULT([no])
      ])
    fi

    SWOW_CAT_INCLUDES="${SWOW_CAT_INCLUDES} -I${ext_srcdir}/include"
    SWOW_CAT_INCLUDES="${SWOW_CAT_INCLUDES} -I${ext_srcdir}/deps/libcat/include"
    SWOW_CAT_CFLAGS="${SWOW_CAT_CFLAGS} ${SWOW_STD_CFLAGS} ${SWOW_CAT_INCLUDES}"
    SWOW_ADD_SOURCES(deps/libcat/src,
      cat_cp.c \
      cat_memory.c \
      cat_string.c \
      cat_error.c \
      cat_module.c \
      cat_log.c \
      cat_env.c \
      cat.c \
      cat_api.c \
      cat_coroutine.c \
      cat_channel.c \
      cat_sync.c \
      cat_event.c \
      cat_poll.c \
      cat_time.c \
      cat_socket.c \
      cat_dns.c \
      cat_work.c \
      cat_buffer.c \
      cat_fs.c \
      cat_signal.c \
      cat_watch_dog.c \
      cat_http.c \
      cat_websocket.c, SWOW_CAT_CFLAGS)

    dnl prepare cat used context

    AS_CASE([$host_os],
      [darwin*], [SWOW_OS="DARWIN"],
      [linux*], [SWOW_OS="LINUX"],
      [openbsd*], [SWOW_OS="OPENBSD"],
      [SWOW_OS="UNKNOWN"]
    )

    AS_CASE([$host_cpu],
      [x86_64*|amd64*], [SWOW_CPU_ARCH="x86_64"],
      [x86*|i?86*|amd*|pentium], [SWOW_CPU_ARCH="x86"],
      [aarch64*|arm64*], [SWOW_CPU_ARCH="arm64"],
      [arm*], [SWOW_CPU_ARCH="arm"],
      [arm64*], [SWOW_CPU_ARCH="arm64"],
      [ppc64*], [SWOW_CPU_ARCH="ppc64"],
      [powerpc*], [SWOW_CPU_ARCH="ppc"],
      [mips64*], [SWOW_CPU_ARCH="mips64"],
      [mips*], [SWOW_CPU_ARCH="mips32"],
      [SWOW_CPU_ARCH="unsupported"]
    )

    AS_CASE([$SWOW_CPU_ARCH],
      [x86_64], [CAT_CONTEXT_FILE_PREFIX="x86_64_sysv"],
      [x86], [CAT_CONTEXT_FILE_PREFIX="x86_sysv"],
      [arm64], [CAT_CONTEXT_FILE_PREFIX="arm64_aapcs"],
      [arm], [CAT_CONTEXT_FILE_PREFIX="arm_aapcs"],
      [ppc64], [CAT_CONTEXT_FILE_PREFIX="ppc64_sysv"],
      [ppc], [CAT_CONTEXT_FILE_PREFIX="ppc_sysv"],
      [mips64], [CAT_CONTEXT_FILE_PREFIX="mips64_n64"],
      [mips32], [CAT_CONTEXT_FILE_PREFIX="mips32_o32"],
      [CAT_CONTEXT_FILE_PREFIX="combined_sysv"]
    )

    CAT_CONTEXT_FILE_SUFFIX=""
    if test "x${SWOW_OS}" = 'xDARWIN'; then
      CAT_CONTEXT_FILE_PREFIX="combined_sysv"
      CAT_CONTEXT_FILE_SUFFIX="macho_gas.S"
    elif test "x${SWOW_OS}" != 'xUNKNOWN'; then
      CAT_CONTEXT_FILE_SUFFIX="elf_gas.S"
    fi

    if test "${CAT_CONTEXT_FILE_SUFFIX}" != ''; then
      SWOW_CAT_ASM_FLAGS=""
      PHP_SUBST(SWOW_CAT_ASM_FLAGS)
      SWOW_ADD_SOURCES(deps/libcat/deps/context/asm,
        make_${CAT_CONTEXT_FILE_PREFIX}_${CAT_CONTEXT_FILE_SUFFIX} \
        jump_${CAT_CONTEXT_FILE_PREFIX}_${CAT_CONTEXT_FILE_SUFFIX}, SWOW_CAT_ASM_FLAGS)
    else
      AC_CHECK_HEADER(ucontext.h,
        [AC_DEFINE(CAT_COROUTINE_USE_UCONTEXT, 1, [ Cat Coroutine use ucontext ])],
        [AC_MSG_ERROR([Unsupported platform])])
    fi

    dnl prepare uv sources
    dnl TODO: may use sepreate uv?

      SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} ${SWOW_STD_CFLAGS}"
      SWOW_UV_INCLUDES="${SWOW_UV_INCLUDES} -I${ext_srcdir}/deps/libcat/deps/libuv/src"
      SWOW_UV_INCLUDES="${SWOW_UV_INCLUDES} -I${ext_srcdir}/deps/libcat/deps/libuv/include"
      SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src,
        fs-poll.c \
        idna.c \
        inet.c \
        random.c \
        strscpy.c \
        threadpool.c \
        timer.c \
        uv-common.c \
        uv-data-getter-setters.c \
        version.c, SWOW_UV_CFLAGS)

      PHP_ADD_LIBRARY(pthread)

      SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
        async.c \
        core.c \
        dl.c \
        fs.c \
        getaddrinfo.c \
        getnameinfo.c \
        loop-watcher.c \
        loop.c \
        pipe.c \
        poll.c \
        process.c \
        random-devurandom.c \
        signal.c \
        stream.c \
        tcp.c \
        thread.c \
        tty.c \
        udp.c, SWOW_UV_CFLAGS)

      if test "x${host_alias%%*aix*}" != "x${host_alias}" ; then
        SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_ALL_SOURCE"
        SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_LINUX_SOURCE_COMPAT"
        SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_THREAD_SAFE"
        SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_XOPEN_SOURCE=500"
        AC_CHECK_HEADERS([sys/ahafs/evprods.h])
        PHP_ADD_LIBRARY(perfstat)
        SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix, aix.c aix-common.c, SWOW_UV_CFLAGS)
      fi

      SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_FILE_OFFSET_BITS=64"
      SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_LARGEFILE_SOURCE"

      if test "x${SWOW_OS}" = "xLINUX" || test "x${SWOW_OS}" = "xDARWIN" ; then
        SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix, proctitle.c, SWOW_UV_CFLAGS)
      fi

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

      if test "x${SWOW_OS}" = "xOPENBSD" || test "x${SWOW_OS}" = "xDARWIN" ; then
        SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix, random-getentropy.c, SWOW_UV_CFLAGS)
      fi

      AC_CHECK_LIB(c, kqueue, [
        AC_DEFINE(HAVE_KQUEUE, 1, [Have Kqueue])
        SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix, bsd-ifaddrs.c kqueue.c, SWOW_UV_CFLAGS)
      ])

      if test "x${SWOW_OS}" = "xDARWIN"; then
        SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_DARWIN_UNLIMITED_SELECT"
        SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_DARWIN_USE_64_BIT_INODE"
        SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
          darwin-proctitle.c \
          darwin.c \
          fsevents.c, SWOW_UV_CFLAGS)
      fi

      if test "x${SWOW_OS}" = "xLINUX"; then
        SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_GNU_SOURCE"
        SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_POSIX_C_SOURCE=200112"
        PHP_ADD_LIBRARY(dl)
        PHP_ADD_LIBRARY(rt)
        SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
          linux-core.c \
          linux-inotify.c \
          linux-syscalls.c \
          procfs-exepath.c \
          random-getrandom.c \
          random-sysctl-linux.c, SWOW_UV_CFLAGS)
      fi

      dnl if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
      dnl   list(APPEND uv_sources src/unix/netbsd.c)
      dnl endif()

      if test "${SWOW_OS}" = "OPENBSD" ; then
        SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix, openbsd.c, SWOW_UV_CFLAGS)
      fi

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

      SWOW_CAT_INCLUDES="${SWOW_CAT_INCLUDES} ${SWOW_UV_INCLUDES}"
      SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} ${SWOW_UV_INCLUDES}"
      PHP_SUBST(SWOW_UV_CFLAGS)

    dnl add llhttp sources

    SWOW_LLHTTP_INCLUDES="-I${ext_srcdir}/deps/libcat/deps/llhttp/include"
    SWOW_CAT_INCLUDES="${SWOW_CAT_INCLUDES} ${SWOW_LLHTTP_INCLUDES}"
    SWOW_LLHTTP_CFLAGS="${SWOW_LLHTTP_CFLAGS} ${SWOW_STD_CFLAGS} ${SWOW_LLHTTP_INCLUDES}"
    PHP_SUBST(SWOW_LLHTTP_CFLAGS)
    SWOW_ADD_SOURCES(deps/libcat/deps/llhttp/src,
      api.c \
      http.c \
      llhttp.c, SWOW_LLHTTP_CFLAGS)

    dnl prepare pkg-config
    if test -z "$PKG_CONFIG"; then
      AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
    fi
    dnl SWOW_PKG_CHECK_MODULES($varname, $libname, $ver, $search_path, $if_found, $if_not_found)
    AC_DEFUN([SWOW_PKG_CHECK_MODULES],[
      dnl find pkg using pkg-config cli tool
      if test ! -x "$PKG_CONFIG" ; then
        AC_MSG_WARN([Cannot find package $2: pkg-config not found])
      fi
      SWOW_PKG_FIND_PATH=${$4}
      if test "xyes" = "x${$4}" ; then
        SWOW_PKG_FIND_PATH=
      fi
      AC_MSG_CHECKING(for $2 $3 or greater)
      dnl shall we find env first?
      dnl echo env PKG_CONFIG_PATH=${SWOW_PKG_FIND_PATH}/lib/pkgconfig $PKG_CONFIG --atleast-version $3 $2
      dnl env PKG_CONFIG_PATH=${SWOW_PKG_FIND_PATH}/lib/pkgconfig $PKG_CONFIG --atleast-version $3 $2; echo $?
      dnl env PKG_CONFIG_PATH=${SWOW_PKG_FIND_PATH}/lib/pkgconfig $PKG_CONFIG --modversion $2
      if env PKG_CONFIG_PATH=${SWOW_PKG_FIND_PATH}/lib/pkgconfig $PKG_CONFIG --atleast-version $3 $2; then
        $2_version_full=`env PKG_CONFIG_PATH=${SWOW_PKG_FIND_PATH}/lib/pkgconfig $PKG_CONFIG --modversion $2`
        AC_MSG_RESULT(${$2_version_full})
        $1_LIBS=`env PKG_CONFIG_PATH=${SWOW_PKG_FIND_PATH}/lib/pkgconfig $PKG_CONFIG --libs   $2`
        $1_INCL=`env PKG_CONFIG_PATH=${SWOW_PKG_FIND_PATH}/lib/pkgconfig $PKG_CONFIG --cflags $2`
        $5
      else
        AC_MSG_RESULT(no)
        $6
      fi
    ])
    dnl add ssl sources
    if test "x${PHP_SWOW_SSL}" != "xno" ; then
      SWOW_PKG_CHECK_MODULES([OPENSSL], openssl, 1.0.1, [PHP_SWOW_SSL], [
        dnl make changes
        AC_DEFINE([CAT_HAVE_SSL], 1, [Enable libcat ssl])
        PHP_EVAL_LIBLINE($OPENSSL_LIBS, SWOW_SHARED_LIBADD)
        PHP_EVAL_INCLINE($OPENSSL_CFLAGS)
        SWOW_ADD_SOURCES(deps/libcat/src, cat_ssl.c, SWOW_CAT_CFLAGS)
        dnl SWOW_ADD_SOURCES(src, swow_curl.c, SWOW_CFLAGS)
      ],[
        AC_MSG_WARN([Swow OpenSSL support not enabled: OpenSSL not found])
      ])
    fi

    dnl add curl sources
    if test "x${PHP_SWOW_CURL}" != "xno" ; then
      SWOW_PKG_CHECK_MODULES([CURL], libcurl, 7.25.2, [PHP_SWOW_CURL], [
        if test "x${PHP_CURL}" = "xno" ; then
          AC_MSG_WARN([Swow cURL support is enabled but cURL PHP extension is not enabled])
        fi
        dnl make changes
        AC_DEFINE([CAT_HAVE_CURL], 1, [Enable libcat cURL])
        PHP_EVAL_LIBLINE($CURL_LIBS, SWOW_SHARED_LIBADD)
        PHP_EVAL_INCLINE($CURL_CFLAGS)
        SWOW_ADD_SOURCES(deps/libcat/src, cat_curl.c, SWOW_CAT_CFLAGS)
        SWOW_ADD_SOURCES(src, swow_curl.c, SWOW_CFLAGS)
      ],[
        AC_MSG_WARN([Swow cURL support not enabled: libcurl not found])
      ])
    fi

    SWOW_CAT_CFLAGS="${SWOW_CAT_CFLAGS} ${SWOW_CAT_INCLUDES}"
    PHP_SUBST(SWOW_CAT_CFLAGS)
  fi

  SWOW_CFLAGS="${SWOW_CFLAGS} ${SWOW_INCLUDES} ${SWOW_CAT_INCLUDES}"
  PHP_SUBST(SWOW_CFLAGS)
  PHP_SUBST(SWOW_SHARED_LIBADD)
fi
