--TEST--
swow_coroutine/nested: nested empty
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

Swow\Coroutine::run(function () {
    echo 'coroutine[1] start' . PHP_LF;
    Swow\Coroutine::run(function () {
        echo 'coroutine[2] start' . PHP_LF;
        echo 'coroutine[2] exit' . PHP_LF;
    });
    echo 'coroutine[1] exit' . PHP_LF;
});

?>
--EXPECT--
coroutine[1] start
coroutine[2] start
coroutine[2] exit
coroutine[1] exit
