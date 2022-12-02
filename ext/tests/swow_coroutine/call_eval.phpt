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

$remote_coro = Coroutine::run(static function (): void {
    // yield first to test call
    Coroutine::yield('nonce');
});

$closure = static function (): void {
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
    echo "caught throwable\n";
    var_dump($e);
} finally {
    $remote_coro->resume();
}

$remote_coro = Coroutine::run(static function () use ($closure): void {
    // yield to test eval
    $a = 0;
    Coroutine::yield('nonce2');
    Assert::same($a, 1);
});

try {
    // eval a statement
    $remote_coro->eval('$a++;');
} catch (Throwable $e) {
    echo "caught throwable\n";
    var_dump($e);
} finally {
    $remote_coro->resume();
}

echo "Done\n";

?>
--EXPECT--
Done
