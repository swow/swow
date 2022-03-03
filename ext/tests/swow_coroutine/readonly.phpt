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
            echo 'Never here' . PHP_LF;
        });
    } catch (Error $error) {
        Assert::same($error->getCode(), Errno::EMISUSE);
    }
    return new Coroutine(function () {
        echo 'Here I am' . PHP_LF;
    });
});
Coroutine::run(function () {
    echo 'It\'s OK' . PHP_LF;
});
Coroutine::getCurrent()->call(function () {
    Coroutine::run(function () {
        echo 'Create new coroutine in $currentCoroutine->call() is allowed' . PHP_LF;
    });
});
$subCoroutine->resume();
$coroutine->resume();

echo 'Done' . PHP_LF;
?>
--EXPECT--
It's OK
Create new coroutine in $currentCoroutine->call() is allowed
Here I am
Done
