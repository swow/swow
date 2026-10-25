--TEST--
swow_thread: methods of unconstructed sync objects
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

use Swow\Futex;
use Swow\Mutex;
use Swow\Semaphore;
use Swow\SharedBuffer;
use Swow\Sync\ThreadBarrier;
use Swow\SyncException;
use Swow\Thannel;
use Swow\ThannelException;

function unconstructed(string $class): object
{
    return (new ReflectionClass($class))->newInstanceWithoutConstructor();
}

$buffer = unconstructed(SharedBuffer::class);
$destination = new SharedBuffer(8);
foreach ([
    static fn () => $buffer->read(0, 0),
    static fn () => $buffer->write(0, 'x'),
    static fn () => $buffer->slice(0, 0),
    static fn () => $buffer->fill(0, 0, 0),
    static fn () => $buffer->clear(),
    static fn () => $buffer->copyFrom($destination, 0, 0, 0),
    static fn () => $destination->copyFrom($buffer, 0, 0, 0),
    static fn () => $buffer->writeF32(0, [1.0]),
    static fn () => $buffer->viewF32(0, 1),
    static fn () => $buffer->writeF64(0, [1.0]),
    static fn () => $buffer->viewF64(0, 1),
    static fn () => $buffer->writeI8(0, [1]),
    static fn () => $buffer->viewI8(0, 1),
    static fn () => $buffer->writeU64(0, [1]),
    static fn () => $buffer->viewU64(0, 1),
    static fn () => $buffer->__serialize(),
] as $call) {
    Assert::throws($call, SyncException::class);
}

# a second construct call is refused
Assert::throws(static fn () => (new Mutex())->__construct(), SyncException::class);
Assert::throws(static fn () => (new Semaphore(0))->__construct(0), SyncException::class);
Assert::throws(static fn () => (new Thannel(1))->__construct(1), ThannelException::class);
Assert::throws(static fn () => (new ThreadBarrier(1))->__construct(1), SyncException::class);

$futex = unconstructed(Futex::class);
Assert::same(0, $futex->getValue());
Assert::same(0, $futex->getSharedId());
foreach ([
    static fn () => $futex->setValue(1),
    static fn () => $futex->add(1),
    static fn () => $futex->compareExchange(0, 1),
    static fn () => $futex->exchange(1),
    static fn () => $futex->wait(0),
    static fn () => $futex->wake(1),
    static fn () => $futex->__serialize(),
] as $call) {
    Assert::throws($call, SyncException::class);
}

$mutex = unconstructed(Mutex::class);
foreach ([
    static fn () => $mutex->lock(),
    static fn () => $mutex->unlock(),
    static fn () => $mutex->__serialize(),
] as $call) {
    Assert::throws($call, SyncException::class);
}

$semaphore = unconstructed(Semaphore::class);
Assert::same(0, $semaphore->getValue());
Assert::same(0, $semaphore->getWaiterCount());
foreach ([
    static fn () => $semaphore->acquire(),
    static fn () => $semaphore->tryAcquire(),
    static fn () => $semaphore->release(),
    static fn () => $semaphore->__serialize(),
] as $call) {
    Assert::throws($call, SyncException::class);
}

$thannel = unconstructed(Thannel::class);
Assert::false($thannel->isClosed());
Assert::same(0, $thannel->getCapacity());
Assert::same(0, $thannel->getLength());
Assert::true($thannel->isEmpty());
Assert::false($thannel->isFull());
Assert::false($thannel->willBlock());
foreach ([
    static fn () => $thannel->push('x'),
    static fn () => $thannel->pop(),
    static fn () => $thannel->pushMany(['x']),
    static fn () => $thannel->popMany(1),
    static fn () => $thannel->drain(),
    static fn () => $thannel->close(),
    static fn () => $thannel->__serialize(),
] as $call) {
    Assert::throws($call, ThannelException::class);
}

$barrier = unconstructed(ThreadBarrier::class);
Assert::same(0, $barrier->getCount());
Assert::same(0, $barrier->getWaiting());
foreach ([
    static fn () => $barrier->wait(),
    static fn () => $barrier->__serialize(),
] as $call) {
    Assert::throws($call, SyncException::class);
}

# a batch that fails to serialize reports the original error
$thannel = new Thannel(1);
Assert::throws(static function () use ($thannel): void {
    $thannel->pushMany([static function (): void {
    }]);
}, Exception::class, null, '/Serialization of .Closure/');

echo "Done\n";
?>
--EXPECT--
Done
