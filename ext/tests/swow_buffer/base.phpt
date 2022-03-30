--TEST--
swow_buffer: base
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;

// size = 0 meaning it's not allocated
$buffer = new Buffer(0);
Assert::same($buffer->toString(), '');
Assert::same($buffer->fetchString(), '');

$buffer = new Buffer();
Assert::same($buffer->getSize(), Buffer::DEFAULT_SIZE);
Assert::same($buffer->getLength(), 0);
// write should return self
Assert::same($buffer->write('foo'), $buffer);
// so it can be chained
$buffer
    ->write('bar')
    ->write('baz');
Assert::same($buffer->getLength(), 9);

Assert::same($buffer->rewind(), $buffer);
Assert::same($buffer->read(3), 'foo');
Assert::same($buffer->read(6), 'barbaz');
Assert::same($buffer->seek(-6, SEEK_END)->read(), 'barbaz');
Assert::same($buffer->tell(), 9);
// getContents is read without arguments
Assert::same($buffer->seek(-6, SEEK_END)->getContents(), 'barbaz');
Assert::same($buffer->tell(), 9);
// peek do not change pointer
// if peek or peekFrom length is larger than available, it returns available
Assert::same($buffer->seek(-3, SEEK_END)->peek(6), 'baz');
Assert::same($buffer->tell(), 6);
// peekFrom can set a start offset
Assert::same($buffer->seek(-3, SEEK_END)->peekFrom(3, 9), 'barbaz');
Assert::same($buffer->tell(), 6);
// like peek, depString reads all buffer, do not change pointer
Assert::same($buffer->seek(-3, SEEK_END)->dupString(), 'foobarbaz');
Assert::same($buffer->tell(), 6);

// copy string is not affected by original buffer
$_buffer = (string) $buffer; /* eq to __toString */
Assert::same($_buffer, 'foobarbaz');
$buffer->clear();
Assert::same($_buffer, 'foobarbaz');
Assert::same((string) $buffer, '');

// copy string is not affected by original buffer
$buffer->write('bazbarfoo');
Assert::same($_buffer = $buffer->fetchString(), 'bazbarfoo');
$buffer->clear();
Assert::same($_buffer, 'bazbarfoo');
Assert::same($buffer->fetchString(), '');

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
