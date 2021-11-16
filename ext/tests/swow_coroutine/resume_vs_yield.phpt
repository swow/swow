--TEST--
swow_coroutine: resume vs yield
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

Coroutine::run(function () {
    echo '1' . PHP_LF;
    Coroutine::run(function () {
        echo '2' . PHP_LF;
        Coroutine::getCurrent()->getPrevious()->resume();
        echo '4' . PHP_LF;
    });
    echo '3' . PHP_LF;
});
echo '5' . PHP_LF;

echo PHP_LF;

Coroutine::run(function () {
    echo '1' . PHP_LF;
    $coroutine = Coroutine::run(function () {
        echo '2' . PHP_LF;
        Coroutine::yield();
        echo '4' . PHP_LF;
    });
    echo '3' . PHP_LF;
    $coroutine->resume();
    echo '5' . PHP_LF;
});
echo '6' . PHP_LF;

?>
--EXPECT--
1
2
3
4
5

1
2
3
4
5
6
