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

#ifndef CAT_ATOMIC_H
#define CAT_ATOMIC_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

/* TODO: Fixed implementation selection
 * or provided C API for external use. */
#if __has_feature(c_atomic)
# define CAT_HAVE_C11_ATOMIC 1
#elif defined(__clang__) || CAT_GCC_VERSION >= 4007
# define CAT_HAVE_GNUC_ATOMIC 1
#elif defined(__GNUC__)
# define CAT_HAVE_SYNC_ATOMIC 1
#elif defined(CAT_OS_WIN)
# define CAT_HAVE_INTERLOCK_ATOMIC 1
#elif 1
# warning "No atomics support detected, mutex used (with poor performance)"
# define CAT_USE_MUTEX_ATOMIC 1
#else
# warning "No atomics support detected, that's terrible!"
# define CAT_HAVE_NO_ATOMIC 1
#endif

#if defined(CAT_HAVE_C11_ATOMIC)
# define CAT_ATOMIC_C11_CASE(...) __VA_ARGS__
# define CAT_ATOMIC_GNUC_CASE(...)
# define CAT_ATOMIC_INTERLOCK_CASE(...)
# define CAT_ATOMIC_SYNC_CASE(...)
# define CAT_ATOMIC_MUTEX_CASE(...)
# define CAT_ATOMIC_NO_CASE(...)
#elif defined(CAT_HAVE_GNUC_ATOMIC)
# define CAT_ATOMIC_C11_CASE(...)
# define CAT_ATOMIC_GNUC_CASE(...) __VA_ARGS__
# define CAT_ATOMIC_INTERLOCK_CASE(...)
# define CAT_ATOMIC_SYNC_CASE(...)
# define CAT_ATOMIC_MUTEX_CASE(...)
# define CAT_ATOMIC_NO_CASE(...)
#elif defined(CAT_HAVE_INTERLOCK_ATOMIC)
# define CAT_ATOMIC_C11_CASE(...)
# define CAT_ATOMIC_GNUC_CASE(...)
# define CAT_ATOMIC_INTERLOCK_CASE(...) __VA_ARGS__
# define CAT_ATOMIC_SYNC_CASE(...)
# define CAT_ATOMIC_MUTEX_CASE(...)
# define CAT_ATOMIC_NO_CASE(...)
#elif defined(CAT_HAVE_SYNC_ATOMIC)
# define CAT_ATOMIC_C11_CASE(...)
# define CAT_ATOMIC_GNUC_CASE(...)
# define CAT_ATOMIC_INTERLOCK_CASE(...)
# define CAT_ATOMIC_SYNC_CASE(...) __VA_ARGS__
# define CAT_ATOMIC_MUTEX_CASE(...)
# define CAT_ATOMIC_NO_CASE(...)
#elif defined(CAT_USE_MUTEX_ATOMIC)
# define CAT_ATOMIC_C11_CASE(...)
# define CAT_ATOMIC_GNUC_CASE(...)
# define CAT_ATOMIC_INTERLOCK_CASE(...)
# define CAT_ATOMIC_SYNC_CASE(...)
# define CAT_ATOMIC_MUTEX_CASE(...) __VA_ARGS__
# define CAT_ATOMIC_NO_CASE(...)
#else
# define CAT_ATOMIC_C11_CASE(...)
# define CAT_ATOMIC_GNUC_CASE(...)
# define CAT_ATOMIC_INTERLOCK_CASE(...)
# define CAT_ATOMIC_SYNC_CASE(...)
# define CAT_ATOMIC_MUTEX_CASE(...)
# define CAT_ATOMIC_NO_CASE(...) __VA_ARGS__
#endif

#define CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_MAP(XX) \
        XX(bool, cat_bool_t, 8,       char) \
        XX(ptr,  cat_ptr_t,  Pointer, PVOID) \

#define CAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_MAP(XX) \
        XX(int8,   int8_t,   8,  char) \
        XX(uint8,  uint8_t,  8,  char) \
        XX(int16,  int16_t,  16, short) \
        XX(uint16, uint16_t, 16, short) \
        XX(int32,  int32_t,  32, long) \
        XX(uint32, uint32_t, 32, long) \
        XX(int64,  int64_t,  64, __int64) \
        XX(uint64, uint64_t, 64, __int64) \

#ifdef CAT_HAVE_INTERLOCK_ATOMIC
# define _InterlockedOr32 _InterlockedOr
# define _InterlockedExchange32 _InterlockedExchange
# define _InterlockedExchangeAdd32 _InterlockedExchangeAdd
# define _InterlockedExchangeSub8(value, operand)  _InterlockedExchangeAdd8(value, -operand)
# define _InterlockedExchangeSub16(value, operand) _InterlockedExchangeAdd16(value, -operand)
# define _InterlockedExchangeSub32(value, operand) _InterlockedExchangeAdd32(value, -operand)
# define _InterlockedExchangeSub64(value, operand) _InterlockedExchangeAdd64(value, -operand)
#endif

#define CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \
typedef struct cat_atomic_##name##_s { \
    CAT_ATOMIC_C11_CASE( \
        _Atomic(type_name_t) value; \
    ) \
    CAT_ATOMIC_GNUC_CASE( \
        volatile type_name_t value; \
    ) \
    CAT_ATOMIC_INTERLOCK_CASE( \
        volatile interlocked_type_t value; \
    ) \
    CAT_ATOMIC_SYNC_CASE( \
        volatile type_name_t value; \
    ) \
    CAT_ATOMIC_MUTEX_CASE( \
        uv_mutex_t mutex; \
        volatile type_name_t value; \
    ) \
    CAT_ATOMIC_NO_CASE( \
        volatile type_name_t value; \
    ) \
} cat_atomic_##name##_t; \
\
static cat_always_inline void cat_atomic_##name##_init(cat_atomic_##name##_t *atomic, type_name_t value) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        __c11_atomic_init(&atomic->value, value); \
        return; \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        atomic->value = (interlocked_type_t) value; \
        return; \
    }) \
    CAT_ATOMIC_MUTEX_CASE({ \
        int error = uv_mutex_init(&atomic->mutex); \
        if (unlikely(error !== 0)) { \
            abort(); \
        } \
    }) \
    atomic->value = value; \
} \
\
static cat_always_inline void cat_atomic_##name##_destroy(volatile cat_atomic_##name##_t *atomic) \
{ \
    CAT_ATOMIC_MUTEX_CASE({ \
        uv_mutex_destroy(&atomic->mutex); \
    }) \
} \
\
static cat_always_inline void cat_atomic_##name##_store(cat_atomic_##name##_t *atomic, type_name_t desired) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        __c11_atomic_store(&atomic->value, desired, __ATOMIC_SEQ_CST); \
    }) \
    CAT_ATOMIC_GNUC_CASE({ \
        __atomic_store(&atomic->value, &desired, __ATOMIC_SEQ_CST); \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        (void) _InterlockedExchange##interlocked_suffix(&atomic->value, (interlocked_type_t) desired); \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        __sync_synchronize(); \
        atomic->value = desired; \
        __sync_synchronize(); \
    }) \
    CAT_ATOMIC_MUTEX_CASE({ \
        uv_mutex_lock(&atomic->mutex); \
        atomic->value = desired; \
        uv_mutex_unlock(&atomic->mutex); \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        atomic->value = desired; \
    }) \
} \
\
static cat_always_inline type_name_t cat_atomic_##name##_load(const volatile cat_atomic_##name##_t *atomic) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_load(&atomic->value, __ATOMIC_SEQ_CST); \
    }) \
    CAT_ATOMIC_GNUC_CASE({ \
        type_name_t ret; \
        __atomic_load(&atomic->value, &ret, __ATOMIC_SEQ_CST); \
        return ret; \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        return (type_name_t) _InterlockedOr##interlocked_suffix(&(((cat_atomic_##name##_t *) atomic)->value), false); \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        return __sync_fetch_and_or(&(((cat_atomic_##name##_t *) atomic)->value), false); \
    }) \
    CAT_ATOMIC_MUTEX_CASE({ \
        uv_mutex_lock(&atomic->mutex); \
        type_name_t value = atomic->value; \
        uv_mutex_unlock(&atomic->mutex); \
        return value; \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        return atomic->value; \
    }) \
} \
\
static cat_always_inline type_name_t cat_atomic_##name##_exchange(volatile cat_atomic_##name##_t *atomic, type_name_t desired) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_exchange(&atomic->value, desired, __ATOMIC_SEQ_CST); \
    }) \
    CAT_ATOMIC_GNUC_CASE({ \
        type_name_t ret; \
        __atomic_exchange(&atomic->value, &desired, &ret, __ATOMIC_SEQ_CST); \
        return ret; \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        return _InterlockedExchange##interlocked_suffix(&atomic->value, (interlocked_type_t) desired); \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        type_name_t ret = __sync_lock_test_and_set(&atomic->value, desired); \
        __sync_synchronize(); \
        return ret; \
    }) \
    CAT_ATOMIC_MUTEX_CASE({ \
        uv_mutex_lock(&atomic->mutex); \
        atomic->value = true; \
        uv_mutex_unlock(&atomic->mutex); \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        type_name_t ret = atomic->value; \
        atomic->value = true; \
        return ret; \
    }) \
} \

#define CAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \
static cat_always_inline type_name_t cat_atomic_##name##_fetch_add(cat_atomic_##name##_t *atomic, type_name_t operand) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_fetch_add(&atomic->value, operand, __ATOMIC_SEQ_CST); \
    }) \
    CAT_ATOMIC_GNUC_CASE({ \
        return __atomic_fetch_add(&atomic->value, operand, __ATOMIC_SEQ_CST); \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        return (type_name_t) _InterlockedExchangeAdd##interlocked_suffix(&atomic->value, (interlocked_type_t) operand); \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        return __sync_fetch_and_add(&atomic->value, operand); \
    }) \
    CAT_ATOMIC_MUTEX_CASE({ \
        uv_mutex_lock(&atomic->mutex); \
        atomic->value += desired; \
        uv_mutex_unlock(&atomic->mutex); \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        atomic->value += desired; \
    }) \
} \
\
static cat_always_inline type_name_t cat_atomic_##name##_fetch_sub(cat_atomic_##name##_t *atomic, type_name_t operand) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_fetch_sub(&atomic->value, operand, __ATOMIC_SEQ_CST); \
    }) \
    CAT_ATOMIC_GNUC_CASE({ \
        return __atomic_fetch_sub(&atomic->value, operand, __ATOMIC_SEQ_CST); \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        return (type_name_t) _InterlockedExchangeSub##interlocked_suffix(&atomic->value, (interlocked_type_t) operand); \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        return __sync_fetch_and_sub(&atomic->value, operand); \
    }) \
    CAT_ATOMIC_MUTEX_CASE({ \
        uv_mutex_lock(&atomic->mutex); \
        atomic->value -= desired; \
        uv_mutex_unlock(&atomic->mutex); \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        atomic->value -= desired; \
    }) \
}

#define CAT_ATOMIC_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \
        CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \
        CAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \

CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_MAP(CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_GEN)
CAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_MAP(CAT_ATOMIC_OPERATION_FUNCTIONS_GEN)

#ifdef __cplusplus
}
#endif
#endif /* CAT_ATOMIC_H */
