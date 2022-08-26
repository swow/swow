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

Assert::throws(function () {
    $coro = Coroutine::run('I am NOT a callable'); // for Coroutine::run()
    echo 'Never Here' . PHP_LF;
    var_dump($coro);
}, Error::class, expectMessage: '/Argument #1 \(\$callable\) must be a valid callback, function "I am NOT a callable" not found or invalid function name/');

Assert::throws(function () {
    $coro = new Coroutine('I am NOT a callable'); // for new Coroutine()
    echo 'Never Here' . PHP_LF;
    var_dump($coro);
}, Error::class, expectMessage: '/Argument #1 \(\$callable\) must be a valid callback, function "I am NOT a callable" not found or invalid function name/');

$coro = new Coroutine(function () {
    Coroutine::yield();
});

$coro->resume();

Assert::throws(function () use ($coro) {
    $ret = $coro->call('I am NOT a callable');
    echo 'Never Here' . PHP_LF;
    var_dump($ret);
}, Error::class, expectMessage: '/Argument #1 \(\$callable\) must be a valid callback, function "I am NOT a callable" not found or invalid function name/');

$coro->resume();

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
