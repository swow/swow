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
#if __has_feature(c_atomic) && defined(__clang__)
# define CAT_HAVE_C11_ATOMIC 1
#elif CAT_GCC_VERSION >= 4007
# define CAT_HAVE_GNUC_ATOMIC 1
#elif defined(__GNUC__)
# define CAT_HAVE_SYNC_ATOMIC 1
#elif defined(CAT_OS_WIN)
# define CAT_HAVE_INTERLOCK_ATOMIC 1
#else
# error "No atomics support detected, that's terrible!"
#endif

#ifdef CAT_HAVE_INTERLOCK_ATOMIC
# include <intrin.h>
#endif

#if defined(CAT_HAVE_C11_ATOMIC)
# define CAT_ATOMIC_C11_CASE(...) __VA_ARGS__
# define CAT_ATOMIC_GNUC_CASE(...)
# define CAT_ATOMIC_INTERLOCK_CASE(...)
# define CAT_ATOMIC_SYNC_CASE(...)
# define CAT_ATOMIC_NO_CASE(...)
#elif defined(CAT_HAVE_GNUC_ATOMIC)
# define CAT_ATOMIC_C11_CASE(...)
# define CAT_ATOMIC_GNUC_CASE(...) __VA_ARGS__
# define CAT_ATOMIC_INTERLOCK_CASE(...)
# define CAT_ATOMIC_SYNC_CASE(...)
# define CAT_ATOMIC_NO_CASE(...)
#elif defined(CAT_HAVE_INTERLOCK_ATOMIC)
# define CAT_ATOMIC_C11_CASE(...)
# define CAT_ATOMIC_GNUC_CASE(...)
# define CAT_ATOMIC_INTERLOCK_CASE(...) __VA_ARGS__
# define CAT_ATOMIC_SYNC_CASE(...)
# define CAT_ATOMIC_NO_CASE(...)
#elif defined(CAT_HAVE_SYNC_ATOMIC)
# define CAT_ATOMIC_C11_CASE(...)
# define CAT_ATOMIC_GNUC_CASE(...)
# define CAT_ATOMIC_INTERLOCK_CASE(...)
# define CAT_ATOMIC_SYNC_CASE(...) __VA_ARGS__
# define CAT_ATOMIC_NO_CASE(...)
#else
# define CAT_ATOMIC_C11_CASE(...)
# define CAT_ATOMIC_GNUC_CASE(...)
# define CAT_ATOMIC_INTERLOCK_CASE(...)
# define CAT_ATOMIC_SYNC_CASE(...)
# define CAT_ATOMIC_NO_CASE(...) __VA_ARGS__
#endif

#define CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_MAP(XX) \
        XX(bool, cat_bool_t, 8,       char) \
        XX(ptr,  cat_ptr_t,  Pointer, PVOID) \
        XX(uintptr, uintptr_t, Pointer, PVOID) \

#define CAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_MAP(XX) \
        XX(int8,   int8_t,   8,  char) \
        XX(uint8,  uint8_t,  8,  char) \
        XX(int16,  int16_t,  16, short) \
        XX(uint16, uint16_t, 16, short) \
        XX(int32,  int32_t,  32, long) \
        XX(uint32, uint32_t, 32, long) \
        XX(int64,  int64_t,  64, __int64) \
        XX(uint64, uint64_t, 64, __int64) \

# if defined(CAT_HAVE_GNUC_ATOMIC)
# define __atomic_compare_exchange_strong(atomic, expected, desired) \
         __atomic_compare_exchange_n(atomic, expected, desired, 0 /* not weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
# define __atomic_compare_exchange_weak(atomic, expected, desired) \
         __atomic_compare_exchange_n(atomic, expected, desired, 1 /* weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
# elif defined(CAT_HAVE_INTERLOCK_ATOMIC)
/* alias */
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
/* not exist at any platform */
# define _InterlockedExchange32_acq                _InterlockedExchange
# define _InterlockedExchange32_rel                _InterlockedExchange
# define _InterlockedExchange32_nf                 _InterlockedExchange
# define _InterlockedLoad8_acq(value)              _InterlockedOr8_acq(value, 0)
# define _InterlockedLoad16_acq(value)             _InterlockedOr16_acq(value, 0)
# define _InterlockedLoad32_acq(value)             _InterlockedOr_acq(value, 0)
# define _InterlockedLoad64_acq(value)             _InterlockedOr64_acq(value, 0)
# define _InterlockedLoad8_nf(value)               _InterlockedOr8_nf(value, 0)
# define _InterlockedLoad16_nf(value)              _InterlockedOr16_nf(value, 0)
# define _InterlockedLoad32_nf(value)              _InterlockedOr_nf(value, 0)
# define _InterlockedLoad64_nf(value)              _InterlockedOr64_nf(value, 0)
# define _InterlockedLoadPointer_acq               _InterlockedLoadPointer
# define _InterlockedLoadPointer_nf                _InterlockedLoadPointer
# ifdef _M_X64
/* not exist at x86 */
#  define _InterlockedExchange8_acq _InterlockedExchange8
#  define _InterlockedExchange8_rel _InterlockedExchange8
#  define _InterlockedExchange8_nf _InterlockedExchange8
#  define _InterlockedExchange16_acq _InterlockedExchange16
#  define _InterlockedExchange16_rel _InterlockedExchange16
#  define _InterlockedExchange16_nf _InterlockedExchange16
#  define _InterlockedExchange_acq _InterlockedExchange
#  define _InterlockedExchange_rel _InterlockedExchange
#  define _InterlockedExchange_nf _InterlockedExchange
#  define _InterlockedExchange64_acq _InterlockedExchange64
#  define _InterlockedExchange64_rel _InterlockedExchange64
#  define _InterlockedExchange64_nf _InterlockedExchange64
#  define _InterlockedOr8_acq _InterlockedOr8
#  define _InterlockedOr8_nf _InterlockedOr8
#  define _InterlockedOr16_acq _InterlockedOr16
#  define _InterlockedOr16_nf _InterlockedOr16
#  define _InterlockedOr_acq _InterlockedOr
#  define _InterlockedOr_nf _InterlockedOr
#  define _InterlockedOr64_acq _InterlockedOr64
#  define _InterlockedOr64_nf _InterlockedOr64
#  define _InterlockedExchangePointer_acq _InterlockedExchangePointer
#  define _InterlockedExchangePointer_nf _InterlockedExchangePointer
#  define _InterlockedExchangePointer_rel _InterlockedExchangePointer
# endif
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
    CAT_ATOMIC_NO_CASE( \
        volatile type_name_t value; \
    ) \
} cat_atomic_##name##_t; \
\
static cat_always_inline void cat_atomic_##name##_init(cat_atomic_##name##_t *atomic, type_name_t desired) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        __c11_atomic_init(&atomic->value, desired); \
    }) \
    CAT_ATOMIC_GNUC_CASE({ \
        atomic->value = desired; \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        atomic->value = (interlocked_type_t) desired; \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        atomic->value = desired; \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        atomic->value = desired; \
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
        (void) __sync_val_compare_and_swap(&atomic->value, atomic->value, desired); \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        atomic->value = desired; \
    }) \
} \
\
static cat_always_inline void cat_atomic_##name##_store_explicit(cat_atomic_##name##_t *atomic, type_name_t desired, cat_atomic_memory_order_t order) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        switch (order) { \
            case CAT_ATOMIC_MEMORY_ORDER_RELAXED: \
                __c11_atomic_store(&atomic->value, desired, __ATOMIC_RELAXED); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_RELEASE: \
                __c11_atomic_store(&atomic->value, desired, __ATOMIC_RELEASE); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_SEQ_CST: \
                __c11_atomic_store(&atomic->value, desired, __ATOMIC_SEQ_CST); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_ACQUIRE: \
            case CAT_ATOMIC_MEMORY_ORDER_CONSUME: \
            case CAT_ATOMIC_MEMORY_ORDER_ACQ_REL: \
                CAT_NEVER_HERE("Invalid memory order for store operation"); \
            default: \
                CAT_NEVER_HERE("Unknown memory order"); \
        } \
    }) \
    CAT_ATOMIC_GNUC_CASE({ \
        switch (order) { \
            case CAT_ATOMIC_MEMORY_ORDER_RELAXED: \
                __atomic_store(&atomic->value, &desired, __ATOMIC_RELAXED); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_RELEASE: \
                __atomic_store(&atomic->value, &desired, __ATOMIC_RELEASE); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_SEQ_CST: \
                __atomic_store(&atomic->value, &desired, __ATOMIC_SEQ_CST); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_ACQUIRE: \
            case CAT_ATOMIC_MEMORY_ORDER_CONSUME: \
            case CAT_ATOMIC_MEMORY_ORDER_ACQ_REL: \
                CAT_NEVER_HERE("Invalid memory order for store operation"); \
            default: \
                CAT_NEVER_HERE("Unknown memory order"); \
        } \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        switch (order) { \
            case CAT_ATOMIC_MEMORY_ORDER_ACQ_REL: \
            case CAT_ATOMIC_MEMORY_ORDER_SEQ_CST: \
                (void) _InterlockedExchange##interlocked_suffix(&atomic->value, (interlocked_type_t) desired); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_ACQUIRE: \
            case CAT_ATOMIC_MEMORY_ORDER_CONSUME: \
                (void) _InterlockedExchange##interlocked_suffix##_acq(&atomic->value, (interlocked_type_t) desired); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_RELEASE: \
                (void) _InterlockedExchange##interlocked_suffix##_rel(&atomic->value, (interlocked_type_t) desired); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_RELAXED: \
                (void) _InterlockedExchange##interlocked_suffix##_nf(&atomic->value, (interlocked_type_t) desired); \
                break; \
            default: \
                CAT_NEVER_HERE("Unknown memory order"); \
        } \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        (void) __sync_val_compare_and_swap(&atomic->value, atomic->value, desired); \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        atomic->value = desired; \
    }) \
} \
\
static cat_always_inline type_name_t cat_atomic_##name##_load(const cat_atomic_##name##_t *atomic) \
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
        return (type_name_t) _InterlockedLoad##interlocked_suffix(&(((cat_atomic_##name##_t *) atomic)->value)); \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        return __sync_fetch_and_or(&(((cat_atomic_##name##_t *) atomic)->value), 0); \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        return atomic->value; \
    }) \
} \
\
static cat_always_inline type_name_t cat_atomic_##name##_load_explicit(const cat_atomic_##name##_t *atomic, cat_atomic_memory_order_t order) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        switch (order) { \
            case CAT_ATOMIC_MEMORY_ORDER_RELAXED: \
                return __c11_atomic_load(&atomic->value, __ATOMIC_RELAXED); \
            case CAT_ATOMIC_MEMORY_ORDER_CONSUME: \
                return __c11_atomic_load(&atomic->value, __ATOMIC_CONSUME); \
            case CAT_ATOMIC_MEMORY_ORDER_ACQUIRE: \
                return __c11_atomic_load(&atomic->value, __ATOMIC_ACQUIRE); \
            case CAT_ATOMIC_MEMORY_ORDER_SEQ_CST: \
                return __c11_atomic_load(&atomic->value, __ATOMIC_SEQ_CST); \
            case CAT_ATOMIC_MEMORY_ORDER_RELEASE: \
            case CAT_ATOMIC_MEMORY_ORDER_ACQ_REL: \
                CAT_NEVER_HERE("Invalid memory order for load operation"); \
            default: \
                CAT_NEVER_HERE("Unknown memory order"); \
        } \
    }) \
    CAT_ATOMIC_GNUC_CASE({ \
        type_name_t ret; \
        switch (order) { \
            case CAT_ATOMIC_MEMORY_ORDER_RELAXED: \
                __atomic_load(&atomic->value, &ret, __ATOMIC_RELAXED); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_CONSUME: \
                __atomic_load(&atomic->value, &ret, __ATOMIC_CONSUME); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_ACQUIRE: \
                __atomic_load(&atomic->value, &ret, __ATOMIC_ACQUIRE); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_SEQ_CST: \
                __atomic_load(&atomic->value, &ret, __ATOMIC_SEQ_CST); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_RELEASE: \
            case CAT_ATOMIC_MEMORY_ORDER_ACQ_REL: \
                CAT_NEVER_HERE("Invalid memory order for load operation"); \
            default: \
                CAT_NEVER_HERE("Unknown memory order"); \
        } \
        return ret; \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        switch (order) { \
            case CAT_ATOMIC_MEMORY_ORDER_ACQUIRE: \
            case CAT_ATOMIC_MEMORY_ORDER_CONSUME: \
                return (type_name_t) _InterlockedLoad##interlocked_suffix##_acq(&(((cat_atomic_##name##_t *) atomic)->value)); \
            case CAT_ATOMIC_MEMORY_ORDER_RELAXED: \
                return (type_name_t) _InterlockedLoad##interlocked_suffix##_nf(&(((cat_atomic_##name##_t *) atomic)->value)); \
            case CAT_ATOMIC_MEMORY_ORDER_RELEASE: \
            case CAT_ATOMIC_MEMORY_ORDER_ACQ_REL: \
            case CAT_ATOMIC_MEMORY_ORDER_SEQ_CST: \
                return (type_name_t) _InterlockedLoad##interlocked_suffix(&(((cat_atomic_##name##_t *) atomic)->value)); \
            default: \
                CAT_NEVER_HERE("Unknown memory order"); \
        } \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        return __sync_fetch_and_or(&(((cat_atomic_##name##_t *) atomic)->value), 0); \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        return atomic->value; \
    }) \
} \
\
static cat_always_inline type_name_t cat_atomic_##name##_exchange(cat_atomic_##name##_t *atomic, type_name_t desired) \
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
        return (type_name_t) _InterlockedExchange##interlocked_suffix(&atomic->value, (interlocked_type_t) desired); \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        return __sync_val_compare_and_swap(&atomic->value, atomic->value, desired); \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        type_name_t ret = atomic->value; \
        atomic->value = desired; \
        return ret; \
    }) \
} \
\
static cat_always_inline type_name_t cat_atomic_##name##_exchange_explicit(cat_atomic_##name##_t *atomic, type_name_t desired, cat_atomic_memory_order_t order) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        switch (order) { \
            case CAT_ATOMIC_MEMORY_ORDER_RELAXED: \
                return __c11_atomic_exchange(&atomic->value, desired, __ATOMIC_RELAXED); \
            case CAT_ATOMIC_MEMORY_ORDER_ACQUIRE: \
                return __c11_atomic_exchange(&atomic->value, desired, __ATOMIC_ACQUIRE); \
            case CAT_ATOMIC_MEMORY_ORDER_RELEASE: \
                return __c11_atomic_exchange(&atomic->value, desired, __ATOMIC_RELEASE); \
            case CAT_ATOMIC_MEMORY_ORDER_ACQ_REL: \
                return __c11_atomic_exchange(&atomic->value, desired, __ATOMIC_ACQ_REL); \
            case CAT_ATOMIC_MEMORY_ORDER_SEQ_CST: \
                return __c11_atomic_exchange(&atomic->value, desired, __ATOMIC_SEQ_CST); \
            case CAT_ATOMIC_MEMORY_ORDER_CONSUME: \
                CAT_NEVER_HERE("Invalid memory order for exchange operation"); \
            default: \
                CAT_NEVER_HERE("Unknown memory order"); \
        } \
    }) \
    CAT_ATOMIC_GNUC_CASE({ \
        type_name_t ret; \
        switch (order) { \
            case CAT_ATOMIC_MEMORY_ORDER_RELAXED: \
                __atomic_exchange(&atomic->value, &desired, &ret, __ATOMIC_RELAXED); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_CONSUME: \
                __atomic_exchange(&atomic->value, &desired, &ret, __ATOMIC_CONSUME); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_ACQUIRE: \
                __atomic_exchange(&atomic->value, &desired, &ret, __ATOMIC_ACQUIRE); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_RELEASE: \
                __atomic_exchange(&atomic->value, &desired, &ret, __ATOMIC_RELEASE); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_ACQ_REL: \
                __atomic_exchange(&atomic->value, &desired, &ret, __ATOMIC_ACQ_REL); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_SEQ_CST: \
                __atomic_exchange(&atomic->value, &desired, &ret, __ATOMIC_SEQ_CST); \
                break; \
            default: \
                CAT_NEVER_HERE("Unknown memory order"); \
        } \
        return ret; \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        type_name_t ret; \
        switch (order) { \
            case CAT_ATOMIC_MEMORY_ORDER_ACQ_REL: \
            case CAT_ATOMIC_MEMORY_ORDER_SEQ_CST: \
                ret = (type_name_t) _InterlockedExchange##interlocked_suffix(&atomic->value, (interlocked_type_t) desired); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_ACQUIRE: \
            case CAT_ATOMIC_MEMORY_ORDER_CONSUME: \
                ret = (type_name_t) _InterlockedExchange##interlocked_suffix##_acq(&atomic->value, (interlocked_type_t) desired); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_RELEASE: \
                ret = (type_name_t) _InterlockedExchange##interlocked_suffix##_rel(&atomic->value, (interlocked_type_t) desired); \
                break; \
            case CAT_ATOMIC_MEMORY_ORDER_RELAXED: \
                ret = (type_name_t) _InterlockedExchange##interlocked_suffix##_nf(&atomic->value, (interlocked_type_t) desired); \
                break; \
            default: \
                CAT_NEVER_HERE("Unknown memory order"); \
        } \
        return ret; \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        return __sync_val_compare_and_swap(&atomic->value, atomic->value, desired); \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        type_name_t ret = atomic->value; \
        atomic->value = desired; \
        return ret; \
    }) \
} \
\
static cat_always_inline cat_bool_t cat_atomic_##name##_compare_exchange_strong(cat_atomic_##name##_t *atomic, type_name_t *expected, type_name_t desired) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_compare_exchange_strong(&atomic->value, expected, desired, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
    }) \
    CAT_ATOMIC_GNUC_CASE({ \
        return __atomic_compare_exchange_strong(&atomic->value, expected, desired); \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        type_name_t value = (type_name_t) _InterlockedCompareExchange##interlocked_suffix(&atomic->value, (interlocked_type_t) desired, (interlocked_type_t) (*expected)); \
        if (value == *expected) { \
            return cat_true; \
        } else { \
            *expected = value; \
            return cat_false; \
        } \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        type_name_t value = __sync_val_compare_and_swap(&atomic->value, *expected, desired); \
         if (value == *expected) { \
            return cat_true; \
        } else { \
            *expected = value; \
            return cat_false; \
        } \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        if (atomic->value == *expected) { \
            atomic->value = desired; \
            return cat_true; \
        } else { \
            *expected = atomic->value; \
            return cat_false; \
        } \
    }) \
} \
\
static cat_always_inline cat_bool_t cat_atomic_##name##_compare_exchange_weak(cat_atomic_##name##_t *atomic, type_name_t *expected, type_name_t desired) \
{ \
    CAT_ATOMIC_C11_CASE({ \
        return __c11_atomic_compare_exchange_weak(&atomic->value, expected, desired, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
    }) \
    CAT_ATOMIC_GNUC_CASE({ \
        return __atomic_compare_exchange_weak(&atomic->value, expected, desired); \
    }) \
    CAT_ATOMIC_INTERLOCK_CASE({ \
        return cat_atomic_##name##_compare_exchange_strong(atomic, expected, desired); \
    }) \
    CAT_ATOMIC_SYNC_CASE({ \
        return cat_atomic_##name##_compare_exchange_strong(atomic, expected, desired); \
    }) \
    CAT_ATOMIC_NO_CASE({ \
        return cat_atomic_##name##_compare_exchange_strong(atomic, expected, desired); \
    }) \
}

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
    CAT_ATOMIC_NO_CASE({ \
        type_name_t ret = atomic->value; \
        atomic->value += operand; \
        return ret; \
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
    CAT_ATOMIC_NO_CASE({ \
        type_name_t ret = atomic->value; \
        atomic->value -= operand; \
        return ret; \
    }) \
}

#define CAT_ATOMIC_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \
        CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \
        CAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_GEN(name, type_name_t, interlocked_suffix, interlocked_type_t) \

typedef enum cat_atomic_memory_order_e {
   CAT_ATOMIC_MEMORY_ORDER_SEQ_CST,
   CAT_ATOMIC_MEMORY_ORDER_ACQUIRE,
   CAT_ATOMIC_MEMORY_ORDER_RELEASE,
   CAT_ATOMIC_MEMORY_ORDER_RELAXED,
   CAT_ATOMIC_MEMORY_ORDER_ACQ_REL,
   CAT_ATOMIC_MEMORY_ORDER_CONSUME,
} cat_atomic_memory_order_t;

CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_MAP(CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_GEN)
CAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_MAP(CAT_ATOMIC_OPERATION_FUNCTIONS_GEN)

#ifdef __cplusplus
}
#endif
#endif /* CAT_ATOMIC_H */
