--TEST--
swow_coroutine/nested: nested 2
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
    Swow\Coroutine::run(function () use ($wr) {
        echo 'coroutine[2] start' . PHP_LF;
        msleep(2);
        echo 'coroutine[2] exit' . PHP_LF;
    });
    msleep(1);
    echo 'coroutine[1] exit' . PHP_LF;
});
echo 'end' . PHP_LF;

WaitReference::wait($wr);
echo 'done' . PHP_LF;

?>
--EXPECT--
coroutine[1] start
coroutine[2] start
end
coroutine[1] exit
coroutine[2] exit
done
