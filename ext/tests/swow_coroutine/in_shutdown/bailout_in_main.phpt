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
--EXPECTREGEX--
(?![\s\S]*(?:Never here|Deadlock|process timed out))(?:Fatal error: \[Fatal error in main\] Allowed memory size of \d+ bytes exhausted[^\n]* \(tried to allocate \d+ bytes\)
Stack trace:
(?:#0 [^\n]+bailout_in_main\.php\(\d+\): str_repeat\('X', 134217728\)\n)?#\d+ \{main\}
  triggered in [^\r\n]+bailout_in_main\.php on line \d+

Coroutine count: \d+|Fatal error: Allowed memory size of \d+ bytes exhausted[^\n]* \(tried to allocate \d+ bytes\) in [^\n]+bailout_in_main\.php on line \d+)
