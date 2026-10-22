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

$timerPrecision = measure_timer_precision();
$shortSleep = max(5, $timerPrecision * 2);
$longSleep = max(10, $timerPrecision * 4);

Swow\Coroutine::run(static function () use ($wr, $shortSleep, $longSleep): void {
    echo "coroutine[1] start\n";
    Swow\Coroutine::run(static function () use ($wr, $longSleep): void {
        echo "coroutine[2] start\n";
        msleep($longSleep);
        echo "coroutine[2] exit\n";
    });
    msleep($shortSleep);
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
