--TEST--
swow_buffer: alignSize getSize getLength getAvailableSize getReadableLength getWritableSize tell
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
Assert::same($buffer->tell(), 0);
Assert::same($buffer->getAvailableSize(), 0);
Assert::same($buffer->getReadableLength(), 0);
Assert::same($buffer->getWritableSize(), 0);
// allocated with 4k
$buffer->alloc(4096);
Assert::same($buffer->getSize(), 4096);
Assert::same($buffer->getLength(), 0);
Assert::same($buffer->tell(), 0);
Assert::same($buffer->getAvailableSize(), 4096);
Assert::same($buffer->getReadableLength(), 0);
Assert::same($buffer->getWritableSize(), 4096);
// write 1k data
$buffer->write(str_repeat('test', 256));
Assert::same($buffer->getSize(), 4096);
Assert::same($buffer->getLength(), 1024);
Assert::same($buffer->tell(), 1024);
Assert::same($buffer->getAvailableSize(), 4096 - 1024);
// buffer pointer is at 1k, so there's no readable data
Assert::same($buffer->getReadableLength(), 0);
Assert::same($buffer->getWritableSize(), 4096 - 1024);
// write another 1k data
$buffer->write(str_repeat('data', 256));
Assert::same($buffer->getSize(), 4096);
Assert::same($buffer->getLength(), 2048);
Assert::same($buffer->tell(), 2048);
Assert::same($buffer->getAvailableSize(), 4096 - 2048);
Assert::same($buffer->getReadableLength(), 0);
Assert::same($buffer->getWritableSize(), 4096 - 2048);
// seek to 1048
$buffer->seek(-1000, SEEK_CUR);
Assert::same($buffer->getSize(), 4096);
Assert::same($buffer->getLength(), 2048);
Assert::same($buffer->tell(), 2048 - 1000);
Assert::same($buffer->getAvailableSize(), 4096 - 2048);
Assert::same($buffer->getReadableLength(), 1000);
Assert::same($buffer->getWritableSize(), 4096 - (2048 - 1000));

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
