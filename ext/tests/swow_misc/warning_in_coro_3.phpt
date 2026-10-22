--TEST--
swow_misc: trig deprecation and errors in coroutine
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_VERSION_ID >= 80200 || PHP_VERSION_ID < 70200, 'no proper deprecation in this version of PHP');
if (memory_get_usage() === 0) {
    // zend mm not enabled, skip test
    exit('SKIP: zend mm not enabled');
}
?>
--INI--
memory_limit=128M
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
use Swow\Coroutine;

Coroutine::run(static function (): void {
    // E_ERROR
    $_ = str_repeat('the quick brown twosee jumps over the lazy black dixyes', 128 * 1048576);
});

echo "Never here\n";
?>
--EXPECTREGEX--
(?![\s\S]*(?:Never here|Deadlock|process timed out))(?:Fatal error: \[Fatal error in R\d+\] Allowed memory size of \d+ bytes exhausted[^\n]* \(tried to allocate \d+ bytes\)\nStack trace:\n#\d+ [^\r\n]+warning_in_coro_3\.php\(\d+\): [^\r\n]+\n#\d+ \[internal function\]: \{closur[^\r\n]+\}\(\)[\s\S]*\n#\d+ [^\r\n]+warning_in_coro_3\.php\(\d+\): Swow\\Coroutine::run\(Object\(Closure\)\)\n#\d+ \{main\}\n  triggered in [^\r\n]+warning_in_coro_3\.php on line \d+|Fatal error: Allowed memory size of \d+ bytes exhausted[^\n]* \(tried to allocate \d+ bytes\) in [^\n]+warning_in_coro_3\.php on line \d+)
