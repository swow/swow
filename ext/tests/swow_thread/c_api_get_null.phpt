--TEST--
swow_thread: C handle getters reject a NULL object
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('ffi');
skip_if_c_function_not_exist('void *swow_futex_get(void *object);');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$ffi = FFI::cdef(<<<'DEF'
typedef struct swow_futex_s swow_futex_t;
typedef struct swow_mutex_s swow_mutex_t;
typedef struct swow_semaphore_s swow_semaphore_t;
typedef struct swow_thannel_s swow_thannel_t;
typedef struct swow_thread_s swow_thread_t;
typedef struct swow_thread_barrier_s swow_thread_barrier_t;

swow_futex_t *swow_futex_get(void *object);
swow_mutex_t *swow_mutex_get(void *object);
swow_semaphore_t *swow_semaphore_get(void *object);
swow_thannel_t *swow_thannel_get(void *object);
swow_thread_t *swow_thread_get(void *object);
swow_thread_barrier_t *swow_thread_barrier_get(void *object);
DEF);

Assert::null($ffi->swow_futex_get(null));
Assert::null($ffi->swow_mutex_get(null));
Assert::null($ffi->swow_semaphore_get(null));
Assert::null($ffi->swow_thannel_get(null));
Assert::null($ffi->swow_thread_get(null));
Assert::null($ffi->swow_thread_barrier_get(null));

echo "Done\n";
?>
--EXPECT--
Done
