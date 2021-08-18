--TEST--
swow_sync/wait_reference: cancel wait
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;
use const Swow\Errno\ECANCELED;

$wr = new WaitReference();
// normal done
Coroutine::run(function () use ($wr) {
    // do nothing
    pseudo_random_sleep();
});
// wait it
$waiting_coro = Coroutine::run(function ($wr) {
    try {
        WaitReference::wait($wr);
        echo 'wait should not success' . PHP_LF;
    } catch (Swow\Sync\Exception $e) {
        Assert::same($e->getCode(), ECANCELED);
    }
}, $wr);
$waiting_coro->resume();
unset($wr);
echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
