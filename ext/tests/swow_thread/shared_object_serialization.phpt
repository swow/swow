--TEST--
swow_thread: shared object serialization and the handle registry
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

# a serialized handle from another process is rejected
$buffer = new SharedBuffer(8);
$data = $buffer->__serialize();
$data['token'] ^= 1;
$other = new SharedBuffer(8);
Assert::throws(static function () use ($other, $data): void {
    $other->__unserialize($data);
}, SyncException::class, null, '/another process/');

# a handle id of one class can not be unserialized as another class
$futex = new Futex(3);
$data = $futex->__serialize();
$data['id'] = $buffer->getSharedId();
$target = new Futex(0);
Assert::throws(static function () use ($target, $data): void {
    $target->__unserialize($data);
}, SyncException::class, null, '/another class/');

# unserializing into a constructed object drops the old handle
$mutexA = new Mutex();
$mutexB = new Mutex();
$mutexB->__unserialize($mutexA->__serialize());

$semaphoreA = new Semaphore(3);
$semaphoreB = new Semaphore(0);
$semaphoreB->__unserialize($semaphoreA->__serialize());
Assert::same(3, $semaphoreB->getValue());

$thannelA = new Thannel(4);
$thannelA->push('payload');
$thannelB = new Thannel(4);
$thannelB->__unserialize($thannelA->__serialize());
Assert::same(1, $thannelB->getLength());
Assert::same('payload', $thannelB->pop());

$barrierA = new ThreadBarrier(3);
$barrierB = new ThreadBarrier(1);
$barrierB->__unserialize($barrierA->__serialize());
Assert::same(3, $barrierB->getCount());

# enough live handles grow the registry and rehash the old entries
$objects = [];
for ($i = 0; $i < 60; $i++) {
    $objects[] = new Futex($i);
}
foreach ($objects as $i => $object) {
    Assert::same($i, $object->getValue());
}

echo "Done\n";
?>
--EXPECT--
Done
