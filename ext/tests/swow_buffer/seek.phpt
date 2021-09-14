--TEST--
swow_buffer: bad arguments passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;

$buffer = new Buffer(4096);

$buffer->write('cafebabe');

$buffer->write('deadbeef');

Assert::same($buffer->tell(), 16);
$buffer->seek(8/*, SEEK_SET*/);
Assert::same($buffer->tell(), 8);
$buffer->seek(0, SEEK_CUR);
Assert::same($buffer->tell(), 8);
$buffer->seek(-1, SEEK_CUR);
Assert::same($buffer->tell(), 7);
$buffer->seek(3, SEEK_CUR);
Assert::same($buffer->tell(), 10);
$buffer->seek(-3, SEEK_END);
Assert::same($buffer->tell(), 13);
$buffer->seek(0, SEEK_END);
Assert::same($buffer->tell(), 16);


echo "Done\n";
?>
--EXPECT--
Done
