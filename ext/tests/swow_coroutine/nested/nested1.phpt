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
    echo "coroutine[1] start\n";
    msleep(1);
    Swow\Coroutine::run(function () use ($wr) {
        echo "coroutine[2] start\n";
        msleep(1);
        echo "coroutine[2] exit\n";
    });
    echo "coroutine[1] exit\n";
});

WaitReference::wait($wr);
echo "end\n";

?>
--EXPECT--
coroutine[1] start
coroutine[2] start
coroutine[1] exit
coroutine[2] exit
end
