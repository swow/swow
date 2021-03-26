--TEST--
swow_coroutine/event_scheduler: kill event scheduler
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
    $eventScheduler->kill();
});

WaitReference::wait($wr);

?>
--EXPECTF--
Warning: [Fatal error in R%d] Uncaught Swow\Coroutine\Exception: Coroutine is not in executing in %s:%d
Stack trace:
#0 %s(%d): Swow\Coroutine->kill()
#1 [internal function]: {closure}()
#2 {main}
  thrown in %s on line %d
