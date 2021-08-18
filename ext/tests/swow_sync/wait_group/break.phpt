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
use Swow\Sync\WaitGroup;
use const Swow\Errno\ECANCELED;

$wg = new WaitGroup();
$wg->add(2);
// normal done
Coroutine::run(function () use ($wg) {
    $wg->done();
});
// wait it
$waiting_coro = Coroutine::run(function () use ($wg) {
    try {
        $wg->wait();
        echo 'wait should not success' . PHP_LF;
    } catch (Swow\Sync\Exception $e) {
        Assert::same($e->getCode(), ECANCELED);
    }
});
// cancel wait
$waiting_coro->resume();
// clear wg to avoid deadlock
$wg->done();
echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
