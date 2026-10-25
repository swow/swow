--TEST--
swow_thread: Futex C interface NULL guards
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
skip_if_extension_not_exist('ffi');
skip_if_c_function_not_exist('int swow_futex_get_value(const void *);');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Futex;
use Swow\SyncException;

$ffi = FFI::cdef(<<<'DEF'
typedef struct swow_futex_s swow_futex_t;
typedef struct cat_shm_s cat_shm_t;

swow_futex_t *swow_futex_get(void *object);
int swow_futex_wait(swow_futex_t *futex, int expected, long long timeout);
int swow_futex_wake(swow_futex_t *futex, int count);
int swow_futex_get_value(const swow_futex_t *futex);
int swow_futex_add(swow_futex_t *futex, int delta);
int swow_futex_compare_exchange(swow_futex_t *futex, int expected, int new_value);
int swow_futex_exchange(swow_futex_t *futex, int new_value);

cat_shm_t *cat_shm_create_named(const char *name, size_t size);
void cat_shm_detach(cat_shm_t *shm);
int cat_shm_unlink(const char *name);
DEF);

Assert::null($ffi->swow_futex_get(null));
Assert::same(0, $ffi->swow_futex_wait(null, 0, -1));
Assert::same(0, $ffi->swow_futex_wake(null, 1));
Assert::same(0, $ffi->swow_futex_get_value(null));
Assert::same(0, $ffi->swow_futex_add(null, 1));
Assert::same(0, $ffi->swow_futex_compare_exchange(null, 0, 1));
Assert::same(0, $ffi->swow_futex_exchange(null, 0));

# a mapping smaller than the futex payload is rejected; macOS maps a shared
# memory object by page, so a small object reports a page-sized mapping, the
# size check passes there, and the magic check rejects it
$name = 'ftx-tiny-' . getmypid() . '-' . bin2hex(random_bytes(4));
$tiny = $ffi->cat_shm_create_named($name, 4);
Assert::true($tiny !== null);
$tinyMessage = PHP_OS_FAMILY === 'Darwin' ? '/not a futex/' : '/too small/';
try {
    Assert::throws(static function () use ($name): void {
        Futex::attachNamed($name);
    }, SyncException::class, null, $tinyMessage);
} finally {
    $ffi->cat_shm_detach($tiny);
    $ffi->cat_shm_unlink($name);
}

echo "Done\n";
?>
--EXPECT--
Done
