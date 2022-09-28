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

Swow\Coroutine::run(static function () use ($wr): void {
    echo "coroutine[1] start\n";
    Swow\Coroutine::run(static function () use ($wr): void {
        echo "coroutine[2] start\n";
        msleep(2);
        echo "coroutine[2] exit\n";
    });
    msleep(1);
    echo "coroutine[1] exit\n";
});
echo "end\n";

WaitReference::wait($wr);
echo "done\n";

?>
--EXPECT--
coroutine[1] start
coroutine[2] start
end
coroutine[1] exit
coroutine[2] exit
done
