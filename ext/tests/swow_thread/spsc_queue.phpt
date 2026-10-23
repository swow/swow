--TEST--
swow_thread: SpscQueue
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\SpscQueue;
use Swow\SpscQueueException;
use Swow\Thread;

$queue = new SpscQueue(3, 64);
Assert::eq(3, $queue->getCapacity());
Assert::eq(64, $queue->getMaxMessageSize());
Assert::true($queue->isEmpty());
Assert::false($queue->isFull());
Assert::null($queue->getName());

$queue->push(['key' => 'value']);
Assert::eq(1, $queue->getLength());
Assert::false($queue->isEmpty());
Assert::same(['key' => 'value'], $queue->pop());
Assert::true($queue->isEmpty());

$queue->pushMany([1, 2, 3]);
Assert::true($queue->isFull());
Assert::same([1, 2, 3], $queue->drain());
Assert::true($queue->isEmpty());

$queue->pushMany([1, 2, 3]);
Assert::same([1, 2], $queue->popMany(2));
Assert::same([3], $queue->drain());

$queue->push('x');
$queue->push('y');
$queue->push('z');
Assert::throws(static function () use ($queue): void {
    $queue->push('w', 0);
}, SpscQueueException::class, expectMessage: 'Timed out waiting for SPSC queue space');
Assert::same('x', $queue->pop());
Assert::same(['y', 'z'], $queue->drain());

Assert::throws(static function () use ($queue): void {
    $queue->push(str_repeat('x', 128), 0);
}, ValueError::class, expectMessage: '/maxMessageSize/');

$queue->close();
Assert::true($queue->isClosed());
Assert::throws(static function () use ($queue): void {
    $queue->pop(0);
}, SpscQueueException::class, expectMessage: 'The SPSC queue is closed');

$queue->close();

$name = 'spsc-' . getmypid() . '-' . bin2hex(random_bytes(4));
try {
    $named = SpscQueue::createNamed($name, 2, 64);
    Assert::same($name, $named->getName());
    $named->push('named');
    $attached = SpscQueue::attachNamed($name);
    Assert::same('named', $attached->pop());
    $attached->close();
} finally {
    try {
        SpscQueue::unlink($name);
    } catch (Throwable) {
    }
}

$queue = new SpscQueue(2, 64);
$thread = Thread::run(static function () use ($queue): void {
    $queue->push(['thread' => true]);
});
$thread->wait();
Assert::same(['thread' => true], $queue->pop());

echo "Done\n";

?>
--EXPECT--
Done
