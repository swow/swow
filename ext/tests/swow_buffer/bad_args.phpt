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
use Swow\BufferException;

Assert::throws(function () {
    // buffer size must > 0
    new Buffer(-1);
}, ValueError::class, expectMessage: '/\$size/');

Assert::throws(function () {
    $buffer = new Buffer(0);
    // buffer size must > 0
    $buffer->realloc(-1);
}, ValueError::class, expectMessage: '/\$size/');

$buffer = new Buffer(4096);
Assert::throws(function () use ($buffer) {
    // buffer size must > 0
    $buffer->extend(-1);
}, ValueError::class, expectMessage: '/\$recommendSize/');

Assert::throws(function () use ($buffer) {
    // extend size must > now size
    $buffer->extend(2048);
}, ValueError::class, expectMessage: '/\$recommendSize/');

$buffer->write(0, 'cafe');
$buffer->append('babe');

Assert::throws(function () use ($buffer) {
    // read length must >= -1
    $buffer->read(-8);
}, ValueError::class, expectMessage: '/\$length/');

Assert::throws(function () use ($buffer) {
    // offset must >= 0
    $buffer->write( -1, 'nope');
}, ValueError::class, expectMessage: '/\$offset/');

Assert::throws(function () use ($buffer) {
    // start must >= 0
    $buffer->write( 0, 'nope', -1);
}, ValueError::class, expectMessage: '/\$start/');

Assert::throws(function () use ($buffer) {
    // length must >= -1
    $buffer->truncate(-123);
}, ValueError::class, expectMessage: '/\$length/');

Assert::throws(function () use ($buffer) {
    // offset must > 0
    $buffer->truncateFrom(-1, 1);
}, ValueError::class, expectMessage: '/\$offset/');

Assert::throws(function () use ($buffer) {
    // length must >= -1
    $buffer->truncateFrom(1, -123);
}, ValueError::class, expectMessage: '/\$length/');

// these are OK
$buffer->truncateFrom(9, 10);
$buffer->truncateFrom(9, 10000000);

echo "Done\n";
?>
--EXPECT--
Done
