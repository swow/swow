--TEST--
swow_coroutine: yield in call, eval
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip('fixme: implement issue here');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$remote_coro = Coroutine::run(function () {
    // yield first to test call
    Coroutine::yield();
});

$closure = function () {
    // context switching is forbidden in called functions
    Coroutine::yield();
};

try {
    // call a closure in remote
    $remote_coro->call($closure);
    echo 'Never here' . PHP_LF;
} catch (Throwable $e) {
} finally {
    $remote_coro->resume();
}

Assert::isInstanceOf($e, Error::class);
Assert::same($e->getMessage(), 'Unexpected coroutine switching');

$remote_coro = Coroutine::run(function () use ($closure) {
    // yield to test eval
    Coroutine::yield();
});

try {
    // cannot issue a context switch via eval
    $remote_coro->eval('\Swow\Coroutine::yield();');
    echo 'Never here' . PHP_LF;
} catch (Throwable $e) {
} finally {
    $remote_coro->resume();
}

Assert::isInstanceOf($e, Error::class);
Assert::same($e->getMessage(), 'Unexpected coroutine switching');

$remote_coro = Coroutine::run(function () use ($closure) {
    // yield to test eval
    Coroutine::yield();
});

try {
    // cannot create a coroutine in eval
    $remote_coro->eval('$coro_denied = new \Swow\Coroutine(function(){});');
    echo 'Never here' . PHP_LF;
} catch (Throwable $e) {
} finally {
    $remote_coro->resume();
}

Assert::isInstanceOf($e, Error::class);
Assert::same($e->getMessage(), 'The object of Swow\Coroutine can not be created for security reasons');

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
