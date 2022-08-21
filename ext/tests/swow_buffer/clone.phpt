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

const CUSTOM_SIZE = Buffer::COMMON_SIZE + 3 * Buffer::PAGE_SIZE;

$buffer = new Buffer(CUSTOM_SIZE);
Assert::same($buffer->getSize(), CUSTOM_SIZE);
Assert::same($buffer->getLength(), 0);
$buffer->append('foo');
$buffer->append('bar');
Assert::same($buffer->getLength(), strlen('foobar'));
Assert::same((string) $buffer, 'foobar');

$clone = clone $buffer;
Assert::same($clone->getSize(), $buffer->getSize());
Assert::same($clone->getLength(), $buffer->getLength());
Assert::same($clone->getAvailableSize(), $buffer->getAvailableSize());
Assert::same((string) $clone, (string) $buffer);

$clone->append('baz');
Assert::same($clone->getLength(), strlen('foobarbaz'));
Assert::same((string) $clone, 'foobarbaz');
Assert::same($clone->getSize(), $buffer->getSize());
Assert::greaterThan($clone->getLength(), $buffer->getLength());
Assert::lessThan($clone->getAvailableSize(), $buffer->getAvailableSize());
Assert::notSame((string) $clone, (string) $buffer);

unset($buffer);

Assert::false($clone->isEmpty());

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
