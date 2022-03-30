--TEST--
swow_buffer: truncate truncateFrom
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;

// not allocated buffer, all things should be 0
$buffer = new Buffer(0);
// allocated with 4k
$buffer->alloc(4096);
// write 1k data
$buffer->write(str_repeat('test', 256));
// keep only head 512
$buffer->truncate(512);
Assert::same($buffer->getLength(), 512);
Assert::same((string) $buffer, str_repeat('test', 128));
// keep [18, 18+256]
$buffer->truncateFrom(18, 256);
Assert::same($buffer->getLength(), 256);
Assert::same((string) $buffer, str_repeat('stte', 64));

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
