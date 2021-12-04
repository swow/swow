--TEST--
swow_coroutine: dead lock
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Signal;

php_proc_with_swow(__DIR__ . '/dead_lock.inc', function ($proc, array $pipes) {
    echo fread($pipes[2], 4096);
    Signal::kill(proc_get_status($proc)['pid'], Signal::TERM);
});

echo PHP_LF . 'Done' . PHP_LF;

?>
--EXPECTF--
Warning: <COROUTINE> Dead lock: all coroutines are asleep in scheduler

Done
