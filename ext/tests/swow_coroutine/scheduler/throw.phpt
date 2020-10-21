--TEST--
swow_coroutine/event_scheduler: throw exception to event scheduler
--SKIPIF--
<?php require __DIR__ . '/../../include/skipif.php'; ?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

Swow\Coroutine::run(function () {
    usleep(1000);
    $eventScheduler = Swow\Coroutine::getCurrent()->getPrevious();
    $eventScheduler->throw(new Exception);
});

?>
--EXPECTF--
Warning: [Fatal error in R2] Uncaught Swow\Coroutine\Exception: Coroutine is not in executing in %s:%d
Stack trace:
#0 %s(%d): Swow\Coroutine->throw(Object(Exception))
#1 [internal function]: {closure}()
#2 {main}
  thrown in %s on line %d
