--TEST--
swow_shared_object: a serialized handle is rejected by every other class
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

$sources = [
    new Swow\Thannel(1),
    new Swow\Mutex(),
    new Swow\Semaphore(0),
    new Swow\Sync\ThreadBarrier(1),
    new Swow\Futex(0),
    new Swow\SpscQueue(1, 64),
    new Swow\SharedBuffer(64),
];

$classes = [
    Swow\Thannel::class,
    Swow\Mutex::class,
    Swow\Semaphore::class,
    Swow\Sync\ThreadBarrier::class,
    Swow\Futex::class,
    Swow\SpscQueue::class,
    Swow\SharedBuffer::class,
];

foreach ($sources as $source) {
    $payload = $source->__serialize();
    foreach ($classes as $class) {
        if ($class === $source::class) {
            Assert::same($source::class, $class);
            continue;
        }
        $target = (new ReflectionClass($class))->newInstanceWithoutConstructor();
        Assert::throws(static function () use ($target, $payload): void {
            $target->__unserialize($payload);
        }, Throwable::class);
    }
}

// the same class still takes the handle
$thannel = new Swow\Thannel(1);
$copy = (new ReflectionClass(Swow\Thannel::class))->newInstanceWithoutConstructor();
$copy->__unserialize($thannel->__serialize());
$copy->push('shared');
Assert::same('shared', $thannel->pop(0));

echo "done\n";
?>
--EXPECT--
done
