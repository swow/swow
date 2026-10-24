--TEST--
swow_thread: an abandoned child thread does not crash the module shutdown
--EXTENSIONS--
mongodb
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

// the module shutdown runs the globals destructor of every extension for every
// thread of the process, which crashes for the thread that is alive here
unset($thread, $thannel);

echo "parent end\n";
?>
--EXPECT--
parent end
