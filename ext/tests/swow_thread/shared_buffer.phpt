--TEST--
swow_thread: SharedBuffer
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
use Swow\SyncException;

Assert::throws(static function (): void {
    new SharedBuffer(0);
}, ValueError::class);
Assert::throws(static function (): void {
    SharedBuffer::createNamed('', 1);
}, ValueError::class);
Assert::throws(static function (): void {
    SharedBuffer::attachNamed('');
}, ValueError::class);
Assert::throws(static function (): void {
    SharedBuffer::unlink('');
}, ValueError::class);

$buffer = new SharedBuffer(256);
Assert::same(256, $buffer->getSize());
Assert::null($buffer->getName());
Assert::true($buffer->getSharedId() !== 0);

$buffer->write(0, 'abcdef');
Assert::same('abcdef', $buffer->read(0, 6));
Assert::same('cde', $buffer->read(2, 3));
Assert::throws(static function () use ($buffer): void {
    $buffer->read(250, 8);
}, ValueError::class);
Assert::throws(static function () use ($buffer): void {
    $buffer->write(250, '12345678');
}, ValueError::class);

$buffer->writeF32(8, [1.0, -2.5, 0.0]);
Assert::same([1.0, -2.5, 0.0], $buffer->viewF32(8, 3));
$buffer->writeF64(128, [1.5, -2.25]);
Assert::same([1.5, -2.25], $buffer->viewF64(128, 2));

$buffer->writeI8(32, [-128, -1, 0, 127]);
Assert::same([-128, -1, 0, 127], $buffer->viewI8(32, 4));
$buffer->writeU8(36, [0, 255]);
Assert::same([0, 255], $buffer->viewU8(36, 2));
$buffer->writeI16(40, [-32768, 32767]);
Assert::same([-32768, 32767], $buffer->viewI16(40, 2));
$buffer->writeU16(44, [0, 65535]);
Assert::same([0, 65535], $buffer->viewU16(44, 2));
$buffer->writeI32(48, [-2147483648, 2147483647]);
Assert::same([-2147483648, 2147483647], $buffer->viewI32(48, 2));
$buffer->writeU32(56, [0, 4294967295]);
Assert::same([0, 4294967295], $buffer->viewU32(56, 2));

Assert::same(0, SharedBuffer::NATIVE_ENDIAN);
Assert::same(1, SharedBuffer::LITTLE_ENDIAN);
Assert::same(2, SharedBuffer::BIG_ENDIAN);

$buffer->writeU32(96, [0x01020304], SharedBuffer::BIG_ENDIAN);
Assert::same("\x01\x02\x03\x04", $buffer->read(96, 4));
Assert::same([0x01020304], $buffer->viewU32(96, 1, SharedBuffer::BIG_ENDIAN));
$buffer->writeU32(96, [0x01020304], SharedBuffer::LITTLE_ENDIAN);
Assert::same("\x04\x03\x02\x01", $buffer->read(96, 4));
Assert::same([0x01020304], $buffer->viewU32(96, 1, SharedBuffer::LITTLE_ENDIAN));

$buffer->writeI16(100, [-2], SharedBuffer::BIG_ENDIAN);
Assert::same("\xFF\xFE", $buffer->read(100, 2));
Assert::same([-2], $buffer->viewI16(100, 1, SharedBuffer::BIG_ENDIAN));

$buffer->writeF32(104, [1.0], SharedBuffer::BIG_ENDIAN);
Assert::same("\x3F\x80\x00\x00", $buffer->read(104, 4));
Assert::same([1.0], $buffer->viewF32(104, 1, SharedBuffer::BIG_ENDIAN));

$buffer->writeF32(104, [1.0], SharedBuffer::LITTLE_ENDIAN);
Assert::same("\x00\x00\x80\x3F", $buffer->read(104, 4));
Assert::same([1.0], $buffer->viewF32(104, 1, SharedBuffer::LITTLE_ENDIAN));

$buffer->writeF64(112, [1.0], SharedBuffer::BIG_ENDIAN);
Assert::same("\x3F\xF0\x00\x00\x00\x00\x00\x00", $buffer->read(112, 8));
Assert::same([1.0], $buffer->viewF64(112, 1, SharedBuffer::BIG_ENDIAN));
$buffer->writeF64(112, [1.0], SharedBuffer::LITTLE_ENDIAN);
Assert::same("\x00\x00\x00\x00\x00\x00\xF0\x3F", $buffer->read(112, 8));
Assert::same([1.0], $buffer->viewF64(112, 1, SharedBuffer::LITTLE_ENDIAN));

$nativeBytes = pack('S', 1) === "\x01\x00"
    ? "\x04\x03\x02\x01"
    : "\x01\x02\x03\x04";
$buffer->writeU32(120, [0x01020304]);
Assert::same($nativeBytes, $buffer->read(120, 4));
Assert::same([0x01020304], $buffer->viewU32(120, 1));

$buffer->writeU32(offsetBytes: 124, values: [0x01020304], endian: SharedBuffer::BIG_ENDIAN);
Assert::same("\x01\x02\x03\x04", $buffer->read(offsetBytes: 124, lengthBytes: 4));
Assert::same([0x01020304], $buffer->viewU32(offsetBytes: 124, count: 1, endian: SharedBuffer::BIG_ENDIAN));

Assert::throws(static function () use ($buffer): void {
    $buffer->writeU32(96, [1], 3);
}, ValueError::class);
Assert::throws(static function () use ($buffer): void {
    $buffer->viewU8(96, 1, 3);
}, ValueError::class);
$buffer->writeI64(64, [PHP_INT_MIN, PHP_INT_MAX]);
Assert::same([PHP_INT_MIN, PHP_INT_MAX], $buffer->viewI64(64, 2));
$buffer->writeU64(80, [0, PHP_INT_MAX]);
Assert::same([0, PHP_INT_MAX], $buffer->viewU64(80, 2));

$buffer->fill(200, 4, 0xAA);
Assert::same("\xAA\xAA\xAA\xAA", $buffer->read(200, 4));
Assert::throws(static function () use ($buffer): void {
    $buffer->writeI8(255, [1, 2]);
}, ValueError::class);
Assert::throws(static function () use ($buffer): void {
    $buffer->viewU32(254, 1);
}, ValueError::class);
Assert::throws(static function () use ($buffer): void {
    $buffer->fill(255, 2, 0);
}, ValueError::class);

$source = new SharedBuffer(16);
$source->write(0, 'copy');
$buffer->copyFrom($source, 0, 240, 4);
Assert::same('copy', $buffer->read(240, 4));
Assert::throws(static function () use ($buffer, $source): void {
    $buffer->copyFrom($source, 0, 250, 10);
}, ValueError::class);
Assert::throws(static function () use ($buffer): void {
    $buffer->viewF32(250, 2);
}, ValueError::class);
Assert::throws(static function () use ($buffer): void {
    $buffer->writeF32(250, [1.0, 2.0]);
}, ValueError::class);
Assert::throws(static function () use ($buffer): void {
    $buffer->__unserialize(['id' => 1, 'token' => 1]);
}, SyncException::class);

$copy = unserialize(serialize($buffer));
Assert::same('abcdef', $copy->read(0, 6));
Assert::same([1.0, -2.5, 0.0], $copy->viewF32(8, 3));
Assert::same([1.5, -2.25], $copy->viewF64(128, 2));
Assert::same([-128, -1, 0, 127], $copy->viewI8(32, 4));
Assert::same([PHP_INT_MIN, PHP_INT_MAX], $copy->viewI64(64, 2));

$copy->clear();
Assert::same(str_repeat("\0", 256), $copy->read(0, 256));

$name = 'sbuf-' . getmypid() . '-' . bin2hex(random_bytes(4));
try {
    $named = SharedBuffer::createNamed($name, 32);
    $named->write(0, 'named');
    $attached = SharedBuffer::attachNamed($name);
    Assert::same($name, $attached->getName());
    Assert::same('named', $attached->read(0, 5));
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
