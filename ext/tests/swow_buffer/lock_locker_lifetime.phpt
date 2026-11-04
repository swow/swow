--TEST--
swow_buffer: a lock goes away with the coroutine that took it
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Coroutine;

$buffer = new Buffer(16);
$buffer->append('data');

$coroutine = Coroutine::run(static function () use ($buffer): void {
    $buffer->lock();
    Coroutine::yield();
});

Assert::true($buffer->isLocked());
Assert::same($buffer->getLocker()->getId(), $coroutine->getId());

// The coroutine ends while it still holds the lock; the lock must not
// outlive it.
$coroutine->kill();

Assert::false($buffer->isLocked());
Assert::null($buffer->getLocker());
Assert::true($buffer->tryLock());
$buffer->unlock();
Assert::same($buffer->toString(), 'data');

echo "Done\n";

?>
--EXPECT--
Done
