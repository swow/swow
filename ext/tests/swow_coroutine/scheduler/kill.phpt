--TEST--
swow_coroutine/event_scheduler: kill event scheduler
--SKIPIF--
<?php require __DIR__ . '/../../include/skipif.php'; ?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Coroutine;
use Swow\CoroutineException;
use Swow\Sync\WaitReference;

$wr = new WaitReference();

Coroutine::run(static function () use ($wr): void {
    usleep(1000);
    $eventScheduler = Coroutine::getCurrent()->getPrevious();
    Assert::throws(static function () use ($eventScheduler): void {
        $eventScheduler->kill();
    }, CoroutineException::class);
});

WaitReference::wait($wr);

echo "Done\n";

?>
--EXPECT--
Done
