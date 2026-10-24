--TEST--
swow_thread: a result that cannot be serialized fails the wait, not the process
--SKIPIF--
<?php
if (getenv('SWOW_TSAN') !== false) {
    exit('skip TSan cannot handle PHP bailout');
}
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=0
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Thread;
use Swow\ThreadException;

// a Closure cannot be serialized while swow.closure_serializer is off
$thread = Thread::run(['Closure', 'fromCallable'], 'strlen');
Assert::throws(static function () use ($thread): void {
    $thread->wait();
}, ThreadException::class);
Assert::same(255, $thread->getExitStatus());

// the thread is joined, so the second wait reports the failure again instead of
// an undefined result or "already joined"
Assert::throws(static function () use ($thread): void {
    $thread->wait();
}, ThreadException::class);

echo "alive\n";
?>
--EXPECT--
alive
