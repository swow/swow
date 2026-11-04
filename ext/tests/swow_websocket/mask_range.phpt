--TEST--
swow_websocket: mask and unmask reject a range outside the data
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\WebSocket\WebSocket;

$key = 'abcd';

// start beyond the buffer length
Assert::throws(static function () use ($key): void {
    WebSocket::mask('data', 10, 1, $key);
}, ValueError::class);
Assert::throws(static function () use ($key): void {
    WebSocket::mask('data', 0, 10, $key);
}, ValueError::class);

$buffer = new Buffer(16);
$buffer->append('data');
Assert::throws(static function () use ($buffer, $key): void {
    WebSocket::unmask($buffer, 10, 1, $key);
}, ValueError::class);
Assert::throws(static function () use ($buffer, $key): void {
    WebSocket::unmask($buffer, 0, 10, $key);
}, ValueError::class);

// a valid range still round-trips
$masked = WebSocket::mask('data', 0, -1, $key);
Assert::same(strlen($masked), 4);
Assert::notSame($masked, 'data');
$buffer = new Buffer(16);
$buffer->append($masked);
WebSocket::unmask($buffer, 0, -1, $key);
Assert::same($buffer->toString(), 'data');

echo "Done\n";

?>
--EXPECT--
Done
