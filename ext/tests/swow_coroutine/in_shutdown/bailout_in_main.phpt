--TEST--
swow_coroutine/in_shutdown: bailout in main
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';

if (memory_get_usage() === 0) {
    // zend mm not enabled, skip test
    exit('SKIP: zend mm not enabled');
}
?>
--INI--
memory_limit=32M
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Coroutine;

register_shutdown_function(static function (): void {
    Assert::same(Coroutine::count(), TEST_MAX_CONCURRENCY + 1);
    echo sprintf("\nCoroutine count: %d\n", Coroutine::count());
});

for ($c = 0; $c < TEST_MAX_CONCURRENCY; $c++) {
    Coroutine::run(static function (): void {
        Coroutine::yield();
        echo "Never here\n";
    });
}

$str128M = str_repeat('X', 128 * 1024 * 1024);
var_dump(md5($str128M));

echo "Never here\n";
?>
--EXPECTF--
%AFatal error: [Fatal error in main] Allowed memory size of %d bytes exhausted%A (tried to allocate %d bytes)
Stack trace:
#0 %sbailout_in_main.php(%d): str_repeat('X', 134217728)
#1 {main}
  triggered in %sbailout_in_main.php on line %d

Coroutine count: %d
