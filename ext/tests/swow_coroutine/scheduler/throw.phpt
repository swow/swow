--TEST--
swow_coroutine/event_scheduler: throw exception to event scheduler
--SKIPIF--
<?php require __DIR__ . '/../../include/skipif.php'; ?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Sync\WaitReference;

$wr = new WaitReference();

Swow\Coroutine::run(function () use ($wr) {
    usleep(1000);
    $eventScheduler = Swow\Coroutine::getCurrent()->getPrevious();
    Assert::throws(function () use ($eventScheduler) {
        $eventScheduler->throw(new Exception);
    }, Swow\Coroutine\Exception::class);
});

WaitReference::wait($wr);

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
