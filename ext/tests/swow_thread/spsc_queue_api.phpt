--TEST--
swow_thread: SpscQueue API and errors
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

Assert::throws(static function (): void {
    new SpscQueue(0);
}, ValueError::class);
Assert::throws(static function (): void {
    new SpscQueue(1, 0);
}, ValueError::class);
Assert::throws(static function (): void {
    SpscQueue::createNamed('', 1);
}, SpscQueueException::class);
Assert::throws(static function (): void {
    SpscQueue::attachNamed('');
}, SpscQueueException::class);
Assert::throws(static function (): void {
    SpscQueue::unlink('');
}, SpscQueueException::class);

$queue = new SpscQueue(2, 64);
Assert::same(0, $queue->pushMany([]));
Assert::same([], $queue->drain());
Assert::throws(static function () use ($queue): void {
    $queue->popMany(0, 0);
}, ValueError::class);
Assert::throws(static function () use ($queue): void {
    $queue->popMany(100, 0);
}, SpscQueueException::class);

Assert::throws(static function () use ($queue): void {
    $queue->pushMany([1, 2, 3], 0);
}, SpscQueueException::class);
Assert::throws(static function () use ($queue): void {
    $queue->pushMany([str_repeat('x', 128)], 0);
}, ValueError::class);

$queue->pushMany([1, 2]);
Assert::same([1, 2], $queue->popMany(100));

$queue->push('pending');
$queue->close();
Assert::same('pending', $queue->pop(0));
Assert::same([], $queue->drain());
Assert::throws(static function () use ($queue): void {
    $queue->pop(0);
}, SpscQueueException::class);
Assert::throws(static function () use ($queue): void {
    $queue->push('closed', 0);
}, SpscQueueException::class);
$queue->close();

$queue = new SpscQueue(1, 64);
$queue->push('anonymous');
$copy = unserialize(serialize($queue));
Assert::same('anonymous', $copy->pop());
Assert::same(null, $copy->getName());
$debug = $copy->__debugInfo();
Assert::same(1, $debug['capacity']);
Assert::same(64, $debug['maxMessageSize']);

$invalid = new SpscQueue(1, 64);
Assert::throws(static function () use ($invalid): void {
    $invalid->__unserialize(['name' => 123]);
}, SpscQueueException::class);
Assert::throws(static function () use ($invalid): void {
    $invalid->__unserialize(['name' => 'swow-spsc-missing']);
}, SpscQueueException::class);
Assert::throws(static function () use ($invalid): void {
    $invalid->__unserialize(['id' => 1, 'token' => 1]);
}, SpscQueueException::class);

$name = 'spsc-api-' . getmypid() . '-' . bin2hex(random_bytes(4));
try {
    $named = SpscQueue::createNamed($name, 2, 64);
    $named->push('named copy');
    $copy = unserialize(serialize($named));
    Assert::same($name, $copy->getName());
    Assert::same('named copy', $copy->pop());

    Assert::throws(static function () use ($name): void {
        SpscQueue::createNamed($name, 2, 64);
    }, SpscQueueException::class);
} finally {
    unset($named, $copy);
    try {
        SpscQueue::unlink($name);
    } catch (Throwable) {
    }
}

echo "Done\n";

?>
--EXPECT--
Done
