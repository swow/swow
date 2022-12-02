--TEST--
swow_coroutine: bad args passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

Assert::throws(static function (): void {
    $coro = Coroutine::run('I am NOT a callable'); // for Coroutine::run()
    echo "Never Here\n";
    var_dump($coro);
}, Error::class, expectMessage: '/Argument #1 \(\$callable\) must be a valid callback, function "I am NOT a callable" not found or invalid function name/');

Assert::throws(static function (): void {
    $coro = new Coroutine('I am NOT a callable'); // for new Coroutine()
    echo "Never Here\n";
    var_dump($coro);
}, Error::class, expectMessage: '/Argument #1 \(\$callable\) must be a valid callback, function "I am NOT a callable" not found or invalid function name/');

$coro = new Coroutine(static function (): void {
    Coroutine::yield();
});

$coro->resume();

Assert::throws(static function () use ($coro): void {
    $ret = $coro->call('I am NOT a callable');
    echo "Never Here\n";
    var_dump($ret);
}, Error::class, expectMessage: '/Argument #1 \(\$callable\) must be a valid callback, function "I am NOT a callable" not found or invalid function name/');

$coro->resume();

echo "Done\n";

?>
--EXPECT--
Done
