--TEST--
swow_thread: a second concurrent wait reports a misuse instead of crashing
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Thread;

$thread = new Thread('usleep', 300000);
$thread->start();

/*
 * Only one caller may take the exit of a thread, because that caller joins and
 * destroys its join queue. The other caller reports the misuse, and the process
 * keeps running instead of joining and destroying the same thread twice.
 */
$first = 'none';
$coroutine = Coroutine::run(static function () use ($thread, &$first): void {
    try {
        $thread->wait();
        $first = 'joined';
    } catch (Throwable) {
        $first = 'error';
    }
});

usleep(10000);

$second = 'none';
try {
    $thread->wait(2000);
    $second = 'joined';
} catch (Throwable) {
    $second = 'error';
}

$deadline = microtime(true) + 3;
while ($coroutine->isAvailable() && microtime(true) < $deadline) {
    usleep(10000);
}

$results = [$first, $second];
sort($results);

var_dump(implode(',', $results));
?>
--EXPECT--
string(12) "error,joined"
