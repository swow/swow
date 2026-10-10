--TEST--
swow_buffer: explicit lock ownership, serialization and string transfer
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Coroutine;

$buffer = new Buffer(64);
$buffer->append('payload');
Assert::false($buffer->isLocked());
Assert::null($buffer->getLocker());
Assert::true($buffer->tryLock());
try {
    Assert::true($buffer->isLocked());
    Assert::same($buffer->getLocker(), Coroutine::getCurrent());
    Assert::false($buffer->tryLock());
    Assert::same($buffer->dupString(), 'payload');
} finally {
    $buffer->unlock();
}
Assert::false($buffer->isLocked());
Assert::null($buffer->getLocker());
$buffer->lock();
$buffer->unlock();

$copy = unserialize(serialize($buffer));
Assert::true($copy instanceof Buffer);
Assert::same((string) $copy, 'payload');
$copy->append(' copy');
Assert::same((string) $buffer, 'payload');
Assert::same($copy->fetchString(), 'payload copy');
Assert::true($copy->isEmpty());
Assert::same((string) $copy, '');

$empty = unserialize(serialize(new Buffer(0)));
Assert::same((string) $empty, '');
Assert::throws(static function (): void {
    (new Buffer(0))->__unserialize([]);
}, ValueError::class, expectMessage: '/value/');
Assert::throws(static function (): void {
    (new Buffer(0))->__unserialize(['value' => 42]);
}, ValueError::class, expectMessage: '/value/');

echo "Done\n";
?>
--EXPECT--
Done
