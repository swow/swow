--TEST--
swow_sync/wait_group: cancel wait
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
use Swow\SyncException;

$wg = new WaitGroup();
$wg->add(2);
// normal done
Coroutine::run(static function () use ($wg): void {
    $wg->done();
});
// wait it
$waiting_coro = Coroutine::run(static function () use ($wg): void {
    try {
        $wg->wait();
        echo "wait should not success\n";
    } catch (SyncException $e) {
        Assert::same($e->getCode(), Errno::ECANCELED);
    }
});
// cancel wait
$waiting_coro->resume();
// clear wg to avoid deadlock
$wg->done();
echo "Done\n";

?>
--EXPECT--
Done
