--TEST--
swow_coroutine: exception handler on coroutine
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
display_errors=0
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

set_exception_handler(function (Throwable $e) {
    echo 'outer handler ' . $e->getMessage() . PHP_LF;
});

$coro = Coroutine::run(function () use (&$inner_coro) {
    $inner_coro = Coroutine::run(function () {
        Coroutine::yield();
        set_exception_handler(function (Throwable $e) {
            echo 'inner handler ' . $e->getMessage() . PHP_LF;
        });
        throw new Exception('inner exception');
    });

    set_exception_handler(function (Throwable $e) {
        echo 'in-coro handler ' . $e->getMessage() . PHP_LF;
    });
    throw new Exception('in-coro exception');
});

$inner_coro->resume();

// note: this exception will be handled by inner handler
// exception_handler should be regarded as a global object
/* @noinspection PhpUnhandledExceptionInspection */
throw new Exception('outer exception');
/* @noinspection PhpUnreachableStatementInspection */
echo "Never here\n";
?>
--EXPECT--
in-coro handler in-coro exception
inner handler inner exception
inner handler outer exception
