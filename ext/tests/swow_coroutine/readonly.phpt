--TEST--
swow_coroutine: readonly
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Errno;

$coroutine = Coroutine::run(static function (): void {
    Coroutine::yield();
});
$subCoroutine = $coroutine->call(static function (): Coroutine {
    try {
        Coroutine::run(static function (): void {
            echo "Never here\n";
        });
    } catch (Error $error) {
        Assert::same($error->getCode(), Errno::EMISUSE);
    }
    return new Coroutine(static function (): void {
        echo "Here I am\n";
    });
});
Coroutine::run(static function (): void {
    echo "It's OK\n";
});
Coroutine::getCurrent()->call(static function (): void {
    Coroutine::run(static function (): void {
        echo "Create new coroutine in \$currentCoroutine->call() is allowed\n";
    });
});
$subCoroutine->resume();
$coroutine->resume();

echo "Done\n";
?>
--EXPECT--
It's OK
Create new coroutine in $currentCoroutine->call() is allowed
Here I am
Done
