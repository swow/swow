dnl config.m4 for extension swow

dnl sometimes /usr/local/include or /usr/include will be appended to INCLUDES in COMMON_FLAGS
dnl at this time, system side libuv may conflict with deps/libuv
dnl to solve this, we have to re-implement this macro here

dnl from acinclude.m4 or build/php.m4
dnl
dnl SWOW_ADD_SOURCES_X(source-path, sources[, special-flags[, target-var[, shared[, special-post-flags]]]])
dnl
dnl Additional to SWOW_ADD_SOURCES (see below), this lets you set the name of the
dnl array target-var directly, as well as whether shared objects will be built
dnl from the sources. Should not be used directly.
dnl
AC_DEFUN([SWOW_ADD_SOURCES_X],[
dnl Relative to source- or build-directory?
dnl ac_srcdir/ac_bdir include trailing slash
  case $1 in
  ""[)] ac_srcdir="$abs_srcdir/"; unset ac_bdir; ac_inc="-I. -I$abs_srcdir" ;;
  /*[)] ac_srcdir=`echo "$1"|cut -c 2-`"/"; ac_bdir=$ac_srcdir; ac_inc="-I$ac_bdir -I$abs_srcdir/$ac_bdir" ;;
  *[)] ac_srcdir="$abs_srcdir/$1/"; ac_bdir="$1/"; ac_inc="-I$ac_bdir -I$ac_srcdir" ;;
  esac

dnl how to build .. shared or static?
  ifelse($5,yes,_PHP_ASSIGN_BUILD_VARS(shared),_PHP_ASSIGN_BUILD_VARS(php))

dnl Iterate over the sources.
  old_IFS=[$]IFS
  for ac_src in $2; do

dnl Remove the suffix.
      IFS=.
      set $ac_src
      ac_obj=[$]1
      IFS=$old_IFS

dnl Append to the array which has been dynamically chosen at m4 time.
      $4="[$]$4 [$]ac_bdir[$]ac_obj.lo"

dnl Choose the right compiler/flags/etc. for the source-file.
      case $ac_src in
        *.c[)] ac_comp="$b_c_pre $3 $ac_inc $b_c_meta -c $ac_srcdir$ac_src -o $ac_bdir$ac_obj.$b_lo $6$b_c_post" ;;
        *.s[)] ac_comp="$b_c_pre $3 $ac_inc $b_c_meta -c $ac_srcdir$ac_src -o $ac_bdir$ac_obj.$b_lo $6$b_c_post" ;;
        *.S[)] ac_comp="$b_c_pre $3 $ac_inc $b_c_meta -c $ac_srcdir$ac_src -o $ac_bdir$ac_obj.$b_lo $6$b_c_post" ;;
        *.cpp|*.cc|*.cxx[)] ac_comp="$b_cxx_pre $3 $ac_inc $b_cxx_meta -c $ac_srcdir$ac_src -o $ac_bdir$ac_obj.$b_lo $6$b_cxx_post" ;;
      esac

dnl note: this tab is necessary for generated Makefile
dnl Create a rule for the object/source combo.
    cat >>Makefile.objects<<EOF
$ac_bdir[$]ac_obj.lo: $ac_srcdir[$]ac_src
	$ac_comp
EOF
  done
])
dnl
dnl SWOW_ADD_SOURCES($src_dir, $files, $ac_inc, $ac_extra): add sources to makefile
dnl
dnl $src_dir is relative dir to source, $files is filename, splited by $IFS
dnl $ac_inc is include, will be prepend to original $ac_inc
dnl $ac_extra is extra cflags, will append to cflags
dnl
AC_DEFUN([SWOW_ADD_SOURCES],[
  swow_extra=''
  if test x"$3" != x ;then
    swow_extra='$('$3')'
  fi
  if test x"$4" != x ;then
    swow_extra="${swow_extra}"' $('$4')'
  fi
  if test $ext_shared = "yes"; then
    SWOW_ADD_SOURCES_X(PHP_EXT_DIR(swow)/$1, $2, $swow_extra, shared_objects_swow, yes)
  else
    SWOW_ADD_SOURCES_X(PHP_EXT_DIR(swow)/$1, $2, $swow_extra, PHP_GLOBAL_OBJS)
  fi
])

dnl
dnl SWOW_PKG_CHECK_MODULES($varname, $libname, $ver, $search_path, $if_found, $if_not_found)
dnl
AC_DEFUN([SWOW_PKG_CHECK_MODULES],[
  AC_MSG_CHECKING(for $2 $3 or greater)
  if test "x${$1_LIBS+set}" = "xset" || test "x${$1_INCLUDES+set}" = "xset"; then
    AC_MSG_RESULT([using $1_CFLAGS and $1_LIBS])
    $1_LIBS=${$1_CFLAGS}
    $1_INCL=${$1_LIBS}
    $5
  elif test -x "$PKG_CONFIG" ; then
dnl find pkg using pkg-config cli tool
    SWOW_PKG_FIND_PATH=${$4}/lib/pkgconfig
    if test "xyes" = "x${$4}" ; then
      SWOW_PKG_FIND_PATH=/lib/pkgconfig
    fi

    if test "x" != "x$PKG_CONFIG_PATH"; then
      SWOW_PKG_FIND_PATH="$SWOW_PKG_FIND_PATH:$PKG_CONFIG_PATH"
    fi

    if env PKG_CONFIG_PATH=${SWOW_PKG_FIND_PATH} $PKG_CONFIG --atleast-version $3 $2; then
      $2_version_full=`env PKG_CONFIG_PATH=${SWOW_PKG_FIND_PATH} $PKG_CONFIG --modversion $2`
      AC_MSG_RESULT(${$2_version_full})
      $1_LIBS=`env PKG_CONFIG_PATH=${SWOW_PKG_FIND_PATH} $PKG_CONFIG --libs   $2`
      $1_INCL=`env PKG_CONFIG_PATH=${SWOW_PKG_FIND_PATH} $PKG_CONFIG --cflags $2`
      $5
    else
      AC_MSG_RESULT(no)
      $6
    fi
  else
    AC_MSG_RESULT(no)
    AC_MSG_WARN([Cannot find pkg-config, please set $1_CFLAGS and $1_LIBS to use $2])
    $6
  fi
])


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

PHP_ARG_ENABLE([swow-thread-context],
  [whether to enable Swow thread context support],
  [AS_HELP_STRING([--enable-swow-thread-context], [Enable Swow thread context support])],
  [no], [no]
)

PHP_ARG_ENABLE([swow-gcov],
  [whether to enable Swow GCOV support],
  [AS_HELP_STRING([--enable-swow-gcov], [Enable Swow GCOV support])],
  [no], [no]
)

PHP_ARG_ENABLE([swow-valgrind],
  [whether to enable Swow valgrind support],
  [AS_HELP_STRING([--enable-swow-valgrind], [Enable Swow valgrind support])],
  [$PHP_SWOW_DEBUG], [no]
)

PHP_ARG_ENABLE([swow-memory-sanitizer],
  [whether to enable Swow MSan support],
  [AS_HELP_STRING([--enable-swow-memory-sanitizer], [Enable memory sanitizer (clang only)])],
  [no], [no]
)

PHP_ARG_ENABLE([swow-address-sanitizer],
  [whether to enable Swow ASan support],
  [AS_HELP_STRING([--enable-swow-address-sanitizer], [Enable address sanitizer])],
  [no], [no]
)

PHP_ARG_ENABLE([swow-undefined-sanitizer],
  [whether to enable Swow UBSan support],
  [AS_HELP_STRING([--enable-swow-undefined-sanitizer], [Enable undefined sanitizer])],
  [no], [no]
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

if test "${PHP_SWOW}" != "no"; then
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

  if test "${PHP_VERSION_ID}" -lt "80000" || test "${PHP_VERSION_ID}" -gt "80200"; then
    AC_MSG_ERROR([not supported. Need a PHP version >= 8.0.0 and <= 8.2.0 (found $PHP_VERSION)])
  else
    AC_MSG_RESULT([supported ($PHP_VERSION)])
  fi

  AC_DEFINE([HAVE_SWOW], 1, [Have Swow])

  SWOW_STD_CFLAGS="${SWOW_STD_CFLAGS} -DHAVE_CONFIG_H"

  dnl start build SWOW_CFLAGS
  SWOW_STD_CFLAGS="${SWOW_STD_CFLAGS} -fvisibility=hidden -std=gnu99"
  SWOW_STD_CFLAGS="${SWOW_STD_CFLAGS} -Wall -Wextra -Wstrict-prototypes"
  SWOW_STD_CFLAGS="${SWOW_STD_CFLAGS} -Wno-unused-parameter"

  PHP_SUBST(SWOW_STD_CFLAGS)

  if test "${PHP_SWOW_DEBUG}" = "yes"; then
    dnl Remove all optimization flags from CFLAGS.
    changequote({,})
    CFLAGS=`echo "$CFLAGS" | "${SED}" -e 's/-O[0-9s]*//g'`
    CXXFLAGS=`echo "$CXXFLAGS" | "${SED}" -e 's/-O[0-9s]*//g'`
    changequote([,])
    CFLAGS="$CFLAGS -g -O0"

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
    AX_CHECK_COMPILE_FLAG(-fno-optimize-sibling-calls,     SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fno-optimize-sibling-calls")
    AX_CHECK_COMPILE_FLAG(-fstack-protector,               SWOW_MAINTAINER_CFLAGS="${SWOW_MAINTAINER_CFLAGS} -fstack-protector")

    if test "${PHP_SWOW_GCOV}" = "yes"; then
      AX_CHECK_COMPILE_FLAG([-fprofile-arcs -ftest-coverage], [
        CFLAGS="$CFLAGS -fprofile-arcs -ftest-coverage"
      ])
    fi
  fi
  PHP_SUBST(SWOW_MAINTAINER_CFLAGS)
  SWOW_CFLAGS="${SWOW_CFLAGS} \$(SWOW_STD_CFLAGS) \$(SWOW_MAINTAINER_CFLAGS)"
  dnl for php 067df263448ee26013cddee1065bc9c1f028bd23 break
  SWOW_CFLAGS="${SWOW_CFLAGS} -D_GNU_SOURCE"

  dnl start declare extension sources

  PHP_NEW_EXTENSION(swow, "swow.c", $ext_shared, ,\\$(SWOW_CFLAGS))

  dnl for git version number in swow.c
  AC_PATH_PROG(SWOW_GIT, git, no)
  swow_check_git()
  {
    # prove git found
    test x"$SWOW_GIT" != x"no" || return 1
    test x"$SWOW_GIT" != x"" || return 1
    # prove git dir exists
    test -d ${ext_srcdir}/../.git || return 1
    # prove it's not bare
    test "x"`$SWOW_GIT --git-dir=${ext_srcdir}/../.git rev-parse --is-bare-repository` = "xtrue" && return 1
    # if it's detached head, it's ok
    test "x"`$SWOW_GIT --git-dir=${ext_srcdir}/../.git rev-parse --abbrev-ref --symbolic-full-name HEAD` = "xHEAD" && return 0
    # prove it's swow's git dir
    # this hash is the commit introducing this
    $SWOW_GIT --git-dir=${ext_srcdir}/../.git branch --contains e874691b20cddae2d169e47c05b7b42464f11cc0 >&- 2>&- || return 1
    return 0
  }
  if swow_check_git ; then
     SWOW_GIT_VERSION_CFLAG=-DSWOW_GIT_VERSION=\\\"'-"`'"$SWOW_GIT --work-tree=${ext_srcdir}/.. --git-dir=${ext_srcdir}/../.git describe --always --abbrev=8 --dirty 2>&-"'`"'\\\"
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

  SWOW_INCLUDES="-I${ext_srcdir}/include"

  SWOW_MAIN_CFLAGS='$(SWOW_CFLAGS) $(SWOW_GIT_VERSION_CFLAG)'
  PHP_SUBST(SWOW_MAIN_CFLAGS)
  SWOW_ADD_SOURCES(src, swow_main.c, SWOW_INCLUDES, SWOW_MAIN_CFLAGS)
  SWOW_ADD_SOURCES(src,
    swow_wrapper.c \
    swow_errno.c \
    swow_log.c \
    swow_exceptions.c \
    swow_debug.c \
    swow_util.c \
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
    swow_stream_wrapper.c \
    swow_signal.c \
    swow_watchdog.c \
    swow_http.c \
    swow_websocket.c \
    swow_proc_open.c \
    , SWOW_INCLUDES, SWOW_CFLAGS)

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

    if test "$PHP_SWOW_MEMORY_SANITIZER" = "yes" &&
       test "$PHP_SWOW_ADDRESS_SANITIZER" = "yes"; then
       AC_MSG_ERROR([MemorySanitizer and AddressSanitizer are mutually exclusive])
    fi

    if test "$PHP_SWOW_MEMORY_SANITIZER" = "yes"; then
      AX_CHECK_COMPILE_FLAG([-fsanitize=memory -fsanitize-memory-track-origins], [
        CFLAGS="$CFLAGS -fsanitize=memory -fsanitize-memory-track-origins"
        CXXFLAGS="$CXXFLAGS -fsanitize=memory -fsanitize-memory-track-origins"
      ], [AC_MSG_ERROR([MemorySanitizer is not available])])
    fi

    if test "$PHP_SWOW_ADDRESS_SANITIZER" = "yes"; then
      AX_CHECK_COMPILE_FLAG([-fsanitize=address], [
        CFLAGS="$CFLAGS -fsanitize=address -DZEND_TRACK_ARENA_ALLOC"
        CXXFLAGS="$CXXFLAGS -fsanitize=address -DZEND_TRACK_ARENA_ALLOC"
      ], [AC_MSG_ERROR([AddressSanitizer is not available])])
    fi

    if test "$PHP_SWOW_MEMORY_SANITIZER" = "yes" ||
       test "$PHP_SWOW_ADDRESS_SANITIZER" = "yes" ||
       test "$PHP_SWOW_UNDEFINED_SANITIZER" = "yes"; then
        CFLAGS="$CFLAGS -fno-omit-frame-pointer"
        CXXFLAGS="$CXXFLAGS -fno-omit-frame-pointer"
    fi

    SWOW_CAT_INCLUDES="${SWOW_CAT_INCLUDES} -I${ext_srcdir}/include -I${ext_srcdir}/deps/libcat/include"
    SWOW_CAT_CFLAGS="${SWOW_CAT_CFLAGS} \$(SWOW_STD_CFLAGS)"
    dnl for php 067df263448ee26013cddee1065bc9c1f028bd23 break
    SWOW_CAT_CFLAGS="${SWOW_CAT_CFLAGS} -D_GNU_SOURCE"
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
      cat_os_wait.c \
      cat_async.c \
      cat_watchdog.c \
      cat_http.c \
      cat_websocket.c, SWOW_CAT_INCLUDES, SWOW_CAT_CFLAGS)

    dnl prepare cat used context

    if test "${PHP_SWOW_THREAD_CONTEXT}" = "yes"; then
        AC_DEFINE(CAT_COROUTINE_USE_THREAD_CONTEXT, 1, [ Cat Coroutine use thread-context ])
    else
        AS_CASE([$host_cpu],
          [x86_64*|amd64*], [SWOW_CPU_ARCH="x86_64"],
          [x86*|i?86*|amd*|pentium], [SWOW_CPU_ARCH="x86"],
          [aarch64*|arm64*], [SWOW_CPU_ARCH="arm64"],
          [arm*], [SWOW_CPU_ARCH="arm"],
          [ppc64*], [SWOW_CPU_ARCH="ppc64"],
          [powerpc*], [SWOW_CPU_ARCH="ppc"],
          [mips64*], [SWOW_CPU_ARCH="mips64"],
          [mips*], [SWOW_CPU_ARCH="mips32"],
          [riscv64*], [SWOW_CPU_ARCH="riscv64"],
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
          [riscv64], [CAT_CONTEXT_FILE_PREFIX="riscv64_sysv"],
          [CAT_CONTEXT_FILE_PREFIX="combined_sysv"]
        )

        dnl will be determined below
        CAT_CONTEXT_FILE_SUFFIX=""
        AS_CASE([$host_os],
          [linux*|*aix*|freebsd*|netbsd*|openbsd*|dragonfly*|solaris*|haiku*], [CAT_CONTEXT_FILE_SUFFIX="elf_gas.S"],
          [darwin*], [
            CAT_CONTEXT_FILE_PREFIX="combined_sysv"
            CAT_CONTEXT_FILE_SUFFIX="macho_gas.S"
          ],
          [CAT_CONTEXT_FILE_SUFFIX="unknown"]
        )

        if test "x${CAT_CONTEXT_FILE_SUFFIX}" = 'xunknown'; then
          AC_CHECK_HEADER(ucontext.h,
            [AC_DEFINE(CAT_COROUTINE_USE_UCONTEXT, 1, [ Cat Coroutine use ucontext ])],
            [AC_MSG_ERROR([Unsupported platform])])
        else
          SWOW_CAT_ASM_FLAGS=""
          PHP_SUBST(SWOW_CAT_ASM_FLAGS)
          SWOW_ADD_SOURCES(deps/libcat/deps/context/asm,
            make_${CAT_CONTEXT_FILE_PREFIX}_${CAT_CONTEXT_FILE_SUFFIX} \
            jump_${CAT_CONTEXT_FILE_PREFIX}_${CAT_CONTEXT_FILE_SUFFIX}, , SWOW_CAT_ASM_FLAGS)
            dnl                              note this should be empty ^
        fi
    fi

    dnl prepare uv sources
    dnl libuv is libcat strong dependency

      PHP_ADD_LIBRARY(pthread)

      dnl all unix things
      SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} \$(SWOW_STD_CFLAGS)"
      SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_FILE_OFFSET_BITS=64"
      SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_LARGEFILE_SOURCE"
      SWOW_UV_INCLUDES="${SWOW_UV_INCLUDES} -I${ext_srcdir}/deps/libcat/deps/libuv/src -I${ext_srcdir}/deps/libcat/deps/libuv/include"
      SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src,
        fs-poll.c \
        idna.c \
        inet.c \
        random.c \
        strscpy.c \
        strtok.c \
        threadpool.c \
        timer.c \
        uv-common.c \
        uv-data-getter-setters.c \
        version.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)

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
        udp.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)

      dnl os-specified things
      AS_CASE([$host_os],
        [*aix*], [
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_ALL_SOURCE"
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_LINUX_SOURCE_COMPAT"
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_THREAD_SAFE"
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_XOPEN_SOURCE=500"
          AC_CHECK_HEADERS([sys/ahafs/evprods.h])
          PHP_ADD_LIBRARY(perfstat)
          SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
            aix.c \
            aix-common.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)
        ],
        [darwin*], [
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_DARWIN_UNLIMITED_SELECT"
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_DARWIN_USE_64_BIT_INODE"
          SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
            darwin.c \
            proctitle.c \
            darwin-proctitle.c \
            fsevents.c \
            random-getentropy.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)
        ],
        [linux*], [
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_GNU_SOURCE"
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_POSIX_C_SOURCE=200112"
          PHP_ADD_LIBRARY(dl)
          PHP_ADD_LIBRARY(rt)
          SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
            proctitle.c \
            linux-core.c \
            linux-inotify.c \
            linux-syscalls.c \
            procfs-exepath.c \
            random-getrandom.c \
            random-sysctl-linux.c \
            epoll.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)
        ],
        [freebsd*], [
          SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
            freebsd.c \
            bsd-proctitle.c \
            bsd-ifaddrs.c \
            posix-hrtime.c \
            random-getrandom.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)
        ],
        [netbsd*], [
          PHP_ADD_LIBRARY(kvm)
          SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
            netbsd.c \
            bsd-proctitle.c \
            bsd-ifaddrs.c \
            posix-hrtime.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)
        ],
        [openbsd*], [
          SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
            openbsd.c \
            bsd-proctitle.c \
            bsd-ifaddrs.c \
            posix-hrtime.c \
            random-getentropy.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)
        ],
        [dragonfly*], [
          SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
            freebsd.c \
            bsd-proctitle.c \
            bsd-ifaddrs.c \
            posix-hrtime.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)
        ],
        [solaris*], [
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D__EXTENSIONS__"
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_XOPEN_SOURCE=500"
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_REENTRANT"
          PHP_ADD_LIBRARY(kstat)
          PHP_ADD_LIBRARY(nsl)
          PHP_ADD_LIBRARY(sendfile)
          PHP_ADD_LIBRARY(socket)
          SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
            sunos.c \
            no-proctitle.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)
        ],
        [haiku*], [
          SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS} -D_BSD_SOURCE"
          PHP_ADD_LIBRARY(bsd)
          PHP_ADD_LIBRARY(network)
          SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix,
            haiku.c \
            posix-hrtime.c \
            posix-poll.c \
            bsd-ifaddrs.c \
            no-fsevents.c \
            no-proctitle.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)
        ]
      )

      AC_CHECK_LIB(c, kqueue, [
        AC_DEFINE(HAVE_KQUEUE, 1, [Have Kqueue])
        SWOW_ADD_SOURCES(deps/libcat/deps/libuv/src/unix, kqueue.c, SWOW_UV_INCLUDES, SWOW_UV_CFLAGS)
      ])

      dnl TODO: other platforms
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

      SWOW_CAT_INCLUDES="${SWOW_CAT_INCLUDES} \$(SWOW_UV_INCLUDES)"
      SWOW_UV_CFLAGS="${SWOW_UV_CFLAGS}"
      PHP_SUBST(SWOW_UV_INCLUDES)
      PHP_SUBST(SWOW_UV_CFLAGS)

    dnl add llhttp sources

    SWOW_LLHTTP_INCLUDES="-I${ext_srcdir}/deps/libcat/deps/llhttp/include"
    SWOW_CAT_INCLUDES="${SWOW_CAT_INCLUDES} \$(SWOW_LLHTTP_INCLUDES)"
    SWOW_LLHTTP_CFLAGS="${SWOW_LLHTTP_CFLAGS} \$(SWOW_STD_CFLAGS)"
    PHP_SUBST(SWOW_LLHTTP_INCLUDES)
    PHP_SUBST(SWOW_LLHTTP_CFLAGS)
    SWOW_ADD_SOURCES(deps/libcat/deps/llhttp/src,
      api.c \
      http.c \
      llhttp.c, SWOW_LLHTTP_INCLUDES, SWOW_LLHTTP_CFLAGS)

    dnl add multipart-parser sources

    SWOW_MULTIPART_PARSER_INCLUDES="-I${ext_srcdir}/deps/libcat/deps/multipart_parser"
    SWOW_CAT_INCLUDES="${SWOW_CAT_INCLUDES} \$(SWOW_MULTIPART_PARSER_INCLUDES)"
    SWOW_MULTIPART_PARSER_CFLAGS="${SWOW_MULTIPART_PARSER_CFLAGS} \$(SWOW_STD_CFLAGS)"
    PHP_SUBST(SWOW_MULTIPART_PARSER_INCLUDES)
    PHP_SUBST(SWOW_MULTIPART_PARSER_CFLAGS)
    SWOW_ADD_SOURCES(deps/libcat/deps/multipart_parser,
      multipart_parser.c, SWOW_MULTIPART_PARSER_INCLUDES, SWOW_MULTIPART_PARSER_CFLAGS)

    dnl prepare pkg-config
    if test -z "$PKG_CONFIG"; then
      AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
    fi
    dnl add ssl sources
    if test "x${PHP_SWOW_SSL}" != "xno" ; then
      SWOW_PKG_CHECK_MODULES([OPENSSL], openssl, 1.0.1, [PHP_SWOW_SSL], [
        dnl make changes
        AC_DEFINE([CAT_HAVE_OPENSSL], 1, [Enable libcat SSL support with OpenSSL])
        PHP_EVAL_LIBLINE($OPENSSL_LIBS, SWOW_SHARED_LIBADD)
        SWOW_CAT_INCLUDES="$SWOW_CAT_INCLUDES $OPENSSL_INCL"
        SWOW_ADD_SOURCES(deps/libcat/src, cat_ssl.c, SWOW_CAT_INCLUDES, SWOW_CAT_CFLAGS)
        dnl SWOW_ADD_SOURCES(src, swow_ssl.c, SWOW_INCLUDES, SWOW_CFLAGS)
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
        SWOW_CAT_INCLUDES="$SWOW_CAT_INCLUDES $CURL_INCL"
        SWOW_ADD_SOURCES(deps/libcat/src, cat_curl.c, SWOW_CAT_INCLUDES, SWOW_CAT_CFLAGS)
        SWOW_ADD_SOURCES(src, swow_curl.c, SWOW_INCLUDES, SWOW_CFLAGS)
      ],[
        AC_MSG_WARN([Swow cURL support not enabled: libcurl not found])
      ])
    fi

    PHP_SUBST(SWOW_CAT_INCLUDES)
    PHP_SUBST(SWOW_CAT_CFLAGS)
  fi

  dnl swow needs libcat
  SWOW_INCLUDES="$SWOW_INCLUDES \$(SWOW_CAT_INCLUDES)"
  PHP_SUBST(SWOW_INCLUDES)
  PHP_SUBST(SWOW_CFLAGS)
  PHP_SUBST(SWOW_SHARED_LIBADD)
fi
