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
}, ValueError::class);

Assert::throws(function () {
    $buffer = new Buffer(0);
    // buffer size must > 0
    $buffer->realloc(-1);
}, ValueError::class);

$buffer = new Buffer(4096);
Assert::throws(function () use ($buffer) {
    // buffer size must > 0
    $buffer->extend(-1);
}, ValueError::class);

Assert::throws(function () use ($buffer) {
    // extend size must > now size
    $buffer->extend(2048);
}, ValueError::class);

Assert::throws(function () use ($buffer) {
    // unknown whence
    $buffer->seek(0, -1);
}, ValueError::class);

Assert::throws(function () use ($buffer) {
    // out of range
    $buffer->seek(123, SEEK_END);
}, Throwable::class);

$buffer->write('cafebabe');

Assert::throws(function () use ($buffer) {
    // read length must >= -1
    $buffer->read(-8);
}, ValueError::class);

Assert::throws(function () use ($buffer) {
    // offset must > 0
    $buffer->peekFrom(-1, 8);
}, ValueError::class);

Assert::throws(function () use ($buffer) {
    // length must >= -1
    $buffer->peekFrom(0, -8);
}, ValueError::class);

Assert::throws(function () use ($buffer) {
    // bad range
    $buffer->peekFrom(9, 23);
}, BufferException::class);

Assert::throws(function () use ($buffer) {
    // bad range
    $buffer->peekFrom(9, 111111);
}, BufferException::class);

Assert::throws(function () use ($buffer) {
    // length must >= -1
    $buffer->peek(-8);
}, ValueError::class);

Assert::throws(function () use ($buffer) {
    // offset must >= 0
    $buffer->write('nope', -1);
}, ValueError::class);

Assert::throws(function () use ($buffer) {
    // length must >= -1
    $buffer->truncate(-123);
}, ValueError::class);

Assert::throws(function () use ($buffer) {
    // offset must > 0
    $buffer->truncateFrom(-1, 1);
}, ValueError::class);

Assert::throws(function () use ($buffer) {
    // length must >= -1
    $buffer->truncateFrom(1, -123);
}, ValueError::class);

// these are OK
$buffer->truncateFrom(9, 10);
$buffer->truncateFrom(9, 10000000);

echo "Done\n";
?>
--EXPECT--
Done
