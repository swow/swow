--TEST--
swow_coroutine: call eval
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$remote_coro = Coroutine::run(function () {
    // yield first to test call
    Coroutine::yield('nonce');
});

$closure = function () {
    $upper_frame = debug_backtrace()[1];
    Assert::same($upper_frame['function'], 'yield');
    Assert::same($upper_frame['class'], 'Swow\\Coroutine');
    Assert::same($upper_frame['type'], '::');
    Assert::same($upper_frame['args'][0], 'nonce');
};

try {
    // call a closure in remote
    $remote_coro->call($closure);
} catch (Throwable $e) {
    echo 'caught throwable' . PHP_LF;
    var_dump($e);
} finally {
    $remote_coro->resume();
}

$remote_coro = Coroutine::run(function () use ($closure) {
    // yield to test eval
    $a = 0;
    Coroutine::yield('nonce2');
    Assert::same($a, 1);
});

try {
    // eval a statement
    $remote_coro->eval('$a++;');
} catch (Throwable $e) {
    echo 'caught throwable' . PHP_LF;
    var_dump($e);
} finally {
    $remote_coro->resume();
}

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
