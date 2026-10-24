--TEST--
swow_spsc_queue: a forged or foreign mapping is rejected
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

use Swow\SharedBuffer;
use Swow\SpscQueue;
use Swow\SpscQueueException;
use Swow\SyncException;

// a header whose slot stride is smaller than its slot size would copy past the
// end of the mapping
$name = 'swow-test-spsc-forged-' . getmypid();
$buffer = SharedBuffer::createNamed($name, 1 << 20);
$buffer->write(0, pack('V6', 0x53505343 /* SPSC */, 1, 1, 2 << 20, 8, 0));
Assert::throws(static function () use ($name): void {
    SpscQueue::attachNamed($name);
}, SpscQueueException::class);
unset($buffer);
SharedBuffer::unlink($name);

// a shared buffer must not take over the mapping of another primitive
$queueName = 'swow-test-spsc-foreign-' . getmypid();
$queue = SpscQueue::createNamed($queueName, 4, 64);
Assert::throws(static function () use ($queueName): void {
    SharedBuffer::attachNamed($queueName);
}, SyncException::class);
unset($queue);
SpscQueue::unlink($queueName);

$futexName = 'swow-test-futex-foreign-' . getmypid();
$futex = Swow\Futex::createNamed($futexName, 1);
Assert::throws(static function () use ($futexName): void {
    SharedBuffer::attachNamed($futexName);
}, SyncException::class);
unset($futex);
Swow\Futex::unlink($futexName);

echo "done\n";
?>
--EXPECT--
done
