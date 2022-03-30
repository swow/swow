--TEST--
swow_buffer: clone object
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;

const CUSTOM_SIZE = Buffer::DEFAULT_SIZE + 3 * Buffer::PAGE_SIZE;

$buffer = new Buffer(CUSTOM_SIZE);
Assert::same($buffer->getSize(), CUSTOM_SIZE);
Assert::same($buffer->getLength(), 0);
$buffer->write('foo');
$buffer->write('bar');
Assert::same($buffer->getLength(), strlen('foobar'));
Assert::same((string) $buffer, 'foobar');

$clone = clone $buffer;
Assert::same($clone->getAvailableSize(), $buffer->getAvailableSize());
Assert::same($clone->getLength(), $buffer->getLength());
Assert::same($clone->getReadableLength(), $buffer->getReadableLength());
Assert::same($clone->getWritableSize(), $buffer->getWritableSize());
Assert::same((string) $clone, (string) $buffer);
Assert::same($clone->getSize(), $buffer->getSize());

$clone->write('baz');
Assert::same($clone->getLength(), strlen('foobarbaz'));
Assert::same((string) $clone, 'foobarbaz');
Assert::notSame($clone->getLength(), $buffer->getLength());
Assert::notSame((string) $clone, (string) $buffer);
Assert::notSame($clone->getWritableSize(), $buffer->getWritableSize());
Assert::same($clone->getSize(), $buffer->getSize());

unset($buffer);

Assert::false($clone->isEmpty());

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
