--TEST--
swow_thread: run code reports the exit status and the exception of the code
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
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Thread;

$thread = Thread::runCode('exit(7);');
Assert::null($thread->wait());
Assert::same(7, $thread->getExitStatus());

$thread = Thread::runCode('throw new RuntimeException("boom");');
Assert::throws(static function () use ($thread): void {
    $thread->wait();
}, RuntimeException::class, null, 'boom');
Assert::same(255, $thread->getExitStatus());

$thread = Thread::runCode('echo "ran\n";');
Assert::null($thread->wait());
Assert::same(0, $thread->getExitStatus());

echo "done\n";
?>
--EXPECT--
ran
done
