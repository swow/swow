--TEST--
swow_coroutine/nested: nested 1
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Sync\WaitReference;

$wr = new WaitReference();

Swow\Coroutine::run(function () use ($wr) {
    echo 'coroutine[1] start' . PHP_LF;
    msleep(1);
    Swow\Coroutine::run(function () use ($wr) {
        echo 'coroutine[2] start' . PHP_LF;
        msleep(1);
        echo 'coroutine[2] exit' . PHP_LF;
    });
    echo 'coroutine[1] exit' . PHP_LF;
});

WaitReference::wait($wr);
echo 'end' . PHP_LF;

?>
--EXPECT--
coroutine[1] start
coroutine[2] start
coroutine[1] exit
coroutine[2] exit
end
