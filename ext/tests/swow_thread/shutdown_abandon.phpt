--TEST--
swow_thread: a child that outlives the shutdown timeout does not crash the exit
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.thread_shutdown_timeout=1000
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Thannel;
use Swow\Thread;

$thannel = new Thannel(0);
$thread = Thread::run([$thannel, 'pop']);
usleep(100000);

// the thread is still parked, so the shutdown timeout ends the join and the
// shared objects must outlive the module
unset($thread, $thannel);

echo "parent end\n";
?>
--EXPECT--
parent end
