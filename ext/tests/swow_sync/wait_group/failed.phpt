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
Assert::throws(static function () use ($wg): void {
    $wg->done();
}, Error::class, expectMessage: 'WaitGroup counter can not be negative');
$wg->add(2);
// normal done
Coroutine::run(static function () use ($wg): void {
    $wg->done();
});
// wait it
Coroutine::run(static function () use ($wg): void {
    $wg->wait();
});
Coroutine::run(static function () use ($wg): void {
    // add on wait is not allowed
    Assert::throws(static function () use ($wg): void {
        $wg->add(1);
    }, Error::class, expectMessage: 'WaitGroup add called concurrently with wait');

    // double wait is not allowed
    Assert::throws(static function () use ($wg): void {
        $wg->wait();
    }, Error::class, expectMessage: 'WaitGroup can not be reused before previous wait has returned');
});
// clear wg to avoid deadlock
$wg->done();
echo "Done 1\n";

$wg = new WaitGroup();
$wg->add(1);
// wait it time out
Coroutine::run(static function () use ($wg): void {
    try {
        // will time out immediately
        $wg->wait(0);
    } catch (Swow\Sync\Exception $e) {
        Assert::same($e->getCode(), Errno::ETIMEDOUT);
    }
});
// clear wg to avoid deadlock
$wg->done();
echo "Done 2\n";

?>
--EXPECT--
Done 1
Done 2
