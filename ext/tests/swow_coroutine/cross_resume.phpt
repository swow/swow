--TEST--
swow_coroutine: cross resume
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
        Coroutine::run(function () {
            echo '3' . PHP_LF;
            Coroutine::getCurrent()->getPrevious()->getPrevious()->resume();
            echo '5' . PHP_LF;
        });
        echo '6' . PHP_LF;
    });
    echo '4' . PHP_LF;
});
echo '7' . PHP_LF;

?>
--EXPECT--
1
2
3
4
5
6
7
