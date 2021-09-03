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
  | Author: Twosee <twosee@php.net>                                          |
  +--------------------------------------------------------------------------+
 */

/* GCC x.y.z supplies __GNUC__ = x and __GNUC_MINOR__ = y */
#ifdef __GNUC__
#define CAT_GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
#else
#define CAT_GCC_VERSION 0
#endif

/* compatibility with non-clang compilers */
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif
#ifndef __has_feature
#define __has_feature(x) 0
#endif

/* built-in */

#if !defined(__GNUC__) || __GNUC__ < 3
#define __builtin_expect(x, expected_value) (x)
#endif
#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#if defined(CAT_OS_WIN) && !defined(__clang__)
#define CAT_ASSUME(x) __assume(x)
#elif ((defined(__GNUC__) && CAT_GCC_VERSION >= 4005) || __has_builtin(__builtin_unreachable))
#define CAT_ASSUME(x) do { \
    if (unlikely(!(x))) { __builtin_unreachable(); } \
} while (0)
#else
#define CAT_NO_ASSUME
#define CAT_ASSUME(x)
#endif

#ifdef CAT_DEBUG
#define CAT_ASSERT(x) assert(x)
#else
#define CAT_ASSERT(x) CAT_ASSUME(x)
#endif

#ifdef CAT_DEBUG
#define CAT_SHOULD_BE(x) CAT_ASSERT(x)
#else
#ifndef CAT_NO_ASSUME
#define CAT_SHOULD_BE(x) CAT_ASSUME(x)
#else
#define CAT_SHOULD_BE(x) do { \
    if (unlikely(!(x))) { abort(); } \
} while (0)
#endif
#endif

#ifndef CAT_IDE_HELPER
#define CAT_STATIC_ASSERT(expression) void cat_static_assert(int static_assert_failed[1 - 2 * !(expression)])
#else
/* make IDE happy (unused function) */
#define CAT_STATIC_ASSERT(expression) int static_assert_failed[1 - 2 * !(expression)]
#endif

#if !defined(CAT_OS_WIN) && !defined(CAT_IDE_HELPER)
#define CAT_NEVER_HERE(reason) CAT_ASSERT(0 && reason)
#else
#define CAT_NEVER_HERE(reason) abort()
#endif

/* pseudo fallthrough keyword; */
#if defined(__GNUC__) && __GNUC__ >= 7
# define CAT_FALLTHROUGH __attribute__((__fallthrough__))
#else
# define CAT_FALLTHROUGH ((void) 0)
#endif

/* function prefix */

#ifdef CAT_DEBUG
#  define cat_always_inline inline
#  define cat_never_inline
#else
#  ifdef CAT_OS_WIN
#    ifdef inline
#      undef inline
#    endif
#    define inline __inline
#  endif
#  if defined(__GNUC__)
#    if __GNUC__ >= 3
#      define cat_always_inline inline __attribute__((always_inline))
#      define cat_never_inline __attribute__((noinline))
#    else
#      define cat_always_inline inline
#      define cat_never_inline
#    endif
#  elif defined(_MSC_VER)
#    define cat_always_inline __forceinline
#    define cat_never_inline __declspec(noinline)
#  else
#    if __has_attribute(always_inline)
#      define cat_always_inline inline __attribute__((always_inline))
#    else
#      define cat_always_inline inline
#    endif
#    if __has_attribute(noinline)
#      define cat_never_inline __attribute__((noinline))
#    else
#      define cat_never_inline
#    endif
#  endif
#endif /* CAT_DEBUG */

#if defined(CAT_SHARED_BUILD) && defined(CAT_SHARED_USE)
#error "Define either CAT_SHARED_BUILD or CAT_SHARED_USE, not both"
#endif

#ifndef CAT_OS_WIN
#  if defined(__GNUC__) && __GNUC__ >= 4
#    define CAT_API __attribute__ ((visibility("default")))
#  else
#    define CAT_API
#  endif
#else
   /* Windows - set up dll import/export decorators */
#  if defined(CAT_SHARED_BUILD)
     /* Building shared library */
#    define CAT_API __declspec(dllexport)
#  elif defined(CAT_SHARED_USE)
     /* Using shared library */
#    define CAT_API __declspec(dllimport)
#  else
     /* Building static library */
#    define CAT_API /* nothing */
#  endif
#endif

#if CAT_GCC_VERSION >= 4003 || __has_attribute(unused)
#  define CAT_ATTRIBUTE_UNUSED __attribute__((unused))
# else
#  define CAT_ATTRIBUTE_UNUSED
# endif

#if defined(__GNUC__) && CAT_GCC_VERSION >= 4003
#define CAT_COLD __attribute__((cold))
#define CAT_HOT __attribute__((hot))
#ifdef __OPTIMIZE__
#  define CAT_OPT_SIZE  __attribute__((optimize("Os")))
#  define CAT_OPT_SPEED __attribute__((optimize("Ofast")))
#else
#  define CAT_OPT_SIZE
#  define CAT_OPT_SPEED
#endif
#else
#define CAT_COLD
#define CAT_HOT
#define CAT_OPT_SIZE
#define CAT_OPT_SPEED
#endif

#if defined(__GNUC__) && CAT_GCC_VERSION >= 5000
# define CAT_ATTRIBUTE_UNUSED_LABEL __attribute__((cold, unused));
# define CAT_ATTRIBUTE_COLD_LABEL __attribute__((cold));
# define CAT_ATTRIBUTE_HOT_LABEL __attribute__((hot));
#else
# define CAT_ATTRIBUTE_UNUSED_LABEL
# define CAT_ATTRIBUTE_COLD_LABEL
# define CAT_ATTRIBUTE_HOT_LABEL
#endif

#if defined(__GNUC__) && CAT_GCC_VERSION >= 3004 && defined(__i386__)
# define CAT_FASTCALL __attribute__((fastcall))
#elif defined(_MSC_VER) && defined(_M_IX86) && _MSC_VER == 1700
# define CAT_FASTCALL __fastcall
#elif defined(_MSC_VER) && _MSC_VER >= 1800 && !defined(__clang__)
# define CAT_FASTCALL __vectorcall
#else
# define CAT_FASTCALL
#endif

#if (defined(__GNUC__) && __GNUC__ >= 3 && !defined(__INTEL_COMPILER) && !defined(DARWIN) && !defined(__hpux) && !defined(_AIX) && !defined(__osf__)) || __has_attribute(noreturn)
#define CAT_HAVE_NORETURN
#define CAT_NORETURN __attribute__((noreturn))
#elif defined(CAT_WIN32)
#define CAT_HAVE_NORETURN
#define CAT_NORETURN __declspec(noreturn)
#else
#define CAT_NORETURN
#endif

#ifdef __GNUC__  /* Also covers __clang__ and __INTEL_COMPILER */
#define CAT_DESTRUCTOR __attribute__((destructor))
#else
#define CAT_DESTRUCTOR
#endif

/* function special suffix */

#define CAT_INTERNAL   /* The API is designed for internal use, it is not recommended unless there are special circumstances */
#define CAT_UNSAFE     /* incorrect usage can lead to security problem */
#define CAT_FREE       /* return ptr should be free'd by cat_free */
#define CAT_MAY_FREE   /* return ptr should be free'd by cat_free if argument is null */

/* function suffix attributes */

#if CAT_GCC_VERSION >= 2007 || __has_attribute(format)
#define CAT_ATTRIBUTE_FORMAT(type, idx, first) __attribute__ ((format(type, idx, first)))
#else
#define CAT_ATTRIBUTE_FORMAT(type, idx, first)
#endif

#if (CAT_GCC_VERSION >= 3001 && !defined(__INTEL_COMPILER)) || __has_attribute(format)
#define CAT_ATTRIBUTE_PTR_FORMAT(type, idx, first) __attribute__ ((format(type, idx, first)))
#else
#define CAT_ATTRIBUTE_PTR_FORMAT(type, idx, first)
#endif

/* source position */

#ifdef CAT_DEBUG
#define CAT_SOURCE_POSITION             1
#ifndef CAT_DISABLE___FUNCTION__
#define CAT_SOURCE_POSITION_D           const char *function, const char *file, const unsigned int line
#define CAT_SOURCE_POSITION_RELAY_C     function, file, line
#define CAT_SOURCE_POSITION_C           __FUNCTION__, __FILE__, __LINE__
#define CAT_SOURCE_POSITION_FMT         "in %s in %s on line %d"
#define CAT_SOURCE_POSITION_STRUCT_D    const char *function; const char *file; const unsigned int line;
#else
#define CAT_SOURCE_POSITION_D           const char *file, const unsigned int line
#define CAT_SOURCE_POSITION_RELAY_C     file, line
#define CAT_SOURCE_POSITION_C           __FILE__, __LINE__
#define CAT_SOURCE_POSITION_FMT         "in %s on line %d"
#define CAT_SOURCE_POSITION_STRUCT_D    const char *file; const unsigned int line;
#endif
#define CAT_SOURCE_POSITION_DC          , CAT_SOURCE_POSITION_D
#define CAT_SOURCE_POSITION_RELAY_CC    , CAT_SOURCE_POSITION_RELAY_C
#define CAT_SOURCE_POSITION_CC          , CAT_SOURCE_POSITION_C
#else
#define CAT_SOURCE_POSITION_D
#define CAT_SOURCE_POSITION_DC
#define CAT_SOURCE_POSITION_RELAY_C
#define CAT_SOURCE_POSITION_RELAY_CC
#define CAT_SOURCE_POSITION_C
#define CAT_SOURCE_POSITION_CC
#define CAT_SOURCE_POSITION_FMT
#define CAT_SOURCE_POSITION_STRUCT_D
#endif

/* code generator */

#define CAT_ENUM_EMPTY_GEN(name, value)
#define CAT_ENUM_GEN(prefix, name, value) prefix##name = (value),

/* sanitizer */

#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#define CAT_HAVE_ASAN 1
#endif
