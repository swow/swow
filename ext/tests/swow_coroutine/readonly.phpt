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

$coroutine = Coroutine::run(function () {
    Coroutine::yield();
});
$subCoroutine = $coroutine->call(function (): Coroutine {
    try {
        Coroutine::run(function () {
            echo "Never here\n";
        });
    } catch (Error $error) {
        Assert::same($error->getCode(), Errno::EMISUSE);
    }
    return new Coroutine(function () {
        echo "Here I am\n";
    });
});
Coroutine::run(function () {
    echo "It's OK\n";
});
Coroutine::getCurrent()->call(function () {
    Coroutine::run(function () {
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
