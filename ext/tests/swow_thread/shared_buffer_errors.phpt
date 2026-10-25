--TEST--
swow_thread: SharedBuffer PHP error paths
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\SharedBuffer;
use Swow\SyncException;

$buffer = new SharedBuffer(32);
Assert::throws(static function () use ($buffer): void {
    $buffer->__construct(8);
}, SyncException::class, null, '/already been constructed/');
Assert::same(32, $buffer->getSize());

$id = getmypid() . '-' . bin2hex(random_bytes(4));
$name = 'sbuf-' . $id;

try {
    Assert::throws(static function () use ($name): void {
        SharedBuffer::createNamed($name, 0);
    }, ValueError::class);

    $named = SharedBuffer::createNamed($name, 16);
    Assert::throws(static function () use ($name): void {
        SharedBuffer::createNamed($name, 16);
    }, SyncException::class);

    # values that do not fit the requested width
    foreach ([
        static fn () => $buffer->writeI8(0, [128]),
        static fn () => $buffer->writeI8(0, [-129]),
        static fn () => $buffer->writeU8(0, [-1]),
        static fn () => $buffer->writeU8(0, [256]),
        static fn () => $buffer->writeI16(0, [32768]),
        static fn () => $buffer->writeU16(0, [65536]),
        static fn () => $buffer->writeI32(0, [2147483648]),
        static fn () => $buffer->writeU32(0, [-1]),
        static fn () => $buffer->writeU32(0, [4294967296]),
        static fn () => $buffer->writeU64(0, [-1]),
    ] as $write) {
        Assert::throws($write, ValueError::class);
    }
    $buffer->writeU8(0, [0]);
    Assert::same([0], $buffer->viewU8(0, 1));

    # the fill byte is a byte
    Assert::throws(static function () use ($buffer): void {
        $buffer->fill(0, 1, -1);
    }, ValueError::class);
    Assert::throws(static function () use ($buffer): void {
        $buffer->fill(0, 1, 256);
    }, ValueError::class);

    # the endian argument is one of the three constants
    foreach ([
        static fn () => $buffer->writeF32(0, [1.0], 3),
        static fn () => $buffer->viewF32(0, 1, 3),
        static fn () => $buffer->writeF64(0, [1.0], 3),
        static fn () => $buffer->viewF64(0, 1, 3),
        static fn () => $buffer->writeI8(0, [1], 3),
        static fn () => $buffer->viewI8(0, 1, 3),
    ] as $typed) {
        Assert::throws($typed, ValueError::class);
    }

    # a serialized name is a string that names a live mapping
    Assert::throws(static function () use ($buffer): void {
        $buffer->__unserialize(['name' => 1]);
    }, SyncException::class);
    Assert::throws(static function () use ($buffer, $id): void {
        $buffer->__unserialize(['name' => 'missing-' . $id]);
    }, SyncException::class);

    # unserializing a name into a constructed object drops the old handle
    $buffer->__unserialize(['name' => $name]);
    Assert::same($name, $buffer->getName());
    Assert::same(16, $buffer->getSize());

    Assert::same(['name' => $name, 'size' => 16], $buffer->__debugInfo());
    $anonymous = new SharedBuffer(8);
    Assert::same(['size' => 8], $anonymous->__debugInfo());

    # an anonymous handle serializes by id, and unserializing it into a
    # constructed object drops the old handle
    $byId = new SharedBuffer(8);
    $byId->write(0, 'id');
    $anonymous->__unserialize($byId->__serialize());
    Assert::same('id', $anonymous->read(0, 2));
} finally {
    try {
        SharedBuffer::unlink($name);
    } catch (Throwable) {
    }
}

echo "Done\n";
?>
--EXPECT--
Done
