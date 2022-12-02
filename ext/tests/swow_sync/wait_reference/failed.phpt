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
use Swow\Sync\WaitReference;
use Swow\SyncException;

$wr = new WaitReference();
// normal done
Coroutine::run(static function () use ($wr): void {
    // do nothing
    pseudo_random_sleep();
});
// wait it
Coroutine::run(static function ($wr): void {
    WaitReference::wait($wr);
}, $wr);
Coroutine::run(static function ($wr): void {
    // double wait is not allowed
    try {
        WaitReference::wait($wr);
        echo "double wait success\n";
    } catch (Error$e) {
        Assert::same($e->getMessage(), 'WaitReference can not be reused before previous wait has returned');
    }
}, $wr);
unset($wr);
echo "Done 1\n";

$wr = new WaitReference();
// normal done
Coroutine::run(static function () use ($wr): void {
    // do nothing
    pseudo_random_sleep();
});
// wait it
Coroutine::run(static function ($wr): void {
    try {
        // will time out immediately
        WaitReference::wait($wr, 0);
        echo "wait should not success\n";
    } catch (SyncException $e) {
        Assert::same($e->getCode(), Errno::ETIMEDOUT);
    }
}, $wr);
msleep(10);
unset($wr);
echo "Done 2\n";

?>
--EXPECT--
Done 1
Done 2
