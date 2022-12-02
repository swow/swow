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
use Swow\Sync\WaitReference;
use Swow\SyncException;

$wr = new WaitReference();
// normal done
Coroutine::run(static function () use ($wr): void {
    // do nothing
    pseudo_random_sleep();
});
// wait it
$waiting_coro = Coroutine::run(static function ($wr): void {
    try {
        WaitReference::wait($wr);
        echo "wait should not success\n";
    } catch (SyncException $e) {
        Assert::same($e->getCode(), Errno::ECANCELED);
    }
}, $wr);
$waiting_coro->resume();
unset($wr);
echo "Done\n";

?>
--EXPECT--
Done
