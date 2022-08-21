--TEST--
swow_buffer: info int
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
Assert::same($buffer->getSize(), 0);
Assert::same($buffer->getLength(), 0);
Assert::same($buffer->getAvailableSize(), 0);
// allocated with 4k
$buffer->alloc(4096);
Assert::same($buffer->getSize(), 4096);
Assert::same($buffer->getLength(), 0);
Assert::same($buffer->getAvailableSize(), 4096);
// write 1k data
$buffer->append(str_repeat('test', 256));
Assert::same($buffer->getSize(), 4096);
Assert::same($buffer->getLength(), 1024);
Assert::same($buffer->getAvailableSize(), 4096 - 1024);
// write another 1k data
$buffer->append(str_repeat('data', 256));
Assert::same($buffer->getSize(), 4096);
Assert::same($buffer->getLength(), 2048);
Assert::same($buffer->getAvailableSize(), 4096 - 2048);

// in default, buffer alignment is sizeof(pointer size)
Assert::same(Buffer::alignSize() % PHP_INT_SIZE, 0);
// 16 is aligned to most systems nowadays
Assert::same(Buffer::alignSize(16), 16);
// 25 aligned to 8 is 32
Assert::same(Buffer::alignSize(25, 8), 32);
// 0 aligned to 32 is 32
Assert::same(Buffer::alignSize(0, 32), 32);

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
