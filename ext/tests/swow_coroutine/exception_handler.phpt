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

set_exception_handler(static function (Throwable $e): void {
    echo 'outer handler ' . $e->getMessage() . "\n";
});

$coro = Coroutine::run(static function () use (&$inner_coro): void {
    $inner_coro = Coroutine::run(static function (): void {
        Coroutine::yield();
        set_exception_handler(static function (Throwable $e): void {
            echo 'inner handler ' . $e->getMessage() . "\n";
        });
        throw new Exception('inner exception');
    });

    set_exception_handler(static function (Throwable $e): void {
        echo 'in-coro handler ' . $e->getMessage() . "\n";
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
