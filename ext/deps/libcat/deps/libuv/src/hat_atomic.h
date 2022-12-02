/*
  +--------------------------------------------------------------------------+
  | libhat                                                                   |
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

#ifndef HAT_ATOMIC_H
#define HAT_ATOMIC_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef __has_feature
# define __has_feature(x) 0
#endif

# if defined(__GNUC__)
#  if __GNUC__ >= 3
#   define hat_atomic_inline inline __attribute__((always_inline))
#  else
#   define hat_atomic_inline inline
#  endif
# elif defined(_MSC_VER)
#  define hat_atomic_inline __forceinline
# else
#  if __has_attribute(always_inline)
#   define hat_atomic_inline inline __attribute__((always_inline))
#  else
#   define hat_atomic_inline inline
#  endif
# endif

#ifndef HAT_PTR_T
# define HAT_PTR_T hat_ptr_t
typedef void *hat_ptr_t;
#endif

/* TODO: Fixed implementation selection
 * or provided C API for external use. */
#if __has_feature(c_atomic)
# define HAT_HAVE_C11_ATOMIC 1
#elif defined(__clang__) || HAT_GCC_VERSION >= 4007
# define HAT_HAVE_GNUC_ATOMIC 1
#elif defined(__GNUC__)
# define HAT_HAVE_SYNC_ATOMIC 1
#elif defined(HAT_OS_WIN)
# define HAT_HAVE_INTERLOCK_ATOMIC 1
#else
# error "No atomics support detected, that's terrible!"
#endif

#ifdef HAT_HAVE_INTERLOCK_ATOMIC
# include <intrin.h>
#endif

#if defined(HAT_HAVE_C11_ATOMIC)
# define HAT_ATOMIC_C11_CASE(...) __VA_ARGS__
# define HAT_ATOMIC_GNUC_CASE(...)
# define HAT_ATOMIC_INTERLOCK_CASE(...)
# define HAT_ATOMIC_SYNC_CASE(...)
# define HAT_ATOMIC_NO_CASE(...)
#elif defined(HAT_HAVE_GNUC_ATOMIC)
# define HAT_ATOMIC_C11_CASE(...)
# define HAT_ATOMIC_GNUC_CASE(...) __VA_ARGS__
# define HAT_ATOMIC_INTERLOCK_CASE(...)
# define HAT_ATOMIC_SYNC_CASE(...)
# define HAT_ATOMIC_NO_CASE(...)
#elif defined(HAT_HAVE_INTERLOCK_ATOMIC)
# define HAT_ATOMIC_C11_CASE(...)
# define HAT_ATOMIC_GNUC_CASE(...)
# define HAT_ATOMIC_INTERLOCK_CASE(...) __VA_ARGS__
# define HAT_ATOMIC_SYNC_CASE(...)
# define HAT_ATOMIC_NO_CASE(...)
#elif defined(HAT_HAVE_SYNC_ATOMIC)
# define HAT_ATOMIC_C11_CASE(...)
# define HAT_ATOMIC_GNUC_CASE(...)
# define HAT_ATOMIC_INTERLOCK_CASE(...)
# define HAT_ATOMIC_SYNC_CASE(...) __VA_ARGS__
# define HAT_ATOMIC_NO_CASE(...)
#else
# define HAT_ATOMIC_C11_CASE(...)
# define HAT_ATOMIC_GNUC_CASE(...)
# define HAT_ATOMIC_INTERLOCK_CASE(...)
# define HAT_ATOMIC_SYNC_CASE(...)
# define HAT_ATOMIC_NO_CASE(...) __VA_ARGS__
#endif

#define HAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_MAP(XX) \
        XX(bool, uint8_t, 8,       char) \
        XX(ptr,  hat_ptr_t,  Pointer, PVOID) \

#define HAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_MAP(XX) \
        XX(int8,   int8_t,   8,  char) \
        XX(uint8,  uint8_t,  8,  char) \
        XX(int16,  int16_t,  16, short) \
        XX(uint16, uint16_t, 16, short) \
        XX(int32,  int32_t,  32, long) \
        XX(uint32, uint32_t, 32, long) \
        XX(int64,  int64_t,  64, __int64) \
        XX(uint64, uint64_t, 64, __int64) \

# if defined(HAT_HAVE_GNUC_ATOMIC)
# define __atomic_compare_exchange_strong(atomic, expected, desired) \
         __atomic_compare_exchange_n(atomic, expected, desired, 0 /* not weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
# define __atomic_compare_exchange_weak(atomic, expected, desired) \
         __atomic_compare_exchange_n(atomic, expected, desired, 1 /* weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
# elif defined(HAT_HAVE_INTERLOCK_ATOMIC)
# define _InterlockedOr32                          _InterlockedOr
# define _InterlockedExchange32                    _InterlockedExchange
# define _InterlockedExchangeAdd32                 _InterlockedExchangeAdd
# define _InterlockedCompareExchange32             _InterlockedCompareExchange
# define _InterlockedLoad8(value)                  _InterlockedOr8(value, 0)
# define _InterlockedLoad16(value)                 _InterlockedOr16(value, 0)
# define _InterlockedLoad32(value)                 _InterlockedOr(value, 0)
# define _InterlockedLoad64(value)                 _InterlockedOr64(value, 0)
# define _InterlockedLoadPointer(value)            _InterlockedCompareExchangePointer(value, NULL, NULL)
# define _InterlockedExchangeSub8(value, operand)  _InterlockedExchangeAdd8(value, -operand)
# define _InterlockedExchangeSub16(value, operand) _InterlockedExchangeAdd16(value, -operand)
# define _InterlockedExchangeSub32(value, operand) _InterlockedExchangeAdd32(value, -operand)
# define _InterlockedExchangeSub64(value, operand) _InterlockedExchangeAdd64(value, -operand)
#endif

#define HAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \
typedef struct hat_atomic_##name##_s { \
    HAT_ATOMIC_C11_CASE( \
        _Atomic(type_name_t) value; \
    ) \
    HAT_ATOMIC_GNUC_CASE( \
        volatile type_name_t value; \
    ) \
    HAT_ATOMIC_INTERLOCK_CASE( \
        volatile interlocked_type_t value; \
    ) \
    HAT_ATOMIC_SYNC_CASE( \
        volatile type_name_t value; \
    ) \
    HAT_ATOMIC_NO_CASE( \
        volatile type_name_t value; \
    ) \
} hat_atomic_##name##_t; \
\
static hat_atomic_inline void hat_atomic_##name##_init(hat_atomic_##name##_t *atomic, type_name_t desired) \
{ \
    HAT_ATOMIC_C11_CASE({ \
        __c11_atomic_init(&atomic->value, desired); \
    }) \
    HAT_ATOMIC_GNUC_CASE({ \
        atomic->value = desired; \
    }) \
    HAT_ATOMIC_INTERLOCK_CASE({ \
        atomic->value = (interlocked_type_t) desired; \
    }) \
    HAT_ATOMIC_SYNC_CASE({ \
        atomic->value = desired; \
    }) \
    HAT_ATOMIC_NO_CASE({ \
        atomic->value = desired; \
    }) \
} \
\
static hat_atomic_inline void hat_atomic_##name##_store(hat_atomic_##name##_t *atomic, type_name_t desired) \
{ \
    HAT_ATOMIC_C11_CASE({ \
        __c11_atomic_store(&atomic->value, desired, __ATOMIC_SEQ_CST); \
    }) \
    HAT_ATOMIC_GNUC_CASE({ \
        __atomic_store(&atomic->value, &desired, __ATOMIC_SEQ_CST); \
    }) \
    HAT_ATOMIC_INTERLOCK_CASE({ \
        (void) _InterlockedExchange##interlocked_suffix(&atomic->value, (interlocked_type_t) desired); \
    }) \
    HAT_ATOMIC_SYNC_CASE({ \
        (void) __sync_val_compare_and_swap(&atomic->value, atomic->value, desired); \
    }) \
    HAT_ATOMIC_NO_CASE({ \
        atomic->value = desired; \
    }) \
} \
\
static hat_atomic_inline type_name_t hat_atomic_##name##_load(const hat_atomic_##name##_t *atomic) \
{ \
    HAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_load(&atomic->value, __ATOMIC_SEQ_CST); \
    }) \
    HAT_ATOMIC_GNUC_CASE({ \
        type_name_t ret; \
        __atomic_load(&atomic->value, &ret, __ATOMIC_SEQ_CST); \
        return ret; \
    }) \
    HAT_ATOMIC_INTERLOCK_CASE({ \
        return (type_name_t) _InterlockedLoad##interlocked_suffix(&(((hat_atomic_##name##_t *) atomic)->value)); \
    }) \
    HAT_ATOMIC_SYNC_CASE({ \
        return __sync_fetch_and_or(&(((hat_atomic_##name##_t *) atomic)->value), 0); \
    }) \
    HAT_ATOMIC_NO_CASE({ \
        return atomic->value; \
    }) \
} \
\
static hat_atomic_inline type_name_t hat_atomic_##name##_exchange(hat_atomic_##name##_t *atomic, type_name_t desired) \
{ \
    HAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_exchange(&atomic->value, desired, __ATOMIC_SEQ_CST); \
    }) \
    HAT_ATOMIC_GNUC_CASE({ \
        type_name_t ret; \
        __atomic_exchange(&atomic->value, &desired, &ret, __ATOMIC_SEQ_CST); \
        return ret; \
    }) \
    HAT_ATOMIC_INTERLOCK_CASE({ \
        return _InterlockedExchange##interlocked_suffix(&atomic->value, (interlocked_type_t) desired); \
    }) \
    HAT_ATOMIC_SYNC_CASE({ \
        return __sync_val_compare_and_swap(&atomic->value, atomic->value, desired); \
    }) \
    HAT_ATOMIC_NO_CASE({ \
        type_name_t ret = atomic->value; \
        atomic->value = desired; \
        return ret; \
    }) \
} \
\
static hat_atomic_inline uint8_t hat_atomic_##name##_compare_exchange_strong(hat_atomic_##name##_t *atomic, type_name_t *expected, type_name_t desired) \
{ \
    HAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_compare_exchange_strong(&atomic->value, expected, desired, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
    }) \
    HAT_ATOMIC_GNUC_CASE({ \
        return __atomic_compare_exchange_strong(&atomic->value, expected, desired); \
    }) \
    HAT_ATOMIC_INTERLOCK_CASE({ \
        type_name_t value = (type_name_t) _InterlockedCompareExchange##interlocked_suffix(&atomic->value, (interlocked_type_t) desired, (interlocked_type_t) (*expected)); \
        if (value == *expected) { \
            return 1; \
        } else { \
            *expected = value; \
            return 0; \
        } \
    }) \
    HAT_ATOMIC_SYNC_CASE({ \
        type_name_t value = __sync_val_compare_and_swap(&atomic->value, *expected, desired); \
         if (value == *expected) { \
            return 1; \
        } else { \
            *expected = value; \
            return 0; \
        } \
    }) \
    HAT_ATOMIC_NO_CASE({ \
        if (atomic->value == *expected) { \
            atomic->value = desired; \
            return 1; \
        } else { \
            *expected = atomic->value; \
            return 0; \
        } \
    }) \
} \
\
static hat_atomic_inline uint8_t hat_atomic_##name##_compare_exchange_weak(hat_atomic_##name##_t *atomic, type_name_t *expected, type_name_t desired) \
{ \
    HAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_compare_exchange_weak(&atomic->value, expected, desired, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
    }) \
    HAT_ATOMIC_GNUC_CASE({ \
        return __atomic_compare_exchange_weak(&atomic->value, expected, desired); \
    }) \
    HAT_ATOMIC_INTERLOCK_CASE({ \
        return hat_atomic_##name##_compare_exchange_strong(atomic, expected, desired); \
    }) \
    HAT_ATOMIC_SYNC_CASE({ \
        return hat_atomic_##name##_compare_exchange_strong(atomic, expected, desired); \
    }) \
    HAT_ATOMIC_NO_CASE({ \
        return hat_atomic_##name##_compare_exchange_strong(atomic, expected, desired); \
    }) \
}

#define HAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \
static hat_atomic_inline type_name_t hat_atomic_##name##_fetch_add(hat_atomic_##name##_t *atomic, type_name_t operand) \
{ \
    HAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_fetch_add(&atomic->value, operand, __ATOMIC_SEQ_CST); \
    }) \
    HAT_ATOMIC_GNUC_CASE({ \
        return __atomic_fetch_add(&atomic->value, operand, __ATOMIC_SEQ_CST); \
    }) \
    HAT_ATOMIC_INTERLOCK_CASE({ \
        return (type_name_t) _InterlockedExchangeAdd##interlocked_suffix(&atomic->value, (interlocked_type_t) operand); \
    }) \
    HAT_ATOMIC_SYNC_CASE({ \
        return __sync_fetch_and_add(&atomic->value, operand); \
    }) \
    HAT_ATOMIC_NO_CASE({ \
        type_name_t ret = atomic->value; \
        atomic->value += operand; \
        return ret; \
    }) \
} \
\
static hat_atomic_inline type_name_t hat_atomic_##name##_fetch_sub(hat_atomic_##name##_t *atomic, type_name_t operand) \
{ \
    HAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_fetch_sub(&atomic->value, operand, __ATOMIC_SEQ_CST); \
    }) \
    HAT_ATOMIC_GNUC_CASE({ \
        return __atomic_fetch_sub(&atomic->value, operand, __ATOMIC_SEQ_CST); \
    }) \
    HAT_ATOMIC_INTERLOCK_CASE({ \
        return (type_name_t) _InterlockedExchangeSub##interlocked_suffix(&atomic->value, (interlocked_type_t) operand); \
    }) \
    HAT_ATOMIC_SYNC_CASE({ \
        return __sync_fetch_and_sub(&atomic->value, operand); \
    }) \
    HAT_ATOMIC_NO_CASE({ \
        type_name_t ret = atomic->value; \
        atomic->value -= operand; \
        return ret; \
    }) \
}

#define HAT_ATOMIC_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \
        HAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \
        HAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \

HAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_MAP(HAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_GEN)
HAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_MAP(HAT_ATOMIC_OPERATION_FUNCTIONS_GEN)

#ifdef __cplusplus
}
#endif
#endif /* HAT_ATOMIC_H */
