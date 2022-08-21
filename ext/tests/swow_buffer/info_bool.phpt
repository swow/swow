--TEST--
swow_buffer: isReadable isWritable isSeekable isAvailable isEmpty isFull eof
--SKIPIF--
<?php

require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;

// buffer is always readable, writable and seekable if no io occurs on it

// not allocated buffer, all things should be 0
$buffer = new Buffer(0);
Assert::false($buffer->isAvailable());
Assert::true($buffer->isEmpty());
Assert::true($buffer->isFull());
// allocated with 4k
$buffer->alloc(4096);
Assert::true($buffer->isAvailable());
Assert::true($buffer->isEmpty());
Assert::false($buffer->isFull());
// write 1k data
$buffer->append(str_repeat('test', 256));
Assert::true($buffer->isAvailable());
Assert::false($buffer->isEmpty());
Assert::false($buffer->isFull());
// write another 3k data
$buffer->append(str_repeat('data', 768));
Assert::true($buffer->isAvailable());
Assert::false($buffer->isEmpty());
Assert::true($buffer->isFull());

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
