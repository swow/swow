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
    $coro = Coroutine::run('I am NOT a callable');
    echo 'Never Here' . PHP_LF;
    var_dump($coro);
}, Error::class, 'Coroutine function must be callable, function "I am NOT a callable" not found or invalid function name');

Assert::throws(function () {
    $coro = new Coroutine('I am NOT a callable');
    echo 'Never Here' . PHP_LF;
    var_dump($coro);
}, Error::class, 'Coroutine function must be callable, function "I am NOT a callable" not found or invalid function name');

$coro = new Coroutine(function () {
    Coroutine::yield();
});

$coro->resume();

try {
    $ret = $coro->call('I am NOT a callable');
    echo 'Never Here' . PHP_LF;
    var_dump($ret);
} catch (Throwable $e) {
} finally {
    $coro->resume();
}

if (isset($e)) {
    Assert::isInstanceOf($e, Error::class);
    //Assert::same($e->getMessage(), 'Coroutine::call() only accept callable parameter, function "I am NOT a callable" not found or invalid function name');
}

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
