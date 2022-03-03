--TEST--
swow_sync/wait_group: failure situation
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Errno;
use Swow\Sync\WaitGroup;

$wg = new WaitGroup();
// done without add is not allowed
Assert::throws(function () use ($wg) {
    $wg->done();
}, Error::class, 'WaitGroup counter can not be negative');
$wg->add(2);
// normal done
Coroutine::run(function () use ($wg) {
    $wg->done();
});
// wait it
Coroutine::run(function () use ($wg) {
    $wg->wait();
});
Coroutine::run(function () use ($wg) {
    // add on wait is not allowed
    Assert::throws(function () use ($wg) {
        $wg->add(1);
    }, Error::class, 'WaitGroup add called concurrently with wait');

    // double wait is not allowed
    Assert::throws(function () use ($wg) {
        $wg->wait();
    }, Error::class, 'WaitGroup can not be reused before previous wait has returned');
});
// clear wg to avoid deadlock
$wg->done();
echo 'Done 1' . PHP_LF;

$wg = new WaitGroup();
$wg->add(1);
// wait it time out
Coroutine::run(function () use ($wg) {
    try {
        // will time out immediately
        $wg->wait(0);
    } catch (Swow\Sync\Exception $e) {
        Assert::same($e->getCode(), Errno::ETIMEDOUT);
    }
});
// clear wg to avoid deadlock
$wg->done();
echo 'Done 2' . PHP_LF;

?>
--EXPECT--
Done 1
Done 2
