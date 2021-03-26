--TEST--
swow_coroutine/event_scheduler: cross resume event scheduler
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

Swow\Coroutine::run(function () {
    $eventScheduler = Swow\Coroutine::getCurrent()->getPrevious()->getPrevious();
    $eventScheduler->resume();
});

?>
--EXPECTF--
Warning: [Fatal error in R%d] Uncaught Swow\Coroutine\Exception: Coroutine is running in %s:%d
Stack trace:
#0 %s(%d): Swow\Coroutine->resume()
#1 [internal function]: {closure}()
#2 {main}
  thrown in %s on line %d
