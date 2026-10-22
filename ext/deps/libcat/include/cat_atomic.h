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

/*
 * ThreadSanitizer does not support a thread fence, and its instrumentation tracks
 * the ordering of the accesses itself. A signal fence is what is left to do.
 */
#if defined(__SANITIZE_THREAD__)
# define CAT_ATOMIC_TSAN 1
#elif defined(__has_feature)
# if __has_feature(thread_sanitizer)
#  define CAT_ATOMIC_TSAN 1
# endif
#endif

/* TODO: Fixed implementation selection
 * or provided C API for external use. */
#if __has_feature(c_atomic) && defined(__clang__)
# define CAT_HAVE_C11_ATOMIC 1
#elif defined(__clang__)
/* clang without the C11 atomics takes the __atomic builtins, as GCC does */
# define CAT_HAVE_GNUC_ATOMIC 1
#elif CAT_GCC_VERSION >= 4007
# define CAT_HAVE_GNUC_ATOMIC 1
#elif defined(__GNUC__)
/*
 * The bare-address layer has no __sync implementation, so this path does not
 * compile. It stays for the case where a compiler below GCC 4.7 needs one.
 */
# error "The bare-address atomics need __atomic builtins, which this compiler has not"
# define CAT_HAVE_SYNC_ATOMIC 1
#elif defined(CAT_OS_WIN)
# define CAT_HAVE_INTERLOCK_ATOMIC 1
#else
# error "No atomics support detected, that's terrible!"
#endif

#ifdef CAT_HAVE_INTERLOCK_ATOMIC
# include <intrin.h>
#endif


#define CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_MAP(XX) \
        XX(bool, cat_bool_t, int8) \
        XX(ptr,  cat_ptr_t, uintptr) \
        XX(uintptr, uintptr_t, uintptr) \

#define CAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_MAP(XX) \
        XX(int8,   int8_t,   int8) \
        XX(uint8,  uint8_t,  int8) \
        XX(int16,  int16_t,  int16) \
        XX(uint16, uint16_t, int16) \
        XX(int32,  int32_t,  int32) \
        XX(uint32, uint32_t, int32) \
        XX(int64,  int64_t,  int64) \
        XX(uint64, uint64_t, int64) \

/*
 * The memory orders, which both layers below take as arguments.
 */

typedef enum cat_atomic_memory_order_e {
   CAT_ATOMIC_MEMORY_ORDER_SEQ_CST,
   CAT_ATOMIC_MEMORY_ORDER_ACQUIRE,
   CAT_ATOMIC_MEMORY_ORDER_RELEASE,
   CAT_ATOMIC_MEMORY_ORDER_RELAXED,
   CAT_ATOMIC_MEMORY_ORDER_ACQ_REL,
   CAT_ATOMIC_MEMORY_ORDER_CONSUME,
} cat_atomic_memory_order_t;

/*
 * Bare-address atomics.
 *
 * The caller supplies the address, so a value the caller owns needs no library
 * type around it. A waiter can then be indexed by an address, which the
 * generated layer above cannot do.
 *
 * Two points differ from C11 <stdatomic.h>. The address is void *, so the
 * compiler does not check its type. A compare-exchange defaults to seq_cst, and
 * the _explicit form takes the success and failure orders.
 *
 * Alignment is the caller's duty: align the address to the size of the access.
 * The library does not check it.
 *
 * The typed layer below forwards every operation here, so both layers agree on
 * orders and on backend behavior.
 */

#define CAT_ATOMIC_LOAD_ORDERS_VALID(order) \
    ((order) == CAT_ATOMIC_MEMORY_ORDER_RELAXED || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_CONSUME || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_ACQUIRE || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_SEQ_CST)

#define CAT_ATOMIC_STORE_ORDERS_VALID(order) \
    ((order) == CAT_ATOMIC_MEMORY_ORDER_RELAXED || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_RELEASE || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_SEQ_CST)

#define CAT_ATOMIC_RMW_ORDERS_VALID(order) \
    ((order) == CAT_ATOMIC_MEMORY_ORDER_RELAXED || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_ACQUIRE || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_RELEASE || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_ACQ_REL || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_SEQ_CST)

#define CAT_ATOMIC_FENCE_ORDERS_VALID(order) \
    ((order) == CAT_ATOMIC_MEMORY_ORDER_ACQUIRE || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_RELEASE || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_ACQ_REL || \
     (order) == CAT_ATOMIC_MEMORY_ORDER_SEQ_CST)

/* the relative strength of a memory order, used to compare two of them */
#define CAT_ATOMIC_ORDER_STRENGTH(order) \
    ((order) == CAT_ATOMIC_MEMORY_ORDER_RELAXED ? 1 : \
     (order) == CAT_ATOMIC_MEMORY_ORDER_CONSUME ? 2 : \
     (order) == CAT_ATOMIC_MEMORY_ORDER_ACQUIRE ? 3 : \
     (order) == CAT_ATOMIC_MEMORY_ORDER_RELEASE ? 4 : \
     (order) == CAT_ATOMIC_MEMORY_ORDER_ACQ_REL ? 5 : \
     (order) == CAT_ATOMIC_MEMORY_ORDER_SEQ_CST ? 6 : 0)

/*
 * C11 constrains the failure order three ways: it can not be release or acq_rel,
 * it can not be consume when the success order is consume, and it can not be
 * stronger than the success order. Call this only after the success order passed
 * the read-modify-write check.
 */
#define CAT_ATOMIC_FAILURE_ORDERS_VALID(success, failure) \
    ((failure) != CAT_ATOMIC_MEMORY_ORDER_RELEASE && \
     (failure) != CAT_ATOMIC_MEMORY_ORDER_ACQ_REL && \
     !((failure) == CAT_ATOMIC_MEMORY_ORDER_CONSUME && (success) == CAT_ATOMIC_MEMORY_ORDER_CONSUME) && \
     CAT_ATOMIC_ORDER_STRENGTH(failure) <= CAT_ATOMIC_ORDER_STRENGTH(success))

#define CAT_ATOMIC_LOAD_ORDERS_TO_NATIVE(order) \
    ((order) == CAT_ATOMIC_MEMORY_ORDER_RELAXED ? __ATOMIC_RELAXED : \
     (order) == CAT_ATOMIC_MEMORY_ORDER_CONSUME ? __ATOMIC_CONSUME : \
     (order) == CAT_ATOMIC_MEMORY_ORDER_ACQUIRE ? __ATOMIC_ACQUIRE : \
     __ATOMIC_SEQ_CST)

#define CAT_ATOMIC_STORE_ORDERS_TO_NATIVE(order) \
    ((order) == CAT_ATOMIC_MEMORY_ORDER_RELAXED ? __ATOMIC_RELAXED : \
     (order) == CAT_ATOMIC_MEMORY_ORDER_RELEASE ? __ATOMIC_RELEASE : \
     __ATOMIC_SEQ_CST)

#define CAT_ATOMIC_RMW_ORDERS_TO_NATIVE(order) \
    ((order) == CAT_ATOMIC_MEMORY_ORDER_RELAXED ? __ATOMIC_RELAXED : \
     (order) == CAT_ATOMIC_MEMORY_ORDER_ACQUIRE ? __ATOMIC_ACQUIRE : \
     (order) == CAT_ATOMIC_MEMORY_ORDER_RELEASE ? __ATOMIC_RELEASE : \
     (order) == CAT_ATOMIC_MEMORY_ORDER_ACQ_REL ? __ATOMIC_ACQ_REL : \
     __ATOMIC_SEQ_CST)

/* a full barrier is the strongest fence order, so it maps to itself */
#define CAT_ATOMIC_FENCE_ORDERS_TO_NATIVE(order) CAT_ATOMIC_RMW_ORDERS_TO_NATIVE(order)

/*
 * The plain-memory operations. The two compiler families need different
 * spellings, and the pointee type travels with the call.
 */

#ifdef CAT_HAVE_C11_ATOMIC
/*
 * The C11 builtins of clang take a pointer to an _Atomic type and reject a
 * pointer to plain memory, so the call declares the pointee type with a cast.
 * GCC has no such requirement and its cast is the identity. Neither form puts
 * an _Atomic member into any structure.
 */
# define CAT_ATOMIC_ATOMIC_CAST(type_name_t, address) ((_Atomic(type_name_t) *) (address))
# define CAT_ATOMIC_PLAIN_LOAD(address, type_name_t) \
        __c11_atomic_load(address, __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_LOAD_ORDERED(address, type_name_t, order) \
        __c11_atomic_load(address, CAT_ATOMIC_LOAD_ORDERS_TO_NATIVE(order))
# define CAT_ATOMIC_PLAIN_STORE(address, desired, type_name_t) \
        __c11_atomic_store(address, (desired), __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_STORE_ORDERED(address, desired, type_name_t, order) \
        __c11_atomic_store(address, (desired), CAT_ATOMIC_STORE_ORDERS_TO_NATIVE(order))
# define CAT_ATOMIC_PLAIN_EXCHANGE(address, desired, type_name_t) \
        __c11_atomic_exchange(address, (desired), __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_EXCHANGE_ORDERED(address, desired, type_name_t, order) \
        __c11_atomic_exchange(address, (desired), CAT_ATOMIC_RMW_ORDERS_TO_NATIVE(order))
# define CAT_ATOMIC_PLAIN_FETCH_ADD(address, operand, type_name_t) \
        __c11_atomic_fetch_add(address, (operand), __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_FETCH_ADD_ORDERED(address, operand, type_name_t, order) \
        __c11_atomic_fetch_add(address, (operand), CAT_ATOMIC_RMW_ORDERS_TO_NATIVE(order))
# define CAT_ATOMIC_PLAIN_FETCH_SUB(address, operand, type_name_t) \
        __c11_atomic_fetch_sub(address, (operand), __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_FETCH_SUB_ORDERED(address, operand, type_name_t, order) \
        __c11_atomic_fetch_sub(address, (operand), CAT_ATOMIC_RMW_ORDERS_TO_NATIVE(order))
# define CAT_ATOMIC_PLAIN_COMPARE_EXCHANGE(address, expected, desired, type_name_t) \
        (cat_bool_t) __c11_atomic_compare_exchange_strong(address, (expected), (desired), __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_COMPARE_EXCHANGE_ORDERED(address, expected, desired, type_name_t, success, failure) \
        (cat_bool_t) __c11_atomic_compare_exchange_strong(address, (expected), (desired), \
            CAT_ATOMIC_RMW_ORDERS_TO_NATIVE(success), CAT_ATOMIC_LOAD_ORDERS_TO_NATIVE(failure))
# define CAT_ATOMIC_PLAIN_FENCE() \
        __c11_atomic_thread_fence(__ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_FENCE_ORDERED(order) \
        __c11_atomic_thread_fence(CAT_ATOMIC_FENCE_ORDERS_TO_NATIVE(order))
#else
# define CAT_ATOMIC_ATOMIC_CAST(type_name_t, address) (address)
# define CAT_ATOMIC_PLAIN_LOAD(address, type_name_t) \
        __atomic_load_n(address, __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_LOAD_ORDERED(address, type_name_t, order) \
        __atomic_load_n(address, CAT_ATOMIC_LOAD_ORDERS_TO_NATIVE(order))
# define CAT_ATOMIC_PLAIN_STORE(address, desired, type_name_t) \
        __atomic_store_n(address, (desired), __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_STORE_ORDERED(address, desired, type_name_t, order) \
        __atomic_store_n(address, (desired), CAT_ATOMIC_STORE_ORDERS_TO_NATIVE(order))
# define CAT_ATOMIC_PLAIN_EXCHANGE(address, desired, type_name_t) \
        __atomic_exchange_n(address, (desired), __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_EXCHANGE_ORDERED(address, desired, type_name_t, order) \
        __atomic_exchange_n(address, (desired), CAT_ATOMIC_RMW_ORDERS_TO_NATIVE(order))
# define CAT_ATOMIC_PLAIN_FETCH_ADD(address, operand, type_name_t) \
        __atomic_fetch_add(address, (operand), __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_FETCH_ADD_ORDERED(address, operand, type_name_t, order) \
        __atomic_fetch_add(address, (operand), CAT_ATOMIC_RMW_ORDERS_TO_NATIVE(order))
# define CAT_ATOMIC_PLAIN_FETCH_SUB(address, operand, type_name_t) \
        __atomic_fetch_sub(address, (operand), __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_FETCH_SUB_ORDERED(address, operand, type_name_t, order) \
        __atomic_fetch_sub(address, (operand), CAT_ATOMIC_RMW_ORDERS_TO_NATIVE(order))
# define CAT_ATOMIC_PLAIN_COMPARE_EXCHANGE(address, expected, desired, type_name_t) \
        (cat_bool_t) __atomic_compare_exchange_n(address, (expected), (desired), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
# define CAT_ATOMIC_PLAIN_COMPARE_EXCHANGE_ORDERED(address, expected, desired, type_name_t, success, failure) \
        (cat_bool_t) __atomic_compare_exchange_n(address, (expected), (desired), 0, \
            CAT_ATOMIC_RMW_ORDERS_TO_NATIVE(success), CAT_ATOMIC_LOAD_ORDERS_TO_NATIVE(failure))
# if defined(CAT_ATOMIC_TSAN)
#  define CAT_ATOMIC_PLAIN_FENCE() \
        __atomic_signal_fence(__ATOMIC_SEQ_CST)
#  define CAT_ATOMIC_PLAIN_FENCE_ORDERED(order) \
        __atomic_signal_fence(CAT_ATOMIC_FENCE_ORDERS_TO_NATIVE(order))
# else
#  define CAT_ATOMIC_PLAIN_FENCE() \
        __atomic_thread_fence(__ATOMIC_SEQ_CST)
#  define CAT_ATOMIC_PLAIN_FENCE_ORDERED(order) \
        __atomic_thread_fence(CAT_ATOMIC_FENCE_ORDERS_TO_NATIVE(order))
# endif
#endif


/*
 * Widths the bare-address layer covers. They are the same on every platform,
 * so this list is not the numeric map above.
 */
#define CAT_ATOMIC_BARE_OPERATIONS_MAP(XX) \
        XX(int8, int8_t, 8) \
        XX(int16, int16_t, 16) \
        XX(int32, int32_t, 32) \
        XX(int64, int64_t, 64) \
        XX(intptr, intptr_t, POINTER) \
        XX(uintptr, uintptr_t, POINTER) \

/* The plain-memory path does not need the interlocked marker, so it ignores it. */
#define CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address) CAT_ATOMIC_ATOMIC_CAST(type_name_t, address)
#define CAT_ATOMIC_BARE_LOAD(address, type_name_t, interlocked_suffix) \
        CAT_ATOMIC_PLAIN_LOAD(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), type_name_t)
#define CAT_ATOMIC_BARE_LOAD_ORDERED(address, type_name_t, interlocked_suffix, order) \
        CAT_ATOMIC_PLAIN_LOAD_ORDERED(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), type_name_t, order)
#define CAT_ATOMIC_BARE_STORE(address, desired, type_name_t, interlocked_suffix) \
        CAT_ATOMIC_PLAIN_STORE(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), desired, type_name_t)
#define CAT_ATOMIC_BARE_STORE_ORDERED(address, desired, type_name_t, interlocked_suffix, order) \
        CAT_ATOMIC_PLAIN_STORE_ORDERED(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), desired, type_name_t, order)
#define CAT_ATOMIC_BARE_EXCHANGE(address, desired, type_name_t, interlocked_suffix) \
        CAT_ATOMIC_PLAIN_EXCHANGE(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), desired, type_name_t)
#define CAT_ATOMIC_BARE_EXCHANGE_ORDERED(address, desired, type_name_t, interlocked_suffix, order) \
        CAT_ATOMIC_PLAIN_EXCHANGE_ORDERED(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), desired, type_name_t, order)
#define CAT_ATOMIC_BARE_FETCH_ADD(address, operand, type_name_t, interlocked_suffix) \
        CAT_ATOMIC_PLAIN_FETCH_ADD(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), operand, type_name_t)
#define CAT_ATOMIC_BARE_FETCH_ADD_ORDERED(address, operand, type_name_t, interlocked_suffix, order) \
        CAT_ATOMIC_PLAIN_FETCH_ADD_ORDERED(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), operand, type_name_t, order)
#define CAT_ATOMIC_BARE_FETCH_SUB(address, operand, type_name_t, interlocked_suffix) \
        CAT_ATOMIC_PLAIN_FETCH_SUB(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), operand, type_name_t)
#define CAT_ATOMIC_BARE_FETCH_SUB_ORDERED(address, operand, type_name_t, interlocked_suffix, order) \
        CAT_ATOMIC_PLAIN_FETCH_SUB_ORDERED(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), operand, type_name_t, order)
#define CAT_ATOMIC_BARE_COMPARE_EXCHANGE(address, expected, desired, type_name_t, interlocked_suffix) \
        CAT_ATOMIC_PLAIN_COMPARE_EXCHANGE(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), expected, desired, type_name_t)
#define CAT_ATOMIC_BARE_COMPARE_EXCHANGE_ORDERED(address, expected, desired, type_name_t, interlocked_suffix, success, failure) \
        CAT_ATOMIC_PLAIN_COMPARE_EXCHANGE_ORDERED(CAT_ATOMIC_BARE_CAST(type_name_t, interlocked_suffix, address), expected, desired, type_name_t, success, failure)
#define CAT_ATOMIC_BARE_FENCE() CAT_ATOMIC_PLAIN_FENCE()
#define CAT_ATOMIC_BARE_FENCE_ORDERED(order) CAT_ATOMIC_PLAIN_FENCE_ORDERED(order)
#if defined(CAT_HAVE_INTERLOCK_ATOMIC)
/*
 * Three things break on 32-bit Windows. The 64-bit columns need Or64, Exchange64
 * and ExchangeAdd64, which x86 does not have, and compare-exchange needs a loop
 * around the 64-bit form. The pointer column would also write 8 bytes into a
 * 4-byte object, which corrupts memory. A 32-bit path needs all three handled,
 * so a 32-bit build stops here until someone writes it.
 */
# if !defined(_WIN64)
#  error "The bare-address atomics need a 64-bit target on MSVC"
# endif
# undef CAT_ATOMIC_BARE_CAST
# undef CAT_ATOMIC_BARE_LOAD
# undef CAT_ATOMIC_BARE_LOAD_ORDERED
# undef CAT_ATOMIC_BARE_STORE
# undef CAT_ATOMIC_BARE_STORE_ORDERED
# undef CAT_ATOMIC_BARE_EXCHANGE
# undef CAT_ATOMIC_BARE_EXCHANGE_ORDERED
# undef CAT_ATOMIC_BARE_FETCH_ADD
# undef CAT_ATOMIC_BARE_FETCH_ADD_ORDERED
# undef CAT_ATOMIC_BARE_FETCH_SUB
# undef CAT_ATOMIC_BARE_FETCH_SUB_ORDERED
# undef CAT_ATOMIC_BARE_COMPARE_EXCHANGE
# undef CAT_ATOMIC_BARE_COMPARE_EXCHANGE_ORDERED
# undef CAT_ATOMIC_BARE_FENCE
# undef CAT_ATOMIC_BARE_FENCE_ORDERED
/*
 * One macro per width instead of a type token pasted into the intrinsic name.
 * MSVC does not expand a macro argument that is an operand of ##, so a pasted
 * type name would arrive at the intrinsic unexpanded. The marker selects the
 * width here, and every operation pattern is written once in the two generic
 * macros below.
 *
 * The intrinsics also take exact pointer types (long *, __int64 * or void **),
 * so the address is cast to the interlocked width before it reaches them.
 */
/*
 * The intrinsic names below are checked against the MSVC lists for ARM64, x64
 * and x86. Two names do not exist anywhere: there is no _InterlockedLoad, and a
 * zero-valued Or is the load form, and there is no _InterlockedExchangeSubtract,
 * and a negated ExchangeAdd is the subtract form. The list marks a base name as
 * a full barrier, so a relaxed order still costs a barrier here.
 */
#define CAT_ATOMIC_INTERLOCK_LOAD_8(address, il_t) \
        ((il_t) _InterlockedOr8((volatile char *) (address), 0))
#define CAT_ATOMIC_INTERLOCK_LOAD_16(address, il_t) \
        ((il_t) _InterlockedOr16((volatile short *) (address), 0))
#define CAT_ATOMIC_INTERLOCK_STORE_8(address, desired, il_t) \
        ((void) _InterlockedExchange8((volatile char *) (address), (char) (desired)))
#define CAT_ATOMIC_INTERLOCK_STORE_16(address, desired, il_t) \
        ((void) _InterlockedExchange16((volatile short *) (address), (short) (desired)))
#define CAT_ATOMIC_INTERLOCK_EXCHANGE_8(address, desired, il_t) \
        ((il_t) _InterlockedExchange8((volatile char *) (address), (char) (desired)))
#define CAT_ATOMIC_INTERLOCK_EXCHANGE_16(address, desired, il_t) \
        ((il_t) _InterlockedExchange16((volatile short *) (address), (short) (desired)))
#define CAT_ATOMIC_INTERLOCK_FETCH_ADD_8(address, operand, il_t) \
        ((il_t) _InterlockedExchangeAdd8((volatile char *) (address), (char) (operand)))
#define CAT_ATOMIC_INTERLOCK_FETCH_ADD_16(address, operand, il_t) \
        ((il_t) _InterlockedExchangeAdd16((volatile short *) (address), (short) (operand)))
#define CAT_ATOMIC_INTERLOCK_FETCH_SUB_8(address, operand, il_t) \
        ((il_t) _InterlockedExchangeAdd8((volatile char *) (address), -(char) (operand)))
#define CAT_ATOMIC_INTERLOCK_FETCH_SUB_16(address, operand, il_t) \
        ((il_t) _InterlockedExchangeAdd16((volatile short *) (address), -(short) (operand)))
#define CAT_ATOMIC_INTERLOCK_COMPARE_EXCHANGE_8(address, expected, desired, il_t) \
        cat_atomic_interlock_cas_char((volatile char *) (address), (char *) (expected), (char) (desired))
#define CAT_ATOMIC_INTERLOCK_COMPARE_EXCHANGE_16(address, expected, desired, il_t) \
        cat_atomic_interlock_cas_short((volatile short *) (address), (short *) (expected), (short) (desired))
#define CAT_ATOMIC_INTERLOCK_LOAD_32(address, il_t) \
        ((il_t) _InterlockedOr((volatile long *) (address), 0))
#define CAT_ATOMIC_INTERLOCK_LOAD_64(address, il_t) \
        ((il_t) _InterlockedOr64((volatile __int64 *) (address), 0))
#define CAT_ATOMIC_INTERLOCK_LOAD_POINTER(address, il_t) \
        ((il_t) _InterlockedCompareExchangePointer((void *volatile *) (address), NULL, NULL))
#define CAT_ATOMIC_INTERLOCK_STORE_32(address, desired, il_t) \
        ((void) _InterlockedExchange((volatile long *) (address), (long) (desired)))
#define CAT_ATOMIC_INTERLOCK_STORE_64(address, desired, il_t) \
        ((void) _InterlockedExchange64((volatile __int64 *) (address), (__int64) (desired)))
#define CAT_ATOMIC_INTERLOCK_STORE_POINTER(address, desired, il_t) \
        ((void) _InterlockedExchangePointer((void *volatile *) (address), (void *) (desired)))
#define CAT_ATOMIC_INTERLOCK_EXCHANGE_32(address, desired, il_t) \
        ((il_t) _InterlockedExchange((volatile long *) (address), (long) (desired)))
#define CAT_ATOMIC_INTERLOCK_EXCHANGE_64(address, desired, il_t) \
        ((il_t) _InterlockedExchange64((volatile __int64 *) (address), (__int64) (desired)))
#define CAT_ATOMIC_INTERLOCK_EXCHANGE_POINTER(address, desired, il_t) \
        ((il_t) _InterlockedExchangePointer((void *volatile *) (address), (void *) (desired)))
#define CAT_ATOMIC_INTERLOCK_FETCH_ADD_32(address, operand, il_t) \
        ((il_t) _InterlockedExchangeAdd((volatile long *) (address), (long) (operand)))
#define CAT_ATOMIC_INTERLOCK_FETCH_ADD_64(address, operand, il_t) \
        ((il_t) _InterlockedExchangeAdd64((volatile __int64 *) (address), (__int64) (operand)))
#define CAT_ATOMIC_INTERLOCK_FETCH_SUB_32(address, operand, il_t) \
        ((il_t) _InterlockedExchangeAdd((volatile long *) (address), -(long) (operand)))
#define CAT_ATOMIC_INTERLOCK_FETCH_SUB_64(address, operand, il_t) \
        ((il_t) _InterlockedExchangeAdd64((volatile __int64 *) (address), -(__int64) (operand)))
#define CAT_ATOMIC_INTERLOCK_COMPARE_EXCHANGE_32(address, expected, desired, il_t) \
        cat_atomic_interlock_cas_long((volatile long *) (address), (long *) (expected), (long) (desired))
#define CAT_ATOMIC_INTERLOCK_COMPARE_EXCHANGE_64(address, expected, desired, il_t) \
        cat_atomic_interlock_cas___int64((volatile __int64 *) (address), (__int64 *) (expected), (__int64) (desired))
#define CAT_ATOMIC_INTERLOCK_COMPARE_EXCHANGE_POINTER(address, expected, desired, il_t) \
        cat_atomic_interlock_cas_pointer((void *volatile *) (address), (void **) (expected), (void *) (desired))
/* fetch_sub has no pointer form: void * has no arithmetic */
#define CAT_ATOMIC_INTERLOCK_FETCH_ADD_POINTER(address, operand, il_t) \
        ((il_t) _InterlockedExchangeAdd64((volatile __int64 *) (address), (__int64) (operand)))
#define CAT_ATOMIC_INTERLOCK_FETCH_SUB_POINTER(address, operand, il_t) \
        ((il_t) _InterlockedExchangeAdd64((volatile __int64 *) (address), -(__int64) (operand)))
#define CAT_ATOMIC_IL_TYPE_8         char
#define CAT_ATOMIC_IL_TYPE_16        short
#define CAT_ATOMIC_IL_TYPE_32        long
#define CAT_ATOMIC_IL_TYPE_64        __int64
#define CAT_ATOMIC_IL_TYPE_POINTER   void *
/*
 * The marker selects one macro above, so each operation pattern is written once
 * per width instead of once per width and operation.
 */
#define CAT_ATOMIC_BARE_LOAD(address, type_name_t, suffix) \
        CAT_ATOMIC_INTERLOCK_LOAD_ ## suffix(address, CAT_ATOMIC_IL_TYPE_ ## suffix)
#define CAT_ATOMIC_BARE_STORE(address, desired, type_name_t, suffix) \
        CAT_ATOMIC_INTERLOCK_STORE_ ## suffix(address, desired, CAT_ATOMIC_IL_TYPE_ ## suffix)
#define CAT_ATOMIC_BARE_EXCHANGE(address, desired, type_name_t, suffix) \
        CAT_ATOMIC_INTERLOCK_EXCHANGE_ ## suffix(address, desired, CAT_ATOMIC_IL_TYPE_ ## suffix)
#define CAT_ATOMIC_BARE_FETCH_ADD(address, operand, type_name_t, suffix) \
        CAT_ATOMIC_INTERLOCK_FETCH_ADD_ ## suffix(address, operand, CAT_ATOMIC_IL_TYPE_ ## suffix)
#define CAT_ATOMIC_BARE_FETCH_SUB(address, operand, type_name_t, suffix) \
        CAT_ATOMIC_INTERLOCK_FETCH_SUB_ ## suffix(address, operand, CAT_ATOMIC_IL_TYPE_ ## suffix)
#define CAT_ATOMIC_BARE_COMPARE_EXCHANGE(address, expected, desired, type_name_t, suffix) \
        CAT_ATOMIC_INTERLOCK_COMPARE_EXCHANGE_ ## suffix(address, expected, desired, CAT_ATOMIC_IL_TYPE_ ## suffix)
#define CAT_ATOMIC_BARE_LOAD_ORDERED(address, type_name_t, suffix, order) \
        CAT_ATOMIC_BARE_LOAD(address, type_name_t, suffix)
#define CAT_ATOMIC_BARE_STORE_ORDERED(address, desired, type_name_t, suffix, order) \
        CAT_ATOMIC_BARE_STORE(address, desired, type_name_t, suffix)
#define CAT_ATOMIC_BARE_EXCHANGE_ORDERED(address, desired, type_name_t, suffix, order) \
        CAT_ATOMIC_BARE_EXCHANGE(address, desired, type_name_t, suffix)
#define CAT_ATOMIC_BARE_FETCH_ADD_ORDERED(address, operand, type_name_t, suffix, order) \
        CAT_ATOMIC_BARE_FETCH_ADD(address, operand, type_name_t, suffix)
#define CAT_ATOMIC_BARE_FETCH_SUB_ORDERED(address, operand, type_name_t, suffix, order) \
        CAT_ATOMIC_BARE_FETCH_SUB(address, operand, type_name_t, suffix)
#define CAT_ATOMIC_BARE_COMPARE_EXCHANGE_ORDERED(address, expected, desired, type_name_t, suffix, success, failure) \
        CAT_ATOMIC_BARE_COMPARE_EXCHANGE(address, expected, desired, type_name_t, suffix)
#if defined(_M_ARM64) || defined(_M_ARM64EC)
/* __dmb and _ARM64_BARRIER_SY come from winnt.h */
# define CAT_ATOMIC_BARE_FENCE() __dmb(_ARM64_BARRIER_SY)
#else
/* _mm_mfence comes from intrin.h and is a full barrier on x86 and x64 */
# define CAT_ATOMIC_BARE_FENCE() _mm_mfence()
#endif
#define CAT_ATOMIC_BARE_FENCE_ORDERED(order) CAT_ATOMIC_BARE_FENCE()
static cat_always_inline cat_bool_t cat_atomic_interlock_cas_char(
    volatile char *address, char *expected, char desired)
{
    const char previous = (char) _InterlockedCompareExchange8(address, desired, *expected);
    if (previous == *expected) {
        return cat_true;
    }
    *expected = previous;
    return cat_false;
}

static cat_always_inline cat_bool_t cat_atomic_interlock_cas_short(
    volatile short *address, short *expected, short desired)
{
    const short previous = (short) _InterlockedCompareExchange16(address, desired, *expected);
    if (previous == *expected) {
        return cat_true;
    }
    *expected = previous;
    return cat_false;
}

static cat_always_inline cat_bool_t cat_atomic_interlock_cas_long(
    volatile long *address, long *expected, long desired)
{
    const long previous = _InterlockedCompareExchange(address, desired, *expected);
    if (previous == *expected) {
        return cat_true;
    }
    *expected = previous;
    return cat_false;
}
static cat_always_inline cat_bool_t cat_atomic_interlock_cas___int64(
    volatile __int64 *address, __int64 *expected, __int64 desired)
{
    const __int64 previous = _InterlockedCompareExchange64(address, desired, *expected);
    if (previous == *expected) {
        return cat_true;
    }
    *expected = previous;
    return cat_false;
}
static cat_always_inline cat_bool_t cat_atomic_interlock_cas_pointer(
    void *volatile *address, void **expected, void *desired)
{
    void *previous = _InterlockedCompareExchangePointer(address, desired, *expected);
    if (previous == *expected) {
        return cat_true;
    }
    *expected = previous;
    return cat_false;
}
#endif /* CAT_HAVE_INTERLOCK_ATOMIC overrides */

#define CAT_ATOMIC_BARE_OPERATIONS_GEN(name, type_name_t, interlocked_suffix) \
static cat_always_inline type_name_t cat_atomic_##name##_load_at(const void *address) \
{ \
    return (type_name_t) CAT_ATOMIC_BARE_LOAD((const type_name_t *) address, type_name_t, interlocked_suffix); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_load_at_explicit( \
    const void *address, cat_atomic_memory_order_t order) \
{ \
    if (!CAT_ATOMIC_LOAD_ORDERS_VALID(order)) { \
        CAT_NEVER_HERE("Invalid memory order for load operation"); \
    } \
    return (type_name_t) CAT_ATOMIC_BARE_LOAD_ORDERED((const type_name_t *) address, type_name_t, interlocked_suffix, order); \
} \
static cat_always_inline void cat_atomic_##name##_store_at(void *address, type_name_t desired) \
{ \
    CAT_ATOMIC_BARE_STORE((type_name_t *) address, desired, type_name_t, interlocked_suffix); \
} \
static cat_always_inline void cat_atomic_##name##_store_at_explicit( \
    void *address, type_name_t desired, cat_atomic_memory_order_t order) \
{ \
    if (!CAT_ATOMIC_STORE_ORDERS_VALID(order)) { \
        CAT_NEVER_HERE("Invalid memory order for store operation"); \
    } \
    CAT_ATOMIC_BARE_STORE_ORDERED((type_name_t *) address, desired, type_name_t, interlocked_suffix, order); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_exchange_at(void *address, type_name_t desired) \
{ \
    return (type_name_t) CAT_ATOMIC_BARE_EXCHANGE((type_name_t *) address, desired, type_name_t, interlocked_suffix); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_exchange_at_explicit( \
    void *address, type_name_t desired, cat_atomic_memory_order_t order) \
{ \
    if (!CAT_ATOMIC_RMW_ORDERS_VALID(order)) { \
        CAT_NEVER_HERE("Invalid memory order for exchange operation"); \
    } \
    return (type_name_t) CAT_ATOMIC_BARE_EXCHANGE_ORDERED((type_name_t *) address, desired, type_name_t, interlocked_suffix, order); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_fetch_add_at(void *address, type_name_t operand) \
{ \
    return (type_name_t) CAT_ATOMIC_BARE_FETCH_ADD((type_name_t *) address, operand, type_name_t, interlocked_suffix); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_fetch_add_at_explicit( \
    void *address, type_name_t operand, cat_atomic_memory_order_t order) \
{ \
    if (!CAT_ATOMIC_RMW_ORDERS_VALID(order)) { \
        CAT_NEVER_HERE("Invalid memory order for fetch_add operation"); \
    } \
    return (type_name_t) CAT_ATOMIC_BARE_FETCH_ADD_ORDERED((type_name_t *) address, operand, type_name_t, interlocked_suffix, order); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_fetch_sub_at(void *address, type_name_t operand) \
{ \
    return (type_name_t) CAT_ATOMIC_BARE_FETCH_SUB((type_name_t *) address, operand, type_name_t, interlocked_suffix); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_fetch_sub_at_explicit( \
    void *address, type_name_t operand, cat_atomic_memory_order_t order) \
{ \
    if (!CAT_ATOMIC_RMW_ORDERS_VALID(order)) { \
        CAT_NEVER_HERE("Invalid memory order for fetch_sub operation"); \
    } \
    return (type_name_t) CAT_ATOMIC_BARE_FETCH_SUB_ORDERED((type_name_t *) address, operand, type_name_t, interlocked_suffix, order); \
} \
static cat_always_inline cat_bool_t cat_atomic_##name##_compare_exchange_strong_at( \
    void *address, type_name_t *expected, type_name_t desired) \
{ \
    return CAT_ATOMIC_BARE_COMPARE_EXCHANGE((type_name_t *) address, expected, desired, type_name_t, interlocked_suffix); \
} \
static cat_always_inline cat_bool_t cat_atomic_##name##_compare_exchange_strong_at_explicit( \
    void *address, type_name_t *expected, type_name_t desired, \
    cat_atomic_memory_order_t success_order, cat_atomic_memory_order_t failure_order) \
{ \
    if (!CAT_ATOMIC_RMW_ORDERS_VALID(success_order)) { \
        CAT_NEVER_HERE("Invalid memory order for compare_exchange operation"); \
    } \
    if (!CAT_ATOMIC_FAILURE_ORDERS_VALID(success_order, failure_order)) { \
        CAT_NEVER_HERE("Invalid failure memory order for compare_exchange operation"); \
    } \
    return CAT_ATOMIC_BARE_COMPARE_EXCHANGE_ORDERED( \
        (type_name_t *) address, expected, desired, type_name_t, interlocked_suffix, success_order, failure_order); \
} \
static cat_always_inline cat_bool_t cat_atomic_##name##_compare_exchange_weak_at( \
    void *address, type_name_t *expected, type_name_t desired) \
{ \
    return CAT_ATOMIC_BARE_COMPARE_EXCHANGE((type_name_t *) address, expected, desired, type_name_t, interlocked_suffix); \
} \
static cat_always_inline cat_bool_t cat_atomic_##name##_compare_exchange_weak_at_explicit( \
    void *address, type_name_t *expected, type_name_t desired, \
    cat_atomic_memory_order_t success_order, cat_atomic_memory_order_t failure_order) \
{ \
    if (!CAT_ATOMIC_RMW_ORDERS_VALID(success_order)) { \
        CAT_NEVER_HERE("Invalid memory order for compare_exchange operation"); \
    } \
    if (!CAT_ATOMIC_FAILURE_ORDERS_VALID(success_order, failure_order)) { \
        CAT_NEVER_HERE("Invalid failure memory order for compare_exchange operation"); \
    } \
    return CAT_ATOMIC_BARE_COMPARE_EXCHANGE_ORDERED( \
        (type_name_t *) address, expected, desired, type_name_t, interlocked_suffix, success_order, failure_order); \
}

CAT_ATOMIC_BARE_OPERATIONS_MAP(CAT_ATOMIC_BARE_OPERATIONS_GEN)

static cat_always_inline void cat_atomic_fence(void)
{
    CAT_ATOMIC_BARE_FENCE();
}

static cat_always_inline void cat_atomic_fence_explicit(cat_atomic_memory_order_t order)
{
    if (!CAT_ATOMIC_FENCE_ORDERS_VALID(order)) {
        CAT_NEVER_HERE("Invalid memory order for fence operation");
    }
    CAT_ATOMIC_BARE_FENCE_ORDERED(order);
}


/*
 * The structure keeps the backend-specific member, because the C11 path and the
 * interlocked path need different payloads. Every function below forwards to the
 * bare-address layer, so the two layers agree on orders and on backend behavior.
 */
#define CAT_ATOMIC_BARE_PAYLOAD_int8      int8_t
#define CAT_ATOMIC_BARE_PAYLOAD_int16     int16_t
#define CAT_ATOMIC_BARE_PAYLOAD_int32     int32_t
#define CAT_ATOMIC_BARE_PAYLOAD_int64     int64_t
#define CAT_ATOMIC_BARE_PAYLOAD_uintptr   uintptr_t

#define CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_GEN(name, type_name_t, bare_t) \
typedef struct cat_atomic_##name##_s { \
    type_name_t value; \
} cat_atomic_##name##_t; \
\
static cat_always_inline void cat_atomic_##name##_init(cat_atomic_##name##_t *atomic, type_name_t desired) \
{ \
    atomic->value = (type_name_t) desired; \
} \
static cat_always_inline type_name_t cat_atomic_##name##_load(const cat_atomic_##name##_t *atomic) \
{ \
    return (type_name_t) cat_atomic_##bare_t##_load_at((const void *) &atomic->value); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_load_explicit( \
    const cat_atomic_##name##_t *atomic, cat_atomic_memory_order_t order) \
{ \
    return (type_name_t) cat_atomic_##bare_t##_load_at_explicit((const void *) &atomic->value, order); \
} \
static cat_always_inline void cat_atomic_##name##_store(cat_atomic_##name##_t *atomic, type_name_t desired) \
{ \
    cat_atomic_##bare_t##_store_at((void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) desired); \
} \
static cat_always_inline void cat_atomic_##name##_store_explicit( \
    cat_atomic_##name##_t *atomic, type_name_t desired, cat_atomic_memory_order_t order) \
{ \
    cat_atomic_##bare_t##_store_at_explicit((void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) desired, order); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_exchange(cat_atomic_##name##_t *atomic, type_name_t desired) \
{ \
    return (type_name_t) cat_atomic_##bare_t##_exchange_at((void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) desired); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_exchange_explicit( \
    cat_atomic_##name##_t *atomic, type_name_t desired, cat_atomic_memory_order_t order) \
{ \
    return (type_name_t) cat_atomic_##bare_t##_exchange_at_explicit( \
        (void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) desired, order); \
} \
static cat_always_inline cat_bool_t cat_atomic_##name##_compare_exchange_strong( \
    cat_atomic_##name##_t *atomic, type_name_t *expected, type_name_t desired) \
{ \
    return cat_atomic_##bare_t##_compare_exchange_strong_at( \
        (void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t *) expected, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) desired); \
} \
static cat_always_inline cat_bool_t cat_atomic_##name##_compare_exchange_strong_explicit( \
    cat_atomic_##name##_t *atomic, type_name_t *expected, type_name_t desired, \
    cat_atomic_memory_order_t success_order, cat_atomic_memory_order_t failure_order) \
{ \
    return cat_atomic_##bare_t##_compare_exchange_strong_at_explicit( \
        (void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t *) expected, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) desired, success_order, failure_order); \
} \
static cat_always_inline cat_bool_t cat_atomic_##name##_compare_exchange_weak( \
    cat_atomic_##name##_t *atomic, type_name_t *expected, type_name_t desired) \
{ \
    return cat_atomic_##bare_t##_compare_exchange_weak_at( \
        (void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t *) expected, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) desired); \
} \
static cat_always_inline cat_bool_t cat_atomic_##name##_compare_exchange_weak_explicit( \
    cat_atomic_##name##_t *atomic, type_name_t *expected, type_name_t desired, \
    cat_atomic_memory_order_t success_order, cat_atomic_memory_order_t failure_order) \
{ \
    return cat_atomic_##bare_t##_compare_exchange_weak_at_explicit( \
        (void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t *) expected, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) desired, success_order, failure_order); \
}

#define CAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_GEN(name, type_name_t, bare_t) \
static cat_always_inline type_name_t cat_atomic_##name##_fetch_add(cat_atomic_##name##_t *atomic, type_name_t operand) \
{ \
    return (type_name_t) cat_atomic_##bare_t##_fetch_add_at( \
        (void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) operand); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_fetch_add_explicit( \
    cat_atomic_##name##_t *atomic, type_name_t operand, cat_atomic_memory_order_t order) \
{ \
    return (type_name_t) cat_atomic_##bare_t##_fetch_add_at_explicit( \
        (void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) operand, order); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_fetch_sub(cat_atomic_##name##_t *atomic, type_name_t operand) \
{ \
    return (type_name_t) cat_atomic_##bare_t##_fetch_sub_at( \
        (void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) operand); \
} \
static cat_always_inline type_name_t cat_atomic_##name##_fetch_sub_explicit( \
    cat_atomic_##name##_t *atomic, type_name_t operand, cat_atomic_memory_order_t order) \
{ \
    return (type_name_t) cat_atomic_##bare_t##_fetch_sub_at_explicit( \
        (void *) &atomic->value, (CAT_ATOMIC_BARE_PAYLOAD_ ## bare_t) operand, order); \
}

#define CAT_ATOMIC_OPERATION_FUNCTIONS_GEN(name, type_name_t, bare_t) \
        CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_GEN(name, type_name_t, bare_t) \
        CAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_GEN(name, type_name_t, bare_t) \


CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_MAP(CAT_ATOMIC_COMMON_OPERATION_FUNCTIONS_GEN)
CAT_ATOMIC_NUMERIC_OPERATION_FUNCTIONS_MAP(CAT_ATOMIC_OPERATION_FUNCTIONS_GEN)

#ifdef __cplusplus
}
#endif
#endif /* CAT_ATOMIC_H */
