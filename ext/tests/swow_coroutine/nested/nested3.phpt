--TEST--
swow_coroutine/nested: nested 3
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
        msleep(1);
        echo 'coroutine[2] exit' . PHP_LF;
    });
    msleep(2);
    echo 'coroutine[1] exit' . PHP_LF;
});
echo 'end' . PHP_LF;
// TODO: WaitGroup

?>
--EXPECT--
coroutine[1] start
coroutine[2] start
end
coroutine[2] exit
coroutine[1] exit
