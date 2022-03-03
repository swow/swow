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
use Swow\Errno;
use Swow\SyncException;
use Swow\Sync\WaitReference;

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
    } catch (SyncException $e) {
        Assert::same($e->getCode(), Errno::ECANCELED);
    }
}, $wr);
$waiting_coro->resume();
unset($wr);
echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
