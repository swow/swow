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

$buffer = new Buffer(Buffer::COMMON_SIZE);
Assert::same($buffer->getSize(), Buffer::COMMON_SIZE);
Assert::same($buffer->getLength(), 0);
// write should return number of write bytes
Assert::same($buffer->write(0,'foo'), strlen('foo'));
// so it can be chained
$buffer->append('bar');
$buffer->append('baz');
Assert::same($buffer->getLength(), 9);

Assert::same($buffer->read(0, 3), 'foo');
Assert::same($buffer->read(3,6), 'barbaz');

// string dup or fetched string are not affected by original buffer
foreach (['stringify', 'fetch'] as $type) {
    $buffer = new Buffer(0);
    $buffer->append('foobarbaz');
    if ($type === 'stringify') {
        $_buffer = (string) $buffer; /* eq to __toString */
    } else {
        $_buffer = $buffer->fetchString();
    }
    Assert::same($_buffer, 'foobarbaz');
    $buffer->write(0, 'xoo');
    Assert::same($_buffer, 'foobarbaz');
    if ($type === 'stringify') {
        Assert::same((string) $buffer, 'xoobarbaz');
    } else {
        Assert::same((string) $buffer, 'xoo');
    }
    $buffer->clear();
    Assert::same((string) $buffer, '');
    Assert::same($_buffer, 'foobarbaz');
}

echo "Done\n";
?>
--EXPECT--
Done
