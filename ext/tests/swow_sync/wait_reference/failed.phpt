--TEST--
swow_sync/wait_reference: failure situation
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
Coroutine::run(function ($wr) {
    WaitReference::wait($wr);
}, $wr);
Coroutine::run(function ($wr) {
    // double wait is not allowed
    try {
        WaitReference::wait($wr);
        echo 'double wait success' . PHP_LF;
    } catch (Error$e) {
        Assert::same($e->getMessage(), 'WaitReference can not be reused before previous wait has returned');
    }
}, $wr);
unset($wr);
echo 'Done 1' . PHP_LF;

$wr = new WaitReference();
// normal done
Coroutine::run(function () use ($wr) {
    // do nothing
    pseudo_random_sleep();
});
// wait it
Coroutine::run(function ($wr) {
    try {
        // will time out immediately
        WaitReference::wait($wr, 0);
        echo 'wait should not success' . PHP_LF;
    } catch (SyncException $e) {
        Assert::same($e->getCode(), Errno::ETIMEDOUT);
    }
}, $wr);
msleep(10);
unset($wr);
echo 'Done 2' . PHP_LF;

?>
--EXPECT--
Done 1
Done 2
