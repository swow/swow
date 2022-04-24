--TEST--
swow_coroutine: bailout in shutdown functions
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Coroutine;
use function Swow\Sync\waitAll;

register_shutdown_function(function () {
    Assert::same(Coroutine::count(), TEST_MAX_CONCURRENCY + 2);
    echo sprintf("\nCoroutine count: %d\n", Coroutine::count());
    echo str_repeat('X', 128 * 1024 * 1024 + 1);
});

for ($c = 0; $c < TEST_MAX_CONCURRENCY; $c++) {
    Coroutine::run(function () {
        Coroutine::yield();
        echo "Never here\n";
    });
}

Coroutine::run(function () {
    echo str_repeat('X', 128 * 1024 * 1024);
});

waitAll();

?>
--EXPECTF--
%AFatal error: [Fatal error in R%d] Allowed memory size of %d bytes exhausted%A (tried to allocate %d bytes)
Stack trace:
#0 %sbailout_in_shutdown_functions.php(%d): str_repeat('X', 134217728)
#1 [internal function]: {closure}()
#2 %sbailout_in_shutdown_functions.php(%d): Swow\Coroutine::run(Object(Closure))
#3 {main}
  triggered in %sbailout_in_shutdown_functions.php on line %d

Coroutine count: 66

%AFatal error: [Fatal error in main] Allowed memory size of %d bytes exhausted%A (tried to allocate %d bytes)
Stack trace:
#0 %sbailout_in_shutdown_functions.php(%d): str_repeat('X', 134217729)
#1 [internal function]: {closure}()
#2 {main}
  triggered in %sbailout_in_shutdown_functions.php on line 10
