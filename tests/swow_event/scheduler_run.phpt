--TEST--
swow_event: scheduler run
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

Swow\Event\Scheduler::run();

?>
--EXPECTF--
Warning: [Fatal error in R1] Uncaught Error: Event loop is running in %s:%d
Stack trace:
#0 %s(%d): Swow\Event\Scheduler::run()
#1 {main}
  thrown in %s on line %d
